#pragma once

#include "control_base.h"

#include "lib_begin.h"

namespace cg {
namespace g2d {

class CGV_API value_control : public control_base {
private:
	double value = 0.0;
	cgv::render::dvec2 range = cgv::render::dvec2(0.0, 1.0);
	double step = 1.0;

public:
	using control_base::control_base;

	double get_value() const { return value; }

	bool set_value(double v);
	
	cgv::render::dvec2 get_range() const { return range; }

	void set_range(cgv::render::dvec2 range);

	double get_step() const { return step; }

	void set_step(double step) { this->step = step; }

	void handle_value_change(double v);
};

}
}

#include <cgv/config/lib_end.h>
