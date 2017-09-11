#include "Common.h"
#include "WorkerManager.h"
#include "Micro.h"
#include "ProductionManager.h"
#include "UnitUtil.h"
#include "StateManager.h"
#include "ScoutManager.h"

using namespace XBot;

WorkerManager::WorkerManager() 
	: previousClosestWorker(nullptr)
	, _collectGas(true)
{
}

WorkerManager & WorkerManager::Instance() 
{
	static WorkerManager instance;
	return instance;
}

void WorkerManager::update() 
{
	// NOTE Combat workers are placed in a combat squad and get their orders there.
	//      We ignore them here.
	updateWorkerStatus();
	handleGasWorkers();
	handleIdleWorkers();
	handleReturnCargoWorkers();
	handleMoveWorkers();
	handleRepairWorkers();

	drawResourceDebugInfo();
	drawWorkerInformation(450,20);

	workerData.drawDepotDebugInfo();
}

// Adjust worker jobs. This is done first, before handling each job.
// NOTE A mineral worker may go briefly idle after collecting minerals.
// That's OK; we don't change its status then.
void WorkerManager::updateWorkerStatus() 
{
	// If any buildings are due for construction, assume that builders are not idle.
	const bool catchIdleBuilders =
		!BuildingManager::Instance().anythingBeingBuilt() &&
		!ProductionManager::Instance().nextIsBuilding();

	for (const auto worker : workerData.getWorkers())
	{
		if (!worker->isCompleted())
		{
			continue;     // the worker list includes drones in the egg
		}

		// TODO temporary debugging - see Micro::SmartMove
		// UAB_ASSERT(UnitUtil::IsValidUnit(worker), "bad worker");

		// If it's supposed to be on minerals but is actually collecting gas, fix it.
		// This can happen when we stop collecting gas; the worker can be mis-assigned.
		if (workerData.getWorkerJob(worker) == WorkerData::Minerals &&
			(worker->getOrder() == BWAPI::Orders::MoveToGas ||
			 worker->getOrder() == BWAPI::Orders::WaitForGas ||
			 worker->getOrder() == BWAPI::Orders::ReturnGas))
		{
			workerData.setWorkerJob(worker, WorkerData::Idle, nullptr);
		}

		// Work around a bug that can cause building drones to go idle.
		// If there should be no builders, then ensure any idle drone is marked idle.
		if (catchIdleBuilders &&
			worker->getOrder() == BWAPI::Orders::PlayerGuard &&
			(workerData.getWorkerJob(worker) == WorkerData::Move || workerData.getWorkerJob(worker) == WorkerData::Build))
		{
			workerData.setWorkerJob(worker, WorkerData::Idle, nullptr);
		}

		// The worker's original job. It may change as we go!
		auto job = workerData.getWorkerJob(worker);

		// Idleness.
		// Order can be PlayerGuard for a drone that tries to build and fails.
		// There are other causes.
		if ((worker->isIdle() || worker->getOrder() == BWAPI::Orders::PlayerGuard) &&
			job != WorkerData::Minerals &&
			job != WorkerData::Build &&
			job != WorkerData::Move &&
			job != WorkerData::Scout)
		{
			workerData.setWorkerJob(worker, WorkerData::Idle, nullptr);
		}

		else if (job == WorkerData::Gas)
		{
			BWAPI::Unit refinery = workerData.getWorkerResource(worker);

			// If the refinery is gone.
			// A missing resource depot is dealt with in handleGasWorkers().
			if (!refinery || !refinery->exists() || refinery->getHitPoints() == 0)
			{
				workerData.setWorkerJob(worker, WorkerData::Idle, nullptr);
			}
			else
			{
				// Self-defense.
				BWAPI::Unit target = findEnemyTargetForWorker(worker);

				if (target)
				{
					Micro::SmartAttackUnit(worker, target);
				}
				else if (worker->getOrder() != BWAPI::Orders::MoveToGas &&
					worker->getOrder() != BWAPI::Orders::WaitForGas &&
					worker->getOrder() != BWAPI::Orders::HarvestGas &&
					worker->getOrder() != BWAPI::Orders::ReturnGas &&
					worker->getOrder() != BWAPI::Orders::ResetCollision)
				{
					workerData.setWorkerJob(worker, WorkerData::Idle, nullptr);
				}
			}
		}
		
		// If the worker is busy mining and an enemy comes near, maybe fight it.
		else if (job == WorkerData::Minerals)
		{
			BWAPI::Unit target = findEnemyTargetForWorker(worker);

			if (target)
			{
				Micro::SmartAttackUnit(worker, target);
			}
			else if (worker->getOrder() != BWAPI::Orders::MoveToMinerals &&
				worker->getOrder() != BWAPI::Orders::WaitForMinerals &&
				worker->getOrder() != BWAPI::Orders::MiningMinerals &&
				worker->getOrder() != BWAPI::Orders::ReturnMinerals &&
				worker->getOrder() != BWAPI::Orders::ResetCollision)
			{
				workerData.setWorkerJob(worker, WorkerData::Idle, nullptr);
			}
		}
	}
}

