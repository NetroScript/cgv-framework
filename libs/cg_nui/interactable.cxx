#include "interactable.h"

void cgv::nui::interactable::change_state(state_enum new_state)
{
	std::string lut[7]{ "idle", "close", "pointed", "grabbed", "triggered" };
	std::cout << name << " changed state: " << lut[static_cast<int>(state)] << " -> " << lut[static_cast<int>(new_state)] << std::endl;
	switch (new_state) {
	case state_enum::idle:
		if (state == state_enum::grabbed)
			on_grabbed_stop();
		else if (state == state_enum::close)
			on_close_stop();
		else if (state == state_enum::triggered)
			on_triggered_stop();
		else if (state == state_enum::pointed)
			on_pointed_stop();
		state = new_state;
		on_set(&state);
	case state_enum::close:
		if (state == state_enum::grabbed)
			on_grabbed_stop();
		else if (state == state_enum::pointed)
			on_pointed_stop();
		state = new_state;
		on_set(&state);
		on_close_start();
		break;
	case state_enum::pointed:
		if (state == state_enum::triggered)
			on_triggered_stop();
		else if (state == state_enum::close)
			on_close_stop();
		state = new_state;
		on_set(&state);
		on_pointed_start();
		break;
	case state_enum::grabbed:
		state = new_state;
		on_set(&state);
		on_grabbed_start();
		break;
	case state_enum::triggered:
		state = new_state;
		on_set(&state);
		on_triggered_start();
		break;
	}
}

cgv::nui::interactable::interactable(const std::string& name) :
	group(name),
	ii_during_focus(vec3(0.0), vec3(0.0), vec3(1.0, 0.0, 0.0), true),
	ii_at_grab(vec3(0.0), vec3(0.0), vec3(1.0, 0.0, 0.0), true),
	focus_debug_point(vec3(0.0f))
{
	debug_sphere_rs.radius = 0.01f;
}

void cgv::nui::interactable::on_set(void* member_ptr)
{
	update_member(member_ptr);
	post_redraw();
}

void cgv::nui::interactable::stream_help(std::ostream& os)
{
	os << "interactable: grab and point at it" << std::endl;
}

bool cgv::nui::interactable::focus_change(cgv::nui::focus_change_action action, cgv::nui::refocus_action rfa,
	const cgv::nui::focus_demand& demand, const cgv::gui::event& e, const cgv::nui::dispatch_info& dis_info)
{
	switch (action) {
	case cgv::nui::focus_change_action::attach:
		if (state == state_enum::idle) {
			// set state based on dispatch mode
			change_state(dis_info.mode == cgv::nui::dispatch_mode::pointing ? state_enum::pointed : state_enum::close);
			// store hid to filter handled events
			hid_id = dis_info.hid_id;
			return true;
		}
		// if focus is given to other hid, refuse attachment to new hid
		return false;
	case cgv::nui::focus_change_action::detach:
		// check whether detach corresponds to stored hid
		if (state != state_enum::idle && hid_id == dis_info.hid_id) {
			change_state(state_enum::idle);
			return true;
		}
		return false;
	case cgv::nui::focus_change_action::index_change:
		prim_idx = int(reinterpret_cast<const cgv::nui::hit_dispatch_info&>(dis_info).get_hit_info()->primitive_index);
		on_set(&prim_idx);
		break;
	}
	return true;
}

