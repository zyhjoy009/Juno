#include "BuildingManager.h"
#include "Config.h"
#include "InformationManager.h"
#include "UnitProbeManager.h"
#include "ConvertManager.h"
#include <assert.h>
#include "Juno.h"
#include "DrawManager.h"


/* recover builder previous data */
void PlanBuildData::builderRecover()
{
	if (builder == nullptr)
		return; 

	builder->setRole(builder_role);
	builder->setTargetRegion(builder_target_region);
	builder->setTargetBaseLocation(builder_target_base_location);
	builder->setTargetUnit(builder_target_unit);
	builder->setTargetPosition(builder_target_position);
	builder->setPlanBuildData(nullptr); 
}

/* onFrame call: initialize variables */
void BuildingManager::onFrame()
{
	timer_display_y += 10;

	if (!(BWAPI::Broodwar->getFrameCount() == 0 ||
		BWAPI::Broodwar->getFrameCount() - last_frame > Config::BuildingManager::FrequencyFrame))
	{
		return;
	}

	if (timer.getNoStopElapsedTimeInMicroSec() + Config::Timer::BuildingManagerMaxMicroSec > Config::Timer::TotalMicroSec)
		return;
	Timer tt;
	tt.start(); 

	last_frame = BWAPI::Broodwar->getFrameCount(); 

	// before map analyzed, let depot train Probe
	if (!InformationManager::getInstance().isMapAnalyzed())
	{
		for (auto& u : map_unanalyze_buildings)
		{
			if (u->getType().isResourceDepot() &&
				u->isIdle() &&
				BWAPI::Broodwar->self()->minerals() >= BWAPI::UnitTypes::Protoss_Probe.mineralPrice())
			{
				u->train(BWAPI::UnitTypes::Protoss_Probe); 
			}
		}
		return; 
	}

	// here is what we do after map analyzed
	/* constructing building completed */
	for (std::vector<BWAPI::Unit>::iterator iter = constructings.begin();
		iter != constructings.end(); )
	{
//		int x = (*iter)->getTilePosition().x, y = (*iter)->getTilePosition().y; 
//		BWAPI::Broodwar->sendText("constructing at (%d, %d)", x, y); 
		if ((*iter)->isCompleted())
		{
			buildings.push_back(*iter);
			iter = constructings.erase(iter);
		}
		// enemy is attacking constructing building and hp is low
		else
		{
			if ((*iter)->isUnderAttack())
			{
				int hp = (*iter)->getHitPoints() + (*iter)->getShields();
				int damage = 0;
				for (auto& eu : (*iter)->getUnitsInRadius(Config::BuildingManager::ConstructingEnemyRadius,
					BWAPI::Filter::IsEnemy && BWAPI::Filter::CanAttack))
				{
					if (eu->getTarget() == *iter || eu->getOrderTarget() == *iter)
						damage += eu->getType().groundWeapon().damageAmount() * eu->getType().groundWeapon().damageFactor();
				}
				if (hp < damage*2)
				{
					(*iter)->cancelConstruction();
				}
			}
			iter++;
		}
	}

	/* planned building waiting too long or builder destroy */
	for (std::vector<PlanBuildData*>::iterator iter = plannings.begin();
		iter != plannings.end(); )
	{
		DrawManager::getInstance().drawPlanBuilding((*iter)->location, (*iter)->building_type);
		// wait too long, delete plan
		if (BWAPI::Broodwar->getFrameCount() - (*iter)->command_frame > Config::BuildingManager::PlanBuildingWaitFrame)
		{
			(*iter)->builderRecover();
			delete (*iter); 
			iter = plannings.erase(iter);
			continue; 
		}

		if (!canBuildHere((*iter)->location, (*iter)->building_type))
		{
			(*iter)->location = getNearBuildLocation((*iter)->location, (*iter)->building_type, 32 * 8);
			if (!(*iter)->location.isValid())
			{
				(*iter)->builderRecover();
				delete (*iter);
				iter = plannings.erase(iter);
				continue;
			}
		}

		// builder destroy, find new one 
		if (!(*iter)->builder || !(*iter)->builder->getUnit()->exists())
		{
			UnitProbe u = getBuilder(BWAPI::Position((*iter)->location)); 
			
			if (u)
			{
				(*iter)->builder = u;
				(*iter)->builder_role = u->getRole();
				(*iter)->builder_target_region = u->getTargetRegion();
				(*iter)->builder_target_base_location = u->getTargetBaseLocation();
				(*iter)->builder_target_unit = u->getTargetUnit();
				(*iter)->builder_target_position = u->getTargetPosition();
				(*iter)->command_frame = BWAPI::Broodwar->getFrameCount();

				u->setRole(Role::BUILD);
				u->setPlanBuildData(*iter);
			}
			iter++; 
			continue; 
		}

		UnitProbe u = getBuilder(BWAPI::Position((*iter)->location));
		if (u && u != (*iter)->builder)
		{
			if ((*iter)->builder->getUnit()->getDistance(BWAPI::Position((*iter)->location))
				> u->getUnit()->getDistance(BWAPI::Position((*iter)->location)) + 4 * 32)
			{
				(*iter)->builderRecover();

				(*iter)->builder = u;
				(*iter)->builder_role = u->getRole();
				(*iter)->builder_target_region = u->getTargetRegion();
				(*iter)->builder_target_base_location = u->getTargetBaseLocation();
				(*iter)->builder_target_unit = u->getTargetUnit();
				(*iter)->builder_target_position = u->getTargetPosition();
				(*iter)->command_frame = BWAPI::Broodwar->getFrameCount();

				u->setRole(Role::BUILD);
				u->setPlanBuildData(*iter);

				iter++;
				continue;
			}

		}
		// finish building, delete 
		bool is_built = false; 
		for (auto& b : BWAPI::Broodwar->getUnitsOnTile((*iter)->location, BWAPI::Filter::IsBuilding && BWAPI::Filter::IsOwned))
		{
			if (b->getType() == (*iter)->building_type)
			{
				is_built = true; 
				(*iter)->builderRecover();
				delete (*iter);
				iter = plannings.erase(iter);
				break; 
			}
		}
		if (!is_built)
			iter++;
	}

	tt.stop();
	DrawManager::getInstance().drawTime(10, timer_display_y, Config::BuildingManager::FrequencyFrame,
		"BuildingManager", tt.getElapsedTimeInMicroSec());

}


