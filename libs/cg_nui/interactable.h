#pragma once

#include <unordered_set>
#include <cgv/base/group.h>
#include <cgv/render/drawable.h>
#include <cg_nui/focusable.h>
#include <cg_nui/pointable.h>
#include <cg_nui/grabable.h>
#include <cgv/gui/provider.h>
#include <cgv_gl/sphere_renderer.h>

#include "lib_begin.h"

namespace cgv {
	namespace nui {
		using namespace render;
/// Base class for objects that can have focus and be selected/activated by pointing or grabbing.
///	Provides a general implementation of different interaction states with event functions that can be overriden.
///	States are:
///		idle: default inactive state
///		close: hid is in range for grabbing
///		grabbed: hid has grabbed this object
///		pointed: hid is pointing at object (ray is intersecting)
///		triggered: hid has triggered this object while pointing at it
///	The event functions are for entering (start) and leaving (stop) the states. Triggered and grabbed states
///	also have a drag event that is called continuously while the object is grabbed/triggered.
///	The hid position and direction, the query point of the intersection/proximity check and whether the interaction
///	was pointing or proximity is stored continuously while the interactable has focus (ii_during_focus) and once at
///	the moment of grab/trigger (ii_at_grab). The stored information is available for deriving classes to be used
///	e.g. for determining an updated position (see translation gizmo for an example).
///	If enabled a debug point is drawn at the intersection / closest surface point while the state is not idle.
///	If enabled an additional debug point is drawn for the intersection/closest surface point at the start of
///	grabbing/triggering.
class CGV_API interactable : public cgv::base::group,
							 public cgv::render::drawable,
							 public cgv::nui::focusable,
							 public cgv::nui::grabable,
							 public cgv::nui::pointable,
							 public cgv::gui::provider
{
	cgv::nui::focus_configuration original_config;
protected:
	/// Collection of values that describe an interaction between a hid and an interactable at one moment in time.
	struct interaction_info
	{
		/// Intersection or nearest point on surface that was used to determine the focus (in local space)
		vec3 query_point;
		/// Position of the interacting hid at the moment of interaction (in local space)
		vec3 hid_position;
		/// Orientation of the interacting hid at the moment of interaction (in local space)
		vec3 hid_direction;
		/// Whether the interaction was by pointing (i.e. a ray-cast) or by closest-point-query
		bool is_pointing{};

		interaction_info() {}
		interaction_info(vec3 query_point, vec3 hid_position, vec3 hid_direction, bool is_pointing) :
			query_point(query_point), hid_position(hid_position), hid_direction(hid_direction),
			is_pointing(is_pointing) {}
	};

	/// Interaction Infos that are constantly updated as long as the interactable is focused by these HIDs (all states except idle).
	/// The info for the HID that is currently grabbing (if any) can be retrieved by using activating_hid_id.
	std::map<hid_identifier, interaction_info> ii_during_focus;
	/// Interaction Info that is set once when transitioning to states grabbed or triggered
	interaction_info ii_at_grab;

	// Configuration
	/// Whether to allow more than one HID to point at or be close to the interactable
	bool allow_simultaneous_focus{ true };

public:
	// different possible object states
	enum class state_enum { idle, close, pointed, grabbed, triggered };

private:
	// helper function that handles switching object states
	void change_state(state_enum new_state);

protected:
	/// HIDs that are pointing at or close to interactable
	std::set<cgv::nui::hid_identifier> selecting_hid_ids;
	/// HID that triggered or grabbed the interactable
	cgv::nui::hid_identifier activating_hid_id;
	// state of object
	state_enum state = state_enum::idle;
	// index of focused primitive (always 0 in case of only one primitive)
	int prim_idx = 0;

	// State change events that can be overriden

	/// Called when entering the 'close' state
	virtual void on_close_start() {}
	/// Called when exiting the 'close' state
	///	Called before the entering event of the next state
	virtual void on_close_stop() {}

	/// Called when entering the 'pointed' state
	virtual void on_pointed_start() {}
	/// Called when exiting the 'pointed' state
	///	Called before the entering event of the next state
	virtual void on_pointed_stop() {}

	/// Called when entering the 'grabbed' state
	virtual void on_grabbed_start() {}
	/// Called each pose-update while in the 'grabbed' state
	virtual void on_grabbed_drag() {}
	/// Called when exiting the 'grabbed' state
	///	Called before the entering event of the next state
	virtual void on_grabbed_stop() {}

	/// Called when entering the 'triggered' state
	virtual void on_triggered_start() {}
	/// Called each pose-update while in the 'triggered' state
	virtual void on_triggered_drag() {}
	/// Called when exiting the 'triggered' state
	///	Called before the entering event of the next state
	virtual void on_triggered_stop() {}

public:
	interactable(const std::string& name = "");

	//@name cgv::base::base interface
	//@{
	std::string get_type_name() const override { return "interactable"; }
	//@}

	//@name cgv::nui::focusable interface
	//@{
	bool focus_change(cgv::nui::focus_change_action action, cgv::nui::refocus_action rfa,
					  const cgv::nui::focus_demand& demand, const cgv::gui::event& e,
					  const cgv::nui::dispatch_info& dis_info) override;
	void stream_help(std::ostream& os) override;
	bool handle(const cgv::gui::event& e, const cgv::nui::dispatch_info& dis_info, cgv::nui::focus_request& request) override;
	//@}

	// Used for drawing debug points
	//@name cgv::render::drawable interface
	//@{
	bool init(cgv::render::context& ctx) override;
	void clear(cgv::render::context& ctx) override;
	void draw(cgv::render::context& ctx) override;
	//@}

	//@name cgv::gui::provider interface
	//@{
	void create_gui() override;
	//@}
};
	}
}

#include <cgv/config/lib_end.h>