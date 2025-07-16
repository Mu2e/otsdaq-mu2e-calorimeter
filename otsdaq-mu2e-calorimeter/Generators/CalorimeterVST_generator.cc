///////////////////////////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////
#include "artdaq/DAQdata/Globals.hh"
#define TRACE_NAME (app_name + "_CalorimeterVST").c_str()

#include "canvas/Utilities/Exception.h"

#include "artdaq-core-mu2e/Overlays/FragmentType.hh"
#include "artdaq-core-mu2e/Overlays/TrkDtcFragment.hh"
#include "artdaq-core/Utilities/SimpleLookupPolicy.hh"
#include "artdaq/Generators/GeneratorMacros.hh"
#include "cetlib_except/exception.h"
#include "fhiclcpp/ParameterSet.h"

#include <fstream>
#include <iomanip>
#include <iterator>

#include <unistd.h>

#include "artdaq-core/Data/Fragment.hh"
#include "artdaq/Generators/CommandableFragmentGenerator.hh"
#include "fhiclcpp/fwd.h"

#include "dtcInterfaceLib/DTC.h"
#include "dtcInterfaceLib/DTCSoftwareCFO.h"

#include <atomic>
#include <chrono>
#include <fstream>
#include <iostream>
#include <vector>

using namespace DTCLib;

namespace mu2e {
class CalorimeterVST : public artdaq::CommandableFragmentGenerator {
	enum { kReadDigis = 0, kReadPattern = 1 };
	//-----------------------------------------------------------------------------
	// FHiCL-configurable variables.
	// C++ variable names are the FHiCL parameter names prepended with a "_"
	//-----------------------------------------------------------------------------
	FragmentType const fragment_type_;  // Type of fragment (see FragmentType.hh)

	std::chrono::steady_clock::time_point lastReportTime_;
	std::chrono::steady_clock::time_point procStartTime_;
	DTCLib::DTC_SimMode                   _sim_mode;
	uint8_t                               _board_id;
	std::vector<uint16_t>                 _fragment_ids;  // handled by CommandableGenerator, but not a data member there
	bool                                  simFileRead_;
	bool                                  _loadSimFile;
	std::string                           _simFileName;
	bool                                  _rawOutputEnable;
	std::string                           rawOutputFile_;
	std::ofstream                         rawOutputStream_;
	bool                                  sendEmpties_;
	int                                   _debugLevel;
	size_t                                _nEventsDbg;
	size_t                                _request_delay;
	size_t                                _heartbeatsAfter;
	int                                   _heartbeatInterval;
	int                                   _dtcId;
	std::vector<int>                      _activeLinks;  // active links - connected ROCs
	uint                                  _rocMask;
	int                                   _sleepTimeDMA;       // sleep time (us) after DMA release
	int                                   _sleepTimeDTC;       // sleep time (ms) after register writes
	int                                   _sleepTimeROC;       // sleep time (ms) after ROC register writes
	int                                   _sleepTimeROCReset;  // sleep time (ms) after ROC reset register write

	int _readData;            // 1: read data, 0: save empty fragment
	int _resetROC;            // 1: reset ROC every event
	int _saveDTCRegisters;    //
	int _saveSPI;             //
	int _printFreq;           // printout frequency
	int _maxEventsPerSubrun;  //
	int _readoutMode;         // 0:digis; 1:ROC pattern; 101:simulate data internally, DTC not
	                          // used; default:0

	DTCLib::DTC*            _dtc;
	mu2edev*                _device;
	DTCLib::DTCSoftwareCFO* _cfo;
	// 6 ROCs per DTC max
	int _nActiveLinks;

	uint16_t _reg[200];  // DTC registers to be saved
	int      _nreg;      // their number
	                     //-----------------------------------------------------------------------------
	                     // functions
	                     //-----------------------------------------------------------------------------
  public:
	explicit CalorimeterVST(fhicl::ParameterSet const& ps);
	virtual ~CalorimeterVST();

  private:
	// The "getNext_" function is used to implement user-specific
	// functionality; it's a mandatory override of the pure virtual
	// getNext_ function declared in CommandableFragmentGenerator

	bool readEvent(artdaq::FragmentPtrs& output);
	bool simulateEvent(artdaq::FragmentPtrs& output);
	bool getNext_(artdaq::FragmentPtrs& output) override;

	bool sendEmpty_(artdaq::FragmentPtrs& output);

	void start() override {}

	void stopNoMutex() override {}

	void stop() override;

	void readSimFile_(std::string sim_file);

	void print_dtc_registers(DTC* Dtc, const char* Header);
	void printBuffer(const void* ptr, int sz);
	//-----------------------------------------------------------------------------
	// clones of Monica's scripts
	//-----------------------------------------------------------------------------
	void monica_digi_clear(DTC* Dtc, int Link);
	void monica_var_link_config(DTC* Dtc, int Link);
	void monica_var_pattern_config(DTC* Dtc, int Link);

	// read functions

	int readData(artdaq::Fragment* Frag);
	int readDTCRegisters(artdaq::Fragment* Frag, uint16_t* reg, int nreg);

	// Like "getNext_", "fragmentIDs_" is a mandatory override; it
	// returns a vector of the fragment IDs an instance of this class
	// is responsible for (in the case of CalorimeterVST, this is just
	// the fragment_id_ variable declared in the parent
	// CommandableFragmentGenerator class)

