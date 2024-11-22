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
      fhicl::Atom<int> data_type {fhicl::Name("dataType" ) , fhicl::Comment("Data type (0:standard, 1:debug, 2:counters)"), 0};
      fhicl::Atom<int> metrics_level {fhicl::Name("metricsLevel" ) , fhicl::Comment("Metrics reporting level"), 1};
      fhicl::Atom<bool> produce_calo_decoders {fhicl::Name("produceCaloDecoders" ) , fhicl::Comment("Produce calo decoders [default: true]"), true};
      fhicl::Atom<std::string> subsystem_override {fhicl::Name("subsystemOverride" ) , fhicl::Comment("Override calo subsystem [\"calo\", \"tracker\"]"), "calo"};
    };

    explicit CaloDataVerifier(const art::EDFilter::Table<Config>& config);

    bool filter(art::Event & e) override;
    virtual bool endRun(art::Run& run ) override;

    long int getEventWindow(ushort ewt[3]){
      long int eventWindow = static_cast<long int>(ewt[0]) +
        (static_cast<long int>(ewt[1]) << 16) +
        (static_cast<long int>(ewt[2]) << 32);
      return eventWindow;
    }
    
    artdaq::Fragments getFragments(art::Event& event);
    void processCaloData(mu2e::DTCEventFragment& eventFragment, std::unique_ptr<std::vector<mu2e::CalorimeterDataDecoder>> const& caloDecoderColl);

    bool checkAndUpdateDTCEWT(const DTCLib::DTC_SubEvent& subevent);
    bool checkAndUpdateROCEWT(const DTCLib::DTC_DataBlock& dataBlock);


  private:
    std::set<int> dtcs_;
    int           data_type_;
    int           metrics_reporting_level_;
    bool          produceCaloDecoders_;
    DTCLib::DTC_Subsystem subsystem_;
    bool          isFirstEvent_;

    std::map<int, int> lastDTCEventWindow;
    std::map<int, std::map<int, int>> lastROCEventWindow;
    
    size_t nCaloEvents;
    size_t nCaloHits;
  };
}  // namespace mu2e

mu2e::CaloDataVerifier::CaloDataVerifier(const art::EDFilter::Table<Config>& config)
  : art::EDFilter{config}, 
    data_type_(config().data_type()),
    metrics_reporting_level_(config().metrics_level()),
    produceCaloDecoders_(config().produce_calo_decoders()),
    isFirstEvent_(true)    
{
  if (config().subsystem_override() == "calo"){
    subsystem_ = DTCLib::DTC_Subsystem::DTC_Subsystem_Calorimeter;
  } else if (config().subsystem_override() == "tracker"){
    subsystem_ = DTCLib::DTC_Subsystem::DTC_Subsystem_Tracker;
  }

  if (produceCaloDecoders_) {
    produces<std::vector<mu2e::CalorimeterDataDecoder>>();
  }

  TLOG(TLVL_DEBUG) << "Reading data type " << data_type_;
}

bool mu2e::CaloDataVerifier::filter(art::Event& event){

  art::EventNumber_t eventNumber = event.event();
  TLOG(TLVL_INFO) << "mu2e::CaloDataVerifier::filter eventNumber= " << (int)eventNumber << std::endl;

  //Prepare vector of output data decoders
  std::unique_ptr<std::vector<mu2e::CalorimeterDataDecoder>> caloDecoderColl(new std::vector<mu2e::CalorimeterDataDecoder>);

  nCaloEvents = 0;
  nCaloHits = 0;

  artdaq::Fragments fragments = getFragments(event);
  TLOG(TLVL_DEBUG) << "Iterating through " << fragments.size() << " fragments\n";
  for (const auto& frag : fragments) {
    mu2e::DTCEventFragment eventFragment(frag);
    processCaloData(eventFragment, caloDecoderColl);
  }
  
  TLOG(TLVL_DEBUG) << "[CaloDataVerifier::filter] found " << nCaloEvents << " calo subevents in event" << (int)eventNumber;
  TLOG(TLVL_DEBUG) << "[CaloDataVerifier::filter] found " << nCaloHits << " calo hits in event" << (int)eventNumber;
  
  if (nCaloEvents == 0) {
    TLOG(TLVL_WARNING) << "[CaloDataVerifier::filter] found no calo subevents in event" << (int)eventNumber << "!";
  }
  if (produceCaloDecoders_) {
    event.put(std::move(caloDecoderColl));
  }

  TLOG(TLVL_INFO) << "mu2e::CaloDataVerifier::filter exiting eventNumber=" << (int)eventNumber;
  return true;
}


