#pragma once 


#include <windows.h>
#include <BWAPI.h>
#include <BWTA.h>
#include "UnitProbeInterface.h"


typedef std::pair<BWAPI::UnitType, int>		BuildActionFrame; 

typedef std::vector<BuildActionFrame>		BuildActionFrames; 


struct SearchThreadParam
{
	int depth; 
	int max_depth;
	int current_frame;
	int max_frame;
	double current_mineral;
	int available_supply;
	int probe_left_frame;
	int mine_num;
	int pylon_left_frame;
	int pylon_num;
	int pylon_need;
	int forge_left_frame;
	int forge_num;
	int forge_need;
	int cannon_left_frame;
	int cannon_num;
	int cannon_need; 

};


DWORD WINAPI searchThread(LPVOID pParam);

extern HANDLE hthread_search; 

class StrategyManager
{
public:

	StrategyManager() {}; 

	/* do some initial thing */
	void initial() 
	{ 
		build_action_frames.clear(); 
		update_build_action_frames.clear(); 
		is_updated_build_actions = false; 
		is_searching = false; 
		last_frame = 0; 
	}

	/* try to adjust unit, building, production, etc */
	void onFrame(); 

	static StrategyManager& getInstance() { static StrategyManager sm; return sm; }; 

	static BuildActionFrames searchCannonBuild2(int depth,
		const int& max_depth,
		int current_frame,
		int max_frame,
		double current_mineral,
		int available_supply,
		int probe_left_frame,
		int mine_num,
		int pylon_left_frame,
		int pylon_num,
		const int& pylon_need,
		int forge_left_frame,
		int forge_num,
		const int& forge_need,
		int cannon_left_frame,
		int cannon_num,
		const int& cannon_need);

public:

	bool is_searching;

	BuildActionFrames update_build_action_frames;

	bool is_updated_build_actions;




private:

	int last_frame; 

	int last_action_update_frame; 

	BuildActionFrames build_action_frames; 

	/* replace low hp defend units in a region by mineral units */
	void swapRegionDefendMineral(BWTA::Region* region); 

	/* let extra defend units in a region to mineral */
	void makeRegionDefendToMineral(BWTA::Region* region); 

	/* let mineral to defend if enemy is more than self */
	void makeRegionMineralToDefend(BWTA::Region* region); 

	/* let mineral attack very close enemy */
	void makeRegionMineralFastAttack(BWTA::Region* region); 

	/* let attack after combat to scout */
	void makeRegionAttackToScout(BWTA::Region* region); 

	/* let more mineral to scout */
	void makeRegionMineralToScout(BWTA::Region* region); 

	/* let scout to assist attack region */
	void makeScoutToAttack(); 

	/* let mineral to replenish attack */
	void makeMineralToAttack(BWTA::Region* region); 

	/* replenish supply when limited */
	void replenishSupply(); 

	/* update build actions */
	void updateBuildActions(); 

	/* follow searched build actions*/
	void executeBuildActions(); 

	/* recover build power */
	void recoverBuildPower(); 

	/* train Probe */
	void trainProbe(); 

	/* build Cannon */
	void buildCannon(); 

	/* rebuild Depot if destroyed */
	void rebuildDepot(); 

	/* recover before map analyze units */
	void recoverBeforeMapAnalyzeUnits(); 

	/* replenish minimum attack */
	void replenishMinimumAttack(BWTA::Region* region); 

	/* sort compare function
	just compare hp+shield
	convert DEFEND unit back to MINERAL and convert MINERAL unit to DEFEND
	former and latter is opponent ordered */
	static bool compUnitHpShield(const UnitProbe& u1, const UnitProbe& u2);
};


