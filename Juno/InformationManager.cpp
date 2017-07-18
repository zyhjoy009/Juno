#include "InformationManager.h"
#include <BWAPI.h>
#include <BWTA.h>
#include "Config.h"
#include "ConvertManager.h"
#include "BuildingManager.h"
#include <assert.h>
#include "UnitProbeManager.h"
#include "Juno.h"
#include "DrawManager.h"
#include "Timer.h"



/* initialize InformationManager variables */
void InformationManager::initial()
{
	is_enemy_start_location_found = false;
	last_frame = 0;
	is_map_analyzed = false;
	is_enemy_start_location_found = false;

	// large tile
	map_largetile_width = BWAPI::Broodwar->mapWidth() * 32 / Config::InformationManager::LargeTileLength + 1;
	map_largetile_height = BWAPI::Broodwar->mapHeight() * 32 / Config::InformationManager::LargeTileLength + 1;
	largetiles_frames.clear();
	for (int x = 0; x < map_largetile_width; x++)
	{
		for (int y = 0; y < map_largetile_height; y++)
			largetiles_frames[getLargeTileIndex(x, y)] = 0;
	}
}


/* after map analyze intial */
void InformationManager::mapAnalyzeInitial()
{
	self_region = BWTA::getRegion(BWAPI::Broodwar->self()->getStartLocation());
	self_base_location = BWTA::getStartLocation(BWAPI::Broodwar->self());

	// region
	regions_enemy_buildings.clear(); 
	regions_enemy_units.clear(); 
	regions_self_buildings.clear(); 
	regions_self_units.clear(); 
	for (auto& r : BWTA::getRegions())
	{
		regions_enemy_buildings[r] = std::vector<EnemyBuilding>(0); 
		regions_enemy_units[r] = std::vector<BWAPI::Unit>(0); 
		regions_self_buildings[r] = std::vector<BWAPI::Unit>(0); 
		regions_self_units[r] = std::vector<UnitProbe>(0); 
	}

	// start locations
	startlocations_scouts.clear(); 
	startlocations_frames.clear(); 
	for (auto& sl : BWTA::getStartLocations())
	{
		startlocations_scouts[sl] = std::vector<UnitProbe>(0); 
		startlocations_frames[sl] = 0; 
	}

	// base locations
	baselocations_scouts.clear();
	baselocations_frames.clear();
	for (auto& sl : BWTA::getBaseLocations())
	{
		baselocations_scouts[sl] = std::vector<UnitProbe>(0);
		baselocations_frames[sl] = 0;
	}
}


/* onFrame call: update enemy information */
void InformationManager::onFrame()
{
	timer_display_y += 10;

	if (!(BWAPI::Broodwar->getFrameCount() == 0 ||
		BWAPI::Broodwar->getFrameCount() - last_frame > Config::InformationManager::FrequencyFrame))
		return; 

	// before map analyzed, do nothing 
	if (!is_map_analyzed)
		return; 

	if (timer.getNoStopElapsedTimeInMicroSec() + Config::Timer::InformationManagerMaxMicroSec > Config::Timer::TotalMicroSec)
		return; 

	Timer tt; 
	tt.start(); 

	if (hthread_analyze)
	{
		CloseHandle(hthread_analyze); 
		hthread_analyze = nullptr; 
	}

	last_frame = BWAPI::Broodwar->getFrameCount(); 

	// enemy depot first found
	if (!is_enemy_start_location_found)
	{
		for (auto& eu : BWAPI::Broodwar->enemy()->getUnits())
		{
			if (eu->getType().isResourceDepot())
			{
				is_enemy_start_location_found = true; 
				break; 
			}
		}
	}

	// enemy unit that change region
	for (auto& region_units : regions_enemy_units)
		region_units.second.clear(); 

	for (auto& eu : BWAPI::Broodwar->enemy()->getUnits())
	{
		if (eu->getType() == BWAPI::UnitTypes::Zerg_Larva)
			continue; 
		if (eu->isMorphing() && !eu->getType().isBuilding())
			continue; 

		if (eu->getType().isBuilding())
		{
			BWTA::Region* region = getRegion(eu);
			// if already store the building 
			std::vector<EnemyBuilding>::iterator iter = getEnemyBuildingIterator(regions_enemy_buildings[region], eu);
			if (iter == regions_enemy_buildings[region].end())
				regions_enemy_buildings[region].push_back(EnemyBuilding(eu));
		}
		else
		{
			BWTA::Region* region = getRegion(eu);
			regions_enemy_units[region].push_back(eu);
		}
	}

	// enemy building destroy but we didn't see
	for (auto& region_buildings : regions_enemy_buildings)
	{
		BWTA::Region* region = region_buildings.first; 
		for (std::vector<EnemyBuilding>::iterator iter = region_buildings.second.begin();
			iter != region_buildings.second.end();
			)
		{
			if (BWAPI::Broodwar->isVisible(iter->location))
			{
				BWAPI::Unit eb = BWAPI::Broodwar->getUnit(iter->id);
				if (!eb || !eb->exists())
				{
					iter = region_buildings.second.erase(iter);
				}
				else
					iter++;
			}
			else
				iter++; 
		}
	}

	// start locations
	for (auto& sl_fr : startlocations_frames)
	{
		if (BWAPI::Broodwar->isVisible(sl_fr.first->getTilePosition()))
			sl_fr.second = BWAPI::Broodwar->getFrameCount(); 
	}

	// base locations
	for (auto& bl_fr : baselocations_frames)
	{
		if (BWAPI::Broodwar->isVisible(bl_fr.first->getTilePosition()))
			bl_fr.second = BWAPI::Broodwar->getFrameCount(); 
	}

	// large tiles
	for (int x = 0; x < map_largetile_width; x++)
	{
		for (int y = 0; y < map_largetile_height; y++)
		{
			if (BWAPI::Broodwar->isVisible(BWAPI::TilePosition(LargeTile(x,y))))
				largetiles_frames[getLargeTileIndex(x,y)] = BWAPI::Broodwar->getFrameCount();
		}
	}

	tt.stop(); 
	DrawManager::getInstance().drawTime(10, timer_display_y, Config::InformationManager::FrequencyFrame,
		"InformationManager", tt.getElapsedTimeInMicroSec()); 
}


