#include "UnitProbeInterface.h"
#include <assert.h>
#include "ConvertManager.h"
#include "Config.h"
#include "InformationManager.h"
#include "InformationManager.h"
#include "Timer.h"
#include "DrawManager.h"
#include "Timer.h"
#include "Juno.h"



/* update unit action */
void UnitProbeInterface::onFrame()
{
	timer_display_y += 10;

	if (BWAPI::Broodwar->getFrameCount() - last_frame < Config::UnitProbeInterface::FrequencyFrame)
		return; 

	if (timer.getNoStopElapsedTimeInMicroSec() + Config::Timer::UnitProbeInterfaceMaxMicroSec > Config::Timer::TotalMicroSec)
		return;
	Timer tt;
	tt.start();

	last_frame = BWAPI::Broodwar->getFrameCount(); 

	switch (role)
	{
	case Role::ATTACK:
		attack(); 
		break; 
	case Role::DEFEND:
		defend(); 
		break; 
	case Role::MINERAL:
		mineral(); 
		break; 
	case Role::BUILD:
		build(); 
		break; 
	case Role::SCOUT:
	case Role::SCOUT_START_LOCATION:
	case Role::SCOUT_BASE_LOCATION:
		scout(); 
		break; 
	case Role::BEFORE_MAP_ANALYZED_SCOUT:
		smartMove(BWAPI::Position(BWAPI::Broodwar->mapWidth() * 32 / 2, BWAPI::Broodwar->mapHeight() * 32 / 2)); 
		// TODO!!! remember to do something after map analyze 
		break; 
	case Role::BEFORE_MAP_ANALYZED_MINERAL:
		smartRightClick(unit->getClosestUnit(BWAPI::Filter::IsMineralField, 32 * 8)); 
		break; 
	}

	tt.stop();
	DrawManager::getInstance().drawTime(10, timer_display_y, Config::UnitProbeInterface::FrequencyFrame,
		"UnitProbeInterface", tt.getElapsedTimeInMicroSec());

}


/* get distance to a group of other units */
int UnitProbeInterface::getDistances(const std::vector<BWAPI::Unit>& others)
{
	int d = 0; 
	for (auto& u : others)
	{
		d += unit->getDistance(u); 
	}
	return d; 
}


/* attack action */
void UnitProbeInterface::attack()
{
	assert(target_region);
	InformationManager::getInstance().getRegionAttackTarget(target_region, target_unit, target_position);

	if (target_unit != nullptr && target_unit->exists())
		stupidAttack(target_unit);
	else if (target_position.isValid())
		stupidMove(target_position); 
}

/* defend action */
void UnitProbeInterface::defend()
{
	InformationManager& IM = InformationManager::getInstance(); 

	BWTA::Region* region = InformationManager::getInstance().getRegion(unit->getPosition()); 
	// move out of self region
	if (region != target_region)
	{
		target_unit = nullptr; 
		std::vector<BWAPI::Unit> depots = IM.getRegionSelfBuildings(target_region, BWAPI::UnitTypes::Protoss_Nexus); 
		if (!depots.empty())
			target_position = depots[0]->getPosition();
		else
			target_position = target_region->getCenter(); 
		stupidMove(target_position); 
	}
	else
	{
		// attack nearest enemy directly 
		BWAPI::Unit eu = unit->getClosestUnit(BWAPI::Filter::IsEnemy && !BWAPI::Filter::IsFlying, 32 * 8); 
		if (eu)
		{
			target_unit = eu; 
			target_position = BWAPI::Position(-1, -1); 
			smartAttack(eu); 
		}
		// or move back to base 
		else
		{
			target_unit = nullptr;
			std::vector<BWAPI::Unit> depots = IM.getRegionSelfBuildings(target_region, BWAPI::UnitTypes::Protoss_Nexus);
			if (!depots.empty())
				target_position = depots[0]->getPosition();
			else
				target_position = target_region->getCenter();
			smartMove(target_position);
		}
	}
}

/* mineral action */
void UnitProbeInterface::mineral()
{
	if (unit->getClosestUnit(BWAPI::Filter::IsEnemy && BWAPI::Filter::CanAttack, 32 * 3) != nullptr)
	{
		stupidMove(BWAPI::Position(-1, -1)); 
	}
	else
	{
		if (unit->isGatheringMinerals())
			return;
		else
		{
			std::vector<BWAPI::Unit> depots = InformationManager::getInstance().getRegionSelfBuildings(target_region,
				BWAPI::UnitTypes::Protoss_Nexus);
			if (!depots.empty())
			{
				BWAPI::Unit m = depots[0]->getClosestUnit(BWAPI::Filter::IsMineralField, 32 * 8);
				if (m)
					smartRightClick(m);
			}
		}
	}
	return; 

	int hp = unit->getHitPoints() + unit->getShields();
	int hp_max = unit->getType().maxHitPoints() + unit->getType().maxShields();

	if (hp < hp_max / 2 && 
		unit->getClosestUnit(BWAPI::Filter::IsEnemy && BWAPI::Filter::CanAttack, 32*3) != nullptr)
	{
		stupidMove(InformationManager::getInstance().getSelfRegion()->getCenter()); 
		return; 
	}

	// under attack try to avoid enemy
	if (unit->isUnderAttack())
	{
		if (unit->isStuck())
		{
			if (unit->isCarryingGas() || unit->isGatheringMinerals())
				smartReturnCargo(); 
			else
			{
				BWAPI::Unit m = unit->getClosestUnit(BWAPI::Filter::IsMineralField); 
				smartRightClick(m); 
			}
		}
		else
		{
			if (unit->canGather())
				stupidMoveToMineral();
			else
				stupidReturnCargo();
		}
	}
	// otherwise just mine
	{
		if (unit->isGatheringMinerals())
			return; 
		else
		{
			std::vector<BWAPI::Unit> depots = InformationManager::getInstance().getRegionSelfBuildings(target_region, 
				BWAPI::UnitTypes::Protoss_Nexus); 
			if (!depots.empty())
			{
				BWAPI::Unit m = depots[0]->getClosestUnit(BWAPI::Filter::IsMineralField, 32*8); 
				if (m)
					smartRightClick(m); 
			}
		}
	}
}


