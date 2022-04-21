#include "grabable_interactable.h"

#include "cgv/math/proximity.h"

void cgv::nui::grabable_interactable::on_grabbed_start()
{
	if (position_ptr_ptr != nullptr && *position_ptr_ptr != nullptr)
		position_at_grab = **position_ptr_ptr;
	else if (position_ptr != nullptr)
		position_at_grab = *position_ptr;
}

void cgv::nui::grabable_interactable::on_grabbed_drag()
{
	if (position_ptr_ptr != nullptr && *position_ptr_ptr != nullptr)
		**position_ptr_ptr = position_at_grab + ii_during_focus.query_point - ii_at_grab.query_point;
	else if (position_ptr != nullptr)
		*position_ptr = position_at_grab + ii_during_focus.query_point - ii_at_grab.query_point;
}

void cgv::nui::grabable_interactable::on_triggered_start()
{
	if (position_ptr_ptr != nullptr && *position_ptr_ptr != nullptr)
		position_at_grab = **position_ptr_ptr;
	else if (position_ptr != nullptr)
		position_at_grab = *position_ptr;

}

void cgv::nui::grabable_interactable::on_triggered_drag()
{
	// to be save even without new intersection, find closest point on ray to hit point at trigger
	vec3 q = cgv::math::closest_point_on_line_to_point(ii_during_focus.hid_position, ii_during_focus.hid_direction, ii_at_grab.query_point);
	if (position_ptr_ptr != nullptr && *position_ptr_ptr != nullptr)
		**position_ptr_ptr = position_at_grab + q - ii_at_grab.query_point;
	else if (position_ptr != nullptr)
		*position_ptr = position_at_grab + q - ii_at_grab.query_point;
}

void cgv::nui::grabable_interactable::draw(cgv::render::context& ctx)
{
	interactable::draw(ctx);
}

void cgv::nui::grabable_interactable::create_gui()
{
	interactable::create_gui();
}