	double _timeSinceLastSend() {
		auto now        = std::chrono::steady_clock::now();
		auto deltaw     = std::chrono::duration_cast<std::chrono::duration<double, std::ratio<1>>>(now - lastReportTime_).count();
		lastReportTime_ = now;
		return deltaw;
	}

	void _startProcTimer() { procStartTime_ = std::chrono::steady_clock::now(); }

	//-----------------------------------------------------------------------------
	// - the first one came from the generator template,
	// - the second one - comments in the CommandableFragmentGenerator.hh
	// - the base class provides the one w/o the underscore
	// - the comments seem to have a general confusion
	// do we really need both ?
	//-----------------------------------------------------------------------------
	std::vector<uint16_t>         fragmentIDs_() { return _fragment_ids; }
	virtual std::vector<uint16_t> fragmentIDs() override;

	double _getProcTimerCount() {
		auto now    = std::chrono::steady_clock::now();
		auto deltaw = std::chrono::duration_cast<std::chrono::duration<double, std::ratio<1>>>(now - procStartTime_).count();
		return deltaw;
	}
};
}  // namespace mu2e

//-----------------------------------------------------------------------------
// define allowed fragment types( = ID's)
//-----------------------------------------------------------------------------
std::vector<uint16_t> mu2e::CalorimeterVST::fragmentIDs() {
	std::vector<uint16_t> v;
	v.push_back(0);
	if(_saveDTCRegisters)
		v.push_back(FragmentType::TRKDTC);
	//  if (_saveSPI)          v.push_back(FragmentType::CALSPI);

	return v;
}

//-----------------------------------------------------------------------------
// sim_mode="N" means real DTC
//-----------------------------------------------------------------------------
mu2e::CalorimeterVST::CalorimeterVST(fhicl::ParameterSet const& ps)
    : CommandableFragmentGenerator(ps)
    , fragment_type_(toFragmentType("MU2E"))
    , lastReportTime_(std::chrono::steady_clock::now())
    , _sim_mode(DTCLib::DTC_SimModeConverter::ConvertToSimMode(ps.get<std::string>("sim_mode", "N")))
    , _board_id(static_cast<uint8_t>(ps.get<int>("board_id", 0)))
    , _fragment_ids(ps.get<std::vector<uint16_t>>("fragment_ids", std::vector<uint16_t>()))  //
    , _loadSimFile(ps.get<bool>("loadSimFile"))
    , _simFileName(ps.get<std::string>("simFileName"))
    , _rawOutputEnable(ps.get<bool>("rawOutputEnable"))
    , rawOutputFile_(ps.get<std::string>("raw_output_file", "/tmp/CalorimeterVST.bin"))
    , sendEmpties_(ps.get<bool>("sendEmpties"))
    , _debugLevel(ps.get<int>("debugLevel", 0))
    , _nEventsDbg(ps.get<size_t>("nEventsDbg", 100))
    , _request_delay(ps.get<size_t>("delay_between_requests_ticks", 20000))
    , _heartbeatsAfter(ps.get<size_t>("heartbeatsAfter"))
    , _heartbeatInterval(ps.get<int>("heartbeatInterval"))
    , _dtcId(ps.get<int>("dtcId", -1))
    , _activeLinks(ps.get<std::vector<int>>("activeLinks"))
    , _sleepTimeDMA(ps.get<int>("sleepTimeDMA", 100))                // 100 microseconds
    , _sleepTimeDTC(ps.get<int>("sleepTimeDTC", 200))                // 200 microseconds
    , _sleepTimeROC(ps.get<int>("sleepTimeROC", 2500))               // 2.5 milliseconds
    , _sleepTimeROCReset(ps.get<int>("sleepTimeROCReset", 4000))     // 4.0 milliseconds
    , _readData(ps.get<int>("readData", 1))                          //
    , _resetROC(ps.get<int>("resetROC", 1))                          //
    , _saveDTCRegisters(ps.get<int>("saveDTCRegisters", 1))          //
    , _saveSPI(ps.get<int>("saveSPI", 1))                            //
    , _printFreq(ps.get<int>("printFreq", 100))                      //
    , _maxEventsPerSubrun(ps.get<int>("maxEventsPerSubrun", 10000))  //
    , _readoutMode(ps.get<int>("readoutMode", 1))                    //