/* scout action */
void UnitProbeInterface::scout()
{
	updateScoutTarget(); 
	stupidMove(target_position); 
}


/* build action */
void UnitProbeInterface::build()
{
	BuildingManager& BM = BuildingManager::getInstance(); 

	assert(plan_build_data_pointer != nullptr); 

	/* first move to location */
	if (unit->getDistance(BWAPI::Position(plan_build_data_pointer->location)) > Config::UnitProbeInterface::BuilderCloseDistance)
	{
		stupidMove(BWAPI::Position(plan_build_data_pointer->location));
		return;
	}

	/* already built */
	for (auto& b : BWAPI::Broodwar->getUnitsOnTile(plan_build_data_pointer->location, BWAPI::Filter::IsBuilding && BWAPI::Filter::IsOwned))
	{
		if (b->getType() == plan_build_data_pointer->building_type)
		{
			BuildingManager::getInstance().removePlanBuilding(plan_build_data_pointer);
			return; 
		}
	}

	/* not enough money */
	if (BWAPI::Broodwar->self()->minerals() < plan_build_data_pointer->building_type.mineralPrice())
		return;

	/* need new location */
	bool res = unit->build(plan_build_data_pointer->building_type, plan_build_data_pointer->location); 

	if (!res)
	{
		if (!BM.canBuildHere(plan_build_data_pointer->location, plan_build_data_pointer->building_type))
		{
			BWAPI::TilePosition tp = BM.getNearBuildLocation(plan_build_data_pointer->location,
				plan_build_data_pointer->building_type,
				Config::UnitProbeInterface::BuildNearMaxRange);
			/* fine new location, change data and build */
			if (tp.isValid())
			{
				plan_build_data_pointer->location = tp;
				unit->build(plan_build_data_pointer->building_type, plan_build_data_pointer->location);
			}
			/* otherwise, remove plan build data and recover builder */
			else
			{
				BuildingManager::getInstance().removePlanBuilding(plan_build_data_pointer);
				return;
			}
		}
	}
}


/* stupid attack behavior */
/* !!! TODO: LET MAP FUNCTION DEAL WITH OUT OF RANGE CASE */
void UnitProbeInterface::stupidAttack(const BWAPI::Unit& target)
{
//	Timer timer; 
	int radius = Config::UnitProbeInterface::BlockRadius; 
	int span = Config::UnitProbeInterface::BlockSpan; 

	// nearby no enemy, attack directly 
	BWAPI::Unitset nearby_enemies = BWAPI::Broodwar->getUnitsInRectangle(unit->getPosition() - BWAPI::Position((radius + 1)*span, (radius + 1)*span),
		unit->getPosition() + BWAPI::Position((radius + 1)*span, (radius + 1)*span),
		BWAPI::Filter::IsEnemy && BWAPI::Filter::CanAttack && 
		!BWAPI::Filter::IsConstructing && !BWAPI::Filter::IsRepairing && !BWAPI::Filter::IsGatheringGas && !BWAPI::Filter::IsGatheringMinerals); 

	int hp = unit->getHitPoints() + unit->getShields();
	int hp_max = unit->getType().maxHitPoints() + unit->getType().maxShields();

	if (nearby_enemies.empty() ||
		(nearby_enemies.size() == 1 && (*nearby_enemies.begin()) == target))
	{
		if (hp > hp_max / 2)
		{
			smartAttack(target);
			return;
		}
		else if (target->getTarget() != unit && target->getOrderTarget() != unit)
		{
			smartAttack(target);
			return;
		}
	}

	// nearby selfs more than enemies attack automatically 
	BWAPI::Unitset nearby_friends = BWAPI::Broodwar->getUnitsInRectangle(unit->getPosition() - BWAPI::Position((radius + 1)*span, (radius + 1)*span),
		unit->getPosition() + BWAPI::Position((radius + 1)*span, (radius + 1)*span),
		BWAPI::Filter::IsOwned && BWAPI::Filter::CanAttack);

	if (nearby_friends.size() > 1.1*nearby_enemies.size())
	{
		if (hp > hp_max / 2)
		{
			smartAttackMove(target->getPosition()); 
			return; 
		}
	}


	std::vector<std::vector<double>> target_rewards; 
	std::vector<std::vector<double>> enemy_rewards; 
	std::vector<std::vector<double>> obstacle_rewards;

//	timer.start(); 
	if (unit->getGroundWeaponCooldown() < (unit->getDistance(target) / unit->getType().topSpeed()))
	{
		target_rewards = getBlocksTargetRewards(unit->getPosition(), target); 
	}
	else
	{
		target_rewards = std::vector<std::vector<double>>(2 * radius + 1, std::vector<double>(2 * radius + 1, 0));
	}
//	timer.stop(); 
	double dur_target = timer.getElapsedTimeInMicroSec(); 
	double target_factor = 1;
	if (hp < hp_max / 2)
		target_factor = 0.5;


//	timer.start();
	enemy_rewards = getBlocksEnemyRewards(unit->getPosition(), target);
//	timer.stop();
	double dur_enemy = timer.getElapsedTimeInMicroSec();

//	timer.start();
	obstacle_rewards = getBlocksObstacleRewards(unit->getPosition());
//	timer.stop();
	double dur_obstacle = timer.getElapsedTimeInMicroSec();

//	timer.start();
	std::vector<std::vector<double>> sum_rewards = target_rewards*target_factor + enemy_rewards + obstacle_rewards; 
//	timer.stop();
	double dur_sum = timer.getElapsedTimeInMicroSec();

//	timer.start();
	std::vector<std::vector<double>> iter_rewards = iterate(sum_rewards, Config::UnitProbeInterface::IterateTime);
//	timer.stop();
	double dur_iterate = timer.getElapsedTimeInMicroSec();

	std::pair<int, int> dir = getAction(unit->getPosition(), iter_rewards);

	bool is_target_nearby = target_rewards[radius][radius] > 0; 
	bool no_enemy_nearby = enemy_rewards[radius][radius] >= 0; 

	// target nearby, no enemy nearby, stop moving
	if (is_target_nearby && no_enemy_nearby && 
		dir.first == 0 && dir.second == 0 && 
		hp > hp_max / 2)
	{
		// high hp -> attack
		smartAttack(target);
	}
	else 
	{
		BWAPI::Position move_position = unit->getPosition() + BWAPI::Position(dir.first*span, dir.second*span);
		smartMove(move_position); 
	}
	if (unit->isSelected())
		DrawManager::getInstance().drawBlocksRewards(unit, sum_rewards);
}


