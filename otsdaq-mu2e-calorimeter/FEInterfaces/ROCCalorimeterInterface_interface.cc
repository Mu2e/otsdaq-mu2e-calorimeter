#include "otsdaq-mu2e-calorimeter/FEInterfaces/MZB.h"
#include "otsdaq-mu2e-calorimeter/FEInterfaces/ROCCalorimeterInterface.h"

#include "otsdaq/Macros/InterfacePluginMacros.h"

#include "cetlib/filepath_maker.h"

#include <cstring>
#include <fstream>

using namespace ots;

#undef __MF_SUBJECT__
#define __MF_SUBJECT__ "FE-ROCCalorimeterInterface"

// extern "C" {
//     #include "MU2E-API/API_I2C.h"
//     #include "MU2E-API/SBL_utils.h"
// }

// 259 (and others) ==> the number of words in block read is written first as a block
// write
const std::set<DTCLib::roc_address_t> ROCCalorimeterInterface::SPECIAL_BLOCK_READ_ADDRS_(
    {263, 256, 257, 261, 262, 264, 260});

//=========================================================================================
ROCCalorimeterInterface::ROCCalorimeterInterface(
    const std::string&       rocUID,
    const ConfigurationTree& theXDAQContextConfigTree,
    const std::string&       theConfigurationPath)
    : ROCPolarFireCoreInterface(rocUID, theXDAQContextConfigTree, theConfigurationPath)
{
	INIT_MF("." /*directory used is USER_DATA/LOG/.*/);

	__CFG_COUT__ << "Constructor..." << __E__;

	__CFG_COUT_INFO__ << "ROCPolarFireCoreInterface instantiated with link: " << linkID_
	                  << " and EventWindowDelayOffset = " << delay_ << __E__;

	try
	{
		inputTemp_ = getSelfNode().getNode("inputTemperature").getValue<double>();
	}
	catch(...)
	{
		__CFG_COUT__ << "inputTemperature field not defined. Defaulting..." << __E__;
		inputTemp_ = 15.;
	}

	temp1_.noiseTemp(inputTemp_);

	// Moved to ROCPolarFireCoreInterface::ROCPolarFireCoreInterface
	// registerFEMacroFunction("Setup for Pattern Data Taking", //Moved to
	// otsdaq_mu2e/otsdaq-mu2e/FEInterfaces/ROCPolarFireCoreInterfaceImpl.cc
	//                         static_cast<FEVInterface::frontEndMacroFunction_t>(
	//                             &ROCCalorimeterInterface::SetupForPatternDataTaking),
	//                         std::vector<std::string>{}, //inputs parameters
	//                         std::vector<std::string>{}, //output parameters
	//                         1);  // requiredUserPermissions

	registerFEMacroFunction(
	    "Setup for Fixed-length Pattern Data Taking",
	    static_cast<FEVInterface::frontEndMacroFunction_t>(
	        &ROCCalorimeterInterface::SetupForPatternFixedLengthDataTaking),
	    std::vector<std::string>{
	        "Fixed Length of Event [units of 16-bit words, Default := 8]"},  // inputs
	                                                                         // parameters
	    std::vector<std::string>{},  // output parameters
	    1);                          // requiredUserPermissions

	registerFEMacroFunction(
	    "Send Mz Command",
	    static_cast<FEVInterface::frontEndMacroFunction_t>(
	        &ROCCalorimeterInterface::SendMzCommand),
	    std::vector<std::string>{"command tag from mz manual",
	                             "argument 0, Default := 0]",
	                             "argument 1, Default := 0]",
	                             "argument 2, Default := 0]",
	                             "argument 3, Default := 0]",
	                             "argument 4, Default := 0]",
	                             "argument 5, Default := 0]",
	                             "argument 6, Default := 0]",
	                             "argument 7, Default := 0]",
	                             "argument 8, Default := 0]",
	                             "argument 9, Default := 0]"},  // inputs parameters
	    std::vector<std::string>{},                             // output parameters
	    1);                                                     // requiredUserPermissions

	registerFEMacroFunction(
	    "Enable and Power SiPMs",
	    static_cast<FEVInterface::frontEndMacroFunction_t>(
	        &ROCCalorimeterInterface::EnableAndPowerSiPMs),
	    std::vector<std::string>{
	        "HV Enabled, Default := 0]",
	        "Bias voltage to set, Default := 0]"},  // inputs parameters
	    std::vector<std::string>{},                 // output parameters
	    1);                                         // requiredUserPermissions

	registerFEMacroFunction(
	    "Set Board Voltages",
	    static_cast<FEVInterface::frontEndMacroFunction_t>(
	        &ROCCalorimeterInterface::SetBoardVoltages),
	    std::vector<std::string>{"configuration folder, Default:= nominal",
	                             "Left/Right, Default := 0]",
	                             "Board Number in Crate, Default := 0]",
	                             "Crate Number, Default := 0]",
	                             "Half Number, Default := 0]",
	                             "Disk Number, Default := 0]",
	                             "Board ID, Default := -1]",
	                             "HV Enabled, Default := 0]"},  // inputs parameters
	    std::vector<std::string>{},                             // output parameters
	    1);                                                     // requiredUserPermissions

	registerFEMacroFunction(
	    "Configure Link",
	    static_cast<FEVInterface::frontEndMacroFunction_t>(
	        &ROCCalorimeterInterface::ConfigureLink),
	    std::vector<std::string>{"Configuration folder, Default:= nominal",
	                             "HV Enabled, Default := 0]",
	                             "Upload MZB calibration parameters, Default := 0]",
	                             "Upload DiRAC thresholds, Default := 0]",
	                             "Offset, Default := 0]"},  // inputs parameters

	    std::vector<std::string>{},  // output parameters
	    1);                          // requiredUserPermissions

	registerFEMacroFunction(
	    "Calibrate Mzb",
	    static_cast<FEVInterface::frontEndMacroFunction_t>(
	        &ROCCalorimeterInterface::CalibrateMZB),
	    std::vector<std::string>{"BoardID, Default := -1]"},  // inputs parameters
	    std::vector<std::string>{},                           // output parameters
	    1);                                                   // requiredUserPermissions

	registerFEMacroFunction(
	    "SetADCsThresholds",
	    static_cast<FEVInterface::frontEndMacroFunction_t>(
	        &ROCCalorimeterInterface::SetADCsThresholds),
	    std::vector<std::string>{"BoardID, Default := -1]",
	                             "Offset, Default := 0]"},  // inputs parameters
	    std::vector<std::string>{},                         // output parameters
	    1);                                                 // requiredUserPermissions

	registerFEMacroFunction(
	    "SetupForNoiseTaking",
	    static_cast<FEVInterface::frontEndMacroFunction_t>(
	        &ROCCalorimeterInterface::SetupForNoiseTaking),
	    std::vector<std::string>{
	        "Number of noise samples per evt [Default := 20]"},  // inputs parameters
	    std::vector<std::string>{},                              // output parameters
	    1);  // requiredUserPermissions

	registerFEMacroFunction(
	    "Read ROC Error Counter",
	    static_cast<FEVInterface::frontEndMacroFunction_t>(
	        &ROCCalorimeterInterface::ReadROCErrorCounter),
	    std::vector<std::string>{"Address to read, Default := 0]"},  // inputs parameters
	    std::vector<std::string>{"Status"},                          // output parameters
	    1);  // requiredUserPermissions

	registerFEMacroFunction(
	    "Setup for ADCs Data Taking",
	    static_cast<FEVInterface::frontEndMacroFunction_t>(
	        &ROCCalorimeterInterface::SetupForADCsDataTaking),
	    std::vector<std::string>{
	        "Set Threshold? [bool, Default := 0]",
	        "Threshold [units of adccounts, Default := 2300]"},  // inputs parameters
	    std::vector<std::string>{},                              // output parameters
	    1);  // requiredUserPermissions

	registerFEMacroFunction(
	    "Toggle MB Busy for Noise",
	    static_cast<FEVInterface::frontEndMacroFunction_t>(
	        &ROCCalorimeterInterface::ToggleMBBusy),
	    std::vector<std::string>{
	        "On/Off [Busy On/Off, Default := 1]"},  // inputs parameters
	    std::vector<std::string>{},                 // output parameters
	    1);                                         // requiredUserPermissions

	registerFEMacroFunction("ROC Status",
	                        static_cast<FEVInterface::frontEndMacroFunction_t>(
	                            &ROCCalorimeterInterface::GetStatus),
	                        std::vector<std::string>{},          // inputs parameters
	                        std::vector<std::string>{"Status"},  // output parameters
	                        1);  // requiredUserPermissions

	registerFEMacroFunction("ROC Slow Control",
	                        static_cast<FEVInterface::frontEndMacroFunction_t>(
	                            &ROCCalorimeterInterface::ROCSlowControl),
	                        std::vector<std::string>{},          // inputs parameters
	                        std::vector<std::string>{"Status"},  // output parameters
	                        1);  // requiredUserPermissions

	registerFEMacroFunction("Read MB Registers",
	                        static_cast<FEVInterface::frontEndMacroFunction_t>(
	                            &ROCCalorimeterInterface::ReadMBRegisters),
	                        std::vector<std::string>{
	                            "Number of 16 bits words to read, Default := 20]",
	                            "Mezzanine Address, Default := 0]"},  // inputs parameters
	                        std::vector<std::string>{"Status"},       // output parameters
	                        1);  // requiredUserPermissions

	// registerFEMacroFunction("SetROCCaloVoltageChannel",
	//                         static_cast<FEVInterface::frontEndMacroFunction_t>(
	//                             &ROCCalorimeterInterface::SetVoltageChannel),
	//                         std::vector<std::string>{"channelNumber", "value"},
	//                         //inputs parameters std::vector<std::string>{}, //output
	//                         parameters 1);  // requiredUserPermissions

	// registerFEMacroFunction("GetROCCaloVoltageChannel",
	//                         static_cast<FEVInterface::frontEndMacroFunction_t>(
	//                             &ROCCalorimeterInterface::GetVoltageChannel),
	//                         std::vector<std::string>{"channelNumber"}, //inputs
	//                         parameters std::vector<std::string>{"readValue"}, //output
	//                         parameters 1);  // requiredUserPermissions

	registerFEMacroFunction("Configure State Machine",
	                        static_cast<FEVInterface::frontEndMacroFunction_t>(
	                            &ROCCalorimeterInterface::Configure),
	                        std::vector<std::string>{},  // inputs parameters
	                        std::vector<std::string>{},  // output parameters
	                        1);                          // requiredUserPermissions

	// // function for webgui
	// 	registerFEMacroFunction("GetROCCaloTemperatureChannel",
	// 	                        static_cast<FEVInterface::frontEndMacroFunction_t>(
	// 	                            &ROCCalorimeterInterface::GetTempChannel),
	// 	                        std::vector<std::string>{"channelNumber"}, //inputs
	// parameters 	                        std::vector<std::string>{"readValue"}, //output parameters 	                        1);  //
	// requiredUserPermissions

}  // end constructor()

