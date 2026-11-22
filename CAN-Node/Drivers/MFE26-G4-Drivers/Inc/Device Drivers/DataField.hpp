/*
 * DataField.hpp
 *
 *  Created on: Feb 15, 2025
 *      Author: mariobouzakhm
 */

#ifndef INC_DATAFIELD_HPP_
#define INC_DATAFIELD_HPP_

#include "TransferFunction.hpp"
#include "Field.hpp"


class DataField: public Field {
	private:
		uint8_t override;
		TransferFunction *transferFunction;
		double realValue;

	public:
		DataField(uint8_t index, string name, TransferFunction *transferFunction);
		uint8_t isOverriden();
		TransferFunction *getTransferFunction();
		double getRealValue();

		void applyTransferFunction();
		void setValue(uint32_t value, bool performCalibration);
		void setOverrideValue(uint8_t value);
		void setOverridenValue(uint32_t value, bool performCalibration);
		void setTransferFunction(TransferFunction *transferFunction);
};

#endif /* INC_DATAFIELD_HPP_ */
