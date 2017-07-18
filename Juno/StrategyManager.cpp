#include "StrategyManager.h"
#include "Config.h"
#include "InformationManager.h"
#include "BuildingManager.h"
#include "UnitProbeInterface.h"
#include "UnitProbeManager.h"
#include "Juno.h"
#include <assert.h>
#include "DrawManager.h"
#include "Timer.h"

extern HANDLE hthread_search = nullptr;

/* try to adjust unit, building, production, etc */
void StrategyManager::onFrame()
{
	timer_display_y += 10;

	if (!(BWAPI::Broodwar->getFrameCount() == 0 ||
		BWAPI::Broodwar->getFrameCount() - last_frame > Config::StrategyManager::FrequencyFrame))
		return; 

	// before map analyzed, do nothing 
	if (!InformationManager::getInstance().isMapAnalyzed())
		return; 

	if (timer.getNoStopElapsedTimeInMicroSec() + Config::Timer::StrategyManagerMaxMicroSec > Config::Timer::TotalMicroSec)
		return;
	Timer tt; 
	tt.start(); 

	last_frame = BWAPI::Broodwar->getFrameCount(); 

	// after map analyze, first recover informaion mananger, building mananger and unit manager
	recoverBeforeMapAnalyzeUnits();

	InformationManager&		IM = InformationManager::getInstance();
	BuildingManager&		BM = BuildingManager::getInstance();
	UnitProbeManager&		UPM = UnitProbeManager::getInstance();

	// 0 depot is destroy, try first to rebuild 
	rebuildDepot(); 

	// 1.1 DEFEND->MINERAL 
	swapRegionDefendMineral(IM.getSelfRegion()); 

	/* 1.2 too many DEFENDs */
	makeRegionDefendToMineral(IM.getSelfRegion()); 

	/* 2.1 MINERAL->DEFEND, enemy more than self DEFEND */
	makeRegionMineralToDefend(IM.getSelfRegion()); 

	/* 2.2 MINERAL very close to enemy */
	makeRegionMineralFastAttack(IM.getSelfRegion()); 

	/* 3 ATTACK->SCOUT */
	for (auto& r : BWTA::getRegions())
		makeRegionAttackToScout(r); 
	
	/* 4 MINERAL -> SCOUT */
	makeRegionMineralToScout(IM.getSelfRegion()); 
	
	/* 5 SCOUT -> ATTACK */
	makeScoutToAttack(); 

	/* replenish minimum attack */
	for (auto& r : BWTA::getRegions())
		replenishMinimumAttack(r); 
	
	// 6 MINERAL->ATTACK 
	makeMineralToAttack(IM.getSelfRegion()); 

	// 7 supply limited 
	replenishSupply(); 

	// 7-1 recover power
	recoverBuildPower(); 

	// update build actions
	if (is_updated_build_actions)
	{
		build_action_frames = update_build_action_frames; 
		is_updated_build_actions = false; 
		last_action_update_frame = BWAPI::Broodwar->getFrameCount(); 
		if (hthread_search)
		{
			CloseHandle(hthread_search); 
			hthread_search = nullptr; 
		}
	}
	updateBuildActions(); 
	
	// 8 follow build actions
	if (!build_action_frames.empty())
	{
		executeBuildActions(); 
	}
	// otherwise
	else
	{
		// train Probe 
		trainProbe(); 
		// build cannon 
		buildCannon(); 
	}

	DrawManager::getInstance().drawSearchBuild(build_action_frames);

	tt.stop();
	DrawManager::getInstance().drawTime(10, timer_display_y, Config::StrategyManager::FrequencyFrame,
		"StrategyManager", tt.getElapsedTimeInMicroSec());

}


/* sort compare function
sort unit from hp low to hp high */
bool StrategyManager::compUnitHpShield(const UnitProbe& u1, const UnitProbe& u2)
{
	/* hp+shield compare: less health */
	if ((u1->getUnit()->getHitPoints() + u1->getUnit()->getShields())
		< (u2->getUnit()->getHitPoints() + u2->getUnit()->getShields()))
		return true;
	else
		return false;
}


/* replace low hp defend units in self region by mineral units */
void StrategyManager::swapRegionDefendMineral(BWTA::Region* region)
{
	InformationManager& IM = InformationManager::getInstance(); 

	// sort mineral units
	std::vector<UnitProbe> mu_low_high = IM.getRegionSelfUnits(region, Role::MINERAL);
	std::sort(mu_low_high.begin(), mu_low_high.end(), compUnitHpShield);
	std::vector<UnitProbe>::reverse_iterator mu_riter = mu_low_high.rbegin();

	// sort defend units
	std::vector<UnitProbe> du_low_high = IM.getRegionSelfUnits(IM.getSelfRegion(), Role::DEFEND);
	std::sort(du_low_high.begin(), du_low_high.end(), compUnitHpShield);
	std::vector<UnitProbe>::iterator du_iter = du_low_high.begin();

	for (; du_iter != du_low_high.end() && mu_riter != mu_low_high.rend();)
	{
		int mhp = (*mu_riter)->getUnit()->getHitPoints() + (*mu_riter)->getUnit()->getShields();
		int mhp_max = (*mu_riter)->getUnit()->getType().maxHitPoints() + (*mu_riter)->getUnit()->getType().maxShields();
		if (mhp < 0.7*mhp_max)
			break;

		int dhp = (*du_iter)->getUnit()->getHitPoints() + (*du_iter)->getUnit()->getShields();
		int dhp_max = (*du_iter)->getUnit()->getType().maxHitPoints() + (*du_iter)->getUnit()->getType().maxShields();
		if (dhp < 0.4*dhp_max)
		{
			UnitProbeManager::getInstance().swap(*du_iter, *mu_riter);
			du_iter++;
			mu_riter++;
		}
		else
			break;
	}
}

