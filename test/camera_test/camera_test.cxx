#include <cgv/base/node.h>
#include <cgv/math/camera.h>
#include <cgv/media/color_scale.h>
#include <cgv/signal/rebind.h>
#include <cgv/base/register.h>
#include <cgv/gui/provider.h>
#include <cgv/render/drawable.h>
#include <cgv/render/shader_program.h>
#include <cgv/render/vertex_buffer.h>
#include <cgv/render/frame_buffer.h>
#include <cgv/render/attribute_array_binding.h>
#include <point_cloud/point_cloud.h>
#include <cgv_gl/gl/gl.h>
#include <cgv_gl/point_renderer.h>
#include <cgv_gl/sphere_renderer.h>
#include <cgv_gl/a_buffer.h>
#include <random>
#include "../../3rd/json/nlohmann/json.hpp"
#include "../../3rd/json/cgv_json/math.h"
#include <fstream>
#include <cgv/utils/file.h>
#include <cgv/utils/convert.h>

using namespace cgv::base;
using namespace cgv::data;
using namespace cgv::signal;
using namespace cgv::gui;
using namespace cgv::render;
using namespace cgv::utils;
using namespace cgv::render::gl;


struct rgbd_kinect_azure
{
	const cgv::math::camera<double>* c_ptr = 0;
	bool map_depth_to_point(int x, int y, int depth, float* point_ptr) const
	{
		// 
		const auto& c = *c_ptr;

		double fx_d = 1.0 / c.s(0);
		double fy_d = 1.0 / c.s(1);
		double cx_d = c.c(0);
		double cy_d = c.c(1);
		// set 0.001 for current vr_rgbd
		double d = 0.001 * depth * 1.000;
		d = 1;
		double x_d = (x - cx_d) * fx_d;
		double y_d = (y - cy_d) * fy_d;

		float uv[2], xy[2];
		uv[0] = float(x_d);
		uv[1] = float(y_d);
		bool valid = false;

		if (transformation_unproject_internal(uv, xy, valid)) {
			point_ptr[0] = float(xy[0] * d);
			point_ptr[1] = float(xy[1] * d);
			point_ptr[2] = float(d);
			return valid;
		}
		return false;
	}
	bool transformation_iterative_unproject(const float* uv, float* xy, bool& valid, unsigned int max_passes) const
	{
		valid = true;
		float Jinv[2 * 2];
		float best_xy[2] = { 0.f, 0.f };
		float best_err = FLT_MAX;

		for (unsigned int pass = 0; pass < max_passes; pass++) {
			float p[2];
			float J[2 * 2];
			if (!transformation_project_internal(xy, p, valid, J))
			{
				return false;
			}
			if (!valid)
			{
				return true;
			}

			float err_x = uv[0] - p[0];
			float err_y = uv[1] - p[1];
			float err = err_x * err_x + err_y * err_y;
			if (err >= best_err) {
				xy[0] = best_xy[0];
				xy[1] = best_xy[1];
				break;
			}

			best_err = err;
			best_xy[0] = xy[0];
			best_xy[1] = xy[1];
			invert_2x2(J, Jinv);
			if (pass + 1 == max_passes || best_err < 1e-22f) {
				break;
			}

			float dx = Jinv[0] * err_x + Jinv[1] * err_y;
			float dy = Jinv[2] * err_x + Jinv[3] * err_y;

			xy[0] += dx;
			xy[1] += dy;
		}
		if (best_err > 1e-6f)
		{
			valid = false;
		}
		return true;
	}
	bool transformation_project_internal(const float xy[2], float point2d[2], bool& valid, float J_xy[2 * 2]) const
	{
		const auto& c = *c_ptr;
		float max_radius_for_projection = float(c.max_radius_for_projection);
		valid = true;

		float xp = float(xy[0] - c.dc(0));
		float yp = float(xy[1] - c.dc(1));

		float xp2 = xp * xp;
		float yp2 = yp * yp;
		float xyp = xp * yp;
		float rs = xp2 + yp2;
		float rm = max_radius_for_projection * max_radius_for_projection;
		if (rs > rm) {
			valid = false;
			return true;
		}
		float rss = rs * rs;
		float rsc = rss * rs;
		float a = float(1.f + c.k[0] * rs + c.k[1] * rss + c.k[2] * rsc);
		float b = float(1.f + c.k[3] * rs + c.k[4] * rss + c.k[5] * rsc);
		float bi;
		if (b != 0.f) {
			bi = 1.f / b;
		}
		else {
			bi = 1.f;
		}
		float d = a * bi;

		float xp_d = xp * d;
		float yp_d = yp * d;

		float rs_2xp2 = rs + 2.f * xp2;
		float rs_2yp2 = rs + 2.f * yp2;

		xp_d += float(rs_2xp2 * c.p[1] + 2.f * xyp * c.p[0]);
		yp_d += float(rs_2yp2 * c.p[0] + 2.f * xyp * c.p[1]);

		float xp_d_cx = float(xp_d + c.dc(0));
		float yp_d_cy = float(yp_d + c.dc(1));

		point2d[0] = float(xp_d_cx * c.s(0) + c.c(0));
		point2d[1] = float(yp_d_cy * c.s(1) + c.c(1));

		if (J_xy == 0) {
			return true;
		}

		// compute Jacobian matrix
		float dudrs = c.k[0] + 2.f * c.k[1] * rs + 3.f * c.k[2] * rss;
		// compute d(b)/d(r^2)
		float dvdrs = c.k[3] + 2.f * c.k[4] * rs + 3.f * c.k[5] * rss;
		float bis = bi * bi;
		float dddrs = (dudrs * b - a * dvdrs) * bis;

		float dddrs_2 = dddrs * 2.f;
		float xp_dddrs_2 = xp * dddrs_2;
		float yp_xp_dddrs_2 = yp * xp_dddrs_2;
		// compute d(u)/d(xp)
		J_xy[0] = float(c.s(0) * (d + xp * xp_dddrs_2 + 6.f * xp * c.p[1] + 2.f * yp * c.p[0]));
		J_xy[1] = float(c.s(0) * (yp_xp_dddrs_2 + 2.f * yp * c.p[1] + 2.f * xp * c.p[0]));
		J_xy[2] = float(c.s(1) * (yp_xp_dddrs_2 + 2.f * xp * c.p[0] + 2.f * yp * c.p[1]));
		J_xy[3] = float(c.s(1) * (d + yp * yp * dddrs_2 + 6.f * yp * c.p[0] + 2.f * xp * c.p[1]));
		return true;
	}
	void invert_2x2(const float J[2 * 2], float Jinv[2 * 2]) const
	{
		float detJ = J[0] * J[3] - J[1] * J[2];
		float inv_detJ = 1.f / detJ;

		Jinv[0] = inv_detJ * J[3];
		Jinv[3] = inv_detJ * J[0];
		Jinv[1] = -inv_detJ * J[1];
		Jinv[2] = -inv_detJ * J[2];
	}
	bool transformation_unproject_internal(const float uv[2], float xy[2], bool valid) const
	{
		const auto& c = *c_ptr;
		double xp_d = uv[0] - c.dc(0);
		double yp_d = uv[1] - c.dc(0);

		double r2 = xp_d * xp_d + yp_d * yp_d;
		double r4 = r2 * r2;
		double r6 = r2 * r4;
		double r8 = r4 * r4;
		double a = 1 + c.k[0] * r2 + c.k[1] * r4 + c.k[2] * r6;
		double b = 1 + c.k[3] * r2 + c.k[4] * r4 + c.k[5] * r6;
		double ai;
		if (a != 0.f) {
			ai = 1.f / a;
		}
		else {
			ai = 1.f;
		}
		float di = ai * b;
		// solve the radial and tangential distortion
		double x_u = xp_d * di;
		double y_u = yp_d * di;

		// approximate correction for tangential params
		float two_xy = 2.f * x_u * y_u;
		float xx = x_u * x_u;
		float yy = y_u * y_u;

		x_u -= (yy + 3.f * xx) * c.p[1] + two_xy * c.p[0];
		y_u -= (xx + 3.f * yy) * c.p[0] + two_xy * c.p[1];

		x_u += c.dc(0);
		y_u += c.dc(1);

		xy[0] = float(x_u);
		xy[1] = float(y_u);
		return transformation_iterative_unproject(uv, xy, valid, 20);
	}
};

