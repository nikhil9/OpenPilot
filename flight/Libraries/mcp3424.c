/**
 ******************************************************************************
 * @addtogroup OpenPilotModules OpenPilot Modules
 * @{ 
 * @addtogroup ET_EGT_Sensor EagleTree EGT Sensor Module
 * @brief Read ET EGT temperature sensors @ref ETEGTSensor "ETEGTSensor UAV Object"
 * @{ 
 *
 * @file       et_egt_sensor.c
 * @author     The OpenPilot Team, http://www.openpilot.org Copyright (C) 2010.
 * @brief      Reads dual thermocouple temperature sensors via EagleTree EGT expander
 *
 * @see        The GNU Public License (GPL) Version 3
 *
 *****************************************************************************/
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include "openpilot.h"
#include "mcp3424.h"
#include "pios_i2c.h"

// Private constants
static double_t MCP3424_REFVOLTAGE = 2.048; //internal reference voltage for MCP3424 IC

//// Private types

//// Private functions

uint8_t MCP3424_GetGain(uint8_t x) {
	switch(x) {
	case(0):
		return 1;
		break;
	case(1):
		return 2;
		break;
	case(2):
		return 4;
		break;
	case(3):
		return 8;
		break;
	default:
		return -1;
	}
}

uint8_t MCP3424_GetResolution(uint8_t x) {
	switch(x) {
	case(0):
		return 12;
		break;
	case(1):
		return 14;
		break;
	case(2):
		return 16;
		break;
	case(3):
		return 18;
		break;
	default:
		return -1;
	}
}

static bool MCP3424_SetConfig(uint16_t I2CAddress, uint8_t channel, uint8_t* pNumDataBytes, uint8_t resolution, uint8_t gain)
{
	uint8_t PGAgain = 0;
	uint8_t sampleRate = 0;

	uint8_t conversionModeBit = 0; //1=Continuous, 0=One Shot
	uint8_t channelBits = 0; //0 = Channel 1

	channelBits = channel - 1; //zero based

	switch(gain) {
	case('8'):
		PGAgain = 0x03;
		break;
	case('4'):
		PGAgain = 0x02;
		break;
	case('2'):
		PGAgain = 0x01;
		break;
	case('1'):
		PGAgain = 0x00;
		break;
	default:
		PGAgain = 0x00;
	}

	switch(resolution) {
	case(18):
		sampleRate = 0x03; //3.75 sps (18 bits), 3 bytes of data
		break;
	case(16):
		sampleRate = 0x02; //2 bytes of data,
		break;
	case(14):
		sampleRate = 0x01; //2 bytes of data
		break;
	case(12):
		sampleRate = 0x00; //240 SPS (12 bits), 2 bytes of data
		break;
	default:
		sampleRate = 0x00;
	}

	uint8_t config = PGAgain;
	config = config | (sampleRate << 2);
	config = config | (conversionModeBit << 4);
	config = config | (channelBits << 5);
	config = config | (1 << 7); //write a 1 here to initiate a new conversion in One-shot mode

	//the resolution setting affects the number of data bytes returned during a read
	if(resolution == 18)
		*pNumDataBytes = 3;
	else
		*pNumDataBytes = 2;

	//Set mcp3424 config register via i2c
	const struct pios_i2c_txn txn_list_1[] = {
		{
		 .addr = I2CAddress,
		 .rw = PIOS_I2C_TXN_WRITE,
		 .len = 1,
		 .buf = &config,
		 },
	};

	return PIOS_I2C_Transfer(PIOS_I2C_MAIN_ADAPTER, txn_list_1, NELEMENTS(txn_list_1));
}

static void MCP3424_DecipherI2Cresponse(uint8_t* pRawDataBytes, uint8_t* pBuffer, uint8_t numDataBytes, uint8_t resolution, uint32_t* pCounts)
{
	int8_t sign = 0; //+ve
	//uint8_t bufferTemp[6] = {0};  //buffer to store return data from sensor


	if((numDataBytes) == 3) {
		memcpy(pBuffer, pRawDataBytes, 4);

		pBuffer[0] = pBuffer[0] & 0x01; //remove the repeated MSB and sign bit
	}
	else { //numDataBytes == 2.
		pBuffer[0] = 0;              //set upper data byte to zero
		pBuffer[1] = pRawDataBytes[0];  //middle data byte
		pBuffer[2] = pRawDataBytes[1];  //lower data byte
		pBuffer[3] = pRawDataBytes[2];  //config byte

		if((resolution) == 14)
			pBuffer[1] = pBuffer[1] & 0x1F; //remove the repeated MSB and sign bit
		else if(resolution == 12)
			pBuffer[1] = pBuffer[1] & 0x07; //remove the repeated MSB and sign bit
	}

	sign = pRawDataBytes[0];

	*pCounts = ((pBuffer[0] << 16) | (pBuffer[1] << 8) | pBuffer[2]);

	//Convert to 2's complement
	if(sign < 0) {
		int32_t largestNumber = ((1 << (resolution - 1)) - 1);
		*pCounts = *pCounts - largestNumber;
	}

	pBuffer[4] = *pCounts;
	pBuffer[5] = resolution;

}

static int32_t MCP3424_GetDelay(uint8_t resolution)
{
	double_t d = 0;
	switch(resolution) {
	case(12):
		d = 1000/240.0; //240 samples per second
		break;
	case(14):
		d = 1000/60.0;  //60 sps
		break;
	case(16):
		d = 1000/15.0;  //15 sps
		break;
	case(18):
		d = 1000/3.75;  //3.75 sps
		break;
	default:
		d = 500;
	}
	return (int32_t)d + 50; //Add extra 50ms time
}

bool MCP3424_GetAnalogValue(uint16_t I2CAddress, uint8_t channel, uint8_t* pBuffer, uint8_t resolution, uint8_t gain, double_t* analogValue)
{
	uint8_t numDataBytes = 3;
	//uint8_t bufferTemp[4] = {0};  //buffer to store return data from sensor
	uint32_t counts = 0;
	uint8_t rawDataBytes[4] = {0};  //buffer to store return data from sensor

	bool success = MCP3424_SetConfig(I2CAddress, channel, &numDataBytes, resolution, gain);

	if(!success)
		return false;

	//wait long enough for conversion to happen after setting config
	vTaskDelay(MCP3424_GetDelay(resolution) / portTICK_RATE_MS);

	//Read the analog value
	const struct pios_i2c_txn txn_list_1[] = {
		{
		 .addr = I2CAddress,
		 .rw = PIOS_I2C_TXN_READ,
		 .len = 4, //Upper, Middle, Lower data bytes and config byte returned for 18 bit mode
		 .buf = rawDataBytes,
		 }
	};

	//Read data bytes
	if(PIOS_I2C_Transfer(PIOS_I2C_MAIN_ADAPTER, txn_list_1, NELEMENTS(txn_list_1))) {

		MCP3424_DecipherI2Cresponse(rawDataBytes, pBuffer, numDataBytes, resolution, &counts);

		double_t LSB = 2 * MCP3424_REFVOLTAGE / (1 << resolution);
		*analogValue = counts * LSB / gain;

		return true;
	}
	else
		return false;
}

