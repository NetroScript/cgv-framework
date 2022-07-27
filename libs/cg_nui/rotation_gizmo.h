#pragma once

#include <cg_nui/gizmo.h>
#include <cg_nui/reusable_gizmo_functionalities.h>
#include <cgv_gl/spline_tube_renderer.h>

#include "lib_begin.h"

namespace cgv {
	namespace nui {

/// A gizmo that provides the means to rotate an object around configured axes.
///	Needs a rotation to manipulate.
class CGV_API rotation_gizmo : public cgv::nui::gizmo,
	public cgv::nui::gizmo_functionality_configurable_axes
{
	cgv::render::spline_tube_render_style strs;

	std::vector<rgb> ring_colors;
	float ring_spline_radius{ 0.05f };
	unsigned ring_nr_spline_segments = 4;
	float ring_radius;

	typedef std::pair<std::vector<vec3>, std::vector<vec4>> spline_data_t;
	std::vector<spline_data_t> ring_splines;

	quat rotation_at_grab;

	int projected_point_handle;
	int direction_at_grab_handle, direction_currently_handle;

	/// Compute the scale-dependent geometry
	void compute_geometry(const vec3& scale);
	/// Draw the handles
	void _draw(cgv::render::context& ctx);
	/// Do proximity check
	bool _compute_closest_point(const vec3& point, vec3& prj_point, vec3& prj_normal, size_t& primitive_idx);
	/// Do intersection check
	bool _compute_intersection(const vec3& ray_start, const vec3& ray_direction, float& hit_param, vec3& hit_normal, size_t& primitive_idx);

protected:
	// pointers to properties of the object the gizmo is attached to
	quat* rotation_ptr{ nullptr };

	// current configuration of the gizmo
	std::vector<float> translation_axes_radii{ 0.2f };
	std::vector<rgb> rotation_axes_colors;

	bool validate_configuration() override;

	void on_handle_grabbed() override;
	void on_handle_drag() override;

	void precompute_geometry() override;

	//void _draw_local_orientation(cgv::render::context& ctx, const vec3& inverse_translation, const quat& inverse_rotation, const vec3& scale, const mat4& view_matrix) override;
	//void _draw_global_orientation(cgv::render::context& ctx, const vec3& inverse_translation, const quat& rotation, const vec3& scale, const mat4& view_matrix) override;
	//
	//bool _compute_closest_point_local_orientation(const vec3& point, vec3& prj_point, vec3& prj_normal, size_t& primitive_idx, const vec3& inverse_translation, const quat& inverse_rotation, const vec3& scale, const mat4& view_matrix) override;
	//bool _compute_closest_point_global_orientation(const vec3& point, vec3& prj_point, vec3& prj_normal, size_t& primitive_idx, const vec3& inverse_translation, const quat& rotation, const vec3& scale, const mat4& view_matrix) override;
	//bool _compute_intersection_local_orientation(const vec3& ray_start, const vec3& ray_direction, float& hit_param, vec3& hit_normal, size_t& primitive_idx, const vec3& inverse_translation, const quat& inverse_rotation, const vec3& scale, const mat4& view_matrix) override;
	//bool _compute_intersection_global_orientation(const vec3& ray_start, const vec3& ray_direction, float& hit_param, vec3& hit_normal, size_t& primitive_idx, const vec3& inverse_translation, const quat& rotation, const vec3& scale, const mat4& view_matrix) override;

public:
	rotation_gizmo() : gizmo("rotation_gizmo") {}

	/// Attach this gizmo to the given object. The given rotation will be manipulated. The position is used as the anchor
	/// for the gizmo's visual representation. The optional scale is needed if the gizmo's visuals should follow the given scale.
	void attach(base_ptr obj, vec3* position_ptr, quat* rotation_ptr, vec3* scale_ptr = nullptr);
	void detach();

	// Configuration functions
	void configure_axes_directions(std::vector<vec3> axes) override;
	/// Set colors for the visual representation of the axes (rings). If less colors then axes are given then the
	///	last color will be repeated.
	// TODO: Extend with configuration of selection/grab color change
	void configure_axes_coloring(std::vector<rgb> colors);
	/// Set various parameters of the individual ring geometries.
	void configure_axes_geometry(float ring_radius, float ring_tube_radius);

	//@name cgv::render::drawable interface
	//@{
	bool init(cgv::render::context& ctx) override;
	void clear(cgv::render::context& ctx) override;
	//@}

	//@name cgv::gui::provider interface
	//@{
	void create_gui() override;
	//@}
};
	}
}

#include <cgv/config/lib_end.h>