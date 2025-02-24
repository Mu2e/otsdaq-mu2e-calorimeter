//
#define __CLING__ 1

#include "srcs/mu2e_pcie_utils/dtcInterfaceLib/DTC.h"
#include "srcs/mu2e_pcie_utils/dtcInterfaceLib/DTCSoftwareCFO.h"

#include <stdint.h>
#include <stdio.h>

#include <boost/format.hpp>

using namespace DTCLib;

//-----------------------------------------------------------------------------
// float_to_uint16_1st_part
//-----------------------------------------------------------------------------
uint16_t float_to_uint16_1st_part(float float_value)
{
	// Use a pointer to point to the float variable
	uint32_t* float_bits = (uint32_t*)&float_value;

	// Extract first 16 bits for the first uint16
	uint16_t uint16_1 = (*float_bits) & 0xFFFF;
	cout << "float_value " << float_value << " First uint16: " << uint16_1 << " "
	     << (boost::format("%x") % uint16_1).str() << " Hex" << endl;
	return uint16_1;
}

//-----------------------------------------------------------------------------
// float_to_uint16_2nd_part
//-----------------------------------------------------------------------------
uint16_t float_to_uint16_2nd_part(float float_value)
{
	// Use a pointer to point to the float variable
	uint32_t* float_bits = (uint32_t*)&float_value;

	// Extract second 16 bits for the second uint16
	uint16_t uint16_2 = (*float_bits >> 16) & 0xFFFF;
	cout << "float_value " << float_value << " Second uint16: " << uint16_2 << " "
	     << (boost::format("%x") % uint16_2).str() << " Hex" << endl;
	return uint16_2;
}

//-----------------------------------------------------------------------------
// uint32_to_uint16_1st_part
//-----------------------------------------------------------------------------
uint16_t uint32_to_uint16_1st_part(uint32_t* uint32_value)
{
	// Extract first 16 bits for the first uint16
	uint16_t uint16_1 = (*uint32_value) & 0xFFFF;
	cout << "uint32_value " << uint32_value << " First uint16: " << uint16_1 << " "
	     << (boost::format("%x") % uint16_1).str() << " Hex" << endl;
	return uint16_1;
}

//-----------------------------------------------------------------------------
// uint32_to_uint16_2nd_part
//-----------------------------------------------------------------------------
uint16_t uint32_to_uint16_2nd_part(uint32_t* uint32_value)
{
	// Extract second 16 bits for the second uint16
	uint16_t uint16_2 = (*uint32_value) & 0xFFFF;
	cout << "uint32_value " << uint32_value << " Second uint16: " << uint16_2 << " "
	     << (boost::format("%x") % uint16_2).str() << " Hex" << endl;
	return uint16_2;
}

//-----------------------------------------------------------------------------
// uint32_to_float
//-----------------------------------------------------------------------------
float uint32_to_float(uint32_t value)
{  //"9" test
	// Use a pointer to point to the float variable
	float* float_bits = (float*)&value;
	return *float_bits;
}

//-----------------------------------------------------------------------------
// APP_chksum
//-----------------------------------------------------------------------------
/**
 * @brief Compute checksum
 * @param ptr pointer to buffer data
 * @param len length of the buffer (it can be any number)
 * @return Checksum value
 */
unsigned APP_chksum(const void* ptr, int len)
{
	unsigned* src = (unsigned*)ptr;
	unsigned  sum;
	for(sum = 0; len >= 4; len -= 4)
		sum += *src++;
	if(len)
	{
		unsigned tmp = 0;
		memcpy(&tmp, src, len);
		sum += tmp;
	}
	return sum;
}

//-----------------------------------------------------------------------------
// print 16 bytes per line
// size - number of bytes to print, even
//-----------------------------------------------------------------------------
void print_buffer(const void* ptr, int nw)
{
	// int     nw  = nbytes/2;
	ushort* p16 = (ushort*)ptr;
	int     n   = 0;

	for(int i = 0; i < nw; i++)
	{
		if(n == 0)
			printf(" 0x%08x: ", i * 2);

		ushort word = p16[i];
		printf("0x%04x ", word);

		n += 1;
		if(n == 8)
		{
			printf("\n");
			n = 0;
		}
	}

	if(n != 0)
		printf("\n");
}

