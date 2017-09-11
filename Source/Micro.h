#pragma once

#include <Common.h>
#include <BWAPI.h>

namespace XBot
{
namespace Micro
{
	void SmartStop(BWAPI::Unit unit);
	void SmartAttackUnit(BWAPI::Unit attacker, BWAPI::Unit target);
    void SmartAttackMove(BWAPI::Unit attacker, const BWAPI::Position & targetPosition);
    void SmartMove(BWAPI::Unit attacker, const BWAPI::Position & targetPosition);
	void SmartRightClick(BWAPI::Unit unit, BWAPI::Unit target);
    void SmartLaySpiderMine(BWAPI::Unit unit, BWAPI::Position pos);
    void SmartRepair(BWAPI::Unit unit, BWAPI::Unit target);
	bool SmartScan(const BWAPI::Position & targetPosition);
	bool SmartStim(BWAPI::Unit unit);
	bool SmartMergeArchon(BWAPI::Unit templar1, BWAPI::Unit templar2);
	void SmartReturnCargo(BWAPI::Unit worker);
	void SmartKiteTarget(BWAPI::Unit rangedUnit, BWAPI::Unit target);
    void MutaDanceTarget(BWAPI::Unit muta, BWAPI::Unit target);
    BWAPI::Position GetKiteVector(BWAPI::Unit unit, BWAPI::Unit target);

    void Rotate(double &x, double &y, double angle);
    void Normalize(double &x, double &y);
};
}