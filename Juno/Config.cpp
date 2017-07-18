#include "Config.h"

namespace Config
{
	namespace Game
	{
		extern int CompleteMapInformation		= 0;
		extern int UserInput					= 1;
		extern int GameSpeed					= 42;
		extern bool UseThread					= true;
	}
	namespace Protoss
	{
		extern double MineSpeed					= 0.03;			/* one probe mine speed per frame */
	}
	namespace StrategyManager
	{
		extern int FrequencyFrame				= 42;
		extern int SearchMaxDepth				= 30;
		extern int MinAttackNum					= 2;		// minimum attackers in each region
		extern int SearchFrequencyFrame			= 42 * 10; 
		extern int MinMineNum					= 2;
	}
	namespace InformationManager
	{
		extern int FrequencyFrame				= 21;
		extern int CannonEquivalentUnits		= 4;		// each cannon can attack how many units 
		extern int BaseLocationScoutFrame		= 42*60;
		extern int ScoutLargeTileRange			= 4;		// larger than 256/128 
	}
	namespace BuildingManager
	{
		extern int FrequencyFrame				= 21;
		extern int PlanBuildingWaitFrame		= 42 * 20;
		extern int ConstructingEnemyRadius		= 32*4;
		extern int BaseBuildRadius				= 8*3;
		extern int BuildLocationSpace			= 2;
	}
	namespace UnitProbeManager
	{
		extern int FrequencyFrame				= 8; 
		extern int MapUnAnalyzeScoutNum			= 3;
	}
	namespace UnitProbeInterface
	{
		extern int FrequencyFrame				= UnitProbeManager::FrequencyFrame;
		extern int BuilderCloseDistance			= 2*32;			/* builder should first move close to build location less than this distance */
		extern int BuildNearMaxRange			= 6;			/* half sight range of probe, 256/2 */
		extern int BlockSpan					= 85;		// 64
		extern int BlockRadius					= 2;		// 3
		extern int EnemyReward					= -200; 
		extern int ObstacleReward				= -200; 
		extern int UnWalkableReward				= -200; 
		extern int TargetReward					= 200; 
		extern int MineralReward				= 100;
		extern int IterateTime					= 3;
	}
	namespace Debug
	{
		extern bool DrawAttack					= true;
		extern bool DrawMove					= true;
		extern bool DrawRightClick				= true;
		extern bool DrawAttackTarget			= true; 
		extern bool DrawBlocksRewards			= true;
		extern bool DrawPlanBuilding			= true; 
		extern bool DrawSearchBuild				= true;
		extern bool DrawTime					= true;
	}
	namespace Timer
	{
		extern double TotalMicroSec = 42;
		extern double InformationManagerMaxMicroSec = 10;
		extern double StrategyManagerMaxMicroSec = 10;
		extern double BuildingManagerMaxMicroSec = 10;
		extern double UnitProbeInterfaceMaxMicroSec = 1;
	}
}