//-----------------------------------------------------------------------------
// fill_uint16_vector
//-----------------------------------------------------------------------------
void fill_uint16_vector(std::vector<uint16_t>& input_data)
{
	float    float_variable = 123.456;
	uint16_t uint16_1, uint16_2;

	uint16_1 = float_to_uint16_1st_part(float_variable);
	uint16_2 = float_to_uint16_2nd_part(float_variable);

	// Conversion test
	std::string str =
	    (boost::format("%x") % uint16_1).str() + (boost::format("%x") % uint16_2).str();
	uint32_t test32tofloat = static_cast<uint32_t>(stoul(str, 0, 16));
	cout << "Conversion test: uint32_t: " << str
	     << " float: " << uint32_to_float(test32tofloat) << endl;

	float param_1 = 100.1;  // REAL*4  // Command parameter #1
	float param_2 = 100.1;  // REAL*4  // Command parameter #2
	float param_3 = 100.1;  // REAL*4  // Command parameter #3
	float param_4 = 100.1;  // REAL*4  // Command parameter #4
	float param_5 = 100.1;  // REAL*4  // Command parameter #5
	float param_6 = 100.1;  // REAL*4  // Command parameter #6
	float param_7 = 100.1;  // REAL*4  // Command parameter #7
	float param_8 = 100.1;  // REAL*4  // Command parameter #8
	float param_9 = 100.1;  // REAL*4  // Command parameter #9

	input_data.push_back(0xC101);  // proc_commandI3

	input_data.push_back(0x4344);  // 620     //INT16     // Structure id for the extended
	                               // command request ('CD')
	input_data.push_back(
	    0x0001);  // 622     //INT16     // Command request (enumerated), Bit 0 to 15
	              // command encoded,
	              //  Bit 16 must be 1=execute, reset by firmware after command
	// execution
	input_data.push_back(
	    float_to_uint16_1st_part(param_1));  // 624     //REAL*4  // Command parameter #1
	input_data.push_back(float_to_uint16_2nd_part(param_1));
	input_data.push_back(
	    float_to_uint16_1st_part(param_2));  // 628     //REAL*4  // Command parameter #2
	input_data.push_back(float_to_uint16_2nd_part(param_2));
	input_data.push_back(
	    float_to_uint16_1st_part(param_3));  // 632     //REAL*4  // Command parameter #3
	input_data.push_back(float_to_uint16_2nd_part(param_3));
	input_data.push_back(
	    float_to_uint16_1st_part(param_4));  // 636     //REAL*4  // Command parameter #4
	input_data.push_back(float_to_uint16_2nd_part(param_4));
	input_data.push_back(
	    float_to_uint16_1st_part(param_5));  // 640     //REAL*4  // Command parameter #5
	input_data.push_back(float_to_uint16_2nd_part(param_5));
	input_data.push_back(
	    float_to_uint16_1st_part(param_6));  // 644     //REAL*4  // Command parameter #6
	input_data.push_back(float_to_uint16_2nd_part(param_6));
	input_data.push_back(
	    float_to_uint16_1st_part(param_7));  // 648     //REAL*4  // Command parameter #7
	input_data.push_back(float_to_uint16_2nd_part(param_7));
	input_data.push_back(
	    float_to_uint16_1st_part(param_8));  // 652     //REAL*4  // Command parameter #8
	input_data.push_back(float_to_uint16_2nd_part(param_8));
	input_data.push_back(
	    float_to_uint16_1st_part(param_9));  // 656     //REAL*4  // Command parameter #9
	input_data.push_back(float_to_uint16_2nd_part(param_9));

	uint32_t chksum32 = APP_chksum(&input_data, input_data.size());
	input_data.push_back(uint32_to_uint16_1st_part(
	    &chksum32));  // 660     //INT32   // Checksum for the extended command request
	input_data.push_back(uint32_to_uint16_2nd_part(&chksum32));
}

//-----------------------------------------------------------------------------
// root main function
//-----------------------------------------------------------------------------
void write_dcs_roc_block(int Link, int ROCSleepTime = 5000)
{
	std::vector<uint16_t> input_data;
	fill_uint16_vector(input_data);

	/*auto link = DTC_Link_ID(Link);

	uint32_t roc_mask = 0x1 << 4*Link;

	DTC dtc(DTC_SimMode_NoCFO,-1,roc_mask,"");
	std::this_thread::sleep_for(std::chrono::milliseconds(1));

	uint16_t u;
	int      rc;

	printf("starting block write DCS\n");
	// roc link 0x103 data_transfer request_ack increment_address timeout
	bool x = dtc.WriteROCBlock(link,259,input_data,0, 0, 100);
	// std::this_thread::sleep_for(std::chrono::microseconds(ROCSleepTime));

	//while((u=dtc.ReadROCRegister(link,128,100)) == 0){} // when the write operation ends
	the microprocessor
	                                                    // writes 0x8000 to register 0x128

	printf("r_128:0x%04x\n",u);

	u = dtc.ReadROCRegister(link,129,100); printf("r_129:0x%04x\n",u);

	int const nw = dtc.ReadROCRegister(link,255,100);

	printf(" -------------- nw = %5i\n",nw);
	std::vector<uint16_t> output_data;
	dtc.ReadROCBlock(output_data,link,259,nw,false,100);

	print_buffer(output_data.data(),nw);*/
}

//-----------------------------------------------------------------------------
// an example of cloning a function
//-----------------------------------------------------------------------------
void write_roc_1()
{
	DTC dtc(DTC_SimMode_NoCFO, -1, 0x1, "");
	std::this_thread::sleep_for(std::chrono::milliseconds(500));

	uint16_t u;
	int      rc;

	std::vector<uint16_t> data;
	data.push_back(100);
	data.push_back(101);
	data.push_back(102);

	bool x = dtc.WriteROCBlock(DTC_Link_0, 259, data, 0, 0, 100);

	u = dtc.ReadROCRegister(DTC_Link_0, 128, 100);
	printf("0x%04x\n", u);
	u = dtc.ReadROCRegister(DTC_Link_0, 129, 100);
	printf("0x%04x\n", u);

	vector<uint16_t> output_data;
	int const        nw = 3;
	dtc.ReadROCBlock(output_data, DTC_Link_0, 259, nw, false, 100);

	print_buffer(output_data.data(), nw);
}
