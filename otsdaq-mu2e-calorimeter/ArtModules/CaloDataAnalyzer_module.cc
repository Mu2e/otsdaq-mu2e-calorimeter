#include "art/Framework/Core/EDAnalyzer.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Services/Registry/ServiceHandle.h"
#include "art/Framework/Principal/Handle.h"
#include "art/Framework/Principal/Run.h"
#include "messagefacility/MessageLogger/MessageLogger.h"

#include "TRACE/tracemf.h"
#include "artdaq/DAQdata/Globals.hh"
#define TRACE_NAME "CaloDataAnalyzer"

#include "canvas/Utilities/Exception.h"
#include "canvas/Utilities/InputTag.h"

#include <artdaq-core/Data/ContainerFragment.hh>
#include "artdaq-core/Data/Fragment.hh"

#include "artdaq-core-mu2e/Overlays/DTCEventFragment.hh"
#include "artdaq-core-mu2e/Overlays/FragmentType.hh"
#include "artdaq-core-mu2e/Data/EventHeader.hh"
#include "artdaq-core-mu2e/Data/CalorimeterDataDecoder.hh"

#include "cetlib_except/exception.h"

#include <iomanip>
#include <sstream>
#include <vector>

#include "art_root_io/TFileService.h"
#include "TH1.h"
#include "TH2.h"
#include "TGraph.h"
#include "TTree.h"


namespace mu2e {
  class CaloDataAnalyzer : public art::EDAnalyzer
  {
  public:
    struct Config {
      fhicl::Atom<std::string> filterModuleLabel {fhicl::Name("filterModuleLabel" ) , fhicl::Comment("filterModuleLabel"), ""};
      fhicl::Atom<std::string> filterInstanceLabel {fhicl::Name("filterInstanceLabel" ) , fhicl::Comment("filterInstanceLabel"), ""};
      fhicl::Atom<int> verbosity {fhicl::Name("verbosity" ) , fhicl::Comment("Verbosity [0-2]"), 0};
      fhicl::Atom<int> data_type {fhicl::Name("dataType" ) , fhicl::Comment("Data type (0:standard, 1:debug, 2:counters)"), 0};
    };

    explicit CaloDataAnalyzer(const art::EDAnalyzer::Table<Config>& config);
	void analyze(art::Event const& event) override;

    long int getEventWindow(ushort ewt[3]){
      long int eventWindow = static_cast<long int>(ewt[0]) +
        (static_cast<long int>(ewt[1]) << 16) +
        (static_cast<long int>(ewt[2]) << 32);
      return eventWindow;
    }
    
    void processDTCData(mu2e::CalorimeterDataDecoder const& caloDecoder);

  private:
    std::string   filterModuleLabel_;
    std::string   filterInstanceLabel_;
    int           verbosity_;
    int           data_type_;
    
    size_t nCaloEvents;
    size_t nCaloHits;
    int this_eventNumber;

    TH1D* h1_t0;
    TH1D* h1_maxIndex;
    TH1D* h1_nSamples;
    TH2D* h2_channelHits;
    TH2D* h2_waveforms;
    TGraph* g_EWTs;
    TGraph* g_eventHits;
    TGraph* g_eventEWT;

    static const int nROCs = 6;
    static const int nCHs = 20;
    static const int nCHANs = 2;
    static const int MAXNHITS = 150;
    static const int MAXNSAMPLES = 6300;
 
    int t_run;
    int t_subrun;
    int t_nevt;
    int t_dtcID;
    Long64_t t_currentDTCEventWindow;
    Long64_t t_currentROCEventWindow[nROCs];
    int t_nhits;
    int t_boardID[MAXNHITS];
    int t_linkID[MAXNHITS];
    int t_chanID[MAXNHITS];
    int t_errflag[MAXNHITS];
    int t_fff[MAXNHITS];
    int t_time_tot[MAXNHITS];
    int t_ewhit[MAXNHITS];
    int t_peakpos[MAXNHITS];
    int t_peakval[MAXNHITS];
    int t_nofsamples[MAXNHITS];
    int t_firstsample[MAXNHITS];
    int t_nsamples;
    int t_ADC[MAXNSAMPLES];