/* let extra defend units in a region to mineral */
void StrategyManager::makeRegionDefendToMineral(BWTA::Region* region)
{
	InformationManager& IM = InformationManager::getInstance();
	BWAPI::WeaponType wt = BWAPI::UnitTypes::Protoss_Probe.groundWeapon(); 

	double enemy_weapon_sum = IM.getRegionEnemyWeaponSum(region);
	double self_weapon_sum = IM.getRegionSelfWeaponSum(region);
	int num = (self_weapon_sum - 2 * enemy_weapon_sum) /
		(double(wt.damageAmount()) * wt.damageFactor() / wt.damageCooldown()); 

	if (num > 0)
	{
		std::vector<UnitProbe> du_low_high = IM.getRegionSelfUnits(region, Role::DEFEND);
		std::sort(du_low_high.begin(), du_low_high.end(), compUnitHpShield);
		std::vector<UnitProbe>::iterator du_iter = du_low_high.begin();
		for (int i = 0; i < num && du_iter != du_low_high.end(); i++, du_iter++)
		{
			(*du_iter)->clear();
			(*du_iter)->setRole(Role::MINERAL);
			(*du_iter)->setTargetRegion(region);
		}
	}
}

/* let mineral to defend if enemy is more than self */
void StrategyManager::makeRegionMineralToDefend(BWTA::Region* region)
{
	InformationManager& IM = InformationManager::getInstance();
	BWAPI::WeaponType wt = BWAPI::UnitTypes::Protoss_Probe.groundWeapon();

	double enemy_weapon_sum = IM.getRegionEnemyWeaponSum(region);
	double self_weapon_sum = IM.getRegionSelfWeaponSum(region);

	int num = __max(0.0, (enemy_weapon_sum - self_weapon_sum) /
		(double(wt.damageAmount()) * wt.damageFactor() / wt.damageCooldown())); 

	if (num > 0)
	{
		std::vector<UnitProbe> mu_low_high = IM.getRegionSelfUnits(region, Role::MINERAL);
		num = __max(0, __min(num, int(mu_low_high.size() - 1))); 
		std::sort(mu_low_high.begin(), mu_low_high.end(), compUnitHpShield); 
		std::vector<UnitProbe>::reverse_iterator mu_riter = mu_low_high.rbegin();
		for (int i = 0; i < num && mu_riter != mu_low_high.rend(); i++, mu_riter++)
		{
			(*mu_riter)->clear();
			(*mu_riter)->setRole(Role::DEFEND);
			(*mu_riter)->setTargetRegion(region);
		}
	}
}

/* let mineral attack very close enemy */
void StrategyManager::makeRegionMineralFastAttack(BWTA::Region* region)
{
	InformationManager& IM = InformationManager::getInstance();
	int probe_max_hp = BWAPI::UnitTypes::Protoss_Probe.maxHitPoints() + BWAPI::UnitTypes::Protoss_Probe.maxShields(); 

	std::vector<UnitProbe> munits = IM.getRegionSelfUnits(region, Role::MINERAL); 

	if (munits.size() <= Config::StrategyManager::MinMineNum)
		return; 

	int available = munits.size() - Config::StrategyManager::MinMineNum;

	for (auto& u : munits)
	{
		if (available <= 0)
			break; 

		if (u->getUnit()->getGroundWeaponCooldown()>0 ||
			(u->getUnit()->getHitPoints() + u->getUnit()->getShields()) < 0.7*probe_max_hp)
			continue;
		BWAPI::Unit eu = u->getUnit()->getClosestUnit(BWAPI::Filter::IsEnemy && !BWAPI::Filter::IsFlying, 1.5 * 32); 
		if (eu)
		{
			u->clear();
			u->setTargetRegion(region);
			u->setRole(Role::DEFEND);
			available++; 
		}
	}
}

/* let attack after combat to scout */
void StrategyManager::makeRegionAttackToScout(BWTA::Region* region)
{
	InformationManager& IM = InformationManager::getInstance();

	if (IM.getRegionSelfUnits(region, Role::ATTACK).empty() ||
		!IM.getRegionEnemyBuildings(region).empty() || !IM.getRegionEnemyUnits(region).empty())
		return; 

/*	// make sure every base locations in the region is seen
	// so even if attack unit don't see anything, still take base locations as target_position
	bool real = true; 
	// any base location is seen is fine
	for (auto& bl : region->getBaseLocations())
	{
		if (BWAPI::Broodwar->isVisible(bl->getTilePosition()))
		{
			real = true;
			break;
		}
		else
			real = false; 
	}
	if (!real)
		return;  */

	for (auto& u : IM.getRegionSelfUnits(region, Role::ATTACK))
	{
		// remember to delete InformationManager data 
		IM.removeRegionSelfUnit(u); 
		u->clear(); 
		u->setRole(Role::SCOUT); 
		IM.addTileScout(u); 
	}
}


/* let more mineral to scout */
void StrategyManager::makeRegionMineralToScout(BWTA::Region* region)
{
	InformationManager& IM = InformationManager::getInstance();

	// region has no depot (maybe destroyed), wait for it
	if (IM.getRegionSelfBuildings(region, BWAPI::UnitTypes::Protoss_Nexus).empty())
		return; 

	// mineral patches
	int minerals = 0; 
	for (auto& bl : region->getBaseLocations())
		minerals += bl->getMinerals().size();

	// mineral units
	std::vector<UnitProbe> mineral_units = IM.getRegionSelfUnits(region, Role::MINERAL);
	int mu_num = mineral_units.size(); 
	if (mu_num <= 2.5*minerals)
		return; 

	int num = mu_num - 2.5*minerals;
	// here I just let unit become SCOUT, 
	// their further type (STARTLOCATION/BASELOCATION) and target is determined in other function
	for (auto& u : mineral_units)
	{
		// remember to delete InformationManager data 
		IM.removeRegionSelfUnit(u);
		u->clear(); 
		u->setRole(Role::SCOUT); 
		IM.addTileScout(u); 
	}
}


