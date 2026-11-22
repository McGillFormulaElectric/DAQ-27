/*
 * Field.hpp
 *
 *  Created on: Feb 15, 2025
 *      Author: mariobouzakhm
 */

#ifndef INC_FIELD_HPP_
#define INC_FIELD_HPP_

using namespace std;

#include <string>
#include <stdint.h>

class Field {
	private:
		uint8_t index;
		string name;

	protected:
		uint32_t value;

	public:
		Field(uint8_t index, string name);
		uint8_t getIndex();
		string getName();
		uint32_t getValue();
};





#endif /* INC_FIELD_HPP_ */
