/*
 * UnityFunction.cpp
 *
 *  Created on: Jan 21, 2025
 *      Author: mariobouzakhm
 */

#include "UnityFunction.hpp"
#include "AffineFunction.hpp"

double UnityFunction::apply(uint32_t value) {
	return value;
}

double AffineFunction::apply(uint32_t value) {
	return this->coefficient*value + this->term;
}