/// frame size in pixels
struct frame_size
{
	/// width of frame in pixel
	int width;
	/// height of frame in pixel 
	int height;
};
/// format of individual pixels
enum PixelFormat {
	PF_I, // infrared

	/* TODO: add color formats in other color spaces like YUV */

	PF_RGB,   // 24 or 32 bit rgb format with byte alignment
	PF_BGR,   // 24 or 24 bit bgr format with byte alignment
	PF_RGBA,  // 32 bit rgba format
	PF_BGRA,  // 32 bit brga format
	PF_BAYER, // 8 bit per pixel, raw bayer pattern values

	PF_DEPTH,
	PF_DEPTH_AND_PLAYER,
	PF_POINTS_AND_TRIANGLES,
	PF_CONFIDENCE
};
/// format of a frame
struct frame_format : public frame_size
{
	/// format of pixels
	PixelFormat pixel_format;
	// total number of bits per pixel
	unsigned nr_bits_per_pixel;
	/// return number of bytes per pixel (ceil(nr_bits_per_pixel/8))
	unsigned get_nr_bytes_per_pixel() const;
	/// buffer size; returns width*height*get_nr_bytes_per_pixel()
	unsigned buffer_size;
	/// standard computation of the buffer size member
	void compute_buffer_size();
};
/// struct to store single frame
struct frame_info : public frame_format
{
	///
	unsigned frame_index;
	/// 
	double time;
};
/// struct to store single frame
struct frame_type : public frame_info
{
	/// vector with RAW frame data 
	std::vector<uint8_t> frame_data;
	/// check whether frame data is allocated
	bool is_allocated() const;
	/// write to file
	bool write(const std::string& fn) const;
	/// read from file
	bool read(const std::string& fn);
};
std::string get_frame_extension(const frame_format& ff)
{
	static const char* exts[] = {
		"ir", "rgb", "bgr", "rgba", "bgra", "byr", "dep", "d_p", "p_tri"
	};
	return std::string(exts[ff.pixel_format]) + to_string(ff.nr_bits_per_pixel);
}
std::string compose_file_name(const std::string& file_name, const frame_format& ff, unsigned idx)
{
	std::string fn = file_name;

	std::stringstream ss;
	ss << std::setfill('0') << std::setw(10) << idx;

	fn += ss.str();
	return fn + '.' + get_frame_extension(ff);
}
/// return number of bytes per pixel (ceil(nr_bits_per_pixel/8))
unsigned frame_format::get_nr_bytes_per_pixel() const
{
	return nr_bits_per_pixel / 8 + ((nr_bits_per_pixel & 7) == 0 ? 0 : 1);
}
/// standard computation of the buffer size member
void frame_format::compute_buffer_size()
{
	buffer_size = width * height * get_nr_bytes_per_pixel();
}
/// check whether frame data is allocated
bool frame_type::is_allocated() const
{
	return !frame_data.empty();
}
/// write to file
bool frame_type::write(const std::string& fn) const
{
	assert(buffer_size == frame_data.size());
	return
		cgv::utils::file::write(fn, reinterpret_cast<const char*>(this), sizeof(frame_format), false) &&
		cgv::utils::file::append(fn, reinterpret_cast<const char*>(&frame_data.front()), frame_data.size(), false);
}
/// read from file
bool frame_type::read(const std::string& fn)
{
	if (!cgv::utils::file::read(fn,
		reinterpret_cast<char*>(static_cast<frame_format*>(this)),
		sizeof(frame_format), false))
		return false;
	frame_data.resize(buffer_size);
	return
		cgv::utils::file::read(fn,
			reinterpret_cast<char*>(&frame_data.front()), buffer_size, false,
			sizeof(frame_format));
}

