//
#define __CLING__ 1

#include "srcs/mu2e_pcie_utils/dtcInterfaceLib/DTC.h"
#include "srcs/mu2e_pcie_utils/dtcInterfaceLib/DTCSoftwareCFO.h"

using namespace DTCLib;

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
//
//-----------------------------------------------------------------------------
void write_testfranco(int Link, int ROCSleepTime = 5000)
{
	auto link = DTC_Link_ID(Link);

	uint32_t roc_mask = 0x1 << 4 * Link;

	DTC dtc(DTC_SimMode_NoCFO, -1, roc_mask, "");
	std::this_thread::sleep_for(std::chrono::milliseconds(1));

	uint16_t u;
	int      rc;

	std::vector<uint16_t> input_data;
	/*input_data.push_back(0xAABB);
	input_data.push_back(3); //payload size
	input_data.push_back(0xC012);  // proc_commandID
	input_data.push_back(0xA0);
	input_data.push_back(0xA1);
	input_data.push_back(0xA2);
	input_data.push_back(0xFFEE);*/

	input_data.push_back(0x004);
	input_data.push_back(0x002);
	input_data.push_back(0x003);

	printf("starting block write DCS\n");
	// roc link 0x103 data_transfer request_ack increment_address timeout
	bool x = dtc.WriteROCBlock(link, 263, input_data, 0, 0, 100);  // was 257
	// std::this_thread::sleep_for(std::chrono::microseconds(ROCSleepTime));

	while((u = dtc.ReadROCRegister(link, 128, 100)) == 0) {
	}  // when the write operation ends the micropro
	   // cessor writes 0x8000 to register 0x128

	printf("r_128:0x%04x\n", u);

	u = dtc.ReadROCRegister(link, 129, 100);
	printf("r_129:0x%04x\n", u);

	int nw = u - 4;  // number of words to read back

	//  int const nw = dtc.ReadROCRegister(link,255,100);

	printf(" -------------- nw = %5i\n", nw);
	std::vector<uint16_t> output_data;
	dtc.ReadROCBlock(output_data, link, 263, nw, false, 100);  // was 257

	print_buffer(output_data.data(), nw);
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