/* onUnitCreate call: new self building is constructed */
void BuildingManager::onUnitCreate(BWAPI::Unit b)
{
	if (b->getPlayer() != BWAPI::Broodwar->self() || 
		!b->getType().isBuilding())
		return; 

	// before map analyze
	if (!InformationManager::getInstance().isMapAnalyzed())
	{
		map_unanalyze_buildings.push_back(b); 
		return; 
	}

	// here is what we do after map analyze
	// if already built, mean depot  which we haven't stored 
	if (b->isCompleted())
		buildings.push_back(b); 
	else
	{
		for (std::vector<PlanBuildData*>::iterator iter = plannings.begin();
			iter != plannings.end();)
		{
			if ((*iter)->building_type == b->getType() && (*iter)->location == b->getTilePosition())
			{
				(*iter)->builderRecover();
				delete (*iter);
				iter = plannings.erase(iter);
				break;
			}
			else
				iter++;
		}
		constructings.push_back(b);
	}
}


/* onUnitDestroy call: self building is destroyed */
void BuildingManager::onUnitDestroy(BWAPI::Unit b)
{
	if (b->getPlayer() != BWAPI::Broodwar->self() ||
		!b->getType().isBuilding())
		return;

	std::vector<BWAPI::Unit>::iterator iter = find(buildings.begin(),
		buildings.end(),
		b); 
	if (iter != buildings.end())
		buildings.erase(iter); 
	else
	{
		iter = find(constructings.begin(),
			constructings.end(),
			b);
		if (iter != constructings.end())
			constructings.erase(iter);
		else
			assert(false); 
	}
}

/* delete pointer after game */
void BuildingManager::onEnd()
{
	for (std::vector<PlanBuildData*>::iterator iter = plannings.begin();
		iter != plannings.end();)
	{
		delete (*iter); 
		iter = plannings.erase(iter); 
	}
}


/* add plan building item */
void BuildingManager::addPlanBuilding(PlanBuildData* pbd)
{
	plannings.push_back(pbd); 
}