bool mu2e::CaloDataVerifier::endRun( art::Run&  ) {
  return true;
}




artdaq::Fragments mu2e::CaloDataVerifier::getFragments(art::Event& event){
  artdaq::Fragments fragments;
  artdaq::FragmentPtrs containerFragments;

  std::vector<art::Handle<artdaq::Fragments>> fragmentHandles;
  fragmentHandles = event.getMany<std::vector<artdaq::Fragment>>();

  TLOG(TLVL_DEBUG) << "Iterating through " << fragmentHandles.size() << " fragment handles\n";
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

  DTCLib::DTC_Event dtcevent = eventFragment.getData();
  //DTCLib::DTC_EventHeader* eventHeader = dtcevent.GetHeader();
  std::vector<DTCLib::DTC_SubEvent> subevents = dtcevent.GetSubEvents();
  TLOG(TLVL_DEBUG) << "Found " << subevents.size() << " total subevents\n";

  auto caloSubEvents = eventFragment.getSubsystemData(subsystem_);
  TLOG(TLVL_DEBUG) << "Iterating through " << caloSubEvents.size() << " calorimeter subevents\n";
  for (const auto& subevent : caloSubEvents) {

    checkAndUpdateDTCEWT(subevent);
    uint64_t dtcID = subevent.GetDTCID();

    mu2e::CalorimeterDataDecoder caloDecoder(subevent);
    if (produceCaloDecoders_) {
      caloDecoderColl->emplace_back(subevent); //FIXME: double construction of this calo decoder
    }
    nCaloEvents++;
    // Iterate over the data blocks (ROCs)
    std::vector<DTCLib::DTC_DataBlock> dataBlocks = subevent.GetDataBlocks();
    uint nROCs = dataBlocks.size();
    TLOG(TLVL_DEBUG) << "Iterating through " << nROCs << " data blocks (ROCs)\n";
    for (uint iroc = 0; iroc < nROCs; iroc++){

      checkAndUpdateROCEWT(dataBlocks[iroc]);

      if (data_type_ == 0){ // STANDARD HITS
      
        auto caloHits = caloDecoder.GetCalorimeterHitData(iroc);
        uint nHits = caloHits->size();
        TLOG(TLVL_INFO) << "There are " << nHits << " hits in DTC " << dtcID << " ROC " << iroc << " / " << nROCs << std::endl;
        for (uint ihit = 0; ihit<nHits; ihit++){
          mu2e::CalorimeterDataDecoder::CalorimeterHitDataPacket hit = caloHits->at(ihit).first;
          std::vector<uint16_t> hit_waveform = caloHits->at(ihit).second;
          if (hit_waveform.size() == 0){
            TLOG(TLVL_WARNING) << "[CaloDataVerifier::filter] found empty waveform! DTC " << dtcID << " ROC " << iroc << " hit " << ihit
              << " BoardID " << hit.BoardID << " ChannelNumber " << hit.ChannelNumber;
          }
          nCaloHits++;
        }
      
      } else if (data_type_ == 1){ // DEBUG HITS
        auto caloHits = caloDecoder.GetCalorimeterHitTestData(iroc);
        uint nHits = caloHits->size();
        TLOG(TLVL_INFO) << "There are " << nHits << " hits in DTC " << dtcID << " ROC " << iroc << " / " << nROCs << std::endl;
        for (uint ihit = 0; ihit<nHits; ihit++){
          mu2e::CalorimeterDataDecoder::CalorimeterHitTestDataPacket hit = caloHits->at(ihit).first;
          std::vector<uint16_t> hit_waveform = caloHits->at(ihit).second;
         if (hit_waveform.size() == 0){
            TLOG(TLVL_WARNING) << "[CaloDataVerifier::filter] found empty waveform! DTC " << dtcID << " ROC " << iroc << " hit " << ihit
              << " BoardID " << hit.BoardID << " ChannelID " << hit.ChannelID;
          }
         nCaloHits++;
    
          TLOG(TLVL_DEBUG)
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
          TLOG(TLVL_DEBUG) << "Waveform:\n" << ss_wf.str();
          
          if (metricMan != nullptr){
            TLOG(TLVL_DEBUG) << "[CaloDataVerifier::filter] sending hit metrics to Grafana..." << std::endl;
            metricMan->sendMetric("BoardID", hit.BoardID, "Board ID number",
              metrics_reporting_level_, artdaq::MetricMode::LastPoint);
            metricMan->sendMetric("ChannelID", hit.ChannelID, "Channel ID number",
              metrics_reporting_level_, artdaq::MetricMode::LastPoint);
            metricMan->sendMetric("NumberOfSamples", hit.NumberOfSamples, "Number of samples",
              metrics_reporting_level_, artdaq::MetricMode::LastPoint);
          }
        }
       if (metricMan != nullptr){
          metricMan->sendMetric("nHits", int(nHits), "Hits",
            metrics_reporting_level_, artdaq::MetricMode::LastPoint);
        }
      }
    }
    if (metricMan != nullptr){
      metricMan->sendMetric("nSubEvents", caloSubEvents.size(), "Subevents",
        metrics_reporting_level_, artdaq::MetricMode::LastPoint);
    }
  }
}