    int tH_run;
    int tH_subrun;
    int tH_nevt;
    int tH_dtcID;
    Long64_t tH_currentDTCEventWindow;
    Long64_t tH_currentROCEventWindow;
    int tH_nhits;
    int tH_boardID;
    int tH_linkID;
    int tH_chanID;
    int tH_errflag;
    int tH_fff;
    int tH_time_tot;
    int tH_ewhit;
    int tH_peakpos;
    int tH_peakval;
    int tH_nofsamples;
    std::vector<int>* tH_ADC=0;

    TTree* tree;
    TTree* treeHits;
  
  };
}  // namespace mu2e

mu2e::CaloDataAnalyzer::CaloDataAnalyzer(const art::EDAnalyzer::Table<Config>& config)
  : art::EDAnalyzer{config},
    filterModuleLabel_(config().filterModuleLabel()),
    filterInstanceLabel_(config().filterInstanceLabel()),
    verbosity_(config().verbosity()),
    data_type_(config().data_type())
{

  art::ServiceHandle<art::TFileService> tfs;

  h1_t0 = tfs->make<TH1D>("h1_t0","t0 distribution;t0",2000,0,20000);
  h1_maxIndex = tfs->make<TH1D>("h1_maxIndex","max Waveform Sample Index;Index",100,0,100);
  h1_nSamples = tfs->make<TH1D>("h1_nSamples","Waveform length;nSamples",100,0,100);
  h2_channelHits = tfs->make<TH2D>("h2_channelHits","Hits per channel;BoardID;ChannelID",256,0,256,20,0,20);
  h2_waveforms = tfs->make<TH2D>("h2_waveforms","All waveforms;ADC;clock ticks",100,0,100,4096,0,4096);
  g_EWTs = tfs->makeAndRegister<TGraph>("g_EWTs", "EWT check;ROC EWT;Hit EWT");
  g_eventHits = tfs->makeAndRegister<TGraph>("g_eventHits", "Total hits per event;EventNumber;Hits");
  g_eventEWT = tfs->makeAndRegister<TGraph>("g_eventEWT", "Event EWT;EventNumber;EWT");
  g_EWTs->SetMarkerStyle(20);
  g_eventHits->SetMarkerStyle(20);
  g_eventEWT->SetMarkerStyle(20);

  tree = tfs->make<TTree>("tree","Event tree");
  tree->Branch("run",&t_run,"run/I");
  tree->Branch("subrun",&t_subrun,"subrun/I");
  tree->Branch("nevt",&t_nevt,"nevt/I");
  tree->Branch("dtcID",&t_dtcID, "dtcID/I");
  tree->Branch("currentDTCEventWindow",&t_currentDTCEventWindow,"currentDTCEventWindow/L");
  tree->Branch("currentROCEventWindow", &t_currentROCEventWindow, "currentROCEventWindow[6]/L");
  tree->Branch("nhits",&t_nhits,"nhits/I");
  tree->Branch("boardID",&t_boardID, "boardID[nhits]/I");
  tree->Branch("linkID",&t_linkID, "linkID[nhits]/I");
  tree->Branch("chanID",&t_chanID,"chanID[nhits]/I");
  tree->Branch("errflag",&t_errflag, "errflag[nhits]/I");
  tree->Branch("fff",&t_fff, "fff[nhits]/I");
  tree->Branch("time",&t_time_tot, "time[nhits]/I");
  tree->Branch("ewhit",&t_ewhit, "ewhit[nhits]/I");
  tree->Branch("peakpos",&t_peakpos,"peakpos[nhits]/I");
  tree->Branch("peakval",&t_peakval,"peakval[nhits]/I");
  tree->Branch("nofsamples",&t_nofsamples, "nofsamples[nhits]/I");
  tree->Branch("firstsample",&t_firstsample, "firstsample[nhits]/I");
  tree->Branch("nsamples",&t_nsamples, "nsamples/I");
  tree->Branch("ADC",&t_ADC, "ADC[nsamples]/I");

  treeHits = tfs->make<TTree>("treeHits","Hit tree");
  treeHits->Branch("run",&tH_run,"run/I");
  treeHits->Branch("subrun",&tH_subrun,"subrun/I");
  treeHits->Branch("nevt",&tH_nevt,"nevt/I");
  treeHits->Branch("dtcID",&tH_dtcID, "dtcID/I");
  treeHits->Branch("currentDTCEventWindow",&tH_currentDTCEventWindow,"currentDTCEventWindow/L");
  treeHits->Branch("currentROCEventWindow", &tH_currentROCEventWindow, "currentROCEventWindow/L");
  treeHits->Branch("nhits",&tH_nhits,"nhits/I");
  treeHits->Branch("boardID",&tH_boardID, "boardID/I");
  treeHits->Branch("linkID",&tH_linkID, "linkID/I");
  treeHits->Branch("chanID",&tH_chanID,"chanID/I");
  treeHits->Branch("errflag",&tH_errflag, "errflag/I");
  treeHits->Branch("fff",&tH_fff, "fff/I");
  treeHits->Branch("time",&tH_time_tot, "time/I");
  treeHits->Branch("ewhit",&tH_ewhit, "ewhit/I");
  treeHits->Branch("peakpos",&tH_peakpos,"peakpos/I");
  treeHits->Branch("peakval",&tH_peakval,"peakval/I");
  treeHits->Branch("nofsamples",&tH_nofsamples, "nofsamples/I");
  treeHits->Branch("ADC",&tH_ADC);

  TLOG(TLVL_DEBUG + 6) << "Reading data type " << data_type_;

}