/* let scout to assist attack region */
void StrategyManager::makeScoutToAttack()
{
	InformationManager& IM = InformationManager::getInstance();

	// get scout units 
	std::vector<UnitProbe> scouts_start = IM.getStartLocationsScouts(); 
	std::vector<UnitProbe> scouts_base = IM.getBaseLocationsScouts(); 
	std::vector<UnitProbe> scouts_tile = IM.getTileScouts(); 
	int scout_num = scouts_start.size() + scouts_base.size() + scouts_tile.size(); 

	if (scout_num == 0)
		return; 

	std::multimap<int, BWTA::Region*> attacks_regions;
	int attack_num = 0; 
	for (auto& r : BWTA::getRegions())
	{
		// no attack in self region
		if (r == IM.getSelfRegion())
			continue; 
		// not attack region 
		if (IM.getRegionEnemyBuildings(r).empty())
			continue; 

		int num = IM.getRegionSelfUnits(r, Role::ATTACK).size(); 
		attack_num += num; 
		attacks_regions.insert(std::pair<int, BWTA::Region*>(num, r)); 
	}

	// no attack region
	if (attacks_regions.empty())
		return; 

	// get average attack num after adjust scouts_tile
	int attack_average_num = std::ceil(double(attack_num + scout_num) / attacks_regions.size()); 
	std::vector<UnitProbe>::iterator iter_start = scouts_start.begin(); 
	std::vector<UnitProbe>::iterator iter_base = scouts_base.begin(); 
	std::vector<UnitProbe>::iterator iter_tile = scouts_tile.begin(); 
	for (auto& ars : attacks_regions)
	{
		int num = __max(0, attack_average_num - ars.first); 
		while (num > 0)
		{
			UnitProbe up = nullptr; 
			if (iter_tile != scouts_tile.end())
			{
				up = *iter_tile;
				IM.removeTileScout(up); 
				iter_tile++; 
			}
			else if (iter_base != scouts_base.end())
			{
				up = *iter_base; 
				IM.removeBaseLocationScout(up); 
				iter_base++; 
			}
			else if (iter_start != scouts_start.end())
			{
				up = *iter_start; 
				IM.removeStartLocationScout(up); 
				iter_start++; 
			}

			if (up)
			{
				up->clear();
				up->setRole(Role::ATTACK);
				up->setTargetRegion(ars.second);
				IM.addRegionSelfUnit(up);
			}
			else
				break; 
			num--; 
		}
	}
}

/* let mineral to replenish attack */
void StrategyManager::makeMineralToAttack(BWTA::Region* region)
{
	InformationManager& IM = InformationManager::getInstance();
	BuildingManager& BM = BuildingManager::getInstance(); 

	std::vector<UnitProbe> mus_low_high = IM.getRegionSelfUnits(region, Role::MINERAL);

	// special consider for zerg 
/*	if (BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Zerg)
	{
		if (mus_low_high.size() < 10)
			return; 
	}
	// other races case
	else */
	{
		// haven't reach build cannon condition
		if (BM.getBuildingUnits(BWAPI::UnitTypes::Protoss_Forge).empty())
		{
			return;
		}

		// if haven't save enough mineral for cannon, don't go
		int need_cannon = 0;
		for (auto& r : BWTA::getRegions())
		{
			need_cannon += IM.getRegionEnemyBuildings(r).size();
		}

		if (BWAPI::Broodwar->self()->minerals() < need_cannon*BWAPI::UnitTypes::Protoss_Photon_Cannon.mineralPrice())
			return;
	}
	
	// leave 3 miner at least 
	if (mus_low_high.size() <= 3)
		return; 

	std::multimap<int, BWTA::Region*> attack_regions; 
	int attack_num = 0; 
	for (auto& r : BWTA::getRegions())
	{
		// no attack in self region
		if (r == region)
			continue;
		// no attack happen
		if (IM.getRegionEnemyBuildings(r).empty())
			continue; 

		int num = IM.getRegionSelfUnits(r, Role::ATTACK).size(); 
		attack_num += num; 
		attack_regions.insert(std::pair<int, BWTA::Region*>(num, r)); 
	}

	if (attack_regions.empty())
		return; 

	// __max available or needed minerals
	int mineral_num = __max(0, int(mus_low_high.size() - 3));
	// average attackers in each region after adjusting 
	int average_num = std::ceil(double(attack_num + mineral_num) / attack_regions.size()); 

	std::sort(mus_low_high.begin(), mus_low_high.end(), compUnitHpShield);

	std::vector<UnitProbe>::reverse_iterator iter = mus_low_high.rbegin(); 

	int probe_max_hp = BWAPI::UnitTypes::Protoss_Probe.maxHitPoints() + BWAPI::UnitTypes::Protoss_Probe.maxShields();

	for (auto& ar : attack_regions)
	{
		int num = average_num - ar.first; 
		bool fail = false; 
		while (num > 0)
		{
			int hp = (*iter)->getUnit()->getHitPoints() + (*iter)->getUnit()->getShields(); 
			if (iter == mus_low_high.rend() || hp < 0.6*probe_max_hp)
			{
				fail = true; 
				break;
			}
			// remove from previous informatoinmanager
			IM.removeRegionSelfUnit(*iter); 
			// set to new role 
			(*iter)->clear(); 
			(*iter)->setRole(Role::ATTACK); 
			(*iter)->setTargetRegion(ar.second); 
			// add new informationmanager
			IM.addRegionSelfUnit(*iter); 

			iter++; 
			num--; 
		}
		if (fail)
			break;
	}
}

/* replenish supply when limited */
void StrategyManager::replenishSupply()
{
	InformationManager& IM = InformationManager::getInstance();
	BuildingManager& BM = BuildingManager::getInstance(); 

	if (BWAPI::Broodwar->self()->supplyUsed() < BWAPI::Broodwar->self()->supplyTotal() - 2)
		return;

	else if (!BM.getConstructingUnits(BWAPI::UnitTypes::Protoss_Pylon).empty())
		return; 

	int need = std::ceil(double(BWAPI::Broodwar->self()->supplyUsed() + 1 - BWAPI::Broodwar->self()->supplyTotal()) / 
		BWAPI::UnitTypes::Protoss_Pylon.supplyProvided()); 
	while (need > 0 && IM.getMineral() >= BWAPI::UnitTypes::Protoss_Pylon.mineralPrice())
	{
		BM.buildBasePylon(); 
		need--; 
	}
}

