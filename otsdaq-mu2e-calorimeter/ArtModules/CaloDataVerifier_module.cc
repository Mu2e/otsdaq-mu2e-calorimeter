#include "art/Framework/Core/EDFilter.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Principal/Event.h"
//#include "art/Framework/Services/Registry/ServiceHandle.h"
#include "art/Framework/Principal/Handle.h"
#include "art/Framework/Principal/Run.h"
#include "messagefacility/MessageLogger/MessageLogger.h"

#include "TRACE/tracemf.h"
#include "artdaq/DAQdata/Globals.hh"
#define TRACE_NAME "CaloDataVerifier"

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


namespace mu2e {
  class CaloDataVerifier : public art::EDFilter
  {
  public:
    struct Config {
      fhicl::Atom<int> verbosity {fhicl::Name("verbosity" ) , fhicl::Comment("Verbosity [0-2]"), 0};
      fhicl::Atom<int> data_type {fhicl::Name("dataType" ) , fhicl::Comment("Data type (0:standard, 1:debug, 2:counters)"), 0};
      fhicl::Atom<int> metrics_level {fhicl::Name("metricsLevel" ) , fhicl::Comment("Metrics reporting level"), 1};
      fhicl::Atom<bool> produce_calo_decoders {fhicl::Name("produceCaloDecoders" ) , fhicl::Comment("Produce calo decoders [default: true]"), true};
      fhicl::Atom<bool> stop_on_failure {fhicl::Name("stopOnFailure" ) , fhicl::Comment("Throw exception if checks fail [default: false]"), false};
      fhicl::Atom<bool> check_ewts {fhicl::Name("checkEWTs" ) , fhicl::Comment("Check for EWT continuity across events [default: false]"), false};
      fhicl::Atom<std::string> subsystem_override {fhicl::Name("subsystemOverride" ) , fhicl::Comment("Override calo subsystem [\"calo\", \"tracker\"]"), "calo"};
    };

    explicit CaloDataVerifier(const art::EDFilter::Table<Config>& config);

    bool filter(art::Event & e) override;
    virtual bool endRun(art::Run& run ) override;
    
    artdaq::Fragments getFragments(art::Event& event);
    void processCaloData(mu2e::DTCEventFragment& eventFragment, std::unique_ptr<std::vector<mu2e::CalorimeterDataDecoder>> const& caloDecoderColl);

    bool checkAndUpdateDTCEWT(const DTCLib::DTC_SubEvent& subevent);
    bool checkAndUpdateROCEWT(const DTCLib::DTC_DataBlock& dataBlock);
    bool checkHitBeginMarker(const mu2e::CalorimeterDataDecoder::CalorimeterHitTestDataPacket& hit);
    bool checkHitLastMarker(const mu2e::CalorimeterDataDecoder::CalorimeterHitTestDataPacket& hit);
    bool checkAndUpdateROCCounter(const DTCLib::DTC_DataBlock& dataBlock, const std::vector<uint32_t>& hit_counters);
    bool checkCounters(const DTCLib::DTC_DataBlock& dataBlock, const std::vector<uint32_t>& hit_counters);

    void printEvent(const DTCLib::DTC_Event& dtcevent);
    void printSubEvent(const DTCLib::DTC_SubEvent& subevent);
    void printEventHeader(const DTCLib::DTC_Event& dtcevent);
    void printDTCHeader(const DTCLib::DTC_SubEvent& subevent);
    void printROC(const DTCLib::DTC_DataBlock& dataBlock);
    void printRunSummary();

  private:
    int           verbosity_;
    int           data_type_;
    int           metrics_reporting_level_;
    bool          produceCaloDecoders_;
    bool          stopOnFailure_;
    bool          checkEWTs_;
    DTCLib::DTC_Subsystem subsystem_;
    
    size_t nCaloEvents;
    size_t nCaloHits;

    std::map<int, int> lastDTCEventWindow;
    std::map<int, std::map<int, int>> lastROCEventWindow;
    std::map<int, std::map<int, uint32_t>> lastROCCounter;