void WorkerManager::setRepairWorker(BWAPI::Unit worker, BWAPI::Unit unitToRepair)
{
    workerData.setWorkerJob(worker, WorkerData::Repair, unitToRepair);
}

void WorkerManager::stopRepairing(BWAPI::Unit worker)
{
    workerData.setWorkerJob(worker, WorkerData::Idle, nullptr);
}

void WorkerManager::handleGasWorkers() 
{
	for (const auto unit : BWAPI::Broodwar->self()->getUnits())
	{
		// if that unit is a refinery
		if (unit->getType().isRefinery() && unit->isCompleted())
		{
			// Don't collect gas if gas collection is off, or if the resource depot is missing.
			if (_collectGas && refineryHasDepot(unit))
			{
				// Gather gas: If too few are assigned, add more.
				int numAssigned = workerData.getNumAssignedWorkers(unit);
				for (int i = 0; i < (Config::Macro::WorkersPerRefinery - numAssigned); ++i)
				{
					BWAPI::Unit gasWorker = getGasWorker(unit);
					if (gasWorker)
					{
						workerData.setWorkerJob(gasWorker, WorkerData::Gas, unit);
					}
					else
					{
						return;    // won't find any more, either for this refinery or others
					}
				}
			}
			else
			{
				// Don't gather gas: If any workers are assigned, take them off.
				std::set<BWAPI::Unit> gasWorkers;
				workerData.getGasWorkers(gasWorkers);
				for (const auto gasWorker : gasWorkers)
				{
					if (gasWorker->getOrder() != BWAPI::Orders::HarvestGas)    // not inside the refinery
					{
						workerData.setWorkerJob(gasWorker, WorkerData::Idle, nullptr);
						// An idle worker carrying gas will become a ReturnCargo worker,
						// so gas will not be lost needlessly.
					}
				}
			}
		}
	}
}

// Is the refinery near a resource depot that it can deliver gas to?
bool WorkerManager::refineryHasDepot(BWAPI::Unit refinery)
{
	// Iterate through units, not bases, because even if the main hatchery is destroyed
	// (so the base is considered gone), a macro hatchery may be close enough.
	// TODO could iterate through bases (from InfoMan) instead of units
	for (const auto unit : BWAPI::Broodwar->self()->getUnits())
	{
		if (unit->getType().isResourceDepot() &&
			(unit->isCompleted() || unit->getType() == BWAPI::UnitTypes::Zerg_Lair || unit->getType() == BWAPI::UnitTypes::Zerg_Hive) &&
			unit->getDistance(refinery) < 400)
		{
			return true;
		}
	}

	return false;
}

