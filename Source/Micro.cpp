#include "Micro.h"
#include "MapGrid.h"
#include "UnitUtil.h"

using namespace XBot;

size_t TotalCommands = 0;

const int dotRadius = 2;

void Micro::SmartStop(BWAPI::Unit unit)
{
	if (!unit || !unit->exists() || unit->getPlayer() != BWAPI::Broodwar->self())
	{
		UAB_ASSERT(false, "bad arg");
		return;
	}

	// if we have issued a command to this unit already this frame, ignore this one
	if (unit->getLastCommandFrame() >= BWAPI::Broodwar->getFrameCount() || unit->isAttackFrame())
	{
		return;
	}

	// If we already gave this command, don't repeat it.
	BWAPI::UnitCommand currentCommand(unit->getLastCommand());
	if (currentCommand.getType() == BWAPI::UnitCommandTypes::Stop)
	{
		return;
	}

	unit->stop();
	TotalCommands++;
}

void Micro::SmartAttackUnit(BWAPI::Unit attacker, BWAPI::Unit target)
{
	if (!attacker || !attacker->exists() || attacker->getPlayer() != BWAPI::Broodwar->self() ||
		!target || !target->exists())
    {
		UAB_ASSERT(false,"bad arg");
        return;
    }

    // if we have issued a command to this unit already this frame, ignore this one
    if (attacker->getLastCommandFrame() >= BWAPI::Broodwar->getFrameCount() || attacker->isAttackFrame())
    {
		return;
    }

    // get the unit's current command
    BWAPI::UnitCommand currentCommand(attacker->getLastCommand());

    // if we've already told this unit to attack this target, ignore this command
    if (currentCommand.getType() == BWAPI::UnitCommandTypes::Attack_Unit &&	currentCommand.getTarget() == target)
    {
		return;
    }
	
    // if nothing prevents it, attack the target
    attacker->attack(target);
    TotalCommands++;

    if (Config::Debug::DrawUnitTargetInfo) 
    {
        BWAPI::Broodwar->drawCircleMap(attacker->getPosition(), dotRadius, BWAPI::Colors::Red, true);
        BWAPI::Broodwar->drawCircleMap(target->getPosition(), dotRadius, BWAPI::Colors::Red, true);
        BWAPI::Broodwar->drawLineMap(attacker->getPosition(), target->getPosition(), BWAPI::Colors::Red);
    }
}

void Micro::SmartAttackMove(BWAPI::Unit attacker, const BWAPI::Position & targetPosition)
{
	if (!attacker || !attacker->exists() || attacker->getPlayer() != BWAPI::Broodwar->self() || !targetPosition.isValid())
    {
		UAB_ASSERT(false, "bad arg");
		return;
    }

    // if we have issued a command to this unit already this frame, ignore this one
    if (attacker->getLastCommandFrame() >= BWAPI::Broodwar->getFrameCount() || attacker->isAttackFrame())
    {
        return;
    }

    // get the unit's current command
    BWAPI::UnitCommand currentCommand(attacker->getLastCommand());

    // if we've already told this unit to attack this target, ignore this command
    if (currentCommand.getType() == BWAPI::UnitCommandTypes::Attack_Move &&	currentCommand.getTargetPosition() == targetPosition)
	{
		return;
	}

	const auto & area1 = InformationManager::Instance().getTileArea(attacker->getTilePosition());
	const auto & area2 = InformationManager::Instance().getTileArea(BWAPI::TilePosition(targetPosition));
	const auto & chokes = BWEM::Map::Instance().GetPath(area1, area2);
	if (attacker->isFlying() || chokes.size() < 2)
	{
		// if nothing prevents it, move the target position
		attacker->attack(targetPosition);
	}
	else
	{
		auto target = BWAPI::Position(chokes[1]->Center());
		if (attacker->getDistance(target) < 100)
		{
			target = targetPosition;
		}
		// if nothing prevents it, move the target position
		attacker->attack(target);
	}

    if (Config::Debug::DrawUnitTargetInfo) 
    {
        BWAPI::Broodwar->drawCircleMap(attacker->getPosition(), dotRadius, BWAPI::Colors::Orange, true);
        BWAPI::Broodwar->drawCircleMap(targetPosition, dotRadius, BWAPI::Colors::Orange, true);
        BWAPI::Broodwar->drawLineMap(attacker->getPosition(), targetPosition, BWAPI::Colors::Orange);
    }
}

