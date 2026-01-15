#ifndef _ots_SubsystemCalorimeterParametersTable_h_
#define _ots_SubsystemCalorimeterParametersTable_h_

#include "otsdaq/ConfigurationInterface/ConfigurationManager.h"
#include "otsdaq/TableCore/TableBase.h"

namespace ots {
class SubsystemCalorimeterParametersTable : public TableBase {
	// clang-format off

  public:
	SubsystemCalorimeterParametersTable						(void);
	virtual ~SubsystemCalorimeterParametersTable				(void);

	// Methods
	void 			init					(ConfigurationManager* configManager) override;

	virtual std::string     getStructureAsJSON	(const ConfigurationManager* configManager) override;

	virtual std::string 	getStatusTableInCSVFormat   (const ConfigurationManager* configManager, const std::string& OfflineCxxClassName);

	void 					generateOfflineTableMap		(const ConfigurationManager* configManager);

	bool					isFirstAppInContext_ 	= false;

  private:

	const static std::string PATH_TO_TRIGGER_OFFLINE_DB;
	const static std::string CHANNEL_STATUS_TABLE;

	std::map<std::string, std::string> mapOfflineTables_;

	// Column names
	struct ColParameters // Calorimeter subsystem top level 
	{
		// Incomplete list
		std::string const colLinkToFETypeTable					= "LinkToChannelStatusTableInfo";
		std::string const colLinkToSlowControlsChannelTable_ 	= "LinkToChannelThresholdTableInfo";
	} ColParameters;

	struct ColChannelStatus // LinkToChannelStatusTableInfo
	{
		std::string const colBoardId_ 		= "BoardID";
		std::string const colStatus_		= "Status";
	} ColChannelStatus;

	struct ColChannelThreshold // LinkToChannelThresholdTableInfo
	{
		std::string const colStatus_ 				= "BoardID";
		std::string const colROCGroupID_			= "Thresholds";
		std::string const colROCInterfacePluginName_= "Baseline";
		std::string const colLinkToSlowControlsChannelTable_ 	= "LinkToSlowControlsChannelTable";
	} ColChannelThreshold;

	// clang-format on
};
}  // namespace ots
#endif
