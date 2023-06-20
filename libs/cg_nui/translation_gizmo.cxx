#include "translation_gizmo.h"

#include <cgv/math/intersection.h>
#include <cgv/math/proximity.h>
#include <cg_nui/translatable.h>

#include <cg_nui/debug_visualization_helper.h>

void cgv::nui::translation_gizmo::precompute_geometry()
{
	arrow_positions.clear();
	arrow_directions.clear();
	for (int i = 0; i < axes_directions.size(); ++i) {
		arrow_positions.push_back(vec3(0.0f));
		arrow_directions.push_back(axes_directions[i] * translation_axes_length);
	}
}

void cgv::nui::translation_gizmo::compute_geometry(const vec3& scale)
{
	for (int i = 0; i < axes_directions.size(); ++i) {
		arrow_positions[i] = scale_dependent_axes_positions[i] * scale + scale_independent_axes_positions[i];
	}
}

bool cgv::nui::translation_gizmo::validate_configuration()
{
	bool configuration_valid = true;

	if (!(
		position_ptr ||
		(position_ptr_ptr && *position_ptr_ptr) ||
		translatable_obj
		)) {
		std::cout << "Translation gizmo requires a valid pointer to a position, or a pointer to a pointer to a position, or a reference to an object implementing translatable" << std::endl;
		configuration_valid = false;
	}

	configuration_valid = configuration_valid && validate_axes();
	configuration_valid = configuration_valid && validate_handles(axes_directions.size());

	return configuration_valid && gizmo::validate_configuration();
}

void cgv::nui::translation_gizmo::on_handle_grabbed()
{
	position_at_grab = get_position();
	grab_handle(prim_idx);
}

void cgv::nui::translation_gizmo::on_handle_released()
{
	release_handles();
}

void cgv::nui::translation_gizmo::on_handle_drag()
{
	vec3 axis = axes_directions[prim_idx];

	vec3 closest_point;
	if (ii_at_grab.is_pointing) {
		if (!cgv::math::closest_point_on_line_to_line(ii_at_grab.query_point, axis,
			ii_during_focus[activating_hid_id].hid_position, ii_during_focus[activating_hid_id].hid_direction, closest_point))
			return;
	}
	else {
		closest_point = cgv::math::closest_point_on_line_to_point(ii_at_grab.query_point, axis, ii_during_focus[activating_hid_id].hid_position);
	}

	vec3 movement = closest_point - ii_at_grab.query_point;

	// Transform movement into value object' parent coordinate system
	movement = gizmo_to_value_parent_transform_vector(movement);

	// If the position that this gizmo changes influences the anchor of this gizmo, then the movement is an incremental update.
	// Otherwise the movement is relative to the original position of the anchor at the time of grabbing.
	if (is_anchor_influenced_by_gizmo)
		set_position(get_position() + movement);
	else
		set_position(position_at_grab + movement);
}

cgv::render::render_types::vec3 cgv::nui::translation_gizmo::get_position()
{
	if (translatable_obj)
		return translatable_obj->get_position();
	if (position_ptr_ptr)
		return **position_ptr_ptr;
	return *position_ptr;
}

void cgv::nui::translation_gizmo::set_position(const vec3& position)
{
	if (translatable_obj) {
		translatable_obj->set_position(position);
	}
	else {
		if (position_ptr_ptr)
			**position_ptr_ptr = position;
		else
			*position_ptr = position;
		if (on_set_obj)
			on_set_obj->on_set(position_ptr);
	}
}

void cgv::nui::translation_gizmo::set_position_reference(vec3* _position_ptr, cgv::base::base_ptr _on_set_obj)
{
	position_ptr = _position_ptr;
	gizmo::set_on_set_object(_on_set_obj);
}

void cgv::nui::translation_gizmo::set_position_reference(vec3** _position_ptr_ptr, cgv::base::base_ptr _on_set_obj)
{
	position_ptr_ptr = _position_ptr_ptr;
	gizmo::set_on_set_object(_on_set_obj);
}