/* update build actions */
void StrategyManager::updateBuildActions()
{
	// for zerg we cant build cannon 
/*	if (BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Zerg)
		return;  */

	// still searching
	if (is_searching)
		return; 
	// still not update last searched actions
	if (is_updated_build_actions)
		return; 
	if (!(BWAPI::Broodwar->getFrameCount() < 5*42 ||
		BWAPI::Broodwar->getFrameCount() - last_action_update_frame > Config::StrategyManager::SearchFrequencyFrame))
		return; 

	InformationManager& IM = InformationManager::getInstance(); 
	BuildingManager& BM = BuildingManager::getInstance(); 

	// count how many enemy buildings
	int num = 0; 
	for (auto& r : BWTA::getRegions())
	{
		for (auto& eb : IM.getRegionEnemyBuildings(r))
		{
			if (BWAPI::Broodwar->getUnitsInRadius(BWAPI::Position(eb.location),
				BWAPI::UnitTypes::Protoss_Photon_Cannon.groundWeapon().maxRange(),
				BWAPI::Filter::IsOwned && BWAPI::Filter::IsBuilding && BWAPI::Filter::CanAttack).empty())
			{
				num++;
			}
		}
	}

	// at first we don't find enemy and we still haven't built any cannon
	// try to meet build cannon condition 
	if (BM.getBuildingUnits(BWAPI::UnitTypes::Protoss_Photon_Cannon).empty() &&
		BM.getConstructingUnits(BWAPI::UnitTypes::Protoss_Photon_Cannon).empty())
	{
		// but if we have reach the condition, never give any action
		if (!BM.getBuildingUnits(BWAPI::UnitTypes::Protoss_Forge).empty() &&
			IM.getMineral() >= BWAPI::UnitTypes::Protoss_Photon_Cannon.mineralPrice())
		{
			update_build_action_frames.clear(); 
			is_searching = false; 
			is_updated_build_actions = true; 
			return; 
		}

		int depth = 0; 
		int max_depth = 15; 
		int current_frame = BWAPI::Broodwar->getFrameCount(); 
		int max_frame = current_frame + 42*60*10; 
		double current_mineral = IM.getMineral(); 
		int available_supply = BWAPI::Broodwar->self()->supplyTotal() - BWAPI::Broodwar->self()->supplyUsed(); 
		int mine_num = IM.getRegionSelfUnits(IM.getSelfRegion(), Role::MINERAL).size();  

		std::vector<BWAPI::Unit> depots = BM.getBuildingUnits(BWAPI::UnitTypes::Protoss_Nexus); 
		if (depots.empty())
			return; 
		int probe_left_frame = depots[0]->getRemainingTrainTime(); 

		// pylon at first need most 2
		std::vector<BWAPI::Unit> construct_pylons = BM.getConstructingUnits(BWAPI::UnitTypes::Protoss_Pylon); 
		int pylon_left_frame; 
		if (construct_pylons.empty())
		{
			std::vector<PlanBuildData*> plan_pylons = BM.getPlanningUnits(BWAPI::UnitTypes::Protoss_Pylon); 
			if (plan_pylons.empty())
				pylon_left_frame = 0;
			else
				pylon_left_frame = BWAPI::UnitTypes::Protoss_Pylon.buildTime(); 
		}
		else
			pylon_left_frame = construct_pylons[0]->getRemainingBuildTime(); 
		int pylon_num = BM.getBuildingUnits(BWAPI::UnitTypes::Protoss_Pylon).size(); 
		int pylon_need = 2; 
		
		// forge need most 1
		int forge_left_frame; 
		std::vector<BWAPI::Unit> construct_forges = BM.getConstructingUnits(BWAPI::UnitTypes::Protoss_Forge); 
		if (construct_forges.empty())
		{
			std::vector<PlanBuildData*> plan_forges = BM.getPlanningUnits(BWAPI::UnitTypes::Protoss_Forge); 
			if (plan_forges.empty())
				forge_left_frame = 0;
			else
				forge_left_frame = BWAPI::UnitTypes::Protoss_Forge.buildTime(); 
		}
		else
			forge_left_frame = construct_forges[0]->getRemainingBuildTime(); 
		int forge_num = BM.getBuildingUnits(BWAPI::UnitTypes::Protoss_Forge).size(); 
		int forge_need = 1; 

		// cannon need most 1
		int cannon_left_frame; 
		std::vector<BWAPI::Unit> construct_cannons = BM.getConstructingUnits(BWAPI::UnitTypes::Protoss_Photon_Cannon); 
		if (construct_cannons.empty())
			cannon_left_frame = 0;
		else
			cannon_left_frame = construct_cannons[0]->getRemainingBuildTime(); 
		int cannon_num = BM.getBuildingUnits(BWAPI::UnitTypes::Protoss_Photon_Cannon).size(); 
		int cannon_need = 1; 		

		is_searching = true; 

		if (Config::Game::UseThread)
		{
			SearchThreadParam* param = new SearchThreadParam;
			param->depth = depth;
			param->max_depth = max_depth;
			param->current_frame = current_frame;
			param->max_frame = max_frame;
			param->current_mineral = current_mineral;
			param->available_supply = available_supply;
			param->probe_left_frame = probe_left_frame;
			param->mine_num = mine_num;
			param->pylon_left_frame = pylon_left_frame;
			param->pylon_num = pylon_num;
			param->pylon_need = pylon_need;
			param->forge_left_frame = forge_left_frame;
			param->forge_num = forge_num;
			param->forge_need = forge_need;
			param->cannon_left_frame = cannon_left_frame;
			param->cannon_num = cannon_num;
			param->cannon_need = cannon_need;

			hthread_search = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)searchThread, param, 0, NULL);
		}
		else
		{
			update_build_action_frames = searchCannonBuild2(depth, max_depth, current_frame, max_frame, 
				current_mineral, available_supply, 
				probe_left_frame, mine_num, 
				pylon_left_frame, pylon_num, pylon_need, 
				forge_left_frame, forge_num, forge_need, 
				cannon_left_frame, cannon_num, cannon_need); 
			is_searching = false; 
			is_updated_build_actions = true; 
		}

	}
	// otherwise, try best to build needed cannon
	else if (num > 0)
	{
		int depth = 0;
		int max_depth = 15+num;
		int current_frame = BWAPI::Broodwar->getFrameCount();
		int max_frame = current_frame + 42*60*5 + num*BWAPI::UnitTypes::Protoss_Photon_Cannon.buildTime(); 
		double current_mineral = IM.getMineral();
		int available_supply = BWAPI::Broodwar->self()->supplyTotal() - BWAPI::Broodwar->self()->supplyUsed();
		int mine_num = IM.getRegionSelfUnits(IM.getSelfRegion(), Role::MINERAL).size();

		std::vector<BWAPI::Unit> depots = BM.getBuildingUnits(BWAPI::UnitTypes::Protoss_Nexus);
		if (depots.empty())
			return;
		int probe_left_frame = depots[0]->getRemainingTrainTime();

		// pylon no special need
		std::vector<BWAPI::Unit> construct_pylons = BM.getConstructingUnits(BWAPI::UnitTypes::Protoss_Pylon);
		int pylon_left_frame;
		if (construct_pylons.empty())
			pylon_left_frame = 0;
		else
			pylon_left_frame = construct_pylons[0]->getRemainingBuildTime();
		int pylon_num = BM.getBuildingUnits(BWAPI::UnitTypes::Protoss_Pylon).size();
		int pylon_need = 2;

		// forge need most 1
		int forge_left_frame;
		std::vector<BWAPI::Unit> construct_forges = BM.getConstructingUnits(BWAPI::UnitTypes::Protoss_Forge);
		if (construct_forges.empty())
			forge_left_frame = 0;
		else
			forge_left_frame = construct_forges[0]->getRemainingBuildTime();
		int forge_num = BM.getBuildingUnits(BWAPI::UnitTypes::Protoss_Forge).size();
		int forge_need = 1;

		// cannon need 
		int cannon_left_frame = 0;
		int cannon_num = 0;
		int cannon_need = num;

		is_searching = true; 

		if (Config::Game::UseThread)
		{
			SearchThreadParam* param = new SearchThreadParam;
			param->depth = depth;
			param->max_depth = max_depth;
			param->current_frame = current_frame;
			param->max_frame = max_frame;
			param->current_mineral = current_mineral;
			param->available_supply = available_supply;
			param->probe_left_frame = probe_left_frame;
			param->mine_num = mine_num;
			param->pylon_left_frame = pylon_left_frame;
			param->pylon_num = pylon_num;
			param->pylon_need = pylon_need;
			param->forge_left_frame = forge_left_frame;
			param->forge_num = forge_num;
			param->forge_need = forge_need;
			param->cannon_left_frame = cannon_left_frame;
			param->cannon_num = cannon_num;
			param->cannon_need = cannon_need;

			hthread_search = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)searchThread, param, 0, NULL);
		}
		else
		{
			update_build_action_frames = searchCannonBuild2(depth, max_depth, current_frame, max_frame,
				current_mineral, available_supply,
				probe_left_frame, mine_num,
				pylon_left_frame, pylon_num, pylon_need,
				forge_left_frame, forge_num, forge_need,
				cannon_left_frame, cannon_num, cannon_need);
			is_searching = false;
			is_updated_build_actions = true;
		}

	}
}

