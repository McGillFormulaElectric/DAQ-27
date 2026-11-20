/*
 * TransferFunction.hpp
 *
 *  Created on: Jan 21, 2025
 *      Author: mariobouzakhm
 */

#ifndef INC_TRANSFERFUNCTION_HPP_
#define INC_TRANSFERFUNCTION_HPP_

#include <stdint.h>

class TransferFunction {
	public:
		virtual double apply(uint32_t value);
};



#endif /* INC_TRANSFERFUNCTION_HPP_ */
