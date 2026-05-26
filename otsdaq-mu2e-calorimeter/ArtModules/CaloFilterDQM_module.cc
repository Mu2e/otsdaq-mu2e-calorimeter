#include "art/Framework/Core/EDFilter.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Principal/Event.h"
// #include "art/Framework/Services/Registry/ServiceHandle.h"
#include "art/Framework/Principal/Handle.h"
#include "art/Framework/Principal/Run.h"
#include "messagefacility/MessageLogger/MessageLogger.h"

#include "TRACE/tracemf.h"
#include "artdaq/DAQdata/Globals.hh"
#define TRACE_NAME "CaloFilterDQM"

#include "canvas/Utilities/Exception.h"
#include "canvas/Utilities/InputTag.h"

#include <artdaq-core/Data/ContainerFragment.hh>
#include "artdaq-core/Data/Fragment.hh"

#include "Offline/DAQ/inc/CaloDAQUtilities.hh"
#include "artdaq-core-mu2e/Data/EventHeader.hh"
#include "artdaq-core-mu2e/Overlays/DTCEventFragment.hh"
#include "artdaq-core-mu2e/Overlays/Decoders/CalorimeterDataDecoder.hh"
#include "artdaq-core-mu2e/Overlays/Decoders/MobileSyncDataDecoder.hh"
#include "artdaq-core-mu2e/Overlays/FragmentType.hh"

#include "cetlib_except/exception.h"

#include <iomanip>
#include <sstream>
#include <vector>

namespace mu2e {
class CaloFilterDQM : public art::EDFilter {
  public:
	struct Config {
		fhicl::Atom<int> verbosity{fhicl::Name("verbosity"), fhicl::Comment("Verbosity [0-2]"), 0};
		fhicl::Atom<int> metrics_level{fhicl::Name("metricsLevel"), fhicl::Comment("Metrics reporting level"), 1};
	};

	explicit CaloFilterDQM(const art::EDFilter::Table<Config>& config);

	bool         filter(art::Event& e) override;
	virtual bool endRun(art::Run& run) override;

	void processCFOData(mu2e::DTCEventFragment& eventFragment);
	void processCaloData(mu2e::DTCEventFragment& eventFragment);

	void printRunSummary();

  private:
	int                    verbosity_;
	int                    metrics_reporting_level_;
	mu2e::CaloDAQUtilities caloDAQUtil_;

	// Per-event
	size_t nCaloDTCs;
	size_t nEventCaloHits;
	size_t nEventGoodHits;
	size_t nEventBadHits;

	long int cfoEWT = -1;
	size_t   nEventBadDTCEWTs;
	size_t   nEventBadROCEWTs;
	size_t   nEventBadHitEWTs;

	// Totals
	size_t nEvents;
	size_t nTotalCaloHits;
	size_t nTotalGoodHits;
	size_t nTotalBadHits;

	size_t nTotalBadDTCEWTs;
	size_t nTotalBadROCEWTs;
	size_t nTotalBadHitEWTs;

	std::map<int, int> failMap_DTCEWTs;

	std::map<int, std::map<int, int>>                    goodHits_per_dtc_link;
	std::map<int, std::map<int, int>>                    failures_per_dtc_link;
	std::map<int, std::map<int, int>>                    badHitEWT_per_dtc_link;
	std::map<int, std::map<int, int>>                    badHitEWT_per_board_channel;
	std::map<int, std::map<int, int>>                    goodHitEWT_per_dtc_link;
	std::map<mu2e::CaloDAQUtilities::CaloHitError, uint> failure_type_counter;

	bool failedEvent;
};
}  // namespace mu2e

mu2e::CaloFilterDQM::CaloFilterDQM(const art::EDFilter::Table<Config>& config)
    : art::EDFilter{config}, verbosity_(config().verbosity()), metrics_reporting_level_(config().metrics_level()), caloDAQUtil_("CaloDigiFromFragments") {
	nEvents          = 0;
	nTotalCaloHits   = 0;
	nTotalGoodHits   = 0;
	nTotalBadHits    = 0;
	nTotalBadDTCEWTs = 0;
	nTotalBadROCEWTs = 0;
	nTotalBadHitEWTs = 0;
	failedEvent      = false;
}

