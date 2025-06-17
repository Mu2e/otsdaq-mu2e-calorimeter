#include "art/Framework/Core/EDAnalyzer.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Principal/Handle.h"
#include "art/Framework/Principal/Run.h"
#include "art/Framework/Services/Registry/ServiceHandle.h"
#include "messagefacility/MessageLogger/MessageLogger.h"

#include "TRACE/tracemf.h"
#include "artdaq/DAQdata/Globals.hh"
#define TRACE_NAME "CaloDataAnalyzer"

#include "canvas/Utilities/Exception.h"
#include "canvas/Utilities/InputTag.h"

#include <artdaq-core/Data/ContainerFragment.hh>
#include "artdaq-core/Data/Fragment.hh"

#include "Offline/DAQ/inc/CaloDAQUtilities.hh"
#include "artdaq-core-mu2e/Data/EventHeader.hh"
#include "artdaq-core-mu2e/Overlays/DTCEventFragment.hh"
#include "artdaq-core-mu2e/Overlays/Decoders/CalorimeterDataDecoder.hh"
#include "artdaq-core-mu2e/Overlays/FragmentType.hh"

#include "cetlib_except/exception.h"

#include <iomanip>
#include <sstream>
#include <vector>

#include "TGraph.h"
#include "TH1.h"
#include "TH2.h"
#include "TTree.h"
#include "art_root_io/TFileService.h"

namespace mu2e {
class CaloDataAnalyzer : public art::EDAnalyzer {
  public:
	// clang-format off
    struct Config {
      fhicl::Atom<std::string> unpackerModuleLabel {fhicl::Name("unpackerModuleLabel" ) , fhicl::Comment("unpackerModuleLabel"), ""};
      fhicl::Atom<std::string> unpackerInstanceLabel {fhicl::Name("unpackerInstanceLabel" ) , fhicl::Comment("unpackerInstanceLabel"), ""};
      fhicl::Atom<int> verbosity {fhicl::Name("verbosity" ) , fhicl::Comment("Verbosity [0-2]"), 0};
      fhicl::Atom<int> data_type {fhicl::Name("dataType" ) , fhicl::Comment("Data type (0:standard, 1:debug, 2:counters)"), 0};
      fhicl::Atom<int> maxEventNum {fhicl::Name("maxEventNum" ) , fhicl::Comment("maxEventNum (-1:infinite)"), -1};
      fhicl::Atom<bool> fillEmptyEvents {fhicl::Name("fillEmptyEvents" ) , fhicl::Comment("Fill tree even if zero hits"), false};
      fhicl::Atom<bool> fillFailedEvents {fhicl::Name("fillFailedEvents" ) , fhicl::Comment("Fill tree even with failed hits"), true};
    };
	// clang-format on

	explicit CaloDataAnalyzer(const art::EDAnalyzer::Table<Config>& config);
	void analyze(art::Event const& event) override;
	void endJob() override;
	void processCaloData(mu2e::CalorimeterDataDecoder const& caloDecoder);

  private:
	std::string            unpackerModuleLabel_;
	std::string            unpackerInstanceLabel_;
	int                    verbosity_;
	int                    data_type_;
	mu2e::CaloDAQUtilities caloDAQUtil_;
	int                    maxEventNum_;
	bool                   fillEmptyEvents_;
	bool                   fillFailedEvents_;

	int                                                  event_failedhits;
	int                                                  total_events;
	int                                                  total_hits;
	int                                                  total_goodhits;
	int                                                  total_failedhits;
	std::map<mu2e::CaloDAQUtilities::CaloHitError, uint> failure_counter;
	size_t                                               nCaloEvents;
	size_t                                               nCaloHits;
	int                                                  this_eventNumber;

