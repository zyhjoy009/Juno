#pragma once


#include <BWAPI.h>
#include <BWTA.h>
#include "BuildingManager.h"


/* unit role:  ATTACK, DEFEND, MINERAL, SCOUT_START_LOCATION, SCOUT_BASE_LOCATION, SCOUT, BUILD*/
enum Role { ATTACK, DEFEND, MINERAL, SCOUT_START_LOCATION, SCOUT_BASE_LOCATION, SCOUT, BUILD, ROLE_NUM, BEFORE_MAP_ANALYZED_SCOUT, BEFORE_MAP_ANALYZED_MINERAL };

struct PlanBuildData; 


/* self defined class for Probe unit 
capable of ATTACK, DEFEND, MINERAL, SCOUT_START_LOCATION, SCOUT_BASE_LOCATION, SCOUT, BUILD 
7 types of actions */
class UnitProbeInterface
{
public:

	/* constrcution function */
	UnitProbeInterface(BWAPI::Unit u,
		Role r = ROLE_NUM,
		BWTA::Region* tr = nullptr,
		BWTA::BaseLocation* tbl = nullptr,
		BWAPI::Unit tu = nullptr,
		BWAPI::Position tp = BWAPI::Position(-1,-1), 
		PlanBuildData* pbdp = nullptr) : unit(u), role(r), target_region(tr), target_base_location(tbl), target_unit(tu), target_position(tp), plan_build_data_pointer(pbdp), last_frame(0) {};
	
	/* copy construction function */
	UnitProbeInterface(const UnitProbeInterface& upi) :
		unit(upi.unit),
		role(upi.role),
		target_region(upi.target_region),
		target_base_location(upi.target_base_location),
		target_unit(upi.target_unit),
		target_position(upi.target_position),
		plan_build_data_pointer(upi.plan_build_data_pointer), 
		last_frame(upi.last_frame){};

	/* update unit action */
	void onFrame();

	/* clear unit data */
	void clear() { role = ROLE_NUM; target_region = nullptr; target_base_location = nullptr; target_unit = nullptr; target_position = BWAPI::Position(-1, -1); plan_build_data_pointer = nullptr;  };

	/* set unit role */
	void setRole(const Role& r) { role = r;  };

	/* set target region */
	void setTargetRegion(BWTA::Region* tr) { target_region = tr;  };

	/* set target base location */
	void setTargetBaseLocation(BWTA::BaseLocation* tbl) { target_base_location = tbl;  };

	/* set target unit */
	void setTargetUnit(const BWAPI::Unit& tu) { target_unit = tu;  };

	/* set target position */
	void setTargetPosition(const BWAPI::Position& tp) { target_position = tp;  };

	/* set plan build data */
	void setPlanBuildData(PlanBuildData* pbd) { plan_build_data_pointer = pbd;  }

	/* get unit BWAPI interface */
	BWAPI::Unit getUnit() const { return unit; }; 

	/* get role */
	Role getRole() const { return role;  };

	/* get target region */
	BWTA::Region* getTargetRegion() { return target_region;  };

	/* get target base location */ 
	BWTA::BaseLocation* getTargetBaseLocation() { return target_base_location;  };

	/* get target unit */
	BWAPI::Unit getTargetUnit() { return target_unit;  };

	/* get target position */
	BWAPI::Position getTargetPosition() const { return target_position;  };

	/* get plan build data */
	PlanBuildData* getPlanBuildDataPointer() const { return plan_build_data_pointer; };

	/* get distance to a group of other units */
	int getDistances(const std::vector<BWAPI::Unit>& others); 

	/* get distances to certain objects from give position */
	int getDistances(const BWAPI::Position& p, const BWAPI::Unitset& unit_set); 
	int getDistances(const BWAPI::Position& p, const std::vector<BWAPI::TilePosition>& tp_set); 


	/* get block information */

	std::vector<std::vector <double>> getBlocksEnemyRewards(const BWAPI::Position& p, const BWAPI::Unit& target = nullptr); 

	std::vector<std::vector<double>> getBlocksObstacleRewards(const BWAPI::Position& p); 

	std::vector<std::vector<double>> getBlocksTargetRewards(const BWAPI::Position& p, const BWAPI::Unit& target); 

	std::vector<std::vector<double>> getBlocksTargetRewards(const BWAPI::Position& p, const BWAPI::Position& target);

	std::vector<std::vector<double>> getBlocksMineralRewards(const BWAPI::Position& p);

	std::vector<std::vector<double>> iterate(const std::vector<std::vector<double>>& r, int times);

	std::pair<int, int> getAction(const BWAPI::Position& p, const std::vector<std::vector<double>>& rewards); 


private:

	BWAPI::Unit				unit;						/* unit own BWAPI interface */

	Role					role;						/* unit role */

	BWTA::Region*			target_region;				/* unit target region */

	BWTA::BaseLocation*		target_base_location;		/* unit target base location */

	BWAPI::Unit				target_unit;				/* unit target unit */

	BWAPI::Position			target_position;			/* unit target position */

	PlanBuildData*			plan_build_data_pointer;	/* unit plan build data */

	int						last_frame; 

	/* attack action */
	void attack(); 

	/* defend action */
	void defend(); 

	/* mineral action */
	void mineral(); 

	/* scout action */
	void scout(); 

	/* build action */
	void build(); 

	/* stupid attack behavior */
	void stupidAttack(const BWAPI::Unit& target); 

	/* stupid move behavior */
	void stupidMove(const BWAPI::Position& target); 

	/* stupid move to mineral behavior */
	void stupidMoveToMineral(); 

	/* stupid return cargo behavior */
	void stupidReturnCargo(); 


	/* smart attack */
	void smartAttack(const BWAPI::Unit& u);

	/* smart move */
	void smartMove(const BWAPI::Position& p); 

	/* smart right click */
	void smartRightClick(const BWAPI::Unit& u); 

	/* smart attack move */
	void smartAttackMove(const BWAPI::Position& p); 

	/* smart return cargo */
	void smartReturnCargo(); 

	/* update scout target, including no target, or arrive target */
	void updateScoutTarget(); 

	/* overwhelming area, input two rectangle topleft point and width/height */
	int getOverwhelmArea(int x1, int y1, int w1, int h1,
		int x2, int y2, int w2, int h2); 

};

std::vector<std::vector<double>> operator + (const std::vector<std::vector<double>>& a, const std::vector<std::vector<double>>& b); 

std::vector<std::vector<double>> operator * (const std::vector<std::vector<double>>& a, const double& b); 

std::vector<std::vector<double>> operator * (const double& b, const std::vector<std::vector<double>>& a);