void Micro::SmartMove(BWAPI::Unit attacker, const BWAPI::Position & targetPosition)
{
	if (!attacker || !attacker->exists() || attacker->getPlayer() != BWAPI::Broodwar->self() || !targetPosition.isValid())
	{
		// UAB_ASSERT(false, "bad arg");  // TODO restore this after the bugs are solved; can make too many beeps
		return;
	}

    // if we have issued a command to this unit already this frame, ignore this one
    if (attacker->getLastCommandFrame() >= BWAPI::Broodwar->getFrameCount() || attacker->isAttackFrame())
    {
        return;
    }

    BWAPI::UnitCommand currentCommand(attacker->getLastCommand());

    // if we've already told this unit to move to this position, ignore this command
    if ((currentCommand.getType() == BWAPI::UnitCommandTypes::Move) && (currentCommand.getTargetPosition() == targetPosition) && attacker->isMoving())
    {
        return;
    }

	const auto & area1 = InformationManager::Instance().getTileArea(attacker->getTilePosition());
	const auto & area2 = InformationManager::Instance().getTileArea(BWAPI::TilePosition(targetPosition));
	const auto & chokes = BWEM::Map::Instance().GetPath(area1, area2);
	if (attacker->isFlying() || chokes.size() < 2)
	{
		// if nothing prevents it, move the target position
		attacker->move(targetPosition);
	}
	else
	{
		auto target = BWAPI::Position(chokes[1]->Center());
		if (attacker->getDistance(target) < 100)
		{
			target = targetPosition;
		}
		// if nothing prevents it, move the target position
		attacker->move(target);
	}

    if (Config::Debug::DrawUnitTargetInfo) 
    {
        BWAPI::Broodwar->drawCircleMap(attacker->getPosition(), dotRadius, BWAPI::Colors::White, true);
        BWAPI::Broodwar->drawCircleMap(targetPosition, dotRadius, BWAPI::Colors::White, true);
        BWAPI::Broodwar->drawLineMap(attacker->getPosition(), targetPosition, BWAPI::Colors::White);
    }
}

void Micro::SmartRightClick(BWAPI::Unit unit, BWAPI::Unit target)
{
	if (!unit || !unit->exists() || unit->getPlayer() != BWAPI::Broodwar->self() ||
		!target || !target->exists())
	{
		UAB_ASSERT(false, "bad arg");
		return;
	}


    // if we have issued a command to this unit already this frame, ignore this one
    if (unit->getLastCommandFrame() >= BWAPI::Broodwar->getFrameCount() || unit->isAttackFrame())
    {
        return;
    }

    // get the unit's current command
    BWAPI::UnitCommand currentCommand(unit->getLastCommand());

    // if we've already told this unit to move to this position, ignore this command
    if ((currentCommand.getType() == BWAPI::UnitCommandTypes::Right_Click_Unit) && (currentCommand.getTargetPosition() == target->getPosition()))
    {
        return;
    }

    // if nothing prevents it, attack the target
    unit->rightClick(target);
    TotalCommands++;

    if (Config::Debug::DrawUnitTargetInfo) 
    {
        BWAPI::Broodwar->drawCircleMap(unit->getPosition(), dotRadius, BWAPI::Colors::Cyan, true);
        BWAPI::Broodwar->drawCircleMap(target->getPosition(), dotRadius, BWAPI::Colors::Cyan, true);
        BWAPI::Broodwar->drawLineMap(unit->getPosition(), target->getPosition(), BWAPI::Colors::Cyan);
    }
}