//==========================================================================================
ROCCalorimeterInterface::~ROCCalorimeterInterface(void)
{
	// NOTE:: be careful not to call __FE_COUT__ decoration because it uses the
	// tree and it may already be destructed partially
	__COUT__ << FEVInterface::interfaceUID_ << "Destructed." << __E__;
}  // end destructor()

//==================================================================================================
void ROCCalorimeterInterface::writeEmulatorRegister(uint16_t address,
                                                    uint16_t data_to_write)
{
	__CFG_COUT__ << "emulator write" << __E__;

	return;

}  // end writeRegister()

//==================================================================================================
uint16_t ROCCalorimeterInterface::readEmulatorRegister(
    uint16_t address)  // not useful, broken by Luca
{
	__CFG_COUT__ << "emulator read" << __E__;

	if(address == 6 || address == 7)
		return ROCPolarFireCoreInterface::readEmulatorRegister(address);
	if(address == ROC_ADDRESS_EW_LENGHT)
		return 0x5;
	else if(address == ROC_ADDRESS_EW_BLIND)
	{
		temp1_.noiseTemp(temp1_.GetBoardTempC());
		return temp1_.GetBoardTempC() * 256;
	}
	else
		return 0xBAAD;

}  // end readRegister()

//==================================================================================================
// return false to stop workloop thread
bool ROCCalorimeterInterface::emulatorWorkLoop(void)
{
	__COUT__ << FEVInterface::interfaceUID_ << "emulator working..." << __E__;

	temp1_.noiseTemp(inputTemp_);
	return true;  // true to keep workloop going

	//	float input, inputTemp;
	//	int addBoard, a;
	//	//
	//	addBoard = 105;
	//	inputTemp = 15.;
	//	a = 0;
	//	while( a < 20 ) {
	//		temp1.noiseTemp(inputTemp);
	//		temperature = temp1.GetBoardTempC();
	//		a++;
	//		return temperature;
	//		usleep(1000000);
	//		return true;
	//	}
}  // end emulatorWorkLoop()

//==================================================================================================
void ROCCalorimeterInterface::universalBlockRead(char*        address,
                                                 char*        returnValue,
                                                 unsigned int numberOfBytes)
{
	std::vector<DTCLib::roc_data_t> data;
	readROCBlock(data,
	             *((DTCLib::roc_address_t*)address),
	             numberOfBytes / 2,
	             false /*incAddress*/);
	if(data.size() != numberOfBytes / 2)
	{
		__FE_SS__ << "Illegal number of bytes: " << data.size() << " not "
		          << numberOfBytes / 2 << __E__;
		__FE_SS_THROW__;
	}
	memcpy(returnValue, &data[0], data.size() * 2);
}