/* stupid move behavior */
void UnitProbeInterface::stupidMove(const BWAPI::Position& target)
{
	int radius = Config::UnitProbeInterface::BlockRadius;
	int span = Config::UnitProbeInterface::BlockSpan;

	// nearby no enemy, attack directly 
	BWAPI::Unitset nearby_enemies = BWAPI::Broodwar->getUnitsInRectangle(unit->getPosition() - BWAPI::Position((radius + 1)*span, (radius + 1)*span),
		unit->getPosition() + BWAPI::Position((radius + 1)*span, (radius + 1)*span),
		BWAPI::Filter::IsEnemy && BWAPI::Filter::CanAttack && 
		!BWAPI::Filter::IsConstructing && !BWAPI::Filter::IsRepairing && !BWAPI::Filter::IsGatheringGas && !BWAPI::Filter::IsGatheringMinerals);
	
	if (nearby_enemies.empty())
	{
		smartMove(target);
		return;
	}

	int hp = unit->getHitPoints() + unit->getShields();
	int hp_max = unit->getType().maxHitPoints() + unit->getType().maxShields();

	// nearby selfs more than enemies attack automatically 
	BWAPI::Unitset nearby_friends = BWAPI::Broodwar->getUnitsInRectangle(unit->getPosition() - BWAPI::Position((radius + 1)*span, (radius + 1)*span),
		unit->getPosition() + BWAPI::Position((radius + 1)*span, (radius + 1)*span),
		BWAPI::Filter::IsOwned && BWAPI::Filter::CanAttack);

	if (nearby_friends.size() > 1.1*nearby_enemies.size())
	{
		if (hp > hp_max / 2)
		{
			smartAttackMove(target);
			return;
		}
	}


	std::vector<std::vector<double>> target_rewards;
	std::vector<std::vector<double>> enemy_rewards;
	std::vector<std::vector<double>> obstacle_rewards;

	target_rewards = getBlocksTargetRewards(unit->getPosition(), target);
	enemy_rewards = getBlocksEnemyRewards(unit->getPosition());
	obstacle_rewards = getBlocksObstacleRewards(unit->getPosition());

	std::vector<std::vector<double>> sum_rewards = target_rewards + enemy_rewards + obstacle_rewards;
	std::vector<std::vector<double>> iter_rewards = iterate(sum_rewards, Config::UnitProbeInterface::IterateTime);
	std::pair<int, int> dir = getAction(unit->getPosition(), iter_rewards);
	BWAPI::Position move_position = unit->getPosition() + BWAPI::Position(dir.first*span, dir.second*span);
	smartMove(move_position);

	if (unit->isSelected())
		DrawManager::getInstance().drawBlocksRewards(unit, sum_rewards);
}

