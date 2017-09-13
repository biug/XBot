#include "Config.h"
#include "UABAssert.h"

namespace Config
{
    namespace ConfigFile
    {
        bool ConfigFileFound                = false;
        bool ConfigFileParsed               = false;
    }

    namespace Strategy
    {
        std::string ProtossStrategyName     = "1ZealotCore";			// default
        std::string TerranStrategyName      = "11Rax";					// default
        std::string ZergStrategyName        = "9PoolSpeed";				// default
        std::string StrategyName            = "9PoolSpeed";
        std::string ReadDir                 = "bwapi-data/read/";
        std::string WriteDir                = "bwapi-data/write/";
        bool ScoutHarassEnemy               = true;
		bool SurrenderWhenHopeIsLost        = true;
        bool UseEnemySpecificStrategy       = true;
        bool FoundEnemySpecificStrategy     = false;
    }

    namespace BotInfo
    {
        std::string BotName                 = "XBot";
        std::string Authors                 = "Xun";
        bool PrintInfoOnStart               = false;
    }

    namespace BWAPIOptions
    {
        int SetLocalSpeed                   = 42;
        int SetFrameSkip                    = 0;
        bool EnableUserInput                = true;
        bool EnableCompleteMapInformation   = false;
    }
    
    namespace Tournament						
    {
        int GameEndFrame                    = 86400;	
    }
    
    namespace Debug								
    {
        bool DrawGameInfo                   = true;
        bool DrawUnitHealthBars             = false;
        bool DrawProductionInfo             = true;
        bool DrawBuildOrderSearchInfo       = false;
        bool DrawScoutInfo                  = false;
        bool DrawResourceInfo               = false;
        bool DrawWorkerInfo                 = false;
        bool DrawModuleTimers               = false;
        bool DrawReservedBuildingTiles      = false;
        bool DrawCombatSimulationInfo       = false;
        bool DrawBuildingInfo               = false;
        bool DrawMouseCursorInfo            = false;
        bool DrawEnemyUnitInfo              = false;
        bool DrawBWTAInfo                   = false;
        bool DrawMapGrid                    = false;
		bool DrawMapDistances				= false;
		bool DrawBaseInfo					= false;
		bool DrawStrategyBossInfo			= false;
		bool DrawUnitTargetInfo				= false;
		bool DrawUnitOrders					= false;
        bool DrawSquadInfo                  = false;
        bool DrawBOSSStateInfo              = false;

        std::string ErrorLogFilename        = "XBot_ErrorLog.txt";
        bool LogAssertToErrorFile           = false;

        BWAPI::Color ColorLineTarget        = BWAPI::Colors::White;
        BWAPI::Color ColorLineMineral       = BWAPI::Colors::Cyan;
        BWAPI::Color ColorUnitNearEnemy     = BWAPI::Colors::Red;
        BWAPI::Color ColorUnitNotNearEnemy  = BWAPI::Colors::Green;
    }

    namespace Micro								
    {
        bool KiteWithRangedUnits            = true;
        std::set<BWAPI::UnitType> KiteLongerRangedUnits;
        bool WorkersDefendRush              = false; 
		int RetreatMeleeUnitShields         = 0;
        int RetreatMeleeUnitHP              = 0;
        int CombatRegroupRadius             = 300;      // radius of units around frontmost unit for combat sim
        int UnitNearEnemyRadius             = 600;      // radius to consider a unit 'near' to an enemy unit
		int ScoutDefenseRadius				= 600;		// radius to chase enemy scout worker
    }

    namespace Macro
    {
        int BOSSFrameLimit                  = 160;
        int WorkersPerRefinery              = 3;
		double WorkersPerPatch              = 3.0;
		int AbsoluteMaxWorkers				= 75;
        int BuildingSpacing                 = 1;
        int PylonSpacing                    = 3;
		int ProductionJamFrameLimit			= 360;
    }

    namespace Tools								
    {
        int MAP_GRID_SIZE            = 320;      // size of grid spacing in MapGrid
    }

	namespace Scout
	{
		int ScoutRound = 4;
		std::hash_set<std::string> MustScout;
	}

	namespace AntiCannon
	{
		std::hash_set<std::string> CannonProtoss;
	}

