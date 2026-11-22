/*
 * Field.cpp
 *
 *  Created on: Feb 15, 2025
 *      Author: mariobouzakhm
 */

#ifndef SRC_FIELD_CPP_
#define SRC_FIELD_CPP_

#include "Field.hpp"

Field::Field(uint8_t index, string name) {
	this->index = index;
	this->name = name;

	this->value = 0;
}

uint8_t Field::getIndex() {
	return this->index;
}

string Field::getName() {
	return this->name;
}

uint32_t Field::getValue() {
	return this->value;
}

#endif /* SRC_FIELD_CPP_ */
