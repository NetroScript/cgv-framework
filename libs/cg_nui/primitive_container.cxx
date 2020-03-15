#include "primitive_container.h"

namespace cgv {
	namespace nui {

primitive_container::primitive_container(PrimitiveType _type, bool _use_colors, bool _use_orientations, ScalingMode _scaling_mode)
	: type(_type), use_colors(_use_colors), use_orientations(_use_orientations), scaling_mode(_scaling_mode)
{
}

primitive_container::~primitive_container()
{
}

void primitive_container::consider_closest_point(uint32_t i, contact_info& info, float distance, const vec3& p, const vec3& n)
{
	if (info.contacts.empty()) {
		contact_info::contact C;
		C.distance = distance;
		C.position = p;
		C.normal = n;
		C.primitive_index = i;
		C.container = this;
		info.contacts.push_back(C);
	}
	else {
		contact_info::contact& C = info.contacts.front();
		if (C.distance > distance) {
			C.distance = distance;
			C.position = p;
			C.normal = n;
			C.primitive_index = i;
			C.container = this;
		}
	}
}

/// last parameter is weight for trading between position and normal distances for closest oriented point query; default implementation defers call to computer_closest_point
void primitive_container::compute_closest_oriented_point(contact_info& info, const vec3& pos, const vec3& normal, float orientation_weight)
{
	compute_closest_point(info, pos);
}

primitive_container::box3 primitive_container::compute_bounding_box() const
{
	box3 B;
	for (const auto& c : center_positions)
		B.add_point(c);
	return B;
}

void primitive_container::prepare_render(cgv::render::context& ctx, cgv::render::renderer& r, const cgv::render::render_style& rs, const std::vector<uint32_t>* indices_ptr) const
{
	r.set_render_style(rs);
	r.set_position_array(ctx, center_positions);
	if (use_colors)
		r.set_color_array(ctx, colors);
	if (indices_ptr)
		r.set_indices(ctx, *indices_ptr);
}

bool primitive_container::render(cgv::render::context& ctx, cgv::render::renderer& r, const cgv::render::render_style& rs, const std::vector<uint32_t>* indices_ptr) const
{
	prepare_render(ctx, r, rs);
	if (!r.validate_and_enable(ctx))
		return false;
	if (indices_ptr)
		r.draw(ctx, 0, indices_ptr->size(), true);
	else
		r.draw(ctx, 0, center_positions.size());
	r.disable(ctx);
	return false;
}

	}
}