void cgv::nui::translation_gizmo::set_position_reference(translatable* _translatable_obj)
{
	translatable_obj = _translatable_obj;
}

void cgv::nui::translation_gizmo::set_axes_directions(std::vector<vec3> axes)
{
	gizmo_functionality_configurable_axes::set_axes_directions(axes);

	// Default configuration
	configure_axes_geometry(0.015f, 0.2f);
}

void cgv::nui::translation_gizmo::configure_axes_geometry(float radius, float length)
{
	ars.radius_relative_to_length = 0;
	ars.radius_lower_bound = radius;
	arrow_radius = radius;
	translation_axes_length = length;
}

bool cgv::nui::translation_gizmo::_compute_closest_point(const vec3& point, vec3& prj_point, vec3& prj_normal,
	size_t& primitive_idx, const vec3& scale, const mat4& view_matrix)
{
	compute_geometry(scale);

	size_t idx = -1;
	vec3 p, n;
	float dist_min = std::numeric_limits<float>::max();
	for (size_t i = 0; i < arrow_positions.size(); ++i) {
		vec3 p1, n1;
		cgv::math::closest_point_on_cylinder_to_point(arrow_positions[i], arrow_directions[i], arrow_radius, point, p1, n1);
		float dist = (p1 - point).length();
		if (dist < dist_min) {
			dist_min = dist;
			p = p1;
			n = n1;
			idx = i + 1;
		}
	}

	prj_point = p;
	prj_normal = n;
	primitive_idx = idx;
	return true;
}

bool cgv::nui::translation_gizmo::_compute_intersection(const vec3& ray_start, const vec3& ray_direction,
	float& hit_param, vec3& hit_normal, size_t& primitive_idx, const vec3& scale, const mat4& view_matrix)
{
	compute_geometry(scale);

	// DEBUG TO REMOVE
	auto& dvh = ref_debug_visualization_helper();
	//dvh.update_debug_value_cylinder(debug_cylinder_handle0, arrow_positions[0] + intersection_debug_position, arrow_directions[0], arrow_radius);
	//dvh.update_debug_value_ray(debug_ray_handle0, ray_start + intersection_debug_position, ray_direction);

	size_t idx = -1;
	float t = std::numeric_limits<float>::max();
	vec3 n;
	for (size_t i = 0; i < arrow_positions.size(); ++i) {
		vec3 n0;
		// DEBUG TO REMOVE - Box version (working)
		vec3 ro = ray_start - arrow_positions[i] - (arrow_directions[i] / 2.0f);
		vec3 norm_arrow_direction = arrow_directions[i];
		norm_arrow_direction.normalize();
		vec3 orth_arrow_direction_mask = vec3(1.0f) - norm_arrow_direction;
		vec3 box_extent_half = (vec3(arrow_radius) * orth_arrow_direction_mask + arrow_directions[i]) / 2.0f;
		if (i == 0) {
			dvh.update_debug_value_box(debug_box_handle0, intersection_debug_position, box_extent_half * 2.0f);
			dvh.update_debug_value_ray(debug_ray_handle0, ro + intersection_debug_position, ray_direction);
		}
		auto res = cgv::math::ray_box_intersection(ro, ray_direction, -box_extent_half, box_extent_half);
		if (res.hit && res.t_near < t) {
			t = res.t_near;
			n = vec3(1.0f, 0.0f, 0.0f);
			idx = i;
		}

		// Using simplified ray cylinder intersection (funktioniert gar nicht)
		//quat arrow_rot;
		//arrow_rot.set_normal(normalize(arrow_directions[i]));
		//quat correction_rot = arrow_rot.inverse();
		//vec3 ro = ray_start - arrow_positions[i];
		//correction_rot.rotate(ro);
		//vec3 rd = ray_direction;
		//correction_rot.rotate(rd);
		//if (i == 0) {
		//	dvh.update_debug_value_cylinder(debug_cylinder_handle0, intersection_debug_position, vec3(arrow_directions[i].length(), 0.0, 0.0), arrow_radius);
		//	dvh.update_debug_value_ray(debug_ray_handle0, ro + intersection_debug_position, rd);
		//}
		//auto res = cgv::math::ray_cylinder_intersection(ro, ray_direction, arrow_directions[i].length(), arrow_radius);
		//if (res.hit && res.t_near < t) {
		//	t = res.t_near;
		//	n = vec3(1.0f, 0.0f, 0.0f);
		//	idx = i;
		//}

		// Old ray cylinder intersection (not working for the case of root = table for unknown reasons)
		//float t0 = cgv::math::ray_cylinder_intersection(ray_start, ray_direction, arrow_positions[i], arrow_directions[i], arrow_radius, n0);
		//if (t0 < t) {
		//	t = t0;
		//	n = n0;
		//	idx = i;
		//}
	}

	if (t == std::numeric_limits<float>::max())
	{
		dehighlight_handles();
		return false;
	}
	hit_param = t;
	hit_normal = n;
	primitive_idx = idx;
	highlight_handle(idx);
	return true;
}

