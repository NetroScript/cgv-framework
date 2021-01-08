#include "plot3d.h"
#include <libs/cgv_gl/gl/gl.h>

namespace cgv {
	namespace plot {


void plot3d_config::set_colors(const rgb& base_color)
{
	plot_base_config::set_colors(base_color);
	surface_color = 0.1f * rgb(1, 1, 1) + 0.9f * base_color;
}

/// overloaded in derived classes to compute complete tick render information
void plot3d::compute_tick_render_information()
{
	collect_tick_geometry(0, 1, &domain_min(0), &domain_max(0), &extent(0));
	collect_tick_geometry(0, 2, &domain_min(0), &domain_max(0), &extent(0));
	collect_tick_geometry(1, 0, &domain_min(0), &domain_max(0), &extent(0));
	collect_tick_geometry(1, 2, &domain_min(0), &domain_max(0), &extent(0));
	collect_tick_geometry(2, 0, &domain_min(0), &domain_max(0), &extent(0));
	collect_tick_geometry(2, 1, &domain_min(0), &domain_max(0), &extent(0));
}

plot3d_config::plot3d_config(const std::string& _name) : plot_base_config(_name)
{
	show_points = true;
	show_lines = true;
	show_line_orientation = true;
	show_bars = false;
	samples_per_row = 0;
	show_surface = true;
	wireframe = false;
	surface_color = rgb(0.7f,0.4f,0);
	face_illumination = PFI_PER_FACE;
	bar_percentual_depth = 1.0f;
}

void plot3d::set_uniforms(cgv::render::context& ctx, cgv::render::shader_program& prog, unsigned i)
{
	plot_base::set_uniforms(ctx, prog, i);
	if (i >= 0 && i < get_nr_sub_plots()) {
		const plot3d_config& spc = ref_sub_plot3d_config(i);
	}
}

bool plot3d::compute_sample_coordinate_interval(int i, int ai, float& samples_min, float& samples_max)
{
	// compute bounding box
	bool found_sample = false;
	float min_value, max_value;
	for (unsigned j = 0; j < samples[i].size(); ++j) {
		if (found_sample) {
			min_value = std::min(min_value, samples[i][j](ai));
			max_value = std::max(max_value, samples[i][j](ai));
		}
		else {
			min_value = samples[i][j](ai);
			max_value = samples[i][j](ai);
			found_sample = true;
		}
	}
	if (found_sample) {
		samples_min = min_value;
		samples_max = max_value;
		return true;
	}
	return false;
}

/// construct empty plot with default domain [0..1,0..1,0..1]
plot3d::plot3d() : plot_base(3)
{
	brs.culling_mode = cgv::render::CM_FRONTFACE;
	brs.map_color_to_material = cgv::render::CM_COLOR;
	brs.illumination_mode = cgv::render::IM_TWO_SIDED;
}

unsigned plot3d::add_sub_plot(const std::string& name)
{
	// determine index of new sub plot
	unsigned i = get_nr_sub_plots();

	// create new config
	if (i == 0)
		configs.push_back(new plot3d_config(name));
	else {
		configs.push_back(new plot3d_config(ref_sub_plot3d_config(i - 1)));
		ref_sub_plot_config(i).name = name;
	}

	// create new point container
	samples.push_back(std::vector<vec3>());
	attribute_sources.push_back(std::vector<attribute_source>());
	attribute_sources.back().push_back(attribute_source(i, 0, 0, 3 * sizeof(float)));
	attribute_sources.back().push_back(attribute_source(i, 1, 0, 3 * sizeof(float)));
	attribute_sources.back().push_back(attribute_source(i, 2, 0, 3 * sizeof(float)));

	// return sub plot index
	return i;
}

void plot3d::delete_sub_plot(unsigned i)
{
	delete configs[i];
	configs[i] = 0;
	configs.erase(configs.begin() + i);
	samples.erase(samples.begin() + i);
}

/// set the number of samples of the i-th sub plot to N
void plot3d::set_samples_per_row(unsigned i, unsigned N)
{
	ref_sub_plot3d_config(i).samples_per_row = N;
}

/// return the number of samples per row
unsigned plot3d::get_samples_per_row(unsigned i) const
{
	return const_cast<plot3d*>(this)->ref_sub_plot3d_config(i).samples_per_row;
}

/// return a reference to the plot base configuration of the i-th plot
plot3d_config& plot3d::ref_sub_plot3d_config(unsigned i)
{
	return static_cast<plot3d_config&>(ref_sub_plot_config(i));
}


/// return the samples of the i-th sub plot
std::vector<plot3d::vec3>& plot3d::ref_sub_plot_samples(unsigned i)
{
	return samples[i];
}


bool plot3d::init(cgv::render::context& ctx)
{
	cgv::render::ref_box_renderer(ctx, 1);
	return true;
}

void plot3d::draw_sub_plot(cgv::render::context& ctx, unsigned i)
{
	size_t count = set_attributes(ctx, i, samples);
	if (count == 0)
		return;
	const plot3d_config& spc = ref_sub_plot3d_config(i);
	float size2radius = (extent(0) + extent(1)) / (2000.0f);
	if (spc.show_points) {
		set_uniforms(ctx, sphere_prog, i);
		sphere_prog.set_uniform(ctx, "radius_scale", spc.point_size*size2radius);
		sphere_prog.set_uniform(ctx, "map_color_to_material", 3);
		sphere_prog.enable(ctx);
			ctx.set_color(spc.point_color);
			sphere_prog.set_attribute(ctx, "att0", 1.0f);
			draw_sub_plot_samples(int(count), spc);
		sphere_prog.disable(ctx);
	}
	
	if (spc.show_bars) {
		if (spc.bar_outline_width > 0) {
			glLineWidth(spc.bar_outline_width);
			set_uniforms(ctx, wirebox_prog, i);
			wirebox_prog.set_uniform(ctx, "percentual_width", spc.bar_percentual_width);
			wirebox_prog.set_uniform(ctx, "percentual_depth", spc.bar_percentual_depth);
			if (spc.samples_per_row == 0) {
				wirebox_prog.set_uniform(ctx, "N", (int)count);
				wirebox_prog.set_uniform(ctx, "M", (int)count);
			}
			else {
				wirebox_prog.set_uniform(ctx, "N", (int)spc.samples_per_row);
				wirebox_prog.set_uniform(ctx, "M", (int)(count / spc.samples_per_row));
			}
			wirebox_prog.enable(ctx);
			ctx.set_color(spc.bar_outline_color);
			draw_sub_plot_samples(int(count), spc);
			wirebox_prog.disable(ctx);
		}
		set_uniforms(ctx, box_prog, i);
		box_prog.set_uniform(ctx, "percentual_width", spc.bar_percentual_width);
		box_prog.set_uniform(ctx, "percentual_depth", spc.bar_percentual_depth);
		if (spc.samples_per_row == 0) {
			box_prog.set_uniform(ctx, "N", (int)count);
			box_prog.set_uniform(ctx, "M", (int)count);
		}
		else {
			box_prog.set_uniform(ctx, "N", (int&)spc.samples_per_row);
			box_prog.set_uniform(ctx, "M", (int)(count / spc.samples_per_row));
		}
		box_prog.enable(ctx);
			ctx.set_color(spc.bar_color);
			box_prog.set_uniform(ctx, "map_color_to_material", 3);
			draw_sub_plot_samples(int(count), spc);
		box_prog.disable(ctx);
	}
	if (spc.show_sticks) {
		set_uniforms(ctx, stick_prog, i);
		glLineWidth(spc.stick_width);
		stick_prog.enable(ctx);
			ctx.set_color(spc.stick_color);
			draw_sub_plot_samples(int(count), spc);
		stick_prog.disable(ctx);
	}
	if (spc.show_lines) {
		set_uniforms(ctx, tube_prog, i);
		tube_prog.set_uniform(ctx, "radius", spc.line_width*size2radius);
		tube_prog.enable(ctx);
		ctx.set_color(spc.line_color);
		draw_sub_plot_samples(int(count), spc, true);
		tube_prog.disable(ctx);
	}
}

void plot3d::draw_domain(cgv::render::context& ctx)
{
	std::vector<vec3> P;
	const domain_config& dc = *get_domain_config_ptr();
	if (dc.fill) {
		cgv::render::box_renderer& br = cgv::render::ref_box_renderer(ctx);
		br.set_attribute_array_manager(ctx, 0);
		br.set_position_array(ctx, &center_location, 1);
		br.set_position_is_center(true);
		br.set_extent(ctx, extent);
		br.set_render_style(brs);
		if (br.validate_and_enable(ctx)) {
			ctx.set_color(dc.color);
			glDrawArrays(GL_POINTS, 0, 1);
			br.disable(ctx);
		}
	}
}

void plot3d::draw_axes(cgv::render::context& ctx)
{
	std::vector<vec3> P;
	for (unsigned ai = 0; ai < 3; ++ai) {
		unsigned aj = (ai + 1) % 3;
		unsigned ak = (ai + 2) % 3;
		axis_config& ac = get_domain_config_ptr()->axis_configs[ai];
		axis_config& ao = get_domain_config_ptr()->axis_configs[aj];
		axis_config& ap = get_domain_config_ptr()->axis_configs[ak];
		ctx.set_color(ac.color);
		glLineWidth(ac.line_width);
		// 4 lines 
		vec3 p(domain_min.size(), &domain_min(0)); P.push_back(p);
		p(ai) = domain_max(ai); P.push_back(p);
		p(aj) = domain_max(aj); P.push_back(p);
		p(ai) = domain_min(ai); P.push_back(p);
		p(ak) = domain_max(ak); P.push_back(p);
		p(ai) = domain_max(ai); P.push_back(p);
		p(aj) = domain_min(aj); P.push_back(p);
		p(ai) = domain_min(ai); P.push_back(p);
		// axis line
		if (domain_min(aj) < 0 && domain_max(aj) > 0) {
			p(aj) = 0.0f; P.push_back(p);
			p(ai) = domain_max(ai); P.push_back(p);
			p(ak) = domain_min(ak); P.push_back(p);
			p(ai) = domain_min(ai); P.push_back(p);
		}
		if (domain_min(ak) < 0 && domain_max(ak) > 0) {
			p(ak) = 0.0f;
			p(aj) = domain_min(aj); P.push_back(p);
			p(ai) = domain_max(ai); P.push_back(p);
			p(aj) = domain_max(aj); P.push_back(p);
			p(ai) = domain_min(ai); P.push_back(p);
		}
		if (!P.empty()) {
			set_attributes(ctx, P);
			glDrawArrays(GL_LINES, 0, GLsizei(P.size()));
			P.clear();
		}
	}
}

void plot3d::draw_ticks(cgv::render::context& ctx)
{
	if (tick_vertices.empty())
		return;
	enable_attributes(ctx, 2);
	tick_label_prog.enable(ctx);
		set_uniforms(ctx, tick_label_prog, -1);
		set_attributes(ctx, tick_vertices);
		for (const auto& tbc : tick_batches) if (tbc.vertex_count > 0) {
			tick_label_prog.set_uniform(ctx, "ai", tbc.ai);
			tick_label_prog.set_uniform(ctx, "aj", tbc.aj);
			int ao = 0;
			while (tbc.ai == ao || tbc.aj == ao)
				++ao;
			tick_label_prog.set_uniform(ctx, "default_value", domain_min[ao]);
			const axis_config& ac = get_domain_config_ptr()->axis_configs[tbc.ai];
			const tick_config& tc = tbc.primary ? ac.primary_ticks : ac.secondary_ticks;
			glLineWidth(tc.line_width);
			ctx.set_color(ac.color);
			glDrawArrays(GL_LINES, tbc.first_vertex, tbc.vertex_count);
		}
	tick_label_prog.disable(ctx);
	disable_attributes(ctx, 2);
}

void plot3d::draw_tick_labels(cgv::render::context& ctx)
{
	if (tick_labels.empty())
		return;
	set_attributes(ctx, tick_vertices);
	for (const auto& tbc : tick_batches) if (tbc.label_count > 0) {
		ctx.set_color(get_domain_config_ptr()->axis_configs[tbc.ai].color);
		for (unsigned i = tbc.first_label; i < tbc.first_label + tbc.label_count; ++i) {
			int a0 = tbc.ai;
			int a1 = tbc.aj;
			if (a0 == 2)
				a0 = 1 - a1;
			if (a1 == 2)
				a1 = 1 - a0;
			const label_info& li = tick_labels[i];
			vec3 p(0.0f);
			p(tbc.ai) = li.position(a0);
			p(tbc.aj) = li.position(a1);
			int ao = 0;
			while (tbc.ai == ao || tbc.aj == ao)
				++ao;
			p(ao) = domain_min[ao];
			ctx.set_cursor(transform_to_world(p.to_vec()).to_vec(), li.label, li.align);
			ctx.output_stream() << li.label;
			ctx.output_stream().flush();
		}
	}
	disable_attributes(ctx, 2);
}

void plot3d::draw(cgv::render::context& ctx)
{
	if (!prog.is_created()) {
		if (!prog.build_program(ctx, "plot3d.glpr")) {
			std::cerr << "could not build GLSL program from plot3d.glpr" << std::endl;
		}
	}
	if (!sphere_prog.is_created()) {
		if (!sphere_prog.build_program(ctx, "plot3d_sphere.glpr")) {
			std::cerr << "could not build GLSL program from plot3d_sphere.glpr" << std::endl;
		}
	}
	if (!stick_prog.is_created()) {
		if (!stick_prog.build_program(ctx, "plot3d_stick.glpr")) {
			std::cerr << "could not build GLSL program from plot3d_stick.glpr" << std::endl;
		}
	}
	if (!tick_label_prog.is_created()) {
		if (!tick_label_prog.build_program(ctx, "plot3d_tick_label.glpr")) {
			std::cerr << "could not build GLSL program from plot3d_tick_label.glpr" << std::endl;
		}
	}
	
	if (!box_prog.is_created()) {
		if (!box_prog.build_program(ctx, "plot3d_box.glpr")) {
			std::cerr << "could not build GLSL program from plot3d_box.glpr" << std::endl;
		}
	}
	if (!wirebox_prog.is_created()) {
		if (!wirebox_prog.build_program(ctx, "plot3d_box_wire.glpr")) {
			std::cerr << "could not build GLSL program from plot3d_box_wire.glpr" << std::endl;
		}
	}
	if (!tube_prog.is_created()) {
		if (!tube_prog.build_program(ctx, "plot3d_tube.glpr", true)) {
			std::cerr << "could not build GLSL program from plot3d_tube.glpr" << std::endl;
		}
		else {
			tube_prog.set_uniform(ctx, "map_color_to_material", 3);
		}
	}
	//if (!surface_prog.is_created()) {
	//	if (!surface_prog.build_program(ctx, "plot3d_surface.glpr")) {
	//		std::cerr << "could not build GLSL program from plot3d_surface.glpr" << std::endl;
	//	}
	//}

	GLboolean line_smooth = glIsEnabled(GL_LINE_SMOOTH); glEnable(GL_LINE_SMOOTH);
	GLboolean point_smooth = glIsEnabled(GL_POINT_SMOOTH); glEnable(GL_POINT_SMOOTH);
	GLboolean blend = glIsEnabled(GL_BLEND); glEnable(GL_BLEND);
	GLenum blend_src, blend_dst, depth;
	glGetIntegerv(GL_BLEND_DST, reinterpret_cast<GLint*>(&blend_dst));
	glGetIntegerv(GL_BLEND_SRC, reinterpret_cast<GLint*>(&blend_src));
	glGetIntegerv(GL_DEPTH_FUNC, reinterpret_cast<GLint*>(&depth));
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDepthFunc(GL_LEQUAL);
	
	enable_attributes(ctx, 3);
	for (unsigned i = 0; i < samples.size(); ++i) {
		// skip unvisible and empty sub plots
		if (!ref_sub_plot3d_config(i).show_plot)
			continue;
		draw_sub_plot(ctx, i);
	}
	disable_attributes(ctx, 3);
	
	if (get_domain_config_ptr()->show_domain) {
		draw_domain(ctx);
		
		enable_attributes(ctx, 3);
			set_uniforms(ctx, prog, -1);
			prog.enable(ctx);
				draw_axes(ctx);
			prog.disable(ctx);
		disable_attributes(ctx, 3);
		
		draw_ticks(ctx);
		
		ctx.enable_font_face(label_font_face, get_domain_config_ptr()->label_font_size);
		draw_tick_labels(ctx);
		
	}
	
	if (!line_smooth)
		glDisable(GL_LINE_SMOOTH);
	if (!point_smooth)
		glDisable(GL_POINT_SMOOTH);
	if (!blend)
		glDisable(GL_BLEND);
	glDepthFunc(depth);
	glBlendFunc(blend_src, blend_dst);
}

void plot3d::clear(cgv::render::context& ctx)
{
	sphere_prog.destruct(ctx);
	box_prog.destruct(ctx);
	wirebox_prog.destruct(ctx);
	stick_prog.destruct(ctx);
	tube_prog.destruct(ctx);
//	surface_prog.destruct(ctx);
	cgv::render::ref_box_renderer(ctx, -1);

}

void plot3d::create_config_gui(cgv::base::base* bp, cgv::gui::provider& p, unsigned i)
{
	plot3d_config& pbc = ref_sub_plot3d_config(i);
	create_config_gui_impl(bp, p, i, "op");
	bool show = p.begin_tree_node("lines", pbc.show_lines, false, "level=3;options='w=142';align=' '");
	p.add_member_control(bp, "show", pbc.show_lines, "toggle", "w=50");
	if (show) {
		p.align("\a");
		p.add_member_control(bp, "width", pbc.line_width, "value_slider", "min=1;max=20;log=true;ticks=true");
		p.add_member_control(bp, "color", pbc.line_color);
		p.add_member_control(bp, "show_orientation", pbc.show_line_orientation, "check");
		p.align("\b");
		p.end_tree_node(pbc.show_lines);
	}
	create_config_gui_impl(bp, p, i, "s");
	show = p.begin_tree_node("bars", pbc.show_bars, false, "level=3;options='w=142';align=' '");
	p.add_member_control(bp, "show", pbc.show_bars, "toggle", "w=50");
	if (show) {
		p.align("\a");
		p.add_member_control(bp, "width", pbc.bar_percentual_width, "value_slider", "min=0.01;max=1;log=true;ticks=true");
		p.add_member_control(bp, "depth", pbc.bar_percentual_depth, "value_slider", "min=0.01;max=1;log=true;ticks=true");
		p.add_member_control(bp, "fill", pbc.bar_color);
		p.add_member_control(bp, "outline_width", pbc.bar_outline_width, "value_slider", "min=0;max=20;log=true;ticks=true");
		p.add_member_control(bp, "outline", pbc.bar_outline_color);
		p.align("\b");
		p.end_tree_node(pbc.show_bars);
	}
	show = p.begin_tree_node("surface", pbc.show_surface, false, "level=3;w=100;align=' '");
	p.add_member_control(bp, "show", pbc.show_surface, "toggle", "w=50");
	if (show) {
		p.align("\a");
		p.add_view("samples per row", pbc.samples_per_row);
		p.add_member_control(bp, "wireframe", pbc.wireframe, "check");
		p.add_member_control(bp, "color", pbc.surface_color);
		p.add_member_control(bp, "wireframe", pbc.face_illumination, "dropdown", "enums='none,face,vertex'");
	}	
}


	}
}