    std::map<int, std::map<int, int>> totalHitMap;
    std::map<int, int> failMap_DTCEWTs;
    std::map<int, std::map<int, int>> failMap_ROCEWTs;
    std::map<int, std::map<int, int>> failMap_ROCCounters;
    std::map<int, std::map<int, int>> failMap_Counters;
    std::map<int, std::map<int, int>> failMap_BeginMarker;
    std::map<int, std::map<int, int>> failMap_LastMarker;
    std::map<int, std::map<int, int>> failMap_StatusError;

    int event_failed_DTCEWTs;
    int event_failed_ROCEWTs;
    int event_failed_ROCCounters;
    int event_failed_Counters;
    int event_failed_BeginMarker;
    int event_failed_LastMarker;

    int total_failed_DTCEWTs;
    int total_failed_ROCEWTs;
    int total_failed_ROCCounters;
    int total_failed_Counters;
    int total_failed_BeginMarker;
    int total_failed_LastMarker;

    art::Event* previousArtEvent;
    DTCLib::DTC_Event* previousDTCEvent;
    bool failedEvent;
  
  };
}  // namespace mu2e

mu2e::CaloDataVerifier::CaloDataVerifier(const art::EDFilter::Table<Config>& config)
  : art::EDFilter{config}, 
    verbosity_(config().verbosity()),
    data_type_(config().data_type()),
    metrics_reporting_level_(config().metrics_level()),
    produceCaloDecoders_(config().produce_calo_decoders()),
    stopOnFailure_(config().stop_on_failure()),
    checkEWTs_(config().check_ewts())
{
  if (config().subsystem_override() == "calo"){
    subsystem_ = DTCLib::DTC_Subsystem::DTC_Subsystem_Calorimeter;
  } else if (config().subsystem_override() == "tracker"){
    subsystem_ = DTCLib::DTC_Subsystem::DTC_Subsystem_Tracker;
  }

  if (produceCaloDecoders_) {
    produces<std::vector<mu2e::CalorimeterDataDecoder>>();
  }

  TLOG(TLVL_DEBUG + 6) << "Reading data type " << data_type_;

  total_failed_DTCEWTs = 0;
  total_failed_ROCEWTs = 0;
  total_failed_ROCCounters = 0;
  total_failed_Counters = 0;
  total_failed_BeginMarker = 0;
  total_failed_LastMarker = 0;

  failedEvent = false;
}