	std::string ConfigFileStr = "{\"Bot Info\":{\"BotName\":\"XBot\",\"Authors\":\"Xun\",\"PrintInfoOnStart\":true},\"BWAPI\":{\"SetLocalSpeed\":1,\"SetFrameSkip\":0,\"UserInput\":true,\"CompleteMapInformation\":false},\"Micro\":{\"KiteWithRangedUnits\":true,\"KiteLongerRangedUnits\":[\"Mutalisk\",\"Guardian\"],\"WorkersDefendRush\":true,\"RetreatMeleeUnitShields\":2,\"RetreatMeleeUnitHP\":{\"Zerg\":8},\"RegroupRadius\":500,\"UnitNearEnemyRadius\":500,\"ScoutDefenseRadius\":{\"Terran\":500}},\"Macro\":{\"BOSSFrameLimit\":160,\"ProductionJamFrameLimit\":1440,\"WorkersPerRefinery\":3,\"WorkersPerPatch\":{\"Zerg\":1.6},\"AbsoluteMaxWorkers\":75,\"BuildingSpacing\":1,\"PylonSpacing\":3},\"Debug\":{\"ErrorLogFilename\":\"bwapi-data/AI/XBot_ErrorLog.txt\",\"LogAssertToErrorFile\":true,\"DrawGameInfo\":true,\"DrawUnitHealthBars\":false,\"DrawProductionInfo\":true,\"DrawBuildOrderSearchInfo\":false,\"DrawScoutInfo\":false,\"DrawEnemyUnitInfo\":false,\"DrawModuleTimers\":false,\"DrawResourceInfo\":false,\"DrawCombatSimInfo\":false,\"DrawUnitTargetInfo\":false,\"DrawUnitOrders\":false,\"DrawBWTAInfo\":false,\"DrawMapGrid\":false,\"DrawMapDistances\":false,\"DrawBaseInfo\":false,\"DrawStrategyBossInfo\":false,\"DrawSquadInfo\":false,\"DrawWorkerInfo\":false,\"DrawMouseCursorInfo\":false,\"DrawBuildingInfo\":false,\"DrawReservedBuildingTiles\":false,\"DrawBOSSStateInfo\":false},\"Tools\":{\"MapGridSize\":320},\"Scout\":{\"Must\":[\"UAlbertaBot\",\"Dave Churchill\"],\"ScoutRound\":4},\"AntiCannon\":{\"CannonProtoss\":[\"Ximp\",\"McRave\",\"Tomas Vajda\"]},\"Strategy\":{\"ScoutHarassEnemy\":false,\"SurrenderWhenHopeIsLost\":true,\"ReadDirectory\":\"bwapi-data/read/\",\"WriteDirectory\":\"bwapi-data/write/\",\"ZvT\":{\"Zerg\":[{\"Weight\":5,\"Strategy\":\"OverhatchExpo\"},{\"Weight\":90,\"Strategy\":\"11Gas10PoolLurker\"},{\"Weight\":5,\"Strategy\":\"ZvT_13Pool\"}]},\"ZvP\":{\"Zerg\":[{\"Weight\":5,\"Strategy\":\"FastPool\",\"Weight2\":10},{\"Weight\":5,\"Strategy\":\"9PoolSpeed\"},{\"Weight\":5,\"Strategy\":\"9PoolHatch\"},{\"Weight\":5,\"Strategy\":\"9HatchExpo9Pool9Gas\"},{\"Weight\":15,\"Strategy\":\"OverpoolSpeed\"},{\"Weight\":2,\"Strategy\":\"ZvP_Overpool3Hatch\",\"Weight2\":0},{\"Weight\":15,\"Strategy\":\"OverhatchLing\"},{\"Weight\":10,\"Strategy\":\"OverhatchMuta\"},{\"Weight\":10,\"Strategy\":\"OverhatchExpo\"},{\"Weight\":5,\"Strategy\":\"11Gas10PoolLurker\"},{\"Weight\":18,\"Strategy\":\"3HatchLingExpo\",\"Weight2\":0},{\"Weight\":0,\"Strategy\":\"3HatchLing\",\"Weight2\":18},{\"Weight\":1,\"Strategy\":\"2HatchHydra\"},{\"Weight\":0,\"Strategy\":\"3HatchHydra\",\"Weight2\":5},{\"Weight\":5,\"Strategy\":\"3HatchHydraExpo\",\"Weight2\":0}]},\"ZvZ\":{\"Zerg\":[{\"Weight\":5,\"Strategy\":\"FastPool\"},{\"Weight\":9,\"Strategy\":\"9PoolSpeed\"},{\"Weight\":10,\"Strategy\":\"9HatchMain9Pool9Gas\"},{\"Weight\":20,\"Strategy\":\"OverhatchLing\",\"Weight2\":10},{\"Weight\":25,\"Strategy\":\"OverhatchMuta\",\"Weight2\":10},{\"Weight\":12,\"Strategy\":\"11Gas10PoolSpire\",\"Weight4\":10}]},\"ZvU\":{\"Zerg\":[{\"Weight\":5,\"Strategy\":\"FastPool\",\"Weight2\":15},{\"Weight\":15,\"Strategy\":\"9PoolHatch\"},{\"Weight\":5,\"Strategy\":\"9PoolHatchSunk\"},{\"Weight\":5,\"Strategy\":\"9PoolExpo\"},{\"Weight\":20,\"Strategy\":\"9PoolSpeed\"},{\"Weight\":20,\"Strategy\":\"OverpoolSpeed\"},{\"Weight\":15,\"Strategy\":\"OverhatchMuta\",\"Weight2\":5},{\"Weight\":15,\"Strategy\":\"OverhatchLing\",\"Weight2\":5}]},\"UseEnemySpecificStrategy\":true,\"EnemySpecificStrategy\":{\"2\":\"FastSpire\",\"Steamhammer\":\"FastSpire\",\"MegaBot\":\"5PoolHard\",\"Microwave\":\"ZvZ_Overpool9Gas\",\"McRave\":\"2HatchMutaMcRave\",\"Dave Churchill\":\"OverpoolSpeedDave\",\"UAlbertaBot\":\"OverpoolSpeedDave\",\"Tomas Vajda\":\"2HatchMutaXimp\",\"Ximp\":\"2HatchMutaXimp\",\"Aiur\":\"5PoolHard\",\"Florian Richoux\":\"5PoolHard\",\"Xelnaga\":\"5PoolHard\",\"Skynet\":\"5PoolHardSkynet\",\"Andrew Smith\":\"5PoolHardSkynet\",\"ZZZKBot\":\"9PoolSpeedExpo\",\"Chris Coxe\":\"9PoolSpeedExpo\"},\"StrategyCombos\":{\"FastPool\":{\"Zerg\":[{\"Weight\":1,\"Strategy\":\"4PoolHard\",\"Weight2\":5},{\"Weight\":4,\"Strategy\":\"4PoolSoft\",\"Weight2\":10},{\"Weight\":80,\"Strategy\":\"5PoolHard\"},{\"Weight\":5,\"Strategy\":\"5PoolSoft\"}]},\"Anti5Pool\":{\"Zerg\":[{\"Weight\":20,\"Strategy\":\"9PoolSpeed\"},{\"Weight\":20,\"Strategy\":\"9PoolHatchSunk\"},{\"Weight\":30,\"Strategy\":\"OverpoolSpeed\"},{\"Weight\":30,\"Strategy\":\"OverpoolHatch\"}]}},\"Strategies\":{\"OverpoolSpeedDave\":{\"Race\":\"Zerg\",\"OpeningBuildOrder\":[\"5 x drone\",\"overlord\",\"spawning pool\",\"go scout if needed\",\"2 x drone\",\"extractor\",\"go gas until 100\",\"drone\",\"6 x zergling\",\"metabolic boost\",\"6 x zergling\"]},\"2HatchMutaXimp\":{\"Race\":\"Zerg\",\"OpeningGroup\":\"zergling_rush\",\"OpeningBuildOrder\":[\"5 x drone\",\"overlord\",\"go scout if needed\",\"spawning pool\",\"2 x drone\",\"extractor\",\"3 x drone\",\"hatchery\",\"drone\",\"Lair\",\"6 x drone\",\"spire\",\"2 x drone\",\"extractor\",\"3 x drone\",\"overlord\",\"drone\",\"overlord\",\"8 x mutalisk\",\"2 x scourge\",\"hatchery\",\"2 x mutalisk\",\"overlord\",\"2 x scourge\",\"4 x mutalisk\",\"2 x scourge\",\"2 x mutalisk\",\"2 x scourge\",\"4 x drone\",\"6 x mutalisk\",\"extractor\",\"2 x scourge\",\"drone\",\"4 x mutalisk\",\"2 x scourge\",\"4 x mutalisk\",\"2 x scourge\",\"drone\",\"4 x mutalisk\",\"2 x scourge\",\"drone\",\"4 x mutalisk\",\"2 x scourge\",\"drone\",\"4 x mutalisk\",\"2 x scourge\",\"drone\",\"4 x mutalisk\",\"2 x scourge\",\"drone\",\"4 x drone\"]},\"2HatchMutaMcRave\":{\"Race\":\"Zerg\",\"OpeningGroup\":\"zergling_rush\",\"OpeningBuildOrder\":[\"5 x drone\",\"overlord\",\"go scout if needed\",\"spawning pool\",\"2 x drone\",\"extractor\",\"3 x drone\",\"hatchery\",\"drone\",\"creep colony\",\"Lair\",\"2 x drone\",\"sunken colony\",\"4 x drone\",\"spire\",\"2 x drone\",\"extractor\",\"3 x drone\",\"overlord\",\"drone\",\"overlord\",\"8 x mutalisk\",\"2 x scourge\",\"hatchery\",\"2 x mutalisk\",\"overlord\",\"2 x scourge\",\"4 x mutalisk\",\"2 x scourge\",\"2 x mutalisk\",\"2 x scourge\",\"4 x drone\",\"6 x mutalisk\",\"extractor\",\"2 x scourge\",\"drone\",\"4 x mutalisk\",\"2 x scourge\",\"4 x mutalisk\",\"2 x scourge\",\"drone\",\"4 x mutalisk\",\"2 x scourge\",\"drone\",\"4 x mutalisk\",\"2 x scourge\",\"drone\",\"4 x mutalisk\",\"2 x scourge\",\"drone\",\"4 x mutalisk\",\"2 x scourge\",\"drone\",\"4 x drone\"]},\"4PoolHard\":{\"Race\":\"Zerg\",\"OpeningGroup\":\"zergling_rush\",\"OpeningBuildOrder\":[\"spawning pool\",\"go scout location\",\"6 x zergling\"]},\"4PoolSoft\":{\"Race\":\"Zerg\",\"OpeningGroup\":\"zergling_rush\",\"OpeningBuildOrder\":[\"spawning pool\",\"go scout location\",\"drone\",\"5 x zergling\",\"go extractor trick zergling\",\"15 x zergling\"]},\"5PoolHard\":{\"Race\":\"Zerg\",\"OpeningGroup\":\"zergling_rush\",\"OpeningBuildOrder\":[\"drone\",\"spawning pool\",\"go scout location\",\"2 x drone\",\"3 x zergling\",\"go extractor trick zergling\",\"7 x zergling\",\"hatchery\",\"drone\",\"13 x zergling\"]},\"5PoolSoft\":{\"Race\":\"Zerg\",\"OpeningGroup\":\"zergling_rush\",\"OpeningBuildOrder\":[\"drone\",\"spawning pool\",\"go scout location\",\"2 x drone\",\"3 x zergling\",\"go extractor trick zergling\",\"3 x drone\"]},\"5PoolHardSkynet\":{\"Race\":\"Zerg\",\"OpeningGroup\":\"zergling_rush\",\"OpeningBuildOrder\":[\"drone\",\"spawning pool\",\"go scout location\",\"2 x drone\",\"go ignore scout worker\",\"3 x zergling\",\"go extractor trick zergling\",\"7 x zergling\",\"hatchery\",\"drone\",\"13 x zergling\"]},\"9PoolSpeed\":{\"Race\":\"Zerg\",\"OpeningGroup\":\"zergling_rush\",\"OpeningBuildOrder\":[\"5 x drone\",\"spawning pool\",\"drone\",\"go scout location\",\"extractor\",\"go gas until 100\",\"overlord\",\"drone\",\"3 x zergling\",\"metabolic boost\",\"5 x zergling\",\"hatchery\",\"drone\",\"zergling\",\"hatchery @ macro\",\"drone\",\"3 x zergling\",\"go start gas\",\"2 x zergling\",\"3 x drone\"]},\"9PoolSpeedExpo\":{\"Race\":\"Zerg\",\"OpeningGroup\":\"zergling_rush\",\"OpeningBuildOrder\":[\"5 x drone\",\"spawning pool\",\"drone\",\"go scout location\",\"extractor\",\"go gas until 100\",\"overlord\",\"drone\",\"3 x zergling\",\"metabolic boost\",\"5 x zergling\",\"creep colony\",\"zergling\",\"sunken colony\",\"hatchery\",\"drone\",\"zergling\",\"drone\",\"3 x zergling\",\"go start gas\",\"2 x zergling\",\"3 x drone\"]},\"9PoolHatch\":{\"Race\":\"Zerg\",\"OpeningGroup\":\"zergling_rush\",\"OpeningBuildOrder\":[\"5 x drone\",\"spawning pool\",\"drone\",\"go extractor trick drone\",\"overlord\",\"go scout location\",\"3 x zergling\",\"hatchery\",\"drone\",\"2 x zergling\",\"drone\",\"extractor\",\"4 x zergling\"]},\"9PoolHatchSunk\":{\"Race\":\"Zerg\",\"OpeningGroup\":\"zergling_rush\",\"OpeningBuildOrder\":[\"5 x drone\",\"spawning pool\",\"drone\",\"creep colony\",\"drone\",\"overlord\",\"go scout location\",\"sunken colony\",\"3 x zergling\",\"hatchery @ macro\",\"drone\",\"4 x zergling\"]},\"9PoolExpo\":{\"Race\":\"Zerg\",\"OpeningGroup\":\"zergling_rush\",\"OpeningBuildOrder\":[\"5 x drone\",\"spawning pool\",\"drone\",\"creep colony\",\"drone\",\"overlord\",\"go scout location\",\"sunken colony\",\"6 x zergling\",\"hatchery @ hidden\",\"drone\",\"4 x zergling\"]},\"ZvZ_Overpool9Gas\":{\"Race\":\"Zerg\",\"OpeningBuildOrder\":[\"5 x drone\",\"overlord\",\"spawning pool\",\"go scout location\",\"drone\",\"extractor\",\"go gas until 850\",\"2 x drone\",\"3 x zergling\",\"lair\",\"zergling\",\"creep colony\",\"drone\",\"sunken colony\",\"2 x zergling\",\"drone\",\"spire\",\"creep colony\",\"drone\",\"sunken colony\",\"overlord\",\"zergling\",\"6 x mutalisk\"]},\"OverpoolHatch\":{\"Race\":\"Zerg\",\"OpeningBuildOrder\":[\"5 x drone\",\"overlord\",\"spawning pool\",\"go scout location\",\"3 x drone\",\"3 x zergling\",\"hatchery @ macro\",\"extractor\",\"go gas until 850\",\"3 x zergling\",\"metabolic boost\",\"2 x zergling\",\"lair\",\"drone\",\"5 x zergling\",\"spire\",\"drone\",\"creep colony\",\"drone\",\"sunken colony\"]},\"OverpoolSpeed\":{\"Race\":\"Zerg\",\"OpeningGroup\":\"zergling_rush\",\"OpeningBuildOrder\":[\"5 x drone\",\"overlord\",\"spawning pool\",\"go scout if needed\",\"2 x drone\",\"extractor\",\"go gas until 100\",\"drone\",\"4 x zergling\",\"metabolic boost\",\"2 x zergling\",\"hatchery @ natural\",\"3 x zergling\"]},\"ZvP_Overpool3Hatch\":{\"Race\":\"Zerg\",\"OpeningGroup\":\"zergling_rush\",\"OpeningBuildOrder\":[\"5 x drone\",\"overlord\",\"spawning pool\",\"go scout if needed\",\"3 x drone\",\"3 x zergling\",\"hatchery @ natural\",\"hatchery @ min only\"]},\"OverpoolLurker\":{\"Race\":\"Zerg\",\"OpeningBuildOrder\":[\"5 x drone\",\"overlord\",\"spawning pool\",\"drone\",\"extractor\",\"go gas until 950\",\"2 x drone\",\"go scout location\",\"3 x zergling\",\"lair\",\"2 x drone\",\"hydralisk den\",\"lurker aspect\",\"metabolic boost\",\"4 x hydralisk\",\"3 x lurker\",\"3 x zergling\",\"lurker\",\"2 x zergling\",\"drone\",\"hatchery\"]},\"9HatchMain9Pool9Gas\":{\"Race\":\"Zerg\",\"OpeningGroup\":\"zergling_rush\",\"OpeningBuildOrder\":[\"5 x drone\",\"hatchery @ macro\",\"go scout if needed\",\"drone\",\"spawning pool\",\"drone\",\"extractor\",\"go gas until 100\",\"overlord\",\"drone\",\"3 x zergling\",\"metabolic boost\",\"3 x zergling\",\"drone\",\"9 x zergling\"]},\"9HatchExpo9Pool9Gas\":{\"Race\":\"Zerg\",\"OpeningGroup\":\"zergling_rush\",\"OpeningBuildOrder\":[\"5 x drone\",\"hatchery\",\"go scout location\",\"drone\",\"spawning pool\",\"drone\",\"extractor\",\"go gas until 100\",\"overlord\",\"drone\",\"3 x zergling\",\"metabolic boost\",\"drone\",\"9 x zergling\"]},\"OverhatchLing\":{\"Race\":\"Zerg\",\"OpeningGroup\":\"zergling_rush\",\"OpeningBuildOrder\":[\"5 x drone\",\"overlord\",\"hatchery @ macro\",\"drone\",\"spawning pool\",\"2 x drone\",\"go scout location\",\"extractor\",\"go gas until 100\",\"drone\",\"5 x zergling\",\"metabolic boost\",\"12 x zergling\",\"hatchery\",\"3 x zergling\",\"drone\",\"go start gas\"]},\"OverhatchMuta\":{\"Race\":\"Zerg\",\"OpeningBuildOrder\":[\"5 x drone\",\"overlord\",\"hatchery @ macro\",\"drone\",\"spawning pool\",\"2 x drone\",\"go scout location\",\"extractor\",\"go gas until 350\",\"drone\",\"5 x zergling\",\"metabolic boost\",\"drone\",\"6 x zergling\",\"lair\",\"4 x zergling\",\"drone\",\"4 x zergling\",\"drone\",\"spire\",\"go start gas\"]},\"OverhatchExpo\":{\"Race\":\"Zerg\",\"OpeningGroup\":\"zergling_rush\",\"OpeningBuildOrder\":[\"go defensive\",\"5 x drone\",\"overlord\",\"hatchery @ natural\",\"drone\",\"spawning pool\",\"4 x drone\",\"go scout location\",\"4 x zergling\",\"creep colony @ natural\",\"drone\",\"zergling\",\"sunken colony\",\"go aggressive\",\"2 x zergling\",\"hatchery @ macro\",\"2 x zergling\",\"extractor\",\"3 x drone\"]},\"FastSpire\":{\"Race\":\"Zerg\",\"OpeningBuildOrder\":[\"5 x drone\",\"overlord\",\"go wait mineral until 250\",\"spawning pool\",\"extractor\",\"5 x drone\",\"go keep build sunken\",\"go gas until 900\",\"creep colony\",\"lair\",\"creep colony\",\"sunken colony\",\"drone\",\"sunken colony\",\"drone\",\"spire\",\"creep colony\",\"drone\",\"overlord\",\"sunken colony\",\"overlord\",\"drone\",\"drone\",\"drone\",\"go wait mineral until 300\",\"12 x mutalisk\"]},\"11Gas10PoolSpire\":{\"Race\":\"Zerg\",\"OpeningBuildOrder\":[\"4 x drone\",\"overlord\",\"3 x drone\",\"extractor\",\"go gas until 950\",\"spawning pool\",\"go scout location\",\"2 x drone\",\"lair\",\"3 x zergling\",\"drone\",\"metabolic boost\",\"zergling\",\"overlord\",\"Spire\",\"zergling\",\"drone\",\"creep colony\",\"zergling\",\"sunken colony\",\"zergling\",\"6 x mutalisk\",\"hatchery @ macro\",\"drone\",\"2 x zergling\",\"drone\",\"2 x zergling\",\"drone\",\"2 x zergling\",\"drone\",\"2 x zergling\",\"hatchery\",\"go start gas\"]},\"11Gas10PoolLurker\":{\"Race\":\"Zerg\",\"OpeningBuildOrder\":[\"4 x drone\",\"overlord\",\"3 x drone\",\"extractor\",\"spawning pool\",\"go scout location\",\"2 x drone\",\"lair\",\"3 x zergling\",\"drone\",\"overlord\",\"hydralisk den\",\"drone\",\"lurker aspect\",\"metabolic boost\",\"4 x hydralisk\",\"zergling\",\"3 x lurker\",\"3 x zergling\",\"lurker\",\"hatchery\"]},\"ZvT_13Pool\":{\"Race\":\"Zerg\",\"OpeningBuildOrder\":[\"4 x drone\",\"overlord\",\"5 x drone\",\"go scout location\",\"spawning pool\",\"extractor\",\"2 x drone\",\"hatchery @ natural\",\"Lair\",\"drone\",\"2 x zergling\",\"3 x drone\",\"SPIRE\",\"drone\",\"creep colony @ natural\",\"drone\",\"sunken colony\",\"3 x drone\",\"2 x overlord\",\"extractor\",\"6 x mutalisk\",\"2 x scourge\",\"6 x mutalisk\",\"6 x drone\"]},\"2HatchHydra\":{\"Race\":\"Zerg\",\"OpeningBuildOrder\":[\"4 x drone\",\"overlord\",\"4 x drone\",\"go scout if needed\",\"hatchery\",\"spawning pool\",\"drone\",\"extractor\",\"3 x drone\",\"hydralisk den\",\"3 x zergling\",\"drone\",\"muscular augments\",\"drone\",\"4 x hydralisk\",\"2 x drone\",\"zergling\",\"grooved spines\",\"8 x hydralisk\"]},\"3HatchLing\":{\"Race\":\"Zerg\",\"OpeningGroup\":\"zergling_rush\",\"OpeningBuildOrder\":[\"4 x drone\",\"overlord\",\"4 x drone\",\"go scout if needed\",\"hatchery\",\"spawning pool\",\"3 x drone\",\"3 x zergling\",\"hatchery @ macro\",\"extractor\",\"go gas until 100\",\"5 x zergling\",\"metabolic boost\",\"9 x zergling\"]},\"3HatchLingExpo\":{\"Race\":\"Zerg\",\"OpeningGroup\":\"zergling_rush\",\"OpeningBuildOrder\":[\"4 x drone\",\"overlord\",\"4 x drone\",\"go scout if needed\",\"hatchery\",\"spawning pool\",\"3 x drone\",\"3 x zergling\",\"hatchery @ min only\",\"extractor\",\"go gas until 100\",\"4 x zergling\",\"drone\",\"metabolic boost\",\"9 x zergling\"]},\"3HatchHydra\":{\"Race\":\"Zerg\",\"OpeningBuildOrder\":[\"4 x drone\",\"overlord\",\"4 x drone\",\"go scout if needed\",\"hatchery\",\"2 x drone\",\"spawning pool\",\"3 x drone\",\"hatchery @ macro\",\"3 x zergling\",\"extractor\",\"drone\",\"2 x zergling\",\"drone\",\"hydralisk den\",\"4 x drone\",\"extractor\",\"4 x hydralisk\",\"muscular augments\",\"3 x drone\",\"4 x hydralisk\"]},\"3HatchHydraExpo\":{\"Race\":\"Zerg\",\"OpeningBuildOrder\":[\"4 x drone\",\"overlord\",\"4 x drone\",\"go scout if needed\",\"hatchery\",\"2 x drone\",\"spawning pool\",\"3 x drone\",\"hatchery @ min only\",\"3 x zergling\",\"extractor\",\"drone\",\"2 x zergling\",\"drone\",\"hydralisk den\",\"4 x drone\",\"extractor\",\"4 x hydralisk\",\"muscular augments\",\"3 x drone\",\"4 x hydralisk\"]}}}}";

}