/* onUnitShow call: add enemy information 
 shortly called after onUnitCreate, so enemy information is added here */
void InformationManager::onUnitShow(BWAPI::Unit u)
{
	// it is not correct, since enemy can move, 
	// when enemy is always visible but move to another region, onUnitShow can't do that

	return; 

/*	if (u->getPlayer() != BWAPI::Broodwar->enemy())
	{
		return;
	}

	if (u->getType().isBuilding())
	{
		BWTA::Region* region = getRegion(u); 
		// if already store the building 
		std::vector<EnemyBuilding>::iterator iter = getEnemyBuildingIterator(regions_enemy_buildings[region], u); 
		if (iter == regions_enemy_buildings[region].end())
			regions_enemy_buildings[region].push_back(EnemyBuilding(u)); 
	}
	else
	{
		BWTA::Region* region = getRegion(u);
		// if already store the unit, something wrong
		std::vector<BWAPI::Unit>::iterator iter = std::find(regions_enemy_units[region].begin(),
			regions_enemy_units[region].end(),
			u);
		if (iter == regions_enemy_units[region].end())
			regions_enemy_units[region].push_back(u);
		else
			assert(false); 
	} */
}

/* onUnitHide call: remove enemy unit 
 shortly called before onUnitDestroy, so enemy unit is removed here */
void InformationManager::onUnitHide(BWAPI::Unit u)
{
	// same reason as onUnitShow

	return; 

/*	if (u->getPlayer() != BWAPI::Broodwar->enemy() ||
		u->getType().isBuilding())
		return; 

	BWTA::Region* region = getRegion(u);
	// if the unit is stored
	std::vector<BWAPI::Unit>::iterator iter = std::find(regions_enemy_units[region].begin(),
		regions_enemy_units[region].end(),
		u);
	if (iter != regions_enemy_units[region].end())
		regions_enemy_units[region].erase(iter);
	else
		assert(false); */
}

/* onUnitCreate call: add self unit 
  shortly called before onUnitShow, so self information is added here */
void InformationManager::onUnitCreate(BWAPI::Unit u)
{
	// only consider self unit and building
	if (u->getPlayer() != BWAPI::Broodwar->self())
		return; 

	// before map analyzed, do nothing
	if (!is_map_analyzed)
		return; 

	// building
	if (u->getType().isBuilding())
	{
		BWTA::Region* region = getRegion(u);
		regions_self_buildings[region].push_back(u); 
	}
	// unit, we need to find UnitProbe from UnitProbeManager
	else
	{
		UnitProbe up = UnitProbeManager::getInstance().getUnit(u); 
		onUnitCreate(up); 
	}
}