bool cgv::nui::interactable::handle(const cgv::gui::event& e, const cgv::nui::dispatch_info& dis_info, cgv::nui::focus_request& request)
{
	// ignore all events in idle mode
	if (state == state_enum::idle)
		return false;
	// ignore events from other hids
	if (!(dis_info.hid_id == hid_id))
		return false;
	bool pressed;
	// hid independent check if grabbing is activated or deactivated
	if (is_grab_change(e, pressed)) {
		if (state == state_enum::close && pressed) {
			ii_at_grab = ii_during_focus;
			change_state(state_enum::grabbed);
			drag_begin(request, false, original_config);
		}
		else if (state == state_enum::grabbed) {
			drag_end(request, original_config);
			change_state(state_enum::close);
		}
		return true;
	}
	// check if event is for grabbing
	if (is_grabbing(e, dis_info)) {
		const auto& prox_info = get_proximity_info(dis_info);
		if (state == state_enum::close) {
			focus_debug_point = prox_info.hit_point;
			ii_during_focus.query_point = prox_info.query_point;
			ii_during_focus.hid_position = prox_info.hid_position;
			ii_during_focus.hid_direction = prox_info.hid_direction;
			ii_during_focus.is_pointing = false;
			prim_idx = int(prox_info.primitive_index);
			on_set(&prim_idx);
		}
		else if (state == state_enum::grabbed) {
			focus_debug_point = prox_info.hit_point;
			ii_during_focus.query_point = prox_info.query_point;
			ii_during_focus.hid_position = prox_info.hid_position;
			ii_during_focus.hid_direction = prox_info.hid_direction;
			ii_during_focus.is_pointing = false;
			on_grabbed_drag();
		}
		post_redraw();
		return true;
	}
	// hid independent check if object is triggered during pointing
	if (is_trigger_change(e, pressed)) {
		if (state == state_enum::pointed && pressed) {
			ii_at_grab = ii_during_focus;
			change_state(state_enum::triggered);
			drag_begin(request, true, original_config);
		}
		else if (state == state_enum::triggered) {
			drag_end(request, original_config);
			change_state(state_enum::pointed);
		}
		return true;
	}
	// check if event is for pointing
	if (is_pointing(e, dis_info)) {
		const auto& inter_info = get_intersection_info(dis_info);
		if (state == state_enum::pointed) {
			focus_debug_point = inter_info.hit_point;
			ii_during_focus.query_point = inter_info.hit_point;
			ii_during_focus.hid_position = inter_info.hid_position;
			ii_during_focus.hid_direction = inter_info.hid_direction;
			ii_during_focus.is_pointing = true;
			prim_idx = int(inter_info.primitive_index);
			on_set(&prim_idx);
		}
		else if (state == state_enum::triggered) {
			// if we still have an intersection point, use as debug point
			if (inter_info.ray_param != std::numeric_limits<float>::max())
				focus_debug_point = inter_info.hit_point;
			ii_during_focus.query_point = inter_info.hit_point;
			ii_during_focus.hid_position = inter_info.hid_position;
			ii_during_focus.hid_direction = inter_info.hid_direction;
			ii_during_focus.is_pointing = true;
			on_triggered_drag();
		}
		post_redraw();
		return true;
	}
	return false;
}

bool cgv::nui::interactable::init(cgv::render::context& ctx)
{
	cgv::render::ref_sphere_renderer(ctx, 1);
	return true;
}

void cgv::nui::interactable::clear(cgv::render::context& ctx)
{
	cgv::render::ref_sphere_renderer(ctx, -1);
}

void cgv::nui::interactable::draw(cgv::render::context& ctx)
{
	if (state == state_enum::idle)
		return;
	auto& sr = cgv::render::ref_sphere_renderer(ctx);
	sr.set_render_style(debug_sphere_rs);
	if (state != state_enum::idle && focus_debug_point_enabled)
	{
		sr.set_position(ctx, focus_debug_point);
		sr.set_color_array(ctx, &focus_debug_point_color, 1);
		sr.render(ctx, 0, 1);
	}
	if ((state == state_enum::grabbed || state == state_enum::triggered) && grab_debug_point_enabled)
	{
		sr.set_position(ctx, ii_at_grab.query_point);
		sr.set_color_array(ctx, &grab_debug_point_color, 1);
		sr.render(ctx, 0, 1);
	}
}

void cgv::nui::interactable::create_gui()
{
	if (begin_tree_node("interactable", focus_debug_point_enabled)) {
		align("\a");
		add_member_control(this, "enable focus debug point", focus_debug_point_enabled, "check");
		add_member_control(this, "focus debug point color", focus_debug_point_color);
		add_member_control(this, "enable grab debug point at grab", grab_debug_point_enabled, "check");
		add_member_control(this, "grab debug point color", grab_debug_point_color);
		align("\b");
		end_tree_node(focus_debug_point_enabled);
	}
}