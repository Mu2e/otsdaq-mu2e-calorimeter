include_directories($ENV{KINKAL_INC})
include_directories($ENV{BTRK_INC})
link_directories($ENV{KINKAL_LIB})
link_directories($ENV{BTRK_LIB})

cet_build_plugin(testFragments art::module LIBRARIES REG
 artdaq_core_mu2e::artdaq-core-mu2e_Overlays art_root_io::TFileService_service ROOT::Hist
 )

cet_build_plugin(CaloDataVerifier art::module LIBRARIES REG
 artdaq_core_mu2e::artdaq-core-mu2e_Data
 artdaq_core_mu2e::artdaq-core-mu2e_Data_dict
 artdaq_core_mu2e::artdaq-core-mu2e_Overlays
 artdaq::DAQdata
)

cet_build_plugin(CaloDataAnalyzer art::module
    REG_SOURCE CaloDataAnalyzer_module.cc
    LIBRARIES REG
     artdaq_core_mu2e::artdaq-core-mu2e_Data
     artdaq_core_mu2e::artdaq-core-mu2e_Data_dict
     artdaq_core_mu2e::artdaq-core-mu2e_Overlays
     artdaq::DAQdata
     art_root_io::TFileService_service
     Offline::DAQ
     ROOT::Hist
     ROOT::Tree
)

cet_build_plugin(CaloiercAnalyzer art::module
    REG_SOURCE CaloiercAnalyzer_module.cc
    LIBRARIES REG
      art_root_io::TFileService_service
      Offline::RecoDataProducts
      Offline::DAQ
      Offline::Mu2eKinKal
      ROOT::Hist
      ROOT::Tree
      ROOT::Core
      ROOT::RIO
      ROOT::Gui
)

cet_build_plugin(CaloEventFilter art::module
    REG_SOURCE CaloEventFilter_module.cc
    LIBRARIES REG
      artdaq_core_mu2e::artdaq-core-mu2e_Data
      artdaq_core_mu2e::artdaq-core-mu2e_Data_dict
      artdaq_core_mu2e::artdaq-core-mu2e_Overlays
      artdaq::DAQdata
)



install_headers()
install_source()