void Micro::SmartLaySpiderMine(BWAPI::Unit unit, BWAPI::Position pos)
{
	if (!unit || !unit->exists() || unit->getPlayer() != BWAPI::Broodwar->self() || !pos.isValid())
	{
		UAB_ASSERT(false, "bad arg");
		return;
	}

    if (!unit->canUseTech(BWAPI::TechTypes::Spider_Mines, pos))
    {
        return;
    }

    BWAPI::UnitCommand currentCommand(unit->getLastCommand());

    // if we've already told this unit to move to this position, ignore this command
    if ((currentCommand.getType() == BWAPI::UnitCommandTypes::Use_Tech_Position) && (currentCommand.getTargetPosition() == pos))
    {
        return;
    }

    unit->canUseTechPosition(BWAPI::TechTypes::Spider_Mines, pos);
}

void Micro::SmartRepair(BWAPI::Unit unit, BWAPI::Unit target)
{
	if (!unit || !unit->exists() || unit->getPlayer() != BWAPI::Broodwar->self() ||
		!target || !target->exists())
	{
		UAB_ASSERT(false, "bad arg");
		return;
	}

    // if we have issued a command to this unit already this frame, ignore this one
    if (unit->getLastCommandFrame() >= BWAPI::Broodwar->getFrameCount() || unit->isAttackFrame())
    {
        return;
    }

    // get the unit's current command
    BWAPI::UnitCommand currentCommand(unit->getLastCommand());

    // if we've already told this unit to move to this position, ignore this command
    if ((currentCommand.getType() == BWAPI::UnitCommandTypes::Repair) && (currentCommand.getTarget() == target))
    {
        return;
    }

    // Nothing prevents it, so attack the target.
    unit->repair(target);
    TotalCommands++;

    if (Config::Debug::DrawUnitTargetInfo) 
    {
        BWAPI::Broodwar->drawCircleMap(unit->getPosition(), dotRadius, BWAPI::Colors::Cyan, true);
        BWAPI::Broodwar->drawCircleMap(target->getPosition(), dotRadius, BWAPI::Colors::Cyan, true);
        BWAPI::Broodwar->drawLineMap(unit->getPosition(), target->getPosition(), BWAPI::Colors::Cyan);
    }
}

// Perform a comsat scan at the given position if possible and necessary.
// If it's not possible, or we already scanned there, do nothing.
// Return whether the scan occurred.
bool Micro::SmartScan(const BWAPI::Position & targetPosition)
{
	UAB_ASSERT(targetPosition.isValid(), "bad position");

	// If a scan of this position is still active, don't scan it again.
	if (MapGrid::Instance().scanIsActiveAt(targetPosition))
	{
		return false;
	}

	// Choose the comsat with the highest energy.
	// If we're not terran, we're unlikely to have any comsats....
	int maxEnergy = 49;      // anything greater is enough energy for a scan
	BWAPI::Unit comsat = nullptr;
	for (const auto unit : BWAPI::Broodwar->self()->getUnits())
	{
		if (unit->getType() == BWAPI::UnitTypes::Terran_Comsat_Station &&
			unit->getEnergy() > maxEnergy &&
			unit->canUseTech(BWAPI::TechTypes::Scanner_Sweep, targetPosition))
		{
			maxEnergy = unit->getEnergy();
			comsat = unit;
		}
	}

	if (comsat)
	{
		MapGrid::Instance().scanAtPosition(targetPosition);
		return comsat->useTech(BWAPI::TechTypes::Scanner_Sweep, targetPosition);
	}

	return false;
}

