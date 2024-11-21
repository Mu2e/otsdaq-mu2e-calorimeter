cet_build_plugin(testFragments art::module LIBRARIES REG
 artdaq_core_mu2e::artdaq-core-mu2e_Overlays art_root_io::TFileService_service ROOT::Hist
 )

cet_build_plugin(CaloDataVerifier art::module LIBRARIES REG
 artdaq_core_mu2e::artdaq-core-mu2e_Data
 artdaq_core_mu2e::artdaq-core-mu2e_Data_dict
 artdaq_core_mu2e::artdaq-core-mu2e_Overlays
 artdaq::DAQdata
)

install_headers()
install_source()