DWORD WINAPI searchThread(LPVOID pParam)
{
	SearchThreadParam* param = (SearchThreadParam*)pParam; 
	StrategyManager::getInstance().update_build_action_frames = StrategyManager::getInstance().searchCannonBuild2(param->depth,
		param->max_depth,
		param->current_frame,
		param->max_frame,
		param->current_mineral,
		param->available_supply,
		param->probe_left_frame,
		param->mine_num,
		param->pylon_left_frame,
		param->pylon_num,
		param->pylon_need,
		param->forge_left_frame,
		param->forge_num,
		param->forge_need,
		param->cannon_left_frame,
		param->cannon_num,
		param->cannon_need); 

	StrategyManager::getInstance().is_searching = false; 
	StrategyManager::getInstance().is_updated_build_actions = true; 

	delete param; 

	return 0; 
}

/* follow searched build actions*/
void StrategyManager::executeBuildActions()
{
	InformationManager& IM = InformationManager::getInstance(); 
	BuildingManager& BM = BuildingManager::getInstance(); 

	for (std::vector<BuildActionFrame>::iterator baf_iter = build_action_frames.begin();
		baf_iter != build_action_frames.end();)
	{
		// no mineral, wait for next call
		if (IM.getMineral() < baf_iter->first.mineralPrice())
			break;

		// Probe
		if (baf_iter->first == BWAPI::UnitTypes::Protoss_Probe)
		{
			std::vector<BWAPI::Unit> depots = BM.getBuildingUnits(BWAPI::UnitTypes::Protoss_Nexus);
			if (depots.empty())
				break;
			if (!depots[0]->isIdle())
				break;
			depots[0]->train(baf_iter->first);
			baf_iter = build_action_frames.erase(baf_iter);
		}
		// Pylon
		else if (baf_iter->first == BWAPI::UnitTypes::Protoss_Pylon)
		{
			// first Pylon always build at self region
			if (BM.getBuildingUnits(baf_iter->first).empty() &&
				BM.getConstructingUnits(baf_iter->first).empty())
			{
				BM.buildBasePylon(); 
			}
			// rest is build ready for cannon
			else
			{
				BWTA::Region* r = IM.getCannonDesiredRegion();
				BWAPI::TilePosition etp = IM.getRegionCannonTarget(r);
				BWAPI::TilePosition location;
				bool is_powered = BM.getCannonLocation(etp, location);
				// future cannon don't have power, build at attack region
				if (!is_powered)
				{
					// some pylon is constructing nearby 
					if (!BM.makeLocationPower(location))
						BM.buildBasePylon(); 
				}
				// future cannon has power, build at self region
				else
				{
					BM.buildBasePylon(); 
				}
			}
			baf_iter = build_action_frames.erase(baf_iter);
		}
		// Forge 
		else if (baf_iter->first == BWAPI::UnitTypes::Protoss_Forge)
		{
			BWAPI::TilePosition location = BM.getBaseBuildLocation(baf_iter->first);
			if (location.isValid())
			{
				BM.buildAtLocation(location, baf_iter->first);
				baf_iter = build_action_frames.erase(baf_iter);
			}
			else
			{
				// if has built Pylon but still constructing, wait
				BM.makeLocationPower(location); 
				break;
			}
		}
		// Cannon 
		else if (baf_iter->first == BWAPI::UnitTypes::Protoss_Photon_Cannon)
		{
			BWTA::Region* r = IM.getCannonDesiredRegion();
			BWAPI::TilePosition etp; 
			// attack region target is enemy
			if (r != IM.getSelfRegion())
				etp = IM.getRegionCannonTarget(r);
			// self region target is self depot
			else
				etp = IM.getSelfBaseLocation()->getTilePosition(); 
			BWAPI::TilePosition location;
			bool is_powered = BM.getCannonLocation(etp, location);
			if (is_powered)
			{
				BM.buildAtLocation(location, baf_iter->first);
				baf_iter = build_action_frames.erase(baf_iter);
			}
			else
			{
				BM.makeLocationPower(location); 
				break;
			}
		}
	}
}