bool mu2e::CaloDataVerifier::checkAndUpdateDTCEWT(const DTCLib::DTC_SubEvent& subevent){
  uint64_t dtcID = subevent.GetDTCID();
  long int dtcEWT = subevent.GetEventWindowTag().GetEventWindowTag(true);
  TLOG(TLVL_DEBUG) << "DTC: " << dtcID << " - Current EWT: " << dtcEWT << " - Previous EWT: " << lastDTCEventWindow[dtcID];
  if (lastDTCEventWindow.find(dtcEWT) != lastDTCEventWindow.end() && dtcEWT != lastDTCEventWindow[dtcID] + 1) {
    TLOG(TLVL_ERROR) << "Error in the event window (DTC)!\n"
                     << "current: " << dtcEWT
                     << " previous: " << lastDTCEventWindow[dtcID]
                     << "\nCurrent DTC HEADER: " << subevent.GetHeader()->toJson();
    lastDTCEventWindow[dtcID] = dtcEWT;
    return false;
  }
  lastDTCEventWindow[dtcID] = dtcEWT;
  return true;
}

bool mu2e::CaloDataVerifier::checkAndUpdateROCEWT(const DTCLib::DTC_DataBlock& dataBlock){

  // print the data block header
  DTCLib::DTC_DataHeaderPacket* rocHeader = dataBlock.GetHeader().get();
  uint64_t dtcID = rocHeader->GetID();
  uint64_t rocID = rocHeader->GetLinkID();
  long int rocEWT = rocHeader->GetEventWindowTag().GetEventWindowTag(true);
  TLOG(TLVL_DEBUG) << "DTC: " << dtcID << " - ROC: " << rocID << " - Current EWT: " << rocEWT << " - Previous EWT: " << lastROCEventWindow[dtcID][rocID];
  if (lastROCEventWindow.find(rocEWT) != lastROCEventWindow.end() && rocEWT != lastROCEventWindow[dtcID][rocID] + 1) {
    TLOG(TLVL_ERROR) << "Error in the event window (ROC)!\n"
                     << "current: " << rocEWT
                     << " previous: " << lastROCEventWindow[dtcID][rocID]
                     << "\nCurrent ROC HEADER: " << rocHeader->toJSON();
    lastROCEventWindow[dtcID][rocID] = rocEWT;
    return false;
  }
  lastROCEventWindow[dtcID][rocID] = rocEWT;
  return true;
}

DEFINE_ART_MODULE(mu2e::CaloDataVerifier)