/* stupid move to mineral behavior */
void UnitProbeInterface::stupidMoveToMineral()
{
	int radius = Config::UnitProbeInterface::BlockRadius;
	int span = Config::UnitProbeInterface::BlockSpan;

	int hp = unit->getHitPoints() + unit->getShields();
	int hp_max = unit->getType().maxHitPoints() + unit->getType().maxShields();

	std::vector<std::vector<double>> mineral_rewards; 
	std::vector<std::vector<double>> enemy_rewards;
	std::vector<std::vector<double>> obstacle_rewards;

	if (hp < 0.5*hp_max)
	{
		mineral_rewards = std::vector<std::vector<double>>(2 * radius + 1, std::vector<double>(2 * radius + 1, 0));
	}
	else
	{
		mineral_rewards = getBlocksMineralRewards(unit->getPosition()); 
	}

	enemy_rewards = getBlocksEnemyRewards(unit->getPosition());
	obstacle_rewards = getBlocksObstacleRewards(unit->getPosition());

	std::vector<std::vector<double>> sum_rewards = mineral_rewards + enemy_rewards + obstacle_rewards;
	std::vector<std::vector<double>> iter_rewards = iterate(sum_rewards, Config::UnitProbeInterface::IterateTime);
	std::pair<int, int> dir = getAction(unit->getPosition(), iter_rewards);

	bool no_enemy_nearby = enemy_rewards[radius][radius] >= 0; 
	bool is_minearl_nearby = mineral_rewards[radius][radius] > 0; 

	// nearby mineral, and no enemy, and stop moving
	if (is_minearl_nearby && no_enemy_nearby && dir.first == 0 && dir.second == 0)
	{
		if (hp > hp_max / 2)
		{
			smartRightClick(unit->getClosestUnit(BWAPI::Filter::IsMineralField, radius*1.5)); 
		}
		else if (unit->getClosestUnit(BWAPI::Filter::IsEnemy && BWAPI::Filter::CanAttack, radius*2) == nullptr)
		{
			smartRightClick(unit->getClosestUnit(BWAPI::Filter::IsMineralField, radius*1.5));
		}
	}
	else
	{
		BWAPI::Position move_position = unit->getPosition() + BWAPI::Position(dir.first*span, dir.second*span);
		smartMove(move_position);
	}
	if (unit->isSelected())
		DrawManager::getInstance().drawBlocksRewards(unit, sum_rewards);
}

/* stupid return cargo behavior */
void UnitProbeInterface::stupidReturnCargo()
{
	int radius = Config::UnitProbeInterface::BlockRadius;
	int span = Config::UnitProbeInterface::BlockSpan;

	int hp = unit->getHitPoints() + unit->getShields();
	int hp_max = unit->getType().maxHitPoints() + unit->getType().maxShields();

	std::vector<std::vector<double>> depot_rewards;
	std::vector<std::vector<double>> enemy_rewards;
	std::vector<std::vector<double>> obstacle_rewards;

	std::vector<BWAPI::Unit> depots = BuildingManager::getInstance().getBuildingUnits(BWAPI::UnitTypes::Protoss_Nexus); 
	BWAPI::Unit depot = (depots.empty()) ? nullptr : depots[0]; 

	if (depots.empty())
	{
		depot_rewards = std::vector<std::vector<double>>(2 * radius + 1, std::vector<double>(2 * radius + 1, 0));
	}
	else
	{
		depot_rewards = getBlocksTargetRewards(unit->getPosition(), depot); 
	}

	enemy_rewards = getBlocksEnemyRewards(unit->getPosition());
	obstacle_rewards = getBlocksObstacleRewards(unit->getPosition());

	std::vector<std::vector<double>> sum_rewards = depot_rewards + enemy_rewards + obstacle_rewards;
	std::vector<std::vector<double>> iter_rewards = iterate(sum_rewards, Config::UnitProbeInterface::IterateTime);
	std::pair<int, int> dir = getAction(unit->getPosition(), iter_rewards);

	bool no_enemy_nearby = enemy_rewards[radius][radius] >= 0;
	bool is_depot_nearby = depot_rewards[radius][radius] > 0;

	// nearby mineral, and no enemy, and stop moving
	if (is_depot_nearby && no_enemy_nearby && dir.first == 0 && dir.second == 0)
	{
		if (hp > hp_max / 2)
		{
			smartRightClick(depot);
		}
		else if (unit->getClosestUnit(BWAPI::Filter::IsEnemy && BWAPI::Filter::CanAttack, radius * 2) == nullptr)
		{
			smartRightClick(depot);
		}
	}
	else
	{
		BWAPI::Position move_position = unit->getPosition() + BWAPI::Position(dir.first*span, dir.second*span);
		smartMove(move_position);
	}

	if (unit->isSelected())
		DrawManager::getInstance().drawBlocksRewards(unit, sum_rewards);
}


/* smart attack */
void UnitProbeInterface::smartAttack(const BWAPI::Unit& u)
{
	if (u == NULL)
		return;

	// if we have issued a command to this unit already this frame, ignore this one
	if (BWAPI::Broodwar->getFrameCount() - unit->getLastCommandFrame() < Config::UnitProbeInterface::FrequencyFrame - 1)
	{
		return;
	}

	// get the unit's current command
	BWAPI::UnitCommand currentCommand(unit->getLastCommand());

	// same command
	if (currentCommand.getType() == BWAPI::UnitCommandTypes::Attack_Unit &&	currentCommand.getTarget() == u
		&& (BWAPI::Broodwar->getFrameCount() - unit->getLastCommandFrame() < 21))
	{
		return;
	}

	// if nothing prevents it, attack the target
	bool re = unit->attack(u);

	DrawManager::getInstance().drawAttack(unit, u, re); 

}