bool mu2e::CaloDataVerifier::filter(art::Event& event){

  art::EventNumber_t eventNumber = event.event();
  //TLOG(TLVL_INFO) << "mu2e::CaloDataVerifier::filter eventNumber= " << (int)eventNumber << std::endl;

  //Prepare vector of output data decoders
  std::unique_ptr<std::vector<mu2e::CalorimeterDataDecoder>> caloDecoderColl(new std::vector<mu2e::CalorimeterDataDecoder>);

  nCaloEvents = 0;
  nCaloHits = 0;
  event_failed_DTCEWTs = 0;
  event_failed_ROCEWTs = 0;
  event_failed_ROCCounters = 0;
  event_failed_Counters = 0;
  event_failed_BeginMarker = 0;
  event_failed_LastMarker = 0;

  artdaq::Fragments fragments = getFragments(event);
  TLOG(TLVL_DEBUG + 6) << "Iterating through " << fragments.size() << " fragments\n";
  for (const auto& frag : fragments) {
    mu2e::DTCEventFragment eventFragment(frag);
    processCaloData(eventFragment, caloDecoderColl);

    total_failed_DTCEWTs += event_failed_DTCEWTs;
    total_failed_ROCEWTs += event_failed_ROCEWTs;
    total_failed_ROCCounters += event_failed_ROCCounters;
    total_failed_Counters += event_failed_Counters;
    total_failed_BeginMarker += event_failed_BeginMarker;
    total_failed_LastMarker += event_failed_LastMarker;

    if (failedEvent && verbosity_ > 0){
      std::cout << "Failed event " << (int)eventNumber << ", stopping here\n";
      std::cout << "Previous event was " << (int)(previousArtEvent->event()) << "\n";

      if (stopOnFailure_) throw cet::exception("CaloDataVerifier") << "Failure detected!";
    }
  }

  //Send metrics
  if (metricMan != nullptr){
    //Failures
    metricMan->sendMetric("failed_DTCEWTs", event_failed_DTCEWTs, "Errors in DTC EWT",
      metrics_reporting_level_, artdaq::MetricMode::Accumulate);
    metricMan->sendMetric("failed_ROCEWTs", event_failed_ROCEWTs, "Errors in DTC EWT",
      metrics_reporting_level_, artdaq::MetricMode::Accumulate);
    metricMan->sendMetric("failed_ROCCounters", event_failed_ROCCounters, "Errors in ROC Counters",
      metrics_reporting_level_, artdaq::MetricMode::Accumulate);
    metricMan->sendMetric("failed_Counters", event_failed_Counters, "Errors in payload Counters",
      metrics_reporting_level_, artdaq::MetricMode::Accumulate);
    metricMan->sendMetric("failed_BeginMarker", event_failed_BeginMarker, "Errors in hit Begin Marker",
      metrics_reporting_level_, artdaq::MetricMode::Accumulate);
    metricMan->sendMetric("failed_LastMarker", event_failed_LastMarker, "Errors in hit Last Marker",
      metrics_reporting_level_, artdaq::MetricMode::Accumulate);

    //Diagnostic
    metricMan->sendMetric("nCaloDTCs", int(nCaloEvents), "Calo DTCs",
      metrics_reporting_level_, artdaq::MetricMode::LastPoint);
    metricMan->sendMetric("nHits", int(nCaloHits), "Hits per event",
      metrics_reporting_level_, artdaq::MetricMode::LastPoint);
  }

  TLOG(TLVL_DEBUG + 6) << "[CaloDataVerifier::filter] found " << nCaloEvents << " calo subevents in event" << (int)eventNumber;
  TLOG(TLVL_DEBUG + 6) << "[CaloDataVerifier::filter] found " << nCaloHits << " calo hits in event" << (int)eventNumber;
  
  if (nCaloEvents == 0) {
    TLOG(TLVL_WARNING) << "[CaloDataVerifier::filter] found no calo subevents in event" << (int)eventNumber << "!";
  }
  if (produceCaloDecoders_) {
    event.put(std::move(caloDecoderColl));
  }

  //TLOG(TLVL_INFO) << "mu2e::CaloDataVerifier::filter exiting eventNumber=" << (int)eventNumber;
  previousArtEvent = &event;
  return true;
}


bool mu2e::CaloDataVerifier::endRun( art::Run&  ) {

  if (verbosity_ > 0){
    printRunSummary();
  }

  return true;
}




artdaq::Fragments mu2e::CaloDataVerifier::getFragments(art::Event& event){
  artdaq::Fragments fragments;
  artdaq::FragmentPtrs containerFragments;

  std::vector<art::Handle<artdaq::Fragments>> fragmentHandles;
  fragmentHandles = event.getMany<std::vector<artdaq::Fragment>>();

  TLOG(TLVL_DEBUG + 6) << "Iterating through " << fragmentHandles.size() << " fragment handles\n";
  for (const auto& handle : fragmentHandles) {
    if (!handle.isValid() || handle->empty()) {
      continue;
    }

    if (handle->front().type() == artdaq::Fragment::ContainerFragmentType) {
      for (const auto& cont : *handle) {
        artdaq::ContainerFragment contf(cont);
        if (contf.fragment_type() != mu2e::FragmentType::DTCEVT) {
          break;
        }

        for (size_t ii = 0; ii < contf.block_count(); ++ii) {
          containerFragments.push_back(contf[ii]);
          fragments.push_back(*containerFragments.back());
        }
      }
    } else {
      if (handle->front().type() == mu2e::FragmentType::DTCEVT) {
        for (auto frag : *handle) {
          fragments.emplace_back(frag);
        }
      }
    }
  }
  return fragments;
}

