#include "art/Framework/Core/EDFilter.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Principal/Event.h"
// #include "art/Framework/Services/Registry/ServiceHandle.h"
#include "art/Framework/Principal/Handle.h"
#include "art/Framework/Principal/Run.h"
#include "messagefacility/MessageLogger/MessageLogger.h"

#include "TRACE/tracemf.h"
#include "artdaq/DAQdata/Globals.hh"
#define TRACE_NAME "CaloHitFilter"

#include "canvas/Utilities/Exception.h"
#include "canvas/Utilities/InputTag.h"

#include <artdaq-core/Data/ContainerFragment.hh>
#include "artdaq-core/Data/Fragment.hh"

#include "artdaq-core-mu2e/Data/EventHeader.hh"
#include "artdaq-core-mu2e/Overlays/DTCEventFragment.hh"
#include "artdaq-core-mu2e/Overlays/Decoders/CalorimeterDataDecoder.hh"
#include "artdaq-core-mu2e/Overlays/FragmentType.hh"

#include "Offline/DAQ/inc/CaloDAQUtilities.hh"

#include "cetlib_except/exception.h"

#include <iomanip>
#include <sstream>
#include <vector>

namespace mu2e {
class CaloHitFilter : public art::EDFilter {
  public:
	// clang-format off
    struct Config {
       fhicl::Atom<unsigned> minimum_hits {fhicl::Name("minimumHits" ) , fhicl::Comment("Minimum number of hits per event to pass"), 1};
	   fhicl::Atom<unsigned> debug_every  {fhicl::Name("debugEvery"), fhicl::Comment("Print running stats every N events (0 disables periodic prints)"), 1000000};
    };
	// clang-format on

	explicit CaloHitFilter(const art::EDFilter::Table<Config>& config);
	void beginJob() override;
	void endJob() override;
	bool filter(art::Event& event) override;

  private:
	mu2e::CaloDAQUtilities caloDAQUtil_;

	DTCLib::DTC_Subsystem subsystem_;
	unsigned              minimum_hits_;
	unsigned              debugEvery_{};
	size_t                eventsSeen_{};
	size_t                eventsPassed_{};
	size_t                eventsWithDtcevt_{};
};
}  // namespace mu2e

mu2e::CaloHitFilter::CaloHitFilter(const art::EDFilter::Table<Config>& config) : art::EDFilter{config}, caloDAQUtil_("CaloHitFilter") {
	minimum_hits_ = config().minimum_hits();
	debugEvery_   = config().debug_every();
}

void mu2e::CaloHitFilter::beginJob() { TLOG(TLVL_INFO) << "CaloHitFilter beginJob: debugEvery=" << debugEvery_; }

void mu2e::CaloHitFilter::endJob() { TLOG(TLVL_INFO) << "CaloHitFilter summary: seen=" << eventsSeen_ << ", withDTCEVT=" << eventsWithDtcevt_ << ", passed=" << eventsPassed_; }

bool mu2e::CaloHitFilter::filter(art::Event& event) {
	++eventsSeen_;
	artdaq::Fragments fragments = caloDAQUtil_.getFragments(event);
	if(!fragments.empty()) {
		++eventsWithDtcevt_;
	}
	uint event_hits = 0;
	for(const auto& frag : fragments) {
		mu2e::DTCEventFragment eventFragment(frag);
		const auto&            caloSubEvents = eventFragment.getSubsystemData(DTCLib::DTC_Subsystem::DTC_Subsystem_Calorimeter);
		// Loop over calo DTCs
		for(const auto& subevent : caloSubEvents) {
			mu2e::CalorimeterDataDecoder caloDecoder(subevent);

			// Loop over ROCs
			for(size_t iROC = 0; iROC < caloDecoder.block_count(); iROC++) {
				auto calHitTestDataVec = caloDecoder.GetCalorimeterHitsForTrigger(iROC);
				event_hits += calHitTestDataVec->size();
			}
		}
	}

	bool pass = false;
	if(event_hits >= minimum_hits_) {
		pass = true;
		++eventsPassed_;
	}

	if(debugEvery_ != 0 && (eventsSeen_ % debugEvery_) == 0) {
		TLOG(TLVL_INFO) << "CaloHitFilter running stats: seen=" << eventsSeen_ << ", withDTCEVT=" << eventsWithDtcevt_ << ", passed=" << eventsPassed_;
	}

	return pass;
}

DEFINE_ART_MODULE(mu2e::CaloHitFilter)