	TH1D*   h1_t0;
	TH1D*   h1_maxIndex;
	TH1D*   h1_nSamples;
	TH2D*   h2_channelHits;
	TH2D*   h2_failedChannels;
	TH2D*   h2_failedROCs;
	TH2D*   h2_waveforms;
	TGraph* g_EWTs;
	TGraph* g_eventHits;
	TGraph* g_eventEWT;

	static const int MAXNHITS    = 1348;   // 1 hit per sipm
	static const int MAXNSAMPLES = 67400;  // 50 samples per hit (max!)

	int                                 t_run;
	int                                 t_subrun;
	int                                 t_nevt;
	Long64_t                            t_currentDTCEventWindow;
	int                                 t_hasFailures;
	int                                 t_nhits;
	int                                 t_dtcID[MAXNHITS];
	int                                 t_boardID[MAXNHITS];
	int                                 t_linkID[MAXNHITS];
	int                                 t_chanID[MAXNHITS];
	int                                 t_errflag[MAXNHITS];
	int                                 t_fff[MAXNHITS];
	int                                 t_time_tot[MAXNHITS];
	Long64_t                            t_ewhit[MAXNHITS];
	int                                 t_peakpos[MAXNHITS];
	int                                 t_peakval[MAXNHITS];
	int                                 t_nofsamples[MAXNHITS];
	int                                 t_firstsample[MAXNHITS];
	int                                 t_nsamples;
	int                                 t_ADC[MAXNSAMPLES];
	std::vector<std::vector<uint16_t>>* t_ADC_hit = 0;

	int                                 tBad_run;
	int                                 tBad_subrun;
	int                                 tBad_nevt;
	Long64_t                            tBad_currentDTCEventWindow;
	int                                 tBad_hasFailures;
	int                                 tBad_nhits;
	int                                 tBad_dtcID[MAXNHITS];
	int                                 tBad_boardID[MAXNHITS];
	int                                 tBad_linkID[MAXNHITS];
	int                                 tBad_chanID[MAXNHITS];
	int                                 tBad_errflag[MAXNHITS];
	int                                 tBad_fff[MAXNHITS];
	int                                 tBad_time_tot[MAXNHITS];
	Long64_t                            tBad_ewhit[MAXNHITS];
	int                                 tBad_peakpos[MAXNHITS];
	int                                 tBad_peakval[MAXNHITS];
	int                                 tBad_nofsamples[MAXNHITS];
	int                                 tBad_firstsample[MAXNHITS];
	int                                 tBad_nsamples;
	int                                 tBad_ADC[MAXNSAMPLES];
	std::vector<std::vector<uint16_t>>* tBad_ADC_hit = 0;

	int               tH_run;
	int               tH_subrun;
	int               tH_nevt;
	int               tH_dtcID;
	Long64_t          tH_currentDTCEventWindow;
	Long64_t          tH_currentROCEventWindow;
	int               tH_nhits;
	int               tH_boardID;
	int               tH_linkID;
	int               tH_chanID;
	int               tH_errflag;
	int               tH_fff;
	int               tH_time_tot;
	Long64_t          tH_ewhit;
	int               tH_peakpos;
	int               tH_peakval;
	int               tH_nofsamples;
	std::vector<int>* tH_ADC = 0;

	TTree* tree;
	TTree* treeBad;
	TTree* treeHits;
};
}  // namespace mu2e

