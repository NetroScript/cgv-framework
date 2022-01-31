#include <cgv/base/node.h>
#include <cgv/base/register.h>
#include <cgv/render/drawable.h>
#include <cgv_gl/rectangle_renderer.h>
#include <cgv_gl/sphere_renderer.h>
#include <cgv_gl/box_renderer.h>
#include <cgv_gl/cone_renderer.h>
#include <cgv_gl/sphere_renderer.h>
#include <cgv_proc/terrain_renderer.h>
#include <cgv/gui/event_handler.h>
#include <cgv/gui/provider.h>
#include <vr_view_interactor.h>
#include <3rd/screen_capture_lite/include/ScreenCapture.h>
#include <cg_nui/label_manager.h>
#include <cg_nui/spatial_dispatcher.h>
#include <cg_nui/vr_table.h>
#include <cg_nui/table_gizmo.h>

#include "lib_begin.h"

namespace vr {

/// different table types
enum TableMode
{
	TM_HIDE,
	TM_RECTANGULAR,
	TM_ROUND
};

/// different ground types
enum GroundMode {
	GM_NONE,
	GM_BOXES,
	GM_TERRAIN
};

/// different environment modes that are not yet supported
enum EnvironmentMode {
	EM_EMPTY,
	EM_SKYBOX,
	EM_PROCEDURAL
};

/// support self reflection of table mode
extern CGV_API cgv::reflect::enum_reflection_traits<TableMode> get_reflection_traits(const TableMode&);

/// support self reflection of ground mode
extern CGV_API cgv::reflect::enum_reflection_traits<GroundMode> get_reflection_traits(const GroundMode&);

/// support self reflection of environment mode
extern CGV_API cgv::reflect::enum_reflection_traits<EnvironmentMode> get_reflection_traits(const EnvironmentMode&);

/// class manages static and dynamic parts of scene
class CGV_API vr_scene :
	public cgv::base::node,
	public cgv::base::registration_listener,
	public cgv::render::drawable,
	public cgv::gui::event_handler,
	public cgv::nui::spatial_dispatcher,
	public cgv::gui::provider
{
private:
	// keep reference to vr view (initialized in init function)
	vr_view_interactor* vr_view_ptr;

	double ctrl_pointing_animation_duration = 0.5;
	// if both grabbing and pointing turned off for controller ci, detach it from focus in hid and kit attachments
	void check_for_detach(int ci, const cgv::gui::event& e);
	// keep reference to vr table
	cgv::nui::vr_table_ptr table;

	//@name boxes and table
	//@{	

	// store the static part of the scene as colored boxes with the table in the last 5 boxes
	std::vector<box3> boxes;
	std::vector<rgb> box_colors;

	// rendering style for rendering of boxes
	cgv::render::box_render_style box_style;

	cgv::render::texture skybox;
	cgv::render::shader_program cubemap_prog;

	bool invert_skybox;
	std::string skybox_file_names;

	GroundMode ground_mode;

	// terrain members
	std::vector<vec2> custom_positions;
	std::vector<unsigned int> custom_indices;
	cgv::render::terrain_render_style terrain_style;
	int grid_width;
	int grid_height;
	dvec3 terrain_translation;
	double terrain_scale;

	EnvironmentMode environment_mode;

	bool draw_room, draw_walls, draw_ceiling;
	float room_width, room_depth, room_height, wall_width;

	/// construct boxes that represent a room of dimensions w,d,h and wall width W
	void construct_room(float w, float d, float h, float W, bool walls, bool ceiling);
	/// construct boxes for environment
	void construct_ground(float s, float ew, float ed, float w, float d, float h);
	/// construct a scene with a table
	void build_scene(float w, float d, float h, float W);
	/// clear scene geometry containers
	void clear_scene();
	//@}

	/// store poses of different coordinate systems. These are computed in init_frame() function
	mat34 pose[5];
	/// store whether poses are valid
	bool valid[5];


	//@name labels
	//@{	
	/// use label manager to organize labels in texture
	cgv::nui::label_manager lm;

	/// store label placements for rectangle renderer
	std::vector<vec3> label_positions;
	std::vector<quat> label_orientations;
	std::vector<vec2> label_extents;
	std::vector<vec4> label_texture_ranges;

	// label visibility
	std::vector<int> label_visibilities;

	/// for rectangle renderer a rectangle_render_style is needed
	cgv::render::rectangle_render_style rrs;
	/// attribute array manager for rectangle renderer
	cgv::render::attribute_array_manager aam;
protected:
	bool draw_controller_mode;
	cgv::render::sphere_render_style srs;
	cgv::render::cone_render_style crs;
	std::vector<vec3> sphere_positions;
	std::vector<rgb> sphere_colors;
	std::vector<vec3> cone_positions;
	std::vector<rgb> cone_colors;
	void construct_hit_geometry();
public:
	/// overload to handle registration events
	void register_object(base_ptr object, const std::string& options);
	/// overload to handle unregistration events
	void unregister_object(base_ptr object, const std::string& options);
	/// different coordinate systems used to place labels
	enum CoordinateSystem
	{
		CS_LAB,
		CS_TABLE,
		CS_HEAD,
		CS_LEFT_CONTROLLER,
		CS_RIGHT_CONTROLLER
	};
	/// different alignments
	enum LabelAlignment
	{
		LA_CENTER,
		LA_LEFT,
		LA_RIGHT,
		LA_BOTTOM,
		LA_TOP
	};
	/// for each label coordinate system
	std::vector<CoordinateSystem> label_coord_systems;
	/// size of pixel in meters
	float pixel_scale;
	/// add a new label without placement information and return its index
	uint32_t add_label(const std::string& text, const rgba& bg_clr, int _border_x = 4, int _border_y = 4, int _width = -1, int _height = -1) {
		uint32_t li = lm.add_label(text, bg_clr, _border_x, _border_y, _width, _height);
		label_positions.push_back(vec3(0.0f));
		label_orientations.push_back(quat());
		label_extents.push_back(vec2(1.0f));
		label_texture_ranges.push_back(vec4(0.0f));
		label_coord_systems.push_back(CS_LAB);
		label_visibilities.push_back(1);
		return li;
	}
	/// update label text
	void update_label_text(uint32_t li, const std::string& text) { lm.update_label_text(li, text); }
	/// update label size in texel
	void update_label_size(uint32_t li, int w, int h) { lm.update_label_size(li, w, h); }
	/// update label background color
	void update_label_background_color(uint32_t li, const rgba& bgclr) { lm.update_label_background_color(li, bgclr); }
	/// fix the label size based on the font metrics even when text is changed later on
	void fix_label_size(uint32_t li) { lm.fix_label_size(li); }
	/// place a label relative to given coordinate system
	void place_label(uint32_t li, const vec3& pos, const quat& ori = quat(),
		CoordinateSystem coord_system = CS_LAB, LabelAlignment align = LA_CENTER, float scale = 1.0f) {
		label_extents[li] = vec2(scale * pixel_scale * lm.get_label(li).get_width(), scale * pixel_scale * lm.get_label(li).get_height());
		static vec2 offsets[5] = { vec2(0.0f,0.0f), vec2(0.5f,0.0f), vec2(-0.5f,0.0f), vec2(0.0f,0.5f), vec2(0.0f,-0.5f) };
		label_positions[li] = pos + ori.get_rotated(vec3(offsets[align] * label_extents[li], 0.0f));
		label_orientations[li] = ori;
		label_coord_systems[li] = coord_system;
	}
	/// hide a label
	void hide_label(uint32_t li) { label_visibilities[li] = 0; }
	/// show a label
	void show_label(uint32_t li) { label_visibilities[li] = 1; }
	/// set the common border color of labels
	void set_label_border_color(const rgba& border_color);
	/// set the common border width in percent of the minimal extent
	void set_label_border_width(float border_width);
	//@}

public:
	/// standard constructor for scene
	vr_scene();
	/// return type name
	std::string get_type_name() const { return "vr_scene"; }
	/// reflect member variables
	bool self_reflect(cgv::reflect::reflection_handler& rh);
	/// callback on member updates to keep data structure consistent
	void on_set(void* member_ptr);
	//@name cgv::gui::event_handler interface
	//@{
	/// provide interaction help to stream
	void stream_help(std::ostream& os);
	/// handle events
	bool handle(cgv::gui::event& e);
	//@}

	//@name cgv::render::drawable interface
	//@{
	/// initialization called once per context creation
	bool init(cgv::render::context& ctx);
	/// initialization called once per frame
	void init_frame(cgv::render::context& ctx);
	/// called before context destruction to clean up GPU objects
	void clear(cgv::render::context& ctx);
	/// draw scene here
	void draw(cgv::render::context& ctx);
	/// draw transparent part here
	void finish_frame(cgv::render::context& ctx);
	//@}
	/// check whether coordinate system is available
	bool is_coordsystem_valid(CoordinateSystem cs) const { return valid[cs]; }
	/// provide access to coordinate system - check validity with is_coordsystem_valid() before
	const mat34& get_coordsystem(CoordinateSystem cs) const { return pose[cs]; }
	/// cgv::gui::provider function to create classic UI
	void create_gui();
};

}

#include <cgv/config/lib_end.h>