{
	TLOG(TLVL_INFO) << "CalorimeterVST_generator CONSTRUCTOR (1) readData:" << _readData;
	printf("CalorimeterVST::CalorimeterVST readData=%i\n", _readData);
	//-----------------------------------------------------------------------------
	// _sim_mode can still be overridden by environment var $DTCLIB_SIM_ENABLE
	// the sim mode conversion is non-trivial
	//-----------------------------------------------------------------------------
	_nActiveLinks = _activeLinks.size();
	_rocMask      = 0;
	for(int i = 0; i < _nActiveLinks; i++) {
		int link = _activeLinks[i];
		_rocMask |= (1 << 4 * link);
	}
	//-----------------------------------------------------------------------------
	// to take the first step, assume that n(links) = 1
	//-----------------------------------------------------------------------------
	// assert(_nActiveLinks == 1);

	_dtc      = new DTC(_sim_mode, _dtcId, _rocMask, "", false, _simFileName);
	_sim_mode = _dtc->GetSimMode();

	_device = _dtc->GetDevice();

	TLOG(TLVL_INFO) << "The DTC Firmware version string is: " << _dtc->ReadDesignVersion();
	// TLOG(TLVL_INFO) << "Fragment IDs: " << _fragment_ids;

	fhicl::ParameterSet cfoConfig = ps.get<fhicl::ParameterSet>("cfo_config", fhicl::ParameterSet());

	bool          use_dtc_cfo_emulator = cfoConfig.get<bool>("use_dtc_cfo_emulator", true);
	size_t        debug_packet_count   = cfoConfig.get<size_t>("debug_packet_count", 0);
	std::string   dbtype               = cfoConfig.get<std::string>("debug_type", "DTC_DebugType_SpecialSequence");  // default was set to "2"
	DTC_DebugType debug_type           = DTC_DebugTypeConverter::ConvertToDebugType(dbtype);
	bool          sticky_debug_type    = cfoConfig.get<bool>("sticky_debug_type", true);  // what is this ?
	bool          quiet                = cfoConfig.get<bool>("quiet", true);
	bool          asyncRR              = cfoConfig.get<bool>("asyncRR", true);              // default was false
	bool          force_no_debug_mode  = cfoConfig.get<bool>("force_no_debug_mode", true);  // sets a bit written to the DTC register
	bool          useCFODRP            = cfoConfig.get<bool>("useCFODRP", false);           // defaults to false

	_cfo = new DTCSoftwareCFO(_dtc, use_dtc_cfo_emulator, debug_packet_count, debug_type, sticky_debug_type, quiet, asyncRR, force_no_debug_mode, useCFODRP);
	//-----------------------------------------------------------------------------
	// not sure I fully understand the logic below
	//-----------------------------------------------------------------------------
	if(_loadSimFile) {
		_dtc->SetDetectorEmulatorInUse();
		_dtc->ResetDDR();
		_dtc->SoftReset();

		if(_simFileName.size() > 0) {
			simFileRead_ = false;
			std::thread reader(&mu2e::CalorimeterVST::readSimFile_, this, _simFileName);
			reader.detach();
		}
	} else {
		_dtc->ClearDetectorEmulatorInUse();  // Needed if we're doing ROC Emulator...
		                                     // make sure Detector Emulation is disabled
		simFileRead_ = true;
	}
	if(_rawOutputEnable)
		rawOutputStream_.open(rawOutputFile_, std::ios::out | std::ios::app | std::ios::binary);
	//-----------------------------------------------------------------------------
	// do it once anyway, the next two lines - DTC soft reset
	// my_cntl write 0x9100 0x80000000 > /dev/null
	// my_cntl write 0x9100 0x00008000 > /dev/nul
	//-----------------------------------------------------------------------------
	// _dtc->GetDevice()->write_register(0x9100,100,0x80000000);
	// std::this_thread::sleep_for(std::chrono::microseconds(_sleepTimeDTC));
	// _dtc->GetDevice()->write_register(0x9100,100,0x00008000);
	// std::this_thread::sleep_for(std::chrono::microseconds(_sleepTimeDTC));

	for(int i = 0; i < _nActiveLinks; i++) {
		monica_digi_clear(_dtc, _activeLinks[i]);

		if(_readoutMode == kReadDigis)
			monica_var_link_config(_dtc, _activeLinks[i]);
		else if(_readoutMode == kReadPattern)
			monica_var_pattern_config(_dtc, _activeLinks[i]);
	}
	//-----------------------------------------------------------------------------
	// DTC registers to save - zero those labeled as counters at begin run !
	//-----------------------------------------------------------------------------
	uint16_t reg[] = {0x9004,
	                  0,
	                  0x9100,
	                  0,
	                  0x9140,
	                  0,
	                  0x9144,
	                  0,
	                  0x9158,
	                  0,
	                  0x9188,
	                  0,
	                  0x9190,
	                  1,  // bits 7-0 could be ignored,
	                  0x9194,
	                  1,  // bits 23-16 could be ignored
	                  0x9198,
	                  1,
	                  0x919c,
	                  1,
	                  0x91a8,
	                  0,
	                  0x91ac,
	                  0,
	                  0x91bc,
	                  0,
	                  0x91c0,
	                  0,
	                  0x91c4,
	                  0,
	                  0x91c8,
	                  0,
	                  0x91f4,
	                  0,
	                  0x91f8,
	                  0,
	                  0x9374,
	                  1,
	                  0x9378,
	                  1,
	                  0x9380,
	                  1,  // Link 0 Error Flags
	                  0x9384,
	                  1,  // Link 1 Error Flags
	                  0x9388,
	                  1,  // Link 2 Error Flags
	                  0x93b0,
	                  1,
	                  0x93b4,
	                  1,
	                  0x93b8,
	                  1,
	                  0x93d0,
	                  1,  // CFO Link EventStart Character Error Count
	                  0x93d8,
	                  1,  // Input Buffer Fragment Dump Count
	                  0x93dc,
	                  1,  //  Output Buffer Fragment Dump Count
	                  0x93e0,
	                  0,
	                  0x9560,
	                  1,  // SERDES RX CRC Error Count Link 0
	                  0x9564,
	                  1,  // SERDES RX CRC Error Count Link 1
	                  0x9568,
	                  1,  // SERDES RX CRC Error Count Link 2
	                  0x9630,
	                  1,  // TX Data Request Packet Count Link 0
	                  0x9634,
	                  1,  // TX Data Request Packet Count Link 1
	                  0x9638,
	                  1,  // TX Data Request Packet Count Link 2
	                  0x9650,
	                  1,  // TX Heartbeat Packet Count Link 0
	                  0x9654,
	                  1,  // TX Heartbeat Packet Count Link 1
	                  0x9658,
	                  1,  // TX Heartbeat Packet Count Link 2
	                  0x9670,
	                  1,  // RX Data Header Packet Count Link 0
	                  0x9674,
	                  1,  // RX Data Header Packet Count Link 1
	                  0x9678,
	                  1,  // RX Data Header Packet Count Link 2
	                  0x9690,
	                  1,  // RX Data Packet Count Link 0                             //  link 2 reset -
	                      // write 0
	                  0x9694,
	                  1,  // RX Data Packet Count Link 1                             //  link 2 reset -
	                      // write 0
	                  0x9698,
	                  1,  // RX Data Packet Count Link 2                             //  link 2 reset -
	                      // write 0
	                  //-----------------------------------------------------------------------------
	                  // 2023-09-14 - new counters by Rick - in DTC2023Sep02_22_22.1
	                  //-----------------------------------------------------------------------------
	                  0x9720,
	                  1,  // rxlink0
	                  0x9724,
	                  1,  // rxlink1
	                  0x9728,
	                  1,  // rxlink2
	                  0x972C,
	                  1,  // rxlink3
	                  0x9730,
	                  1,  // rxlink4
	                  0x9734,
	                  1,  // rxlink5
	                  0x9740,
	                  1,  // rxinputbufferin
	                  0x9744,
	                  1,  // DDRWrite
	                  0x9748,
	                  1,  // DDRRead
	                  0x974C,
	                  1,  // DMAtoPCI

	                  0xffff};

	_nreg = 0;
	int i = 0;
	do {
		ushort r    = reg[2 * i];
		int    flag = reg[2 * i + 1];
		//-----------------------------------------------------------------------------
		// flag=1 : a counter, reset counters
		// according to Rick, to clear a counter one writes 0xffffffff to it
		//-----------------------------------------------------------------------------
		if(flag != 0)
			_dtc->GetDevice()->write_register(r, 100, 0xffffffff);
		_reg[_nreg] = r;
		_nreg += 1;
		i += 1;
	} while(reg[2 * i] != 0xffff);

	TLOG(TLVL_INFO) << "N DTC registers to save: " << _nreg;
}