mu2e::CaloDataAnalyzer::CaloDataAnalyzer(const art::EDAnalyzer::Table<Config>& config)
    : art::EDAnalyzer{config}
    , unpackerModuleLabel_(config().unpackerModuleLabel())
    , unpackerInstanceLabel_(config().unpackerInstanceLabel())
    , verbosity_(config().verbosity())
    , data_type_(config().data_type())
    , caloDAQUtil_("CaloDigiFromFragments")
    , maxEventNum_(config().maxEventNum())
    , fillEmptyEvents_(config().fillEmptyEvents())
    , fillFailedEvents_(config().fillFailedEvents()) {
	art::ServiceHandle<art::TFileService> tfs;

	h1_t0 = tfs->make<TH1D>("h1_t0", "t0 distribution;t0", 2000, 0, 20000);
	h1_maxIndex =
	    tfs->make<TH1D>("h1_maxIndex", "max Waveform Sample Index;Index", 100, 0, 100);
	h1_nSamples = tfs->make<TH1D>("h1_nSamples", "Waveform length;nSamples", 100, 0, 100);
	h2_channelHits = tfs->make<TH2D>(
	    "h2_channelHits", "Hits per channel;BoardID;ChannelID", 256, 0, 256, 20, 0, 20);
	h2_failedChannels = tfs->make<TH2D>("h2_failedChannels",
	                                    "Failures per channel;BoardID;ChannelID",
	                                    256,
	                                    0,
	                                    256,
	                                    20,
	                                    0,
	                                    20);
	h2_failedROCs =
	    tfs->make<TH2D>("h2_failedROCs", "Failures per ROC;DTC;ROC", 30, 0, 30, 6, 0, 6);
	h2_waveforms = tfs->make<TH2D>(
	    "h2_waveforms", "All waveforms;clock ticks;ADC", 150, 0, 150, 4096, 0, 4096);
	g_EWTs      = tfs->makeAndRegister<TGraph>("g_EWTs", "EWT check;ROC EWT;Hit EWT");
	g_eventHits = tfs->makeAndRegister<TGraph>("g_eventHits",
	                                           "Total hits per event;EventNumber;Hits");
	g_eventEWT  = tfs->makeAndRegister<TGraph>("g_eventEWT", "Event EWT;EventNumber;EWT");
	g_EWTs->SetMarkerStyle(20);
	g_eventHits->SetMarkerStyle(20);
	g_eventEWT->SetMarkerStyle(20);

	tree = tfs->make<TTree>("tree", "Event tree");
	tree->Branch("run", &t_run, "run/I");
	tree->Branch("subrun", &t_subrun, "subrun/I");
	tree->Branch("nevt", &t_nevt, "nevt/I");
	tree->Branch(
	    "currentDTCEventWindow", &t_currentDTCEventWindow, "currentDTCEventWindow/L");
	tree->Branch("hasFailures", &t_hasFailures, "hasFailures/I");
	tree->Branch("nhits", &t_nhits, "nhits/I");
	tree->Branch("dtcID", &t_dtcID, "dtcID[nhits]/I");
	tree->Branch("boardID", &t_boardID, "boardID[nhits]/I");
	tree->Branch("linkID", &t_linkID, "linkID[nhits]/I");
	tree->Branch("chanID", &t_chanID, "chanID[nhits]/I");
	tree->Branch("errflag", &t_errflag, "errflag[nhits]/I");
	tree->Branch("fff", &t_fff, "fff[nhits]/I");
	tree->Branch("timetot", &t_time_tot, "time[nhits]/I");
	tree->Branch("ewhit", &t_ewhit, "ewhit[nhits]/L");
	tree->Branch("peakpos", &t_peakpos, "peakpos[nhits]/I");
	tree->Branch("peakval", &t_peakval, "peakval[nhits]/I");
	tree->Branch("nofsamples", &t_nofsamples, "nofsamples[nhits]/I");
	tree->Branch("firstsample", &t_firstsample, "firstsample[nhits]/I");
	tree->Branch("nsamples", &t_nsamples, "nsamples/I");
	tree->Branch("ADC", &t_ADC, "ADC[nsamples]/I");
	tree->Branch("ADChit", &t_ADC_hit);

	treeBad = tfs->make<TTree>("treeBad", "Event tree with corrupted hits only");
	treeBad->Branch("run", &tBad_run, "run/I");
	treeBad->Branch("subrun", &tBad_subrun, "subrun/I");
	treeBad->Branch("nevt", &tBad_nevt, "nevt/I");
	treeBad->Branch(
	    "currentDTCEventWindow", &tBad_currentDTCEventWindow, "currentDTCEventWindow/L");
	treeBad->Branch("hasFailures", &tBad_hasFailures, "hasFailures/I");
	treeBad->Branch("nhits", &tBad_nhits, "nhits/I");
	treeBad->Branch("dtcID", &tBad_dtcID, "dtcID[nhits]/I");
	treeBad->Branch("boardID", &tBad_boardID, "boardID[nhits]/I");
	treeBad->Branch("linkID", &tBad_linkID, "linkID[nhits]/I");
	treeBad->Branch("chanID", &tBad_chanID, "chanID[nhits]/I");
	treeBad->Branch("errflag", &tBad_errflag, "errflag[nhits]/I");
	treeBad->Branch("fff", &tBad_fff, "fff[nhits]/I");
	treeBad->Branch("timetot", &tBad_time_tot, "time[nhits]/I");
	treeBad->Branch("ewhit", &tBad_ewhit, "ewhit[nhits]/L");
	treeBad->Branch("peakpos", &tBad_peakpos, "peakpos[nhits]/I");
	treeBad->Branch("peakval", &tBad_peakval, "peakval[nhits]/I");
	treeBad->Branch("nofsamples", &tBad_nofsamples, "nofsamples[nhits]/I");
	treeBad->Branch("firstsample", &tBad_firstsample, "firstsample[nhits]/I");
	treeBad->Branch("nsamples", &tBad_nsamples, "nsamples/I");
	treeBad->Branch("ADC", &tBad_ADC, "ADC[nsamples]/I");
	treeBad->Branch("ADChit", &tBad_ADC_hit);

	treeHits = tfs->make<TTree>("treeHits", "Hit tree");
	treeHits->Branch("run", &tH_run, "run/I");
	treeHits->Branch("subrun", &tH_subrun, "subrun/I");
	treeHits->Branch("nevt", &tH_nevt, "nevt/I");
	treeHits->Branch("dtcID", &tH_dtcID, "dtcID/I");
	treeHits->Branch(
	    "currentDTCEventWindow", &tH_currentDTCEventWindow, "currentDTCEventWindow/L");
	treeHits->Branch(
	    "currentROCEventWindow", &tH_currentROCEventWindow, "currentROCEventWindow/L");
	treeHits->Branch("nhits", &tH_nhits, "nhits/I");
	treeHits->Branch("boardID", &tH_boardID, "boardID/I");
	treeHits->Branch("linkID", &tH_linkID, "linkID/I");
	treeHits->Branch("chanID", &tH_chanID, "chanID/I");
	treeHits->Branch("errflag", &tH_errflag, "errflag/I");
	treeHits->Branch("fff", &tH_fff, "fff/I");
	treeHits->Branch("timetot", &tH_time_tot, "time/I");
	treeHits->Branch("ewhit", &tH_ewhit, "ewhit/L");
	treeHits->Branch("peakpos", &tH_peakpos, "peakpos/I");
	treeHits->Branch("peakval", &tH_peakval, "peakval/I");
	treeHits->Branch("nofsamples", &tH_nofsamples, "nofsamples/I");
	treeHits->Branch("ADC", &tH_ADC);

	TLOG(TLVL_DEBUG + 6) << "Reading data type " << data_type_;

	total_events     = 0;
	total_hits       = 0;
	total_goodhits   = 0;
	total_failedhits = 0;
}