/* train Probe */
void StrategyManager::trainProbe()
{
	InformationManager& IM = InformationManager::getInstance(); 
	BuildingManager& BM = BuildingManager::getInstance(); 

	if (IM.getMineral() >= BWAPI::UnitTypes::Protoss_Probe.mineralPrice() &&
		BWAPI::Broodwar->self()->supplyTotal() > BWAPI::Broodwar->self()->supplyUsed())
	{
		std::vector<BWAPI::Unit> depots = BM.getBuildingUnits(BWAPI::UnitTypes::Protoss_Nexus);
		if (!depots.empty() && depots[0]->isIdle())
			depots[0]->train(BWAPI::UnitTypes::Protoss_Probe);
	}
}

/* build Cannon  */
void StrategyManager::buildCannon()
{
/*	if (BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Zerg)
		return;  */

	InformationManager& IM = InformationManager::getInstance(); 
	BuildingManager& BM = BuildingManager::getInstance(); 

	// too early to be able to build cannon
	if (BWAPI::Broodwar->getFrameCount() < 2000)
		return; 

	if (IM.getMineral() < BWAPI::UnitTypes::Protoss_Photon_Cannon.mineralPrice())
		return; 

	BWTA::Region* r = IM.getCannonDesiredRegion();

	if (r == nullptr)
		return; 

	BWAPI::TilePosition etp;
	// attack region target is enemy
	if (r != IM.getSelfRegion())
		etp = IM.getRegionCannonTarget(r);
	// self region target is self depot
	else
		etp = IM.getSelfBaseLocation()->getTilePosition();
	BWAPI::TilePosition location;
	bool is_powered = BM.getCannonLocation(etp, location);
	if (is_powered)
	{
		BM.buildAtLocation(location, BWAPI::UnitTypes::Protoss_Photon_Cannon);
	}
	else
	{
		BM.makeLocationPower(location); 
	}

}

/* recover build power */
void StrategyManager::recoverBuildPower()
{
	InformationManager& IM = InformationManager::getInstance(); 
	BuildingManager& BM = BuildingManager::getInstance(); 

	for (auto& u : BM.getBuildingUnits())
	{
		if (u->getType().isPowerup() &&
			!BWAPI::Broodwar->hasPower(u->getTilePosition(), u->getType()) &&
			IM.getMineral() >= BWAPI::UnitTypes::Protoss_Pylon.mineralPrice())
		{
			BM.buildAtLocation(u->getTilePosition(), BWAPI::UnitTypes::Protoss_Pylon);
		}
	}
}

/* rebuild Depot if destroyed */
void StrategyManager::rebuildDepot()
{
	InformationManager& IM = InformationManager::getInstance();
	BuildingManager& BM = BuildingManager::getInstance();

	if (BM.getBuildingUnits(BWAPI::UnitTypes::Protoss_Nexus).empty() &&
		BM.getConstructingUnits(BWAPI::UnitTypes::Protoss_Nexus).empty() &&
		BM.getPlanningUnits(BWAPI::UnitTypes::Protoss_Nexus).empty())
	{
		BM.buildAtLocation(IM.getSelfBaseLocation()->getTilePosition(), BWAPI::UnitTypes::Protoss_Nexus);
	}
}