bool mu2e::CaloFilterDQM::filter(art::Event& event) {
	art::EventNumber_t eventNumber = event.event();

	nEvents++;

	nCaloDTCs      = 0;
	nEventCaloHits = 0;
	nEventGoodHits = 0;
	nEventBadHits  = 0;

	nEventBadDTCEWTs = 0;
	nEventBadROCEWTs = 0;
	nEventBadHitEWTs = 0;

	artdaq::Fragments fragments = caloDAQUtil_.getFragments(event);
	TLOG(TLVL_DEBUG + 6) << "Iterating through " << fragments.size() << " fragments\n";
	for(const auto& frag : fragments) {
		mu2e::DTCEventFragment eventFragment(frag);
		processCFOData(eventFragment);
		processCaloData(eventFragment);
	}

	nTotalCaloHits += nEventCaloHits;
	nTotalGoodHits += nEventGoodHits;
	nTotalBadHits += nEventBadHits;

	nTotalBadDTCEWTs += nEventBadDTCEWTs;
	nTotalBadROCEWTs += nEventBadROCEWTs;
	nTotalBadHitEWTs += nEventBadHitEWTs;

	int nBadHitEWTChannels = 0;
	for(const auto& boardChannelBadHitEWT : badHitEWT_per_board_channel) {
		for(int channelID = 0; channelID < 20; channelID++) {
			if(boardChannelBadHitEWT.second.count(channelID)) {
				nBadHitEWTChannels++;
			}
		}
	}

	// Send metrics
	if(metricMan != nullptr) {
		// EwT
		metricMan->sendMetric("CaloFilterDQM.nEventBadDTCEWTs", int(nEventBadDTCEWTs), "Number of bad DTC EwTs", metrics_reporting_level_, artdaq::MetricMode::Maximum);
		metricMan->sendMetric("CaloFilterDQM.nEventBadROCEWTs", int(nEventBadROCEWTs), "Number of bad ROC EwTs", metrics_reporting_level_, artdaq::MetricMode::Maximum);
		metricMan->sendMetric("CaloFilterDQM.nEventBadHitEWTs", int(nEventBadHitEWTs), "Number of bad hit EwTs", metrics_reporting_level_, artdaq::MetricMode::Maximum);
		metricMan->sendMetric("CaloFilterDQM.nBadHitEWTChannels", int(nBadHitEWTChannels), "Number of channels with bad hit EwTs", metrics_reporting_level_, artdaq::MetricMode::LastPoint);

		// Failures
		metricMan->sendMetric("CaloFilterDQM.nTotalHits", int(nTotalCaloHits), "Total hits", metrics_reporting_level_, artdaq::MetricMode::LastPoint);
		metricMan->sendMetric("CaloFilterDQM.nTotalBadHits", int(nTotalBadHits), "Total failed hits", metrics_reporting_level_, artdaq::MetricMode::LastPoint);
		metricMan->sendMetric("CaloFilterDQM.nTotalGoodHits", int(nTotalGoodHits), "Total good hits", metrics_reporting_level_, artdaq::MetricMode::LastPoint);
		for(auto fail : failure_type_counter) {
			metricMan->sendMetric(
			    "CaloFilterDQM.FailedHits_" + caloDAQUtil_.getCaloHitErrorName(fail.first), int(fail.second), "Failed hits, by failure type", metrics_reporting_level_, artdaq::MetricMode::LastPoint);
		}
		metricMan->sendMetric("CaloFilterDQM.FailedDTCLinks", int(failures_per_dtc_link.size()), "Number of DTC links with failed hits", metrics_reporting_level_, artdaq::MetricMode::LastPoint);
		for(const auto& dtcLinkFail : failures_per_dtc_link) {
			int dtcID = dtcLinkFail.first;
			for(const auto& rocFail : dtcLinkFail.second) {
				int rocID  = rocFail.first;
				int nFails = rocFail.second;
				metricMan->sendMetric("CaloFilterDQM.FailedHits_DTC" + std::to_string(dtcID) + "_Link" + std::to_string(rocID),
				                      int(nFails),
				                      "Failed hits, by DTC link and ROC ID",
				                      metrics_reporting_level_,
				                      artdaq::MetricMode::LastPoint);
			}
		}
		for(const auto& dtcLinkGood : goodHits_per_dtc_link) {
			int dtcID = dtcLinkGood.first;
			for(const auto& rocGood : dtcLinkGood.second) {
				int rocID  = rocGood.first;
				int nGoods = rocGood.second;
				metricMan->sendMetric("CaloFilterDQM.GoodHits_DTC" + std::to_string(dtcID) + "_Link" + std::to_string(rocID),
				                      int(nGoods),
				                      "Good hits, by DTC link and ROC ID",
				                      metrics_reporting_level_,
				                      artdaq::MetricMode::LastPoint);
			}
		}

		// Per-event
		metricMan->sendMetric("CaloFilterDQM.nCaloDTCs", int(nCaloDTCs), "Calo DTCs", metrics_reporting_level_, artdaq::MetricMode::LastPoint);
		metricMan->sendMetric("CaloFilterDQM.nHits", int(nEventCaloHits), "Hits per event", metrics_reporting_level_, artdaq::MetricMode::Average);
		metricMan->sendMetric("CaloFilterDQM.nGoodHits", int(nEventGoodHits), "Good hits per event", metrics_reporting_level_, artdaq::MetricMode::Average);
		metricMan->sendMetric("CaloFilterDQM.nBadHits", int(nEventBadHits), "Bad hits per event", metrics_reporting_level_, artdaq::MetricMode::Average);

		if(verbosity_ > 1) {
			std::cout << "Metrics for event " << eventNumber << ":"
			          << " nCaloDTCs=" << nCaloDTCs << ", nEventCaloHits=" << nEventCaloHits << ", nTotalBadHits=" << nTotalBadHits << std::endl;

			if(verbosity_ > 2) {
				for(const auto& fail : failure_type_counter) {
					std::cout << "\tFailure mode " << fail.first << " [bad " << caloDAQUtil_.getCaloHitErrorName(fail.first) << "], count: " << fail.second << " ("
					          << int(100. * fail.second / nTotalCaloHits) << "%)\n";
				}
				for(const auto& dtcLinkFail : failures_per_dtc_link) {
					int dtcID = dtcLinkFail.first;
					for(const auto& rocFail : dtcLinkFail.second) {
						int rocID  = rocFail.first;
						int nFails = rocFail.second;
						std::cout << "\tDTC " << dtcID << ", Link " << rocID << ": " << nFails << " failed hits\n";
					}
				}
			}
		}

	} else {
		TLOG(TLVL_DEBUG + 6) << "WARNING: No metric manager found!";
	}

	TLOG(TLVL_DEBUG + 6) << "[CaloFilterDQM::filter] found " << nCaloDTCs << " calo DTCs in event " << (int)eventNumber;
	TLOG(TLVL_DEBUG + 6) << "[CaloFilterDQM::filter] found " << nEventCaloHits << " calo hits in event " << (int)eventNumber;

	if(nCaloDTCs == 0) {
		TLOG(TLVL_WARNING) << "[CaloFilterDQM::filter] found no calo DTCs in event " << (int)eventNumber << "!";
	}

	return failedEvent;
}