class camera_test :
	public node,
	public drawable,
	public provider
{
protected:
	// rendering configuration
	cgv::math::camera<double> color_calib, depth_calib;
	rgbd_kinect_azure rka;
	point_render_style prs;

	bool use_azure_impl = false;
	float slow_down = 1.0f;
	unsigned sub_sample = 1;
	unsigned sub_line_sample = 4;
	unsigned nr_iterations = 20;
	bool skip_dark = true;
	float scale = 1.0f;
	float  xu0_rad = 2.1f;
	bool debug_xu0 = false;
	double xu_xd_lambda = 1.0f;
	float depth_lambda = 1.0f;
	double error_threshold = 0.0001;
	double error_scale = 1000;
	bool debug_colors = true;
	float random_offset = 0.001f;

	std::vector<vec3> P;
	std::vector<rgb8> C;

	frame_type warped_color_frame;
	frame_type depth_frame;
	point_cloud pc;

	bool read_frames()
	{
		/*
		if (!depth_frame.read("D:/data/images/kinect/door.dep"))
			return false;
		if (!warped_color_frame.read("D:/data/images/kinect/door.wrgb"))
			return false;
		if (!pc.read("D:/data/images/kinect/door.bpc"))
			return false;
		*/
		if (!depth_frame.read("D:/data/Anton/me_in_office.dep"))
			return false;
		if (!warped_color_frame.read("D:/data/Anton/me_in_office.wrgb"))
			return false;
		if (!pc.read("D:/data/Anton/me_in_office.bpc"))
			return false;
		construct_point_clouds();
		return true;
	}
	void construct_point_clouds()
	{
		P.clear(); 
		C.clear();
		float sx = 1.0f / depth_frame.width;
		float sy = 1.0f / depth_frame.height;
		for (int y = 0; y < depth_frame.height; y += sub_sample) {
			for (int x = 0; x < depth_frame.width; x += sub_sample) {
				if (((x % sub_line_sample) != 0) && ((y % sub_line_sample) != 0))
					continue;
				uint8_t* pix_ptr = &warped_color_frame.frame_data[(y * depth_frame.width + x) * warped_color_frame.get_nr_bytes_per_pixel()];
				uint16_t depth = reinterpret_cast<const uint16_t&>(depth_frame.frame_data[(y * depth_frame.width + x) * depth_frame.get_nr_bytes_per_pixel()]);
				if (depth == 0)
					continue;
				//if (pix_ptr[0] + pix_ptr[1] + pix_ptr[2] < 16)
				//	continue;
				cgv::math::camera<double>::distortion_inversion_result dir;
				unsigned iterations = 1;
				dvec2 xu, xd;
				if (debug_xu0) {
					xd = xu0_rad * vec2(sx * x - 0.5f, sy * y - 0.5f);
					depth_calib.apply_distortion_model(xd, xu);
					std::default_random_engine g;
					std::uniform_real_distribution<double> d(-0.5, 0.5);
					xd += double(random_offset)*dvec2(d(g), d(g));
					dir= depth_calib.invert_distortion_model(xu, xd, false, &iterations, cgv::math::camera<double>::standard_epsilon, nr_iterations, slow_down);
				}
				else {
					xu = depth_calib.pixel_to_image_coordinates(dvec2(x, y));
					xd = xu;
					if (!use_azure_impl)
						dir = depth_calib.invert_distortion_model(xu, xd, true, &iterations, cgv::math::camera<double>::standard_epsilon, nr_iterations, slow_down);
					else {
						vec3 p;
						if (rka.map_depth_to_point(x, y, 0, &p[0])) {
							xd[0] = p[0];
							xd[1] = p[1];
							dir = cgv::math::camera<double>::distortion_inversion_result::convergence;
							/*double err = sqrt((xd[0] - p[0]) * (xd[0] - p[0]) + (xd[1] - p[1]) * (xd[1] - p[1]));
							if (err > 0.1f) {
								rka.map_depth_to_point(x, y, 0, &p[0]);
								nr = depth_calib.invert_distortion_model(xu, xd, slow_down, nr_iterations);
							}*/
						}
						else
							dir = cgv::math::camera<double>::distortion_inversion_result::divergence;
					}
				}
				vec3 p(vec2((1.0 - xu_xd_lambda) * xu + xu_xd_lambda * xd), 1.0);
				p *= float((1 - depth_lambda) + depth_lambda * 0.001 * depth);
				P.push_back(p);

				dvec2 xu_rec;
				dmat2 J;
				depth_calib.apply_distortion_model(xd, xu_rec, &J);
				double error = (xu_rec - xu).length();
				if (!debug_colors)
					C.push_back(rgb8(pix_ptr[2], pix_ptr[1], pix_ptr[0]));
				else {
					switch (dir) {
					case cgv::math::camera<double>::distortion_inversion_result::divergence:
						C.push_back(rgb8(255, 0, 255));
						break;
					case cgv::math::camera<double>::distortion_inversion_result::division_by_zero:
						C.push_back(rgb8(0, 255, 255));
						break;
					case cgv::math::camera<double>::distortion_inversion_result::max_iterations_reached:
						if (error > error_threshold)
							C.push_back(rgb8(cgv::media::color_scale(error_scale * error)));
						else {
							C.push_back(rgb8(pix_ptr[2], pix_ptr[1], pix_ptr[0]));
						}
						break;
					case cgv::math::camera<double>::distortion_inversion_result::out_of_bounds:
						C.push_back(rgb8(128, 128, 255));
						break;
					default:
						C.push_back(rgb8(pix_ptr[2], pix_ptr[1], pix_ptr[0]));
						break;
					}
				}
			}
		}
	}

	bool read_calibs()
	{
		nlohmann::json j;
		std::ifstream is("D:/data/Anton/me_in_office.json");
		if (is.fail())
			return false;
		is >> j;
		std::string serial;
		j.at("serial").get_to(serial);
		j.at("color_calib").get_to(color_calib);
		j.at("depth_calib").get_to(depth_calib);
		depth_calib.max_radius_for_projection = 1.7f;
		color_calib.max_radius_for_projection = 1.7f;
		return true;
	}
	attribute_array_manager pc_aam;
	cgv::render::view* view_ptr = 0;
public:
	camera_test()
	{
		rka.c_ptr = &depth_calib;
		set_name("camera_test");
		prs.point_size = 5;
		prs.blend_width_in_pixel = 0;
	}
	void on_set(void* member_ptr)
	{
		if (member_ptr == &sub_sample || member_ptr == &nr_iterations || member_ptr == &slow_down ||
			member_ptr == & sub_line_sample ||
			member_ptr == & xu0_rad ||
			member_ptr == &random_offset ||
			member_ptr == & scale ||
			member_ptr == & use_azure_impl ||
			member_ptr == & debug_colors ||
			member_ptr == & xu_xd_lambda ||
			member_ptr == &depth_calib.max_radius_for_projection ||
			member_ptr == &debug_xu0 ||
			member_ptr == &depth_lambda ||
			member_ptr == & error_threshold ||
			member_ptr == & error_scale) {
			construct_point_clouds();
		}
		update_member(member_ptr);
		post_redraw();
	}
	std::string get_type_name() const { return "camera_test"; }

	void create_gui()
	{
		add_decorator("a_buffer", "heading", "level=1");
		add_member_control(this, "sub_sample", sub_sample, "value_slider", "min=1;max=16;ticks=true");
		add_member_control(this, "sub_line_sample", sub_line_sample, "value_slider", "min=1;max=16;ticks=true");
		add_member_control(this, "use_azure_impl", use_azure_impl, "toggle");
		add_member_control(this, "slow_down", slow_down, "value_slider", "min=0;max=1;ticks=true");
		add_member_control(this, "nr_iterations", nr_iterations, "value_slider", "min=0;max=30;ticks=true");
		add_member_control(this, "debug_colors", debug_colors, "toggle");
		add_member_control(this, "scale", scale, "value_slider", "min=0.5;max=10;ticks=true");
		add_member_control(this, "max_radius_for_projection", depth_calib.max_radius_for_projection, "value_slider", "min=0.5;max=10;ticks=true");
		add_member_control(this, "debug_xu0", debug_xu0, "toggle");
		add_member_control(this, "random_offset", random_offset, "value_slider", "min=0.00001;max=0.01;step=0.0000001;ticks=true;log=true");
		add_member_control(this, "xu0_rad", xu0_rad, "value_slider", "min=0.5;max=5;ticks=true");
		add_member_control(this, "xu_xd_lambda", xu_xd_lambda, "value_slider", "min=0;max=1;ticks=true");
		add_member_control(this, "depth_lambda", depth_lambda, "value_slider", "min=0;max=1;ticks=true");
		add_member_control(this, "error_threshold", error_threshold, "value_slider", "min=0.000001;max=0.1;step=0.0000001;log=true;ticks=true");
		add_member_control(this, "error_scale", error_scale, "value_slider", "min=1;max=10000;log=true;ticks=true");

		if (begin_tree_node("point style", prs)) {
			align("\a");
			add_gui("point style", prs);
			align("\a");
			end_tree_node(prs);
		}

	}
	bool init(context& ctx)
	{
		ctx.set_bg_clr_idx(4);
		ref_point_renderer(ctx, 1);
		read_calibs();
		read_frames();
		return true;
	}
	void destruct(context& ctx)
	{
		ref_point_renderer(ctx, -1);
	}
	void init_frame(context& ctx)
	{
		if (!view_ptr)
			view_ptr = find_view_as_node();
		pc_aam.init(ctx);
	}
	void draw_points(context& ctx, size_t nr_elements, const vec3* P, const rgb8* C)
	{
		auto& pr = ref_point_renderer(ctx);
		if (view_ptr)
			pr.set_y_view_angle(float(view_ptr->get_y_view_angle()));
		pr.set_render_style(prs);
		pr.enable_attribute_array_manager(ctx, pc_aam);
		pr.set_position_array(ctx, P, nr_elements);
		pr.set_color_array(ctx, C, nr_elements);
		glDisable(GL_CULL_FACE);
		pr.render(ctx, 0, nr_elements);
		glEnable(GL_CULL_FACE);
	}
	void draw(context& ctx)
	{
		draw_points(ctx, P.size(), &P.front(), &C.front());
		//draw_points(ctx, pc.get_nr_points(), &pc.pnt(0), &pc.clr(0));
	}
};

object_registration<camera_test> camera_reg("");