void WorkerManager::handleIdleWorkers() 
{
	for (const auto worker : workerData.getWorkers())
	{
        UAB_ASSERT(worker != nullptr, "Worker was null");

		if (workerData.getWorkerJob(worker) == WorkerData::Idle) 
		{
			if (worker->isCarryingMinerals() || worker->isCarryingGas())
			{
				// It's carrying something, set it to hand in its cargo.
				setReturnCargoWorker(worker);         // only happens if there's a resource depot
			}
			else {
				// Otherwise send it to mine minerals.
				setMineralWorker(worker);             // only happens if there's a resource depot
			}
		}
	}
}

void WorkerManager::handleReturnCargoWorkers()
{
	for (const auto worker : workerData.getWorkers())
	{
		UAB_ASSERT(worker != nullptr, "Worker was null");

		if (workerData.getWorkerJob(worker) == WorkerData::ReturnCargo)
		{
			// If it still needs to return cargo, return it; otherwise go idle.
			// We have to make sure it has a resource depot to return cargo to.
			BWAPI::Unit depot;
			if ((worker->isCarryingMinerals() || worker->isCarryingGas()) &&
				(depot = getClosestDepot(worker)) &&
				worker->getDistance(depot) < 600)
			{
				Micro::SmartReturnCargo(worker);
			}
			else
			{
				// Can't return cargo. Let's be a mineral worker instead--if possible.
				setMineralWorker(worker);
			}
		}
	}
}

// Terran can assign SCVs to repair.
void WorkerManager::handleRepairWorkers()
{
    if (BWAPI::Broodwar->self()->getRace() != BWAPI::Races::Terran)
    {
        return;
    }

    for (const auto unit : BWAPI::Broodwar->self()->getUnits())
    {
        if (unit->getType().isBuilding() && (unit->getHitPoints() < unit->getType().maxHitPoints()))
        {
            BWAPI::Unit repairWorker = getClosestMineralWorkerTo(unit);
            setRepairWorker(repairWorker, unit);
			break;
        }
    }
}

// Used for worker self-defense.
// Only include enemy units within 64 pixels that can be targeted by workers
// and are not moving or are stuck and moving randomly to dislodge themselves.
BWAPI::Unit WorkerManager::findEnemyTargetForWorker(BWAPI::Unit worker)
{
    UAB_ASSERT(worker != nullptr, "Worker was null");

	BWAPI::Unit closestUnit = nullptr;
	int closestDist = 65;         // ignore anything farther away

	for (const auto unit : BWAPI::Broodwar->enemy()->getUnits())
	{
		int dist;

		if (unit->isVisible() &&
			(!unit->isMoving() || unit->isStuck()) &&
			unit->getPosition().isValid() &&
			(dist = unit->getDistance(worker)) < closestDist &&
			!unit->isFlying() &&
			unit->isCompleted() &&
			unit->isDetected())
		{
			closestUnit = unit;
			closestDist = dist;
		}
	}

	return closestUnit;
}

BWAPI::Unit WorkerManager::getClosestMineralWorkerTo(BWAPI::Unit enemyUnit)
{
    UAB_ASSERT(enemyUnit != nullptr, "Unit was null");

    BWAPI::Unit closestMineralWorker = nullptr;
    int closestDist = 100000;

	// Former closest worker may have died or (if zerg) morphed into a building.
	if (UnitUtil::IsValidUnit(previousClosestWorker) && previousClosestWorker->getType().isWorker())
	{
		return previousClosestWorker;
    }

	for (const auto worker : workerData.getWorkers())
	{
        UAB_ASSERT(worker != nullptr, "Worker was null");

        if (workerData.getWorkerJob(worker) == WorkerData::Minerals) 
		{
			int dist = worker->getDistance(enemyUnit);
			if (worker->isCarryingMinerals() || worker->isCarryingGas())
			{
				// If it has cargo, pretend it is farther away.
				// That way we prefer empty workers and lose less cargo.
				dist += 64;
			}

            if (dist < closestDist)
            {
                closestMineralWorker = worker;
                dist = closestDist;
            }
		}
	}

    previousClosestWorker = closestMineralWorker;
    return closestMineralWorker;
}