// Stim the given marine or firebat, if possible; otherwise, do nothing.
// Return whether the stim occurred.
bool Micro::SmartStim(BWAPI::Unit unit)
{
	if (!unit ||
		unit->getType() != BWAPI::UnitTypes::Terran_Marine && unit->getType() != BWAPI::UnitTypes::Terran_Firebat ||
		unit->getPlayer() != BWAPI::Broodwar->self())
	{
		UAB_ASSERT(false, "bad unit");
		return false;
	}

	if (unit->isStimmed())
	{
		return false;
	}

	// if we have issued a command to this unit already this frame, ignore this one
	if (unit->getLastCommandFrame() >= BWAPI::Broodwar->getFrameCount())
	{
		return false;
	}

	// Allow a small latency for a previous stim command to take effect.
	// Marines and firebats have only 1 tech to use, so we don't need to check which.
	if (unit->getLastCommand().getType() == BWAPI::UnitCommandTypes::Use_Tech &&
		BWAPI::Broodwar->getFrameCount() - unit->getLastCommandFrame() < 8)
	{
		return false;
	}

	// useTech() checks whether stim is researched and any other conditions.
	return unit->useTech(BWAPI::TechTypes::Stim_Packs);
}

// Merge the 2 given high templar into an archon.
// TODO Check whether the 2 templar can reach each other: Is there a ground path between them?
bool Micro::SmartMergeArchon(BWAPI::Unit templar1, BWAPI::Unit templar2)
{
	if (!templar1 || !templar2 ||
		templar1->getPlayer() != BWAPI::Broodwar->self() ||
		templar2->getPlayer() != BWAPI::Broodwar->self() ||
		templar1->getType() != BWAPI::UnitTypes::Protoss_High_Templar ||
		templar2->getType() != BWAPI::UnitTypes::Protoss_High_Templar ||
		templar1 == templar2)
	{
		UAB_ASSERT(false, "bad unit");
		return false;
	}

	// If we have issued a command already this frame, ignore this one.
	if (templar1->getLastCommandFrame() >= BWAPI::Broodwar->getFrameCount() ||
		templar2->getLastCommandFrame() >= BWAPI::Broodwar->getFrameCount())
	{
		return false;
	}

	// useTech() checks any other conditions.
	return templar1->useTech(BWAPI::TechTypes::Archon_Warp, templar2);
}

void Micro::SmartReturnCargo(BWAPI::Unit worker)
{
	if (!worker || !worker->exists() || worker->getPlayer() != BWAPI::Broodwar->self() ||
		!worker->getType().isWorker())
	{
		UAB_ASSERT(false, "bad arg");
		return;
	}

	// If the worker has no cargo, ignore this command.
	if (!worker->isCarryingMinerals() && !worker->isCarryingGas())
	{
		return;
	}

	// if we have issued a command to this unit already this frame, ignore this one
	if (worker->getLastCommandFrame() >= BWAPI::Broodwar->getFrameCount() || worker->isAttackFrame())
	{
		return;
	}

	// If we've already issued this command, don't issue it again.
	BWAPI::UnitCommand currentCommand(worker->getLastCommand());
	if (currentCommand.getType() == BWAPI::UnitCommandTypes::Return_Cargo)
	{
		return;
	}

	// Nothing prevents it, so return cargo.
	worker->returnCargo();
	TotalCommands++;
}