void InformationManager::onUnitCreate(UnitProbe up)
{
	// before map analyzed, do nothing
	if (!is_map_analyzed)
		return;

	assert(up);
	BWTA::Region* region = up->getTargetRegion();
	// has region mean ATTACK, DEFEND, MINERAL
	if (region)
		regions_self_units[region].push_back(up);
	// otherwise SCOUTs
	else if (up->getRole() == Role::SCOUT_START_LOCATION)
	{
		BWTA::BaseLocation* sl = BWTA::getNearestBaseLocation(up->getTargetPosition()); 
		assert(sl); 
		startlocations_scouts[sl].push_back(up);
	}
	else if (up->getRole() == Role::SCOUT_BASE_LOCATION)
	{
		BWTA::BaseLocation* bl = BWTA::getNearestBaseLocation(up->getTargetPosition());
		assert(bl);
		baselocations_scouts[bl].push_back(up);
	}
	else if (up->getRole() == Role::SCOUT)
	{
		tile_scouts.push_back(up); 
	}
	else
		assert(false);
}


/* onUnitDestroy call: remove self unit 
  shortly called after onUnitHide, 
  so self information and enemy building is removed here */
void InformationManager::onUnitDestroy(BWAPI::Unit u)
{
	// self 
	if (u->getPlayer() == BWAPI::Broodwar->self())
	{
		if (u->getType().isBuilding())
		{
			BWTA::Region* region = getRegion(u);
			std::vector<BWAPI::Unit>::iterator iter = std::find(regions_self_buildings[region].begin(),
				regions_self_buildings[region].end(),
				u);
			assert(iter != regions_self_buildings[region].end());
			regions_self_buildings[region].erase(iter);
		}
		else
		{
			UnitProbe up = UnitProbeManager::getInstance().getUnit(u);
			assert(up);
			BWTA::Region* region = up->getTargetRegion();
			if (region)
			{
				std::vector<UnitProbe>::iterator iter = getSelfUnitIterator(regions_self_units[region], u); 
				assert(iter != regions_self_units[region].end());
				regions_self_units[region].erase(iter);
			}
			else if (up->getRole() == Role::SCOUT_START_LOCATION)
			{
				BWTA::BaseLocation* sl = BWTA::getNearestBaseLocation(up->getTargetPosition());
				assert(sl);
				std::vector<UnitProbe>::iterator iter = getSelfUnitIterator(startlocations_scouts[sl], u);
				assert(iter != startlocations_scouts[sl].end());
				startlocations_scouts[sl].erase(iter);
			}
			else if (up->getRole() == Role::SCOUT_BASE_LOCATION)
			{
				BWTA::BaseLocation* bl = BWTA::getNearestBaseLocation(up->getTargetPosition());
				assert(bl);
				std::vector<UnitProbe>::iterator iter = getSelfUnitIterator(baselocations_scouts[bl], u);
				assert(iter != baselocations_scouts[bl].end());
				baselocations_scouts[bl].erase(iter);
			}
			else if (up->getRole() == Role::SCOUT)
			{
				std::vector<UnitProbe>::iterator iter = std::find(tile_scouts.begin(),
					tile_scouts.end(),
					up); 
				assert(iter != tile_scouts.end()); 
				tile_scouts.erase(iter); 
			}
			else
				assert(false);
		}
	}
	// enemy building
	else if (u->getPlayer() == BWAPI::Broodwar->enemy() &&
		u->getType().isBuilding())
	{
		BWTA::Region* region = getRegion(u);
		std::vector<EnemyBuilding>::iterator iter = getEnemyBuildingIterator(regions_enemy_buildings[region], u); 
		assert(iter != regions_enemy_buildings[region].end());
		regions_enemy_buildings[region].erase(iter);
	}
}


/* get unit or position BWTA::Region* */
BWTA::Region* InformationManager::getRegion(BWAPI::Unit u)
{
	return getRegion(u->getPosition()); 
}

BWTA::Region* InformationManager::getRegion(BWAPI::Position p)
{
	BWTA::Chokepoint* cp = BWTA::getNearestChokepoint(p); 
	// unwalkable position return nullptr 
	if (cp == nullptr)
		return nullptr; 
	if (p.getDistance(cp->getCenter()) < cp->getSides().first.getDistance(cp->getSides().second) / 2 + cp->getWidth() / 2)
		return cp->getRegions().first;
	else
		return BWTA::getRegion(p); 
}


/* return self units of prescribed role at certain region */
std::vector<UnitProbe> InformationManager::getRegionSelfUnits(BWTA::Region* region, const Role& role)
{
	assert(region); 

	// return all units
	if (role == Role::ROLE_NUM)
		return regions_self_units[region]; 

	// return certain role
	std::vector<UnitProbe> units(0); 
	for (auto& u : regions_self_units[region])
	{
		// mineral may be still trained, don't count
		if (u->getRole() == role && u->getUnit()->isCompleted())
			units.push_back(u); 
	}
	return units; 
}


