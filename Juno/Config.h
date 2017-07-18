#pragma once 


namespace Config
{
	namespace Game
	{
		extern int UserInput; 
		extern int CompleteMapInformation; 
		extern int GameSpeed; 
		extern bool UseThread;
	}
	namespace Protoss
	{
		extern double MineSpeed; 
	}
	namespace StrategyManager
	{
		extern int FrequencyFrame; 
		extern int SearchMaxDepth; 
		extern int MinAttackNum;
		extern int SearchFrequencyFrame; 
		extern int MinMineNum; 
	}
	namespace InformationManager
	{
		extern int FrequencyFrame; 
		extern int GetNearBuildLength;     /* suggest 4, half sight range */
		const int LargeTileLength = 128; 
		extern int CannonEquivalentUnits; 
		extern int BaseLocationScoutFrame; 
		extern int ScoutLargeTileRange; 
	}
	namespace BuildingManager
	{
		extern int FrequencyFrame; 
		extern int PlanBuildingWaitFrame; 
		extern int ConstructingEnemyRadius; 
		extern int BaseBuildRadius; 
		extern int BuildLocationSpace;
	}
	namespace UnitProbeManager
	{
		extern int FrequencyFrame;
		extern int MapUnAnalyzeScoutNum; 
	}
	namespace UnitProbeInterface
	{
		extern int FrequencyFrame; 
		extern int BuilderCloseDistance; 
		extern int BuildNearMaxRange; 
		extern int BlockSpan; 
		extern int BlockRadius; 
		extern int EnemyReward; 
		extern int ObstacleReward;
		extern int UnWalkableReward; 
		extern int TargetReward; 
		extern int MineralReward; 
		extern int IterateTime; 
	}
	namespace Debug
	{
		extern bool DrawAttack; 
		extern bool DrawMove; 
		extern bool DrawRightClick; 
		extern bool DrawAttackTarget; 
		extern bool DrawBlocksRewards; 
		extern bool DrawPlanBuilding; 
		extern bool DrawSearchBuild; 
		extern bool DrawTime; 
	}
	namespace Timer
	{
		extern double TotalMicroSec; 
		extern double InformationManagerMaxMicroSec; 
		extern double StrategyManagerMaxMicroSec;
		extern double BuildingManagerMaxMicroSec;
		extern double UnitProbeInterfaceMaxMicroSec;
	}
}