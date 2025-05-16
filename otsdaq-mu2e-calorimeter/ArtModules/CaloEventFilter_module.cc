#include "art/Framework/Core/EDFilter.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Principal/Event.h"
// #include "art/Framework/Services/Registry/ServiceHandle.h"
#include "art/Framework/Principal/Handle.h"
#include "art/Framework/Principal/Run.h"
#include "messagefacility/MessageLogger/MessageLogger.h"

#include "TRACE/tracemf.h"
#include "artdaq/DAQdata/Globals.hh"
#define TRACE_NAME "CaloEventFilter"

#include "canvas/Utilities/Exception.h"
#include "canvas/Utilities/InputTag.h"

#include <artdaq-core/Data/ContainerFragment.hh>
#include "artdaq-core/Data/Fragment.hh"

#include "artdaq-core-mu2e/Overlays/Decoders/CalorimeterDataDecoder.hh"
#include "artdaq-core-mu2e/Data/EventHeader.hh"
#include "artdaq-core-mu2e/Overlays/DTCEventFragment.hh"
#include "artdaq-core-mu2e/Overlays/FragmentType.hh"

#include "cetlib_except/exception.h"

#include <iomanip>
#include <sstream>
#include <vector>

namespace mu2e {
class CaloEventFilter : public art::EDFilter {
  public:
	// clang-format off
    struct Config {
       fhicl::Atom<std::string> subsystem_override {fhicl::Name("subsystemOverride" ) , fhicl::Comment("Override calo subsystem [\"calo\", \"tracker\"]"), "calo"};
    };
	// clang-format on

	explicit CaloEventFilter(const art::EDFilter::Table<Config>& config);
	bool filter(art::Event& event) override;

	artdaq::Fragments getFragments(art::Event& event);

  private:
	DTCLib::DTC_Subsystem subsystem_;
};
}  // namespace mu2e

mu2e::CaloEventFilter::CaloEventFilter(const art::EDFilter::Table<Config>& config)
    : art::EDFilter{config} {
	if(config().subsystem_override() == "calo") {
		subsystem_ = DTCLib::DTC_Subsystem::DTC_Subsystem_Calorimeter;
	} else if(config().subsystem_override() == "tracker") {
		subsystem_ = DTCLib::DTC_Subsystem::DTC_Subsystem_Tracker;
	}
}

bool mu2e::CaloEventFilter::filter(art::Event& event) {
	artdaq::Fragments fragments = getFragments(event);
	for(const auto& frag : fragments) {
		mu2e::DTCEventFragment eventFragment(frag);
		const auto&            caloSubEvents = eventFragment.getSubsystemData(subsystem_);
		// Loop over calo DTCs
		for(const auto& subevent : caloSubEvents) {
			// Loop over ROCs
			for(const auto& dataBlock : subevent.GetDataBlocks()) {
				int nPackets = dataBlock.GetHeader()->GetPacketCount();
				if(nPackets > 0) {
					// We have data!
					return true;
				}
			}
		}
	}

	// Didn't find anything
	return false;
}

artdaq::Fragments mu2e::CaloEventFilter::getFragments(art::Event& event) {
	artdaq::Fragments    fragments;
	artdaq::FragmentPtrs containerFragments;

	std::vector<art::Handle<artdaq::Fragments>> fragmentHandles;
	fragmentHandles = event.getMany<std::vector<artdaq::Fragment>>();

	TLOG(TLVL_DEBUG + 6) << "Iterating through " << fragmentHandles.size()
	                     << " fragment handles\n";
	for(const auto& handle : fragmentHandles) {
		if(!handle.isValid() || handle->empty()) {
			continue;
		}

		if(handle->front().type() == artdaq::Fragment::ContainerFragmentType) {
			for(const auto& cont : *handle) {
				artdaq::ContainerFragment contf(cont);
				if(contf.fragment_type() != mu2e::FragmentType::DTCEVT) {
					break;
				}

				for(size_t ii = 0; ii < contf.block_count(); ++ii) {
					containerFragments.push_back(contf[ii]);
					fragments.push_back(*containerFragments.back());
				}
			}
		} else {
			if(handle->front().type() == mu2e::FragmentType::DTCEVT) {
				for(auto frag : *handle) {
					fragments.emplace_back(frag);
				}
			}
		}
	}
	return fragments;
}

DEFINE_ART_MODULE(mu2e::CaloEventFilter)
