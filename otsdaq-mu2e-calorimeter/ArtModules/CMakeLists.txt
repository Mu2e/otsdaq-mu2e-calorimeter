cet_build_plugin(testFragments art::module LIBRARIES REG
 artdaq-core-mu2e::Overlays art_root_io::TFileService_service ROOT::Hist
 )

cet_build_plugin(CaloDataVerifier art::module LIBRARIES REG
 artdaq-core-mu2e::Data
 artdaq-core-mu2e::Data_dict
 artdaq-core-mu2e::Overlays
 artdaq::DAQdata
)

cet_build_plugin(CaloDataAnalyzer art::module LIBRARIES REG
 artdaq-core-mu2e::Data
 artdaq-core-mu2e::Data_dict
 artdaq-core-mu2e::Overlays
 artdaq::DAQdata
 art_root_io::TFileService_service
 ROOT::Hist
 ROOT::Tree
)

install_headers()
install_source()