//-----------------------------------------------------------------------------
void mu2e::CalorimeterVST::readSimFile_(std::string sim_file) {
	TLOG(TLVL_INFO) << "Starting read of simulation file " << sim_file << "."
	                << " Please wait to start the run until finished.";

	_dtc->WriteSimFileToDTC(sim_file, true, true);
	simFileRead_ = true;
	TLOG(TLVL_INFO) << "Done reading simulation file into DTC memory.";
}

//-----------------------------------------------------------------------------
mu2e::CalorimeterVST::~CalorimeterVST() {
	rawOutputStream_.close();
	delete _cfo;
	delete _dtc;
}

//-----------------------------------------------------------------------------
void mu2e::CalorimeterVST::stop() {
	_dtc->DisableDetectorEmulator();
	_dtc->DisableCFOEmulation();
}

//-----------------------------------------------------------------------------
void mu2e::CalorimeterVST::print_dtc_registers(DTC* Dtc, const char* Header) {
	printf("---------------------- %s : DTC status :\n", Header);
	uint32_t res;
	int      rc;
	rc = _device->read_register(0x9100, 100, &res);
	printf("DTC status       : 0x%08x rc:%i\n", res, rc);  // expect: 0x40808404
	rc = _device->read_register(0x91c8, 100, &res);
	printf("debug packet type: 0x%08x rc:%i\n", res, rc);  // expect: 0x00000000
}

//-----------------------------------------------------------------------------
void mu2e::CalorimeterVST::monica_digi_clear(DTCLib::DTC* Dtc, int Link) {
	//-----------------------------------------------------------------------------
	//  Monica's digi_clear
	//  this will proceed in 3 steps each for HV and CAL DIGIs:
	// 1) pass TWI address and data toTWI controller (fiber is enabled by default)
	// 2) write TWI INIT high
	// 3) write TWI INIT low
	//-----------------------------------------------------------------------------
	auto link = DTC_Link_ID(Link);

	Dtc->WriteROCRegister(link, 28, 0x10, false, 1000);  //

	// Writing 0 & 1 to  address=16 for HV DIGIs ???

	Dtc->WriteROCRegister(link, 27, 0x00, false, 1000);  // write 0
	Dtc->WriteROCRegister(link, 26, 0x01, false, 1000);  // toggle INIT
	Dtc->WriteROCRegister(link, 26, 0x00, false, 1000);  //

	Dtc->WriteROCRegister(link, 27, 0x01, false, 1000);  //  write 1
	Dtc->WriteROCRegister(link, 26, 0x01, false, 1000);  //  toggle INIT
	Dtc->WriteROCRegister(link, 26, 0x00, false, 1000);  //

	// echo "Writing 0 & 1 to  address=16 for CAL DIGIs"
	Dtc->WriteROCRegister(link, 25, 0x10, false, 1000);  //

	Dtc->WriteROCRegister(link, 24, 0x00, false, 1000);  // write 0
	Dtc->WriteROCRegister(link, 23, 0x01, false, 1000);  // toggle INIT
	Dtc->WriteROCRegister(link, 23, 0x00, false, 1000);  //

	Dtc->WriteROCRegister(link, 24, 0x01, false, 1000);  // write 1
	Dtc->WriteROCRegister(link, 23, 0x01, false, 1000);  // toggle INIT
	Dtc->WriteROCRegister(link, 23, 0x00, false, 1000);  //
}

