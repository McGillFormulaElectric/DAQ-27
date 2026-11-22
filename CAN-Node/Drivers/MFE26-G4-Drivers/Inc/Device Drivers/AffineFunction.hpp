/*
 * AffineFunction.hpp
 *
 *  Created on: Jan 21, 2025
 *      Author: mariobouzakhm
 */

#ifndef INC_AFFINEFUNCTION_HPP_
#define INC_AFFINEFUNCTION_HPP_

#include <stdint.h>
#include "TransferFunction.hpp"

class AffineFunction : public TransferFunction {
private:
	double coefficient;
	double term;

public:
	AffineFunction(double coefficient, double term);
	double apply(uint32_t value);
};





#endif /* INC_AFFINEFUNCTION_HPP_ */