void mu2e::CaloDataAnalyzer::analyze(art::Event const& event) {
	art::EventNumber_t eventNumber = event.event();
	// TLOG(TLVL_INFO) << "mu2e::CaloDataAnalyzer::analyzer eventNumber= " <<
	// (int)eventNumber << std::endl;
	this_eventNumber = (int)eventNumber;

	if(maxEventNum_ > 0 && this_eventNumber > maxEventNum_)
		return;

	t_run       = event.run();
	t_subrun    = event.subRun();
	t_nevt      = event.event();
	tBad_run    = event.run();
	tBad_subrun = event.subRun();
	tBad_nevt   = event.event();
	tH_run      = event.run();
	tH_subrun   = event.subRun();
	tH_nevt     = event.event();

	t_hasFailures = 0;
	t_nhits       = 0;
	t_nsamples    = 0;
	t_ADC_hit->clear();
	tBad_hasFailures = 0;
	tBad_nhits       = 0;
	tBad_nsamples    = 0;
	tBad_ADC_hit->clear();

	nCaloEvents      = 0;
	nCaloHits        = 0;
	event_failedhits = 0;

	// const auto &caloDecoderColl =
	// *event.getValidHandle<CalorimeterDataDecoders>({unpackerModuleLabel_,
	// unpackerInstanceLabel_});

	art::InputTag caloFragmentsTag_(unpackerModuleLabel_);
	auto          caloDecoderColl =
	    event.getValidHandle<std::vector<mu2e::CalorimeterDataDecoder>>(
	        caloFragmentsTag_);

	TLOG(TLVL_DEBUG + 6) << "Iterating through " << caloDecoderColl->size() << " DTCs\n";
	for(auto caloDTC : *caloDecoderColl) {
		processCaloData(caloDTC);
	}
	if(fillEmptyEvents_ || t_nhits > 0) {
		if(fillFailedEvents_ || event_failedhits == 0) {
			tree->Fill();  // Only fill if we have at least 1 hit and no failures
		}
	}
	if(fillEmptyEvents_ || tBad_nhits > 0) {
		treeBad->Fill();
	}
	total_events++;

	g_eventHits->AddPoint(this_eventNumber, t_nhits);

	TLOG(TLVL_DEBUG + 6) << "[CaloDataAnalyzer::analyzer] found " << nCaloEvents
	                     << " calo subevents in event " << (int)eventNumber;
	TLOG(TLVL_DEBUG + 6) << "[CaloDataAnalyzer::analyzer] found " << t_nhits
	                     << " calo hits in event " << (int)eventNumber;

	if(nCaloEvents == 0) {
		TLOG(TLVL_WARNING)
		    << "[CaloDataAnalyzer::analyzer] found no calo subevents in event "
		    << (int)eventNumber << "!";
	}

	// TLOG(TLVL_INFO) << "mu2e::CaloDataAnalyzer::analyzer exiting eventNumber=" <<
	// (int)eventNumber;
}