//-----------------------------------------------------------------------------
void mu2e::CalorimeterVST::monica_var_link_config(DTCLib::DTC* Dtc, int Link) {
	mu2edev* dev = Dtc->GetDevice();

	dev->write_register(0x91a8, 100, 0);  // disable event window marker - set deltaT = 0
	std::this_thread::sleep_for(std::chrono::microseconds(_sleepTimeDTC));

	auto link = DTC_Link_ID(Link);
	Dtc->WriteROCRegister(link, 14, 1, false, 1000);  // reset ROC
	std::this_thread::sleep_for(std::chrono::microseconds(_sleepTimeROCReset));

	Dtc->WriteROCRegister(link, 8, 0x030f, false, 1000);  // configure ROC to read all 4 lanes
	std::this_thread::sleep_for(std::chrono::microseconds(_sleepTimeROC));

	// added register for selecting kind of data to report in DTC status bits
	// Use with pattern data. Set to zero, ie STATUS=0x55, when taking DIGI data
	// rocUtil -a 30  -w 0  -l $LINK write_register > /dev/null

	Dtc->WriteROCRegister(link, 30, 0x0000, false, 1000);  // configure ROC to read all 4 lanes
	std::this_thread::sleep_for(std::chrono::microseconds(_sleepTimeROC));

	// echo "Setting packet format version to 1"
	// rocUtil -a 29  -w 1  -l $LINK write_register > /dev/null

	Dtc->WriteROCRegister(link, 29, 0x0001, false, 1000);  // configure ROC to read all 4 lanes
	std::this_thread::sleep_for(std::chrono::microseconds(_sleepTimeROC));
}

//-----------------------------------------------------------------------------
void mu2e::CalorimeterVST::monica_var_pattern_config(DTC* Dtc, int Link) {
	TLOG(TLVL_DEBUG) << "---------------------------------- operation \"var_patern_config\"" << std::endl;

	//  auto startTime = std::chrono::steady_clock::now();

	if(_debugLevel > 0)
		print_dtc_registers(Dtc, "var_pattern_config 001");  // debug

	mu2edev* dev = Dtc->GetDevice();

	dev->ResetDeviceTime();

	dev->write_register(0x91a8, 100, 0);  // disable event window marker - set deltaT = 0
	std::this_thread::sleep_for(std::chrono::microseconds(_sleepTimeDTC));

	int tmo_ms(1500);  // 1500 is OK

	auto link = DTC_Link_ID(Link);
	Dtc->WriteROCRegister(link, 14, 0x01, false, tmo_ms);  // this seems to be a reset
	std::this_thread::sleep_for(std::chrono::microseconds(_sleepTimeROCReset));

	Dtc->WriteROCRegister(link, 8, 0x10, false, tmo_ms);
	std::this_thread::sleep_for(std::chrono::microseconds(_sleepTimeROC));

	Dtc->WriteROCRegister(link, 30, 0x00, false, tmo_ms);
	std::this_thread::sleep_for(std::chrono::microseconds(_sleepTimeROC));

	if(_debugLevel > 0)
		print_dtc_registers(Dtc, "var_pattern_config 002");  // debug
}

//-----------------------------------------------------------------------------
// print 16 bytes per line
// size - number of bytes to print, even
//-----------------------------------------------------------------------------
void mu2e::CalorimeterVST::printBuffer(const void* ptr, int sz) {
	int     nw  = sz / 2;
	ushort* p16 = (ushort*)ptr;
	int     n   = 0;

	for(int i = 0; i < nw; i++) {
		if(n == 0)
			printf(" 0x%08x: ", i * 2);

		ushort word = p16[i];
		printf("0x%04x ", word);

		n += 1;
		if(n == 8) {
			printf("\n");
			n = 0;
		}
	}

	if(n != 0)
		printf("\n");
}

//-----------------------------------------------------------------------------
// read SPI data from ROC, in 16-bit words
// decoding doesn't belong here
// links to be handled
//-----------------------------------------------------------------------------
int mu2e::CalorimeterVST::readDTCRegisters(artdaq::Fragment* Frag, uint16_t* Reg, int NReg) {
	int rc(0);

	Frag->resizeBytes(NReg * sizeof(TrkDtcFragment::RegEntry));
	uint* f2d = (uint*)Frag->dataBegin();

	for(int i = 0; i < NReg; i++) {
		f2d[2 * i] = Reg[i];
		try {
			rc = _dtc->GetDevice()->read_register(Reg[i], 100, f2d + 2 * i + 1);
		} catch(...) {
			TLOG(TLVL_ERROR) << "event: " << ev_counter() << "readDTCRegisters ERROR, register : " << Reg[i];
			break;
		}
	}

	return rc;
}