BWAPI::Unit WorkerManager::getWorkerScout()
{
	for (const auto worker : workerData.getWorkers())
	{
        UAB_ASSERT(worker != nullptr, "Worker was null");
        if (workerData.getWorkerJob(worker) == WorkerData::Scout) 
		{
			return worker;
		}
	}

    return nullptr;
}

void WorkerManager::handleMoveWorkers() 
{
	for (const auto worker : workerData.getWorkers())
	{
        UAB_ASSERT(worker != nullptr, "Worker was null");

		if (workerData.getWorkerJob(worker) == WorkerData::Move) 
		{
			BWAPI::Unit depot;
			if ((worker->isCarryingMinerals() || worker->isCarryingGas()) &&
				(depot = getClosestDepot(worker)) &&
				worker->getDistance(depot) <= 256)
			{
				// A move worker is being sent to build or something.
				// Don't let it carry minerals or gas around wastefully.
				Micro::SmartReturnCargo(worker);
			}
			else
			{
				// UAB_ASSERT(worker->exists(), "bad worker");  // TODO temporary debugging - see Micro::SmartMove
				WorkerMoveData data = workerData.getWorkerMoveData(worker);
				auto & state = StateManager::Instance();
				// for terran choke
				if (state.being_terran_choke
					&& BWAPI::TilePosition(data.position) == state.anti_terran_choke_pos
					&& state.base_completed == 1)
				{
					double groundDist = MapTools::Instance().getGroundDistance(worker->getPosition(), data.position);
					int frame = groundDist / worker->getType().topSpeed();
					for (const auto & unit : BWAPI::Broodwar->self()->getUnits())
					{
						if (unit->getType() == BWAPI::UnitTypes::Zerg_Hatchery && !unit->isCompleted())
						{
							if (unit->getRemainingBuildTime() < frame)
							{
								Micro::SmartMove(worker, data.position);
							}
						}
					}
				}
				else
				{
					Micro::SmartMove(worker, data.position);
				}
			}
		}
	}
}

// Send the worker to mine minerals at the closest resource depot, if any.
void WorkerManager::setMineralWorker(BWAPI::Unit unit)
{
    UAB_ASSERT(unit != nullptr, "Unit was null");

	BWAPI::Unit depot = getClosestDepot(unit);

	if (depot)
	{
		workerData.setWorkerJob(unit, WorkerData::Minerals, depot);
	}
	else
	{
		// BWAPI::Broodwar->printf("No depot for mineral worker");
	}
}

// Worker is carrying minerals or gas. Tell it to hand them in.
void WorkerManager::setReturnCargoWorker(BWAPI::Unit unit)
{
	UAB_ASSERT(unit != nullptr, "Unit was null");

	BWAPI::Unit depot = getClosestDepot(unit);

	if (depot)
	{
		workerData.setWorkerJob(unit, WorkerData::ReturnCargo, depot);
	}
	else
	{
		// BWAPI::Broodwar->printf("No depot to accept return cargo");
	}
}

BWAPI::Unit WorkerManager::getClosestDepot(BWAPI::Unit worker)
{
	UAB_ASSERT(worker != nullptr, "Worker was null");

	BWAPI::Unit closestDepot = nullptr;
	int closestDistance = 0;

	for (const auto unit : BWAPI::Broodwar->self()->getUnits())
	{
        UAB_ASSERT(unit != nullptr, "Unit was null");

		if (unit->getType().isResourceDepot() &&
			(unit->isCompleted() || unit->getType() == BWAPI::UnitTypes::Zerg_Lair || unit->getType() == BWAPI::UnitTypes::Zerg_Hive) &&
			!workerData.depotIsFull(unit))
		{
			int distance = unit->getDistance(worker);
			if (!closestDepot || distance < closestDistance)
			{
				closestDepot = unit;
				closestDistance = distance;
			}
		}
	}

	return closestDepot;
}