//==================================================================================================
void ROCCalorimeterInterface::ROCSlowControl(__ARGS__)
{
	std::stringstream os;
	os << __E__;

	os << "Board unique ID:" << __E__;
	os << __E__;

	os << "Reg 145: 0x" << std::hex << thisDTC_->ReadROCRegister(linkID_, 145, 100)
	   << __E__;
	os << "Reg 146: 0x" << std::hex << thisDTC_->ReadROCRegister(linkID_, 146, 100)
	   << __E__;
	os << "Reg 147: 0x" << std::hex << thisDTC_->ReadROCRegister(linkID_, 147, 100)
	   << __E__;

	std::vector<DTCLib::roc_data_t> data;
	readROCBlock(data, 263, 12, false /*incAddress*/);
	if(data.size() != 12)
	{
		__FE_SS__ << "Illegal number of bytes: " << data.size() << " not " << 12 << __E__;
		__FE_SS_THROW__;
	}

	os << std::dec << __E__;

	os << "Slow control values:" << __E__;

	os << __E__;

	os << "Word " << 0 << ":  3.3D current: " << (data[0] * 2.687) << " mA" << __E__;
	os << "Word " << 1 << ":  3.3A current: " << (data[1] * 2.687) << " mA" << __E__;
	os << "Word " << 2 << ":  1.8D current: " << (data[2] * 2.687) << " mA" << __E__;
	os << "Word " << 3 << ":  1.8A current: " << (data[3] * 2.687) << " mA" << __E__;
	os << "Word " << 4 << ":  VTRX RRSI: " << (data[4] * 0.806) << " mV" << __E__;
	os << "Word " << 5 << ":  RADMON: " << (data[5] * 0.806) << " mV" << __E__;
	os << "Word " << 6 << ":  TEMP0: " << (((data[6] * 0.806) - 509) / 6.45) << " C"
	   << __E__;
	os << "Word " << 7 << ":  TEMP1: " << (((data[7] * 0.806) - 509) / 6.45) << " C"
	   << __E__;
	os << "Word " << 8 << ":  2.5 voltage: " << (data[8] * 0.806) << " mV" << __E__;
	os << "Word " << 9 << ":  2.5D voltage: " << (data[9] * 0.806) << " mV" << __E__;
	os << "Word " << 10 << ": 5 voltage: " << (data[10] * 1.612) << " mV" << __E__;
	os << "Word " << 11 << ": 28 current: " << (data[11] * 2.687) << " mA" << __E__;

	os << __E__;

	os << "Reading VTRX registers:" << __E__;
	os << __E__;

	/*  std::vector<DTCLib::roc_data_t> data3;
	  readROCBlock(data3,
	           264,
	           7,
	           false);
	  if(data3.size() != 7)	{
	    __FE_SS__ << "Illegal number of bytes: "  <<  data3.size() << " not " << 7 <<
	  __E__;
	    __FE_SS_THROW__;
	  }

	  os << "Word " << 0 << ":  VTRX_REG_Control: 0x" << std::hex << data3[0] << " " <<
	  __E__; os << "Word " << 1 << ":  VTRX_REG_ModCurrent: 0x" << std::hex << data3[1]<<
	  " " << __E__; os << "Word " << 2 << ":  VTRX_REG_BiasCurrent: 0x" << std::hex <<
	  data3[2] << " " << __E__; os << "Word " << 3 << ":  VTRX_REG_PreEmphasis: 0x" <<
	  std::hex  << data3[3] << " " << __E__; os << "Word " << 4 << ":  VTRX_REG_ModMask:
	  0x" << std::hex << data3[4]<< " " << __E__; os << "Word " << 5 << ":
	  VTRX_REG_BiasMask: 0x" << std::hex << data3[5] << " " << __E__; os << "Word " << 6
	  << ":  VTRX_REG_PreDriver: 0x" << std::hex << data3[6]   << " " << __E__;

	  os <<  __E__;

	*/

	os << "Reading MB registers:" << __E__;
	os << __E__;

	std::vector<DTCLib::roc_data_t> data2;
	readROCBlock(data2, 261, 20, false /*incAddress*/);

	if(data2.size() != 20)
	{
		__FE_SS__ << "Illegal number of bytes: " << data2.size() << " not " << 20 << "  !"
		          << __E__;
		__FE_SS_THROW__;
	}

	for(size_t i = 0; i < data2.size(); i++)
	{
		os << "Word " << i << ": 0x" << std::hex << data2[i] << __E__;
	}

	__SET_ARG_OUT__("Status", os.str());
}

//==================================================================================================
void ROCCalorimeterInterface::ReadMBRegisters(__ARGS__)
{
	unsigned int nwords = __GET_ARG_IN__(
	    "Number of 16 bits words to read, Default := 20]", unsigned int, 20);
	int offset = __GET_ARG_IN__("Mezzanine Address, Default := 0]", int, 0);

	std::vector<DTCLib::roc_data_t> data;
	readROCBlock(data, 261, nwords, false /*incAddress*/);

	if(data.size() != nwords)
	{
		__FE_SS__ << "Illegal number of bytes: " << data.size() << " not " << nwords
		          << "  !" << offset << __E__;
		__FE_SS_THROW__;
	}

	std::stringstream os;

	for(size_t i = 0; i < data.size(); i++)
	{
		os << "Word " << i << ": 0x" << std::hex << data[i] << __E__;
	}

	__SET_ARG_OUT__("Status", os.str());
}

//==================================================================================================
void ROCCalorimeterInterface::readROCBlock(std::vector<DTCLib::roc_data_t>& data,
                                           DTCLib::roc_address_t            address,
                                           uint16_t                         wordCount,
                                           bool incrementAddress)
{
	__FE_COUT__ << "Calling read ROC block: link number " << std::dec << linkID_
	            << ", address = " << address << ", wordCount = " << wordCount
	            << ", incrementAddress = " << incrementAddress << __E__;

	uint16_t u;
	u = thisDTC_->ReadROCRegister(linkID_, 0, 100);

	// check if special Block Write required
	if(ROCCalorimeterInterface::SPECIAL_BLOCK_READ_ADDRS_.find(address) !=
	   ROCCalorimeterInterface::SPECIAL_BLOCK_READ_ADDRS_.end())
	{
		__FE_COUT__ << "Doing special block write!" << __E__;

		switch(address)
		{
		case 261:
			// writeROCBlock({wordCount*2, 0}, address, false /* incrementAddress*/);
			writeROCBlock({static_cast<unsigned short>(wordCount * 2), 0},
			              address,
			              false /* incrementAddress*/);
			break;

		case 260:
			address = offsetof(EE_DATABUF_t, apdTemp_tag);
			wordCount =
			    offsetof(EE_DATABUF_t, apdPwrs_tag) - offsetof(EE_DATABUF_t, apdTemp_tag);
			writeROCBlock({wordCount, address}, 261, false /* incrementAddress*/);
			wordCount = wordCount / 2;
			break;

		case 257:
			writeROCBlock({wordCount}, address, false /* incrementAddress*/);
			break;

		case 263:
			writeROCBlock({wordCount}, address, false /* incrementAddress*/);
			break;

		case 264:
			writeROCBlock({wordCount}, address, false /* incrementAddress*/);
			break;

		default:
			writeROCBlock({wordCount}, address, false /* incrementAddress*/);
			break;
		}

		// uint16_t j = 0;
		while((u = thisDTC_->ReadROCRegister(linkID_, 128, 100)) == 0)
		{
			/*			usleep(100);
			          j++;
			          if(j == 100)
			          {
			          __FE_SS__ << "ROC block failed at 128"  << __E__;
			          __FE_SS_THROW__;
			          }*/
		}  // when the write operation ends the micropro
		// cessor writes 0x8000 to register 0x128
		__COUT__ << "r_128: 0x" << std::hex << u << __E__;
		usleep(1000);

		// j = 0;
		while((u = thisDTC_->ReadROCRegister(linkID_, 129, 100)) == 0)
		{
			/*usleep(100);
			  j++;
			  if(j == 100)
			  {
			  __FE_SS__ << "ROC block failed at 129"  << __E__;
			  __FE_SS_THROW__;
			  }*/
		}

		__COUT__ << "r_129: 0x" << std::hex << u << __E__;

		// wordCount = u - 4;  // number of words to read back
	}
	__FE_COUTV__(data.size());
	__FE_COUTV__(wordCount);
	__FE_COUTV__(u - 4);
	thisDTC_->ReadROCBlock(data, linkID_, address, u - 4, incrementAddress, 0);
	__FE_COUTV__(data.size());
	// only fix data if received more than needed - TODO fix in ROC firmware
	while(data.size() > wordCount)
		data.pop_back();

	if(data.size() != (long unsigned int)u - 4)
	{
		__FE_SS__ << "ROC block read failed, expecting " << wordCount
		          << " words, and read " << data.size() << " words." << __E__;
		__FE_SS_THROW__;
	}

}  // end readBlock()

//==================================================================================================

