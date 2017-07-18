#pragma once 


#include <BWAPI.h>
#include "InformationManager.h"
#include "UnitProbeInterface.h"
#include "Timer.h"


DWORD WINAPI AnalyzeThread();

extern HANDLE hthread_analyze; 

extern Timer timer;

extern int timer_display_y; 


/* the main capacities of AI bot is realized here */
class Juno : public BWAPI::AIModule
{
public:

	/* onStart call: add depot, units, and initialize relevant variables */
	void onStart(); 

	/* onFrame call: update InformationManager, BuildingManager, UnitProbeManager */
	void onFrame(); 

	/* if u is not building, onUnitCreate is called once it is trained */
	void onUnitCreate(BWAPI::Unit u);

	/* onUnitDestroy call; remove UnitProbeManager or BuildingManager */
	void onUnitDestroy(BWAPI::Unit u); 

	/* onUnitShow call: add enemy units in InformationManager, onUnitDiscover can work at CompleteMapInformation mode */
	void onUnitShow(BWAPI::Unit u); 

	/* onUnitHide call: when unit become invisible */
	void onUnitHide(BWAPI::Unit u); 

	/* some display code is input here */
	void	onSendText(std::string text);

	/* close thread */
	void	onEnd(bool isWinner); 

	void	onUnitMorph(BWAPI::Unit unit) {};

	void	onUnitComplete(BWAPI::Unit unit) {};

	void	onUnitRenegade(BWAPI::Unit unit) {};

public: 

private:

	bool		is_playing;			/* flag of if game is playing */


};


