#include "otsdaq-mu2e-calorimeter/TablePlugins/SubsystemCalorimeterParametersTable.h"
#include "otsdaq/Macros/TablePluginMacros.h"  //for DEFINE_OTS_TABLE

#include "otsdaq/TablePlugins/XDAQContextTable/XDAQContextTable.h"

#include <sys/stat.h>  //for mkdir
#include <iostream>

using namespace ots;


const std::string SubsystemCalorimeterParametersTable::PATH_TO_TRIGGER_OFFLINE_DB = \
	getenv("PATH_TO_TRIGGER_OFFLINE_DB") ? \
	getenv("PATH_TO_TRIGGER_OFFLINE_DB") : \
	"";
const std::string SubsystemCalorimeterParametersTable::CHANNEL_STATUS_TABLE = "SubsystemCalorimeterStatusTable";

//==============================================================================
SubsystemCalorimeterParametersTable::SubsystemCalorimeterParametersTable(void)
    : TableBase("SubsystemCalorimeterParametersTable")
{
}

//==============================================================================
SubsystemCalorimeterParametersTable::~SubsystemCalorimeterParametersTable(void) {}

//==============================================================================
/// init 
/// generate calo specific files needed by the online trigger and save them in the filesystem 'offline' db
void SubsystemCalorimeterParametersTable::init(ConfigurationManager* configManager)
{
	// use isFirstAppInContext to only run once per context, for example to avoid
	//	generating files on local disk multiple times.
	bool isFirstAppInContext_ = configManager->isOwnerFirstAppInContext();

	__COUTV__(isFirstAppInContext_);
	if(!isFirstAppInContext_)
		return;

	__COUTV__(SubsystemCalorimeterParametersTable::PATH_TO_TRIGGER_OFFLINE_DB);
	if(SubsystemCalorimeterParametersTable::PATH_TO_TRIGGER_OFFLINE_DB.size() == 0)
		return;

	__COUT__ << "*&*&*&*&*&*&*&*&*&*&*&*&*&*&*&*&*&*&*&*&*&*" << __E__;
	__COUT__ << configManager->__SELF_NODE__ << __E__;

	mapOfflineTables.clear();
	mapOfflineTables["cal.channelstatus"] = getStatusTableInCSVFormat(configManager, "CalChannelStatus");
	__COUT__ << "TESTING getStatusTableInCSVFormat" << __E__;
	__COUT__ << mapOfflineTables["cal.channelstatus"] << __E__;
}  // end init()

//==============================================================================
std::string SubsystemCalorimeterParametersTable::getStatusTableInCSVFormat(
													ConfigurationManager* configManager, 
													const std::string& OfflineCxxClassName){

	std::stringstream OfflineTable;
	OfflineTable << "TABLE " << OfflineCxxClassName << __E__;
	std::vector<std::pair<std::string, ConfigurationTree>> channelStatusRecords =
	    configManager->getNode(SubsystemCalorimeterParametersTable::CHANNEL_STATUS_TABLE).getChildren();

	for(auto& channelStatusPair : channelStatusRecords)  // start main fe/DTC record loop
	{
		uint16_t boardID = channelStatusPair.second.getNode(ColChannelStatus.colBoardId_).getValue<uint16_t>();
		ConfigurationTree::BitMap<std::string> bitmap = channelStatusPair.second.getNode(ColChannelStatus.colStatus_).getValueAsBitMap();

		OfflineTable << boardID << ", ";
		// assume data is 1-dimensional 
		for(uint32_t j=0; j<bitmap.numberOfColumns(0); j++)
		{
			OfflineTable << ((bitmap.get(0,j).size()==0) ? "0" : bitmap.get(0,j));
			OfflineTable << ((j+1==bitmap.numberOfColumns(0)) ? "" : ", ");
		}
	}

	return OfflineTable.str();
} // end getStatusTableInCSVFormat()

//==============================================================================
// return status structures
std::string SubsystemCalorimeterParametersTable::getStructureStatusAsJSON(
	const ConfigurationManager* cfgMgr) const
{
	std::vector<std::pair<std::string, ConfigurationTree>> channelStatusRecords =
	    cfgMgr->getNode(SubsystemCalorimeterParametersTable::CHANNEL_STATUS_TABLE).getChildren();

	std::stringstream outstream;
	outstream << "{" << __E__;
	outstream << "\t\"childern length\": " << channelStatusRecords.size() << "," <<__E__;
	outstream << "\t[" << __E__;

	uint16_t statusPairIdx = 0;
	for(auto& channelStatusPair : channelStatusRecords)
	{
		uint16_t boardID = channelStatusPair.second.getNode(ColChannelStatus.colBoardId_).getValue<uint16_t>();
		ConfigurationTree::BitMap<std::string> bitmap = channelStatusPair.second.getNode(ColChannelStatus.colStatus_).getValueAsBitMap();
		statusPairIdx++;

		outstream << "\t\t{" << __E__;
		outstream << "\t\t\"BoardID\": " << boardID << "," <<__E__;
		outstream << "\t\t\"Rows\": " << bitmap.numberOfRows() << "," <<__E__;
		outstream << "\t\t\"BitMap\": [";

		// assume data is 1-dimensional 
		for(uint32_t j=0; j<bitmap.numberOfColumns(0); j++)
		{
			outstream << ((bitmap.get(0,j).size()==0) ? "0" : bitmap.get(0,j));
			outstream << ((j+1==bitmap.numberOfColumns(0)) ? "" : ", ");
		}
		outstream << "]" << __E__;
		outstream << "\t\t}" << (statusPairIdx==channelStatusRecords.size() ? "" : ",") << __E__;
	}

	outstream << "\t]" << __E__;
	outstream << "}";
	return outstream.str();
}  // end getStructureStatusAsJSON()

DEFINE_OTS_TABLE(SubsystemCalorimeterParametersTable)