/* return certain type of self building */
std::vector<BWAPI::Unit> InformationManager::getRegionSelfBuildings(BWTA::Region* region, const BWAPI::UnitType& type)
{
	// return all
	if (type == BWAPI::UnitTypes::None)
		return regions_self_buildings[region]; 

	// return certain type
	std::vector<BWAPI::Unit> units(0); 
	for (auto& u : regions_self_buildings[region])
	{
		if (u->getType() == type)
			units.push_back(u); 
	}
	return units; 
}


/* return enemy units at certain region */
std::vector<BWAPI::Unit> InformationManager::getRegionEnemyUnits(BWTA::Region* region)
{
	return regions_enemy_units[region];
}

/* return enemy buildings at certain region */
std::vector<EnemyBuilding> InformationManager::getRegionEnemyBuildings(BWTA::Region* region)
{
	return regions_enemy_buildings[region]; 
}


/* add self unit (ATTACK/DEFEND/MINERAL) at a certain region */
void InformationManager::addRegionSelfUnit(const UnitProbe& u)
{
	BWTA::Region* region = u->getTargetRegion(); 
	assert(region); 

	regions_self_units[region].push_back(u); 
}


/* remove self unit from a certain region */
void InformationManager::removeRegionSelfUnit(const UnitProbe& unit)
{
	BWTA::Region* region = unit->getTargetRegion(); 
	assert(region); 

	std::vector<UnitProbe>::iterator iter = std::find(regions_self_units[region].begin(),
		regions_self_units[region].end(),
		unit); 
	if (iter != regions_self_units[region].end())
		regions_self_units[region].erase(iter);
	else
		assert(false); 
}


/* calculate given region enemy weapon sum, including units and buildings */
double InformationManager::getRegionEnemyWeaponSum(BWTA::Region* r)
{
	double sum = 0; 

	for (auto& eu : regions_enemy_buildings[r])
	{
		if (eu.unit_type.canAttack())
		{
			const BWAPI::WeaponType& wt = eu.unit_type.groundWeapon(); 
			sum += double(wt.damageAmount()) * wt.damageFactor() / wt.damageCooldown(); 
		}
	}

	for (auto& eu : regions_enemy_units[r])
	{
		if (eu->getType().canAttack())
		{
			const BWAPI::WeaponType& wt = eu->getType().groundWeapon();
			sum += double(wt.damageAmount()) * wt.damageFactor() / wt.damageCooldown();
		}
	}
	return sum; 
}

/* calculate given region self weapon sum, only unit is included 
   I want our units are powerful than enemy units+buildings */
double InformationManager::getRegionSelfWeaponSum(BWTA::Region* r)
{
	double sum = 0;

	for (auto& u : regions_self_units[r])
	{
		if (u->getUnit()->getType().canAttack())
		{
			const BWAPI::WeaponType& wt = u->getUnit()->getType().groundWeapon();
			sum += double(wt.damageAmount()) * wt.damageFactor() / wt.damageCooldown();
		}
	}
	return sum;
}


/* return current mineral minus planned buildings */
int InformationManager::getMineral()
{
	int mineral = BWAPI::Broodwar->self()->minerals(); 
	for (auto& pld : BuildingManager::getInstance().getPlanningUnits())
		mineral -= pld->building_type.mineralPrice(); 
	return mineral; 
}

/* get most cannon desired region */
BWTA::Region* InformationManager::getCannonDesiredRegion()
{
	BuildingManager& BM = BuildingManager::getInstance(); 

	BWTA::Region* region_desired = nullptr; 
	int num_desired = 0; 

	for (auto& r : BWTA::getRegions())
	{
		int num = 0; 
		num += getRegionEnemyBuildings(r).size() - getRegionSelfBuildings(r, BWAPI::UnitTypes::Protoss_Photon_Cannon).size(); 
		num += getRegionEnemyUnits(r).size() / Config::InformationManager::CannonEquivalentUnits; 
		if (num > num_desired)
		{
			num_desired = num; 
			region_desired = r; 
		}
	}

	if (region_desired == nullptr)
	{
		num_desired = 0; 
		for (auto& r : BWTA::getRegions())
		{
			int num = getRegionEnemyBuildings(r).size(); 
			if (num > num_desired)
			{
				num_desired = num; 
				region_desired = r; 
			}
		}
	}

	if (region_desired == nullptr)
		region_desired = self_region; 

	return region_desired; 
}


/* return region desired target that needs a new cannon to attack 
   first find canproduce enemy building that has no cannon surrounded 
   then find noproduce enemy building that has no cannon surrounded 
   last if all enemy building has cannon, find closest to chokepoint building 
   alway try to select closest to chokepoint enemy building 
   if no enemybuilding, build near chokepoint */