/* remove plan building item */
void BuildingManager::removePlanBuilding(PlanBuildData* pbd)
{
	std::vector<PlanBuildData*>::iterator iter = std::find(plannings.begin(),
		plannings.end(),
		pbd); 
	if (iter != plannings.end())
	{
		(*iter)->builderRecover();
		delete (*iter);
		iter = plannings.erase(iter);
	}
}


/* assign a unit to build certain type of building at given location */
void BuildingManager::buildAtLocation(const BWAPI::TilePosition& p, const BWAPI::UnitType& type)
{
	if (!p.isValid())
		return; 

//	UnitProbe unit = UnitProbeManager::getInstance().getClosestUnit(BWAPI::Position(p), ROLE_NUM, 32*16); 

	UnitProbe unit = getBuilder(BWAPI::Position(p)); 

	if (unit == nullptr || unit->getRole() == Role::BUILD)
		return; 

	PlanBuildData* pbd = new PlanBuildData; 
	pbd->building_type = type; 
	pbd->location = p; 
	pbd->builder = unit;
	pbd->builder_role = unit->getRole(); 
	pbd->builder_target_region = unit->getTargetRegion(); 
	pbd->builder_target_base_location = unit->getTargetBaseLocation(); 
	pbd->builder_target_unit = unit->getTargetUnit(); 
	pbd->builder_target_position = unit->getTargetPosition(); 
	pbd->command_frame = BWAPI::Broodwar->getFrameCount(); 

	addPlanBuilding(pbd); 
	/* set BUILD role */
	unit->setRole(Role::BUILD); 
	unit->setPlanBuildData(pbd); 
}


/* return base region build pylon location with the idea of extanding powered area */
BWAPI::TilePosition BuildingManager::getBasePylonLocation()
{
	for (int length = 2; length < 1 + 2 * Config::BuildingManager::BaseBuildRadius; length += 2)
	{
		BWAPI::TilePosition tp(InformationManager::getInstance().getSelfBaseLocation()->getTilePosition() 
			- BWAPI::TilePosition(length / 2, length / 2));
		for (int i = 0; i < 3; i++)
		{
			int dx, dy;
			switch (i)
			{
			case 0:
				dx = 1; dy = 0;
				break;
			case 1:
				dx = 0; dy = 1;
				break;
			case 2:
				dx = -1; dy = 0;
				break;
			case 3:
				dx = 0; dy = -1;
				break;
			}
			for (int j = 0; j <= length - 1; j++)
			{
				if (tp.isValid())
				{
					if (!BWAPI::Broodwar->hasPower(tp) && canBuildHere(tp, BWAPI::UnitTypes::Protoss_Pylon))
						return tp;
				}
				tp.x += dx;
				tp.y += dy;
			}
		}
	}
	return BWAPI::TilePosition(-1, -1);
}

/* return cannon location where it can attack target
if return location is powered, build directly at return location
other first build a pylon */
bool BuildingManager::getCannonLocation(const BWAPI::TilePosition& target, BWAPI::TilePosition& location)
{
	InformationManager& IM = InformationManager::getInstance(); 

	BWAPI::TilePosition location_best(-1, -1);
	BWAPI::TilePosition location_powered_best(-1, -1);
	int distance_best = 99999;
	int distance_powered_best = 99999;
	int cannon_range = BWAPI::UnitTypes::Protoss_Photon_Cannon.groundWeapon().maxRange(); 

	// region closest chokepoint
	BWTA::Region* region = IM.getRegion(BWAPI::Position(target));
	BWTA::Chokepoint* closest_cp;
	assert(!region->getChokepoints().empty());
	if (region->getChokepoints().size() == 1)
		closest_cp = *region->getChokepoints().begin();
	else
		closest_cp = BWTA::getNearestChokepoint(BWAPI::Position(target));

	BWAPI::Position cp_center = closest_cp->getCenter();

	for (int length = 3; length <= 1 + 4 * cannon_range / 32; length += 2)
	{
		bool is_inside = false; 
		BWAPI::TilePosition tp(target - BWAPI::TilePosition(length / 2, length / 2));
		for (int i = 0; i < 3; i++)
		{
			int dx, dy;
			switch (i)
			{
			case 0:
				dx = 1; dy = 0;
				break;
			case 1:
				dx = 0; dy = 1;
				break;
			case 2:
				dx = -1; dy = 0;
				break;
			case 3:
				dx = 0; dy = -1;
				break;
			}
			for (int j = 0; j <= length - 1; j++)
			{
				if (region != IM.getRegion(BWAPI::Position(tp)))
					continue; 
				is_inside = true; 

				if (tp.isValid() && BWAPI::Broodwar->isWalkable(BWAPI::WalkPosition(tp)) &&
					canBuildHere(tp, BWAPI::UnitTypes::Protoss_Photon_Cannon))
				{
					int distance = cp_center.getDistance(BWAPI::Position(tp));
					if (BWAPI::Broodwar->hasPower(tp, BWAPI::UnitTypes::Protoss_Photon_Cannon))
					{
						if (distance < distance_powered_best)
						{
							distance_powered_best = distance;
							location_powered_best = tp;
						}
					}
					else
					{
						if (distance < distance_best)
						{
							distance_best = distance;
							location_best = tp;
						}
					}
				}

				tp.x += dx;
				tp.y += dy;
			}
		}

		if (location_powered_best.isValid())
		{
			location = location_powered_best;
			return true;
		}
		else if (location_best.isValid() && !is_inside)
		{
			location = location_best; 
			return false; 
		}
	}

	
	location = location_best;
	return false;
}


