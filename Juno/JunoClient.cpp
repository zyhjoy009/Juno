#include <BWAPI.h>
#include <BWAPI/Client.h>

#include <iostream>
#include <thread>
#include <chrono>
#include <string>

#include "StrategyManager.h"

using namespace BWAPI;


void reconnect()
{
	while (!BWAPIClient.connect())
	{
		std::this_thread::sleep_for(std::chrono::milliseconds{ 1000 });
	}
}

int main(int argc, const char* argv[])
{
	std::cout << "Connecting..." << std::endl;;
	reconnect();
	while (true)
	{
		std::cout << "waiting to enter match" << std::endl;
		while (!Broodwar->isInGame())
		{
			BWAPI::BWAPIClient.update();
			if (!BWAPI::BWAPIClient.isConnected())
			{
				std::cout << "Reconnecting..." << std::endl;;
				reconnect();
			}
		}
		std::cout << "starting match!" << std::endl;
		Broodwar->sendText("Hello world!");
		Broodwar << "The map is " << Broodwar->mapName() << ", a " << Broodwar->getStartLocations().size() << " player map" << std::endl;
		
		if (BWAPI::Broodwar->self()->getRace() != BWAPI::Races::Protoss)
		{
			if (BWAPI::Broodwar->getFrameCount() % 42  == 0)
				Broodwar << "Sorry I can only play Protoss" << std::endl;
			continue; 
		}

		BWTA::readMap(); 
		BWTA::analyze(); 
		StrategyManager::getInstance().onStart(); 

		if (Broodwar->isReplay())
		{
			Broodwar << "The following players are in this replay:" << std::endl;;
			Playerset players = Broodwar->getPlayers();
			for (auto p : players)
			{
				if (!p->getUnits().empty() && !p->isNeutral())
					Broodwar << p->getName() << ", playing as " << p->getRace() << std::endl;
			}
		}
		else
		{
			while (Broodwar->isInGame())
			{
				for (auto &e : Broodwar->getEvents())
				{
					switch (e.getType())
					{
					case EventType::MatchEnd:
						if (e.isWinner())
							Broodwar << "I won the game" << std::endl;
						else
							Broodwar << "I lost the game" << std::endl;
						break;
					case EventType::SendText:
						break;
					case EventType::ReceiveText:
						Broodwar << e.getPlayer()->getName() << " said \"" << e.getText() << "\"" << std::endl;
						break;
					case EventType::PlayerLeft:
						Broodwar << e.getPlayer()->getName() << " left the game." << std::endl;
						break;
					case EventType::NukeDetect:
						if (e.getPosition() != Positions::Unknown)
						{
							Broodwar->drawCircleMap(e.getPosition(), 40, Colors::Red, true);
							Broodwar << "Nuclear Launch Detected at " << e.getPosition() << std::endl;
						}
						else
							Broodwar << "Nuclear Launch Detected" << std::endl;
						break;
					case EventType::UnitCreate:
						StrategyManager::getInstance().onUnitCreate(e.getUnit());
						break;
					case EventType::UnitDestroy:
						StrategyManager::getInstance().onUnitDestroy(e.getUnit());
						break;
					case EventType::UnitMorph:
						break;
					case EventType::UnitDiscover:
						StrategyManager::getInstance().onUnitDiscover(e.getUnit());
						break;
					case EventType::UnitShow:
						break;
					case EventType::UnitHide:
						break;
					case EventType::UnitRenegade:
						break;
					case EventType::SaveGame:
						break;
					}
				}

				Broodwar->drawTextScreen(300, 0, "FPS: %f", Broodwar->getAverageFPS());

				BWAPI::BWAPIClient.update();
				if (!BWAPI::BWAPIClient.isConnected())
				{
					std::cout << "Reconnecting..." << std::endl;
					reconnect();
				}
			}
		}
		std::cout << "Game ended" << std::endl;
	}
	std::cout << "Press ENTER to continue..." << std::endl;
	std::cin.ignore();
	return 0;
}