/*

//==================================================================================================
void ROCCalorimeterInterface::readMZBregisters(std::vector<DTCLib::roc_data_t>& data,
uint16_t                   wordCount,
bool incrementAddress)
{
__FE_COUT__ << "Calling read MZB block: link number " << std::dec << linkID_
<< ", address = " << address << ", wordCount = " << wordCount
<< ", incrementAddress = " << incrementAddress << __E__;

uint16_t u;
u = thisDTC_->ReadROCRegister(linkID_, 0, 100);



//uint16_t j = 0;
while((u = thisDTC_->ReadROCRegister(linkID_, 128, 100)) == 0)

__COUT__ << "r_128: 0x" << std::hex << u << __E__;
usleep(1000);

while((u = thisDTC_->ReadROCRegister(linkID_, 129, 100)) == 0)
{
usleep(100);
}

__COUT__ << "r_129: 0x" << std::hex << u << __E__;


thisDTC_->ReadROCBlock(data, linkID_, 261, u-4, incrementAddress, 0);

//only fix data if received more than needed - TODO fix in ROC firmware
while(data.size() > wordCount) data.pop_back();

if(data.size() != wordCount)
{
__FE_SS__ << "ROC block read failed, expecting " << wordCount
<< " words, and read " << data.size() << " words." << __E__;
__FE_SS_THROW__;
}


for(int i=0; i<data.size(); i++){
__FE_COUT__ << "Calling read MZB block: link number " << std::dec << linkID_
<< ", address = " << address << ", wordCount = " << wordCount
<< ", incrementAddress = " << incrementAddress << __E__;
}

}  // end readBlock()

*/

//======================================================================================================

void ROCCalorimeterInterface::configure(void)
try
{
	ROCPolarFireCoreInterface::configure();

	// consider that we know all the init files
	// all the init information are stored in the configuration tree    //
	// set parameter
	// int linkID = getSelfNode().getNode("linkID").getValue<int>();
	//	__COUTV__(linkID);

	runSequenceOfCommands(
	    "ROCTypeLinkTable/LinkToConfigureSequence"); /*Run Configure Sequence Commands*/

	//    int myTemp = GetTemperature(1);
	// 	__COUTV__(myTemp);
	// 	__COUT__<<"my temp"<<myTemp<<__E__;
	//     __COUTV__(myTemp);
	//     __MCOUTV__(myTemp);
}
catch(const std::runtime_error& e)
{
	__FE_COUT__ << "Error caught: " << e.what() << __E__;
	throw;
}
catch(...)
{
	__FE_SS__ << "Unknown error caught. Check printouts!" << __E__;
	try
	{
		throw;
	}  // one more try to printout extra info
	catch(const std::exception& e)
	{
		ss << "Exception message: " << e.what();
	}
	catch(...)
	{
	}
	__FE_SS_THROW__;
}

//==================================================================================================
bool ROCCalorimeterInterface::running(void)
{
	// SetupForPatternFixedLengthDataTaking(40);
	SetupForADCsDataTaking(0, 2300);

	return false;
}

//==================================================================================================
void ROCCalorimeterInterface::Configure(__ARGS__)
{
	__COUT_INFO__ << "Configure called" << __E__;
	configure();
}

//==================================================================================================
void ROCCalorimeterInterface::SetVoltageChannel(__ARGS__)
{
	__COUT_INFO__ << "Set called" << __E__;
}

//==================================================================================================
void ROCCalorimeterInterface::GetVoltageChannel(__ARGS__)
{
	__COUT_INFO__ << "Get called" << __E__;
	__SET_ARG_OUT__("readValue", 12);
}
//==================================================================================================
void ROCCalorimeterInterface::GetTempChannel(__ARGS__)
{
	__COUT_INFO__ << "Temp is" << __E__;
	int channelnumber = __GET_ARG_IN__("channelNumber", int);
	__SET_ARG_OUT__("readValue", GetTemperature(channelnumber));
}

//==================================================================================================
int ROCCalorimeterInterface::GetTemperature(int idchannel)  // wrong address
{
	return readRegister(ROC_ADDRESS_EW_LENGHT);
}

// //==================================================================================================
// Moved to ROCPolarFireCoreInterface::ROCPolarFireCoreInterface
// Moved to otsdaq_mu2e/otsdaq-mu2e/FEInterfaces/ROCPolarFireCoreInterfaceImpl.cc
// void ROCCalorimeterInterface::SetupForPatternDataTaking(__ARGS__)
// {
// 	__COUT_INFO__ << "SetupForPatternDataTaking()" << __E__;

// 	//For future, to get link ID of this ROC:
// 	__FE_COUTV__(getLinkID());

// 	writeRegister(14,1);  //ROC reset
// 	writeRegister(8,1 << 4);
// 	writeRegister(30,0);
// 	writeRegister(29,1);

// 	__COUT_INFO__ << "end SetupForPatternDataTaking()" << __E__;

// 	// __SET_ARG_OUT__("readValue",GetTemperature(channelnumber));
// } //end SetupForPatternDataTaking()

//==================================================================================================
void ROCCalorimeterInterface::SetupForPatternFixedLengthDataTaking(__ARGS__)
{
	unsigned int numberOfWords = __GET_ARG_IN__(
	    "Fixed Length of Event [units of 16-bit words, Default := 8]", uint32_t, 8);

	SetupForPatternFixedLengthDataTaking(numberOfWords);

}  // end SetupForPatternFixedLengthDataTaking()

//==================================================================================================
void ROCCalorimeterInterface::SetupForPatternFixedLengthDataTaking(
    unsigned int numberOfWords)
{
	__COUT_INFO__ << "SetupForPatternFixedLengthDataTaking()" << __E__;

	writeRegister(ROC_ADDRESS_MASK_A, 0);
	writeRegister(ROC_ADDRESS_MASK_B, 0);

	writeRegister(ROC_ADDRESS_IS_PATTERN, 0);
	writeRegister(ROC_ADDRESS_IS_COUNTER, 1);
	writeRegister(ROC_ADDRESS_COUNTER_IS_FALLING, 1);
	writeRegister(ROC_ADDRESS_EW_LENGHT, 5000);

	writeRegister(ROC_ADDRESS_DDRRESET, 1);
	writeRegister(ROC_ADDRESS_ANALOGRESET, 1);
	writeRegister(ROC_ADDRESS_ANALOGRESET, 0);

	__FE_COUTV__(numberOfWords);
	writeRegister(ROC_ADDRESS_COUNTER_SIZE, numberOfWords);

	__COUT_INFO__ << "end SetupForPatternFixedLengthDataTaking()" << __E__;
}  // end SetupForPatternFixedLengthDataTaking()

//==================================================================================================

//==================================================================================================
void ROCCalorimeterInterface::SendMzCommand(std::string command, float paramVect[])
{
	__COUT_INFO__ << "SendMzCommand()" << __E__;

	uint8_t* vectToWrite;

	// writeRegister(ROC_ADDRESS_MZB_BUSY, 1);

	// MZB_OSCMDCODE_t cmd_code = mz_string_to_enum(command.c_str());
	MZB_OSCMDCODE_t cmd_code = SYNTAX_ERROR;

	for(size_t i = 0; i < sizeof(code_map) / sizeof(code_map[0]); i++)
	{
		// if (code_map[i].str == command.c_str()) {
		if(strcmp(code_map[i].str, command.c_str()) == 0)
		{
			cmd_code = code_map[i].code;
			break;
		}
	}

	if(cmd_code == SYNTAX_ERROR)
	{
		__FE_SS__ << "Wrong MZB command, please check the inserted string! " << __E__
		          << command << __E__ << command.c_str() << __E__ << code_map[21].str
		          << __E__;
		__FE_SS_THROW__;
	}

	vectToWrite = MZB_Encode_CMD_Command_raw(cmd_code, paramVect);

	// uint16_t *input_data = &vectToWrite;
	//__COUT_INFO__ << "Mz debug ****" << __E__;

	std::vector<uint16_t> input_data;
	for(std::size_t i = 0; i < MZ_BUFFER_SIZE; i += 2)
	{
		uint16_t value = (static_cast<uint16_t>(vectToWrite[i]) << 8) |
		                 (static_cast<uint16_t>(vectToWrite[i + 1]));
		__COUT_INFO__ << std::hex << std::setprecision(4) << std::setfill('0') << "0x"
		              << value << __E__;
		input_data.push_back(value);
	}

	writeROCBlock(input_data, MZ_ADDRESS, false /* incrementAddress*/);

	// free(vectToWrite);

	// writeRegister(ROC_ADDRESS_MZB_BUSY, 0);

	__COUT_INFO__ << "end SendMzCommand()" << __E__;
}  // end SendMzCommand()