BuildActionFrames StrategyManager::searchCannonBuild2(int depth,
	const int&	max_depth,
	int			current_frame,
	int			max_frame,
	double		current_mineral,
	int			available_supply,
	int			probe_left_frame,
	int			mine_num,
	int			pylon_left_frame,
	int			pylon_num,
	const int&	pylon_need,
	int			forge_left_frame,
	int			forge_num,
	const int&	forge_need,
	int			cannon_left_frame,
	int			cannon_num,
	const int&	cannon_need)
{
	/* exceed __max frame */
	if (depth >= max_depth || current_frame >= max_frame)
	{
		return BuildActionFrames(1, BuildActionFrame(BWAPI::UnitTypes::None, max_frame));
	}

	/* only last cannon to build and have enough mineral */
	if (pylon_num >= 2 && pylon_num >= pylon_need && forge_num >= forge_need &&
		cannon_num >= cannon_need - 1 &&
		current_mineral >= BWAPI::UnitTypes::Protoss_Photon_Cannon.mineralPrice())
	{
		return BuildActionFrames(1, BuildActionFrame(BWAPI::UnitTypes::Protoss_Photon_Cannon, current_frame));
	}

	BuildActionFrames action_frames_best(0);
	/* select action and move into next search */
	// build cannon 
	if (pylon_num >= 2 && forge_num > 0 && pylon_num > 0 && cannon_left_frame == 0 &&
		current_mineral >= BWAPI::UnitTypes::Protoss_Photon_Cannon.mineralPrice())
	{
		BuildActionFrames res = searchCannonBuild2(depth + 1, max_depth, current_frame, max_frame,
			current_mineral - BWAPI::UnitTypes::Protoss_Photon_Cannon.mineralPrice(), available_supply,
			probe_left_frame, mine_num,
			pylon_left_frame, pylon_num, pylon_need,
			forge_left_frame, forge_num, forge_need,
			BWAPI::UnitTypes::Protoss_Photon_Cannon.buildTime(), cannon_num, cannon_need); 
		if (!res.empty() &&
			res.back().first == BWAPI::UnitTypes::Protoss_Photon_Cannon &&
			(action_frames_best.empty() || res.back().second < action_frames_best.back().second))
		{
			res.insert(res.begin(), BuildActionFrame(BWAPI::UnitTypes::Protoss_Photon_Cannon, current_frame)); 
			action_frames_best = res; 
			max_frame = action_frames_best.back().second; 
		}
	}

	// build forge 
	if (pylon_num > 0 && forge_num < forge_need && pylon_num > 0 && forge_left_frame == 0 && 
		current_mineral >= BWAPI::UnitTypes::Protoss_Forge.mineralPrice())
	{
		BuildActionFrames res = searchCannonBuild2(depth + 1, max_depth, current_frame, max_frame,
			current_mineral - BWAPI::UnitTypes::Protoss_Forge.mineralPrice(), available_supply,
			probe_left_frame, mine_num,
			pylon_left_frame, pylon_num, pylon_need,
			BWAPI::UnitTypes::Protoss_Forge.buildTime(), forge_num, forge_need,
			cannon_left_frame, cannon_num, cannon_need);
		if ((!res.empty() &&
			res.back().first == BWAPI::UnitTypes::Protoss_Photon_Cannon &&
			(action_frames_best.empty() || res.back().second < action_frames_best.back().second)))
		{
			res.insert(res.begin(), BuildActionFrame(BWAPI::UnitTypes::Protoss_Forge, current_frame));
			action_frames_best = res;
			max_frame = action_frames_best.back().second;
		}
	}

	// build pylon 
	if (pylon_left_frame == 0 && current_mineral >= BWAPI::UnitTypes::Protoss_Pylon.mineralPrice())
	{
		BuildActionFrames res = searchCannonBuild2(depth + 1, max_depth, current_frame, max_frame,
			current_mineral - BWAPI::UnitTypes::Protoss_Pylon.mineralPrice(), available_supply,
			probe_left_frame, mine_num,
			BWAPI::UnitTypes::Protoss_Pylon.buildTime(), pylon_num, pylon_need,
			forge_left_frame, forge_num, forge_need,
			cannon_left_frame, cannon_num, cannon_need); 
		if ((!res.empty() &&
			res.back().first == BWAPI::UnitTypes::Protoss_Photon_Cannon &&
			(action_frames_best.empty() || res.back().second < action_frames_best.back().second)))
		{
			res.insert(res.begin(), BuildActionFrame(BWAPI::UnitTypes::Protoss_Pylon, current_frame));
			action_frames_best = res;
			max_frame = action_frames_best.back().second;
		}
	}

	// build probe 
	if (probe_left_frame == 0 && current_mineral >= BWAPI::UnitTypes::Protoss_Probe.mineralPrice() && available_supply > 0)
	{
		BuildActionFrames res = searchCannonBuild2(depth + 1, max_depth, current_frame, max_frame,
			current_mineral - BWAPI::UnitTypes::Protoss_Probe.mineralPrice(), available_supply,
			BWAPI::UnitTypes::Protoss_Probe.buildTime(), mine_num,
			pylon_left_frame, pylon_num, pylon_need,
			forge_left_frame, forge_num, forge_need,
			cannon_left_frame, cannon_num, cannon_need); 
		if ((!res.empty() &&
			res.back().first == BWAPI::UnitTypes::Protoss_Photon_Cannon &&
			(action_frames_best.empty() || res.back().second < action_frames_best.back().second)))
		{
			res.insert(res.begin(), BuildActionFrame(BWAPI::UnitTypes::Protoss_Probe, current_frame));
			action_frames_best = res;
			max_frame = action_frames_best.back().second;
		}
	}

	// no action, but wait for next event 
	int plus_frame = max_frame - current_frame;
	
	// wait for cannon mineral
	if (cannon_left_frame > 0)
		plus_frame = __min(plus_frame, cannon_left_frame); 
	else if (pylon_num > 0 && forge_num > 0 && current_mineral < BWAPI::UnitTypes::Protoss_Photon_Cannon.mineralPrice())
	{
		plus_frame = __min(plus_frame,
			(mine_num == 0) ? plus_frame : int(std::ceil(BWAPI::UnitTypes::Protoss_Photon_Cannon.mineralPrice() - current_mineral) / (Config::Protoss::MineSpeed*mine_num)));
	}

	// wait for forge build or mineral
	if (forge_left_frame > 0)
		plus_frame = __min(plus_frame, forge_left_frame);
	else if (pylon_num > 0 && forge_num < forge_need && current_mineral < BWAPI::UnitTypes::Protoss_Forge.mineralPrice())
	{
		plus_frame = __min(plus_frame,
			(mine_num == 0) ? plus_frame : int(std::ceil(BWAPI::UnitTypes::Protoss_Forge.mineralPrice() - current_mineral) / (Config::Protoss::MineSpeed*mine_num)));
	}

	// wait for pylon build or mineral
	if (pylon_left_frame > 0)
		plus_frame = __min(plus_frame, pylon_left_frame);
	else if (current_mineral < BWAPI::UnitTypes::Protoss_Pylon.mineralPrice())
	{
		plus_frame = __min(plus_frame,
			(mine_num == 0) ? plus_frame : int(std::ceil(BWAPI::UnitTypes::Protoss_Pylon.mineralPrice() - current_mineral) / (Config::Protoss::MineSpeed*mine_num)));
	}

	// wait for probe pruduce or mineral
	if (probe_left_frame > 0)
		plus_frame = __min(plus_frame, probe_left_frame);
	else if (current_mineral < BWAPI::UnitTypes::Protoss_Probe.mineralPrice())
	{
		plus_frame = __min(plus_frame,
			(mine_num == 0) ? plus_frame : int(std::ceil(BWAPI::UnitTypes::Protoss_Probe.mineralPrice() - current_mineral) / (Config::Protoss::MineSpeed*mine_num)));
	}

	// update type nums
	if (cannon_left_frame > 0)
	{
		if (plus_frame >= cannon_left_frame)
			cannon_num += 1; 
		cannon_left_frame -= plus_frame; 
	}
	if (forge_left_frame > 0)
	{
		if (plus_frame >= forge_left_frame)
			forge_num += 1; 
		forge_left_frame -= plus_frame;
	}
	if (pylon_left_frame > 0)
	{
		if (plus_frame >= pylon_left_frame)
		{
			pylon_num += 1; 
			available_supply += BWAPI::UnitTypes::Protoss_Pylon.supplyProvided();
		}
		pylon_left_frame -= plus_frame;
	}
	int old_mine_num = mine_num;
	if (probe_left_frame > 0)
	{
		if (plus_frame >= probe_left_frame)
		{
			available_supply -= BWAPI::UnitTypes::Protoss_Probe.supplyRequired();
			mine_num += 1;
		}
		probe_left_frame -= plus_frame;
	}

	BuildActionFrames res = searchCannonBuild2(depth, max_depth, current_frame + plus_frame, max_frame,
		current_mineral + plus_frame*old_mine_num*Config::Protoss::MineSpeed, available_supply,
		probe_left_frame, mine_num,
		pylon_left_frame, pylon_num, pylon_need,
		forge_left_frame, forge_num, forge_need,
		cannon_left_frame, cannon_num, cannon_need); 
	if ((!res.empty() &&
		res.back().first == BWAPI::UnitTypes::Protoss_Photon_Cannon &&
		(action_frames_best.empty() || res.back().second < action_frames_best.back().second)))
	{
		action_frames_best = res;
		max_frame = action_frames_best.back().second;
	}

	return action_frames_best;
}

