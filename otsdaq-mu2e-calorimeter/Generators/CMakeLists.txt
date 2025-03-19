
include(artdaq::commandableGenerator)

cet_build_plugin(Mu2eCaloEventReceiver   artdaq::commandableGenerator LIBRARIES 
			       REG artdaq-mu2e::Mu2eReceiverBase artdaq-core-mu2e::Overlays canvas::canvas mu2e-pcie-utils::DTCInterface)


cet_build_plugin(CalorimeterVST   artdaq::commandableGenerator LIBRARIES 
			       REG artdaq-mu2e::Mu2eReceiverBase artdaq-core-mu2e::Overlays canvas::canvas mu2e-pcie-utils::DTCInterface)
# cet_build_plugin(CalorimeterVST    artdaq::commandableGenerator LIBRARIES 
# 			       REG artdaq-core-mu2e::Overlays canvas::canvas mu2e-pcie-utils::DTCInterface)
