/*
 * ConfigField.hpp
 *
 *  Created on: Feb 15, 2025
 *      Author: mariobouzakhm
 */

#ifndef INC_CONFIGFIELD_HPP_
#define INC_CONFIGFIELD_HPP_

#include "Field.hpp"

class ConfigField: public Field {
	public:
		ConfigField(uint8_t index, string name);
};




#endif /* INC_CONFIGFIELD_HPP_ */