/* smart move */
void UnitProbeInterface::smartMove(const BWAPI::Position& p)
{
	// if we have issued a command to this unit already this frame, ignore this one
	if (unit->getLastCommand().getType() != BWAPI::UnitCommandTypes::None && 
		BWAPI::Broodwar->getFrameCount() - unit->getLastCommandFrame() < Config::UnitProbeInterface::FrequencyFrame - 1)
	{
		return;
	}

	// get the unit's current command
	BWAPI::UnitCommand currentCommand(unit->getLastCommand());

	// if we've already told this unit to attack this target, ignore this command
/*	if ((currentCommand.getType() == BWAPI::UnitCommandTypes::Move)
		&& (currentCommand.getTargetPosition() == p)
		&& (BWAPI::Broodwar->getFrameCount() - unit->getLastCommandFrame() < 20)
		&& (unit->isMoving())) */
	// may be unit is stuck by something so that it does't move, so we shouldn't give too frequent command
	if ((currentCommand.getType() == BWAPI::UnitCommandTypes::Move)
		&& (currentCommand.getTargetPosition() == p)
		&& (BWAPI::Broodwar->getFrameCount() - unit->getLastCommandFrame() < 20))
	{
		return;
	}

	// if nothing prevents it, attack the target
	unit->move(p);
	
	DrawManager::getInstance().drawMove(unit, p); 

}

/* smart right click */
void UnitProbeInterface::smartRightClick(const BWAPI::Unit& u)
{
	// if we have issued a command to this unit already this frame, ignore this one
	if (BWAPI::Broodwar->getFrameCount() - unit->getLastCommandFrame() < Config::UnitProbeInterface::FrequencyFrame - 1)
	{
		return;
	}

	// get the unit's current command
	BWAPI::UnitCommand currentCommand(unit->getLastCommand());

	// if we've already told this unit to attack this target, ignore this command
	if ((currentCommand.getType() == BWAPI::UnitCommandTypes::Right_Click_Unit)
		&& (currentCommand.getTarget() == u)
		&& (BWAPI::Broodwar->getFrameCount() - unit->getLastCommandFrame() < 20))
	{
		return;
	}

	// if nothing prevents it, attack the target
	unit->rightClick(u);

	DrawManager::getInstance().drawRightClick(unit, u); 
}

int UnitProbeInterface::getDistances(const BWAPI::Position& p, const BWAPI::Unitset& unit_set)
{
	int d = 0; 
	for (auto& u : unit_set)
		d += u->getDistance(p); 
	return d; 
}

int UnitProbeInterface::getDistances(const BWAPI::Position& p, const std::vector<BWAPI::TilePosition>& tp_set)
{
	int d = 99999; 
	for (auto& tp : tp_set)
	{
		if (BWAPI::TilePosition(p) == tp)
		{
			d = -1;
			break;
		}
		else
			d = __min(d, int(p.getDistance(BWAPI::Position(tp))));
	}
	return d; 
}


// enemy rewards don't consider target
std::vector<std::vector <double>> UnitProbeInterface::getBlocksEnemyRewards(const BWAPI::Position& p, const BWAPI::Unit& target)
{
	int radius = Config::UnitProbeInterface::BlockRadius;
	int span = Config::UnitProbeInterface::BlockSpan;
	int reward = Config::UnitProbeInterface::EnemyReward; 
	std::vector<std::vector<double>> map(2 * radius + 1,
		std::vector<double>(2 * radius + 1, 0)); 


	for (auto& eu : BWAPI::Broodwar->getUnitsInRectangle(unit->getPosition() - BWAPI::Position((radius + 1)*span, (radius + 1)*span),
		unit->getPosition() + BWAPI::Position((radius + 1)*span, (radius + 1)*span),
		BWAPI::Filter::IsEnemy && BWAPI::Filter::CanAttack  && 
		!BWAPI::Filter::IsConstructing && !BWAPI::Filter::IsRepairing && !BWAPI::Filter::IsGatheringGas && !BWAPI::Filter::IsGatheringMinerals))
	{
		if (eu == target)
			continue; 

		if (!eu->isCompleted() || eu->isMorphing())
			continue; 

		int start_i = std::ceil(double(eu->getPosition().x - p.x) / span) - 1;
		int end_i = start_i + 1;
		int start_j = std::ceil(double(eu->getPosition().y - p.y) / span) - 1;
		int end_j = start_j + 1;
		for (int i = __max(-radius, __min(radius, start_i));
			i <= __max(-radius, __min(radius, end_i)); i++)
		{
			for (int j = __max(-radius, __min(radius, start_j));
				j <= __max(-radius, __min(radius, end_j)); j++)
			{
				map[i + radius][j + radius] += reward;
			}
		}
	}
	return map; 
}

