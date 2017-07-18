
#include "DrawManager.h"
#include "Config.h"


// draw attack behavior
void DrawManager::drawAttack(const BWAPI::Unit& unit, const BWAPI::Unit& target, bool re)
{
	if (!Config::Debug::DrawAttack)
		return; 

	BWAPI::Color c;
	if (re)
	{
		c = BWAPI::Colors::Red;
	}
	else
	{
		c = BWAPI::Colors::Blue;
	}

	BWAPI::Broodwar->registerEvent([unit, target, c](BWAPI::Game*){ 
		if (!unit->exists() || !target->exists())
			return; 
		BWAPI::Broodwar->drawLineMap(unit->getPosition().x, unit->getPosition().y,
		target->getPosition().x, target->getPosition().y, c); 
	},   // action
		nullptr,    // condition
		Config::UnitProbeInterface::FrequencyFrame);  // frames to run

}

void DrawManager::drawMove(const BWAPI::Unit& unit, const BWAPI::Position& p)
{
	if (Config::Debug::DrawMove)
	{
		BWAPI::Broodwar->registerEvent([unit, p](BWAPI::Game*)
		{
			if (!unit->exists())
				return; 
			BWAPI::Broodwar->drawLineMap(unit->getPosition(), p, BWAPI::Colors::Green);
		},
			nullptr,  // condition
			Config::UnitProbeInterface::FrequencyFrame);  // frames to run
	}

}

void DrawManager::drawRightClick(const BWAPI::Unit& unit, const BWAPI::Unit& target)
{
	if (Config::Debug::DrawRightClick)
	{
		BWAPI::Broodwar->registerEvent([unit, target](BWAPI::Game*)
		{
			if (!unit->exists() || !target->exists())
				return;
			BWAPI::Broodwar->drawLineMap(unit->getPosition(), target->getPosition(), BWAPI::Colors::Orange);
		},
			nullptr,  // condition
			Config::UnitProbeInterface::FrequencyFrame);  // frames to run
	}

}

void DrawManager::drawBlocksRewards(const BWAPI::Unit& unit, const std::vector<std::vector<double>>& rewards)
{
	if (Config::Debug::DrawBlocksRewards)
	{
		BWAPI::Broodwar->registerEvent([unit, rewards](BWAPI::Game*)
		{
			if (!unit->exists())
				return;

			int radius = Config::UnitProbeInterface::BlockRadius;
			int span = Config::UnitProbeInterface::BlockSpan;
			BWAPI::Position p = unit->getPosition();

			for (int i = -radius; i <= radius; i++)
			{
				BWAPI::Broodwar->drawLineMap(p.x + span*i, p.y - span*radius, p.x + span*i, p.y + span*radius, BWAPI::Colors::Grey);
			}
			for (int j = -radius; j <= radius; j++)
			{
				BWAPI::Broodwar->drawLineMap(p.x - span*radius, p.y + span*j, p.x + span*radius, p.y + span*j, BWAPI::Colors::Grey);
			}
			for (int i = -radius; i <= radius; i++)
			{
				for (int j = -radius; j <= radius; j++)
					BWAPI::Broodwar->drawTextMap(p.x + span*i, p.y + span*j, "%.0f", rewards[i + radius][j + radius]);
			}		
		},
			nullptr,  // condition
			Config::UnitProbeInterface::FrequencyFrame);  // frames to run
	}
}


void DrawManager::drawPlanBuilding(const BWAPI::TilePosition& location, const BWAPI::UnitType& unit_type)
{
	if (Config::Debug::DrawPlanBuilding)
	{
		BWAPI::Broodwar->registerEvent([location, unit_type](BWAPI::Game*)
		{
			if (!location.isValid() || !unit_type.isBuilding())
				return;
			BWAPI::Broodwar->drawBoxMap(BWAPI::Position(location), 
				BWAPI::Position(location)+BWAPI::Position(unit_type.tileWidth()*32, unit_type.tileHeight()*32), 
				BWAPI::Colors::White);
			BWAPI::Broodwar->drawTextMap(BWAPI::Position(location), "%s", unit_type.getName().c_str());
		},
			nullptr,  // condition
			Config::BuildingManager::FrequencyFrame);  // frames to run
	}

}


void DrawManager::drawAttackTarget(const BWAPI::Unit& unit, const BWAPI::Position& position)
{
	if (Config::Debug::DrawAttackTarget)
	{
		BWAPI::Broodwar->registerEvent([unit, position](BWAPI::Game*)
		{
			if (unit)
				BWAPI::Broodwar->drawBoxMap(unit->getLeft(), unit->getTop(), unit->getRight(), unit->getBottom(), BWAPI::Colors::Cyan);
			else if (position.isValid())
				BWAPI::Broodwar->drawCircleMap(position, 32, BWAPI::Colors::Cyan);
		},
			nullptr,  // condition
			Config::InformationManager::FrequencyFrame - 1);  // frames to run
	}

}


void DrawManager::drawSearchBuild(const BuildActionFrames& action_frames)
{
	if (Config::Debug::DrawSearchBuild)
	{
		BWAPI::Broodwar->registerEvent([action_frames](BWAPI::Game*)
		{
			int display_x = 350, display_y = 20; 
			int row = 12, col_action = 130, col_frame = 70; 

			BWAPI::Broodwar->drawBoxScreen(display_x, display_y, display_x+col_action, display_y + row, BWAPI::Colors::Brown); 
			BWAPI::Broodwar->drawTextScreen(display_x, display_y, "BUILD LIST:"); 

			BWAPI::Broodwar->drawBoxScreen(display_x + col_action, display_y, display_x + col_action+col_frame, display_y + row, BWAPI::Colors::Brown); 
			BWAPI::Broodwar->drawTextScreen(display_x + col_action, display_y, "BUILD FRAME:"); 

			BWAPI::Broodwar->drawTextScreen(display_x + col_action+col_frame, display_y, "CURRENT:"); 
			BWAPI::Broodwar->drawTextScreen(display_x + col_action + col_frame, display_y + row, "%d", BWAPI::Broodwar->getFrameCount());

			int num = action_frames.size(); 
			BWAPI::Broodwar->drawBoxScreen(display_x, display_y + row, display_x + col_action, display_y + (num + 1)*row, BWAPI::Colors::Brown);
			BWAPI::Broodwar->drawBoxScreen(display_x+col_action, display_y + row, display_x + col_action + col_frame, display_y + (num + 1)*row, BWAPI::Colors::Brown);

			for (int i = 0; i < action_frames.size(); i++)
			{
				BWAPI::Broodwar->drawTextScreen(display_x, display_y + (i + 1)*row, "%s", action_frames[i].first.getName().c_str()); 

				BWAPI::Broodwar->drawTextScreen(display_x + col_action, display_y + (i + 1)*row, "%d", action_frames[i].second); 
			}

		},
			nullptr,  // condition
			Config::StrategyManager::FrequencyFrame - 1);  // frames to run
	}
}

void DrawManager::drawTime(int x, int y, int frames, std::string str, double micro_sec)
{
	if (Config::Debug::DrawTime)
	{
		BWAPI::Broodwar->registerEvent([x, y, frames, str, micro_sec](BWAPI::Game*)
		{
			BWAPI::Broodwar->drawTextScreen(x, y, "%s use: %.0f micro sec", str.c_str(), micro_sec);
		},
			nullptr,  // condition
			frames - 1);  // frames to run
	}
}