// other managers that need workers call this when they're done with a unit
void WorkerManager::finishedWithWorker(BWAPI::Unit unit) 
{
	UAB_ASSERT(unit != nullptr, "Unit was null");

	workerData.setWorkerJob(unit, WorkerData::Idle, nullptr);
}

// Find a worker to be reassigned to gas duty.
BWAPI::Unit WorkerManager::getGasWorker(BWAPI::Unit refinery)
{
	UAB_ASSERT(refinery != nullptr, "Refinery was null");

	BWAPI::Unit closestWorker = nullptr;
	int closestDistance = 0;

	for (const auto unit : workerData.getWorkers())
	{
		UAB_ASSERT(unit != nullptr, "Unit was null");

		if (workerData.getWorkerJob(unit) == WorkerData::Minerals)
		{
			// Don't waste minerals. It's OK (and unlikely) to already be carrying gas.
			if (unit->isCarryingMinerals() ||                       // doesn't have minerals and
				unit->getOrder() == BWAPI::Orders::MiningMinerals)  // isn't about to get them
			{
				continue;
			}

			int distance = unit->getDistance(refinery);
			if (!closestWorker || distance < closestDistance)
			{
				closestWorker = unit;
				closestDistance = distance;
			}
		}
	}

	return closestWorker;
}

void WorkerManager::setBuildingWorker(BWAPI::Unit worker, Building & b)
{
     UAB_ASSERT(worker != nullptr, "Worker was null");

	 workerData.setWorkerJob(worker, WorkerData::Build, b.type);
}

// Get a builder for BuildingManager.
// if setJobAsBuilder is true (default), it will be flagged as a builder unit
// set 'setJobAsBuilder' to false if we just want to see which worker will build a building
BWAPI::Unit WorkerManager::getBuilder(const Building & b, bool setJobAsBuilder)
{
	// variables to hold the closest worker of each type to the building
	BWAPI::Unit closestMovingWorker = nullptr;
	BWAPI::Unit closestMiningWorker = nullptr;
	int closestMovingWorkerDistance = 0;
	int closestMiningWorkerDistance = 0;

	// look through each worker that had moved there first
	for (const auto unit : workerData.getWorkers())
	{
        UAB_ASSERT(unit != nullptr, "Unit was null");

        // gas steal building uses scout worker
        if (b.isGasSteal && (workerData.getWorkerJob(unit) == WorkerData::Scout))
        {
            if (setJobAsBuilder)
            {
                workerData.setWorkerJob(unit, WorkerData::Build, b.type);
            }
            return unit;
        }

		// if anti terran choke and we should build a enemy natural base, we can choose a scout worker
		if (StateManager::Instance().being_terran_choke &&
			unit->isCompleted() &&
			(workerData.getWorkerJob(unit) == WorkerData::Scout) &&
			b.macroLocation == MacroLocation::EnemyNatural)
		{
			if (setJobAsBuilder)
			{
				ScoutManager::Instance().releaseWorkerScout();
				workerData.setWorkerJob(unit, WorkerData::Build, b.type);
				Micro::SmartMove(unit, BWAPI::Position(StateManager::Instance().anti_terran_choke_pos));
			}
			return unit;
		}

		// mining worker check
		if (unit->isCompleted() && (workerData.getWorkerJob(unit) == WorkerData::Minerals))
		{
			// if it is a new closest distance, set the pointer
			int distance = unit->getDistance(BWAPI::Position(b.finalPosition));
			if (unit->isCarryingMinerals() || unit->isCarryingGas() ||
				unit->getOrder() == BWAPI::Orders::MiningMinerals)
			{
				// If it has cargo or is busy getting some, pretend it is farther away.
				// That way we prefer empty workers and lose less cargo.
				distance += 96;
			}
			if (!closestMiningWorker || distance < closestMiningWorkerDistance)
			{
				closestMiningWorker = unit;
				closestMiningWorkerDistance = distance;
			}
		}

		// moving worker check
		if (unit->isCompleted() && (workerData.getWorkerJob(unit) == WorkerData::Move))
		{
			// if it is a new closest distance, set the pointer
			int distance = unit->getDistance(BWAPI::Position(b.finalPosition));
			if (unit->isCarryingMinerals() || unit->isCarryingGas() ||
				unit->getOrder() == BWAPI::Orders::MiningMinerals) {
				// If it has cargo or is busy getting some, pretend it is farther away.
				// That way we prefer empty workers and lose less cargo.
				distance += 96;
			}
			if (!closestMovingWorker || distance < closestMovingWorkerDistance)
			{
				closestMovingWorker = unit;
				closestMovingWorkerDistance = distance;
			}
		}
	}

	// if we found a moving worker, use it, otherwise using a mining worker
	BWAPI::Unit chosenWorker = closestMovingWorker ? closestMovingWorker : closestMiningWorker;

	// if the worker exists (one may not have been found in rare cases)
	if (chosenWorker && setJobAsBuilder)
	{
		workerData.setWorkerJob(chosenWorker, WorkerData::Build, b.type);
	}

	return chosenWorker;
}

