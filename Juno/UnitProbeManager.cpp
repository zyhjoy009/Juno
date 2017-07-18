#include "UnitProbeManager.h"
#include "InformationManager.h"
#include "Config.h"
#include <assert.h>
#include "Juno.h"

/* onFrame call: update each member*/
void UnitProbeManager::onFrame()
{
	for (auto& u : units)
		u->onFrame();

	for (auto& u : map_unanalyze_units)
		u->onFrame(); 
}

/* onUnitCreate call */
void UnitProbeManager::onUnitCreate(BWAPI::Unit u)
{
	if (u->getPlayer() != BWAPI::Broodwar->self() ||
		u->getType().isBuilding())
		return; 

	// before map is analyzed 
	if (!InformationManager::getInstance().isMapAnalyzed())
	{
		// when game start all units are set to BeforeMapAnazlyed
		if (map_unanalyze_units.size() <= Config::UnitProbeManager::MapUnAnalyzeScoutNum)
		{
			UnitProbe up = new UnitProbeInterface(u,
				Role::BEFORE_MAP_ANALYZED_SCOUT,
				nullptr,
				nullptr,
				nullptr,
				BWAPI::Position(-1, -1));
			map_unanalyze_units.push_back(up);
		}
		else
		{
			UnitProbe up = new UnitProbeInterface(u,
				Role::BEFORE_MAP_ANALYZED_MINERAL,
				nullptr,
				nullptr,
				nullptr,
				BWAPI::Position(-1, -1));
			map_unanalyze_units.push_back(up);
		}
		return; 
	}

	// here is what we do after map analyze
	InformationManager& IM = InformationManager::getInstance(); 

	// when game start all scout start locations
	if (BWAPI::Broodwar->getFrameCount() <= 0)
	{
/*		std::vector<BWTA::BaseLocation*> sls = IM.getScoutTargetStartLocations(); 
		assert(!sls.empty()); 
		UnitProbe up = new UnitProbeInterface(u,
			Role::SCOUT_START_LOCATION,
			sls[0]->getRegion(),
			sls[0],
			nullptr,
			sls[0]->getPosition());
		units.push_back(up);  */
		// where and how to scout work is done in UnitProbeInterface::scout()
		std::pair<Role, BWAPI::Position> role_target = InformationManager::getInstance().getDesireScoutTarget(u->getPosition());
		UnitProbe up = new UnitProbeInterface(u,
			role_target.first,
			nullptr,
			nullptr,
			nullptr,
			role_target.second);
		units.push_back(up);
	}
	else
	{
		UnitProbe up = new UnitProbeInterface(u, 
			Role::MINERAL, 
			IM.getSelfRegion(), 
			IM.getSelfBaseLocation(), 
			nullptr, 
			BWAPI::Position(-1, -1)); 
		units.push_back(up); 
	}
}

/* onUnitDestroy call */
void UnitProbeManager::onUnitDestroy(BWAPI::Unit u)
{
	if (u->getPlayer() != BWAPI::Broodwar->self() ||
		u->getType().isBuilding())
		return;

	std::vector<UnitProbe>::iterator iter = getUnitIterator(u); 
	assert(iter != units.end()); 

	// special case for builder, builder is null 
	if ((*iter)->getRole() == Role::BUILD)
		(*iter)->getPlanBuildDataPointer()->builder = nullptr; 

	units.erase(iter); 
}

/* delete pointer after game */
void UnitProbeManager::onEnd()
{
	for (std::vector<UnitProbe>::iterator iter = units.begin();
		iter != units.end();)
	{
		delete (*iter); 
		iter = units.erase(iter); 
	}

	for (std::vector<UnitProbe>::iterator iter = map_unanalyze_units.begin();
		iter != map_unanalyze_units.end();)
	{
		delete (*iter);
		iter = map_unanalyze_units.erase(iter);
	}

}


/* return the closest unit from a position with give Role */
UnitProbe UnitProbeManager::getClosestUnit(const BWAPI::Position& p, const Role& r, int radius)
{
	int distance_closest = 99999; 
	UnitProbe unit_closest = nullptr; 
	for (auto& u : units)
	{
		if (u->getUnit()->isCompleted() &&
			(r == ROLE_NUM || u->getRole() == r))
		{
			int dis = u->getUnit()->getDistance(p);
			if (dis < radius && dis < distance_closest)
			{
				distance_closest = dis;
				unit_closest = u;
			}
		}
	}
	return unit_closest; 
}


/* swap two units of their data */
void UnitProbeManager::swap(UnitProbe u1, UnitProbe u2)
{
	UnitProbeInterface upi(*u2); 
	*u2 = *u1; 
	*u1 = upi; 
}


/* find UnitProbe from Unit */
UnitProbe UnitProbeManager::getUnit(const BWAPI::Unit& u)
{
	for (std::vector<UnitProbe>::reverse_iterator iter = units.rbegin();
		iter != units.rend();
		iter++)
	{
		if ((*iter)->getUnit() == u)
			return *iter; 
	}
	return nullptr; 
}

std::vector<UnitProbe>::iterator UnitProbeManager::getUnitIterator(const BWAPI::Unit& u)
{
	std::vector<UnitProbe>::iterator iter; 
	for (iter = units.begin(); iter != units.end(); iter++)
	{
		if ((*iter)->getUnit() == u)
			break; 
	}
	return iter; 
}