void mu2e::CaloDataVerifier::processCaloData(mu2e::DTCEventFragment& eventFragment, std::unique_ptr<std::vector<mu2e::CalorimeterDataDecoder>> const& caloDecoderColl){

  failedEvent = false;

  DTCLib::DTC_Event dtcevent = eventFragment.getData();
  //DTCLib::DTC_EventHeader* eventHeader = dtcevent.GetHeader();
  std::vector<DTCLib::DTC_SubEvent> subevents = dtcevent.GetSubEvents();
  TLOG(TLVL_DEBUG + 6) << "Found " << subevents.size() << " total subevents (DTCs)\n";

  auto caloSubEvents = eventFragment.getSubsystemData(subsystem_);
  TLOG(TLVL_DEBUG + 6) << "Iterating through " << caloSubEvents.size() << " calorimeter subevents (DTCs)\n";

  //Loop over calo DTCs
  for (const auto& subevent : caloSubEvents) {

    //Create caloDecoder instance -> it will create an internal memory copy of this subevent
    caloDecoderColl->emplace_back(subevent);
    mu2e::CalorimeterDataDecoder& caloDecoder = caloDecoderColl->back();
    auto& this_subevent = caloDecoder.event_;

    uint64_t dtcID = this_subevent.GetDTCID();
    if (checkEWTs_ && !checkAndUpdateDTCEWT(this_subevent)){
      event_failed_DTCEWTs++;
      failMap_DTCEWTs[dtcID]++;
      failedEvent = true;
    }

    nCaloEvents++;
    // Iterate over the data blocks (ROCs)
    std::vector<DTCLib::DTC_DataBlock> dataBlocks = this_subevent.GetDataBlocks();
    uint nROCs = dataBlocks.size();
    TLOG(TLVL_DEBUG + 6) << "Iterating through " << nROCs << " data blocks (ROCs)\n";
    std::vector<int> roc_hits;
    for (uint iroc = 0; iroc < nROCs; iroc++){

      if (checkEWTs_ && !checkAndUpdateROCEWT(dataBlocks[iroc])){
        event_failed_ROCEWTs++;
        failMap_ROCEWTs[dtcID][iroc]++;
        failedEvent = true;
      }

      if (data_type_ == 0){ /////// STANDARD HITS ///////
      
        auto caloHits = caloDecoder.GetCalorimeterHitData(iroc);
        uint nHits = caloHits->size();
        roc_hits.push_back(nHits);
        for (uint ihit = 0; ihit<nHits; ihit++){
          mu2e::CalorimeterDataDecoder::CalorimeterHitDataPacket hit = caloHits->at(ihit).first;
          std::vector<uint16_t> hit_waveform = caloHits->at(ihit).second;
          if (hit_waveform.size() == 0){
            TLOG(TLVL_WARNING) << "[CaloDataVerifier::filter] found empty waveform! DTC " << dtcID << " ROC " << iroc << " hit " << ihit
              << " BoardID " << hit.BoardID << " ChannelNumber " << hit.ChannelNumber;
          }
          nCaloHits++;
        }
      
      } else if (data_type_ == 1){ /////// DEBUG HITS ///////
        auto caloHits = caloDecoder.GetCalorimeterHitTestData(iroc);
        uint nHits = caloHits->size();
        roc_hits.push_back(nHits);
        
        for (uint ihit = 0; ihit<nHits; ihit++){
          mu2e::CalorimeterDataDecoder::CalorimeterHitTestDataPacket hit = caloHits->at(ihit).first;
          std::vector<uint16_t> hit_waveform = caloHits->at(ihit).second;
          if (!checkHitBeginMarker(hit)){
            event_failed_BeginMarker++;
            failMap_BeginMarker[dtcID][iroc]++;
            failedEvent = true;
          }
          if (!checkHitLastMarker(hit)){
            event_failed_LastMarker++;
            failMap_LastMarker[dtcID][iroc]++;
            failedEvent = true;
          }
          if (hit_waveform.size() == 0){
            TLOG(TLVL_WARNING) << "[CaloDataVerifier::filter] found empty waveform! DTC " << dtcID << " ROC " << iroc << " hit " << ihit
              << " BoardID " << hit.BoardID << " ChannelID " << hit.ChannelID;
          }
          nCaloHits++;
          totalHitMap[hit.BoardID][hit.ChannelID]++;
    
          TLOG(TLVL_DEBUG + 6)
            << "Hit "                          << ihit << " :"                                  << std::endl
            << "\tBeginMarker: "               << std::hex << hit.BeginMarker << std::dec       << std::endl
            << "\tBoardID: "                   << hit.BoardID                                   << std::endl
            << "\tChannelID: "                 << hit.ChannelID                                 << std::endl
            << "\tInPayloadEventWindowTag: "   << hit.InPayloadEventWindowTag                   << std::endl
            << "\tLastSampleMarker: "          << std::hex << hit.LastSampleMarker << std::dec  << std::endl
            << "\tErrorFlags: "                << hit.ErrorFlags                                << std::endl
            << "\tTime: "                      << hit.Time                                      << std::endl
            << "\tIndexOfMaxDigitizerSample: " << hit.IndexOfMaxDigitizerSample                 << std::endl
            << "\tNumberOfSamples: "           << hit.NumberOfSamples                           << std::endl
            << "\twaveform size: "             << hit_waveform.size()                           << std::endl;

          std::stringstream ss_wf;
          for (auto sample : hit_waveform) ss_wf << sample << " ";
          TLOG(TLVL_DEBUG + 6) << "Waveform:\n" << ss_wf.str();

        }

      } else if (data_type_ == 2){ /////// COUNTERS ///////
        auto caloHits = caloDecoder.GetCalorimeterCountersData(iroc);
        uint nHits = caloHits->size();
        roc_hits.push_back(nHits);
        for (uint ihit = 0; ihit<nHits; ihit++){
          mu2e::CalorimeterDataDecoder::CalorimeterCountersDataPacket hit = caloHits->at(ihit).first;
          std::vector<uint32_t> hit_counters = caloHits->at(ihit).second;
          if (hit_counters.size() == 0){
            TLOG(TLVL_WARNING) << "[CaloDataVerifier::filter] found empty counters! DTC " << dtcID << " ROC " << iroc << " hit " << ihit;
          }
          nCaloHits++;

          if (checkEWTs_ && !checkAndUpdateROCCounter(dataBlocks[iroc], hit_counters)){
            event_failed_ROCCounters++;
            failedEvent = true;
            failMap_ROCCounters[dtcID][iroc]++;
          }

          if (!checkCounters(dataBlocks[iroc], hit_counters)){
            event_failed_Counters++;
            failedEvent = true;
            failMap_Counters[dtcID][iroc]++;
            if (verbosity_ > 1){
              std::cout << "Dumping these counters:\n";
              for (auto c : hit_counters){
                std::cout << c << " ";
              }
              std::cout << std::endl;
            }
          }
    
          TLOG(TLVL_DEBUG + 6)
            << "Hit "                          << ihit << " :"                                  << std::endl
            << "\numberOfCounters: "           << hit.numberOfCounters                          << std::endl
            << "\tcounters size: "             << hit_counters.size()                           << std::endl;

          std::stringstream ss_wf;
          for (auto sample : hit_counters) ss_wf << sample << " ";
          TLOG(TLVL_DEBUG + 6) << "Counters:\n" << ss_wf.str();

          std::stringstream ss_wf_hex;
          for (auto sample : hit_counters) ss_wf_hex << std::hex << std::setw(8) << std::setfill('0') << sample << std::dec << " ";
          TLOG(TLVL_DEBUG + 6) << "Counters (hex):\n" << ss_wf_hex.str();
          
        } // end of hit loop

      }

      //Print number of hits per ROC
      std::stringstream ss_rh;
      ss_rh << "Hits in DTC " << dtcID << ": ";
      for (auto rh : roc_hits) ss_rh << rh << " ";
      TLOG(TLVL_DEBUG + 6) << ss_rh.str() << std::endl;

    } // loop over ROCs

  } // loop over subevents (DTCs)


  if (failedEvent && verbosity_ > 1){
    std::cout<<"===================================================\n";
    std::cout<<"========== THIS EVENT HAS SOME FAILURES ===========\n";
    std::cout<<"===================================================\n";
    std::cout<<"\n----- Dumping previous event -----\n\n";
    printEvent(*previousDTCEvent);
    std::cout<<"\n----- Dumping current event -----\n\n";
    printEvent(dtcevent);
  }
  previousDTCEvent = &dtcevent;
}

