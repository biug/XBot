#pragma once

#include "Common.h"
#include "MapGrid.h"

#include "InformationManager.h"

namespace XBot
{
class CombatSimulation
{
public:

	CombatSimulation();

	void setCombatUnits(const BWAPI::Position & center, const int radius);

	double simulateCombat();
};
}