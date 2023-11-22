#pragma once

#include <cgv/render/render_types.h>

namespace cgv {
namespace g2d {

/// @brief A rectangle class that stores 2D position and size members and provides convenient accessors and member methods.
/// @tparam coord_type the coordinate type
template<typename coord_type>
struct rect {
	typedef cgv::math::fvec<coord_type, 2> point_type;
	
	point_type position;
	point_type size;

	rect() {
		position = point_type(0);
		size = point_type(0);
	}

	rect(const point_type& position, const point_type& size) : position(position), size(size) {}

	// minimum point (bottom left)
	inline point_type a() const { return position; }
	// maximum point (top right)
	inline point_type b() const { return position + size; }

	// minimum x position (left)
	inline coord_type x() const { return position.x(); }
	inline coord_type& x() { return position.x(); }
	// minimum y position (bottom)
	inline coord_type y() const { return position.y(); }
	inline coord_type& y() { return position.y(); }

	// maximum x position (right)
	inline coord_type x1() const { return position.x() + size.x(); }
	// maximum y position (top)
	inline coord_type y1() const { return position.y() + size.y(); }

	// width
	inline coord_type w() const { return size.x(); }
	inline coord_type& w() { return size.x(); }
	// height
	inline coord_type h() const { return size.y(); }
	inline coord_type& h() { return size.y(); }

	// center position
	template<typename coord_type_ = coord_type, typename std::enable_if_t<std::is_integral<coord_type_>::value, bool> = true>
	inline point_type center() const {
		return position + size / coord_type_(2);
	}

	// center position
	template<typename coord_type_ = coord_type, typename std::enable_if_t<std::is_floating_point<coord_type_>::value, bool> = true>
	inline point_type center() const {
		return position + coord_type_(0.5) * size;
	}

	// translate (move) whole rectangle by offset
	inline void translate(coord_type dx, coord_type dy) {
		translate(point_type(dx, dy));
	}

	// translate (move) whole rectangle by offset
	inline void translate(point_type o) {
		position += o;
	}

	// scale from center by given size difference (delta);
	inline void scale(coord_type dx, coord_type dy) {
		scale(point_type(dx, dy));
	}

	// scale from center by given size difference (delta);
	inline void scale(point_type d) {
		position -= d;
		size += coord_type(2) * d;
	}

	// resize by given size difference (delta); pivot unchanged
	inline void resize(coord_type dx, coord_type dy) {
		resize(point_type(dx, dy));
	}

	// resize by given size difference (delta); pivot unchanged
	inline void resize(point_type d) {
		size += d;
	}

	// returns true if the query point is inside the rectangle, false otherwise
	inline bool is_inside(point_type p) const {
		return
			p.x() >= x() && p.x() <= x1() &&
			p.y() >= y() && p.y() <= y1();
	}
};

typedef rect<unsigned> urect;
typedef rect<int> irect;
typedef rect<float> frect;
typedef rect<double> drect;

}
}