bool mu2e::CaloDataVerifier::checkAndUpdateDTCEWT(const DTCLib::DTC_SubEvent& subevent){
  uint64_t dtcID = subevent.GetDTCID();
  long int dtcEWT = subevent.GetEventWindowTag().GetEventWindowTag(true);
  if (lastDTCEventWindow.find(dtcID) != lastDTCEventWindow.end()){
    if (verbosity_ > 1){
      std::cout << "Checking the event window (DTC: " << dtcID << ")\n"
                << "current: " << dtcEWT
                << " previous: " << lastDTCEventWindow[dtcID] << "\n";
    }
    if (dtcEWT != lastDTCEventWindow[dtcID] + 1) {
      TLOG(TLVL_ERROR) << "Error in the event window (DTC " << dtcID << ")!\n"
                       << "current: " << dtcEWT
                       << " previous: " << lastDTCEventWindow[dtcID]
                       << "\nCurrent DTC HEADER: " << subevent.GetHeader()->toJson();
      lastDTCEventWindow[dtcID] = dtcEWT;
      return false;
    }
  }
  lastDTCEventWindow[dtcID] = dtcEWT;
  return true;
}

bool mu2e::CaloDataVerifier::checkAndUpdateROCEWT(const DTCLib::DTC_DataBlock& dataBlock){
  DTCLib::DTC_DataHeaderPacket* rocHeader = dataBlock.GetHeader().get();
  uint64_t dtcID = rocHeader->GetID();
  uint64_t rocID = rocHeader->GetLinkID();
  long int rocEWT = rocHeader->GetEventWindowTag().GetEventWindowTag(true);
  if (lastROCEventWindow.find(dtcID) != lastROCEventWindow.end() && lastROCEventWindow[dtcID].find(rocID) != lastROCEventWindow[dtcID].end()){
    if (verbosity_ > 1){
      std::cout << "Checking the event window (DTC: " << dtcID << ", ROC: " << rocID << ")\n"
                << "current: " << rocEWT
                << " previous: " << lastROCEventWindow[dtcID][rocID] << "\n";
    }
    if (rocEWT != lastROCEventWindow[dtcID][rocID] + 1) {
      TLOG(TLVL_ERROR) << "Error in the event window (DTC: " << dtcID << ", ROC: " << rocID << ")!\n"
                       << "current: " << rocEWT
                       << " previous: " << lastROCEventWindow[dtcID][rocID]
                       << "\nCurrent ROC HEADER: " << rocHeader->toJSON();
      lastROCEventWindow[dtcID][rocID] = rocEWT;
      return false;
    }
  }
  lastROCEventWindow[dtcID][rocID] = rocEWT;
  return true;
}