bool mu2e::CaloFilterDQM::endRun(art::Run&) {
	if(verbosity_ > 0) {
		printRunSummary();
	}

	return true;
}

void mu2e::CaloFilterDQM::processCFOData(mu2e::DTCEventFragment& eventFragment) {
	// CFO data is needed to get the "correct" EwT for the event, to compare against the EwTs in the calo data
	// Default to -1 if no CFO found
	cfoEWT            = -1;
	auto cfoSubEvents = eventFragment.getSubsystemData(DTCLib::DTC_Subsystem::DTC_Subsystem_MobileSync);
	for(const auto& subevent : cfoSubEvents) {
		mu2e::MobileSyncDataDecoder cfoDecoder(subevent);
		auto&                       this_subevent = cfoDecoder.event_;
		cfoEWT                                    = this_subevent.GetEventWindowTag().GetEventWindowTag(true);
	}
}

void mu2e::CaloFilterDQM::processCaloData(mu2e::DTCEventFragment& eventFragment) {
	failedEvent = false;

	auto caloSubEvents = eventFragment.getSubsystemData(DTCLib::DTC_Subsystem::DTC_Subsystem_Calorimeter);

	// Loop over calo DTCs (should be only one per fragment)
	for(const auto& subevent : caloSubEvents) {
		mu2e::CalorimeterDataDecoder caloDecoder(subevent);
		auto&                        this_subevent = caloDecoder.event_;

		uint64_t dtcID  = this_subevent.GetDTCID();
		long int dtcEWT = this_subevent.GetEventWindowTag().GetEventWindowTag(true);
		if(cfoEWT >= 0 && dtcEWT != cfoEWT)
			nEventBadDTCEWTs++;

		nCaloDTCs++;
		// Iterate over the data blocks (ROCs)
		std::vector<DTCLib::DTC_DataBlock> dataBlocks = this_subevent.GetDataBlocks();
		uint                               nROCs      = dataBlocks.size();
		TLOG(TLVL_DEBUG + 6) << "Iterating through " << nROCs << " data blocks (ROCs)\n";
		std::vector<int> roc_hits;
		for(uint iroc = 0; iroc < nROCs; iroc++) {
			// DTCLib::DTC_DataHeaderPacket* rocHeader = dataBlocks[iroc].GetHeader().get();
			// long int rocEWT = rocHeader->GetEventWindowTag().GetEventWindowTag(true);
			long int rocEWT = dataBlocks[iroc].GetHeader()->GetEventWindowTag().GetEventWindowTag(true);
			if(rocEWT != dtcEWT)
				nEventBadROCEWTs++;

			auto caloHits = caloDecoder.GetCalorimeterHitData(iroc);
			uint nHits    = caloHits->size();
			roc_hits.push_back(nHits);
			for(uint ihit = 0; ihit < nHits; ihit++) {
				mu2e::CalorimeterDataDecoder::CalorimeterHitDataPacket hit = caloHits->at(ihit).first;
				// std::vector<uint16_t>                                  hit_waveform = caloHits->at(ihit).second;

				// Check that the hit is good
				auto errorCode = caloDAQUtil_.isHitGood(caloHits->at(ihit));
				if(errorCode) {
					failure_type_counter[errorCode]++;
					failures_per_dtc_link[dtcID][iroc]++;
					nEventBadHits++;
				} else {
					goodHits_per_dtc_link[dtcID][iroc]++;
					nEventGoodHits++;

					long int hitEWT = hit.InPayloadEventWindowTag - 1;
					if(hitEWT != dtcEWT % 65536) {
						if(verbosity_ > 2) {
							std::cout << "WARNING! DTC " << dtcID << " Link " << iroc << " Board " << hit.BoardID << " Channel " << hit.ChannelID << ": Hit EwT " << hitEWT << " != DTC EwT "
							          << dtcEWT % 65536 << " (ROC EWT: " << rocEWT << ", DTC EWT: " << dtcEWT << ", CFO EwT: " << cfoEWT << ")\n";
						}
						nEventBadHitEWTs++;
						badHitEWT_per_dtc_link[dtcID][iroc]++;
						badHitEWT_per_board_channel[hit.BoardID][hit.ChannelID]++;
					} else {
						goodHitEWT_per_dtc_link[dtcID][iroc]++;
					}
				}
				nEventCaloHits++;
			}
		}  // loop over ROCs
	}      // loop over subevents (DTCs)
}

