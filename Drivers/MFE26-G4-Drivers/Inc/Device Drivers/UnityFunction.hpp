/*
 * UnityFunction.hpp
 *
 *  Created on: Jan 21, 2025
 *      Author: mariobouzakhm
 */

#ifndef INC_UNITYFUNCTION_HPP_
#define INC_UNITYFUNCTION_HPP_

#include <stdint.h>
#include "TransferFunction.hpp"

class UnityFunction : public TransferFunction {
public:
	double apply(uint32_t value);
};

#endif /* INC_UNITYFUNCTION_HPP_ */