/* can produce building > produce building > base location 
   in same priority, distance to chokepoint determine */
BWAPI::TilePosition InformationManager::getRegionCannonTarget(BWTA::Region* r)
{
	BWAPI::Position produce_pos(-1, -1), no_produce_pos(-1, -1), any_pos(-1, -1); 
	int dist_produce_best = 99999, dist_no_produce_best = 99999, dist_any_best = 99999; 

	// region closest chokepoint
	BWTA::Chokepoint* closest_cp; 

	// isolated region may not have any chokepoint 
	if (r->getChokepoints().empty())
		return BWAPI::TilePosition(-1, -1); 

	if (r->getChokepoints().size() == 1)
		closest_cp = *r->getChokepoints().begin();
	else
		closest_cp = BWTA::getNearestChokepoint(r->getCenter()); 

	for (auto& b : regions_enemy_buildings[r])
	{
		bool exist_nearby_cannon = !BWAPI::Broodwar->getUnitsInRadius(BWAPI::Position(b.location),
			0.9*BWAPI::UnitTypes::Protoss_Photon_Cannon.groundWeapon().maxRange(),
			BWAPI::Filter::IsOwned && BWAPI::Filter::IsBuilding && BWAPI::Filter::CanAttack).empty(); 

		// canproduce enemy building and has no cannon 
		if (b.unit_type.canProduce() && !exist_nearby_cannon)
		{
			int d = BWAPI::Position(b.location).getDistance(closest_cp->getCenter());
			if (d < dist_produce_best)
			{
				dist_produce_best = d;
				produce_pos = BWAPI::Position(b.location); 
			}
		}
		
		// noproduce enemybuilding and has no cannon
		else if (!produce_pos.isValid() && !exist_nearby_cannon)
		{
			int d = BWAPI::Position(b.location).getDistance(closest_cp->getCenter());
			if (d < dist_no_produce_best)
			{
				dist_no_produce_best = d;
				no_produce_pos = BWAPI::Position(b.location);
			}
		}
		
		// any building
		else if (!produce_pos.isValid() && !no_produce_pos.isValid())
		{
			int d = BWAPI::Position(b.location).getDistance(closest_cp->getCenter());
			if (d < dist_any_best)
			{
				dist_any_best = d;
				any_pos = BWAPI::Position(b.location);
			}
		}
	}

	if (produce_pos.isValid())
		return BWAPI::TilePosition(produce_pos);
	else if (no_produce_pos.isValid())
		return BWAPI::TilePosition(no_produce_pos);
	else if (any_pos.isValid())
		return BWAPI::TilePosition(any_pos); 

	// no enemy building, build at base location 
	int num = 99999; 
	BWTA::BaseLocation* base_location = nullptr; 
	for (auto&bl : r->getBaseLocations())
	{
		int n = BWAPI::Broodwar->getUnitsInRadius(bl->getPosition(),
			BWAPI::UnitTypes::Protoss_Photon_Cannon.groundWeapon().maxRange(),
			BWAPI::Filter::IsOwned && BWAPI::Filter::IsBuilding && BWAPI::Filter::CanAttack).size(); 
		if (n < num)
		{
			num = n; 
			base_location = bl;
		}
	}

	if (base_location)
		return base_location->getTilePosition(); 
	else
		return BWAPI::TilePosition(r->getCenter()); 
}