bool mu2e::CaloDataVerifier::checkAndUpdateROCCounter(const DTCLib::DTC_DataBlock& dataBlock, const std::vector<uint32_t>& hit_counters){
  DTCLib::DTC_DataHeaderPacket* rocHeader = dataBlock.GetHeader().get();
  uint64_t dtcID = rocHeader->GetID();
  uint64_t rocID = rocHeader->GetLinkID();
  uint32_t thisFirstCounter = hit_counters.front();
  uint32_t thisLastCounter = hit_counters.back();
  if (lastROCCounter.find(dtcID) != lastROCCounter.end() && lastROCCounter[dtcID].find(rocID) != lastROCCounter[dtcID].end()){
    if (verbosity_ > 1){
      std::cout << "Checking the ROC counter sequence (DTC: " << dtcID << ", ROC: " << rocID << ")\n"
                << "current: " << thisFirstCounter
                << " previous: " << lastROCCounter[dtcID][rocID] << "\n";
    }
    if (thisFirstCounter != lastROCCounter[dtcID][rocID] + 1) {
      TLOG(TLVL_ERROR) << "Error in the ROC counter sequence (DTC: " << dtcID << ", ROC: " << rocID << ")!\n"
                       << "current: " << thisFirstCounter
                       << " previous: " << lastROCCounter[dtcID][rocID] << "\n";
      lastROCCounter[dtcID][rocID] = thisLastCounter;
      return false;
    }
  }
  lastROCCounter[dtcID][rocID] = thisLastCounter;
  return true;
}

