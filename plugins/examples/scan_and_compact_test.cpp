#include <random>

#include <cgv/base/node.h>
#include <cgv/gui/provider.h>
#include <cgv/render/drawable.h>
#include <cgv_gl/sphere_render_data.h>
#include <cgv_gpgpu/scan_and_compact.h>

class scan_and_compact_test : public cgv::base::node, public cgv::render::drawable, public cgv::gui::provider {
protected:
	cgv::render::view* view_ptr;

	unsigned n;

	float rad_min = 0.005f;
	float rad_max = 0.05f;

	float threshold_min = rad_min;
	float threshold_max = rad_max;

	cgv::render::sphere_render_style sphere_style;
	cgv::render::sphere_render_data<> spheres;

	cgv::gpgpu::scan_and_compact sac;
	bool do_sort;

public:
	scan_and_compact_test() : cgv::base::node("scan and compact test")
	{
		view_ptr = nullptr;
		n = 10000;
		sphere_style.surface_color = rgb(0.5f);
		sphere_style.map_color_to_material = CM_COLOR;
		do_sort = true;
	}
	void on_set(void* member_ptr)
	{
		post_redraw();
		update_member(member_ptr);
	}
	std::string get_type_name() const
	{
		return "scan_and_compact_test";
	}
	void clear(cgv::render::context& ctx)
	{
		cgv::render::ref_sphere_renderer(ctx, -1);
		spheres.destruct(ctx);

		sac.destruct(ctx);
	}
	bool init(cgv::render::context& ctx)
	{
		cgv::render::ref_sphere_renderer(ctx, 1);
		if(!spheres.init(ctx))
			return false;
		
		sac.set_data_type_override("float x;");
		sac.set_uniform_definition_override("uniform float threshold_min, threshold_max;");
		sac.set_vote_definition_override("return value.x >= threshold_min && value.x <= threshold_max;");
		sac.set_mode(cgv::gpgpu::scan_and_compact::M_CREATE_INDICES);

		create_data();

		view_ptr = find_view_as_node();

		return true;
	}
	void init_frame(cgv::render::context& ctx)
	{
		if(!view_ptr)
			view_ptr = find_view_as_node();
	}
	void draw(cgv::render::context& ctx)
	{	
		if(!view_ptr || spheres.ref_pos().size() == 0) return;
		
		auto& sr = ref_sphere_renderer(ctx);
		spheres.early_transfer(ctx, sr);

		int rad_handle = 0;
		int idx_handle = 0;
		auto& aam = spheres.ref_aam();
		
		rad_handle = sr.get_vbo_handle(ctx, aam, "radius");
		idx_handle = sr.get_index_buffer_handle(aam);
		
		if(sac.is_initialized()) {
			if(rad_handle > 0 && idx_handle > 0) {
				auto& vote_prog = sac.ref_vote_prog();
				vote_prog.enable(ctx);
				vote_prog.set_uniform(ctx, "threshold_min", threshold_min);
				vote_prog.set_uniform(ctx, "threshold_max", threshold_max);


				sac.begin_time_query();
				unsigned count = sac.execute(ctx, rad_handle, idx_handle);
				float time = sac.end_time_query();
				std::cout << "Filtering done in " << time << " ms. Drawing " << count << " of " << spheres.ref_pos().size() << " spheres." << std::endl;

				if(count > 0)
					spheres.render(ctx, sr, sphere_style, 0, count);
			}
		} else {
			std::cout << "Warning: GPU scan and compact is not initialized." << std::endl;
			spheres.render(ctx, sr, sphere_style);
		}
	}
	void create_gui()
	{
		add_decorator("Scan and Compact", "heading");

		add_member_control(this, "N", n, "value_slider", "min=1000;max=10000000;ticks=true");
		connect_copy(add_button("Generate")->click, cgv::signal::rebind(this, &scan_and_compact_test::create_data));

		std::string options = "min=" + std::to_string(rad_min) + ";max=" + std::to_string(rad_max) + ";step=0.0001;ticks=true";
		add_decorator("Radius Range", "heading", "level=3");
		add_member_control(this, "Min", threshold_min, "value_slider", options);
		add_member_control(this, "Max", threshold_max, "value_slider", options);

		if(begin_tree_node("Sphere Style", sphere_style, false)) {
			align("\a");
			add_gui("", sphere_style);
			align("\b");
			end_tree_node(sphere_style);
		}
	}
	void create_data()
	{
		auto& ctx = *get_context();

		spheres.clear();

		std::mt19937 rng(42);
		std::uniform_real_distribution<float> snorm_distr(-1.0f, 1.0f);

		rgb green(0.0f, 1.0f, 0.0f);
		rgb red(1.0f, 0.0f, 0.0f);

		for(unsigned i = 0; i < n; ++i) {
			vec3 pos(
				snorm_distr(rng),
				snorm_distr(rng),
				snorm_distr(rng)
			);

			float t = 0.5f * snorm_distr(rng) + 0.5f;
			float rad = cgv::math::lerp(rad_min, rad_max, t);

			rgb col = (1.0f - t)*green + t*red;

			spheres.add(pos);
			spheres.add(rad);
			spheres.add(col);

			spheres.add_idx(i);
		}

		if(!sac.init(ctx, spheres.ref_rad().size()))
			std::cout << "Error: Could not initialize GPU scan and compact algorithm!" << std::endl;

		spheres.set_out_of_date();
		post_redraw();
	}
};

#include <cgv/base/register.h>

/// register a factory to create new visibility sorting tests
cgv::base::factory_registration<scan_and_compact_test> test_scan_and_compact_fac("New/GPGPU/Scan and Compact");