void mu2e::CaloDataAnalyzer::analyze(art::Event const& event){

  art::EventNumber_t eventNumber = event.event();
  //TLOG(TLVL_INFO) << "mu2e::CaloDataAnalyzer::analyzer eventNumber= " << (int)eventNumber << std::endl;
  this_eventNumber = (int)eventNumber;

  t_run = event.run();
  t_subrun = event.subRun();
  t_nevt = event.event();
  tH_run = event.run();
  tH_subrun = event.subRun();
  tH_nevt = event.event();

  nCaloEvents = 0;
  nCaloHits = 0;

  const auto &caloDecoderColl = *event.getValidHandle<CalorimeterDataDecoders>({filterModuleLabel_, filterInstanceLabel_});

  TLOG(TLVL_DEBUG + 6) << "Iterating through " << caloDecoderColl.size() << " DTCs\n";
  for (const auto& caloDTC : caloDecoderColl) {
    processDTCData(caloDTC);
  }

  g_eventHits->AddPoint(this_eventNumber,nCaloHits);

  TLOG(TLVL_DEBUG + 6) << "[CaloDataAnalyzer::analyzer] found " << nCaloEvents << " calo subevents in event" << (int)eventNumber;
  TLOG(TLVL_DEBUG + 6) << "[CaloDataAnalyzer::analyzer] found " << nCaloHits << " calo hits in event" << (int)eventNumber;
  
  if (nCaloEvents == 0) {
    TLOG(TLVL_WARNING) << "[CaloDataAnalyzer::analyzer] found no calo subevents in event" << (int)eventNumber << "!";
  }

  //TLOG(TLVL_INFO) << "mu2e::CaloDataAnalyzer::analyzer exiting eventNumber=" << (int)eventNumber;
}

