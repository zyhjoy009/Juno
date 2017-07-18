#pragma once

#include <BWAPI.h>
#include <BWTA.h>
#include "UnitProbeInterface.h"
#include "Config.h"



typedef BWAPI::Point<int, Config::InformationManager::LargeTileLength> LargeTile; 


/* enemy building data, easy to deal with previously seen but now invisible buildings */
struct EnemyBuilding
{
	int						id;				/* building BWAPI unit id */
	BWAPI::UnitType			unit_type;		/* building BWAPI unit type */
	BWAPI::TilePosition		location;		/* building BWAPI unit location */

	EnemyBuilding() : id(0), unit_type(BWAPI::UnitTypes::None), location(BWAPI::TilePosition(-1, -1)) {}; 
	EnemyBuilding(const BWAPI::Unit& u) : id(u->getID()), unit_type(u->getType()), location(u->getTilePosition()) {}; 

};


/* store and handle whole information */
class InformationManager
{
public:

	/* initialize InformationManager variables */
	void initial(); 

	/* after map analyze intial */
	void mapAnalyzeInitial(); 

	/* onFrame call: update enemy information */
	void onFrame(); 

	/* onUnitShow call: add enemy information */
	void onUnitShow(BWAPI::Unit u); 

	/* onUnitHide call: remove enemy unit */
	void onUnitHide(BWAPI::Unit u); 

	/* onUnitCreate call: add self unit */
	void onUnitCreate(BWAPI::Unit u); 

	/* onUnitDestroy call: remove self unit */
	void onUnitDestroy(BWAPI::Unit u); 


	/* special function for UnitProbe */
	void onUnitCreate(UnitProbe up);


	/* get unit or position BWTA::Region* */
	BWTA::Region* getRegion(BWAPI::Unit u); 

	BWTA::Region* getRegion(BWAPI::Position p); 

	/* return self region */
	BWTA::Region* getSelfRegion() { return self_region; };

	/* return self base location */
	BWTA::BaseLocation* getSelfBaseLocation() { return self_base_location;  };

	/* return self units of prescribed role at certain region */
	std::vector<UnitProbe> getRegionSelfUnits(BWTA::Region* region, const Role& role = Role::ROLE_NUM); 

	/* return certain type or all of self building */
	std::vector<BWAPI::Unit> getRegionSelfBuildings(BWTA::Region* region, const BWAPI::UnitType& type = BWAPI::UnitTypes::None);

	/* return enemy units at certain region */
	std::vector<BWAPI::Unit> getRegionEnemyUnits(BWTA::Region* region); 

	/* return enemy buildings at certain region */
	std::vector<EnemyBuilding> getRegionEnemyBuildings(BWTA::Region* region);


	/* add self unit (ATTACK/DEFEND/MINERAL) at a certain region */
	void addRegionSelfUnit(const UnitProbe& u); 

	/* remove self unit from a certain region */
	void removeRegionSelfUnit(const UnitProbe& u); 

	/* calculate given region enemy weapon sum */
	double getRegionEnemyWeaponSum(BWTA::Region* r); 

	/* calculate given region self weapon sum */
	double getRegionSelfWeaponSum(BWTA::Region* r); 

	/* return current mineral minus planned buildings */
	int getMineral(); 
	
	/* return region desired target that needs a new cannon to attack */
	BWAPI::TilePosition getRegionCannonTarget(BWTA::Region* r); 
	

	/* return region highest attack target */
	static void getRegionAttackTarget(BWTA::Region* r, BWAPI::Unit& target_unit, BWAPI::Position& target_position); 

	/* get scout units */
	std::vector<UnitProbe> getStartLocationsScouts(); 

	std::vector<UnitProbe> getBaseLocationsScouts(); 

	std::vector<UnitProbe> getTileScouts(); 


	/* remove scout unit */
	void removeStartLocationScout(const UnitProbe& up); 

	void removeBaseLocationScout(const UnitProbe& up);

	void removeTileScout(const UnitProbe& up);

	/* add scout unit */
	void addStartLocationScout(const UnitProbe& up); 

	void addBaseLocationScout(const UnitProbe& up);

	void addTileScout(const UnitProbe& up);

	/* get most cannon desired region */
	BWTA::Region* getCannonDesiredRegion(); 

	// return enemy building iterator
	std::vector<EnemyBuilding>::iterator getEnemyBuildingIterator(std::vector<EnemyBuilding>& enemy_buildings, 
		const BWAPI::Unit& u); 

	std::vector<UnitProbe>::iterator getSelfUnitIterator(std::vector<UnitProbe>& self_units,
		const BWAPI::Unit& u); 

	/* get most desired scout position */
	std::pair<Role, BWAPI::Position> getDesireScoutTarget(const BWAPI::Position& p);

	bool isMapAnalyzed() { return is_map_analyzed; }; 
	void setMapAnalyzed() { is_map_analyzed = true; }; 

	bool isEnemyStartLocationFound() { return is_enemy_start_location_found; }; 

	/* get singleton instance */
	static InformationManager& getInstance() { static InformationManager im; return im; };


private:

	BWTA::Region*			self_region;				/* self region */

	BWTA::BaseLocation*		self_base_location;			/* self base location */ /* maybe it is not useful */

	std::map<BWTA::Region*, std::vector<EnemyBuilding>>			regions_enemy_buildings; 

	std::map<BWTA::Region*, std::vector<BWAPI::Unit>>			regions_enemy_units;

	std::map<BWTA::Region*, std::vector<BWAPI::Unit>>			regions_self_buildings;

	std::map<BWTA::Region*, std::vector<UnitProbe>>				regions_self_units;

	std::map<BWTA::BaseLocation*, std::vector<UnitProbe>>		startlocations_scouts; 

	std::map<BWTA::BaseLocation*, int>							startlocations_frames;

	std::map<BWTA::BaseLocation*, std::vector<UnitProbe>>		baselocations_scouts;

	std::map<BWTA::BaseLocation*, int>							baselocations_frames;

	std::vector<UnitProbe>										tile_scouts; 

	std::map<int, int>											largetiles_frames;			// 128 size tile

	int				map_largetile_width; 

	int				map_largetile_height; 

	int				last_frame; 

	bool			is_map_analyzed; 

	bool			is_enemy_start_location_found; 
	

	/* convert LargeTile into an int index, x+y*map_largettile_width */
	int getLargeTileIndex(const LargeTile& lt) { return int(lt.x + lt.y*map_largetile_width); }

	int getLargeTileIndex(int x, int y) { return int(x + y*map_largetile_width); }

	int getDistances(const BWAPI::Unit& u, const BWAPI::Unitset& us); 

	int getDistances(const BWAPI::Position& p, const BWAPI::Unitset& us);

};