/* return region highest attack target 
working unit > not completed building > isolated enemy > can produce building > closest enemy*/
void InformationManager::getRegionAttackTarget(BWTA::Region* r, BWAPI::Unit& target_unit, BWAPI::Position& target_position)
{
	InformationManager& IM = InformationManager::getInstance(); 

	// self region don't use any target, just attack closest enemy 
	assert(r != IM.getSelfRegion()); 

	static std::map<BWTA::Region*, std::pair<int, BWAPI::Unit>> regions_frame_unit; 
	static std::map<BWTA::Region*, std::pair<int, BWAPI::Position>> regions_frame_position;

	if (regions_frame_unit.find(r) != regions_frame_unit.end() &&
		BWAPI::Broodwar->getFrameCount() - regions_frame_unit[r].first < Config::InformationManager::FrequencyFrame)
	{
		target_unit = regions_frame_unit[r].second;
		target_position = regions_frame_position[r].second;
		return; 
	}

	// all self attack units
	BWAPI::Unitset self_units; 
	for (auto& u : IM.getRegionSelfUnits(r, ATTACK))
		self_units.insert(u->getUnit()); 

	BWAPI::Unit working_unit = nullptr; 
	int working_distance = 99999; 

	BWAPI::Unit mining_unit = nullptr; 
	int mining_distance = 99999; 
	
	BWAPI::Unit uncomplete_building = nullptr; 
	int uncomplete_distance = 99999; 

	BWAPI::Unit isolated_unit = nullptr; 
	int isolated_distance = 99999; 

	// all enemy visible units
	std::vector<BWAPI::Unit> enemies = IM.getRegionEnemyUnits(r); 
	for (auto& eb : IM.getRegionEnemyBuildings(r))
	{
		BWAPI::Unit u = BWAPI::Broodwar->getUnit(eb.id); 
		if (u && u->exists())
			enemies.push_back(u); 
	}

	for (auto& eu : enemies)
	{
		if (!eu || !eu->exists())
		{
			continue; 
		}

		// enemy is terrain and is working
		if ((BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Terran &&
			!eu->getType().isBuilding() &&
			(eu->isConstructing() || eu->isRepairing())))
		{
			int distance = IM.getDistances(eu, self_units); 
			if (distance < working_distance)
			{
				working_unit = eu; 
				working_distance = distance; 
			}
		}
		else if (eu->isGatheringGas() || eu->isGatheringMinerals())
		{
			int distance = IM.getDistances(eu, self_units);
			if (distance < mining_distance)
			{
				mining_unit = eu;
				mining_distance = distance;
			}
		}
		// enemy unfinished building
/*		else if (working_unit == nullptr &&
			((eu->getType().isBuilding() && !eu->isCompleted()) || 
			(eu->isBeingConstructed()) ||
			(eu->getBuildType().isBuilding())))
		{
			int distance = IM.getDistances(eu, self_units);
			if (distance < uncomplete_distance)
			{
				uncomplete_building = eu;
				uncomplete_distance = distance;
			}
		} */
		else if (working_unit == nullptr &&
			uncomplete_building == nullptr && 
			!eu->getType().isBuilding() && 
			!eu->isFlying())
		{
			// isolated enemy unit 
			if (eu->getType() != BWAPI::UnitTypes::Zerg_Larva &&
				eu->getClosestUnit(BWAPI::Filter::IsEnemy && BWAPI::Filter::CanAttack && 
				!BWAPI::Filter::IsRepairing && !BWAPI::Filter::IsConstructing && 
				!BWAPI::Filter::IsGatheringGas && !BWAPI::Filter::IsGatheringMinerals, 
				0.5*eu->getType().sightRange()) == nullptr)
			{
				int distance = IM.getDistances(eu, self_units);
				if (distance < isolated_distance)
				{
					isolated_unit = eu;
					isolated_distance = distance;
				}
			}
		}
	}
	if (working_unit)
	{
		target_unit = working_unit;
		target_position = BWAPI::Position(-1, -1); 
	}
	else if (mining_unit)
	{
		target_unit = mining_unit; 
		target_position = BWAPI::Position(-1, -1); 
	}
	else if (uncomplete_building)
	{
		target_unit = uncomplete_building; 
		target_position = BWAPI::Position(-1, -1); 
	}
	else if (isolated_unit)
	{
		target_unit = isolated_unit;
		target_position = BWAPI::Position(-1, -1);
	}
	else
	{
		EnemyBuilding produce_building;
		int produce_distance = 99999;

		EnemyBuilding closest_building;
		int closest_distance = 99999;

		for (auto& eb : IM.getRegionEnemyBuildings(r))
		{
			if (eb.unit_type.canProduce())
			{
				int distance = IM.getDistances(BWAPI::Position(eb.location), self_units);
				if (distance < produce_distance)
				{
					produce_building = eb;
					produce_distance = distance;
				}
			}
			else if (produce_distance >= 99999)
			{
				int distance = IM.getDistances(BWAPI::Position(eb.location), self_units);
				if (distance < closest_distance)
				{
					closest_building = eb;
					closest_distance = distance;
				}
			}
		}

		// hiding enemy building
		if (produce_distance < 99999)
		{
			BWAPI::Unit u = BWAPI::Broodwar->getUnit(produce_building.id);
			if (u && u->exists())
			{
				target_unit = u;
				target_position = BWAPI::Position(-1, -1);
			}
			else
			{
				target_unit = nullptr;
				target_position = BWAPI::Position(produce_building.location);
			}
		}
		else if (closest_distance < 99999)
		{
			BWAPI::Unit u = BWAPI::Broodwar->getUnit(closest_building.id);
			if (u && u->exists())
			{
				target_unit = u;
				target_position = BWAPI::Position(-1, -1);
			}
			else
			{
				target_unit = nullptr;
				target_position = BWAPI::Position(closest_building.location);
			}
		}
		// just attack visible unit 
		else if (!enemies.empty())
		{
			target_unit = enemies[0]; 
			target_position = BWAPI::Position(-1, -1);
		}
		else
		{
			target_unit = nullptr;
			target_position = r->getCenter();
		}
	}

	regions_frame_unit[r] = std::pair<int, BWAPI::Unit>(BWAPI::Broodwar->getFrameCount(), target_unit);
	regions_frame_position[r] = std::pair<int, BWAPI::Position>(BWAPI::Broodwar->getFrameCount(), target_position);

	DrawManager::getInstance().drawAttackTarget(target_unit, target_position); 
}


