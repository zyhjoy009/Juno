#pragma once 


#include <BWAPI.h>


class ConvertManager
{
public:
	/* TilePosition to Position*/
	BWAPI::Position tilePositionToPosition(const BWAPI::TilePosition& tp) { return BWAPI::Position(tp.x * 32, tp.y * 32 ); };


	/* return singleton instance */
	static ConvertManager& getInstance() { static ConvertManager cm; return cm; }; 
};