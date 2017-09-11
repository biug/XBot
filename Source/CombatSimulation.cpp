#include "CombatSimulation.h"
#include "FAP.h"
#include "UnitUtil.h"

using namespace XBot;

CombatSimulation::CombatSimulation()
{
}

// sets the starting states based on the combat units within a radius of a given position
// this center will most likely be the position of the forwardmost combat unit we control
void CombatSimulation::setCombatUnits(const BWAPI::Position & center, const int radius)
{
	fap.clearState();

	if (Config::Debug::DrawCombatSimulationInfo)
	{
		BWAPI::Broodwar->drawCircleMap(center.x, center.y, 6, BWAPI::Colors::Red, true);
		BWAPI::Broodwar->drawCircleMap(center.x, center.y, radius, BWAPI::Colors::Red);
	}

	BWAPI::Unitset ourCombatUnits;
	std::vector<UnitInfo> enemyCombatUnits;

	// Both of these return only completed units, so we don't need to check that later.
	MapGrid::Instance().GetUnits(ourCombatUnits, center, Config::Micro::CombatRegroupRadius, true, false);
	InformationManager::Instance().getNearbyForce(enemyCombatUnits, center, BWAPI::Broodwar->enemy(), Config::Micro::CombatRegroupRadius);

	// Add enemy units first.
	for (const UnitInfo & ui : enemyCombatUnits)
	{
		if (ui.type.isWorker() || ui.lastHealth == 0 || ui.type == BWAPI::UnitTypes::Unknown)
		{
			continue;
		}

		// Skip uncompleted or unpowered static defense.
		if (ui.type.isBuilding() && ui.unit->exists() && (!ui.unit->isCompleted() || !ui.unit->isPowered()))
		{
			continue;
		}

		fap.addIfCombatUnitPlayer2(ui);
	}

	// Add our units.
	for (const auto unit : ourCombatUnits)
	{
		bool include =
			unit->getHitPoints() > 0 &&
			UnitUtil::IsCombatSimUnit(unit->getType());

		// Skip uncompleted or unpowered static defense.
		if (unit->getType().isBuilding() && (!unit->isCompleted() || !unit->isPowered()))
		{
			include = false;
		}

		if (include)
		{
			fap.addIfCombatUnitPlayer1(unit);
		}
	}
}

double CombatSimulation::simulateCombat()
{
	fap.simulate();
	std::pair<int, int> scores = fap.playerScores();

	int score = scores.first - scores.second;

	if (Config::Debug::DrawCombatSimulationInfo)
	{
		BWAPI::Broodwar->drawTextScreen(150, 200, "%cCombat sim: us %c%d %c- them %c%d %c= %c%d",
			white, orange, scores.first, white, orange, scores.second, white,
			score >= 0 ? green : red, score);
	}

	return double(score);
}
