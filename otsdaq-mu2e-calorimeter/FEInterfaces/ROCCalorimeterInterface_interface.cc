#include "otsdaq-mu2e-calorimeter/FEInterfaces/MZB.h"
#include "otsdaq-mu2e-calorimeter/FEInterfaces/ROCCalorimeterInterface.h"

#include "otsdaq/ConfigurationInterface/ConfigurationManager.h"
#include "otsdaq/ConfigurationInterface/ConfigurationManagerRW.h"
#include "otsdaq/Macros/InterfacePluginMacros.h"

#include "cetlib/filepath_maker.h"

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>

using namespace ots;

#undef __MF_SUBJECT__
#define __MF_SUBJECT__ "FE-ROCCalorimeterInterface"

// extern "C" {
//     #include "MU2E-API/API_I2C.h"
//     #include "MU2E-API/SBL_utils.h"
// }

// 259 (and others) ==> the number of words in block read is written first as a block
// write
const std::set<DTCLib::roc_address_t> ROCCalorimeterInterface::SPECIAL_BLOCK_READ_ADDRS_({263, 256, 257, 261, 262, 264, 260, 265, 266, 267});

//=========================================================================================
ROCCalorimeterInterface::ROCCalorimeterInterface(const std::string& rocUID, const ConfigurationTree& theXDAQContextConfigTree, const std::string& theConfigurationPath)
    : ROCPolarFireCoreInterface(rocUID, theXDAQContextConfigTree, theConfigurationPath) {
	INIT_MF("." /*directory used is USER_DATA/LOG/.*/);

	__CFG_COUT__ << "Constructor..." << __E__;

	__CFG_COUT_INFO__ << "ROCPolarFireCoreInterface instantiated with link: " << linkID_ << " and EventWindowDelayOffset = " << delay_ << __E__;

	try {
		inputTemp_ = getSelfNode().getNode("inputTemperature").getValue<double>();
	} catch(...) {
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

	registerFEMacroFunction("Setup for Fixed-length Pattern Data Taking",
	                        static_cast<FEVInterface::frontEndMacroFunction_t>(&ROCCalorimeterInterface::SetupForPatternFixedLengthDataTaking),
	                        std::vector<std::string>{"Fixed Length of Event [units of 16-bit words, Default := 8]"},  // inputs
	                                                                                                                  // parameters
	                        std::vector<std::string>{},                                                               // output parameters
	                        1);                                                                                       // requiredUserPermissions

	registerFEMacroFunction("Send Mz Command",
	                        static_cast<FEVInterface::frontEndMacroFunction_t>(&ROCCalorimeterInterface::SendMzCommand),
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

	registerFEMacroFunction("Enable and Power SiPMs",
	                        static_cast<FEVInterface::frontEndMacroFunction_t>(&ROCCalorimeterInterface::EnableAndPowerSiPMs),
	                        std::vector<std::string>{"HV Enabled, Default := 0]", "Bias voltage to set, Default := 0]"},  // inputs parameters
	                        std::vector<std::string>{},                                                                   // output parameters
	                        1);                                                                                           // requiredUserPermissions

	registerFEMacroFunction("Enable/Disable MZB Leds",
	                        static_cast<FEVInterface::frontEndMacroFunction_t>(&ROCCalorimeterInterface::EnableDisableLEDs),
	                        std::vector<std::string>{"Enable LEDs, Default := 0]"},
	                        std::vector<std::string>{},  // output parameters
	                        1);

	registerFEMacroFunction("Find BoardID From Serial",
	                        static_cast<FEVInterface::frontEndMacroFunction_t>(&ROCCalorimeterInterface::FindBoardIDFromSerial),
	                        std::vector<std::string>{},          // input
	                        std::vector<std::string>{"Status"},  // output
	                        1);

	registerFEMacroFunction("Set Board Voltages",
	                        static_cast<FEVInterface::frontEndMacroFunction_t>(&ROCCalorimeterInterface::SetBoardVoltages),
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

	registerFEMacroFunction("Configure Link",
	                        static_cast<FEVInterface::frontEndMacroFunction_t>(&ROCCalorimeterInterface::ConfigureLink),
	                        std::vector<std::string>{"Configuration folder, Default:= nominal",
	                                                 "HV Enabled, Default := 0]",
	                                                 "Upload MZB calibration parameters, Default := 0]",
	                                                 "Upload DiRAC thresholds, Default := 0]",
	                                                 "Offset, Default := 0]"},  // inputs parameters

	                        std::vector<std::string>{},  // output parameters
	                        1);                          // requiredUserPermissions

	registerFEMacroFunction("Calibrate Mzb",
	                        static_cast<FEVInterface::frontEndMacroFunction_t>(&ROCCalorimeterInterface::CalibrateMZB),
	                        std::vector<std::string>{},  // inputs parameters
	                        std::vector<std::string>{},  // output parameters
	                        1);                          // requiredUserPermissions

	registerFEMacroFunction("SetADCsThresholds",
	                        static_cast<FEVInterface::frontEndMacroFunction_t>(&ROCCalorimeterInterface::SetADCsThresholds),
	                        std::vector<std::string>{"Offset, Default := 0]"},  // inputs parameters
	                        std::vector<std::string>{},                         // output parameters
	                        1);                                                 // requiredUserPermissions

	registerFEMacroFunction("ReadVoltagesFromDB",
	                        static_cast<FEVInterface::frontEndMacroFunction_t>(&ROCCalorimeterInterface::ReadVoltagesFromDB),
	                        std::vector<std::string>{""},        // inputs parameters
	                        std::vector<std::string>{"Status"},  // output parameters
	                        1);                                  // requiredUserPermissions

	registerFEMacroFunction("ReadChannelStatusFromDB",
	                        static_cast<FEVInterface::frontEndMacroFunction_t>(&ROCCalorimeterInterface::ReadChannelStatusFromDB),
	                        std::vector<std::string>{""},        // inputs parameters
	                        std::vector<std::string>{"Status"},  // output parameters
	                        1);                                  // requiredUserPermissions

	registerFEMacroFunction("Print ROC Configuration",
	                        static_cast<FEVInterface::frontEndMacroFunction_t>(&ROCCalorimeterInterface::PrintROCConfiguration),
	                        std::vector<std::string>{},
	                        std::vector<std::string>{"Status"},
	                        1);

	registerFEMacroFunction("SetupForNoiseTaking",
	                        static_cast<FEVInterface::frontEndMacroFunction_t>(&ROCCalorimeterInterface::SetupForNoiseTaking),
	                        std::vector<std::string>{"Number of noise samples per evt [Default := 20]"},  // inputs parameters
	                        std::vector<std::string>{},                                                   // output parameters
	                        1);                                                                           // requiredUserPermissions

	registerFEMacroFunction("Read ROC Error Counter",
	                        static_cast<FEVInterface::frontEndMacroFunction_t>(&ROCCalorimeterInterface::ReadROCErrorCounter),
	                        std::vector<std::string>{"Address to read, Default := 0]"},  // inputs parameters
	                        std::vector<std::string>{"Status"},                          // output parameters
	                        1);                                                          // requiredUserPermissions

	registerFEMacroFunction(
	    "Setup for ADCs Data Taking",
	    static_cast<FEVInterface::frontEndMacroFunction_t>(&ROCCalorimeterInterface::SetupForADCsDataTaking),
	    std::vector<std::string>{"Set Threshold? [bool, Default := 0]", "Threshold [units of adccounts, Default := 2300]", "Is new Firmware? [bool, Default := 0]"},  // inputs parameters
	    std::vector<std::string>{},                                                                                                                                   // output parameters
	    1);                                                                                                                                                           // requiredUserPermissions

	registerFEMacroFunction("Toggle MB Busy for Noise",
	                        static_cast<FEVInterface::frontEndMacroFunction_t>(&ROCCalorimeterInterface::ToggleMBBusy),
	                        std::vector<std::string>{"On/Off [Busy On/Off, Default := 1]"},  // inputs parameters
	                        std::vector<std::string>{},                                      // output parameters
	                        1);                                                              // requiredUserPermissions

	registerFEMacroFunction("ROC Status",
	                        static_cast<FEVInterface::frontEndMacroFunction_t>(&ROCCalorimeterInterface::GetStatus),
	                        std::vector<std::string>{},          // inputs parameters
	                        std::vector<std::string>{"Status"},  // output parameters
	                        1);                                  // requiredUserPermissions

	registerFEMacroFunction("MZB Status",
	                        static_cast<FEVInterface::frontEndMacroFunction_t>(&ROCCalorimeterInterface::GetMZBStatus),
	                        std::vector<std::string>{},          // inputs parameters
	                        std::vector<std::string>{"Status"},  // output parameters
	                        1);                                  // requiredUserPermissions

	registerFEMacroFunction("ROC Slow Control",
	                        static_cast<FEVInterface::frontEndMacroFunction_t>(&ROCCalorimeterInterface::ROCSlowControl),
	                        std::vector<std::string>{},          // inputs parameters
	                        std::vector<std::string>{"Status"},  // output parameters
	                        1);                                  // requiredUserPermissions

	registerFEMacroFunction("TRAD Slow Control",
	                        static_cast<FEVInterface::frontEndMacroFunction_t>(&ROCCalorimeterInterface::TRADSlowControl),
	                        std::vector<std::string>{},          // inputs parameters
	                        std::vector<std::string>{"Status"},  // output parameters
	                        1);                                  // requiredUserPermissions

	registerFEMacroFunction("TRAD Set Mask",
	                        static_cast<FEVInterface::frontEndMacroFunction_t>(&ROCCalorimeterInterface::TRADSetMask),
	                        std::vector<std::string>{"Set Mask as series of bit (0 enabled, 1 disabled) Default := 0"},  // inputs parameters
	                        std::vector<std::string>{},                                                                  // output parameters
	                        1);                                                                                          // requiredUserPermissions

	registerFEMacroFunction("Read MB Registers",
	                        static_cast<FEVInterface::frontEndMacroFunction_t>(&ROCCalorimeterInterface::ReadMBRegisters),
	                        std::vector<std::string>{"Number of 16 bits words to read, Default := 20]", "Mezzanine Address, Default := 0]"},  // inputs parameters
	                        std::vector<std::string>{"Status"},                                                                               // output parameters
	                        1);                                                                                                               // requiredUserPermissions

	registerFEMacroFunction("Evaluate BlockWrite Error Rate",
	                        static_cast<FEVInterface::frontEndMacroFunction_t>(&ROCCalorimeterInterface::EvaluateBlockWriteErrorRate),
	                        std::vector<std::string>{"Number of test loops, Default := 900]",
	                                                 "Number of 16-bits words in a block transfer, Default "
	                                                 ":= 500]"},  // inputs parameters
	                        std::vector<std::string>{"Status"},   // output parameters
	                        1);                                   // requiredUserPermissions

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
	                        static_cast<FEVInterface::frontEndMacroFunction_t>(&ROCCalorimeterInterface::Configure),
	                        std::vector<std::string>{},  // inputs parameters
	                        std::vector<std::string>{},  // output parameters
	                        1);                          // requiredUserPermissions

	// // function for webgui
	// 	registerFEMacroFunction("GetROCCaloTemperatureChannel",
	// 	                        static_cast<FEVInterface::frontEndMacroFunction_t>(
	// 	                            &ROCCalorimeterInterface::GetTempChannel),
	// 	                        std::vector<std::string>{"channelNumber"}, //inputs
	// parameters 	                        std::vector<std::string>{"readValue"},
	// //output parameters 	                        1);  // requiredUserPermissions

	registerFEMacroFunction("Create Global ROC Table",
	                        static_cast<FEVInterface::frontEndMacroFunction_t>(&ROCCalorimeterInterface::CreateGlobalROCTable),
	                        std::vector<std::string>{},          // inputs parameters
	                        std::vector<std::string>{"Status"},  // output parameters
	                        1);                                  // requiredUserPermissions

}  // end constructor()

//==========================================================================================
ROCCalorimeterInterface::~ROCCalorimeterInterface(void) {
	// NOTE:: be careful not to call __FE_COUT__ decoration because it uses the
	// tree and it may already be destructed partially
	__COUT__ << FEVInterface::interfaceUID_ << "Destructed." << __E__;
}  // end destructor()

//==================================================================================================
void ROCCalorimeterInterface::writeEmulatorRegister(uint16_t address, uint16_t data_to_write) {
	__CFG_COUT__ << "emulator write" << __E__;

	return;

}  // end writeRegister()

//==================================================================================================
uint16_t ROCCalorimeterInterface::readEmulatorRegister(uint16_t address)  // not useful, broken by Luca
{
	__CFG_COUT__ << "emulator read" << __E__;

	if(address == 6 || address == 7)
		return ROCPolarFireCoreInterface::readEmulatorRegister(address);
	if(address == ROC_ADDRESS_EW_LENGHT)
		return 0x5;
	else if(address == ROC_ADDRESS_EW_BLIND) {
		temp1_.noiseTemp(temp1_.GetBoardTempC());
		return temp1_.GetBoardTempC() * 256;
	} else
		return 0xBAAD;

}  // end readRegister()

//==================================================================================================
// return false to stop workloop thread
bool ROCCalorimeterInterface::emulatorWorkLoop(void) {
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
void ROCCalorimeterInterface::universalBlockRead(char* address, char* returnValue, unsigned int numberOfBytes) {
	std::vector<DTCLib::roc_data_t> data;
	readROCBlock(data, *((DTCLib::roc_address_t*)address), numberOfBytes / 2, false /*incAddress*/);
	if(data.size() != numberOfBytes / 2) {
		__FE_SS__ << "Illegal number of bytes: " << data.size() << " not " << numberOfBytes / 2 << __E__;
		__FE_SS_THROW__;
	}
	memcpy(returnValue, &data[0], data.size() * 2);
}

//==================================================================================================
void ROCCalorimeterInterface::ROCSlowControl(__ARGS__) {
	std::stringstream os;

	const int boardID = static_cast<int>(boardConfig_.boardID);
	__COUT_INFO__ << "Target BoardID = " << boardID << __E__;

	if(boardConfig_.boardID == INVALID_BOARDID) {
		__FE_SS__ << "Skipping tag slow control for board " << boardID << ", boardID not initialized correctly!" << __E__;
		__FE_SS_THROW__;
		return;
	}

	if(boardID > MAX_BOARD_ID) {
		__FE_SS__ << "Skipping upload MZB parameters to board " << boardID << ", boardID out of range!" << __E__;
		__FE_SS_THROW__;
		return;
	}

	const char* env_path = std::getenv("MU2E_CALORIMETER_CONFIG_PATH");
	if(!env_path) {
		__FE_SS__ << "MU2E_CALORIMETER_CONFIG_PATH not defined";
		__FE_SS_THROW__;
		;
	}

	std::string full_path(env_path);
	std::string first_path = full_path.substr(0, full_path.find(':'));

	namespace fs              = std::filesystem;
	fs::path slow_control_dir = fs::path(first_path) / "slowControl";

	std::error_code ec;
	if(!fs::exists(slow_control_dir)) {
		if(!fs::create_directories(slow_control_dir, ec)) {
			__FE_SS__ << "Error in directory creation: " << ec.message();
			__FE_SS_THROW__;
			;
		}
	}

	char filename_buff[50], filename_buff_vi[50];
	std::snprintf(filename_buff, sizeof(filename_buff), "slowControl%03d.log", boardID);
	std::snprintf(filename_buff_vi, sizeof(filename_buff_vi), "slowControlVI%03d.log", boardID);
	fs::path      output_file    = slow_control_dir / filename_buff;
	fs::path      output_file_VI = slow_control_dir / filename_buff_vi;
	std::ofstream file(output_file, std::ios::app);
	std::ofstream fileVI(output_file_VI, std::ios::app);

	if(!file.is_open() || !fileVI.is_open()) {
		__FE_SS__ << "Error in opening slow control file!";
		__FE_SS_THROW__;
		;
	}

	auto        now   = std::chrono::system_clock::now();
	std::time_t now_c = std::chrono::system_clock::to_time_t(now);

	std::string timestamp = std::ctime(&now_c);
	timestamp.pop_back();
	file << "[" << timestamp << "] " << boardID << " ";
	fileVI << "[" << timestamp << "] " << boardID << " ";

	os << __E__;

	os << "Board unique ID:" << __E__;
	os << __E__;

	os << "Reg 145: 0x" << std::hex << thisDTC_->ReadROCRegister(linkID_, 145, 100) << __E__;
	os << "Reg 146: 0x" << std::hex << thisDTC_->ReadROCRegister(linkID_, 146, 100) << __E__;
	os << "Reg 147: 0x" << std::hex << thisDTC_->ReadROCRegister(linkID_, 147, 100) << __E__;

	std::vector<DTCLib::roc_data_t> data;
	readROCBlock(data, 263, 12, false /*incAddress*/);
	if(data.size() != 12) {
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
	os << "Word " << 6 << ":  TEMP0: " << (((data[6] * 0.806) - 509) / 6.45) << " C" << __E__;
	os << "Word " << 7 << ":  TEMP1: " << (((data[7] * 0.806) - 509) / 6.45) << " C" << __E__;
	os << "Word " << 8 << ":  2.5 voltage: " << (data[8] * 0.806) << " mV" << __E__;
	os << "Word " << 9 << ":  2.5D voltage: " << (data[9] * 0.806) << " mV" << __E__;
	os << "Word " << 10 << ": 5 voltage: " << (data[10] * 1.612) << " mV" << __E__;
	os << "Word " << 11 << ": 28 current: " << (data[11] * 2.687) << " mA" << __E__;

	os << __E__;
	os << "Reading SiPMs temperature:" << __E__;
	os << __E__;

	std::vector<DTCLib::roc_data_t> data3;
	readROCBlock(data3, 260, 7, false);

	os << "vector size:" << data3.size() << __E__;

	os << __E__;

	for(size_t i = 0; i < data3.size(); i++) {
		os << "Word " << i << ": 0x" << ((float)data3[i]) / 100. << __E__;
		file << ((float)data3[i]) / 100. << " ";
	}

	os << __E__;
	os << "Reading SiPMs current:" << __E__;
	os << __E__;

	std::vector<DTCLib::roc_data_t> data4;
	readROCBlock(data4, 266, 7, false);

	os << "vector size:" << data4.size() << __E__;

	os << __E__;

	for(size_t i = 0; i < data4.size(); i++) {
		os << "Word " << i << ": 0x" << ((float)data4[i]) / 10. << __E__;
		fileVI << ((float)data4[i]) / 10. << " ";
	}

	os << __E__;
	os << "Reading SiPMs voltages:" << __E__;
	os << __E__;

	std::vector<DTCLib::roc_data_t> data5;
	readROCBlock(data5, 265, 7, false);

	os << "vector size:" << data5.size() << __E__;

	os << __E__;

	for(size_t i = 0; i < data5.size(); i++) {
		os << "Word " << i << ": 0x" << ((float)data5[i]) / 100. << __E__;
		fileVI << ((float)data5[i]) / 100. << " ";
	}

	os << __E__;
	os << "Reading SiPMs requested voltages:" << __E__;
	os << __E__;

	std::vector<DTCLib::roc_data_t> data6;
	readROCBlock(data6, 267, FEE_NUM, false);

	os << "vector size:" << data6.size() << __E__;

	os << __E__;

	for(size_t i = 0; i < data6.size(); i++) {
		const auto raw     = data6[i];
		const bool enabled = (raw & 0x8000) != 0;
		const auto value   = raw & 0x7fff;
		os << "Word " << i << ": raw=0x" << std::hex << raw << std::dec
		   << " enabled=" << enabled << " request=" << ((float)value) / 10. << " V"
		   << __E__;
	}

	file << std::endl;
	fileVI << std::endl;

	// Chiudi il file
	file.close();
	fileVI.close();

	/*if(data3.size() != 7)	{
	  __FE_SS__ << "Illegal number of bytes: "  <<  data3.size() << " not " << 7 << __E__;
	  __FE_SS_THROW__;
	}

	os << "Word " << 0 << ":  VTRX_REG_Control: 0x" << std::hex << data3[0] << " " <<
	__E__; os << "Word " << 1 << ":  VTRX_REG_ModCurrent: 0x" << std::hex << data3[1]<< "
	" << __E__; os << "Word " << 2 << ":  VTRX_REG_BiasCurrent: 0x" << std::hex <<
	data3[2] << " " << __E__; os << "Word " << 3 << ":  VTRX_REG_PreEmphasis: 0x" <<
	std::hex  << data3[3] << " " << __E__; os << "Word " << 4 << ":  VTRX_REG_ModMask: 0x"
	<< std::hex << data3[4]<< " " << __E__; os << "Word " << 5 << ":  VTRX_REG_BiasMask:
	0x" << std::hex << data3[5] << " " << __E__; os << "Word " << 6 << ":
	VTRX_REG_PreDriver: 0x" << std::hex << data3[6]   << " " << __E__;
	*/

	os << __E__;

	os << "Reading MB registers:" << __E__;
	os << __E__;

	std::vector<DTCLib::roc_data_t> data2;
	readROCBlock(data2, 261, 20, false /*incAddress*/);

	if(data2.size() != 20) {
		__FE_SS__ << "Illegal number of bytes: " << data2.size() << " not " << 20 << "  !" << __E__;
		__FE_SS_THROW__;
	}

	for(size_t i = 0; i < data2.size(); i++) {
		os << "Word " << i << ": 0x" << std::hex << data2[i] << __E__;
	}

	__SET_ARG_OUT__("Status", os.str());
}

//==================================================================================================
void ROCCalorimeterInterface::TRADSlowControl(__ARGS__) {
	std::stringstream os;

	std::vector<DTCLib::roc_data_t> data;
	readROCBlock(data, 264, 27, false /*incAddress*/);
	if(data.size() != 27) {
		__FE_SS__ << "Illegal number of bytes: " << data.size() << " not " << 12 << __E__;
		__FE_SS_THROW__;
	}

	os << std::dec << __E__;

	os << "Slow control values:" << __E__;

	os << __E__;

	os << "Word " << 0 << ":  Sensor 0 Radfet: " << data[0] << " " << __E__;
	os << "Word " << 1 << ":  Sensor 0 Sipm: " << data[1] << " " << __E__;
	os << "Word " << 2 << ":  Sensor 0 Temperature: " << data[2] / 100.0 << " C " << __E__;

	os << "Word " << 3 << ":  Sensor 1 Radfet: " << data[3] << " " << __E__;
	os << "Word " << 4 << ":  Sensor 1 Sipm: " << data[4] << " " << __E__;
	os << "Word " << 5 << ":  Sensor 1 Temperature: " << data[5] / 100.0 << " C " << __E__;

	os << "Word " << 6 << ":  Sensor 2 Radfet: " << data[6] << " " << __E__;
	os << "Word " << 7 << ":  Sensor 2 Sipm: " << data[7] << " " << __E__;
	os << "Word " << 8 << ":  Sensor 2 Temperature: " << data[8] / 100.0 << " C " << __E__;

	os << "Word " << 9 << ":  Sensor 3 Radfet: " << data[9] << " " << __E__;
	os << "Word " << 10 << ":  Sensor 3 Sipm: " << data[10] << " " << __E__;
	os << "Word " << 11 << ":  Sensor 3 Temperature: " << data[11] / 100.0 << " C " << __E__;

	os << "Word " << 12 << ":  Sensor 4 Radfet: " << data[12] << " " << __E__;
	os << "Word " << 13 << ":  Sensor 4 Sipm: " << data[13] << " " << __E__;
	os << "Word " << 14 << ":  Sensor 4 Temperature: " << data[14] / 100.0 << " C " << __E__;

	os << "Word " << 15 << ":  Sensor 5 Radfet: " << data[15] << " " << __E__;
	os << "Word " << 16 << ":  Sensor 5 Sipm: " << data[16] << " " << __E__;
	os << "Word " << 17 << ":  Sensor 5 Temperature: " << data[17] / 100.0 << " C " << __E__;

	os << "Word " << 18 << ":  Sensor 6 Radfet: " << data[18] << " " << __E__;
	os << "Word " << 19 << ":  Sensor 6 Sipm: " << data[19] << " " << __E__;
	os << "Word " << 20 << ":  Sensor 6 Temperature: " << data[20] / 100.0 << " C " << __E__;

	os << "Word " << 21 << ":  Sensor 7 Radfet: " << data[21] << " " << __E__;
	os << "Word " << 22 << ":  Sensor 7 Sipm: " << data[22] << " " << __E__;
	os << "Word " << 23 << ":  Sensor 7 Temperature: " << data[23] / 100.0 << " C " << __E__;

	os << "Word " << 24 << ":  Sensor 8 Radfet: " << data[24] << " " << __E__;
	os << "Word " << 25 << ":  Sensor 8 Sipm: " << data[25] << " " << __E__;
	os << "Word " << 26 << ":  Sensor 8 Temperature: " << data[26] / 100.0 << " C " << __E__;

	__SET_ARG_OUT__("Status", os.str());
}

//==================================================================================================
void ROCCalorimeterInterface::TRADSetMask(__ARGS__) {
	unsigned int TRADMask = __GET_ARG_IN__("Set Mask as series of bit (0 enabled, 1 disabled) [Default:=0]", unsigned int, 0);
	TRADSetMask(TRADMask);
}

void ROCCalorimeterInterface::TRADSetMask(unsigned int TRADMask) {
	std::vector<DTCLib::roc_data_t> TRADout;
	TRADout.push_back(TRADMask);

	writeROCBlock(TRADout, 270, false /* incrementAddress*/);
}

//==================================================================================================
void ROCCalorimeterInterface::ReadMBRegisters(__ARGS__) {
	unsigned int nwords = __GET_ARG_IN__("Number of 16 bits words to read, Default := 20]", unsigned int, 20);
	int          offset = __GET_ARG_IN__("Mezzanine Address, Default := 0]", int, 0);

	std::vector<DTCLib::roc_data_t> data;
	readROCBlock(data, 261, nwords, false /*incAddress*/);

	if(data.size() != nwords) {
		__FE_SS__ << "Illegal number of bytes: " << data.size() << " not " << nwords << "  !" << offset << __E__;
		__FE_SS_THROW__;
	}

	std::stringstream os;

	for(size_t i = 0; i < data.size(); i++) {
		os << "Word " << i << ": 0x" << std::hex << data[i] << __E__;
	}

	__SET_ARG_OUT__("Status", os.str());
}

//==================================================================================================
void ROCCalorimeterInterface::EvaluateBlockWriteErrorRate(__ARGS__) {
	uint16_t nloops   = __GET_ARG_IN__("Number of test loops, Default := 900]", unsigned int, 900);
	uint16_t blockLen = __GET_ARG_IN__("Number of 16-bits words in a block transfer, Default := 500]", unsigned int, 500);

	std::stringstream os;

	os << "Starting evaluation of ROC Block Write Bit Error Rate (BER) " << __E__;

	uint16_t u;
	u = thisDTC_->ReadROCRegister(linkID_, 0, 100);

	uint16_t current_counter = 0;
	int      nerrs           = 0;

	long int time_first_loop        = 0;
	long int time_second_loop       = 0;
	long int current_first_loop     = 0;
	long int current_second_loop    = 0;
	long int first_time_first_loop  = 0;
	long int first_time_second_loop = 0;
	long int max_time_first_loop    = 0;
	long int max_time_second_loop   = 0;

	for(uint16_t iloop = 0; iloop < nloops; iloop++) {
		std::vector<DTCLib::roc_data_t> dataout;
		std::vector<DTCLib::roc_data_t> datain;

		for(uint16_t ic = 0; ic < blockLen; ic++) {
			dataout.push_back(current_counter);
			current_counter++;
		}

		writeROCBlock(dataout, 259, false /* incrementAddress*/);

		// should here read the CRC errors from error block Error addr=9

		while((u = thisDTC_->ReadROCRegister(linkID_, 128, 100)) != 0x8000) {
			current_first_loop++;
			time_first_loop++;
			if(u != 0) {
				os << "new Max time for first loop, loop " << iloop << " is " << current_first_loop << __E__;
				nerrs++;
				writeRegister(14, 1);
				writeRegister(14, 0);
				continue;
			}
		}
		while((u = thisDTC_->ReadROCRegister(linkID_, 129, 100)) == 0) {
			current_second_loop++;
			time_second_loop++;
		}

		if(current_first_loop > max_time_first_loop) {
			max_time_first_loop = current_first_loop;
			os << "new Max time for first loop, loop " << iloop << " is " << current_first_loop << __E__;
		}
		if(current_second_loop > max_time_second_loop) {
			max_time_second_loop = current_second_loop;
			os << "new Max time for second loop, loop " << iloop << " is " << current_second_loop << __E__;
		}

		if(u - 4 != blockLen) {
			os << "Error detected in transfer " << iloop << ", nWORDs read is " << u - 4 << __E__;
			writeRegister(14, 1);
			writeRegister(14, 0);
			continue;
		}

		thisDTC_->ReadROCBlock(datain, linkID_, 259, u - 4, 0, 0);
		while(datain.size() > blockLen)
			datain.pop_back();  // maybe not needed anymnore

		uint16_t icomp = 0;
		while(icomp < dataout.size()) {
			if(dataout[icomp] != datain[icomp]) {
				os << "Error detected in transfer " << iloop << ", word " << icomp << __E__;
				nerrs++;
			}
			// else if(icomp == 0) os << iloop << " loop, first counter is " <<
			// datain[icomp]  <<   __E__;
			icomp++;
		}

		writeRegister(14, 1);
		writeRegister(14, 0);

		if(iloop == 0) {
			first_time_first_loop  = time_first_loop;
			first_time_second_loop = time_second_loop;
		}

		current_first_loop  = 0;
		current_second_loop = 0;
	}

	time_first_loop  = time_first_loop / nloops;
	time_second_loop = time_second_loop / nloops;

	os << "Test completed!" << __E__;

	os << "Word error rate: " << nerrs / current_counter << " over " << current_counter << " words" << __E__;

	os << "First loop time is " << first_time_first_loop << ", second is " << first_time_second_loop << __E__;
	os << "Average First loop time is " << time_first_loop << ", second is " << time_second_loop << __E__;

	__SET_ARG_OUT__("Status", os.str());
}

//==================================================================================================
void ROCCalorimeterInterface::readROCBlock(std::vector<DTCLib::roc_data_t>& data, DTCLib::roc_address_t address, uint16_t wordCount, bool incrementAddress) {
	__FE_COUT__ << "Calling read ROC block: link number " << std::dec << linkID_ << ", address = " << address << ", wordCount = " << wordCount << ", incrementAddress = " << incrementAddress << __E__;

	if(ROCCalorimeterInterface::SPECIAL_BLOCK_READ_ADDRS_.find(address) == ROCCalorimeterInterface::SPECIAL_BLOCK_READ_ADDRS_.end())
		return ROCCoreVInterface::readROCBlock(data, address, wordCount, incrementAddress);

	uint16_t u;
	uint16_t readCount = 0;

	// check if special Block Write required
	if(ROCCalorimeterInterface::SPECIAL_BLOCK_READ_ADDRS_.find(address) != ROCCalorimeterInterface::SPECIAL_BLOCK_READ_ADDRS_.end()) {
		__FE_COUT__ << "Doing special block write!" << __E__;

		switch(address) {
		case 261:
			// writeROCBlock({wordCount*2, 0}, address, false /* incrementAddress*/);
			writeROCBlock({static_cast<unsigned short>(wordCount * 2), 0}, address, false /* incrementAddress*/);
			break;

		case 260:
			address   = offsetof(EE_DATABUF_t, apdTemp_tag) + 4;
			wordCount = offsetof(EE_DATABUF_t, apdPwrs_tag) - offsetof(EE_DATABUF_t, apdTemp_tag) - 8;
			writeROCBlock({wordCount, address}, 261, false /* incrementAddress*/);
			wordCount = wordCount / 2;
			address   = 261;
			break;

		case 265:
			address   = offsetof(EE_DATABUF_t, apdBiasV_tag) + 4;
			wordCount = offsetof(EE_DATABUF_t, apdBiasA_tag) - offsetof(EE_DATABUF_t, apdBiasV_tag) - 8;
			writeROCBlock({wordCount, address}, 261, false /* incrementAddress*/);
			wordCount = wordCount / 2;
			address   = 261;
			break;

		case 266:
			address   = offsetof(EE_DATABUF_t, apdBiasA_tag) + 4;
			wordCount = offsetof(EE_DATABUF_t, apdTemp_tag) - offsetof(EE_DATABUF_t, apdBiasA_tag) - 8;
			writeROCBlock({wordCount, address}, 261, false /* incrementAddress*/);
			wordCount = wordCount / 2;
			address   = 261;
			break;

		case 267:
			address   = offsetof(EE_DATABUF_t, biasVreq_tag) + 4;
			wordCount = offsetof(EE_DATABUF_t, functionReq_tag) -
			            offsetof(EE_DATABUF_t, biasVreq_tag) - 8;
			writeROCBlock({wordCount, address}, 261, false /* incrementAddress*/);
			wordCount = wordCount / 2;
			address   = 261;
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

		// Phase 1: wait for DONE to clear (command accepted by processor)
		uint16_t j = 0;
		while(((u = thisDTC_->ReadROCRegister(linkID_, 128, 100)) & 0x8000) != 0) {
			usleep(100);
			j++;
			if(j == 100) {
				__FE_SS__ << "ROC block read: DONE stuck high (reg128=0x" << std::hex << u 
				          << "), command not accepted" << __E__;
				__FE_SS_THROW__;
			}
		}

		// Phase 2: wait for DONE to set (command completed)
		j = 0;
		while(((u = thisDTC_->ReadROCRegister(linkID_, 128, 100)) & 0x8000) == 0) {
			usleep(100);
			j++;
			if(j == 100) {
				__FE_SS__ << "ROC block read: timeout waiting for DONE (reg128=0x" << std::hex << u << ")" << __E__;
				__FE_SS_THROW__;
			}
		}
		__COUT__ << "r_128: 0x" << std::hex << u << __E__;
		usleep(1000);

		j = 0;
		while(((u = thisDTC_->ReadROCRegister(linkID_, 129, 100)) & 0x07ff) == 0) {
			usleep(100);
			j++;
			if(j == 100) {
				__FE_SS__ << "ROC block failed at 129" << __E__;
				__FE_SS_THROW__;
			}
		}

		readCount = u & 0x07ff;

		__COUT__ << "r_129: 0x" << std::hex << u << __E__;

		if(readCount < 4) {
			__FE_SS__ << "ROC block read of address " << address << "(0x" << std::hex << address << std::dec
			          << ") returned invalid TX word count " << readCount << " from register 129 value 0x"
			          << std::hex << u << std::dec << __E__;
			__FE_SS_THROW__;
		}

		// wordCount = u - 4;  // number of words to read back
	}
	__FE_COUTV__(data.size());
	__FE_COUTV__(wordCount);
	__FE_COUTV__(readCount - 4);
	thisDTC_->ReadROCBlock(data, linkID_, address, readCount - 4, incrementAddress, 0);
	__FE_COUTV__(data.size());
	// only fix data if received more than needed - TODO fix in ROC firmware
	while(data.size() > wordCount)
		data.pop_back();

	if(emulatedInDTC_)  // fix count for emulated ROC to survive
		readCount = wordCount + 4;
	if(data.size() != (long unsigned int)readCount - 4) {
		__FE_SS__ << "ROC block read of address " << address << "(0x" << std::hex << address << std::dec << ") failed, expecting " << readCount - 4 << " words, and read " << data.size() << " words." << __E__;
		{
			__FE_COUT_ERR__ << ss.str();  // demoted to error rather than exception on 19-Feb-2026 during Calo MC2 commissioning
			// just pad with zeros for now if wrong
			while(data.size() < wordCount)
				data.push_back(0);
		}
		// __FE_SS_THROW__;
	}

}  // end readBlock()

//==================================================================================================

void ots::ROCCalorimeterInterface::FindBoardIDFromSerial(__ARGS__) {
	std::stringstream os;

	//   updateBoardIdFromSerial_();

	os << "identityValid          = " << (boardConfig_.identityValid ? "true" : "false") << "\n";
	os << "serial (reg 147)       = 0x" << std::hex << boardConfig_.serial << std::dec << "\n";
	os << "boardID (from DB)      = " << boardConfig_.boardID << "\n";

	const int boardID = static_cast<int>(boardConfig_.boardID);
	os << "Target BoardID = " << boardID << "\n";

	const ots::ConfigurationManager* cfgMgr = getConfigurationManager();

	if(boardID > MAX_BOARD_ID) {
		os << "Skipping setting thresholds to board " << boardID << ", boardID out of range!" << __E__;
		__SET_ARG_OUT__("Status", os.str());
		return;
	}

	auto rows = cfgMgr->getNode("SubsystemCalorimeterThresholdsTable").getChildren();
	os << "SubsystemCalorimeterThresholdsTable rows=" << rows.size() << "\n";

	const ots::ConfigurationTree* rec = nullptr;
	std::string                   recUID;

	for(const auto& row : rows) {
		const auto&    r            = row.second;
		const uint64_t tableBoardId = r.getNode("BoardID").getValue<uint64_t>();

		if(static_cast<int>(tableBoardId) != boardID)
			continue;

		rec    = &r;
		recUID = row.first;
		break;
	}

	if(!rec) {
		os << "ERROR: BoardID=" << boardID << " not found in SubsystemCalorimeterThresholdsTable\n";
		__SET_ARG_OUT__("Status", os.str());
		return;
	}

	os << "FOUND record UID=" << recUID << " for BoardID=" << boardID << "\n";

	auto thrBmp = rec->getNode("Thresholds").getValueAsBitMap();
	os << "Thresholds bitmap: rows=" << thrBmp.numberOfRows() << " cols(0)=" << thrBmp.numberOfColumns(0) << "\n";

	const uint32_t thrCols    = thrBmp.numberOfColumns(0);
	const uint32_t thrToPrint = std::min<uint32_t>(thrCols, 20);
	for(uint32_t ch = 0; ch < thrToPrint; ++ch) {
		std::string v = thrBmp.get(0, ch);
		if(v.empty())
			v = "0";
		os << "Thr[" << ch << "]=" << v << "\n";
	}

	__SET_ARG_OUT__("Status", os.str());
}

///////////////////////

void ROCCalorimeterInterface::updateBoardIdFromSerial_() {
	boardConfig_.identityValid = false;
	boardConfig_.serial        = 0;
	boardConfig_.boardID       = INVALID_BOARDID;

	boardConfig_.serial = readRegister(147);

	const ots::ConfigurationManager* cfgMgr = getConfigurationManager();

	auto rows = cfgMgr->getNode("SubsystemCalorimeterParametersTable").getChildren();
	for(const auto& row : rows) {
		const auto&    rec         = row.second;
		const uint64_t tableSerial = rec.getNode("SerialNumber").getValue<uint64_t>();

		if(static_cast<uint16_t>(tableSerial) != boardConfig_.serial)
			continue;

		boardConfig_.boardID = static_cast<uint16_t>(rec.getNode("BoardId").getValue<uint64_t>());
		writeRegister(ROC_ADDRESS_BOARD_ID, boardConfig_.boardID);
		boardConfig_.identityValid = true;
		break;
	}

	boardConfig_.voltagesLoaded       = false;
	boardConfig_.thresholdsLoaded     = false;
	boardConfig_.statusLoaded         = false;
	boardConfig_.mzbCalibrationLoaded = false;
	boardConfig_.voltageRecordUID.clear();
	boardConfig_.thresholdRecordUID.clear();
	boardConfig_.statusRecordUID.clear();
	boardConfig_.mzbCalibrationRecordUID.clear();

	if(boardConfig_.identityValid)
		__FE_COUT__ << "Mapped serial 0x" << std::hex << boardConfig_.serial << std::dec << " -> BoardId=" << boardConfig_.boardID << __E__;
	else
		__FE_COUT__ << "No match in ParametersTable for serial 0x" << std::hex << boardConfig_.serial << std::dec << __E__;
}

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

void ROCCalorimeterInterface::configure(void) try {
	ROCPolarFireCoreInterface::configure();

	// consider that we know all the init files
	// all the init information are stored in the configuration tree    //
	// set parameter
	// int linkID = getSelfNode().getNode("linkID").getValue<int>();
	//	__COUTV__(linkID);

	runSequenceOfCommands("ROCTypeLinkTable/LinkToConfigureSequence"); /*Run Configure Sequence Commands*/

	__COUT_INFO__ << "Enter ROC configuration.." << __E__;

	int readVal = 0;
	int ntries  = 0;
	while(readVal != 0x1234 || ntries < 3) {
		try {
			readVal = readRegister(0);
		} catch(const std::exception& e) {}
		ntries++;
		__COUT_INFO__ << "Attempt number " << ntries << __E__;
	}

	if(readVal == 0x1234)
		__COUT_INFO__ << "Configuration correct! :)" << __E__;
	else
		__COUT_INFO__ << "Configuration failed! :(" << __E__;

	updateBoardIdFromSerial_();

	std::stringstream dbLoadStatus;
	if(!loadVoltagesFromDB_(dbLoadStatus)) {
		__FE_SS__ << dbLoadStatus.str() << __E__;
		__FE_SS_THROW__;
	}
	if(!loadThresholdsFromDB_(dbLoadStatus)) {
		__FE_SS__ << dbLoadStatus.str() << __E__;
		__FE_SS_THROW__;
	}
	if(!loadStatusFromDB_(dbLoadStatus)) {
		__FE_SS__ << dbLoadStatus.str() << __E__;
		__FE_SS_THROW__;
	}
	__COUT_INFO__ << dbLoadStatus.str() << __E__;

	CalibrateMZB();
	SetADCsThresholds(50);
	SetBoardVoltages(true);
	SetBoardVoltages(true);
	SetBoardVoltages(true);
	SetBoardVoltages(true);

	writeRegister(ROC_ADDRESS_MASK_A, 1023);
	writeRegister(ROC_ADDRESS_MASK_B, 1023);

	writeRegister(ROC_ADDRESS_MZB_BUSY, 1);

} catch(const std::runtime_error& e) {
	__FE_COUT__ << "Error caught: " << e.what() << __E__;
	throw;
} catch(...) {
	__FE_SS__ << "Unknown error caught. Check printouts!" << __E__;
	try {
		throw;
	}  // one more try to printout extra info
	catch(const std::exception& e) {
		ss << "Exception message: " << e.what();
	} catch(...) {}
	__FE_SS_THROW__;
}

//==============================================================================
void ROCCalorimeterInterface::start(std::string runNumber) {
	SetupForADCsDataTaking(0, 1, 2300);

	return;
}

//==================================================================================================
bool ROCCalorimeterInterface::running(void) {
	// SetupForPatternFixedLengthDataTaking(40);

	// SetupForADCsDataTaking(0, 0, 2300);

	return false;
}

//==================================================================================================
void ROCCalorimeterInterface::Configure(__ARGS__) {
	__COUT_INFO__ << "Configure called" << __E__;
	configure();
}

//==================================================================================================
void ROCCalorimeterInterface::SetVoltageChannel(__ARGS__) { __COUT_INFO__ << "Set called" << __E__; }

//==================================================================================================
void ROCCalorimeterInterface::GetVoltageChannel(__ARGS__) {
	__COUT_INFO__ << "Get called" << __E__;
	__SET_ARG_OUT__("readValue", 12);
}
//==================================================================================================
void ROCCalorimeterInterface::GetTempChannel(__ARGS__) {
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
void ROCCalorimeterInterface::SetupForPatternFixedLengthDataTaking(__ARGS__) {
	unsigned int numberOfWords = __GET_ARG_IN__("Fixed Length of Event [units of 16-bit words, Default := 8]", uint32_t, 8);

	SetupForPatternFixedLengthDataTaking(numberOfWords);

}  // end SetupForPatternFixedLengthDataTaking()

//==================================================================================================
void ROCCalorimeterInterface::SetupForPatternFixedLengthDataTaking(unsigned int numberOfWords) {
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
void ROCCalorimeterInterface::SendMzCommand(std::string command, float paramVect[]) {
	__COUT_INFO__ << "SendMzCommand()" << __E__;

	uint8_t* vectToWrite;

	// writeRegister(ROC_ADDRESS_MZB_BUSY, 1);

	// MZB_OSCMDCODE_t cmd_code = mz_string_to_enum(command.c_str());
	MZB_OSCMDCODE_t cmd_code = SYNTAX_ERROR;

	for(size_t i = 0; i < sizeof(code_map) / sizeof(code_map[0]); i++) {
		// if (code_map[i].str == command.c_str()) {
		if(strcmp(code_map[i].str, command.c_str()) == 0) {
			cmd_code = code_map[i].code;
			break;
		}
	}

	if(cmd_code == SYNTAX_ERROR) {
		__FE_SS__ << "Wrong MZB command, please check the inserted string! " << __E__ << command << __E__ << command.c_str() << __E__ << code_map[21].str << __E__;
		__FE_SS_THROW__;
	}

	vectToWrite = MZB_Encode_CMD_Command_raw(cmd_code, paramVect);

	// uint16_t *input_data = &vectToWrite;
	//__COUT_INFO__ << "Mz debug ****" << __E__;

	std::vector<uint16_t> input_data;
	for(std::size_t i = 0; i < MZ_BUFFER_SIZE; i += 2) {
		uint16_t value = (static_cast<uint16_t>(vectToWrite[i]) << 8) | (static_cast<uint16_t>(vectToWrite[i + 1]));
		__COUT_INFO__ << std::hex << std::setprecision(4) << std::setfill('0') << "0x" << value << __E__;
		input_data.push_back(value);
	}

	writeROCBlock(input_data, MZ_ADDRESS, false /* incrementAddress*/);

	// free(vectToWrite);

	// writeRegister(ROC_ADDRESS_MZB_BUSY, 0);

	__COUT_INFO__ << "end SendMzCommand()" << __E__;
}  // end SendMzCommand()

//==================================================================================================

void ROCCalorimeterInterface::EnableDisableLEDs(__ARGS__) {
	__COUT_INFO__ << "EnableDisableLEDs()" << __E__;

	bool ledonoff = __GET_ARG_IN__("Enable LEDs, Default := 0]", bool, 0);

	std::string command = "CPULED";
	float       paramVect[9];
	paramVect[0] = ledonoff;
	paramVect[1] = NAN;
	paramVect[2] = NAN;
	paramVect[3] = NAN;
	paramVect[4] = NAN;
	paramVect[5] = NAN;
	paramVect[6] = NAN;
	paramVect[7] = NAN;
	paramVect[8] = NAN;

	SendMzCommand(command, paramVect);

	__COUT_INFO__ << "end EnableDisableLEDs()" << __E__;
}

//==================================================================================================

void ROCCalorimeterInterface::SendMzCommand(__ARGS__) {
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

void ROCCalorimeterInterface::EnableAndPowerSiPMs(__ARGS__) {
	bool  hvonoff = __GET_ARG_IN__("HV Enabled, Default := 0]", bool, 0);
	float vbias   = __GET_ARG_IN__("Bias voltage to set, Default := 0]", float, 0);

	EnableAndPowerSiPMs(hvonoff, vbias);

}  // end EnableAndPowerSiPMs()

//==================================================================================================

void ROCCalorimeterInterface::SetBoardVoltages(__ARGS__) {
	std::string conf = __GET_ARG_IN__("configuration folder, Default:= nominal", std::string, "nominal");

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

void ROCCalorimeterInterface::ConfigureLink(__ARGS__) {
	std::string conf          = __GET_ARG_IN__("Configuration folder, Default:= nominal", std::string, "nominal");
	bool        hvonoff       = __GET_ARG_IN__("HV Enabled, Default := 0]", bool, 0);
	bool        doCalibration = __GET_ARG_IN__("Upload MZB calibration parameters, Default := 0]", bool, 0);
	bool        setThresholds = __GET_ARG_IN__("Upload DiRAC thresholds, Default := 0]", bool, 0);
	int         offset        = __GET_ARG_IN__("Offset, Default := 0]", int, 0);

	std::string confFile = "boardMap.config";

	ConfigureLink(conf, confFile, hvonoff, doCalibration, setThresholds, offset);
}  // end ConfigureLink()

//==================================================================================================

//==================================================================================================

void ROCCalorimeterInterface::ConfigureLink(std::string conf, std::string confFile, bool hvonoff, bool doCalibration, bool setThresholds, int offset) {
	const int boardID = static_cast<int>(boardConfig_.boardID);
	__COUT_INFO__ << "Target BoardID = " << boardID << __E__;

	if(boardConfig_.boardID == INVALID_BOARDID) {
		updateBoardIdFromSerial_();
		if(boardConfig_.boardID == INVALID_BOARDID) {
			__FE_SS__ << "Skipping configuring board " << boardID << ", boardID not initialized correctly!" << __E__;
			__FE_SS_THROW__;
			return;
		}
	}

	if(boardID > MAX_BOARD_ID) {
		__FE_SS__ << "Skipping configuring board " << boardID << ", boardID out of range!" << __E__;
		__FE_SS_THROW__;
		return;
	}

	if(doCalibration)
		CalibrateMZB();
	SetBoardVoltages(hvonoff, boardID, conf);  // This checks which channels are pin diodes
	if(setThresholds)
		SetADCsThresholds(offset);

}  // end ConfigureLink()

//==================================================================================================

//==================================================================================================

void ROCCalorimeterInterface::CalibrateMZB(__ARGS__) { CalibrateMZB(); }  // end ConfigureLink()

//==================================================================================================

//==================================================================================================

void ROCCalorimeterInterface::CalibrateMZB() {
	const int boardID = static_cast<int>(boardConfig_.boardID);
	__COUT_INFO__ << "Target BoardID = " << boardID << __E__;

	if(boardConfig_.boardID == INVALID_BOARDID) {
		__FE_SS__ << "Skipping upload MZB parameters to board " << boardID << ", boardID not initialized correctly!" << __E__;
		__FE_SS_THROW__;
		return;
	}

	if(boardID > MAX_BOARD_ID) {
		__FE_SS__ << "Skipping upload MZB parameters to board " << boardID << ", boardID out of range!" << __E__;
		__FE_SS_THROW__;
		return;
	}

	const ots::ConfigurationManager* cfgMgr = getConfigurationManager();
	if(!cfgMgr) {
		__FE_SS__ << "getConfigurationManager() returned nullptr" << __E__;
		__FE_SS_THROW__;
		return;
	}

	auto                          rows = cfgMgr->getNode("SubsystemCalorimeterMZBCalibsTable").getChildren();
	const ots::ConfigurationTree* rec  = nullptr;
	std::string                   recUID;

	for(const auto& row : rows) {
		const auto&    r            = row.second;
		const uint64_t tableBoardId = r.getNode("BoardID").getValue<uint64_t>();
		if(static_cast<int>(tableBoardId) != boardID)
			continue;
		rec    = &r;
		recUID = row.first;
		break;
	}

	if(!rec) {
		__FE_SS__ << "BoardID=" << boardID << " not found in SubsystemCalorimeterMZBCalibsTable" << __E__;
		__FE_SS_THROW__;
		return;
	}

	__COUT_INFO__ << "Loaded MZB calibration from DB record UID=" << recUID << " for BoardID=" << boardID << __E__;

	// Read all 8 bitmaps (4 slope + 4 offset), each is 1-row x 20-col
	// icalib 0: HV DAC    → paramVect[1], paramVect[2]
	// icalib 1: HV ADC    → paramVect[3], paramVect[4]
	// icalib 2: Current   → paramVect[5], paramVect[6]
	// icalib 3: Temperature → paramVect[7], paramVect[8]
	auto hvDaqSlopeBmp  = rec->getNode("HVDAQslope").getValueAsBitMap();
	auto hvDaqOffsetBmp = rec->getNode("HVDAQoffset").getValueAsBitMap();
	auto hvAdcSlopeBmp  = rec->getNode("HVADCslope").getValueAsBitMap();
	auto hvAdcOffsetBmp = rec->getNode("HVADCoffset").getValueAsBitMap();
	auto curSlopeBmp    = rec->getNode("CurrentADCslope").getValueAsBitMap();
	auto curOffsetBmp   = rec->getNode("CurrentADCoffset").getValueAsBitMap();
	auto tempSlopeBmp   = rec->getNode("TemperatureADCslope").getValueAsBitMap();
	auto tempOffsetBmp  = rec->getNode("TemperatureADCoffset").getValueAsBitMap();

	for(int ichan = 0; ichan < 20; ichan++) {
		std::string command = "CALCARD";
		float       paramVect[9];

		paramVect[0] = ichan + 1;  // 1-indexed for firmware

		// Extract 4 slope/offset pairs from the bitmaps for this channel
		std::string sStr, oStr;

		// icalib 0: HV DAC calibration
		sStr         = hvDaqSlopeBmp.get(0, ichan);
		oStr         = hvDaqOffsetBmp.get(0, ichan);
		paramVect[1] = std::strtof(sStr.empty() ? "0" : sStr.c_str(), nullptr);
		paramVect[2] = std::strtof(oStr.empty() ? "0" : oStr.c_str(), nullptr);

		// icalib 1: HV ADC readback calibration
		sStr         = hvAdcSlopeBmp.get(0, ichan);
		oStr         = hvAdcOffsetBmp.get(0, ichan);
		paramVect[3] = std::strtof(sStr.empty() ? "0" : sStr.c_str(), nullptr);
		paramVect[4] = std::strtof(oStr.empty() ? "0" : oStr.c_str(), nullptr);

		// icalib 2: Current ADC calibration
		sStr         = curSlopeBmp.get(0, ichan);
		oStr         = curOffsetBmp.get(0, ichan);
		paramVect[5] = std::strtof(sStr.empty() ? "0" : sStr.c_str(), nullptr);
		paramVect[6] = std::strtof(oStr.empty() ? "0" : oStr.c_str(), nullptr);

		// icalib 3: Temperature ADC calibration
		sStr         = tempSlopeBmp.get(0, ichan);
		oStr         = tempOffsetBmp.get(0, ichan);
		paramVect[7] = std::strtof(sStr.empty() ? "0" : sStr.c_str(), nullptr);
		paramVect[8] = std::strtof(oStr.empty() ? "0" : oStr.c_str(), nullptr);

		__COUT_INFO__ << "ch " << ichan << "  DAC(" << paramVect[1] << ", " << paramVect[2] << ")"
		              << "  HVADC(" << paramVect[3] << ", " << paramVect[4] << ")"
		              << "  CurADC(" << paramVect[5] << ", " << paramVect[6] << ")"
		              << "  TmpADC(" << paramVect[7] << ", " << paramVect[8] << ")" << __E__;

		usleep(100000);

		SendMzCommand(command, paramVect);
	}

	boardConfig_.mzbCalibrationLoaded    = true;
	boardConfig_.mzbCalibrationRecordUID = recUID;

	__COUT_INFO__ << "MZB calibration done from SubsystemCalorimeterMZBCalibsTable (UID=" << recUID << ") for boardID=" << boardID << __E__;

}  // end CalibrateMZB()

//==================================================================================================

//==================================================================================================

void ROCCalorimeterInterface::SetADCsThresholds(__ARGS__) {
	int offset = __GET_ARG_IN__("Offset, Default := 0]", int, 0);

	SetADCsThresholds(offset);
}  // end ConfigureLink()

void ROCCalorimeterInterface::SetADCsThresholds(int offset) {
	const int boardID = static_cast<int>(boardConfig_.boardID);
	__COUT_INFO__ << "Target BoardID = " << boardID << __E__;

	if(boardID > MAX_BOARD_ID) {
		__COUT_ERR__ << "Skipping setting thresholds to board " << boardID << ", boardID out of range!" << __E__;
		return;
	}

	if(!boardConfig_.identityValid || boardConfig_.boardID == INVALID_BOARDID) {
		__COUT_ERR__ << "Skipping setting thresholds to board " << boardID << ", boardID not initialized correctly!" << __E__;
		return;
	}

	if(!boardConfig_.thresholdsLoaded) {
		__COUT_ERR__ << "Threshold cache not loaded. Run configure first." << __E__;
		return;
	}

	if(!boardConfig_.voltagesLoaded) {
		__COUT_ERR__ << "Voltage/sensor-type cache not loaded. Run configure first." << __E__;
		return;
	}

	for(int ichan = 0; ichan < 20; ++ichan) {
		const int thr     = boardConfig_.channels[ichan].threshold;
		int       thr2set = thr + offset;

		if(isPinDiodeChannel_(ichan)) {
			thr2set = thr;
			__COUT_INFO__ << "Board " << boardID << " Channel " << ichan << " detected as pin diode from DB cache. Ignoring requested offset. "
			              << "Threshold: " << thr2set << __E__;
		}

		__COUT_INFO__ << "Board " << boardID << " ch " << ichan << " thr=" << thr << " offset=" << offset << " -> set " << thr2set << __E__;

		writeRegister(ROC_ADDRESS_BASE_THRESHOLD + ichan, thr2set);
	}

	__COUT_INFO__ << "Thresholds set done from cached SubsystemCalorimeterThresholdsTable (UID=" << boardConfig_.thresholdRecordUID << ") for boardID=" << boardID << __E__;
}

/*void ROCCalorimeterInterface::SetADCsThresholds(int boardID, int offset) {
    char buff[50];
    sprintf(buff, "dirac%03d.baseline", boardID);

    if(boardID >= MAX_BOARD_ID) {
        __COUT_INFO__ << "Skipping setting thresholds to board " << boardID << __E__;
        return;
    }

    cet::filepath_lookup lookup_policy("MU2E_CALORIMETER_CONFIG_PATH");
    auto                 filename = lookup_policy(std::string("diracCalib/") + buff);  // FIXME! THIS NEEDS TO BE A VARIABLE

    std::ifstream confFile(filename);
    if(!confFile.is_open()) {
        __FE_SS__ << "Could not open file: " << filename << __E__;

        for(int ichan = 0; ichan < 20; ichan++) {
            writeRegister(ROC_ADDRESS_BASE_THRESHOLD + ichan, 2300);
        }
    } else {
        __COUT_INFO__ << "Opening file: " << filename << __E__;

        for(int ichan = 0; ichan < 20; ichan++) {
            int   chindex;
            float baseline;
            float sigma;
            int   thr;
            int   thr2set;

            confFile >> chindex >> baseline >> sigma >> thr;

            thr2set = thr + offset;

            for(auto pin : _pin_diode_list) {
                if(boardID * 100 + chindex == pin) {
                    thr2set = thr;
                    __COUT_INFO__ << "Board " << boardID << " Channel " << chindex
                                  << " detected as pin diode. Ignoring requested offset. "
                                     "Threshold: "
                                  << thr2set << __E__;
                    break;
                }
            }

            __COUT_INFO__ << chindex << "  " << baseline << "  " << sigma << "  " << thr2set << __E__;
            writeRegister(ROC_ADDRESS_BASE_THRESHOLD + ichan, thr2set);
        }

        confFile.close();
    }

    __COUT_INFO__ << "Thresholds set done.." << filename << __E__;

}  // end SetADCsThresholds()*/

//==================================================================================================

// void ROCCalorimeterInterface::ReadVoltagesFromDB(__ARGS__) {
//	ReadVoltagesFromDB();
// }

bool ROCCalorimeterInterface::isPinDiodeChannel_(int ch) const {
	if(ch < 0 || ch >= 20)
		return false;
	return boardConfig_.channels[ch].isPinDiode();
}

bool ROCCalorimeterInterface::loadVoltagesFromDB_(std::ostream& os) {
	const int boardID = static_cast<int>(boardConfig_.boardID);
	os << "Target BoardID = " << boardID << "\n";

	if(!boardConfig_.identityValid || boardConfig_.boardID == INVALID_BOARDID) {
		os << "ERROR: board identity not initialized correctly!\n";
		return false;
	}

	if(boardID > MAX_BOARD_ID) {
		os << "ERROR: boardID=" << boardID << " out of range!\n";
		return false;
	}

	const ots::ConfigurationManager* cfgMgr = getConfigurationManager();
	if(!cfgMgr) {
		os << "ERROR: getConfigurationManager() returned nullptr\n";
		return false;
	}

	auto                          rows = cfgMgr->getNode("SubsystemCalorimeterVoltagesTable").getChildren();
	const ots::ConfigurationTree* rec  = nullptr;
	std::string                   recUID;

	for(const auto& row : rows) {
		const auto&    r            = row.second;
		const uint64_t tableBoardId = r.getNode("BoardID").getValue<uint64_t>();
		if(static_cast<int>(tableBoardId) != boardID)
			continue;
		rec    = &r;
		recUID = row.first;
		break;
	}

	if(!rec) {
		os << "ERROR: BoardID=" << boardID << " not found in SubsystemCalorimeterVoltagesTable\n";
		return false;
	}

	auto voltageBmp = rec->getNode("Voltage").getValueAsBitMap();
	auto sensorBmp  = rec->getNode("SensorType").getValueAsBitMap();

	os << "FOUND voltage record UID=" << recUID << " for BoardID=" << boardID << "\n";
	os << "Voltage bitmap: rows=" << voltageBmp.numberOfRows() << " cols(0)=" << (voltageBmp.numberOfRows() > 0 ? voltageBmp.numberOfColumns(0) : 0) << "\n";
	os << "SensorType bitmap: rows=" << sensorBmp.numberOfRows() << " cols(0)=" << (sensorBmp.numberOfRows() > 0 ? sensorBmp.numberOfColumns(0) : 0) << "\n";

	if(voltageBmp.numberOfRows() == 0 || voltageBmp.numberOfColumns(0) < 20) {
		os << "ERROR: Voltage bitmap has too few columns for BoardID=" << boardID << "\n";
		return false;
	}

	if(sensorBmp.numberOfRows() == 0 || sensorBmp.numberOfColumns(0) < 20) {
		os << "ERROR: SensorType bitmap has too few columns for BoardID=" << boardID << "\n";
		return false;
	}

	boardConfig_.voltageRecordUID = recUID;

	for(int ichan = 0; ichan < 20; ++ichan) {
		std::string vStr  = voltageBmp.get(0, ichan);
		std::string sType = sensorBmp.get(0, ichan);

		if(vStr.empty())
			vStr = "0";
		if(sType.empty())
			sType = "UNKNOWN";

		char*       endp    = nullptr;
		const float voltage = std::strtof(vStr.c_str(), &endp);
		if(endp == vStr.c_str()) {
			os << "ERROR: Invalid voltage value for BoardID=" << boardID << " ch=" << ichan << ": '" << vStr << "'\n";
			return false;
		}

		boardConfig_.channels[ichan].voltage    = voltage;
		boardConfig_.channels[ichan].sensorType = sType;
	}

	boardConfig_.voltagesLoaded = true;
	return true;
}

bool ROCCalorimeterInterface::loadThresholdsFromDB_(std::ostream& os) {
	const int boardID = static_cast<int>(boardConfig_.boardID);
	os << "Target BoardID = " << boardID << "\n";

	if(!boardConfig_.identityValid || boardConfig_.boardID == INVALID_BOARDID) {
		os << "ERROR: board identity not initialized correctly!\n";
		return false;
	}

	if(boardID > MAX_BOARD_ID) {
		os << "ERROR: boardID=" << boardID << " out of range!\n";
		return false;
	}

	const ots::ConfigurationManager* cfgMgr = getConfigurationManager();
	if(!cfgMgr) {
		os << "ERROR: getConfigurationManager() returned nullptr\n";
		return false;
	}

	auto                          rows = cfgMgr->getNode("SubsystemCalorimeterThresholdsTable").getChildren();
	const ots::ConfigurationTree* rec  = nullptr;
	std::string                   recUID;

	for(const auto& row : rows) {
		const auto&    r            = row.second;
		const uint64_t tableBoardId = r.getNode("BoardID").getValue<uint64_t>();
		if(static_cast<int>(tableBoardId) != boardID)
			continue;
		rec    = &r;
		recUID = row.first;
		break;
	}

	if(!rec) {
		os << "ERROR: BoardID=" << boardID << " not found in SubsystemCalorimeterThresholdsTable\n";
		return false;
	}

	auto thrBmp = rec->getNode("Thresholds").getValueAsBitMap();
	os << "FOUND threshold record UID=" << recUID << " for BoardID=" << boardID << "\n";
	os << "Thresholds bitmap: rows=" << thrBmp.numberOfRows() << " cols(0)=" << (thrBmp.numberOfRows() > 0 ? thrBmp.numberOfColumns(0) : 0) << "\n";

	if(thrBmp.numberOfRows() == 0 || thrBmp.numberOfColumns(0) < 20) {
		os << "ERROR: Thresholds bitmap has too few columns for BoardID=" << boardID << "\n";
		return false;
	}

	boardConfig_.thresholdRecordUID = recUID;

	for(int ichan = 0; ichan < 20; ++ichan) {
		std::string s = thrBmp.get(0, ichan);
		if(s.empty())
			s = "2300";

		char*        endp = nullptr;
		const double v    = std::strtod(s.c_str(), &endp);
		if(endp == s.c_str()) {
			os << "ERROR: Invalid threshold value for BoardID=" << boardID << " ch=" << ichan << ": '" << s << "'\n";
			return false;
		}

		boardConfig_.channels[ichan].threshold = static_cast<int>(v + (v >= 0 ? 0.5 : -0.5));
	}

	boardConfig_.thresholdsLoaded = true;
	return true;
}

bool ROCCalorimeterInterface::loadStatusFromDB_(std::ostream& os) {
	const int boardID = static_cast<int>(boardConfig_.boardID);
	os << "Target BoardID = " << boardID << "\n";

	if(!boardConfig_.identityValid || boardConfig_.boardID == INVALID_BOARDID) {
		os << "ERROR: board identity not initialized correctly!\n";
		return false;
	}

	if(boardID > MAX_BOARD_ID) {
		os << "ERROR: boardID=" << boardID << " out of range!\n";
		return false;
	}

	const ots::ConfigurationManager* cfgMgr = getConfigurationManager();
	if(!cfgMgr) {
		os << "ERROR: getConfigurationManager() returned nullptr\n";
		return false;
	}

	auto                          rows = cfgMgr->getNode("SubsystemCalorimeterStatusTable").getChildren();
	const ots::ConfigurationTree* rec  = nullptr;
	std::string                   recUID;

	for(const auto& row : rows) {
		const auto&    r            = row.second;
		const uint64_t tableBoardId = r.getNode("BoardID").getValue<uint64_t>();
		if(static_cast<int>(tableBoardId) != boardID)
			continue;
		rec    = &r;
		recUID = row.first;
		break;
	}

	if(!rec) {
		os << "ERROR: BoardID=" << boardID << " not found in SubsystemCalorimeterStatusTable\n";
		return false;
	}

	auto stsBmp = rec->getNode("Status").getValueAsBitMap();
	os << "FOUND status record UID=" << recUID << " for BoardID=" << boardID << "\n";
	os << "Status bitmap: rows=" << stsBmp.numberOfRows() << " cols(0)=" << (stsBmp.numberOfRows() > 0 ? stsBmp.numberOfColumns(0) : 0) << "\n";

	if(stsBmp.numberOfRows() == 0 || stsBmp.numberOfColumns(0) < 20) {
		os << "ERROR: Status bitmap has too few columns for BoardID=" << boardID << "\n";
		return false;
	}

	boardConfig_.statusRecordUID = recUID;

	for(int ichan = 0; ichan < 20; ++ichan) {
		std::string s = stsBmp.get(0, ichan);
		if(s.empty())
			s = "undefined";

		boardConfig_.channels[ichan].channelStatus = s;
	}

	boardConfig_.statusLoaded = true;
	return true;
}

void ROCCalorimeterInterface::ReadChannelStatusFromDB(__ARGS__) {
	std::stringstream os;

	if(!loadStatusFromDB_(os)) {
		__SET_ARG_OUT__("Status", os.str());
		return;
	}

	os << "\n";
	os << "Ch  ChannelStatus\n";
	os << "--- -------------\n";

	for(int ichan = 0; ichan < 20; ++ichan) {
		os << std::setw(3) << ichan << " " << std::setw(13) << boardConfig_.channels[ichan].channelStatus << "\n";
	}

	__COUT_INFO__ << "ReadChannelStatusFromDB for BoardID=" << boardConfig_.boardID << ":\n" << os.str() << __E__;

	__SET_ARG_OUT__("Status", os.str());
}

void ROCCalorimeterInterface::ReadVoltagesFromDB(__ARGS__) {
	std::stringstream os;

	if(!loadVoltagesFromDB_(os)) {
		__SET_ARG_OUT__("Status", os.str());
		return;
	}

	os << "\n";
	os << "Ch  Voltage     SensorType\n";
	os << "--- ----------- ----------\n";

	for(int ichan = 0; ichan < 20; ++ichan) {
		os << std::setw(3) << ichan << " " << std::setw(11) << boardConfig_.channels[ichan].voltage << " " << boardConfig_.channels[ichan].sensorType << "\n";
	}

	__COUT_INFO__ << "ReadVoltagesFromDB for BoardID=" << boardConfig_.boardID << ":\n" << os.str() << __E__;

	__SET_ARG_OUT__("Status", os.str());
}

void ROCCalorimeterInterface::PrintROCConfiguration(__ARGS__) {
	std::stringstream os;

	os << "ROC cached configuration\n";
	os << "------------------------\n";
	os << "identityValid      = " << (boardConfig_.identityValid ? "true" : "false") << "\n";
	os << "serial             = 0x" << std::hex << boardConfig_.serial << std::dec << "\n";
	os << "boardID            = " << boardConfig_.boardID << "\n";
	os << "voltagesLoaded     = " << (boardConfig_.voltagesLoaded ? "true" : "false") << "\n";
	os << "thresholdsLoaded   = " << (boardConfig_.thresholdsLoaded ? "true" : "false") << "\n";
	os << "statusLoaded       = " << (boardConfig_.statusLoaded ? "true" : "false") << "\n";
	os << "mzbCalibrationLoaded = " << (boardConfig_.mzbCalibrationLoaded ? "true" : "false") << "\n";
	os << "voltageRecordUID   = " << boardConfig_.voltageRecordUID << "\n";
	os << "thresholdRecordUID = " << boardConfig_.thresholdRecordUID << "\n";
	os << "statusRecordUID    = " << boardConfig_.statusRecordUID << "\n";
	os << "mzbCalibrationRecordUID = " << boardConfig_.mzbCalibrationRecordUID << "\n";

	os << "\n";
	os << "Ch  Voltage     Threshold   Status  SensorType\n";
	os << "--- ----------- ----------- ------- ----------\n";

	for(int ichan = 0; ichan < 20; ++ichan) {
		os << std::setw(3) << ichan << " " << std::setw(11) << boardConfig_.channels[ichan].voltage << " " << std::setw(11) << boardConfig_.channels[ichan].threshold << " " << std::setw(7)
		   << boardConfig_.channels[ichan].channelStatus << " " << boardConfig_.channels[ichan].sensorType << "\n";
	}

	__COUT_INFO__ << os.str() << __E__;
	__SET_ARG_OUT__("Status", os.str());
}

//==================================================================================================

void ROCCalorimeterInterface::SetBoardVoltages(bool hvonoff) { SetBoardVoltages(hvonoff, static_cast<int>(boardConfig_.boardID), "DB"); }

void ROCCalorimeterInterface::SetBoardVoltages(bool hvonoff, int boardID, std::string conf) {
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

	if(hvonoff == 1) {
		if(!boardConfig_.identityValid || boardConfig_.boardID != boardID) {
			__FE_SS__ << "Board identity cache does not match requested boardID=" << boardID << __E__;
			__FE_SS_THROW__;
		}

		if(!boardConfig_.voltagesLoaded) {
			__FE_SS__ << "Voltage cache not loaded. Run configure first." << __E__;
			__FE_SS_THROW__;
		}

		if(!boardConfig_.statusLoaded) {
			__FE_SS__ << "Channel status cache not loaded. Run configure first." << __E__;
			__FE_SS_THROW__;
		}

		float vbias[20];

		for(int ichan = 0; ichan < 20; ichan++) {
			const bool channelIsGood = (boardConfig_.channels[ichan].channelStatus == "good");
			vbias[ichan]             = channelIsGood ? boardConfig_.channels[ichan].voltage : 0.0f;

			__COUT_INFO__ << "Board " << boardID << " ch " << ichan << " voltage " << vbias[ichan] << " requested " << boardConfig_.channels[ichan].voltage << " status "
			              << boardConfig_.channels[ichan].channelStatus << " type " << boardConfig_.channels[ichan].sensorType << __E__;
		}

		RMZB_writeAllSiPMbias(vbias);

		__COUT_INFO__ << "Ramping up good channels from cached DB voltages for BoardID=" << boardID << __E__;
		__COUT_INFO__ << "Configuration done from DB cache for BoardID=" << boardID << __E__;
	}

}  // end SetBoardVoltages()

//==================================================================================================

//==================================================================================================

void ROCCalorimeterInterface::EnableAndPowerSiPMs(bool hvonoff, float vbias) {
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
void ROCCalorimeterInterface::ReadROCErrorCounter(__ARGS__) {
	__COUT_INFO__ << "ReadROCErrorCounter()" << __E__;

	unsigned int errAddr = __GET_ARG_IN__("Address to read, Default := 0]", uint16_t, 0);
	__FE_COUTV__(errAddr);

	writeRegister(ROC_ADDRESS_ERRCNT, errAddr);
	writeRegister(ROC_ADDRESS_IS_PATTERN, 64);

	std::stringstream  os;
	DTCLib::roc_data_t readVal;
	readVal = readRegister(ROC_ADDRESS_ERRCNT);

	os << std::hex << std::setprecision(4) << std::setfill('0') << "address 0x" << errAddr << " (" << std::dec << errAddr << std::hex << "): data 0x" << readVal << " (" << std::dec << readVal << ")\n"
	   << __E__;

	writeRegister(ROC_ADDRESS_IS_PATTERN, 0);

	__COUT_INFO__ << "end ReadROCErrorCounter()" << __E__;

	__SET_ARG_OUT__("Status", os.str());

}  // end ReadROCErrorCounter()

//==================================================================================================

void ROCCalorimeterInterface::SetupForADCsDataTaking(__ARGS__) {
	bool         setThr    = __GET_ARG_IN__("Set Threshold? [bool, Default := 0]", bool, 0);
	bool         isNfw     = __GET_ARG_IN__("Is new Firmware? [bool, Default := 0]", bool, 0);
	unsigned int threshold = __GET_ARG_IN__("Threshold [units of adccounts, Default := 2300]", uint32_t, 2300);

	SetupForADCsDataTaking(setThr, isNfw, threshold);

}  // end SetupForADCsDataTaking()

//==================================================================================================

//==================================================================================================

void ROCCalorimeterInterface::SetupForNoiseTaking(__ARGS__) {
	unsigned int numberOfsamples = __GET_ARG_IN__("Number of noise samples per evt [Default := 20]", uint32_t, 20);

	SetupForNoiseTaking(numberOfsamples);

}  // end SetupForNoiseTaking()

//==================================================================================================

//==================================================================================================

void ROCCalorimeterInterface::SetupForNoiseTaking(unsigned int numberOfsamples) {
	__COUT_INFO__ << "SetupForNoiseTaking()" << __E__;

	// writeRegister(ROC_ADDRESS_MASK_A, 1023);
	// writeRegister(ROC_ADDRESS_MASK_B, 1023);

	writeRegister(ROC_ADDRESS_SIMWF_ENABLE_A, 0);
	writeRegister(ROC_ADDRESS_SIMWF_ENABLE_B, 0);
	writeRegister(ROC_ADDRESS_SIMWF_MULTI_A, 0);
	writeRegister(ROC_ADDRESS_SIMWF_MULTI_B, 0);

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
void ROCCalorimeterInterface::SetupForADCsDataTaking(bool setThr, bool isNFw, unsigned int threshold) {
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

	// writeRegister(ROC_ADDRESS_MASK_A, 1023);
	// writeRegister(ROC_ADDRESS_MASK_B, 1023);

	if(isNFw) {
		writeRegister(ROC_ADDRESS_SIMWF_ENABLE_A, 0);
		writeRegister(ROC_ADDRESS_SIMWF_ENABLE_B, 0);
		writeRegister(ROC_ADDRESS_SIMWF_MULTI_A, 0);
		writeRegister(ROC_ADDRESS_SIMWF_MULTI_B, 0);
	}

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
	if(setThr) {
		for(int ich = 0; ich < 20; ich++) {
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

void ROCCalorimeterInterface::ToggleMBBusy(__ARGS__) {
	unsigned int busyonoff = __GET_ARG_IN__("On/Off [Busy On/Off, Default := 1]", bool, 1);
	ToggleMBBusy(busyonoff);

}  // end ToggleMBBusy()

void ROCCalorimeterInterface::ToggleMBBusy(bool busyonoff) { writeRegister(ROC_ADDRESS_MZB_BUSY, busyonoff); }  // end ToggleMBBusy()

//==================================================================================================

//==================================================================================================
void ROCCalorimeterInterface::GetStatus(__ARGS__) {
	// copied from Monica's va_read_all.sh

	DTCLib::roc_data_t readVal;

	std::stringstream     os;
	DTCLib::roc_address_t address;

	address = 0x0;
	readVal = readRegister(address);
	os << std::hex << std::setprecision(4) << std::setfill('0') << "address 0x" << address << " (" << std::dec << address << std::hex << "): data 0x" << readVal << " (" << std::dec << readVal << ")\n"
	   << __E__;

	address = 0x8;
	readVal = readRegister(address);
	os << std::hex << std::setprecision(4) << std::setfill('0') << "address 0x" << address << " (" << std::dec << address << std::hex << "): data 0x" << readVal << " (" << std::dec << readVal << ")"
	   << __E__;
	os << "\t\t"
	   << "bit[9:8]=[enable_marker,enable_clock]"
	      "\n\t\t bit[7:4]=[en_int_ewm,en_free_ewm,error_en,pattern_en]"
	      "\n\t\t bit[3:0]=en_lanes[HV1,HV0,CAl1,CAL0]\n"
	   << __E__;

	address = 18;
	readVal = readRegister(address);
	os << std::hex << std::setprecision(4) << std::setfill('0') << "address 0x" << address << " (" << std::dec << address << std::hex << "): data 0x" << readVal << " (" << std::dec << readVal << ")"
	   << __E__;
	os << "\t\t"
	   << "bit[9:8]=[enable_marker,enable_clock]"
	      "\n\t\t bit[7:4]=[en_int_ewm,en_free_ewm,error_en,pattern_en]"
	      "\n\t\t bit[3:0]=en_lanes[HV1,HV0,CAl1,CAL0]\n"
	   << __E__;

	address = 72;
	readVal = readRegister(address);
	os << std::hex << std::setprecision(4) << std::setfill('0') << "address 0x" << address << " (" << std::dec << address << std::hex << "): hbtag error 0x" << readVal << " (" << std::dec << readVal
	   << ") \n"
	   << __E__;

	address = 73;
	readVal = readRegister(address);
	os << std::hex << std::setprecision(4) << std::setfill('0') << "address 0x" << address << " (" << std::dec << address << std::hex << "): dreq error 0x" << readVal << " (" << std::dec << readVal
	   << ") \n"
	   << __E__;

	address = 74;
	readVal = readRegister(address);
	os << std::hex << std::setprecision(4) << std::setfill('0') << "address 0x" << address << " (" << std::dec << address << std::hex << "): hblost 0x" << readVal << " (" << std::dec << readVal
	   << ") \n"
	   << __E__;

	address = 75;
	readVal = readRegister(address);
	os << std::hex << std::setprecision(4) << std::setfill('0') << "address 0x" << address << " (" << std::dec << address << std::hex << "): evm lost 0x" << readVal << " (" << std::dec << readVal
	   << ") \n"
	   << __E__;

	uint32_t doubleRegVal = 0;

	std::vector<DTCLib::roc_address_t> doubleReads = {23, 25, 64, 27, 29, 31, 33, 35, 37, 39, 41, 43, 45, 48, 51, 54, 57};

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

	for(size_t i = 0; i < doubleReads.size(); ++i) {
		address      = doubleReads[i];
		readVal      = readRegister(address);
		doubleRegVal = readVal;
		readVal      = readRegister(++address);
		doubleRegVal |= readVal << 16;

		os << std::hex << std::setprecision(4) << std::setfill('0') << "address 0x" << address - 1 << " (" << std::dec << address - 1 << std::setprecision(8) << std::hex << "): data 0x"
		   << doubleRegVal << " (" << std::dec << doubleRegVal << ")" << __E__;
		os << "\t\t" << doubleReadCaptions[i] << "\n" << __E__;
	}  // end double read register loop

	__SET_ARG_OUT__("Status", os.str());

}  // end GetStatus()

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define hrdFmt                                                                                  \
	"Ch#   errcnt |  HVDAC errcnt |  ADC1  ADC2  ADC3  ADC4  ADC5  ADC6  ADC7  ADC8 errcnt |\n" \
	"             |   [V]         |  [V]  [uA]   [C]   [V]   [V]   [V]   [V]   [V]         |\n"
#define enCard1 "Ch#%02d %6d | "
#define enCard2 "%5.1f %6d  | "
#define enCard3 "%6d |\n"
#define dsCard "Ch#%02d disabled --------------+---------------------------------------------------------+\n"
#define outtext_1(x) \
	sprintf(buf, x); \
	os << buf;
#define outtext_2(x, y) \
	sprintf(buf, x, y); \
	os << buf;
#define outtext_3(x, y, z) \
	sprintf(buf, x, y, z); \
	os << buf;
#define outtext_4(x, y, z, t) \
	sprintf(buf, x, y, z, t); \
	os << buf;
#define outtext_5(x, y, z, t, u) \
	sprintf(buf, x, y, z, t, u); \
	os << buf;
#define outtext_6(x, y, z, t, u, v) \
	sprintf(buf, x, y, z, t, u, v); \
	os << buf;

typedef struct MZB_STATUS_REG_tag {
	// SOFTWARE flags
	unsigned char flashConfig : 1;     // bit #0   - 1=configuration from flash, 0=factory default
	unsigned char autoConfigFlag : 1;  // bit #1   - 1=AD auto configuration in progress
	unsigned char rampActive : 1;      // bit #2   - Ramp active flag
	unsigned char feeRef_enabled : 1;  // bit #3   - 1=enable DAQ on FEE reference channels (4 to 8)
	unsigned char image : 2;           // bit #4-5 - 1= IMAGE1, 2= IMAGE2
	unsigned char validImages : 2;     // bit #6-7 - 1= IMAGE1, 2= IMAGE2

	unsigned char AD_enabled : 1;      // bit #8
	unsigned char AD_pendingRead : 1;  // bit #9
	unsigned char : 2;                 // bit #10-11 reserved
	unsigned char diracHbtIn : 1;      // bit #12   - Heartbeat from DIRAC enabled
	unsigned char diracHbtOut : 1;     // bit #13   - Heartbeat to DIRAC enabled
	unsigned char hwWdg : 1;           // bit #14   - Set if wdg is active
	unsigned char swWdg : 1;           // bit #15   - Set if wdg is active

	unsigned char rstsrc : 8;  // bit #16-23 Reset source

	unsigned char : 5;             // bit #24..28
	unsigned char cpuLedFlag : 1;  // bit #29  - CPU active will flash
	unsigned char wrnflag : 1;     // bit #30
	unsigned char errflag : 1;     // bit #31
} MZB_STATUS_REG_t;

//==================================================================================================
std::stringstream formatCardStatus(EE_DATABUF_t* pbuf) {
	char              buf[512];
	std::stringstream os;
	MZB_STATUS_REG_t  stsreg;

	std::memcpy(&stsreg, &pbuf->stsreg, sizeof(pbuf->stsreg));

	outtext_2("config from flash: %d\n", stsreg.flashConfig);
	outtext_2("ramp active      : %d\n", stsreg.rampActive);
	outtext_2("FEE ref enable   : %d\n", stsreg.feeRef_enabled);
	outtext_2("current image    : %d\n", stsreg.image);
	outtext_2("valid images     : %d\n", stsreg.validImages);
	outtext_2("HW watchdog      : %d\n", stsreg.hwWdg);
	outtext_2("SW watchdog      : %d\n", stsreg.swWdg);

	outtext_2("CPU led flash    : %d\n", stsreg.cpuLedFlag);
	outtext_2("MZB error flag   : %d\n", stsreg.errflag);

	outtext_2("MZB error count  : %d\n", pbuf->errcnt);
	outtext_2("MZB last error   : %d\n", pbuf->lastError);

	return os;

	/*
	MZB_DATA_t mzb;                  // Application specific data

	outtext_2("FEE status, total conversion %d\n", mzb.mzbconv);

	// Report card data
	outtext_2("FEE status, total conversion %d\n", mzb.mzbconv);
	outtext_2("HV switch: O%s", mzb.____ ? "N" : "FF");
	outtext_2(", HVS fsm: %s\n", mzb.____);
	outtext_1(hrdFmt);
	card = &mzb.card[i];
	for(; i<ch; i++, card++) {
	outtext_3(enCard1, i+1, card->errcnt);
	outtext_3(enCard2, card->edac.phvreq, card->edac.errcnt);
	for(j=0; j<3; j++) {
	    outtext_2("%5.2f ", card->eads.adch[j].phv);
	}
	for(; j<FEE_ADC_CHANNELS; j++) {
	    outtext_2("%5.3f ", card->eads.adch[j].phv);
	}
	outtext_2(enCard3, card->eads.errcnt);
	}
	outtext_6("FUSES: %d\t%d\t%d\t%d\t%d\n",
	mzb.inpp.FUSE1, mzb.inpp.FUSE2, mzb.inpp.FUSE3,
	mzb.inpp.FUSE4, mzb.inpp.FUSE5);

	outtext_5("GPIO: HV_%d\tBACK_%d\tRST_%d\tATTN_%d\n",
	    mzb.outp.bits.HVDACEN,
	    mzb.outp.bits.BUSYACK,
	    mzb.outp.bits.RSTCPU,
	    mzb.outp.bits.ATTNO);

	outtext_6("AUX_1: %.1f\t%.1f\t%.1f\t%.1f\t%.1f\n",
	mzb.ladc.adch[0].phv, mzb.ladc.adch[1].phv, mzb.ladc.adch[2].phv,
	mzb.ladc.adch[3].phv, mzb.ladc.adch[4].phv);

	// Display AUX_IN0 data & status
	adch = mzb.auxadc.adch;
	outtext_3("AUX_2: %.0f_(%d)",
	adch->phv, adch->sts.flipCount);
	// Display AUX_IN1 data & status
	adch++;
	outtext_2("\t%.1f", adch->phv);
	// Display AUX_IN2 data & status
	adch++;
	outtext_2("\t%.1f", adch->phv);
	// Display AUX_IN3 data & status
	adch++;
	outtext_3("\t%.0f_(%d)", adch->phv, adch->sts.flipCount);
	// Display AUX_IN4 data & status
	adch++;
	outtext_3("\t%.0f_(%d)", adch->phv, adch->sts.flipCount);
	// Display AUX_IN5 data & status
	adch++;
	outtext_3("\t%.0f_(%d)", adch->phv, adch->sts.flipCount);
	// Display AUX_IN6 data & status
	adch++;
	outtext_2("\t%.1f", adch->phv);
	// Display AUX_IN7 data & status
	adch++;
	outtext_2("\t%.1f\n", adch->phv);
	*/
}

//==================================================================================================
void ROCCalorimeterInterface::GetMZBStatus(__ARGS__) {
	EE_DATABUF_t buf;

	std::stringstream os;
	std::stringstream os1;
	os << "Reading MB registers:" << __E__;
	os << __E__;

	std::vector<DTCLib::roc_data_t> payload;

	int nwords = offsetof(EE_DATABUF_t, apdBiasV_tag) / 2;
	readROCBlock(payload, 261, nwords, false /*incAddress*/);

	memcpy(&buf, payload.data(), nwords * 2);

	os << formatCardStatus(&buf).str();

	os << std::hex << std::showbase;

	os << "ridx: " << buf.ridx << "\n";
	os << "reserved1[0]: " << buf.reserved1[0] << "\n";
	os << "reserved1[1]: " << buf.reserved1[1] << "\n";
	os << "reserved1[2]: " << buf.reserved1[2] << "\n";

	os << "updateCounter_tag: " << buf.updateCounter_tag << "\n";
	os << "reserved2: " << buf.reserved2 << "\n";
	os << "updateCounter: " << buf.updateCounter << "\n";
	os << "stsreg: " << buf.stsreg << "\n";
	os << "errreg: " << buf.errreg << "\n";
	os << "errcnt: " << buf.errcnt << "\n";
	os << "ch_ensts: " << buf.ch_ensts << "\n";
	os << "gpi: " << buf.gpi << "\n";
	os << "gpo: " << buf.gpo << "\n";
	os << "ladc[0]: " << buf.ladc[0] << "\n";
	os << "ladc[1]: " << buf.ladc[1] << "\n";
	// Se MZB_LADC_CHANNELS > 2 stamperesti altri, altrimenti continua:
	os << "lastError: " << buf.lastError << "\n";

	// Contiamo già 17 campi, aggiungiamo due di auxadc:
	os << "slewRate: " << +buf.slewRate << "\n";
	os << "hvmfsm: " << +buf.hvmfsm << "\n";

	__SET_ARG_OUT__("Status", os.str());

}  // end GetStatus()

/**
 *  @brief   Write the the SiPM HV set command to the MEZZANINE slave
 *  @param   ch    FEE channel to set
 *  @param   hv    HV bias Voltage
 *  @return  0=SUCCESS else error code

 */

//==================================================================================================
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

//==================================================================================================
unsigned ees_chksum_test(void* ptr, int len) {
	unsigned* src = (unsigned*)ptr;
	unsigned  sum;
	unsigned  tmp;

	__COUT_INFO__ << "checksum check : " << len << __E__;
	for(sum = 0; len >= 4; len -= 4, src++) {
		sum += *src;
		__COUT_INFO__ << "0x" << std::hex << *src << " --<< "
		              << "0x" << std::hex << sum << " " << __E__;
	}

	if(len) {
		tmp = 0;
		memcpy(&tmp, src, len);
		sum += tmp;
	}

	__COUT_INFO__ << "0x" << std::hex << sum << " sum)fine=" << __E__;

	return sum;
}

//==================================================================================================
void ROCCalorimeterInterface::RMZB_writeAllSiPMbias(float* hv) {
	struct __attribute__((packed, aligned(4))) {
		short dummy;
		short adr;
		struct __attribute__((packed)) {
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

	for(int ch = 0; ch < FEE_NUM; ch++) {
		float hvset          = hv[ch] * 10. + .49999;
		bm.bias.biasVreq[ch] = hvset;
		bm.bias.biasVreq[ch] |= 0x8000;
		input_data.push_back(bm.bias.biasVreq[ch]);
	}

	bm.chksum = -ees_chksum_test((void*)&bm.bias, sizeof(bm.bias));

	ees_chksum_test((void*)&bm.bias, 4 + sizeof(bm.bias));
	input_data.push_back((uint16_t)(bm.chksum & 0xFFFF));
	input_data.push_back((uint16_t)(bm.chksum >> 16));

	for(auto& value : input_data) {
		value = (value >> 8) | (value << 8);  // Scambia i due byte
	}

	writeROCBlock(input_data, MZ_ADDRESS, false /* incrementAddress*/);

	// usleep(10000);

	// std::vector<DTCLib::roc_data_t> check;
	// readROCBlock(check, 267, 1, false);

	// if (check[0] != 0x7668) writeROCBlock(input_data, MZ_ADDRESS, false /* incrementAddress*/);
}

//==================================================================================================
void ROCCalorimeterInterface::CreateGlobalROCTable(__ARGS__) {
	__FE_COUT__ << "CreateGlobalROCTable(FE)" << __E__;

	std::stringstream result;

	try {
		TableVersion newVersion = CreateGlobalROCTable(result);
		result << "\n\nCreateGlobalROCTable() done, newVersion = " << newVersion << __E__;
	} catch(const std::runtime_error& e) {
		__FE_SS__ << "CreateGlobalROCTable() Failed: " << e.what() << __E__ << "\n\nHere is the detail of what happened: " << result.str() << __E__;
		__FE_SS_THROW__;
	}

	__SET_ARG_OUT__("Status", result.str());
	__FE_COUT__ << "CreateGlobalROCTable(FE) done" << __E__;
}  // end CreateGlobalROCTable()

//==================================================================================================
// To be called by configure()
TableVersion ROCCalorimeterInterface::CreateGlobalROCTable(std::ostream& os, bool saveTemporaryTable /* = true */) {
	__FE_COUT__ << "CreateGlobalROCTable()" << __E__;

	// Steps:
	//  1. Read every ROC (in system) device ID .. register 47
	//	 	- Check every ROC device ID matches location in Configuration Tree
	//			* The first one in the Configuration Tree is the 'one' that does all actions!
	//  2. If mismatch, write a new table with observed location
	//		- Keep OFF ROCs untouched
	//  3. Throw and exception with mismatch and new table version

	const std::string tableName            = "ROCInterfaceTable";
	const std::string COL_LINK_ID          = "linkID";
	const std::string COL_DTC_ID           = "ROCGroupID";
	bool              boardIdMismatchFound = false;

	// Step 1. -------------------------------------
	//	read all board IDs (register 47) and cache all local ROC config values
	std::map<std::string /* roc UID */, std::map<std::string /* colName */, std::string /* value */>> rocCache;
	{
		auto localRocTable = getConfigurationManager()->getTableByName(tableName);

		{
			__SS__;
			localRocTable->getView().print(ss);
			__FE_COUTV__(ss.str());
		}

		auto rocs = getConfigurationManager()->getNode(tableName).getChildren();

		if(rocs.size() != localRocTable->getView().getDataView().size()) {
			__FE_SS__ << "ROC count mismatch! Something is very wrong! Notify admins." << __E__;
			__FE_SS_THROW__;
		}

		// cache all local values
		for(size_t row = 0; row < localRocTable->getView().getDataView().size(); ++row) {
			auto& columnInfo = localRocTable->getView().getColumnsInfo();
			for(size_t col = 0; col < columnInfo.size(); ++col) {
				rocCache[rocs[row].first][columnInfo[col].getName()] = localRocTable->getView().getDataView()[row][col];
				__FE_COUTT__ << "rocCache[" << rocs[row].first << "][" << columnInfo[col].getName() << "] = " << localRocTable->getView().getDataView()[row][col] << __E__;
			}
		}  // end cache all local values

		os << "\n\n";  // spacer;

		// now read Board ID from each ROC
		for(const auto& roc : rocs) {
			__FE_COUTV__(roc.first);

			// // cache all local values
			// for(const auto& colNodePair : roc.second.getChildren()) {
			// 	rocCache[roc.first][colNodePair.first] = colNodePair.second.getValueAsString();
			// 	__FE_COUTTV__(colNodePair.first);
			// 	__FE_COUTTV__(rocCache[roc.first][colNodePair.first]);
			// }

			// read register 47
			{
				std::vector<frontEndMacroArg_t> argsOut;
				std::vector<frontEndMacroArg_t> argsIn;
				__SET_ARG_IN__("Target ROC or Mask (Default = -1 := all ROCs, or 0x111111 := all)", roc.second.getNode(COL_LINK_ID).getValue<uint32_t>());
				__SET_ARG_IN__("address", (unsigned int)47);

				__FE_COUTV__(StringMacros::vectorToString(argsIn));

				std::string targetDTC = roc.second.getNode(COL_DTC_ID).getValue();
				if(targetDTC.find("Group") != std::string::npos) {
					__FE_COUT__ << "Assuming 'Group' is appended to DTC UID... removing to target DTC" << __E__;
					targetDTC = targetDTC.substr(0, targetDTC.find("Group"));
				}
				if(targetDTC.find("ROCs") != std::string::npos) {
					__FE_COUT__ << "Assuming 'ROCs' is appended to DTC UID... removing to target DTC" << __E__;
					targetDTC = targetDTC.substr(0, targetDTC.find("ROCs"));
				}

				try {
					runFrontEndMacro(targetDTC,   // const std::string& targetInterfaceID,
					                 "ROC Read",  // const std::string& feMacroName,
					                 argsIn,      // const std::vector<FEVInterface::frontEndMacroArg_t>& inputArgs,
					                 argsOut);    // std::vector<FEVInterface::frontEndMacroArg_t>& outputArgs) const;

					__FE_COUTV__(StringMacros::vectorToString(argsOut));

					for(const auto& arg : argsOut) {
						std::string name  = StringMacros::decodeURIComponent(arg.first);
						std::string value = StringMacros::decodeURIComponent(arg.second);

						__FE_COUT__ << "pre-Output " << name << ": " << value << __E__;
						if(name == "readData") {
							name = "UID";
							uint32_t boardIdNumber;
							StringMacros::getNumber(value.substr(value.find(':') + 2), boardIdNumber);
							value = std::to_string(boardIdNumber);
						}
						__FE_COUT__ << "Output " << name << ": " << value << __E__;

						if(rocCache[roc.first][name] == value) {
							__FE_COUT__ << "Board ID already correct for ROC " << roc.first << " on DTC " << targetDTC << ": cached value = " << rocCache[roc.first][name] << ", read value = " << value
							            << __E__;
							os << "* Board ID already correct for " << roc.first << ": " << value << __E__;
							continue;
						}

						boardIdMismatchFound      = true;  // flag that a mismatch
						rocCache[roc.first][name] = value;
						os << "* UPDATED Board ID found for " << roc.first << ": " << value << __E__;
					}
				} catch(const std::runtime_error& e) {
					__FE_COUT_WARN__ << "Ignoring (assuming ROC/DTC does not exist) error reading Board ID for ROC " << roc.first << " on DTC " << targetDTC << ":\n" << e.what() << __E__;
					os << "--> Ignoring (assuming ROC/DTC does not exist) error reading Board ID for ROC " << roc.first << " on DTC " << targetDTC << "." << __E__;
				}
			}
		}  // end roc cache and read loop
	}      // end Step 1. read all board IDs and cache all local ROC config values

	os << "\n\n";  // spacer;

	// at this point all values cached
	__FE_COUTV__(rocCache.size());

	if(!boardIdMismatchFound) {
		__FE_COUT_INFO__ << "ROC Board IDs all validated!" << __E__;
		os << "ROC Board IDs all validated!" << __E__;
		return TableVersion();  // return invalid version to indicate no new table was created
	}

	// Step 2. -------------------------------------
	std::string             author = "CaloROCmanager";
	ConfigurationManagerRW  cfgMgrInst(author);
	ConfigurationManagerRW* cfgMgr = &cfgMgrInst;

	// update table info for each new configuration manager
	//	IMPORTANTLY this also fills all configuration manager pointers with instances,
	//	so we are not dealing with changing pointers later on
	cfgMgr->getAllTableInfo(true /* refresh */,  // load empty instance of everything important
	                        0 /* accumulatedWarnings */,
	                        "" /* errorFilterName */,
	                        false /* getGroupKeys */,
	                        false /* getGroupInfo */,
	                        false /* initializeActiveGroups */);

	TableBase*   table            = cfgMgr->getTableByName(tableName);
	TableVersion temporaryVersion = table->createTemporaryView();
	TableView*   cfgView          = table->getTemporaryView(temporaryVersion);

	// create records and set values based on assembled ROC cache
	for(const auto& rocRecord : rocCache) {
		__FE_COUT__ << "Creating ROC " << rocRecord.first << __E__;
		uint32_t row = cfgView->addRow(author);
		for(const auto& rocFieldPair : rocRecord.second) {
			__FE_COUTT__ << "Setting field " << rocFieldPair.first << " --> " << rocFieldPair.second << __E__;
			const uint32_t col = cfgView->findCol(rocFieldPair.first);
			cfgView->setValueAsString(rocFieldPair.second, row, col);
		}
	}  // end create and set records based on assembled ROC cache

	cfgView->setComment("This table was automatically generated by the ROCCalorimeterInterface. It contains the mapping of ROC boards to their parent DTCs and link IDs.");

	std::stringstream tableSs;
	cfgView->print(tableSs);
	__FE_COUTV__(tableSs.str());

	try {
		cfgView->init();  // validate table before saving
	} catch(const std::runtime_error& e) {
		__FE_SS__ << "Error validating table: " << e.what() << __E__ << "\n\n"
		          << "Here is the detail and content of the created table with the error:\n"
		          << tableSs.str() << __E__;
		__FE_SS_THROW__;
	}

	bool         foundEquivalent;
	TableVersion newAssignedVersion = cfgMgr->saveModifiedVersion(tableName,
	                                                              TableVersion() /* originalVersion */,
	                                                              saveTemporaryTable /* makeTemporary */,
	                                                              table,
	                                                              cfgView->getVersion(),
	                                                              false /* ignoreDuplicates */,
	                                                              true /* lookForEquivalent */,
	                                                              &foundEquivalent);

	__FE_COUTV__(foundEquivalent);
	__FE_COUTV__(newAssignedVersion);
	__FE_COUTV__(tableName);

	os << "New generated " << tableName << " version = " << newAssignedVersion << __E__;
	os << "------------------\n\n" << tableSs.str() << __E__;

	__FE_COUT__ << "CreateGlobalROCTable() done" << __E__;

	return newAssignedVersion;
}  // end CreateGlobalROCTable()

DEFINE_OTS_INTERFACE(ROCCalorimeterInterface)
