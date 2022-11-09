#pragma once

#include <cgv/render/color_map.h>
#include <cgv/utils/convert_string.h>
#include <cgv/utils/stopwatch.h>
#include <cgv_app/canvas_overlay.h>
#include <cgv_g2d/generic_2d_renderer.h>
#include <cgv_g2d/msdf_gl_canvas_font_renderer.h>

#include "lib_begin.h"

namespace cgv {
namespace app {

class CGV_API performance_monitor : public canvas_overlay {
protected:
	struct layout_attributes {
		int padding;
		ivec2 total_size;

		// dependent members
		cgv::g2d::rect content_rect;
		cgv::g2d::rect plot_rect;
		
		void update(const ivec2& parent_size) {
			
			content_rect.set_pos(padding, padding);
			content_rect.set_size(total_size - 2 * padding);

			plot_rect = content_rect;
			plot_rect.set_h(30);
		}
	} layout;

	/// measuring state fields
	struct {
		/// whether measuring is enabled
		bool enabled = true;
		/// timer to count elapsed time
		cgv::utils::stopwatch timer;
		/// counter for rendered frames since start of measurements
		unsigned total_frame_count = 0u;
		/// counter for rendered frames incurrent interval
		unsigned interval_frame_count = 0u;
		/// time interval for measurement averaging
		double interval = 0.25;
		/// store total seconds since the last measuring
		double last_seconds_since_start = 0.0;
		/// store time of the last rendered frame in seconds
		double delta_time = 0.0;
		/// store time of current running interval in seconds
		double running_time = 0.0;
		/// store the average frames per second
		double avg_fps = 0.0;
	} monitor;

	bool show_background = true;
	bool invert_color = false;
	bool show_plot = true;

	cgv::g2d::generic_2d_renderer bar_renderer;
	DEFINE_GENERIC_RENDER_DATA_CLASS(bar_geometry, 3, vec2, position, vec2, size, rgb, color);
	bar_geometry bars;

	// general appearance
	cgv::g2d::shape2d_style container_style, border_style, bar_style;
	cgv::g2d::line2d_style line_style;
	cgv::render::color_map plot_color_map;

	// text appearance
	float text_font_size = 12.0f;
	float label_font_size = 10.0f;
	cgv::g2d::shape2d_style text_style, label_style;
	cgv::g2d::msdf_text_geometry texts;
	cgv::g2d::msdf_font label_font;
	cgv::g2d::msdf_text_geometry labels;

	void init_styles(cgv::render::context& ctx);
	void create_texts();
	void update_stats_texts();
	void create_labels();
	void update_plot();

	virtual void create_gui_impl();

public:
	performance_monitor();
	std::string get_type_name() const { return "performance_monitor"; }

	void clear(cgv::render::context& ctx);

	bool self_reflect(cgv::reflect::reflection_handler& _rh);
	void stream_help(std::ostream& os) {}

	bool handle_event(cgv::gui::event& e);
	void on_set(void* member_ptr);

	bool init(cgv::render::context& ctx);
	void init_frame(cgv::render::context& ctx);
	void draw_content(cgv::render::context& ctx);
	void after_finish(cgv::render::context& ctx);

	void enable_monitoring(bool enabled);
};

typedef cgv::data::ref_ptr<performance_monitor> performance_monitor_ptr;

}
}

#include <cgv/config/lib_end.h>