int InformationManager::getDistances(const BWAPI::Unit& u, const BWAPI::Unitset& us)
{
	int d = 0; 
	for (auto& unit : us)
		d += u->getDistance(unit); 
	return d; 
}

int InformationManager::getDistances(const BWAPI::Position& p, const BWAPI::Unitset& us)
{
	int d = 0; 
	for (auto& unit : us)
		d += unit->getDistance(p);
	return d;
}

/* get scout units */
std::vector<UnitProbe> InformationManager::getStartLocationsScouts()
{
	std::vector<UnitProbe> ss(0); 
	for (auto sls : startlocations_scouts)
		ss.insert(ss.end(), sls.second.begin(), sls.second.end()); 
	return ss; 
}

std::vector<UnitProbe> InformationManager::getBaseLocationsScouts()
{
	std::vector<UnitProbe> ss(0);
	for (auto bls : baselocations_scouts)
		ss.insert(ss.end(), bls.second.begin(), bls.second.end());
	return ss;
}

std::vector<UnitProbe> InformationManager::getTileScouts()
{
	return tile_scouts;
}

/* remove scout unit */
void InformationManager::removeStartLocationScout(const UnitProbe& up)
{
	assert(up->getRole() == Role::SCOUT_START_LOCATION); 
	
	BWTA::BaseLocation* sl = BWTA::getNearestBaseLocation(up->getTargetPosition()); 
	assert(sl); 
	
	assert(startlocations_scouts.find(sl) != startlocations_scouts.end()); 

	std::vector<UnitProbe>::iterator iter = std::find(startlocations_scouts[sl].begin(),
		startlocations_scouts[sl].end(),
		up); 
	assert(iter != startlocations_scouts[sl].end()); 

	startlocations_scouts[sl].erase(iter); 
}

void InformationManager::removeBaseLocationScout(const UnitProbe& up)
{
	assert(up->getRole() == Role::SCOUT_BASE_LOCATION);

	BWTA::BaseLocation* bl = BWTA::getNearestBaseLocation(up->getTargetPosition()); 
	assert(bl);

	assert(baselocations_scouts.find(bl) != baselocations_scouts.end()); 

	std::vector<UnitProbe>::iterator iter = std::find(baselocations_scouts[bl].begin(),
		baselocations_scouts[bl].end(),
		up);
	assert(iter != baselocations_scouts[bl].end());

	baselocations_scouts[bl].erase(iter);
}

void InformationManager::removeTileScout(const UnitProbe& up)
{
	assert(up->getRole() == Role::SCOUT);

	std::vector<UnitProbe>::iterator iter = std::find(tile_scouts.begin(),
		tile_scouts.end(),
		up);
	assert(iter != tile_scouts.end());
	tile_scouts.erase(iter);
}


std::vector<EnemyBuilding>::iterator InformationManager::getEnemyBuildingIterator(std::vector<EnemyBuilding>& enemy_buildings, 
	const BWAPI::Unit& u)
{
	std::vector<EnemyBuilding>::iterator iter; 
	for (iter = enemy_buildings.begin(); iter != enemy_buildings.end(); iter++)
	{
		if ((*iter).id == u->getID())
			break; 
	}
	return iter; 
}

std::vector<UnitProbe>::iterator InformationManager::getSelfUnitIterator(std::vector<UnitProbe>& self_units,
	const BWAPI::Unit& u)
{
	std::vector<UnitProbe>::iterator iter; 
	for (iter = self_units.begin(); iter != self_units.end(); iter++)
	{
		if ((*iter)->getUnit() == u)
			break;
	}
	return iter; 
}