// sets a worker as a scout
void WorkerManager::setScoutWorker(BWAPI::Unit worker)
{
	UAB_ASSERT(worker != nullptr, "Worker was null");

	workerData.setWorkerJob(worker, WorkerData::Scout, nullptr);
}

// gets a worker which will move to a current location
BWAPI::Unit WorkerManager::getMoveWorker(BWAPI::Position p)
{
	BWAPI::Unit closestWorker = nullptr;
	int closestDistance = 0;

	for (const auto unit : workerData.getWorkers())
	{
        UAB_ASSERT(unit != nullptr, "Unit was null");

		// only consider it if it's a mineral worker
		if (unit->isCompleted() && workerData.getWorkerJob(unit) == WorkerData::Minerals)
		{
			// if it is a new closest distance, set the pointer
			int distance = unit->getDistance(p);
			if (unit->isCarryingMinerals() || unit->isCarryingGas() ||
				unit->getOrder() == BWAPI::Orders::MiningMinerals) {
				// If it has cargo or is busy getting some, pretend it is farther away.
				// That way we prefer empty workers and lose less cargo.
				distance += 96;
			}
			if (!closestWorker || distance < closestDistance)
			{
				closestWorker = unit;
				closestDistance = distance;
			}
		}
	}

	return closestWorker;
}

// Sets a worker to move to a given location. Use getMoveWorker() to choose the worker.
void WorkerManager::setMoveWorker(BWAPI::Unit worker, int mineralsNeeded, int gasNeeded, BWAPI::Position & p)
{
	UAB_ASSERT(worker && p.isValid(), "bad call");
	workerData.setWorkerJob(worker, WorkerData::Move, WorkerMoveData(mineralsNeeded, gasNeeded, p));
}

// will we have the required resources by the time a worker can travel the given distance
bool WorkerManager::willHaveResources(int mineralsRequired, int gasRequired, double distance)
{
	// if we don't require anything, we will have it
	if (mineralsRequired <= 0 && gasRequired <= 0)
	{
		return true;
	}

	double speed = BWAPI::Broodwar->self()->getRace().getWorker().topSpeed();

	// how many frames it will take us to move to the building location
	// add a little to account for worker getting stuck. better early than late
	double framesToMove = (distance / speed) + 24;

	// magic numbers to predict income rates
	double mineralRate = getNumMineralWorkers() * 0.045;
	double gasRate     = getNumGasWorkers() * 0.07;

	// calculate if we will have enough by the time the worker gets there
	return
		mineralRate * framesToMove >= mineralsRequired &&
		gasRate * framesToMove >= gasRequired;
}