//-----------------------------------------------------------------------------
int mu2e::CalorimeterVST::readData(artdaq::Fragment* Frag) {
	//-----------------------------------------------------------------------------
	// the body of the readDTCBuffer
	//-----------------------------------------------------------------------------
	int  rc(-1);  // return code, negative if error
	bool timeout(false);
	bool readSuccess(false);
	auto tmo_ms(1500);

	mu2e_databuff_t* buffer;
	size_t           nbytes = _dtc->GetDevice()->read_data(DTC_DMA_Engine_DAQ, reinterpret_cast<void**>(&buffer), tmo_ms);
	std::this_thread::sleep_for(std::chrono::microseconds(_sleepTimeDTC));

	if(nbytes > 0) {
		readSuccess      = true;
		void*    readPtr = &buffer[0];
		uint16_t bufSize = static_cast<uint16_t>(*static_cast<uint64_t*>(readPtr));
		readPtr          = static_cast<uint8_t*>(readPtr) + 8;

		TLOG(TLVL_DEBUG) << "event: " << ev_counter() << "bufSize: " << std::dec << bufSize << " nbytes: " << nbytes;

		timeout = false;

		if(nbytes > sizeof(DTC_EventHeader) + sizeof(DTC_SubEventHeader) + 8) {
			//-----------------------------------------------------------------------------
			// I suspect this is a check of any useful data being present. although not
			// sure what 8 stands for. Also not sure about the meaning of the checks
			// performed Check for dead or cafe in first packet
			//-----------------------------------------------------------------------------
			readPtr = static_cast<uint8_t*>(readPtr) + sizeof(DTC_EventHeader) + sizeof(DTC_SubEventHeader);
			std::vector<size_t> wordsToCheck{1, 2, 3, 7, 8};
			for(auto& word : wordsToCheck) {
				auto wordPtr = static_cast<uint16_t*>(readPtr) + (word - 1);
				TLOG(TLVL_DEBUG) << word << (word == 1 ? "st" : word == 2 ? "nd" : word == 3 ? "rd" : "th") << " *wordPtr: 0x" << std::hex << *wordPtr;

				if(*wordPtr == 0xcafe || *wordPtr == 0xdead) {
					TLOG(TLVL_WARNING) << "Buffer Timeout detected! " << word << (word == 1 ? "st" : word == 2 ? "nd" : word == 3 ? "rd" : "th") << " *wordPtr 0x" << std::hex << *wordPtr;
					DTCLib::Utilities::PrintBuffer(readPtr, 16, 0, TLVL_DEBUG);
					timeout = true;
					break;
				}
			}
		}
	}

	int print_event = (ev_counter() % _printFreq) == 0;
	if(print_event) {
		printf(" event: %10lu readSuccess : %1i timeout: %1i nbytes: %10lu\n", ev_counter(), readSuccess, timeout, nbytes);
	}

	if((_debugLevel > 0) and (ev_counter() < _nEventsDbg)) {
		// print_roc_registers(&dtc,link,"002 [after readDTCBuffer]");
		DTCLib::Utilities::PrintBuffer(buffer, nbytes, 0);
	}
	//-----------------------------------------------------------------------------
	// first 0x40 bytes seem to be useless, they are followed by the data header packer,
	// offline starts from there
	// suspect the offset is the size of the (DTC_EventHeader + DTC_SubEventHeader)
	// UPDATE: the latest version of the Mu2e interface assumes that
	//         the event and subevent headers are not dropped
	//-----------------------------------------------------------------------------
	Frag->resizeBytes(nbytes);

	char* cbuf = (char*)buffer;
	void* afd  = Frag->dataBegin();

	if(nbytes > 0)
		memcpy(afd, cbuf, nbytes);
	//-----------------------------------------------------------------------------
	// artdaq fragment starts from the data header packet
	// do we still need to set version ? ADC bit ordering ?
	//-----------------------------------------------------------------------------
	if(nbytes >= 0x50) {
		//-----------------------------------------------------------------------------
		// copy data and patch format version - set it to 1
		//-----------------------------------------------------------------------------
		struct DataHeaderPacket_t {
			uint16_t nBytes;
			uint16_t w2;
			uint16_t nPackets;
			uint16_t evtWindowTag[3];
			uint16_t w7;
			uint16_t w8;

			void setVersion(int version) { w7 = (w7 & 0x00ff) + ((version & 0xff) << 8); }
			void setStatus(int status) { w7 = (w7 & 0xff00) + (status & 0xff); }
		};

		DataHeaderPacket_t* dhp = (DataHeaderPacket_t*)((char*)afd + 0x40);
		dhp->setVersion(1);
	} else {
		//-----------------------------------------------------------------------------
		// ERROR: data size < (event_header+subevent_header+data_packet_header)
		// generate diagnostics
		//-----------------------------------------------------------------------------
		TLOG(TLVL_ERROR) << "event: " << ev_counter() << " read length ERROR: " << nbytes;
		rc = -1;
	}
	return rc;
}