/* return base region build certain unit type location that has power */
BWAPI::TilePosition BuildingManager::getBaseBuildLocation(const BWAPI::UnitType& type)
{
	InformationManager& IM = InformationManager::getInstance(); 

	for (int length = 2; length < 1 + 2 * Config::BuildingManager::BaseBuildRadius; length += 2)
	{
		BWAPI::TilePosition tp(IM.getSelfBaseLocation()->getTilePosition() - BWAPI::TilePosition(length / 2, length / 2));
		for (int i = 0; i < 3; i++)
		{
			int dx, dy;
			switch (i)
			{
			case 0:
				dx = 1; dy = 0;
				break;
			case 1:
				dx = 0; dy = 1;
				break;
			case 2:
				dx = -1; dy = 0;
				break;
			case 3:
				dx = 0; dy = -1;
				break;
			}
			for (int j = 0; j <= length - 1; j++)
			{
				if (BWAPI::Broodwar->hasPower(tp, type) && canBuildHere(tp, type))
					return tp;
				tp.x += dx;
				tp.y += dy;
			}
		}
	}
	return BWAPI::TilePosition(-1, -1);
}

/* get certain type buildings */
std::vector<BWAPI::Unit> BuildingManager::getBuildingUnits(BWAPI::UnitType type)
{
	if (type == BWAPI::UnitTypes::None)
		return buildings; 

	std::vector<BWAPI::Unit> type_buildings(0); 
	for (auto& u : buildings)
	{
		if (u->getType() == type)
			type_buildings.push_back(u); 
	}

	return type_buildings; 
}

std::vector<BWAPI::Unit> BuildingManager::getConstructingUnits(BWAPI::UnitType type)
{
	if (type == BWAPI::UnitTypes::None)
		return constructings;

	std::vector<BWAPI::Unit> type_constructings(0);
	for (auto& u : constructings)
	{
		if (u->getType() == type)
			type_constructings.push_back(u);
	}
	return type_constructings;
}

std::vector<PlanBuildData*> BuildingManager::getPlanningUnits(BWAPI::UnitType type)
{
	if (type == BWAPI::UnitTypes::None)
		return plannings;

	std::vector<PlanBuildData*> type_plannings(0);
	for (auto& u : plannings)
	{
		if (u->building_type == type)
			type_plannings.push_back(u);
	}
	return type_plannings;
}