std::vector<std::vector<double>> UnitProbeInterface::getBlocksObstacleRewards(const BWAPI::Position& pos)
{
	int radius = Config::UnitProbeInterface::BlockRadius;
	int span = Config::UnitProbeInterface::BlockSpan;
	int reward_obst = Config::UnitProbeInterface::ObstacleReward;
	int reward_unwalk = Config::UnitProbeInterface::UnWalkableReward; 
	std::vector<std::vector<double>> map(2 * radius + 1,
		std::vector<double>(2 * radius + 1, 0));


	BWAPI::TilePosition start(pos - BWAPI::Position((radius + 1)*span, (radius + 1)*span));
	BWAPI::TilePosition end(pos + BWAPI::Position((radius + 1)*span, (radius + 1)*span));
	int num = 0;
	// now I want to statistic block unwalkable area and small/medium/large objects 
	// unwalkable area
	for (int ii = start.x; ii <= end.x; ii++)
	{
		for (int jj = start.y; jj <= end.y; jj++)
		{
			BWAPI::TilePosition tp(ii, jj);
			if (!tp.isValid() ||
				!BWAPI::Broodwar->isWalkable(BWAPI::WalkPosition(tp)))
			{
				int start_i = std::ceil(double(tp.x*32 - pos.x) / span) - 1;
				int end_i = start_i + 1;
				int start_j = std::ceil(double(tp.y*32 - pos.y) / span) - 1;
				int end_j = start_j + 1;
				for (int i = __max(-radius, __min(radius, start_i));
					i <= __max(-radius, __min(radius, end_i)); i++)
				{
					for (int j = __max(-radius, __min(radius, start_j));
						j <= __max(-radius, __min(radius, end_j)); j++)
					{
						map[i + radius][j + radius] += reward_unwalk;
					}
				}
			}
		}
	}

	// small/medium/large objects, enemy and own
	for (auto& u : BWAPI::Broodwar->getUnitsInRectangle(pos - BWAPI::Position((radius + 1)*span, (radius + 1)*span),
		pos + BWAPI::Position((radius + 1)*span, (radius + 1)*span),
		!BWAPI::Filter::IsFlying && !BWAPI::Filter::IsGatheringGas && !BWAPI::Filter::IsGatheringMinerals))
	{
		if (unit == u)
			continue; 

		if (u->getType() == BWAPI::UnitTypes::Resource_Mineral_Field)
			BWAPI::Broodwar->sendText("mineral field"); 

		if (u->getType() == BWAPI::UnitTypes::Resource_Vespene_Geyser)
			BWAPI::Broodwar->sendText("geyser"); 

/*		int factor_size = 1; 
		if (u->getType().size() == BWAPI::UnitSizeTypes::Small)
			factor_size = 1;
		else if (u->getType().size() == BWAPI::UnitSizeTypes::Medium)
			factor_size = 2;
		else if (u->getType().size() == BWAPI::UnitSizeTypes::Large || 
			u->getType().size() == BWAPI::UnitSizeTypes::Independent)
			factor_size = 4;  */
		int start_i = std::ceil(double(u->getLeft() - pos.x) / span) - 1;
		int end_i = std::ceil(double(u->getRight() - pos.x) / span);
		int start_j = std::ceil(double(u->getTop() - pos.y) / span) - 1;
		int end_j = std::ceil(double(u->getBottom() - pos.y) / span);
		for (int i = __max(-radius, __min(radius, start_i));
			i <= __max(-radius, __min(radius, end_i)); i++)
		{
			for (int j = __max(-radius, __min(radius, start_j));
				j <= __max(-radius, __min(radius, end_j)); j++)
			{
				int bx = pos.x + (i - 1)*span; 
				int by = pos.y + (j - 1)*span; 
				int bw = 2 * span, bh = 2 * span; 
				int area = getOverwhelmArea(u->getLeft()/32, u->getTop()/32, u->getType().tileWidth(), u->getType().tileHeight(),
					bx/32, by/32, bw/32, bh/32); 
				map[i + radius][j + radius] += reward_obst*area; 
				// map[i + radius][j + radius] += reward_obst*factor_size;
			}
		}
	}
	return map;
}

std::vector<std::vector<double>> UnitProbeInterface::getBlocksTargetRewards(const BWAPI::Position& p, const BWAPI::Unit& target)
{
	int radius = Config::UnitProbeInterface::BlockRadius; 
	int span = Config::UnitProbeInterface::BlockSpan; 
	int reward = Config::UnitProbeInterface::TargetReward;
	std::vector<std::vector<double>> map(2 * radius + 1,
		std::vector<double>(2 * radius + 1, 0));
	// target close
	BWAPI::Position target_pos = target->getPosition(); 
	if (target_pos.x >= p.x - (radius + 1)*span &&
		target_pos.x <= p.x + (radius + 1)*span &&
		target_pos.y >= p.y - (radius + 1)*span &&
		target_pos.y <= p.y + (radius + 1)*span)
	{
		int start_i = std::ceil(double(target->getLeft() - p.x) / span)-1; 
		int end_i = std::ceil(double(target->getRight() - p.x) / span);
		int start_j = std::ceil(double(target->getTop() - p.y) / span) - 1;
		int end_j = std::ceil(double(target->getBottom() - p.y) / span);
		for (int i = __max(-radius, __min(radius, start_i)); 
			i <= __max(-radius, __min(radius, end_i)); i++)
		{
			for (int j = __max(-radius, __min(radius, start_j)); 
				j <= __max(-radius, __min(radius, end_j)); j++)
			{
				map[i + radius][j + radius] = reward;
			}
		}
	}
	// target far
	else  
	{
		return getBlocksTargetRewards(p, target->getPosition()); 
	}
	return map;
}

