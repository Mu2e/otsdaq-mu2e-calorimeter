#include "artdaq-mu2e/Generators/Mu2eEventReceiverBase.hh"
#include "artdaq/Generators/GeneratorMacros.hh"
#include "trace.h"

#include "artdaq/DAQdata/Globals.hh"
#include "canvas/Utilities/Exception.h"

#include "artdaq-core-mu2e/Overlays/FragmentType.hh"
#include "artdaq-core-mu2e/Overlays/TrkDtcFragment.hh"
#include "artdaq-core/Utilities/SimpleLookupPolicy.hh"
#include "artdaq/Generators/GeneratorMacros.hh"
#include "cetlib_except/exception.h"
#include "fhiclcpp/ParameterSet.h"

#include "artdaq-core/Data/Fragment.hh"
#include "artdaq/Generators/CommandableFragmentGenerator.hh"
#include "fhiclcpp/fwd.h"

#define TRACE_NAME "Mu2eCaloEventReceiver"

namespace mu2e {
class Mu2eCaloEventReceiver : public mu2e::Mu2eEventReceiverBase {
	std::vector<uint16_t> _fragment_ids;
	DTCLib::DTC_SimMode   _sim_mode;

  public:
	explicit Mu2eCaloEventReceiver(fhicl::ParameterSet const& ps);
	virtual ~Mu2eCaloEventReceiver();

  private:
	// The "getNext_" function is used to implement user-specific
	// functionality; it's a mandatory override of the pure virtual
	// getNext_ function declared in CommandableFragmentGenerator

	bool getNext_(artdaq::FragmentPtrs& output) override;
	bool simulateEvent(artdaq::FragmentPtrs& output);
	bool sendEmpty_(artdaq::FragmentPtrs& output);
};
}  // namespace mu2e

mu2e::Mu2eCaloEventReceiver::Mu2eCaloEventReceiver(fhicl::ParameterSet const& ps)
    : Mu2eEventReceiverBase(ps)
    , _fragment_ids(ps.get<std::vector<uint16_t>>("fragment_ids", std::vector<uint16_t>()))
    , _sim_mode(DTCLib::DTC_SimModeConverter::ConvertToSimMode(ps.get<std::string>("sim_mode", "N"))) {
	TLOG(TLVL_DEBUG) << "Mu2eCaloEventReceiver Initialized with mode " << mode_;
	std::cout << "init Mu2eCaloEventRecevier" << std::endl;
}

mu2e::Mu2eCaloEventReceiver::~Mu2eCaloEventReceiver() {}

bool mu2e::Mu2eCaloEventReceiver::getNext_(artdaq::FragmentPtrs& frags) {
	bool rc(true);
	std::cout << "[Mu2eCaloEventReceiver:getNext] getting next event" << std::endl;
	while(!simFileRead_ && !should_stop()) {
		std::cout << "[Mu2eCaloEventReceiver:getNext]  sleeping" << std::endl;
		usleep(5000);
	}

	if(should_stop()) {
		std::cout << "[Mu2eCaloEventReceiver:getNext]  should stop" << std::endl;
		return false;
	}

	rc = simulateEvent(frags);

	//-----------------------------------------------------------------------------
	// increment number of generated events
	//-----------------------------------------------------------------------------
	ev_counter_inc();
	//-----------------------------------------------------------------------------
	// increment the subrun number, if needed
	//-----------------------------------------------------------------------------
	if((ev_counter() % 10) == 0) {
		artdaq::Fragment* esf = new artdaq::Fragment(1);
		std::cout << "[Mu2eCaloEventReceiver:getNext]  setting end of sub-run type" << std::endl;
		esf->setSystemType(artdaq::Fragment::EndOfSubrunFragmentType);
		esf->setSequenceID(ev_counter() + 1);
		esf->setTimestamp(1 + (ev_counter() / 1));
		*esf->dataBegin() = 0;
		frags.emplace_back(esf);
	}

	std::cout << "[Mu2eCaloEventReceiver:getNext]  returning next event" << std::endl;
	return rc;
}

//-----------------------------------------------------------------------------
bool mu2e::Mu2eCaloEventReceiver::simulateEvent(artdaq::FragmentPtrs& Frags) {
	double            tstamp = ev_counter();
	artdaq::Fragment* frag   = new artdaq::Fragment(ev_counter(), _fragment_ids[0], FragmentType::CAL, tstamp);
	std::cout << "[Mu2eCaloEventReceiver:simulateEventt]  making fake data" << std::endl;
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
	// printBuffer(frag->dataBegin(),4);

	Frags.emplace_back(frag);
	std::cout << "Mu2eCaloEventReceiver:simulateEventt] returning fake data"
	          << " size " << Frags.size() << std::endl;
	return true;
}
//-----------------------------------------------------------------------------
bool mu2e::Mu2eCaloEventReceiver::sendEmpty_(artdaq::FragmentPtrs& Frags) {
	std::cout << "[Mu2eCaloEventReceiver:sendEMpty]  making empty frag" << std::endl;
	Frags.emplace_back(new artdaq::Fragment());
	Frags.back()->setSystemType(artdaq::Fragment::EmptyFragmentType);
	Frags.back()->setSequenceID(ev_counter());
	Frags.back()->setFragmentID(_fragment_ids[0]);
	ev_counter_inc();
	std::cout << "[Mu2eCaloEventReceiver:sendEMpty]  finished" << std::endl;
	return true;
}

// The following macro is defined in artdaq's GeneratorMacros.hh header
DEFINE_ARTDAQ_COMMANDABLE_GENERATOR(mu2e::Mu2eCaloEventReceiver)