//-----------------------------------------------------------------------------
bool mu2e::CalorimeterVST::readEvent(artdaq::FragmentPtrs& Frags) {
	//-----------------------------------------------------------------------------
	// read data
	// Monica's way of resetting the DTC and the ROC - so far, assume just one ROC
	// having reset the ROC, go back to the requested time window
	//-----------------------------------------------------------------------------
	if(_resetROC) {
		for(int i = 0; i < _nActiveLinks; i++) {
			monica_digi_clear(_dtc, _activeLinks[i]);
			// monica_var_link_config(_dtc,_activeLinks[i]);
		}
		_dtc->GetDevice()->write_register(0x91a8, 100, _heartbeatInterval);
	}
	//-----------------------------------------------------------------------------
	// send a request for one event, what is the role of requestsAhead ?
	//-----------------------------------------------------------------------------
	uint64_t           z(0);
	DTC_EventWindowTag zero(z);
	bool               incrementTimestamp(true);
	int                requestsAhead(0);

	int nev = 1;
	//-----------------------------------------------------------------------------
	// duplicating setting the distance btw the two event window markers
	//-----------------------------------------------------------------------------
	_cfo->SendRequestsForRange(nev, DTC_EventWindowTag(ev_counter()), incrementTimestamp, _heartbeatInterval, requestsAhead, _heartbeatsAfter);

	std::this_thread::sleep_for(std::chrono::microseconds(_sleepTimeDTC));

	_dtc->GetDevice()->ResetDeviceTime();
	//-----------------------------------------------------------------------------
	// a hack : reduce the PMT logfile size
	//-----------------------------------------------------------------------------
	int print_event = (ev_counter() % _printFreq) == 0;

	if(print_event) {
		// print_roc_registers(&_dtc,link,"001 [after cfo.SendRequestForRange]");
		printf(" ------------------------------ reading event %li\n", ev_counter());
	}
	//-----------------------------------------------------------------------------
	// make sure even a fake fragment goes in
	//-----------------------------------------------------------------------------
	double            tstamp = ev_counter();
	artdaq::Fragment* f1     = new artdaq::Fragment(ev_counter(), _fragment_ids[0], FragmentType::CAL, tstamp);
	if(_readData) {
		//-----------------------------------------------------------------------------
		// read data
		//-----------------------------------------------------------------------------
		readData(f1);
	} else {
		//-----------------------------------------------------------------------------
		// fake reading
		//-----------------------------------------------------------------------------
		f1->resizeBytes(4);
		uint* afd = (uint*)f1->dataBegin();
		*afd      = 0x00ffffff;
		// printf("%s: fake data\n",__func__);
		// printBuffer(f1->dataBegin(),4);
	}
	Frags.emplace_back(f1);
	//-----------------------------------------------------------------------------
	// read DTC registers
	// add one more fragment of debug type with the diagnostics registers: 8 bytes per
	// register - (register number, value) for simplicity, keep both 4-byte integers
	//-----------------------------------------------------------------------------
	artdaq::Fragment* f2       = new artdaq::Fragment(ev_counter(), _fragment_ids[1], FragmentType::TRKDTC, tstamp);
	auto              metadata = TrkDtcFragment::create_metadata();
	f2->setMetadata(metadata);
	if(_saveDTCRegisters) {
		readDTCRegisters(f2, _reg, _nreg);
		if((_debugLevel > 0) and (ev_counter() < _nEventsDbg)) {
			printf("%s: DTC registers\n", __func__);
			int nb_dtc = 4 + 8 * _nreg;
			printBuffer(f2->dataBegin(), nb_dtc);
		}
	}
	Frags.emplace_back(f2);
	//-----------------------------------------------------------------------------
	// release the DMA channel
	//-----------------------------------------------------------------------------
	_dtc->GetDevice()->read_release(DTC_DMA_Engine_DAQ, 1);

	if(_sleepTimeDMA > 0) {
		std::this_thread::sleep_for(std::chrono::microseconds(_sleepTimeDMA));
	}

	if((_debugLevel > 0) and (ev_counter() < _nEventsDbg)) {
		int      rc;
		uint32_t res;
		rc = _dtc->GetDevice()->read_register(0x9100, 100, &res);
		printf("DTC status       : 0x%08x rc:%i\n", res, rc);  // expect: 0x40808404
		rc = _dtc->GetDevice()->read_register(0x91c8, 100, &res);
		printf("debug packet type: 0x%08x rc:%i\n", res, rc);  // expect: 0x00000000
	}

	_dtc->GetDevice()->release_all(DTC_DMA_Engine_DAQ);
	return true;
}