void WorkerManager::setCombatWorker(BWAPI::Unit worker)
{
	UAB_ASSERT(worker != nullptr, "Worker was null");

	workerData.setWorkerJob(worker, WorkerData::Combat, nullptr);
}

void WorkerManager::onUnitMorph(BWAPI::Unit unit)
{
	UAB_ASSERT(unit != nullptr, "Unit was null");

	// if something morphs into a worker, add it
	if (unit->getType().isWorker() && unit->getPlayer() == BWAPI::Broodwar->self() && unit->getHitPoints() >= 0)
	{
		workerData.addWorker(unit);
	}

	// if something morphs into a building, was it a drone?
	if (unit->getType().isBuilding() && unit->getPlayer() == BWAPI::Broodwar->self() && unit->getPlayer()->getRace() == BWAPI::Races::Zerg)
	{
		workerData.workerDestroyed(unit);
	}
}

void WorkerManager::onUnitShow(BWAPI::Unit unit)
{
	UAB_ASSERT(unit && unit->exists(), "bad unit");

	// add the depot if it exists
	if (unit->getType().isResourceDepot() && unit->getPlayer() == BWAPI::Broodwar->self())
	{
		workerData.addDepot(unit);
	}

	// if something morphs into a worker, add it
	if (unit->getType().isWorker() && unit->getPlayer() == BWAPI::Broodwar->self() && unit->getHitPoints() > 0)
	{
		workerData.addWorker(unit);
	}
}

void WorkerManager::rebalanceWorkers()
{
	for (const auto worker : workerData.getWorkers())
	{
        UAB_ASSERT(worker != nullptr, "Worker was null");

		if (!workerData.getWorkerJob(worker) == WorkerData::Minerals)
		{
			continue;
		}

		BWAPI::Unit depot = workerData.getWorkerDepot(worker);

		if (depot && workerData.depotIsFull(depot))
		{
			workerData.setWorkerJob(worker, WorkerData::Idle, nullptr);
		}
		else if (!depot)
		{
			workerData.setWorkerJob(worker, WorkerData::Idle, nullptr);
		}
	}
}

void WorkerManager::onUnitDestroy(BWAPI::Unit unit) 
{
	UAB_ASSERT(unit != nullptr, "Unit was null");

	if (unit->getType().isResourceDepot() && unit->getPlayer() == BWAPI::Broodwar->self())
	{
		workerData.removeDepot(unit);
	}

	if (unit->getType().isWorker() && unit->getPlayer() == BWAPI::Broodwar->self()) 
	{
		workerData.workerDestroyed(unit);
	}

	if (unit->getType() == BWAPI::UnitTypes::Resource_Mineral_Field)
	{
		rebalanceWorkers();
	}
}

void WorkerManager::drawResourceDebugInfo() 
{
    if (!Config::Debug::DrawResourceInfo)
    {
        return;
    }

	for (const auto worker : workerData.getWorkers()) 
    {
        UAB_ASSERT(worker != nullptr, "Worker was null");

		char job = workerData.getJobCode(worker);

		BWAPI::Position pos = worker->getTargetPosition();

		BWAPI::Broodwar->drawTextMap(worker->getPosition().x, worker->getPosition().y - 5, "\x07%c", job);
		BWAPI::Broodwar->drawTextMap(worker->getPosition().x, worker->getPosition().y + 5, "\x03%s", worker->getOrder().getName().c_str());

		BWAPI::Broodwar->drawLineMap(worker->getPosition().x, worker->getPosition().y, pos.x, pos.y, BWAPI::Colors::Cyan);

		BWAPI::Unit depot = workerData.getWorkerDepot(worker);
		if (depot)
		{
			BWAPI::Broodwar->drawLineMap(worker->getPosition().x, worker->getPosition().y, depot->getPosition().x, depot->getPosition().y, BWAPI::Colors::Orange);
		}
	}
}