bool cgv::nui::translation_gizmo::init(cgv::render::context& ctx)
{
	if (!gizmo::init(ctx))
		return false;
	cgv::render::ref_arrow_renderer(ctx, 1);

	auto& dvh = cgv::nui::ref_debug_visualization_helper(ctx, 1);
	debug_coord_system_handle0 = dvh.register_debug_value_coordinate_system();
	{
		auto config = dvh.get_config_debug_value_coordinate_system(debug_coord_system_handle0);
		config.show_translation = false;
		config.position = vec3(0.8f, 2.0f, 0.0f);
		dvh.set_config_debug_value_coordinate_system(debug_coord_system_handle0, config);
	}
	debug_coord_system_handle1 = dvh.register_debug_value_coordinate_system();
	{
		auto config = dvh.get_config_debug_value_coordinate_system(debug_coord_system_handle1);
		config.show_translation = false;
		config.position = vec3(1.2f, 2.0f, 0.0f);
		dvh.set_config_debug_value_coordinate_system(debug_coord_system_handle1, config);
	}
	debug_coord_system_handle2 = dvh.register_debug_value_coordinate_system();
	{
		auto config = dvh.get_config_debug_value_coordinate_system(debug_coord_system_handle2);
		config.show_translation = false;
		config.position = vec3(1.2f, 3.2f, 0.0f);
		dvh.set_config_debug_value_coordinate_system(debug_coord_system_handle2, config);
	}
	debug_coord_system_handle3 = dvh.register_debug_value_coordinate_system();
	{
		auto config = dvh.get_config_debug_value_coordinate_system(debug_coord_system_handle3);
		config.show_translation = false;
		config.position = vec3(1.2f, 2.8f, 0.0f);
		dvh.set_config_debug_value_coordinate_system(debug_coord_system_handle3, config);
	}
	debug_coord_system_handle4 = dvh.register_debug_value_coordinate_system();
	{
		auto config = dvh.get_config_debug_value_coordinate_system(debug_coord_system_handle4);
		config.show_translation = false;
		config.position = vec3(1.2f, 2.4f, 0.0f);
		dvh.set_config_debug_value_coordinate_system(debug_coord_system_handle4, config);
	}
	debug_ray_handle0 = dvh.register_debug_value_ray();
	debug_ray_handle1 = dvh.register_debug_value_ray();
	{
		auto config = dvh.get_config_debug_value_ray(debug_ray_handle1);
		config.ray_color = rgb(0.2f, 1.0f, 0.2f);
		config.start_offset = 1.0f;
		dvh.set_config_debug_value_ray(debug_ray_handle1, config);
	}
	debug_cylinder_handle0 = dvh.register_debug_value_cylinder();
	{
		auto config = dvh.get_config_debug_value_cylinder(debug_cylinder_handle0);
		dvh.set_config_debug_value_cylinder(debug_cylinder_handle0, config);
	}
	debug_box_handle0 = dvh.register_debug_value_box();
	{
		auto config = dvh.get_config_debug_value_box(debug_box_handle0);
		dvh.set_config_debug_value_box(debug_box_handle0, config);
	}

	return true;
}