//-----------------------------------------------------------------------------
bool mu2e::CalorimeterVST::simulateEvent(artdaq::FragmentPtrs& Frags) {
	double            tstamp = ev_counter();
	artdaq::Fragment* frag   = new artdaq::Fragment(ev_counter(), _fragment_ids[0], FragmentType::CAL, tstamp);

	const uint16_t fake_event[] = {
	    0x01d0, 0x0000, 0x0000, 0x0000, 0x01c8, 0x0000, 0x0169, 0x0000,  // 0x000000:
	    0x0000, 0x0101, 0x0000, 0x0000, 0x0000, 0x0000, 0x0100, 0x0000,  // 0x000010:
	    0x01b0, 0x0000, 0x0169, 0x0000, 0x0000, 0x0101, 0x0000, 0x0000,  // 0x000020:
	    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x01ee,  // 0x000030:
	    0x0190, 0x8150, 0x0018, 0x0169, 0x0000, 0x0000, 0x0155, 0x0000,  // 0x000040:
	    0x005b, 0x858d, 0x1408, 0x8560, 0x0408, 0x0041, 0xa955, 0x155a,  // 0x000050:
	    0x56aa, 0x2aa5, 0xa955, 0x155a, 0x56aa, 0x2aa5, 0xa955, 0x155a,  // 0x000060:
	    0x005b, 0x548e, 0x1415, 0x5462, 0x0415, 0x0041, 0xa955, 0x155a,  // 0x000070:
	    0x56aa, 0x2aa5, 0xa955, 0x155a, 0x56aa, 0x2aa5, 0xa955, 0x155a,  // 0x000080:
	    0x005b, 0x2393, 0x1422, 0x2362, 0x0422, 0x0041, 0xa955, 0x155a,  // 0x000090:
	    0x56aa, 0x2aa5, 0xa955, 0x155a, 0x56aa, 0x2aa5, 0xa955, 0x155a,  // 0x0000a0:
	    0x002a, 0x859a, 0x1408, 0x85b2, 0x0408, 0x0041, 0x56aa, 0x2aa5,  // 0x0000b0:
	    0xa955, 0x155a, 0x56aa, 0x2aa5, 0xa955, 0x155a, 0x56aa, 0x2aa5,  // 0x0000c0:
	    0x002a, 0x549a, 0x1415, 0x54b5, 0x0415, 0x0041, 0x56aa, 0x2aa5,  // 0x0000d0:
	    0xa955, 0x155a, 0x56aa, 0x2aa5, 0xa955, 0x155a, 0x56aa, 0x2aa5,  // 0x0000e0:
	    0x002a, 0x239c, 0x1422, 0x23b5, 0x0422, 0x0041, 0x56aa, 0x2aa5,  // 0x0000f0:
	    0xa955, 0x155a, 0x56aa, 0x2aa5, 0xa955, 0x155a, 0x56aa, 0x2aa5,  // 0x000100:
	    0x00de, 0xca6a, 0x1400, 0xca5c, 0x0400, 0x0041, 0x56aa, 0x2aa5,  // 0x000110:
	    0xa955, 0x155a, 0x56aa, 0x2aa5, 0xa955, 0x155a, 0x56aa, 0x2aa5,  // 0x000120:
	    0x00de, 0x996a, 0x140d, 0x995c, 0x040d, 0x0041, 0x56aa, 0x2aa5,  // 0x000130:
	    0xa955, 0x155a, 0x56aa, 0x2aa5, 0xa955, 0x155a, 0x56aa, 0x2aa5,  // 0x000140:
	    0x00de, 0x686c, 0x141a, 0x685d, 0x041a, 0x0041, 0xa955, 0x155a,  // 0x000150:
	    0x56aa, 0x2aa5, 0xa955, 0x155a, 0x56aa, 0x2aa5, 0xa955, 0x155a,  // 0x000160:
	    0x00ac, 0xc90d, 0x1500, 0xcabf, 0x0400, 0x0041, 0xa955, 0x155a,  // 0x000170:
	    0x56aa, 0x2aa5, 0xa955, 0x155a, 0x56aa, 0x2aa5, 0xa955, 0x155a,  // 0x000180:
	    0x00ac, 0x980d, 0x150d, 0x99c5, 0x040d, 0x0041, 0x56aa, 0x2aa5,  // 0x000190:
	    0xa955, 0x155a, 0x56aa, 0x2aa5, 0xa955, 0x155a, 0x56aa, 0x2aa5,  // 0x0001a0:
	    0x00ac, 0x670d, 0x151a, 0x68c5, 0x041a, 0x0041, 0x56aa, 0x2aa5,  // 0x0001b0:
	    0xa955, 0x155a, 0x56aa, 0x2aa5, 0xa955, 0x155a, 0x56aa, 0x2aa5   // 0x0001c0:
	};

	int nb = 0x1d0;
	frag->resizeBytes(nb);

	uint* afd = (uint*)frag->dataBegin();
	memcpy(afd, fake_event, nb);

	// printf("%s: fake data\n",__func__);
	// printBuffer(f1->dataBegin(),4);

	Frags.emplace_back(frag);
	return true;
}

//-----------------------------------------------------------------------------
// a virtual function called from the outside world
//-----------------------------------------------------------------------------
bool mu2e::CalorimeterVST::getNext_(artdaq::FragmentPtrs& Frags) {
	//  const char* oname = "mu2e::CalorimeterVST::getNext_: ";
	bool rc(true);

	TLOG(TLVL_DEBUG) << "event: " << ev_counter() << "STARTING";

	if(should_stop())
		return false;

	_startProcTimer();

	TLOG(TLVL_DEBUG) << "event: " << ev_counter() << "after startProcTimer";
	if(_readoutMode < 100) {
		rc = readEvent(Frags);
	} else {
		//-----------------------------------------------------------------------------
		// increment number of generated events
		//-----------------------------------------------------------------------------
		//    readout mode > 100 : simulation
		//-----------------------------------------------------------------------------
		rc = simulateEvent(Frags);
	}
	//-----------------------------------------------------------------------------
	// increment number of generated events
	//-----------------------------------------------------------------------------
	ev_counter_inc();
	//-----------------------------------------------------------------------------
	// increment the subrun number, if needed
	//-----------------------------------------------------------------------------
	if((ev_counter() % _maxEventsPerSubrun) == 0) {
		artdaq::Fragment* esf = new artdaq::Fragment(1);

		esf->setSystemType(artdaq::Fragment::EndOfSubrunFragmentType);
		esf->setSequenceID(ev_counter() + 1);
		esf->setTimestamp(1 + (ev_counter() / _maxEventsPerSubrun));
		*esf->dataBegin() = 0;
		Frags.emplace_back(esf);
	}

	return rc;
}

//-----------------------------------------------------------------------------
bool mu2e::CalorimeterVST::sendEmpty_(artdaq::FragmentPtrs& Frags) {
	Frags.emplace_back(new artdaq::Fragment());
	Frags.back()->setSystemType(artdaq::Fragment::EmptyFragmentType);
	Frags.back()->setSequenceID(ev_counter());
	Frags.back()->setFragmentID(_fragment_ids[0]);
	ev_counter_inc();
	return true;
}

// The following macro is defined in artdaq's GeneratorMacros.hh header
DEFINE_ARTDAQ_COMMANDABLE_GENERATOR(mu2e::CalorimeterVST)
