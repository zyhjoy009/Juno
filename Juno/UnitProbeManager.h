#pragma once


#include <BWAPI.h>
#include "UnitProbeInterface.h"


/* store all self units (Probe) */
class UnitProbeManager
{
public:

	UnitProbeManager() {}; 

	/* do some initial work */
	void initial() { units.clear(); map_unanalyze_units.clear(); };

	/* onFrame call: update each member*/
	void onFrame(); 

	/* onUnitCreate call */
	void onUnitCreate(BWAPI::Unit u); 

	void onUnitCreate(UnitProbe u) { units.push_back(u);  };

	/* onUnitDestroy call */
	void onUnitDestroy(BWAPI::Unit u); 

	/* delete pointer after game */
	void onEnd(); 

	/* return the closest unit from a position with give Role */
	UnitProbe getClosestUnit(const BWAPI::Position& p, const Role& r, int radius = 99999);

	/* swap two units of their data */
	void swap(UnitProbe u1, UnitProbe u2); 

	/* find UnitProbe from Unit */
	UnitProbe getUnit(const BWAPI::Unit& u); 

	std::vector<UnitProbe>::iterator getUnitIterator(const BWAPI::Unit& u);

	std::vector<UnitProbe>& getBeforeMapAnazlyeUnits() { return map_unanalyze_units; }; 

	std::vector<UnitProbe>& getUnits() { return units; }; 

	/* return instance */
	static UnitProbeManager& getInstance() { static UnitProbeManager upm; return upm; }


private:

	std::vector<UnitProbe>			units;			/* unit members */

	std::vector<UnitProbe>			map_unanalyze_units;		// before map analyze units are stored here



};