void WorkerManager::drawWorkerInformation(int x, int y) 
{
    if (!Config::Debug::DrawWorkerInfo)
    {
        return;
    }

	BWAPI::Broodwar->drawTextScreen(x, y, "\x04 Workers %d", workerData.getNumMineralWorkers());
	BWAPI::Broodwar->drawTextScreen(x, y+20, "\x04 UnitID");
	BWAPI::Broodwar->drawTextScreen(x+50, y+20, "\x04 State");

	int yspace = 0;

	for (const auto unit : workerData.getWorkers())
	{
        UAB_ASSERT(unit != nullptr, "Worker was null");

		BWAPI::Broodwar->drawTextScreen(x, y+40+((yspace)*10), "\x03 %d", unit->getID());
		BWAPI::Broodwar->drawTextScreen(x+50, y+40+((yspace++)*10), "\x03 %c", workerData.getJobCode(unit));
	}
}

bool WorkerManager::isFree(BWAPI::Unit worker)
{
    UAB_ASSERT(worker != nullptr, "Worker was null");

	WorkerData::WorkerJob job = workerData.getWorkerJob(worker);
	return job == WorkerData::Minerals || job == WorkerData::Idle;
}

bool WorkerManager::isWorkerScout(BWAPI::Unit worker)
{
	UAB_ASSERT(worker != nullptr, "Worker was null");

	return workerData.getWorkerJob(worker) == WorkerData::Scout;
}

bool WorkerManager::isCombatWorker(BWAPI::Unit worker)
{
    UAB_ASSERT(worker != nullptr, "Worker was null");

	return workerData.getWorkerJob(worker) == WorkerData::Combat;
}

bool WorkerManager::isBuilder(BWAPI::Unit worker)
{
    UAB_ASSERT(worker != nullptr, "Worker was null");

	return workerData.getWorkerJob(worker) == WorkerData::Build;
}

int WorkerManager::getNumMineralWorkers() const
{
	return workerData.getNumMineralWorkers();	
}

int WorkerManager::getNumGasWorkers() const
{
	return workerData.getNumGasWorkers();
}

int WorkerManager::getNumReturnCargoWorkers() const
{
	return workerData.getNumReturnCargoWorkers();
}

int WorkerManager::getNumCombatWorkers() const
{
	return workerData.getNumCombatWorkers();
}

int WorkerManager::getNumIdleWorkers() const
{
	return workerData.getNumIdleWorkers();
}

// The largest number of workers that it is efficient to have right now.
// Does not take into account possible preparations for future expansions.
// May not exceed Config::Macro::AbsoluteMaxWorkers.
int WorkerManager::getMaxWorkers() const
{
	int patches = InformationManager::Instance().getMyNumMineralPatches();
	int refineries, geysers;
	InformationManager::Instance().getMyGasCounts(refineries, geysers);

	// Never let the max number of workers fall to 0!
	// Set aside 1 for future opportunities.
	return std::min(
			Config::Macro::AbsoluteMaxWorkers,
			1 + int(std::round(Config::Macro::WorkersPerPatch * patches + Config::Macro::WorkersPerRefinery * refineries))
		);
}

// Mine out any blocking minerals that the worker runs headlong into.
bool WorkerManager::maybeMineMineralBlocks(BWAPI::Unit worker)
{
	UAB_ASSERT(worker, "Worker was null");

	if (worker->isGatheringMinerals() &&
		worker->getTarget() &&
		worker->getTarget()->getInitialResources() <= 16)
	{
		// Still busy mining the block.
		return true;
	}

	for (const auto patch : worker->getUnitsInRadius(64, BWAPI::Filter::IsMineralField))
	{
		if (patch->getInitialResources() <= 16)    // any patch we can mine out quickly
		{
			// Go start mining.
			worker->gather(patch);
			return true;
		}
	}

	return false;
}