void mu2e::CaloFilterDQM::printRunSummary() {
	std::cout << "Run summary:" << std::endl
	          << "Total events: " << nEvents << std::endl
	          << "\t total calo hits: " << nTotalCaloHits << std::endl
	          << "\t total good hits: " << nTotalGoodHits << " (" << int(100. * nTotalGoodHits / nTotalCaloHits) << "%)" << std::endl
	          << "\t total bad hits: " << nTotalBadHits << " (" << int(100. * nTotalBadHits / nTotalCaloHits) << "%)" << std::endl;

	std::cout << "EwT failures:" << std::endl
	          << "\t bad DTC EwTs: " << nTotalBadDTCEWTs << std::endl
	          << "\t bad ROC EwTs: " << nTotalBadROCEWTs << std::endl
	          << "\t bad hit EwTs: " << nTotalBadHitEWTs << std::endl;

	std::cout << "Failures:" << std::endl;
	for(auto fail : failure_type_counter) {
		std::cout << "Failure mode " << fail.first << " [bad " << caloDAQUtil_.getCaloHitErrorName(fail.first) << "], count: " << fail.second << " (" << int(100. * fail.second / nTotalCaloHits)
		          << "%)\n";
	}

	std::cout << "Failed DTC links:" << std::endl;
	for(const auto& dtcLinkFail : failures_per_dtc_link) {
		int dtcID = dtcLinkFail.first;
		for(const auto& rocFail : dtcLinkFail.second) {
			int rocID  = rocFail.first;
			int nFails = rocFail.second;
			std::cout << "\tDTC " << dtcID << ", Link " << rocID << ": " << nFails << " failed hits\n";
		}
	}

	std::cout << "Good hits per DTC link:" << std::endl;
	std::cout << std::setw(12) << "DTC ID" << std::setw(12) << "Link 0" << std::setw(12) << "Link 1" << std::setw(12) << "Link 2" << std::setw(12) << "Link 3" << std::setw(12) << "Link 4"
	          << std::setw(12) << "Link 5" << std::endl;
	for(const auto& dtcLinkGood : goodHits_per_dtc_link) {
		int dtcID = dtcLinkGood.first;
		std::cout << std::setw(12) << dtcID;
		for(int rocID = 0; rocID < 6; rocID++) {
			if(dtcLinkGood.second.count(rocID)) {
				std::cout << std::setw(12) << dtcLinkGood.second.at(rocID);
			} else {
				std::cout << std::setw(12) << "-";
			}
		}
		std::cout << std::endl;
	}

	std::cout << "Bad hit EwT failures per DTC link:" << std::endl;
	std::cout << std::setw(12) << "DTC ID" << std::setw(12) << "Link 0" << std::setw(12) << "Link 1" << std::setw(12) << "Link 2" << std::setw(12) << "Link 3" << std::setw(12) << "Link 4"
	          << std::setw(12) << "Link 5" << std::endl;
	for(const auto& dtcLinkBadHitEWT : badHitEWT_per_dtc_link) {
		int dtcID = dtcLinkBadHitEWT.first;
		std::cout << std::setw(12) << dtcID;
		for(int rocID = 0; rocID < 6; rocID++) {
			if(dtcLinkBadHitEWT.second.count(rocID)) {
				std::cout << std::setw(12) << dtcLinkBadHitEWT.second.at(rocID);
			} else {
				std::cout << std::setw(12) << "-";
			}
		}
		std::cout << std::endl;
	}

	std::cout << "Bad hit EwT failures per board/channel:" << std::endl;
	std::cout << std::setw(12) << "Board ID";
	for(int i = 0; i < 20; i++)
		std::cout << std::setw(12) << "Chan " << i;
	std::cout << std::endl;
	for(const auto& boardChannelBadHitEWT : badHitEWT_per_board_channel) {
		int boardID = boardChannelBadHitEWT.first;
		std::cout << std::setw(12) << boardID;
		for(int channelID = 0; channelID < 20; channelID++) {
			if(boardChannelBadHitEWT.second.count(channelID)) {
				std::cout << std::setw(12) << boardChannelBadHitEWT.second.at(channelID);
			} else {
				std::cout << std::setw(12) << "-";
			}
		}
		std::cout << std::endl;
	}
}

DEFINE_ART_MODULE(mu2e::CaloFilterDQM)