//==================================================================================================

void ROCCalorimeterInterface::SendMzCommand(__ARGS__)
{
	std::string command = __GET_ARG_IN__("command tag from mz manual", std::string, "");
	float       paramVect[9];

	paramVect[0] = __GET_ARG_IN__("argument 0, Default := 0]", float, NAN);
	paramVect[1] = __GET_ARG_IN__("argument 1, Default := 0]", float, NAN);
	paramVect[2] = __GET_ARG_IN__("argument 2, Default := 0]", float, NAN);
	paramVect[3] = __GET_ARG_IN__("argument 3, Default := 0]", float, NAN);
	paramVect[4] = __GET_ARG_IN__("argument 4, Default := 0]", float, NAN);
	paramVect[5] = __GET_ARG_IN__("argument 5, Default := 0]", float, NAN);
	paramVect[6] = __GET_ARG_IN__("argument 6, Default := 0]", float, NAN);
	paramVect[7] = __GET_ARG_IN__("argument 7, Default := 0]", float, NAN);
	paramVect[8] = __GET_ARG_IN__("argument 8, Default := 0]", float, NAN);

	SendMzCommand(command, paramVect);

}  // end SendMzCommand()

//==================================================================================================

//==================================================================================================

void ROCCalorimeterInterface::EnableAndPowerSiPMs(__ARGS__)
{
	bool  hvonoff = __GET_ARG_IN__("HV Enabled, Default := 0]", bool, 0);
	float vbias   = __GET_ARG_IN__("Bias voltage to set, Default := 0]", float, 0);

	EnableAndPowerSiPMs(hvonoff, vbias);

}  // end EnableAndPowerSiPMs()

//==================================================================================================

void ROCCalorimeterInterface::SetBoardVoltages(__ARGS__)
{
	std::string conf =
	    __GET_ARG_IN__("configuration folder, Default:= nominal", std::string, "nominal");

	// boardID == L/R + 2*bordNum + 8* cratenum + 40*half + 80*disk

	int leftright = __GET_ARG_IN__("Left/Right, Default := 0]", int, 0);
	int boardNum  = __GET_ARG_IN__("Board Number in Crate, Default := 0]", int, 0);
	int crateNum  = __GET_ARG_IN__("Crate Number, Default := 0]", int, 0);
	int halfNum   = __GET_ARG_IN__("Half Number, Default := 0]", int, 0);
	int diskNum   = __GET_ARG_IN__("Disk Number, Default := 0]", int, 1);

	int boardID = __GET_ARG_IN__("Board ID, Default := -1]", int, -1);

	bool hvonoff = __GET_ARG_IN__("HV Enabled, Default := 0]", bool, 0);

	if(boardID == -1)
		boardID = leftright + 2 * boardNum + 8 * crateNum + 40 * halfNum + 80 * diskNum;

	SetBoardVoltages(hvonoff, boardID, conf);

}  // end SetBoardVoltages()

//==================================================================================================

//==================================================================================================

void ROCCalorimeterInterface::ConfigureLink(__ARGS__)
{
	std::string conf =
	    __GET_ARG_IN__("Configuration folder, Default:= nominal", std::string, "nominal");
	bool hvonoff = __GET_ARG_IN__("HV Enabled, Default := 0]", bool, 0);
	bool doCalibration =
	    __GET_ARG_IN__("Upload MZB calibration parameters, Default := 0]", bool, 0);
	bool setThresholds =
	    __GET_ARG_IN__("Upload DiRAC thresholds, Default := 0]", bool, 0);
	int offset = __GET_ARG_IN__("Offset, Default := 0]", int, 0);

	std::string confFile = "boardMap.config";

	ConfigureLink(conf, confFile, hvonoff, doCalibration, setThresholds, offset);
}  // end ConfigureLink()

//==================================================================================================

//==================================================================================================

void ROCCalorimeterInterface::ConfigureLink(std::string conf,
                                            std::string confFile,
                                            bool        hvonoff,
                                            bool        doCalibration,
                                            bool        setThresholds,
                                            int         offset)
{
	DTCLib::roc_address_t address = 147;
	DTCLib::roc_data_t    readVal;
	readVal = readRegister(address);

	cet::filepath_lookup lookup_policy("MU2E_CALORIMETER_CONFIG_PATH");
	auto                 filename = lookup_policy(confFile);

	std::ifstream confMap(filename);
	if(!confMap.is_open())
	{
		__FE_SS__ << "Could not open file: " << filename << __E__;
		__FE_SS_THROW__;
		;
	}

	int boardid = -1;

	for(int iboard = 0; iboard < 161; iboard++)
	{
		int nboard;
		int notused;
		int buid;

		confMap >> nboard >> notused >> buid;
		if(readVal == buid)
		{
			boardid = nboard;
			break;
		}
	}
	confMap.close();

	if(boardid != -1)
	{
		if(doCalibration)
			CalibrateMZB(boardid);
		if(setThresholds)
			SetADCsThresholds(boardid, offset);
		SetBoardVoltages(hvonoff, boardid, conf);
		writeRegister(ROC_ADDRESS_BOARD_ID, boardid);
	}
	else
	{
		__FE_SS__ << "Cannot match board unique ID: readval is " << readVal
		          << ", while boardid is " << boardid << __E__;
		__FE_SS_THROW__;
		;
	}

}  // end ConfigureLink()

//==================================================================================================

//==================================================================================================

void ROCCalorimeterInterface::CalibrateMZB(__ARGS__)
{
	int boardID = __GET_ARG_IN__("BoardID, Default := -1]", int, -1);

	CalibrateMZB(boardID);

}  // end ConfigureLink()

//==================================================================================================

//==================================================================================================

void ROCCalorimeterInterface::CalibrateMZB(int boardID)
{
	char buff[50];
	sprintf(buff, "mzb%03d.config", boardID);

	cet::filepath_lookup lookup_policy("MU2E_CALORIMETER_CONFIG_PATH");
	auto                 filename = lookup_policy(std::string("mzbCalib/") +
                                  buff);  // FIXME! THIS NEEDS TO BE A VARIABLE

	std::ifstream confFile(filename);
	if(!confFile.is_open())
	{
		__FE_SS__ << "Could not open file: " << filename << __E__;
		__FE_SS_THROW__;
		;
	}

	__COUT_INFO__ << "Opening file: " << filename << __E__;

	for(int ichan = 0; ichan < 20; ichan++)
	{
		int   chindex;
		float slope;
		float offset;

		std::string command = "CALCARD";
		float       paramVect[9];

		confFile >> chindex;
		paramVect[0] = chindex + 1;

		for(int icalib = 0; icalib < 4; icalib++)
		{
			confFile >> slope >> offset;
			paramVect[(icalib * 2) + 1] = slope;
			paramVect[(icalib * 2) + 2] = offset;
			__COUT_INFO__ << slope << "  " << offset << __E__;
		}

		usleep(100000);

		SendMzCommand(command, paramVect);
	}

	confFile.close();

	__COUT_INFO__ << "Calibration done.." << filename << __E__;

}  // end CalibrateMZB()