void cgv::nui::translation_gizmo::clear(cgv::render::context& ctx)
{
	auto& dvh = ref_debug_visualization_helper();
	dvh.deregister_debug_value(debug_coord_system_handle0);
	dvh.deregister_debug_value(debug_coord_system_handle1);
	dvh.deregister_debug_value(debug_coord_system_handle2);
	dvh.deregister_debug_value(debug_coord_system_handle3);
	dvh.deregister_debug_value(debug_coord_system_handle4);
	dvh.deregister_debug_value(debug_ray_handle0);
	dvh.deregister_debug_value(debug_ray_handle1);
	dvh.deregister_debug_value(debug_cylinder_handle0);
	cgv::nui::ref_debug_visualization_helper(ctx, -1);
	cgv::render::ref_arrow_renderer(ctx, -1);
	gizmo::clear(ctx);
}

void cgv::nui::translation_gizmo::_draw(cgv::render::context& ctx, const vec3& scale, const mat4& view_matrix)
{
	compute_geometry(scale);

	// DEBUG TO REMOVE
	vec3 anchor_obj_parent_global_translation;
	quat anchor_obj_parent_global_rotation;
	vec3 anchor_obj_parent_global_scale;
	transforming::extract_transform_components(transforming::get_global_model_transform(anchor_obj->get_parent()),
		anchor_obj_parent_global_translation, anchor_obj_parent_global_rotation, anchor_obj_parent_global_scale);
	ref_debug_visualization_helper().update_debug_value_coordinate_system(debug_coord_system_handle0, get_global_model_transform(this));

	mat4 gtoo_transform = get_global_model_transform(this);
	gtoo_transform = gtoo_transform * get_inverse_model_transform();
	ref_debug_visualization_helper().update_debug_value_coordinate_system(debug_coord_system_handle2, gtoo_transform);
	gtoo_transform = gtoo_transform
		* anchor_obj->get_interface<transforming>()->get_inverse_model_transform();
	ref_debug_visualization_helper().update_debug_value_coordinate_system(debug_coord_system_handle3, gtoo_transform);
	gtoo_transform = gtoo_transform
		* anchor_obj->get_parent()->get_interface<transforming>()->get_inverse_model_transform();
	ref_debug_visualization_helper().update_debug_value_coordinate_system(debug_coord_system_handle4, gtoo_transform);
	gtoo_transform = gtoo_transform
		* get_global_model_transform(value_obj->get_parent());

	ref_debug_visualization_helper().update_debug_value_coordinate_system(debug_coord_system_handle1, gtoo_transform);

	//ref_debug_visualization_helper().update_debug_value_coordinate_system(debug_coord_system_handle1, get_global_model_transform(this) * gizmo_to_other_object_transform(value_obj->get_parent()));



	if (!arrow_directions.empty()) {
		auto& ar = cgv::render::ref_arrow_renderer(ctx);
		ar.set_render_style(ars);

		ar.set_position_array(ctx, arrow_positions);
		ar.set_direction_array(ctx, arrow_directions);
		ar.set_color_array(ctx, handle_colors);
		ar.render(ctx, 0, (GLsizei)arrow_positions.size());
	}
}

void cgv::nui::translation_gizmo::create_gui()
{
	gizmo::create_gui();
	if (begin_tree_node("arrow style", ars)) {
		align("\a");
		add_gui("ars", ars);
		align("\b");
		end_tree_node(ars);
	}
}