std::vector<std::vector<double>> UnitProbeInterface::getBlocksTargetRewards(const BWAPI::Position& p, const BWAPI::Position& target)
{
	int radius = Config::UnitProbeInterface::BlockRadius;
	int span = Config::UnitProbeInterface::BlockSpan;
	int reward = Config::UnitProbeInterface::TargetReward; 
	std::vector<std::vector<double>> map(2 * radius + 1,
		std::vector<double>(2 * radius + 1, 0));

	if (!p.isValid())
		return map; 

	// target close 
	if (p.getDistance(target) <= 256 - 32)
	{
		int x1 = std::ceil(double(target.x - p.x) / span); 
		int y1 = std::ceil(double(target.y - p.y) / span);
		x1 = __min(radius, __max(-radius, x1));
		y1 = __min(radius, __max(-radius, y1));
		int x0 = __min(radius, __max(-radius, x1 - 1));
		int y0 = __min(radius, __max(-radius, y1 - 1)); 
		map[x0 + radius][y0 + radius] = reward;
		map[x0 + radius][y1 + radius] = reward;
		map[x1 + radius][y0 + radius] = reward;
		map[x1 + radius][y1 + radius] = reward;
	}
	else
	{
		// target and current position are in same Region, use line
		BWTA::Region* region_p = InformationManager::getInstance().getRegion(p);
		assert(region_p); 

		BWTA::Region* region_target = InformationManager::getInstance().getRegion(target);
//		assert(region_target); 

		if (region_p == region_target)
		{
			int start_i = std::ceil(double(target.x - p.x) / span) - 1;
			int end_i = start_i + 1;
			int start_j = std::ceil(double(target.y - p.y) / span) - 1;
			int end_j = start_j + 1;
			for (int i = __max(-radius, __min(radius, start_i));
				i <= __max(-radius, __min(radius, end_i)); i++)
			{
				for (int j = __max(-radius, __min(radius, start_j));
					j <= __max(-radius, __min(radius, end_j)); j++)
				{
					map[i + radius][j + radius] = reward;
				}
			}
		}
		// target and current position are in different Region
		else
		{
			; // nothing
		}
	}
	return map; 
}

std::vector<std::vector<double>> UnitProbeInterface::getBlocksMineralRewards(const BWAPI::Position& p)
{
	BWAPI::Unitset ms = BWAPI::Broodwar->getUnitsInRadius(p, 32 * 8, BWAPI::Filter::IsMineralField); 
	if (ms.empty())
	{
		BWAPI::Position blp = InformationManager::getInstance().getSelfBaseLocation()->getPosition(); 
		return getBlocksTargetRewards(p, blp); 
	}
	else
	{
		int radius = Config::UnitProbeInterface::BlockRadius;
		int span = Config::UnitProbeInterface::BlockSpan;
		int reward = Config::UnitProbeInterface::MineralReward;
		std::vector<std::vector<double>> map(2 * radius + 1,
			std::vector<double>(2 * radius + 1, 0));

		for (auto& m : ms)
		{
			int start_i = std::ceil(double(m->getPosition().x - p.x) / span) - 1;
			int end_i = start_i + 1;
			int start_j = std::ceil(double(m->getPosition().y - p.y) / span) - 1;
			int end_j = start_j + 1;
			for (int i = __max(-radius, __min(radius, start_i));
				i <= __max(-radius, __min(radius, end_i)); i++)
			{
				for (int j = __max(-radius, __min(radius, start_j));
					j <= __max(-radius, __min(radius, end_j)); j++)
				{
					map[i + radius][j + radius] += reward;
				}
			}
		}
		return map;
	}
}

std::vector<std::vector<double>> operator + (const std::vector<std::vector<double>>& a, const std::vector<std::vector<double>>& b)
{
	assert(a.size() == b.size()); 
	std::vector <std::vector<double>> sum(a.size()); 
	int i = 0; 
	for (std::vector<std::vector<double>>::const_iterator iter_a = a.cbegin(), iter_b = b.cbegin();
		iter_a != a.cend() && iter_b != b.cend(); iter_a++, iter_b++, i++)
	{
		assert(iter_a->size() == iter_b->size()); 
		sum[i] = std::vector<double>(iter_a->size()); 
		int j = 0; 
		for (std::vector<double>::const_iterator iter_aa = iter_a->cbegin(), iter_bb = iter_b->cbegin();
			iter_aa != iter_a->cend(), iter_bb != iter_b->cend(); iter_aa++, iter_bb++, j++)
			sum[i][j] = *iter_aa + *iter_bb; 
	}
	return sum; 
}

std::vector<std::vector<double>> UnitProbeInterface::iterate(const std::vector<std::vector<double>>& r, int times = 3)
{
	std::vector<std::vector<double>> r0(r); 
	for (int t = 0; t < times; t++)
	{
		std::vector<std::vector<double>> r1(r0); 
		for (int i = 0; i < r.size(); i++)
		{
			for (int j = 0; j < r[0].size(); j++)
			{
				double max_value = -1e9; 
				for (int ii = -1; ii <= 1; ii++)
				{
					for (int jj = -1; jj <= 1; jj++)
					{
						if (i + ii<0 || i + ii>=r.size() ||
							j + jj<0 || j + jj>=r[0].size())
							continue; 
						max_value = __max(max_value, r[i][j] + r1[i + ii][j + jj]); 
					}
				}
				r0[i][j] = max_value; 
			}
		}
	}
	return r0;
}