//==================================================================================================

//==================================================================================================

void ROCCalorimeterInterface::SetADCsThresholds(__ARGS__)
{
	int boardID = __GET_ARG_IN__("BoardID, Default := -1]", int, -1);
	int offset  = __GET_ARG_IN__("offset, Default := 0]", int, 0);

	SetADCsThresholds(boardID, offset);
}  // end ConfigureLink()

void ROCCalorimeterInterface::SetADCsThresholds(int boardID, int offset)
{
	char buff[50];
	sprintf(buff, "dirac%03d.baseline", boardID);

	if(boardID >= 160)
	{
		__COUT_INFO__ << "Skipping setting thresholds to board " << boardID << __E__;
		return;
	}

	cet::filepath_lookup lookup_policy("MU2E_CALORIMETER_CONFIG_PATH");
	auto                 filename = lookup_policy(std::string("diracCalib/") +
                                  buff);  // FIXME! THIS NEEDS TO BE A VARIABLE

	std::ifstream confFile(filename);
	if(!confFile.is_open())
	{
		__FE_SS__ << "Could not open file: " << filename << __E__;

		for(int ichan = 0; ichan < 20; ichan++)
		{
			writeRegister(ROC_ADDRESS_BASE_THRESHOLD + ichan, 2300);
		}
	}
	else
	{
		__COUT_INFO__ << "Opening file: " << filename << __E__;

		for(int ichan = 0; ichan < 20; ichan++)
		{
			int   chindex;
			float baseline;
			float sigma;
			int   thr2set;

			confFile >> chindex;
			confFile >> baseline;
			confFile >> sigma;
			confFile >> thr2set;

			__COUT_INFO__ << chindex << "  " << baseline << "  " << sigma << "  "
			              << thr2set + offset << __E__;
			writeRegister(ROC_ADDRESS_BASE_THRESHOLD + ichan, thr2set + offset);
		}

		confFile.close();
	}

	__COUT_INFO__ << "Thresholds set done.." << filename << __E__;

}  // end SetADCsThresholds()

//==================================================================================================

void ROCCalorimeterInterface::SetBoardVoltages(bool        hvonoff,
                                               int         boardID,
                                               std::string conf)
{
	std::string command = "HVONOFF";
	float       paramVect[9];
	paramVect[0] = hvonoff;
	paramVect[1] = NAN;
	paramVect[2] = NAN;
	paramVect[3] = NAN;
	paramVect[4] = NAN;
	paramVect[5] = NAN;
	paramVect[6] = NAN;
	paramVect[7] = NAN;
	paramVect[8] = NAN;

	SendMzCommand(command, paramVect);

	usleep(100000);

	command      = "ADCFG";
	paramVect[0] = 1;
	paramVect[1] = 0;
	paramVect[2] = NAN;
	paramVect[3] = NAN;
	paramVect[4] = NAN;
	paramVect[5] = NAN;
	paramVect[6] = NAN;
	paramVect[7] = NAN;
	paramVect[8] = NAN;

	SendMzCommand(command, paramVect);

	usleep(100000);

	command      = "SLEWRATE";
	paramVect[0] = 40;
	paramVect[1] = NAN;
	paramVect[2] = NAN;
	paramVect[3] = NAN;
	paramVect[4] = NAN;
	paramVect[5] = NAN;
	paramVect[6] = NAN;
	paramVect[7] = NAN;
	paramVect[8] = NAN;

	SendMzCommand(command, paramVect);

	usleep(100000);

	if(hvonoff == 1)
	{
		char buff[50];
		sprintf(buff, "board%03d.config", boardID);

		cet::filepath_lookup lookup_policy("MU2E_CALORIMETER_CONFIG_PATH");
		auto                 filename = lookup_policy(conf + "/" + buff);

		std::ifstream confFile(filename);
		if(!confFile.is_open())
		{
			__FE_SS__ << "Could not open file: " << filename << __E__;
			__FE_SS_THROW__;
			;
		}

		__COUT_INFO__ << "Opening file: " << filename << __E__;

		float vbias[20];

		for(int ichan = 0; ichan < 20; ichan++)
		{
			int         chindex;
			std::string type;

			confFile >> chindex >> vbias[ichan] >> type;
			__COUT_INFO__ << chindex << "  " << vbias[ichan] << "  " << type << __E__;
		}

		/*for(int ichan = 0; ichan<20; ichan++){

		  int chindex;
		  float vbias;
		  std::string type;

		  confFile >> chindex >> vbias >> type;

		  __COUT_INFO__ << chindex << "  " <<  vbias  <<  "  " << type << __E__;

		  command = "DACSET";
		  paramVect[0] = chindex+1;
		  paramVect[1] = vbias;
		  paramVect[2] = NAN;
		  paramVect[3] = NAN;
		  paramVect[4] = NAN;
		  paramVect[5] = NAN;
		  paramVect[6] = NAN;
		  paramVect[7] = NAN;
		  paramVect[8] = NAN;

		  usleep(500000);

		  SendMzCommand(command, paramVect);


		  RMZB_writeSiPMbias(chindex+1, vbias);
		  usleep(100000);


		  }*/

		RMZB_writeAllSiPMbias(vbias);

		__COUT_INFO__ << "Ramping up.. " << filename << __E__;

		confFile.close();

		__COUT_INFO__ << "Configuration done.." << filename << __E__;
	}

}  // end SetBoardVoltages()

//==================================================================================================

//==================================================================================================

void ROCCalorimeterInterface::EnableAndPowerSiPMs(bool hvonoff, float vbias)
{
	std::string command = "HVONOFF";  // the dalays have a random value
	float       paramVect[9];
	paramVect[0] = hvonoff;
	paramVect[1] = NAN;
	paramVect[2] = NAN;
	paramVect[3] = NAN;
	paramVect[4] = NAN;
	paramVect[5] = NAN;
	paramVect[6] = NAN;
	paramVect[7] = NAN;
	paramVect[8] = NAN;

	SendMzCommand(command, paramVect);

	usleep(100000);

	command      = "ADCFG";
	paramVect[0] = 1;
	paramVect[1] = 0;
	paramVect[2] = NAN;
	paramVect[3] = NAN;
	paramVect[4] = NAN;
	paramVect[5] = NAN;
	paramVect[6] = NAN;
	paramVect[7] = NAN;
	paramVect[8] = NAN;

	usleep(100000);

	SendMzCommand(command, paramVect);

	command      = "DACSET";
	paramVect[0] = 0;
	paramVect[1] = vbias;
	paramVect[2] = NAN;
	paramVect[3] = NAN;
	paramVect[4] = NAN;
	paramVect[5] = NAN;
	paramVect[6] = NAN;
	paramVect[7] = NAN;
	paramVect[8] = NAN;

	usleep(100000);

	SendMzCommand(command, paramVect);

}  // end EnableAndPowerSiPMs()

//==================================================================================================