/* recover before map analyze units */
void StrategyManager::recoverBeforeMapAnalyzeUnits()
{
	InformationManager& IM = InformationManager::getInstance();
	UnitProbeManager& UPM = UnitProbeManager::getInstance(); 

	// unit probe manager
	std::vector<UnitProbe>& units = UnitProbeManager::getInstance().getBeforeMapAnazlyeUnits();

	for (std::vector<UnitProbe>::iterator iter = units.begin(); iter != units.end();)
	{
		if ((*iter)->getRole() == BEFORE_MAP_ANALYZED_SCOUT)
		{
			(*iter)->clear(); 
			std::pair<Role, BWAPI::Position> role_position = IM.getDesireScoutTarget((*iter)->getUnit()->getPosition()); 
			(*iter)->setRole(role_position.first);
			(*iter)->setTargetPosition(role_position.second); 
		}
		else if ((*iter)->getRole() == BEFORE_MAP_ANALYZED_MINERAL)
		{
			(*iter)->setRole(Role::MINERAL); 
			(*iter)->setTargetRegion(IM.getSelfRegion()); 
			(*iter)->setTargetBaseLocation(IM.getSelfBaseLocation()); 
			(*iter)->setTargetUnit(nullptr); 
			(*iter)->setTargetPosition(BWAPI::Position(-1, -1));
		}
		UPM.onUnitCreate(*iter); 
		IM.onUnitCreate(*iter);
		iter = units.erase(iter); 
	}

	// building manager
	std::vector<BWAPI::Unit>& buildings = BuildingManager::getInstance().getMapUnAnalyzeBuildings(); 
	for (std::vector<BWAPI::Unit>::iterator iter = buildings.begin(); iter != buildings.end();)
	{
		BuildingManager::getInstance().onUnitCreate(*iter); 
		IM.onUnitCreate(*iter); 
		iter = buildings.erase(iter); 
	}
}


/* replenish minimum attack */
void StrategyManager::replenishMinimumAttack(BWTA::Region* region)
{
	InformationManager& IM = InformationManager::getInstance(); 

	if (region == IM.getSelfRegion())
		return; 

	// haven't found enemy start, don't do
	if (!IM.isEnemyStartLocationFound())
		return; 
	// no enemy, don't do
	if (IM.getRegionEnemyBuildings(region).empty() &&
		IM.getRegionEnemyUnits(region).empty())
	{
		return; 
	}
	// have minimum attack, don't do
	int num = IM.getRegionSelfUnits(region).size(); 
	if (num >= Config::StrategyManager::MinAttackNum)
		return; 

	BWAPI::Unit target_unit = nullptr; 
	BWAPI::Position target_position(-1, -1); 
	IM.getRegionAttackTarget(region, target_unit, target_position); 
	if (target_unit)
		target_position = target_unit->getPosition(); 

	if (!target_position.isValid())
		return; 

	int need_num = Config::StrategyManager::MinAttackNum - num; 
	std::map<int, UnitProbe> replenish_units; 

	for (auto& u : UnitProbeManager::getInstance().getUnits())
	{
		if (!(u->getRole() == ATTACK || u->getRole() == MINERAL))
			continue; 

		if (u->getTargetRegion() == region)
			continue; 

		if (!(u->getTargetRegion() == IM.getSelfRegion() && IM.getRegionSelfUnits(u->getTargetRegion(), MINERAL).size() >= 3) &&
			!(IM.getRegionSelfUnits(u->getTargetRegion()).size() > Config::StrategyManager::MinAttackNum))
		{
			continue; 
		}

		int d = u->getUnit()->getDistance(target_position); 
		if (replenish_units.size() < need_num ||
			(*replenish_units.rbegin()).first < d)
		{
			replenish_units.insert(std::pair<int, UnitProbe>(d, u)); 
		}

		if (replenish_units.size() > need_num)
			replenish_units.erase(--replenish_units.end()); 
			
	}

	for (auto& d_u : replenish_units)
	{
		IM.removeRegionSelfUnit(d_u.second);
		d_u.second->clear(); 
		d_u.second->setRole(Role::ATTACK); 
		d_u.second->setTargetRegion(region); 
		IM.addRegionSelfUnit(d_u.second); 
	}
}