void Micro::SmartKiteTarget(BWAPI::Unit rangedUnit, BWAPI::Unit target)
{
	if (!rangedUnit || !rangedUnit->exists() || rangedUnit->getPlayer() != BWAPI::Broodwar->self() ||
		!target || !target->exists())
	{
		UAB_ASSERT(false, "bad arg");
		return;
	}

	double range(rangedUnit->getType().groundWeapon().maxRange());
	if (rangedUnit->getType() == BWAPI::UnitTypes::Protoss_Dragoon && BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Singularity_Charge))
	{
		range = 6 * 32;
	}
	else if (rangedUnit->getType() == BWAPI::UnitTypes::Zerg_Hydralisk && BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Grooved_Spines))
	{
		range = 5 * 32;
	}

	bool kite(true);

	// Kite if configured as a "KiteLonger" unit, or if the enemy's range is shorter than ours.
	// Note: Assumes that the enemy does not have range upgrades.
    bool kiteLonger = Config::Micro::KiteLongerRangedUnits.find(rangedUnit->getType()) != Config::Micro::KiteLongerRangedUnits.end();
	if (!kiteLonger && (range <= target->getType().groundWeapon().maxRange()))
	{
		kite = false;
	}
    // If the target can't attack back, then don't kite.
	else if (!UnitUtil::CanAttack(target, rangedUnit))
    {
    	kite = false;
    }

	// Kite if we're not ready yet: Wait for the weapon.
	double dist(rangedUnit->getDistance(target));
	double speed(rangedUnit->getType().topSpeed());
	double timeToEnter = 0.0;                      // time to reach firing range
	if (speed > .00001)                            // don't even visit the same city as division by zero
	{
		timeToEnter = std::max(0.0, dist - range) / speed;
	}
	if (timeToEnter >= rangedUnit->getGroundWeaponCooldown() ||
		target->getType().isBuilding())
	{
		kite = false;
	}

	if (kite)
	{
		// Run away.
		BWAPI::Position fleePosition(rangedUnit->getPosition() - target->getPosition() + rangedUnit->getPosition());
		if (Config::Debug::DrawUnitTargetInfo)
		{
			BWAPI::Broodwar->drawLineMap(rangedUnit->getPosition(), fleePosition, BWAPI::Colors::Cyan);
		}
		Micro::SmartMove(rangedUnit, fleePosition);
	}
	else
	{
		// Shoot.
		Micro::SmartAttackUnit(rangedUnit, target);
	}
}

// Used for both mutalisks and vultures--fast units with short range.
void Micro::MutaDanceTarget(BWAPI::Unit muta, BWAPI::Unit target)
{
	if (!muta || !muta->exists() || muta->getPlayer() != BWAPI::Broodwar->self() ||
		(muta->getType() != BWAPI::UnitTypes::Zerg_Mutalisk && muta->getType() != BWAPI::UnitTypes::Terran_Vulture) ||
		!target || !target->exists())
	{
		UAB_ASSERT(false, "bad arg");
		return;
	}

    const int cooldown                  = muta->getType().groundWeapon().damageCooldown();
    const int latency                   = BWAPI::Broodwar->getLatency();
    const double speed                  = muta->getType().topSpeed();   // known to be non-zero :-)
    const double range                  = muta->getType().groundWeapon().maxRange();
    const double distanceToTarget       = muta->getDistance(target);
	const double distanceToFiringRange  = std::max(distanceToTarget - range,0.0);
	const double timeToEnterFiringRange = distanceToFiringRange / speed;
	const int framesToAttack            = static_cast<int>(timeToEnterFiringRange) + 2*latency;

	// How many frames are left before we can attack?
	const int currentCooldown = muta->isStartingAttack() ? cooldown : muta->getGroundWeaponCooldown();

	// If we can attack by the time we reach our firing range
	if(currentCooldown <= framesToAttack)
	{
		// Move towards and attack the target
		muta->attack(target);
	}
	else // Otherwise we cannot attack and should temporarily back off
	{
		BWAPI::Position fleeVector = GetKiteVector(target, muta);
		BWAPI::Position moveToPosition(muta->getPosition() + fleeVector);
		if (moveToPosition.isValid())
		{
			muta->rightClick(moveToPosition);
		}
	}
}

BWAPI::Position Micro::GetKiteVector(BWAPI::Unit unit, BWAPI::Unit target)
{
    BWAPI::Position fleeVec(target->getPosition() - unit->getPosition());
    double fleeAngle = atan2(fleeVec.y, fleeVec.x);
    fleeVec = BWAPI::Position(static_cast<int>(64 * cos(fleeAngle)), static_cast<int>(64 * sin(fleeAngle)));
    return fleeVec;
}