//==================================================================================================
void ROCCalorimeterInterface::ReadROCErrorCounter(__ARGS__)
{
	__COUT_INFO__ << "ReadROCErrorCounter()" << __E__;

	unsigned int errAddr = __GET_ARG_IN__("Address to read, Default := 0]", uint16_t, 0);
	__FE_COUTV__(errAddr);

	writeRegister(ROC_ADDRESS_ERRCNT, errAddr);
	writeRegister(ROC_ADDRESS_IS_PATTERN, 64);

	std::stringstream  os;
	DTCLib::roc_data_t readVal;
	readVal = readRegister(ROC_ADDRESS_ERRCNT);

	os << std::hex << std::setprecision(4) << std::setfill('0') << "address 0x" << errAddr
	   << " (" << std::dec << errAddr << std::hex << "): data 0x" << readVal << " ("
	   << std::dec << readVal << ")\n"
	   << __E__;

	writeRegister(ROC_ADDRESS_IS_PATTERN, 0);

	__COUT_INFO__ << "end ReadROCErrorCounter()" << __E__;

	__SET_ARG_OUT__("Status", os.str());

}  // end ReadROCErrorCounter()

//==================================================================================================

void ROCCalorimeterInterface::SetupForADCsDataTaking(__ARGS__)
{
	bool         setThr = __GET_ARG_IN__("Set Threshold? [bool, Default := 0]", bool, 0);
	unsigned int threshold =
	    __GET_ARG_IN__("Threshold [units of adccounts, Default := 2300]", uint32_t, 2300);

	SetupForADCsDataTaking(setThr, threshold);

}  // end SetupForADCsDataTaking()

//==================================================================================================

//==================================================================================================

void ROCCalorimeterInterface::SetupForNoiseTaking(__ARGS__)
{
	unsigned int numberOfsamples =
	    __GET_ARG_IN__("Number of noise samples per evt [Default := 20]", uint32_t, 20);

	SetupForNoiseTaking(numberOfsamples);

}  // end SetupForNoiseTaking()

//==================================================================================================

//==================================================================================================

void ROCCalorimeterInterface::SetupForNoiseTaking(unsigned int numberOfsamples)
{
	__COUT_INFO__ << "SetupForNoiseTaking()" << __E__;

	writeRegister(ROC_ADDRESS_MASK_A, 1023);
	writeRegister(ROC_ADDRESS_MASK_B, 1023);

	writeRegister(ROC_ADDRESS_IS_PATTERN, 0);
	writeRegister(ROC_ADDRESS_IS_COUNTER, 0);
	writeRegister(ROC_ADDRESS_IS_LASER, 0);

	writeRegister(ROC_ADDRESS_EW_DELAY, 0);
	writeRegister(ROC_ADDRESS_EW_BLIND, 0);
	writeRegister(ROC_ADDRESS_EW_LENGHT, numberOfsamples);

	writeRegister(ROC_ADDRESS_OSCMODE_FLAG, 1);
	writeRegister(ROC_ADDRESS_OSCMODE_LENGHT, numberOfsamples);

	writeRegister(ROC_ADDRESS_DDRRESET, 1);
	writeRegister(ROC_ADDRESS_DDRRESET, 0);
	writeRegister(ROC_ADDRESS_ANALOGRESET, 1);
	writeRegister(ROC_ADDRESS_ANALOGRESET, 0);

	__COUT_INFO__ << "end SetupForNoiseTaking()" << __E__;
}  // end SetupForNoiseTaking()

//==================================================================================================

void ROCCalorimeterInterface::SetupForADCsDataTaking(bool setThr, unsigned int threshold)
{
	__COUT_INFO__ << "SetupForADCsDataTaking()" << __E__;

	__FE_COUTV__(threshold);

	/*std::string filename = std::string(__ENV__("USER_DATA")) + "/roc_thr.csv";
	  std::ifstream myFile(filename);

	  // Create a vector of <string, int vector> pairs to store the result
	  std::vector<std::pair<std::string, std::vector<int>>> result;

	  if(!myFile.is_open())
	  {
	  __FE_SS__ << "Could not open file: " << filename << __E__;
	  __FE_SS_THROW__;;
	  }

	  // Read myFile
	  std::vector<std::vector<std::string>> csvRows;

	  for (std::string line; std::getline(myFile, line);) {

	  std::istringstream ss(std::move(line));
	  std::vector<std::string> row;

	  if (!csvRows.empty())
	  {
	  // We expect each row to be as big as the first row
	  row.reserve(csvRows.front().size());
	  }

	  // std::getline can split on other characters, here we use ','
	  for (std::string value; std::getline(ss, value, ',');)
	  {
	  row.push_back(std::move(value));
	  }

	  csvRows.push_back(std::move(row));
	  }

	  // Close file
	  myFile.close();*/

	writeRegister(ROC_ADDRESS_MASK_A, 1023);
	writeRegister(ROC_ADDRESS_MASK_B, 1023);

	writeRegister(ROC_ADDRESS_IS_PATTERN, 0);
	writeRegister(ROC_ADDRESS_IS_COUNTER, 0);
	writeRegister(ROC_ADDRESS_IS_LASER, 0);
	writeRegister(ROC_ADDRESS_OSCMODE_FLAG, 0);

	writeRegister(ROC_ADDRESS_EW_DELAY, 0);
	writeRegister(ROC_ADDRESS_EW_BLIND, 0);
	writeRegister(ROC_ADDRESS_EW_LENGHT, 19500);

	writeRegister(ROC_ADDRESS_DDRRESET, 1);
	writeRegister(ROC_ADDRESS_DDRRESET, 0);
	writeRegister(ROC_ADDRESS_ANALOGRESET, 1);
	writeRegister(ROC_ADDRESS_ANALOGRESET, 0);

	// Write Roc thrsholds using
	if(setThr)
	{
		for(int ich = 0; ich < 20; ich++)
		{
			// writeRegister(ROC_ADDRESS_BASE_THRESHOLD + ich,
			// std::stoi(csvRows[0][ich+1]));
			writeRegister(ROC_ADDRESS_BASE_THRESHOLD + ich, threshold);
		}
	}

	/*for (const std::vector<std::string>& row : csvRows)
	  {
	  for (const std::string& value : row)
	  {
	  __COUT_INFO__ << std::setw(10) << value;
	  }

	  __COUT_INFO__ << "\n";
	  }*/

	__COUT_INFO__ << "end SetupForADCsDataTaking()" << __E__;
}  // end SetupForADCsDataTaking()

//==================================================================================================

void ROCCalorimeterInterface::ToggleMBBusy(__ARGS__)
{
	unsigned int busyonoff =
	    __GET_ARG_IN__("On/Off [Busy On/Off, Default := 1]", bool, 1);
	ToggleMBBusy(busyonoff);

}  // end ToggleMBBusy()

void ROCCalorimeterInterface::ToggleMBBusy(bool busyonoff)
{
	writeRegister(ROC_ADDRESS_MZB_BUSY, busyonoff);

}  // end ToggleMBBusy()

//==================================================================================================