bool mu2e::CaloDataVerifier::checkHitBeginMarker(const mu2e::CalorimeterDataDecoder::CalorimeterHitTestDataPacket& hit){
  return (hit.BeginMarker == 0xAAA);
}

bool mu2e::CaloDataVerifier::checkHitLastMarker(const mu2e::CalorimeterDataDecoder::CalorimeterHitTestDataPacket& hit){
  return (hit.LastSampleMarker != 0);
}

bool mu2e::CaloDataVerifier::checkCounters(const DTCLib::DTC_DataBlock& dataBlock, const std::vector<uint32_t>& hit_counters){
  DTCLib::DTC_DataHeaderPacket* rocHeader = dataBlock.GetHeader().get();
  uint64_t dtcID = rocHeader->GetID();
  uint64_t rocID = rocHeader->GetLinkID();
  for (uint i=0; i<hit_counters.size()-1; i++){
    if (hit_counters[i+1] != hit_counters[i] + 1){
      TLOG(TLVL_ERROR) << "Error in the counter sequence (DTC: " << dtcID << ", ROC: " << rocID << ")!\n"
                       << "counter " << i <<" : " << hit_counters[i] << "\n"
                       << "counter " << i+1 <<" : " << hit_counters[i+1] << "\n";
      return false;
    }
  }
  return true;
}

void mu2e::CaloDataVerifier::printEvent(const DTCLib::DTC_Event& dtcevent){
  printEventHeader(dtcevent);
  std::vector<DTCLib::DTC_SubEvent> subevents = dtcevent.GetSubEvents();
  for (uint idtc = 0; idtc < subevents.size(); idtc++){
    std::cout << "-- DTC " << idtc << " --\n";
    printSubEvent(subevents[idtc]);
  }
}

void mu2e::CaloDataVerifier::printSubEvent(const DTCLib::DTC_SubEvent& subevent){
  printDTCHeader(subevent);
  std::vector<DTCLib::DTC_DataBlock> dataBlocks = subevent.GetDataBlocks();
  for (uint iroc = 0; iroc < dataBlocks.size(); iroc++){
    std::cout << "-- ROC " << iroc << " --\n";
    printROC(dataBlocks[iroc]);
  }
}

void mu2e::CaloDataVerifier::printEventHeader(const DTCLib::DTC_Event& dtcevent){
  auto headerPtr = reinterpret_cast<uint16_t const *>(dtcevent.GetRawBufferPointer());
  std::cout << "Event HEX DUMP (24 bytes) ------------------\n";
  for (size_t word=0; word<12; word++){
    std::cout << std::hex << std::setw(4) << std::setfill('0') << headerPtr[word] << std::dec << " ";
  }
  std::cout << std::endl;
  return;
}

