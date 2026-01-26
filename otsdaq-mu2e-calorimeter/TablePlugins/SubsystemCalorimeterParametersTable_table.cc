#include "otsdaq-mu2e-calorimeter/TablePlugins/SubsystemCalorimeterParametersTable.h"
#include "otsdaq/Macros/TablePluginMacros.h"  //for DEFINE_OTS_TABLE

#include "otsdaq/TablePlugins/XDAQContextTable/XDAQContextTable.h"

#include <sys/stat.h>  //for mkdir
#include <fstream>
#include <iostream>

using namespace ots;

const std::string SubsystemCalorimeterParametersTable::PATH_TO_TRIGGER_OFFLINE_DB = getenv("PATH_TO_TRIGGER_OFFLINE_DB") ? getenv("PATH_TO_TRIGGER_OFFLINE_DB") : "";
const std::string SubsystemCalorimeterParametersTable::CHANNEL_MAP_TABLE          = "SubsystemCalorimeterMapTable";
const std::string SubsystemCalorimeterParametersTable::CHANNEL_STATUS_TABLE       = "SubsystemCalorimeterStatusTable";

//==============================================================================
SubsystemCalorimeterParametersTable::SubsystemCalorimeterParametersTable(void) : TableBase("SubsystemCalorimeterParametersTable") {}

//==============================================================================
SubsystemCalorimeterParametersTable::~SubsystemCalorimeterParametersTable(void) {}

//==============================================================================
/// init
/// generate calo specific files needed by the online trigger and save them in the filesystem 'offline' db
void SubsystemCalorimeterParametersTable::init(ConfigurationManager* configManager) {
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

	generateOfflineTableMap(configManager);

	for(const auto& offlineTable : mapOfflineTables_) {
		std::string offlineTableFileName = PATH_TO_TRIGGER_OFFLINE_DB + "/" + offlineTable.first + ".txt";

		try {
			std::ofstream out(offlineTableFileName);
			if(!out) {
				__SS__ << "Failed to open file: " << offlineTableFileName << __E__;
				__SS_THROW__;
			}
			out << offlineTable.second;
			out.close();
		} catch(const std::exception& e) {
			__SS__ << "Failed to write offline table " << offlineTable.first << " to file: " << e.what() << __E__;
			__SS_THROW__;
		}
	}

}  // end init()

//==============================================================================
void SubsystemCalorimeterParametersTable::generateOfflineTableMap(const ConfigurationManager* configManager) {
	mapOfflineTables_.clear();
	mapOfflineTables_["CalChannelMap"] = getChannelMapAndCSVFormat(configManager, "CalChannelMap");
	__COUTT__ << mapOfflineTables_["CalChannelMap"] << __E__;
	mapOfflineTables_["CalChannelStatus"] = getStatusTableInCSVFormat(configManager, "CalChannelStatus");
	__COUTT__ << mapOfflineTables_["CalChannelStatus"] << __E__;
}  // end generateOfflineTableMap()

//==============================================================================
std::string SubsystemCalorimeterParametersTable::getChannelMapAndCSVFormat(const ConfigurationManager* configManager, const std::string& OfflineCxxClassName) {
	mapChannels_.clear();

	std::stringstream OfflineTable;
	OfflineTable << "TABLE " << OfflineCxxClassName << __E__;
	std::vector<std::pair<std::string, ConfigurationTree>> channelMapRecords = configManager->getNode(SubsystemCalorimeterParametersTable::CHANNEL_MAP_TABLE).getChildren();

	// start main fe/DTC record loop
	for(auto& channelMapPair : channelMapRecords) {
		uint16_t onlineID      = channelMapPair.second.getNode(ColChannelMap.onlineId_).getValue<uint16_t>();
		uint16_t offlineID     = channelMapPair.second.getNode(ColChannelMap.offlineId_).getValue<uint16_t>();
		mapChannels_[onlineID] = offlineID;

		OfflineTable << onlineID << "," << offlineID << "\n";
	}
	return OfflineTable.str();
}  // end getChannelMapAndCSVFormat()

