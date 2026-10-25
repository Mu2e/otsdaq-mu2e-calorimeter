#include "art/Framework/Core/EDFilter.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Principal/Event.h"
// #include "art/Framework/Services/Registry/ServiceHandle.h"
#include "art/Framework/Principal/Handle.h"
#include "art/Framework/Principal/Run.h"
#include "messagefacility/MessageLogger/MessageLogger.h"

#include "TRACE/tracemf.h"
#include "artdaq/DAQdata/Globals.hh"
#define TRACE_NAME "CaloLaserFilter"

#include "canvas/Utilities/Exception.h"
#include "canvas/Utilities/InputTag.h"

#include <artdaq-core/Data/ContainerFragment.hh>
#include "artdaq-core/Data/Fragment.hh"

#include "artdaq-core-mu2e/Data/EventHeader.hh"
#include "artdaq-core-mu2e/Overlays/DTCEventFragment.hh"
#include "artdaq-core-mu2e/Overlays/Decoders/CalorimeterDataDecoder.hh"
#include "artdaq-core-mu2e/Overlays/FragmentType.hh"

#include "Offline/DAQ/inc/CaloDAQUtilities.hh"
#include "Offline/DataProducts/inc/CaloConst.hh"

#include "cetlib_except/exception.h"

#include <iomanip>
#include <map>
#include <sstream>
#include <vector>

namespace mu2e {
class CaloLaserFilter : public art::EDFilter {
  public:
	// clang-format off
    struct Config {
       fhicl::Atom<unsigned> minimum_hits {fhicl::Name("minimumHits" ) , fhicl::Comment("Minimum number of hits per event to pass"), 1};
	   fhicl::Atom<bool>     check_disk_pins {fhicl::Name("checkDiskPins"), fhicl::Comment("Use disk pin-diodes too"), false};
	   fhicl::Atom<unsigned> debug_every  {fhicl::Name("debugEvery"), fhicl::Comment("Print running stats every N events (0 disables periodic prints)"), 1000000};
    };
	// clang-format on

	explicit CaloLaserFilter(const art::EDFilter::Table<Config>& config);
	void beginJob() override;
	void endJob() override;
	bool filter(art::Event& event) override;

  private:
	mu2e::CaloDAQUtilities caloDAQUtil_;

	DTCLib::DTC_Subsystem subsystem_;
	unsigned              minimum_hits_;
	bool                  check_disk_pins_;
	unsigned              debugEvery_{};
	size_t                eventsSeen_{};
	size_t                eventsPassed_{};
	size_t                eventsWithDtcevt_{};
};
}  // namespace mu2e

mu2e::CaloLaserFilter::CaloLaserFilter(const art::EDFilter::Table<Config>& config) : art::EDFilter{config}, caloDAQUtil_("CaloLaserFilter") {
	minimum_hits_    = config().minimum_hits();
	check_disk_pins_ = config().check_disk_pins();
	debugEvery_      = config().debug_every();
}

void mu2e::CaloLaserFilter::beginJob() { TLOG(TLVL_INFO) << "CaloLaserFilter beginJob: debugEvery=" << debugEvery_; }

void mu2e::CaloLaserFilter::endJob() { TLOG(TLVL_INFO) << "CaloLaserFilter summary: seen=" << eventsSeen_ << ", withDTCEVT=" << eventsWithDtcevt_ << ", passed=" << eventsPassed_; }

bool mu2e::CaloLaserFilter::filter(art::Event& event) {
	++eventsSeen_;
	artdaq::Fragments fragments = caloDAQUtil_.getFragments(event);
	if(!fragments.empty()) {
		++eventsWithDtcevt_;
	}
	uint                              laser_total_hits = 0;
	std::map<int, std::map<int, int>> laser_hits_per_channel;
	for(const auto& frag : fragments) {
		mu2e::DTCEventFragment eventFragment(frag);
		const auto&            caloSubEvents = eventFragment.getSubsystemData(DTCLib::DTC_Subsystem::DTC_Subsystem_Calorimeter);
		// Loop over calo DTCs
		for(const auto& subevent : caloSubEvents) {
			mu2e::CalorimeterDataDecoder caloDecoder(subevent);

			// Loop over ROCs
			for(size_t iROC = 0; iROC < caloDecoder.block_count(); iROC++) {
				auto calHitTestDataVec = caloDecoder.GetCalorimeterHitsForTrigger(iROC);

				for(const auto& hit : *calHitTestDataVec) {
					if (caloDAQUtil_.isHitGood(hit) != mu2e::CaloDAQUtilities::CaloHitError::Good) {
						continue;  // Skip bad hits	
					}
					if(!check_disk_pins_ && hit.first.BoardID != mu2e::CaloConst::_laserBoardId) {
						// Skip non-laser boards if check_disk_pins_ is false
						continue;
					}
					if(hit.first.DetectorID == mu2e::CaloConst::detType::PINDiode) {
						uint boardId   = hit.first.BoardID;
						uint channelId = hit.first.ChannelID;
						laser_total_hits++;
						laser_hits_per_channel[boardId][channelId]++;
					}
				}
			}
		}
	}

	bool pass = false;
	if(laser_total_hits >= minimum_hits_) {
		uint total_lr_hits = 0;
		// Laser board: the two sides of a pin-diode are on the same board, channel pairs (0,1), (2,3), ...
		// Other (disk pin-diode) boards: for every even board with >= 1 hits on a channel, check if the same channel of board+1 has >= 1 hits.
		// This assumes that the channelId is the same for both boards, which is true for the current Calorimeter readout.
		for(const auto& [boardId, channels] : laser_hits_per_channel) {
			if(boardId == mu2e::CaloConst::_laserBoardId) {
				for(const auto& [channelId, hits] : channels) {
					if(channelId % 2 == 0 && hits >= 1) {
						auto next = channels.find(channelId + 1);
						if(next != channels.end()) {
							total_lr_hits += std::min(hits, next->second);
						}
					}
				}
			} else if(check_disk_pins_ && boardId % 2 == 0) {
				for(const auto& [channelId, hits] : channels) {
					if(hits >= 1) {
						int nextBoardId = boardId + 1;
						if(laser_hits_per_channel.find(nextBoardId) != laser_hits_per_channel.end()) {
							if(laser_hits_per_channel[nextBoardId].find(channelId) != laser_hits_per_channel[nextBoardId].end()) {
								int nextHits = laser_hits_per_channel[nextBoardId][channelId];
								total_lr_hits += std::min(hits, nextHits);
							}
						}
					}
				}
			}
			if(total_lr_hits >= minimum_hits_) {
				pass = true;
				++eventsPassed_;
				break;  // No need to check further if we've already passed
			}
		}
	}

	if(debugEvery_ != 0 && (eventsSeen_ % debugEvery_) == 0) {
		TLOG(TLVL_INFO) << "CaloLaserFilter running stats: seen=" << eventsSeen_ << ", withDTCEVT=" << eventsWithDtcevt_ << ", passed=" << eventsPassed_;
	}

	return pass;
}

DEFINE_ART_MODULE(mu2e::CaloLaserFilter)
