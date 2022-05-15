#pragma once

#include "view_overlay.h"
#include <libs/plot/plot_base.h>
#include "lib_begin.h"

namespace stream_vis {

	struct view2d_update_handler : public cgv::render::render_types
	{
		virtual void handle_view2d_update(int pi, const vec2& pixel_scales) = 0;
	};

	class CGV_API view2d_overlay : public view_overlay
	{
	protected:
		int pan_start_x = 0, pan_start_y = 0;
		vec2 pan_start_pos = vec2(0.0f);
		float zoom_factor = 1.0f;
		float view_width = 2.0f;
		vec2 pan_pos = vec2(0.0f);
		vec2 initial_pan_pos = vec2(0.0f);
		float initial_zoom_factor = 1.0f;
		float zoom_sensitivity = 0.1f;
		view2d_update_handler* handler = 0;
		void update_views();
		void set_modelview_projection(cgv::render::context& ctx);
		void toggle_default_view(bool set_default);
		bool handle_mouse_event(const cgv::gui::mouse_event& me);
	public:
		view2d_overlay();
		void set_update_handler(view2d_update_handler* _handler);
		std::string get_type_name() const { return "view2d_overlay"; }
		void on_set(void* member_ptr);
		void create_gui();
	};
}

#include <cgv/config/lib_end.h>