//==================================================================================================
void ROCCalorimeterInterface::GetStatus(__ARGS__)
{
	// copied from Monica's va_read_all.sh

	DTCLib::roc_data_t readVal;

	std::stringstream     os;
	DTCLib::roc_address_t address;

	address = 0x0;
	readVal = readRegister(address);
	os << std::hex << std::setprecision(4) << std::setfill('0') << "address 0x" << address
	   << " (" << std::dec << address << std::hex << "): data 0x" << readVal << " ("
	   << std::dec << readVal << ")\n"
	   << __E__;

	address = 0x8;
	readVal = readRegister(address);
	os << std::hex << std::setprecision(4) << std::setfill('0') << "address 0x" << address
	   << " (" << std::dec << address << std::hex << "): data 0x" << readVal << " ("
	   << std::dec << readVal << ")" << __E__;
	os << "\t\t"
	   << "bit[9:8]=[enable_marker,enable_clock]"
	      "\n\t\t bit[7:4]=[en_int_ewm,en_free_ewm,error_en,pattern_en]"
	      "\n\t\t bit[3:0]=en_lanes[HV1,HV0,CAl1,CAL0]\n"
	   << __E__;

	address = 18;
	readVal = readRegister(address);
	os << std::hex << std::setprecision(4) << std::setfill('0') << "address 0x" << address
	   << " (" << std::dec << address << std::hex << "): data 0x" << readVal << " ("
	   << std::dec << readVal << ")" << __E__;
	os << "\t\t"
	   << "bit[9:8]=[enable_marker,enable_clock]"
	      "\n\t\t bit[7:4]=[en_int_ewm,en_free_ewm,error_en,pattern_en]"
	      "\n\t\t bit[3:0]=en_lanes[HV1,HV0,CAl1,CAL0]\n"
	   << __E__;

	address = 72;
	readVal = readRegister(address);
	os << std::hex << std::setprecision(4) << std::setfill('0') << "address 0x" << address
	   << " (" << std::dec << address << std::hex << "): hbtag error 0x" << readVal
	   << " (" << std::dec << readVal << ") \n"
	   << __E__;

	address = 73;
	readVal = readRegister(address);
	os << std::hex << std::setprecision(4) << std::setfill('0') << "address 0x" << address
	   << " (" << std::dec << address << std::hex << "): dreq error 0x" << readVal << " ("
	   << std::dec << readVal << ") \n"
	   << __E__;

	address = 74;
	readVal = readRegister(address);
	os << std::hex << std::setprecision(4) << std::setfill('0') << "address 0x" << address
	   << " (" << std::dec << address << std::hex << "): hblost 0x" << readVal << " ("
	   << std::dec << readVal << ") \n"
	   << __E__;

	address = 75;
	readVal = readRegister(address);
	os << std::hex << std::setprecision(4) << std::setfill('0') << "address 0x" << address
	   << " (" << std::dec << address << std::hex << "): evm lost 0x" << readVal << " ("
	   << std::dec << readVal << ") \n"
	   << __E__;

	uint32_t doubleRegVal = 0;

	std::vector<DTCLib::roc_address_t> doubleReads = {
	    23, 25, 64, 27, 29, 31, 33, 35, 37, 39, 41, 43, 45, 48, 51, 54, 57};

	std::vector<std::string> doubleReadCaptions = {
	    "SIZE_FIFO_FULL[28]+STORE_POS[25:24]+STORE_CNT[19:0]",   // 23,
	    "SIZE_FIFO_EMPTY[28]+FETCH_POS[25:24]+FETCH_CNT[19:0]",  // 25,
	    "no. EVM seen",                                          // 64,
	    "no. HB seen",                                           // 27,
	    "no. null HB seen:",                                     // 29,
	    "no. HB on hold",                                        // 31,
	    "no. PREFETCH seen",                                     // 33,
	    "no. DATA REQ seen",                                     // 35,
	    "no. DATA REQ read from DDR",                            // 37,
	    "no. DATA REQ sent to DTC",                              // 39,
	    "no. DATA REQ with null data",                           // 41,
	    "last SPILL TAG",                                        // 43,
	    "last HB tag",                                           // 45,
	    "last PREFETCH tag",                                     // 48,
	    "last FETCHED tag",                                      // 51,
	    "last DATA REQ tag",                                     // 54,
	    "OFFSET tag",                                            // 57
	};

	for(size_t i = 0; i < doubleReads.size(); ++i)
	{
		address      = doubleReads[i];
		readVal      = readRegister(address);
		doubleRegVal = readVal;
		readVal      = readRegister(++address);
		doubleRegVal |= readVal << 16;

		os << std::hex << std::setprecision(4) << std::setfill('0') << "address 0x"
		   << address - 1 << " (" << std::dec << address - 1 << std::setprecision(8)
		   << std::hex << "): data 0x" << doubleRegVal << " (" << std::dec << doubleRegVal
		   << ")" << __E__;
		os << "\t\t" << doubleReadCaptions[i] << "\n" << __E__;
	}  // end double read register loop

	__SET_ARG_OUT__("Status", os.str());

}  // end GetStatus()

/**
 *  @brief   Write the the SiPM HV set command to the MEZZANINE slave
 *  @param   ch    FEE channel to set
 *  @param   hv    HV bias Voltage
 *  @return  0=SUCCESS else error code

 */

/*void ROCCalorimeterInterface::RMZB_writeSiPMbias(int ch, float hv) {

  struct __attribute__((packed, aligned(4))) {
  short dummy;
  short adr;
  struct __attribute__((packed)) {
  short biasVreq_tag;             // 'HV'
  short reserved;
  unsigned short biasVreq[FEE_NUM];
  } bias;
  unsigned chksum;
  } bm;

  hv = hv * 10. + .49999;

  memset(&bm, 0, sizeof(bm));
  bm.adr = offsetof(EE_DATABUF_t, biasVreq_tag);
  bm.bias.biasVreq_tag = 'H' | ('V'<<8);
  if(ch) {
  ch = ch-1;
  bm.bias.biasVreq[ch] = hv;
  bm.bias.biasVreq[ch] |= 0x8000;
  } else {
  for(;ch<FEE_NUM; ch++) {
  bm.bias.biasVreq[ch] = hv+ch;
  bm.bias.biasVreq[ch] |= 0x8000;
  }
  }
  bm.chksum   = 1 + (~ees_chksum((void *)&bm.bias, sizeof(bm.bias)));

  std::vector<uint16_t> input_data;

  char *p = (char *) &bm;
  p+=2;

  for (std::size_t i = 2; i < sizeof(bm); i += 2, p+=2) {

  uint16_t value = ((uint16_t) (*(p+1)) << 8) | ((uint16_t) (*p));
  //uint16_t value = (static_cast<uint16_t>(vectToWrite[i]) << 8) |
  (static_cast<uint16_t>(vectToWrite[i + 1]));
  __COUT_INFO__ << std::hex << std::setprecision(4) << std::setfill('0') << "0x" << value
  << __E__; input_data.push_back(value);
  }

  //  retval = MZB_writeRegisters((void*)&bm.adr, sizeof(bm)-2);

  //writeROCBlock(input_data, MZ_ADDRESS, false  incrementAddress);


  } */

void ROCCalorimeterInterface::RMZB_writeAllSiPMbias(float* hv)
{
	struct __attribute__((packed, aligned(4)))
	{
		short dummy;
		short adr;
		struct __attribute__((packed))
		{
			short          biasVreq_tag;  // 'HV'
			short          reserved;
			unsigned short biasVreq[FEE_NUM];
		} bias;
		unsigned chksum;
	} bm;

	std::vector<uint16_t> input_data;

	memset(&bm, 0, sizeof(bm));
	bm.adr = offsetof(EE_DATABUF_t, biasVreq_tag);
	input_data.push_back(bm.adr);

	bm.bias.biasVreq_tag = 'H' | ('V' << 8);
	input_data.push_back(bm.bias.biasVreq_tag);

	input_data.push_back(0x00);

	for(int ch = 0; ch < FEE_NUM; ch++)
	{
		float hvset          = hv[ch] * 10. + .49999;
		bm.bias.biasVreq[ch] = hvset;
		bm.bias.biasVreq[ch] |= 0x8000;
		input_data.push_back(bm.bias.biasVreq[ch]);
	}

	bm.chksum = 1 + (~ees_chksum((void*)&bm.bias, sizeof(bm.bias)));
	input_data.push_back((uint16_t)(bm.chksum >> 16));
	input_data.push_back((uint16_t)(bm.chksum & 0xFFFF));

	for(auto& value : input_data)
	{
		value = (value >> 8) | (value << 8);  // Scambia i due byte
	}

	writeROCBlock(input_data, MZ_ADDRESS, false /* incrementAddress*/);
}

DEFINE_OTS_INTERFACE(ROCCalorimeterInterface)
