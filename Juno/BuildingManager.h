#pragma once


#include <BWAPI.h>
#include "UnitProbeInterface.h"

class UnitProbeInterface; 

typedef UnitProbeInterface* UnitProbe; 

/* self plan build data, also store builder (Probe) previous data */
struct PlanBuildData
{
	BWAPI::UnitType				building_type;
	BWAPI::TilePosition			location; 
	UnitProbe					builder; 
	enum Role					builder_role; 
	BWTA::Region*				builder_target_region; 
	BWTA::BaseLocation*			builder_target_base_location;
	BWAPI::Unit					builder_target_unit;
	BWAPI::Position				builder_target_position;  
	int							command_frame; 

	/* recover builder previous data */
	void builderRecover();

};


/* store self building data, including built, constructing, planned */
class BuildingManager
{
public:

	BuildingManager() {};
	
	/* do some initial work */
	void initial() 
	{ 
		buildings.clear(); 
		constructings.clear(); 
		plannings.clear(); 
		map_unanalyze_buildings.clear();  
		last_frame = 0; 
	};

	/* onFrame call: initialize variables */
	void onFrame(); 

	/* onUnitCreate call: new self building is constructed */
	void onUnitCreate(BWAPI::Unit b); 

	/* onUnitDestroy call: self building is destroyed */
	void onUnitDestroy(BWAPI::Unit b); 

	/* delete pointer after game */
	void onEnd(); 

	/* add plan building item */
	void addPlanBuilding(PlanBuildData* pbd);

	/* remove plan building item */
	void removePlanBuilding(PlanBuildData* pbd); 

	/* return a neighbor buildable location for a given build type
	also return if the location is powered */
	BWAPI::TilePosition getNearBuildLocation(const BWAPI::TilePosition& location,
		const BWAPI::UnitType& unit_type,
		int max_length);

	/* assign a unit to build certain type of building at given location */
	void buildAtLocation(const BWAPI::TilePosition& p, const BWAPI::UnitType& type); 

	/* return cannon location where it can attack target
	if return location is powered, build directly at return location
	other first build a pylon */
	bool getCannonLocation(const BWAPI::TilePosition& target, BWAPI::TilePosition& location);

	/* return base region build certain unit type location that has power */
	BWAPI::TilePosition getBaseBuildLocation(const BWAPI::UnitType& type);


	/* get certain type or all buildings */
	std::vector<BWAPI::Unit> getBuildingUnits(BWAPI::UnitType type = BWAPI::UnitTypes::None); 

	std::vector<BWAPI::Unit> getConstructingUnits(BWAPI::UnitType type = BWAPI::UnitTypes::None);

	std::vector<PlanBuildData*> getPlanningUnits(BWAPI::UnitType type = BWAPI::UnitTypes::None);

	/* return base region build pylon location with the idea of extanding powered area */
	BWAPI::TilePosition getBasePylonLocation();

	std::vector<BWAPI::Unit>& getMapUnAnalyzeBuildings() { return map_unanalyze_buildings; }; 

	/* build a pylon at self region */
	void buildBasePylon(); 

	/* make a position has power, 
	if some pylon is constructing, return directly 
	otherwise build a pylon at the same location
	return if or not a new pylon is built */
	bool makeLocationPower(const BWAPI::TilePosition& tp); 

	bool canBuildHere(const BWAPI::TilePosition& tp, const BWAPI::UnitType& type); 

	bool BuildingManager::buildable(int x, int y, const BWAPI::UnitType & type);

	UnitProbe getBuilder(const BWAPI::Position& p); 


	/* get singleton instance of BuildingManager */
	static BuildingManager& getInstance() { static BuildingManager bm; return bm; };


private:

	std::vector<BWAPI::Unit>	buildings;				/* built buildings */

	std::vector<BWAPI::Unit>	constructings;			/* constructing buildings */

	std::vector<PlanBuildData*>	plannings;			/* planned buildings */

	std::vector<BWAPI::Unit>	map_unanalyze_buildings;		// before map analyze first store buildings here 

	int last_frame; 

};