/* get most desired scout position from give position  */
std::pair<Role, BWAPI::Position> InformationManager::getDesireScoutTarget(const BWAPI::Position& p)
{
	// enemy start location hasn't been found
	if (!is_enemy_start_location_found)
	{
		// start location that has not been visited and has no scouts
		int distance = 99999;
		BWTA::BaseLocation* start_location = nullptr;
		for (auto& sl : BWTA::getStartLocations())
		{
			if (sl == self_base_location)
				continue;

			if (startlocations_frames[sl] == 0 &&
				startlocations_scouts[sl].empty())
			{
				int d = p.getDistance(sl->getPosition());
				if (d < distance)
				{
					distance = d;
					start_location = sl;
				}
			}
		}
		if (start_location)
		{
			return std::pair<Role, BWAPI::Position>(Role::SCOUT_START_LOCATION, start_location->getPosition());
		}

		// all start location have visit or have scouts, send to start location that has least scouts
		int num = 99999;
		distance = 99999;
		start_location = nullptr;
		for (auto& sl : BWTA::getStartLocations())
		{
			if (sl == self_base_location)
				continue;

			if (startlocations_frames[sl] == 0)
			{
				if (startlocations_scouts[sl].size() < num)
				{
					num = startlocations_scouts[sl].size();
					distance = p.getDistance(sl->getPosition());
					start_location = sl;
				}
				else if (startlocations_scouts[sl].size() == num)
				{
					int d = p.getDistance(sl->getPosition());
					if (d < distance)
					{
						distance = d;
						start_location = sl;
					}
				}
			}
		}
		if (start_location)
		{
			return std::pair<Role, BWAPI::Position>(Role::SCOUT_START_LOCATION, start_location->getPosition());
		}
	}

	// base location that has not been recently visited and has no scouts
	int frame = 99999; 
	BWTA::BaseLocation* base_location = nullptr; 
	for (auto& bl : BWTA::getBaseLocations())
	{
		if (BWAPI::Broodwar->getFrameCount() - baselocations_frames[bl] > Config::InformationManager::BaseLocationScoutFrame &&
			baselocations_scouts[bl].empty() &&
			baselocations_frames[bl] < frame)
		{
			frame = baselocations_frames[bl]; 
			base_location = bl; 
		}
	}
	if (base_location)
	{
		return std::pair<Role, BWAPI::Position>(Role::SCOUT_BASE_LOCATION, base_location->getPosition()); 
	}

	// around large tile that is last visited 
	int last_visited_frame = 0;
	LargeTile lt_best(-1, -1);
	int length = 1 + 2 * Config::InformationManager::ScoutLargeTileRange;
	LargeTile lt(p);
	lt.x -= length / 2; 
	lt.y -= length / 2; 

	for (int i = 0; i < 4; i++)
	{
		int dx, dy;
		switch (i)
		{
			/* left bottom -> right bottom */
		case 0:
			dx = 1; dy = 0;
			break;
			/* right bottom -> right top */
		case 1:
			dx = 0; dy = 1;
			break;
			/* right top -> left top */
		case 2:
			dx = -1; dy = 0;
			break;
			/* left top -> left bottom */
		case 3:
			dx = 0; dy = -1;
			break;
		}
		for (int j = 0; j < length; j++)
		{
			lt = lt + LargeTile(dx, dy);
			BWAPI::TilePosition tp = BWAPI::TilePosition(lt); 
			BWAPI::WalkPosition wp = BWAPI::WalkPosition(lt); 
			if (BWAPI::Position(tp).isValid() && BWAPI::Broodwar->isWalkable(wp) &&
				largetiles_frames[getLargeTileIndex(lt)] < last_visited_frame)
			{
				last_visited_frame = largetiles_frames[getLargeTileIndex(lt)];
				lt_best = lt;
			}
		}
	}
	if (BWAPI::Position(lt_best).isValid())
	{
		return std::pair<Role, BWAPI::Position>(Role::SCOUT, BWAPI::Position(lt_best));
	}
	// still same position
	else
	{
		return std::pair < Role, BWAPI::Position>(Role::SCOUT, p);
	}
}

/* add scout unit */
void InformationManager::addStartLocationScout(const UnitProbe& up)
{
	assert(up->getRole() == SCOUT_START_LOCATION); 

	BWTA::BaseLocation* sl = BWTA::getNearestBaseLocation(up->getTargetPosition()); 

	if (startlocations_scouts.find(sl) == startlocations_scouts.end())
		return; 

	startlocations_scouts[sl].push_back(up); 
}

void InformationManager::addBaseLocationScout(const UnitProbe& up)
{
	assert(up->getRole() == SCOUT_BASE_LOCATION);

	BWTA::BaseLocation* bl = BWTA::getNearestBaseLocation(up->getTargetPosition());

	if (baselocations_scouts.find(bl) == baselocations_scouts.end())
		return;

	baselocations_scouts[bl].push_back(up);
}

void InformationManager::addTileScout(const UnitProbe& up)
{
	assert(up->getRole() == SCOUT); 

	tile_scouts.push_back(up); 
}