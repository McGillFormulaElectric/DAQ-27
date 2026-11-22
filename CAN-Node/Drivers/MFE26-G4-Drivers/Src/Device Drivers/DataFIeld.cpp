/*
 * DataFIeld.cpp
 *
 *  Created on: Feb 15, 2025
 *      Author: mariobouzakhm
 */

#include "DataField.hpp"

DataField::DataField(uint8_t index, string name, TransferFunction *transferFunction): Field(index, name) {
	this->transferFunction = transferFunction;
}

uint8_t DataField::isOverriden() {
	return this->override;
}

TransferFunction *DataField::getTransferFunction() {
	return this->transferFunction;
}


double DataField::getRealValue() {
	return this->realValue;
}

void DataField::applyTransferFunction() {
	this->realValue = this->transferFunction->apply(this->getValue());
}

void DataField::setValue(uint32_t value, bool performCalibration) {
	if(this->override == 0) {
		this->value = value;
		if(performCalibration) {
			this->applyTransferFunction();
		}
	}
}
void DataField::setOverridenValue(uint32_t value, bool performCalibration) {
	this->value = value;
	if(performCalibration) {
		this->applyTransferFunction();
	}
}
void DataField::setOverrideValue(uint8_t value) {
	this->override = value;
}
void DataField::setTransferFunction(TransferFunction *transferFunction) {
	this->transferFunction = transferFunction;
}