void mu2e::CaloDataVerifier::printDTCHeader(const DTCLib::DTC_SubEvent& subevent){
  auto headerPtr = reinterpret_cast<uint16_t const *>(subevent.GetRawBufferPointer());
  std::cout << "DTC HEX DUMP (48 bytes) ------------------\n";
  for (size_t word=0; word<24; word++){
    std::cout << std::hex << std::setw(4) << std::setfill('0') << headerPtr[word] << std::dec << " ";
    if (word%8==7) std::cout << std::endl;
  }
  return;
}

void mu2e::CaloDataVerifier::printROC(const DTCLib::DTC_DataBlock& dataBlock){
  auto headerPtr = reinterpret_cast<uint16_t const *>(dataBlock.GetRawBufferPointer());
  auto dataPtr = reinterpret_cast<uint16_t const *>(dataBlock.GetData());
  std::cout << "ROC HEX DUMP (" << dataBlock.byteSize << " bytes) ------------------\n";
  for (size_t word=0; word<8; word++){
    std::cout << std::hex << std::setw(4) << std::setfill('0') << headerPtr[word] << std::dec << " ";
  }
  std::cout << std::endl;
  for (size_t word=0; word<(dataBlock.byteSize-16)/2; word++){
    std::cout << std::hex << std::setw(4) << std::setfill('0') << dataPtr[word] << std::dec << " ";
    if (word%16==15) std::cout << std::endl;
  }
  return;
}

void mu2e::CaloDataVerifier::printRunSummary(){
  std::cout << "Run summary:" << std::endl
    << "\t total calo events: " << nCaloEvents << std::endl
    << "\t total calo hits: " << nCaloHits << std::endl;

  std::cout << "Total hits per channel:" << std::endl;
  for (auto board : totalHitMap){
    std::cout << "\tBoard " << board.first << std::endl;
    for (auto channel : board.second){
      std::cout << "\t\tChannel " << channel.first << " , hits: " << channel.second << std::endl;
    }
  }

  std::cout << "Failures:" << std::endl
    << "\t total failed DTC EWT checks: " << total_failed_DTCEWTs << std::endl
    << "\t total failed ROC EWT checks: " << total_failed_ROCEWTs << std::endl
    << "\t total failed ROC counters checks: " << total_failed_ROCCounters << std::endl
    << "\t total failed counters checks: " << total_failed_Counters << std::endl
    << "\t total failed 0xAAA checks: " << total_failed_BeginMarker << std::endl
    << "\t total failed 0xFFF checks: " << total_failed_LastMarker << std::endl;
  
  std::cout << "Failed boundary counters per ROC:" << std::endl;
  for (auto dtc : failMap_ROCCounters){
    std::cout << "\tDTC " << dtc.first << std::endl;
    for (auto roc : dtc.second){
      std::cout << "\t\tROC " << roc.first << " , failures: " << roc.second << std::endl;
    }
  }

  std::cout << "Failed internal counters per ROC:" << std::endl;
  for (auto dtc : failMap_Counters){
    std::cout << "\tDTC " << dtc.first << std::endl;
    for (auto roc : dtc.second){
      std::cout << "\t\tROC " << roc.first << " , failures: " << roc.second << std::endl;
    }
  }

  std::cout << "Failed 0xAAA words per ROC:" << std::endl;
  for (auto dtc : failMap_BeginMarker){
    std::cout << "\tDTC " << dtc.first << std::endl;
    for (auto roc : dtc.second){
      std::cout << "\t\tROC " << roc.first << " , failures: " << roc.second << std::endl;
    }
  }

  std::cout << "Failed 0xFFF words per ROC:" << std::endl;
  for (auto dtc : failMap_LastMarker){
    std::cout << "\tDTC " << dtc.first << std::endl;
    for (auto roc : dtc.second){
      std::cout << "\t\tROC " << roc.first << " , failures: " << roc.second << std::endl;
    }
  }
}


DEFINE_ART_MODULE(mu2e::CaloDataVerifier)
