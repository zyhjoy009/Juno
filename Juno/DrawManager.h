
#pragma once


#include <BWAPI.h>
#include "StrategyManager.h"


class DrawManager
{
public:

	DrawManager() {}; 

	static DrawManager& getInstance() { static DrawManager dm; return dm; }; 

	// draw attack behavior
	void drawAttack(const BWAPI::Unit& unit, const BWAPI::Unit& target, bool re);

	void drawMove(const BWAPI::Unit& unit, const BWAPI::Position& p); 

	void drawRightClick(const BWAPI::Unit& unit, const BWAPI::Unit& target); 

	void drawBlocksRewards(const BWAPI::Unit& unit, const std::vector<std::vector<double>>& rewards); 

	void drawPlanBuilding(const BWAPI::TilePosition& location, const BWAPI::UnitType& unit_type); 

	void drawAttackTarget(const BWAPI::Unit& unit, const BWAPI::Position& position); 

	void drawSearchBuild(const BuildActionFrames& action_frames);

	void drawTime(int x, int y, int frames, std::string str, double micro_sec); 

};