//==============================================================================
std::string SubsystemCalorimeterParametersTable::getStatusTableInCSVFormat(const ConfigurationManager* configManager, const std::string& OfflineCxxClassName) {
	std::stringstream OfflineTable;
	OfflineTable << "TABLE " << OfflineCxxClassName << __E__;
	std::vector<std::pair<std::string, ConfigurationTree>> channelStatusRecords = configManager->getNode(SubsystemCalorimeterParametersTable::CHANNEL_STATUS_TABLE).getChildren();

	// start main fe/DTC record loop
	for(auto& channelStatusPair : channelStatusRecords) {
		uint16_t                               boardID = channelStatusPair.second.getNode(ColChannelStatus.colBoardId_).getValue<uint16_t>();
		ConfigurationTree::BitMap<std::string> bitmap  = channelStatusPair.second.getNode(ColChannelStatus.colStatus_).getValueAsBitMap();

		// assume data is 1-dimensional
		for(uint32_t j = 0; j < bitmap.numberOfColumns(0); j++) {
			OfflineTable << mapChannels_.at(boardID * CHANNELS_PER_BOARD + j) << ", ";
			OfflineTable << ((bitmap.get(0, j).size() == 0) ? "0" : bitmap.get(0, j));
			OfflineTable << ((j + 1 == bitmap.numberOfColumns(0)) ? "" : "\n");
		}
		OfflineTable << "\n";
	}
	return OfflineTable.str();
}  // end getStatusTableInCSVFormat()

//==============================================================================
// return status structures
std::string SubsystemCalorimeterParametersTable::getStructureAsJSON(const ConfigurationManager* cfgMgr) {
	// Don't generate maps if done already in init()
	if(mapOfflineTables_.size() == 0)
		generateOfflineTableMap(cfgMgr);

	std::vector<std::pair<std::string, ConfigurationTree>> channelStatusRecords = cfgMgr->getNode(SubsystemCalorimeterParametersTable::CHANNEL_STATUS_TABLE).getChildren();

	std::stringstream outstream;

	outstream << "{";

	// Write all cat-3 tables converted from MongoDB
	outstream << "\t\"cat3\": {" << __E__;
	std::map<std::string, std::string>::iterator it;
	for(it = mapOfflineTables_.begin(); it != mapOfflineTables_.end(); ++it) {
		outstream << "\"" << it->first << "\":" << it->second;
		outstream << (std::next(it) == mapOfflineTables_.end() ? "" : ",");
	}
	outstream << "},";

	// Write any desired table in a custom format for user-friendly quick reading
	outstream << "\t\"custom\": ";
	outstream << "{" << __E__;
	outstream << "\t\"Number of rows\": " << channelStatusRecords.size() << "," << __E__;
	outstream << "\t\"Rows\": [" << __E__;

	uint16_t statusPairIdx = 0;
	for(auto& channelStatusPair : channelStatusRecords) {
		uint16_t                               boardID = channelStatusPair.second.getNode(ColChannelStatus.colBoardId_).getValue<uint16_t>();
		ConfigurationTree::BitMap<std::string> bitmap  = channelStatusPair.second.getNode(ColChannelStatus.colStatus_).getValueAsBitMap();
		statusPairIdx++;

		outstream << "\t\t{" << __E__;
		outstream << "\t\t\"BoardID\": " << boardID << "," << __E__;
		outstream << "\t\t\"Rows\": " << bitmap.numberOfRows() << "," << __E__;
		outstream << "\t\t\"BitMap\": [";

		// assume data is 1-dimensional
		for(uint32_t j = 0; j < bitmap.numberOfColumns(0); j++) {
			outstream << ((bitmap.get(0, j).size() == 0) ? "0" : bitmap.get(0, j));
			outstream << ((j + 1 == bitmap.numberOfColumns(0)) ? "" : ", ");
		}
		outstream << "]" << __E__;
		outstream << "\t\t}" << (statusPairIdx == channelStatusRecords.size() ? "" : ",") << __E__;
	}

	outstream << "\t]" << __E__;
	outstream << "}";  // close custom blob
	outstream << "}";  // close full blob
	return outstream.str();
}  // end getStructureAsJSON()

DEFINE_OTS_TABLE(SubsystemCalorimeterParametersTable)
