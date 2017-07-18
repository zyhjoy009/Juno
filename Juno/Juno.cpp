#include "Juno.h"
#include "StrategyManager.h"
#include "InformationManager.h"
#include <BWAPI.h>
#include <BWTA.h>
#include "Config.h"
#include "BuildingManager.h"
#include "ConvertManager.h"
#include "UnitProbeManager.h"
#include "UnitProbeInterface.h"
#include "Timer.h"
#include "DrawManager.h"


extern HANDLE hthread_analyze = nullptr; 

extern Timer timer = Timer();

extern int timer_display_y = 10;

/* onStart call: add depot, units, and initialize relevant variables */
void Juno::onStart()
{
	// Enable some cheat flags
	BWAPI::Broodwar->enableFlag(BWAPI::Flag::UserInput);

	// Uncomment to enable complete map information
//	BWAPI::Broodwar->enableFlag(BWAPI::Flag::CompleteMapInformation);

	// set game speed
	BWAPI::Broodwar->setLocalSpeed(Config::Game::GameSpeed); 

	// determine if we are now playing 
	if (BWAPI::Broodwar->isReplay() || BWAPI::Broodwar->getGameType() == BWAPI::GameTypes::Use_Map_Settings)
		is_playing = false;
	else
		is_playing = true; 

	// if we are not playing, gg
	if (!is_playing)
		return; 

	InformationManager::getInstance().initial();		// effected by BWTA::analyze

	BuildingManager::getInstance().initial();

	UnitProbeManager::getInstance().initial();

	// AIIDE use fastest speed, so just let BWTA analyze wait 
/*	if (Config::Game::UseThread)
	{
		hthread_analyze = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)AnalyzeThread, NULL, 0, NULL);
	}
	else */
	{
		/* read map */
		BWTA::readMap();
		/* BWTA analyze */
		BWTA::analyze();

		/* InformationManager initialize */

		InformationManager::getInstance().mapAnalyzeInitial(); 

		InformationManager::getInstance().setMapAnalyzed(); 
	}
	
	StrategyManager::getInstance().initial(); 

}


/* onUnitCreate call: add UnitProbeManager or BuildingManager 
 for self units and buildings */
void Juno::onUnitCreate(BWAPI::Unit u)
{
	if (!is_playing)
		return; 

	UnitProbeManager::getInstance().onUnitCreate(u); 

	BuildingManager::getInstance().onUnitCreate(u); 

	InformationManager::getInstance().onUnitCreate(u); 

}


/* onUnitDestroy call; remove UnitProbeManager or BuildingManager 
   for self unit and building 
   for enemy building */
void Juno::onUnitDestroy(BWAPI::Unit u)
{
	if (!is_playing)
		return;


	InformationManager::getInstance().onUnitDestroy(u); 

	BuildingManager::getInstance().onUnitDestroy(u); 

	UnitProbeManager::getInstance().onUnitDestroy(u); 

}


/* onUnitShow call: add enemy units in InformationManager 
   for enemy unit and building */
void Juno::onUnitShow(BWAPI::Unit u)
{
	if (!is_playing)
		return;

	InformationManager::getInstance().onUnitShow(u);
}


/* onUnitHide call: remove enemy unit */
void Juno::onUnitHide(BWAPI::Unit u)
{
	if (!is_playing)
		return;

	InformationManager::getInstance().onUnitHide(u);
}





/* onFrame call: update InformationManager, BuildingManager, UnitProbeManager */
void Juno::onFrame()
{
	if (!is_playing)
		return;

	timer.start(); 
	timer_display_y = 10; 

	InformationManager::getInstance().onFrame();

	StrategyManager::getInstance().onFrame(); 

	BuildingManager::getInstance().onFrame(); 

	UnitProbeManager::getInstance().onFrame(); 

	timer.stop(); 
	timer_display_y += 10; 
	DrawManager::getInstance().drawTime(10, timer_display_y, BWAPI::Broodwar->getLatency(), 
		"Total", timer.getElapsedTimeInMicroSec()); 

}


/* some display code is input here */
void Juno::onSendText(std::string text)
{
	if (text == "++")
		BWAPI::Broodwar->setLocalSpeed(0);
	else if (text == "--")
		BWAPI::Broodwar->setLocalSpeed(42);
	else if (text.compare(0, 6, "\\speed") == 0)
	{
		int speed = 42; 
		if (sscanf_s(text.c_str(), "\\speed %d", &speed) != 0)
			BWAPI::Broodwar->setLocalSpeed(speed); 
	}
}

DWORD WINAPI AnalyzeThread()
{
	BWTA::readMap(); 
	BWTA::analyze();

	// use BWTA information 
	InformationManager::getInstance().mapAnalyzeInitial();

	InformationManager::getInstance().setMapAnalyzed(); 

	BWAPI::Broodwar->sendText("Map analyze finished"); 

	return 0;
}


/* close thread */
void Juno::onEnd(bool isWinner)
{
	if (hthread_analyze)
	{
		TerminateThread(hthread_analyze, 0); 
		CloseHandle(hthread_analyze); 
		hthread_analyze = nullptr; 
	}
	if (hthread_search)
	{
		TerminateThread(hthread_search, 0); 
		CloseHandle(hthread_search); 
		hthread_search = nullptr; 
	}

	BuildingManager::getInstance().onEnd(); 

	UnitProbeManager::getInstance().onEnd(); 
}