std::pair<int, int> UnitProbeInterface::getAction(const BWAPI::Position& p, const std::vector<std::vector<double>>& rewards)
{
	InformationManager& IM = InformationManager::getInstance(); 
	int span = Config::UnitProbeInterface::BlockSpan;

	int cx = rewards.size() / 2;
	int cy = rewards[0].size() / 2; 
	double best = -99999; 
	int dir_x = 0, dir_y = 0; 

	double angle = unit->getAngle(); 
	double vx = cos(angle), vy = sin(angle); 
	double cross = -99999; 

	for (int i = -1; i <= 1; i++)
	{
		for (int j = -1; j <= 1; j++)
		{
			BWAPI::Position np = p + BWAPI::Position(i*span, j*span); 
			if (!np.isValid() || 
				!BWAPI::Broodwar->isWalkable(BWAPI::WalkPosition(np)) ||
				!BWTA::isConnected(BWAPI::TilePosition(p), BWAPI::TilePosition(np)))
				continue; 

			if (best < rewards[cx + i][cy + j])
			{
				best = rewards[cx + i][cy + j]; 
				dir_x = i; 
				dir_y = j;
			}
			else if (best == rewards[cx + i][cy + j])
			{
				double c = vx*i + vy*j; 
				if (c > cross)
				{
					cross = c; 
					dir_x = i; 
					dir_y = j;
				}
			}
		}
	}
	return std::pair<int, int>(dir_x, dir_y); 
}

std::vector<std::vector<double>> operator * (const std::vector<std::vector<double>>& a, const double& b)
{
	std::vector<std::vector<double>> c(a); 
	for (int i = 0; i < a.size(); i++)
	{
		for (int j = 0; j < a[i].size(); j++)
			c[i][j] *= b; 
	}
	return c; 
}

std::vector<std::vector<double>> operator * (const double& b, const std::vector<std::vector<double>>& a)
{
	return (a*b); 
}

/* update scout target, including no target, or arrive target */
void UnitProbeInterface::updateScoutTarget()
{
	InformationManager& IM = InformationManager::getInstance(); 

	// need new scout target, may be change scout role
	if (!target_position.isValid() || BWAPI::Broodwar->isVisible(BWAPI::TilePosition(target_position)))
	{
		std::pair<Role, BWAPI::Position> role_position = IM.getDesireScoutTarget(unit->getPosition()); 
		target_position = role_position.second; 

		if (role == Role::SCOUT && role_position.first == Role::SCOUT)
			return; 

		else
		{
			switch (role)
			{
			case Role::SCOUT_START_LOCATION:
				IM.removeStartLocationScout(this);
				break;
			case Role::SCOUT_BASE_LOCATION:
				IM.removeBaseLocationScout(this);
				break;
			case Role::SCOUT:
				IM.removeTileScout(this);
				break;
			}

			switch (role_position.first)
			{
			case Role::SCOUT_START_LOCATION:
				role = SCOUT_START_LOCATION; 
				IM.addStartLocationScout(this);
				break;
			case Role::SCOUT_BASE_LOCATION:
				role = SCOUT_BASE_LOCATION; 
				IM.addBaseLocationScout(this);
				break;
			case Role::SCOUT:
				role = SCOUT; 
				IM.addTileScout(this);
				break;
			}

			role = role_position.first;

		}
	}
	// otherwise don't change
}


void UnitProbeInterface::smartAttackMove(const BWAPI::Position& targetPosition)
{
	assert(targetPosition.isValid());

	// if we have issued a command to this unit already this frame, ignore this one
	if (BWAPI::Broodwar->getFrameCount() - unit->getLastCommandFrame() < Config::UnitProbeInterface::FrequencyFrame - 1)
	{
		return;
	}

	// get the unit's current command
	BWAPI::UnitCommand currentCommand(unit->getLastCommand());

	// if we've recently already told this unit to attack this target, ignore
	if (currentCommand.getType() == BWAPI::UnitCommandTypes::Attack_Move &&	currentCommand.getTargetPosition() == targetPosition
		&& (BWAPI::Broodwar->getFrameCount() - unit->getLastCommandFrame() < 20))
	{
		return;
	}

	// if nothing prevents it, attack the target
	unit->attack(targetPosition);

}


/* smart return cargo */
void UnitProbeInterface::smartReturnCargo()
{
	// if we have issued a command to this unit already this frame, ignore this one
	if (BWAPI::Broodwar->getFrameCount() - unit->getLastCommandFrame() < Config::UnitProbeInterface::FrequencyFrame - 1)
	{
		return;
	}

	// get the unit's current command
	BWAPI::UnitCommand currentCommand(unit->getLastCommand());

	// if we've already told this unit to attack this target, ignore this command
	if ((currentCommand.getType() == BWAPI::UnitCommandTypes::Return_Cargo)
		&& (BWAPI::Broodwar->getFrameCount() - unit->getLastCommandFrame() < 20))
	{
		return;
	}

	// if nothing prevents it, attack the target
	unit->returnCargo();

}

int UnitProbeInterface::getOverwhelmArea(int x1, int y1, int w1, int h1,
	int x2, int y2, int w2, int h2)
{
	int left = __max(x1, x2); 
	int right = __min(x1 + w1, x2 + w2); 
	int top = __max(y1, y2); 
	int bottom = __min(y1 + h1, y2 + h2); 

	if (left > right || top > bottom)
		return 0; 

	return (right - left)*(bottom - top); 
}