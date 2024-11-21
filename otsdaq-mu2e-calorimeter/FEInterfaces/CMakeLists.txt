
include(otsdaq::FEInterface)

cet_make_library(
  LIBRARY_NAME FEInterfaces
  SOURCE MZB.C
  LIBRARIES PUBLIC
#  otsdaq::NetworkUtilities
#  ROOT::Core
  otsdaq::Macros
#  ROOT::RIO
#  ROOT::Gui
)

cet_build_plugin(ROCCalorimeterInterface
  otsdaq::FEInterface
  LIBRARIES REG
  otsdaq_mu2e::ROCPolarFireCoreInterface
  otsdaq_mu2e_calorimeter::FEInterfaces
  otsdaq::Macros
  )
 
 
install_headers()
install_source()