void mu2e::CaloDataAnalyzer::processDTCData(mu2e::CalorimeterDataDecoder const& caloDecoder){

  auto& this_subevent = caloDecoder.event_;
  long int thisDTCEWT = this_subevent.GetEventWindowTag().GetEventWindowTag(true);
  uint64_t dtcID = this_subevent.GetDTCID();

  nCaloEvents++;
  // Iterate over the data blocks (ROCs)
  std::vector<DTCLib::DTC_DataBlock> dataBlocks = this_subevent.GetDataBlocks();
  uint nROCs = dataBlocks.size();
  TLOG(TLVL_DEBUG + 6) << "Iterating through " << nROCs << " data blocks (ROCs)\n";
  std::vector<int> roc_hits;
  for (uint iroc = 0; iroc < nROCs; iroc++){

    long int thisROCEWT = dataBlocks[iroc].GetHeader().get()->GetEventWindowTag().GetEventWindowTag(true);
    g_eventEWT->AddPoint(this_eventNumber,thisROCEWT);
    if (data_type_ == 0){ /////// STANDARD HITS ///////
    
      auto caloHits = caloDecoder.GetCalorimeterHitData(iroc);
      uint nHits = caloHits->size();
      roc_hits.push_back(nHits);
      for (uint ihit = 0; ihit<nHits; ihit++){
        mu2e::CalorimeterDataDecoder::CalorimeterHitDataPacket hit = caloHits->at(ihit).first;
        std::vector<uint16_t> hit_waveform = caloHits->at(ihit).second;
        if (hit_waveform.size() == 0){
          TLOG(TLVL_WARNING) << "[CaloDataAnalyzer::analyzer] found empty waveform! DTC " << dtcID << " ROC " << iroc << " hit " << ihit
            << " BoardID " << hit.BoardID << " ChannelNumber " << hit.ChannelNumber;
        }
        nCaloHits++;
      }
    
    } else if (data_type_ == 1){ /////// DEBUG HITS ///////
      auto caloHits = caloDecoder.GetCalorimeterHitTestData(iroc);
      uint nHits = caloHits->size();
      roc_hits.push_back(nHits);
      
      
      t_dtcID = dtcID;
      t_currentDTCEventWindow = thisDTCEWT;
      t_currentROCEventWindow[nROCs] = thisROCEWT;
      t_nhits = nHits;
      t_nsamples = 0;
      tH_dtcID = dtcID;
      tH_currentDTCEventWindow = thisDTCEWT;
      tH_currentROCEventWindow = thisROCEWT;
      tH_nhits = nHits;
      for (uint ihit = 0; ihit<nHits; ihit++){
        mu2e::CalorimeterDataDecoder::CalorimeterHitTestDataPacket hit = caloHits->at(ihit).first;
        std::vector<uint16_t> hit_waveform = caloHits->at(ihit).second;
        
        nCaloHits++;

        //Fill hists
        h2_channelHits->Fill(hit.BoardID,hit.ChannelID);
        g_EWTs->AddPoint(thisROCEWT,hit.InPayloadEventWindowTag);
        h1_t0->Fill(hit.Time);
        h1_maxIndex->Fill(hit.IndexOfMaxDigitizerSample);
        h1_nSamples->Fill(hit.NumberOfSamples);
        for (uint wfi=0; wfi<hit_waveform.size(); wfi++){
          h2_waveforms->Fill(wfi,hit_waveform[wfi]);
        }

        //Fill trees
        t_boardID[ihit] = hit.BoardID;
        t_linkID[ihit] = iroc;
        t_chanID[ihit] = hit.ChannelID;
        t_errflag[ihit] = hit.ErrorFlags;
        t_fff[ihit] = hit.LastSampleMarker;
        t_time_tot[ihit] = hit.Time;
        t_ewhit[ihit] = hit.InPayloadEventWindowTag;
        t_peakpos[ihit] = hit.IndexOfMaxDigitizerSample;
        t_peakval[ihit] = hit_waveform[hit.IndexOfMaxDigitizerSample];
        t_nofsamples[ihit] = hit.NumberOfSamples;
        t_firstsample[ihit] = t_nsamples;
        for (auto adc : hit_waveform){
          t_ADC[t_nsamples] = adc;
          t_nsamples++;
        }
        tH_boardID = hit.BoardID;
        tH_linkID = iroc;
        tH_chanID = hit.ChannelID;
        tH_errflag = hit.ErrorFlags;
        tH_fff = hit.LastSampleMarker;
        tH_time_tot = hit.Time;
        tH_ewhit = hit.InPayloadEventWindowTag;
        tH_peakpos = hit.IndexOfMaxDigitizerSample;
        tH_peakval = hit_waveform[hit.IndexOfMaxDigitizerSample];
        tH_nofsamples = hit.NumberOfSamples;
        tH_ADC->clear();
        for (auto adc : hit_waveform){
          tH_ADC->push_back(adc);
        }
        treeHits->Fill();
      }
      tree->Fill();
    
    } else if (data_type_ == 2){ /////// COUNTERS ///////
      auto caloHits = caloDecoder.GetCalorimeterCountersData(iroc);
      uint nHits = caloHits->size();
      roc_hits.push_back(nHits);
      for (uint ihit = 0; ihit<nHits; ihit++){
        mu2e::CalorimeterDataDecoder::CalorimeterCountersDataPacket hit = caloHits->at(ihit).first;
        std::vector<uint32_t> hit_counters = caloHits->at(ihit).second;
        
        nCaloHits++;
  
        tH_linkID = iroc;
        tH_nofsamples = hit.numberOfCounters;
        tH_ADC->clear();
        for (auto adc : hit_counters){
          tH_ADC->push_back(adc);
        }
        treeHits->Fill();
        
      } // end of hit loop
    }
  } // loop over ROCs

}

DEFINE_ART_MODULE(mu2e::CaloDataAnalyzer)