void mu2e::CaloDataAnalyzer::processCaloData(
    mu2e::CalorimeterDataDecoder const& caloDecoder) {
	auto&    this_subevent  = caloDecoder.event_;
	long int thisDTCEWT     = this_subevent.GetEventWindowTag().GetEventWindowTag(true);
	t_currentDTCEventWindow = thisDTCEWT;
	tBad_currentDTCEventWindow = thisDTCEWT;
	int dtcID                  = int(this_subevent.GetDTCID());

	nCaloEvents++;
	// Iterate over the data blocks (ROCs)
	std::vector<DTCLib::DTC_DataBlock> dataBlocks = this_subevent.GetDataBlocks();
	uint                               nROCs      = caloDecoder.block_count();
	TLOG(TLVL_DEBUG + 6) << "Iterating through " << nROCs << " data blocks (ROCs)\n";
	std::vector<int> roc_hits;
	for(uint iroc = 0; iroc < nROCs; iroc++) {
		long int thisROCEWT =
		    dataBlocks[iroc].GetHeader().get()->GetEventWindowTag().GetEventWindowTag(
		        true);
		g_eventEWT->AddPoint(this_eventNumber, thisROCEWT);
		if(data_type_ == 0) {  /////// STANDARD HITS ///////

			auto caloHits = caloDecoder.GetCalorimeterHitData(iroc);
			uint nHits    = caloHits->size();
			roc_hits.push_back(nHits);
			for(uint ihit = 0; ihit < nHits; ihit++) {
				mu2e::CalorimeterDataDecoder::CalorimeterHitDataPacket hit =
				    caloHits->at(ihit).first;
				std::vector<uint16_t> hit_waveform = caloHits->at(ihit).second;
				if(hit_waveform.size() == 0) {
					TLOG(TLVL_WARNING)
					    << "[CaloDataAnalyzer::analyzer] found empty waveform! DTC "
					    << dtcID << " ROC " << iroc << " hit " << ihit << " BoardID "
					    << hit.BoardID << " ChannelNumber " << hit.ChannelNumber;
				}
				nCaloHits++;
			}
		} else if(data_type_ == 1) {  /////// DEBUG HITS ///////
			auto caloHits = caloDecoder.GetCalorimeterHitTestData(iroc);
			// auto caloHits = caloDecoder.GetCalorimeterHitTestDataNoPointer(iroc);
			uint nHits = caloHits->size();
			roc_hits.push_back(nHits);

			total_hits += nHits;
			for(uint ihit = 0; ihit < nHits; ihit++) {
				mu2e::CalorimeterDataDecoder::CalorimeterHitTestDataPacket hit =
				    caloHits->at(ihit).first;
				std::vector<uint16_t> hit_waveform = caloHits->at(ihit).second;

				// Check that the hit is good
				auto errorCode = caloDAQUtil_.isHitGood(caloHits->at(ihit));
				if(errorCode) {
					h2_failedChannels->Fill(hit.BoardID, hit.ChannelID);
					h2_failedROCs->Fill(dtcID, iroc);
					failure_counter[errorCode]++;
					t_hasFailures++;
					tBad_hasFailures++;
					event_failedhits++;
					total_failedhits++;
					if(verbosity_ > 0) {
						std::cout << "[CaloDataAnalyzer] BAD calo hit! DTC: " << dtcID
						          << ", ROC: " << iroc << ", hit number: " << ihit
						          << " [failure code: " << errorCode << "]" << std::endl;
						caloDAQUtil_.printCaloPulse(hit);
						std::cout << "[CaloDataAnalyzer] \twaveform size \t"
						          << hit_waveform.size() << std::endl;
					}

					if(tBad_nhits >= MAXNHITS)
						continue;
					if(tBad_nsamples + hit_waveform.size() >= MAXNSAMPLES)
						continue;
					tBad_nhits++;
					tBad_dtcID[tBad_nhits - 1]    = dtcID;
					tBad_boardID[tBad_nhits - 1]  = hit.BoardID;
					tBad_linkID[tBad_nhits - 1]   = iroc;
					tBad_chanID[tBad_nhits - 1]   = hit.ChannelID;
					tBad_errflag[tBad_nhits - 1]  = hit.ErrorFlags;
					tBad_fff[tBad_nhits - 1]      = hit.LastSampleMarker;
					tBad_time_tot[tBad_nhits - 1] = hit.Time;
					tBad_ewhit[tBad_nhits - 1]    = hit.InPayloadEventWindowTag;
					tBad_peakpos[tBad_nhits - 1]  = hit.IndexOfMaxDigitizerSample;
					if(hit.IndexOfMaxDigitizerSample >= 0 &&
					   hit.IndexOfMaxDigitizerSample < hit_waveform.size()) {
						tBad_peakval[tBad_nhits - 1] =
						    hit_waveform[hit.IndexOfMaxDigitizerSample];
					} else {
						tBad_peakval[tBad_nhits - 1] = 0;
					}
					tBad_nofsamples[tBad_nhits - 1]  = hit.NumberOfSamples;
					tBad_firstsample[tBad_nhits - 1] = tBad_nsamples;
					for(auto adc : hit_waveform) {
						tBad_ADC[tBad_nsamples] = adc;
						tBad_nsamples++;
					}
					tBad_ADC_hit->push_back(hit_waveform);

					continue;
				}
				nCaloHits++;
				total_goodhits++;

				// Fill hists
				h2_channelHits->Fill(hit.BoardID, hit.ChannelID);
				// g_EWTs->AddPoint(thisROCEWT,hit.InPayloadEventWindowTag);
				h1_t0->Fill(hit.Time);
				h1_maxIndex->Fill(hit.IndexOfMaxDigitizerSample);
				h1_nSamples->Fill(hit.NumberOfSamples);

				for(uint wfi = 0; wfi < hit_waveform.size(); wfi++) {
					h2_waveforms->Fill(wfi, hit_waveform[wfi]);
				}

				// Fill trees
				if(t_nhits >= MAXNHITS) {
					std::cout << "ERROR! This event has more than " << MAXNHITS
					          << " hits (MAXNHITS)\n";
					continue;
				}
				if(t_nsamples + hit_waveform.size() >= MAXNSAMPLES) {
					std::cout << "ERROR! This event has more than " << MAXNSAMPLES
					          << " waveform samples (MAXNSAMPLES)\n";
					continue;
				}

				t_nhits++;
				t_dtcID[t_nhits - 1]       = dtcID;
				t_boardID[t_nhits - 1]     = hit.BoardID;
				t_linkID[t_nhits - 1]      = iroc;
				t_chanID[t_nhits - 1]      = hit.ChannelID;
				t_errflag[t_nhits - 1]     = hit.ErrorFlags;
				t_fff[t_nhits - 1]         = hit.LastSampleMarker;
				t_time_tot[t_nhits - 1]    = hit.Time;
				t_ewhit[t_nhits - 1]       = hit.InPayloadEventWindowTag;
				t_peakpos[t_nhits - 1]     = hit.IndexOfMaxDigitizerSample;
				t_peakval[t_nhits - 1]     = hit_waveform[hit.IndexOfMaxDigitizerSample];
				t_nofsamples[t_nhits - 1]  = hit.NumberOfSamples;
				t_firstsample[t_nhits - 1] = t_nsamples;
				for(auto adc : hit_waveform) {
					t_ADC[t_nsamples] = adc;
					t_nsamples++;
				}
				t_ADC_hit->push_back(hit_waveform);
			}
		} else if(data_type_ == 2) {  /////// COUNTERS ///////
			auto caloHits = caloDecoder.GetCalorimeterCountersData(iroc);
			uint nHits    = caloHits->size();
			roc_hits.push_back(nHits);
			for(uint ihit = 0; ihit < nHits; ihit++) {
				mu2e::CalorimeterDataDecoder::CalorimeterCountersDataPacket hit =
				    caloHits->at(ihit).first;
				std::vector<uint32_t> hit_counters = caloHits->at(ihit).second;

				nCaloHits++;

				tH_linkID     = iroc;
				tH_nofsamples = hit.numberOfCounters;
				tH_ADC->clear();
				for(auto adc : hit_counters) {
					tH_ADC->push_back(adc);
				}
				treeHits->Fill();

			}  // end of hit loop
		}
	}  // loop over ROCs
}

void mu2e::CaloDataAnalyzer::endJob() {
	std::cout << "\n ----- [CaloDataAnalyzer] Decoding errors summary ----- "
	          << std::endl;
	std::cout << "Total events: " << total_events << "\n";
	std::cout << "Total hits: " << total_hits << "\n";
	std::cout << "Total good hits: " << total_goodhits << "\n";
	std::cout << "Total failed hits: " << total_failedhits << " ["
	          << int(100. * total_failedhits / total_hits) << "%]\n";
	for(auto fail : failure_counter) {
		std::cout << "Failure mode " << fail.first << " [bad "
		          << caloDAQUtil_.getCaloHitErrorName(fail.first)
		          << "], count: " << fail.second << " ("
		          << int(100. * fail.second / total_hits) << "%)\n";
	}
}

DEFINE_ART_MODULE(mu2e::CaloDataAnalyzer)
