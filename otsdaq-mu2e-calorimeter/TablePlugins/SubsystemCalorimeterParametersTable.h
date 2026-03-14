#ifndef _ots_SubsystemCalorimeterParametersTable_h_
#define _ots_SubsystemCalorimeterParametersTable_h_

#include "otsdaq/ConfigurationInterface/ConfigurationManager.h"
#include "otsdaq/TableCore/TableBase.h"

namespace ots
{
class SubsystemCalorimeterParametersTable : public TableBase
{
	// clang-format off

  public:
	SubsystemCalorimeterParametersTable						(void);
	virtual ~SubsystemCalorimeterParametersTable				(void);

	// Methods
	void 					init						(ConfigurationManager* configManager) override;

	virtual std::string		getStructureAsJSON			(const ConfigurationManager* configManager) override;

	virtual std::string		getChannelMapAndCSVFormat	(const ConfigurationManager* configManager, const std::string& OfflineCxxClassName);

	virtual std::string		getStatusTableInCSVFormat	(const ConfigurationManager* configManager, const std::string& OfflineCxxClassName);

	void 					generateOfflineTableMap		(const ConfigurationManager* configManager);


  private:

	const static std::string DBSERVICE_ONLINE_PATH;
	const static std::string CHANNEL_STATUS_TABLE;
	const static std::string CHANNEL_MAP_TABLE;

	std::map<std::string, std::string> mapOfflineTables_;
	std::map<uint16_t, uint16_t> mapChannels_;

	// Column names
	struct ColParameters // Calorimeter subsystem top level
	{
		// Incomplete list
		std::string const colLinkToFETypeTable					= "LinkToChannelStatusTableInfo";
		std::string const colLinkToSlowControlsChannelTable_ 	= "LinkToChannelThresholdTableInfo";
	} ColParameters;

	struct ColChannelMap // LinkToChannelMapTableInfo
	{
		std::string const offlineId_ = "offID";
		std::string const onlineId_  = "FEEchan";
	} ColChannelMap;

	struct ColChannelStatus // LinkToChannelStatusTableInfo
	{
		std::string const colBoardId_ 		= "BoardID";
		std::string const colStatus_		= "Status";
	} ColChannelStatus;

	struct ColChannelThreshold // LinkToChannelThresholdTableInfo
	{
		std::string const colBoardId_ 				= "BoardID";
		std::string const colROCGroupID_			= "Thresholds";
		std::string const colROCInterfacePluginName_= "Baseline";
		std::string const colLinkToSlowControlsChannelTable_ 	= "LinkToSlowControlsChannelTable";
	} ColChannelThreshold;

	// clang-format on
};
}  // namespace ots
#endif