/* return a neighbor buildable location for a given build type
also return if the location is powered */
BWAPI::TilePosition BuildingManager::getNearBuildLocation(const BWAPI::TilePosition& location,
	const BWAPI::UnitType& unit_type,
	int radius)
{
	// when build new building, it need some space from other buildings
	int space = Config::BuildingManager::BuildLocationSpace; 
	BWAPI::TilePosition tp_space(space, space); 

	for (int length = 3; length <= 1 + 2 * radius; length += 2)
	{
		BWAPI::TilePosition tp(location - BWAPI::TilePosition(length / 2, length / 2));
		for (int i = 0; i < 3; i++)
		{
			int dx, dy;
			switch (i)
			{
			case 0:
				dx = 1; dy = 0;
				break;
			case 1:
				dx = 0; dy = 1;
				break;
			case 2:
				dx = -1; dy = 0;
				break;
			case 3:
				dx = 0; dy = -1;
				break;
			}
			for (int j = 0; j <= length - 1; j++)
			{
/*				if (tp.isValid() && BWAPI::Broodwar->canBuildHere(tp, unit_type) &&
					(tp+tp_space).isValid() && BWAPI::Broodwar->canBuildHere(tp+tp_space, unit_type)) */

				bool can_build_here = canBuildHere(tp, unit_type); 

				if (can_build_here)
				{
					if (unit_type == BWAPI::UnitTypes::Protoss_Pylon)
						return tp;
					else if (BWAPI::Broodwar->hasPower(tp))
						return tp; 
				}
				tp.x += dx;
				tp.y += dy;
			}
		}
	}
	return BWAPI::TilePosition(-1, -1);
}


void BuildingManager::buildBasePylon()
{
	BWAPI::TilePosition location = getBasePylonLocation();
	if (location.isValid())
		buildAtLocation(location, BWAPI::UnitTypes::Protoss_Pylon);
	else
	{
		location = getBaseBuildLocation(BWAPI::UnitTypes::Protoss_Pylon);
		buildAtLocation(location, BWAPI::UnitTypes::Protoss_Pylon);
	}
}

/* make a position has power,
if some pylon is constructing, return directly
otherwise build a pylon at the same location
return if or not a new pylon is built */
bool BuildingManager::makeLocationPower(const BWAPI::TilePosition& location)
{
	if (!location.isValid())
		return false; 

	bool is_pylon_constructing = false;

	// pylon power range is little more than 8 tiles
	for (auto& u : BWAPI::Broodwar->getUnitsInRadius(BWAPI::Position(location),
		32 * 7,
		BWAPI::Filter::IsOwned && !BWAPI::Filter::IsCompleted))
	{
		if (u->getType() = BWAPI::UnitTypes::Protoss_Pylon)
		{
			is_pylon_constructing = true;
			break;
		}
	}

	if (!is_pylon_constructing)
	{
		buildAtLocation(location, BWAPI::UnitTypes::Protoss_Pylon);
		return true;
	}
	else
		return false; 
}

bool BuildingManager::canBuildHere(const BWAPI::TilePosition& tp, const BWAPI::UnitType& type)
{
	for (int x = tp.x; x < tp.x + type.tileWidth(); x++)
	{
		for (int y = tp.y; y < tp.y + type.tileHeight(); y++)
		{
			if (!buildable(x, y, type))
				return false; 
		}
	}
	return true; 
}

bool BuildingManager::buildable(int x, int y, const BWAPI::UnitType & type)
{
	//returns true if this tile is currently buildable, takes into account units on tile
	if (!BWAPI::TilePosition(x,y).isValid() || !BWAPI::Broodwar->isBuildable(x, y)) // &&|| b.type == BWAPI::UnitTypes::Zerg_Hatchery
	{
		return false;
	}

	if (BWAPI::Broodwar->hasCreep(x, y))
		return false; 

	// just consider if something block, never mind power
/*	if (!(BWAPI::Broodwar->hasPower(x, y) || type == BWAPI::UnitTypes::Protoss_Pylon))
	{
		return false;
	} */

	for(auto& unit : BWAPI::Broodwar->getUnitsOnTile(x, y))
	{
		if (unit->getType().isBuilding() && !unit->isLifted())
		{
			return false;
		}
	}

	return true;
}


UnitProbe BuildingManager::getBuilder(const BWAPI::Position& p)
{
	InformationManager& IM = InformationManager::getInstance(); 

	BWTA::Region* region = IM.getRegion(p); 

	int distance = 99999; 
	UnitProbe builder = nullptr; 

	for (auto& u : IM.getRegionSelfUnits(region))
	{
		int d = u->getUnit()->getDistance(p); 
		if (u->getRole() != Role::BUILD && d < distance)
		{
			distance = d; 
			builder = u; 
		}
	}

	if (builder == nullptr)
	{
		builder = UnitProbeManager::getInstance().getClosestUnit(p, Role::ATTACK, 32 * 10); 
	}

	return builder; 
}