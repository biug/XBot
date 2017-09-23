#include "ProductionManager.h"
#include "GameCommander.h"
#include "StateManager.h"
#include "UnitUtil.h"

using namespace XBot;

ProductionManager::ProductionManager()
	: _lastProductionFrame				 (0)
	, _assignedWorkerForThisBuilding     (nullptr)
	, _haveLocationForThisBuilding       (false)
	, _delayBuildingPredictionUntilFrame (0)
	, _lastCreateFrame					 (0)
	, _outOfBook                         (false)
	, _targetGasAmount                   (0)
	, _targetMineralAmount				 (0)
	, _extractorTrickState			     (ExtractorTrick::None)
	, _extractorTrickUnitType			 (BWAPI::UnitTypes::None)
	, _extractorTrickBuilding			 (nullptr)
	, _untilFindEnemy				 (false)
{
    setBuildOrder(StrategyManager::Instance().getOpeningBookBuildOrder());
}

void ProductionManager::setBuildOrder(const BuildOrder & buildOrder)
{
	_queue.clearAll();

	for (size_t i(0); i<buildOrder.size(); ++i)
	{
		_queue.queueAsLowestPriority(buildOrder[i]);
	}
	_queue.resetModified();
}

void ProductionManager::update() 
{
	// TODO move this to worker manager and make it more precise; it often goes a little over
	// If we have reached a target amount of gas, take workers off gas.
	if (_targetGasAmount && BWAPI::Broodwar->self()->gatheredGas() >= _targetGasAmount)  // tends to go over
	{
		WorkerManager::Instance().setCollectGas(false);
		_targetGasAmount = 0;           // clear the target
	}
	if (_targetMineralAmount && BWAPI::Broodwar->self()->minerals() >= _targetMineralAmount)
	{
		_targetMineralAmount = 0;
	}
	// if has mineral target, wait until it comes to zero
	if (_targetMineralAmount)
	{
		return;
	}

	// update status
	StateManager::Instance().updateCurrentState(_queue);
	// If we're in trouble, adjust the production queue to help.
	// Includes scheduling supply as needed.
	StrategyManager::Instance().handleUrgentProductionIssues(_queue);

	// if nothing is currently building, get a new goal from the strategy manager
	if (_queue.isEmpty())
	{
		if (Config::Debug::DrawBuildOrderSearchInfo)
		{
			BWAPI::Broodwar->drawTextScreen(150, 10, "Nothing left to build, replanning.");
		}

		goOutOfBook();
		StrategyManager::Instance().freshProductionPlan();
	}

	// Build stuff from the production queue.
	manageBuildOrderQueue();
}

// If something important was destroyed, we may want to react.
void ProductionManager::onUnitDestroy(BWAPI::Unit unit)
{
	// If it's not our unit, we don't care.
	// This mostly works for zerg, but the extractor trick confuses it.
	// So bail out if the extractor trick is underway.
	if (!unit || unit->getPlayer() != BWAPI::Broodwar->self() ||
		BWAPI::Broodwar->self()->getRace() == BWAPI::Races::Zerg)
	{
		return;
	}
	
	// If it's a worker or a building, it affects the production plan.
	if (unit->getType().isWorker() && !_outOfBook)
	{
		// We lost a worker in the opening. Replace it.
		// It helps if a small number of workers are killed. If many are killed, you're toast anyway.
		// Still, it's better than breaking out of the opening altogether.
		_queue.queueAsHighestPriority(unit->getType());

		// If we have a gateway and no zealots, or a barracks and no marines,
		// consider making a military unit first. To, you know, stay alive and stuff.
		if (BWAPI::Broodwar->self()->getRace() == BWAPI::Races::Protoss)
		{
			if (UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Protoss_Gateway) > 0 &&
				UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Protoss_Zealot) == 0 &&
				(BWAPI::Broodwar->self()->minerals() >= 150 || UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Protoss_Probe) > 0))
			{
				_queue.queueAsHighestPriority(BWAPI::UnitTypes::Protoss_Zealot);
			}
		}
		else if (BWAPI::Broodwar->self()->getRace() == BWAPI::Races::Terran)
		{
			if (UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Terran_Barracks) > 0 &&
				UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Terran_Marine) == 0 &&
				(BWAPI::Broodwar->self()->minerals() >= 100 || UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Terran_SCV) > 0))
			{
				_queue.queueAsHighestPriority(BWAPI::UnitTypes::Terran_Marine);
			}
		}
		else // Zerg
		{
			if (UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Zerg_Spawning_Pool) > 0 &&
				UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Zerg_Zergling) == 0 &&
				(BWAPI::Broodwar->self()->minerals() >= 100 || UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Zerg_Drone) > 0))
			{
				_queue.queueAsHighestPriority(BWAPI::UnitTypes::Zerg_Zergling);
			}
		}
	}
	else if (unit->getType().isBuilding() && !(UnitUtil::CanAttackAir(unit) || UnitUtil::CanAttackGround(unit)))
	{
		// We lost a building other than static defense. It may be serious. Replan from scratch.
		goOutOfBook();
	}
}

void ProductionManager::manageBuildOrderQueue() 
{
	// If the extractor trick is in progress, do that.
	if (_extractorTrickState != ExtractorTrick::None)
	{
		doExtractorTrick();
		return;
	}

	// If we were planning to build and assigned a worker, but the queue was then
	// changed behind our back, release the worker and continue.
	if (_queue.isModified() && _assignedWorkerForThisBuilding)
	{
		WorkerManager::Instance().finishedWithWorker(_assignedWorkerForThisBuilding);
		_assignedWorkerForThisBuilding = nullptr;
	}

	if (_untilFindEnemy)
	{
		if (StateManager::Instance().enemy_supply_depot == 0)
		{
			return;
		}
		_untilFindEnemy = false;
	}

	// We do nothing if the queue is empty (obviously).
	while (!_queue.isEmpty()) 
	{
		const BuildOrderItem & currentItem = _queue.getHighestPriorityItem();

		// If this is a command, execute it and keep going.
		if (currentItem.macroAct.isCommand())
		{
			executeCommand(currentItem.macroAct.getCommandType());
			_queue.doneWithHighestPriorityItem();
			_lastProductionFrame = BWAPI::Broodwar->getFrameCount();
			continue;
		}

		// The unit which can produce the currentItem. May be null.
        BWAPI::Unit producer = getProducer(currentItem.macroAct, BWAPI::Position(BWAPI::Broodwar->self()->getStartLocation()));

		// Work around a bug: If you say you want 2 comsats total, BOSS may order up 2 comsats more
		// than you already have. So we drop any extra comsat.
		if (!producer && currentItem.macroAct.isUnit() && currentItem.macroAct.getUnitType() == BWAPI::UnitTypes::Terran_Comsat_Station)
		{
			_queue.doneWithHighestPriorityItem();
			_lastProductionFrame = BWAPI::Broodwar->getFrameCount();
			continue;
		}

		// check to see if we can make it right now
		bool canMake = producer && canMakeNow(producer, currentItem.macroAct);

		// if the next item in the list is a building and we can't yet make it
        if (currentItem.macroAct.isBuilding() &&
			!canMake &&
			currentItem.macroAct.whatBuilds().isWorker() &&
			BWAPI::Broodwar->getFrameCount() >= _delayBuildingPredictionUntilFrame)
		{
			// construct a temporary building object
			Building b(currentItem.macroAct.getUnitType(), InformationManager::Instance().getMyMainBaseLocation()->getTilePosition());
			b.macroLocation = currentItem.macroAct.getMacroLocation();
            b.isGasSteal = currentItem.isGasSteal;

			// set the producer as the closest worker, but do not set its job yet
			producer = WorkerManager::Instance().getBuilder(b, false);

			// predict the worker movement to that building location
			// NOTE If the worker is set moving, this sets flag _movingToThisBuildingLocation = true
			//      so that we don't 

			// if we're using anti terran choke, don't predict move
			if (StateManager::Instance().being_terran_choke && b.macroLocation == MacroLocation::EnemyNatural)
			{
				Micro::SmartMove(producer, BWAPI::Position(StateManager::Instance().anti_terran_choke_pos));
			}
			else
			{
				predictWorkerMovement(b);
			}
			break;
		}

		// if we can make the current item
		if (canMake)
		{
			// create it
			create(producer, currentItem);
			_lastCreateFrame = BWAPI::Broodwar->getFrameCount();
			_assignedWorkerForThisBuilding = nullptr;
			_haveLocationForThisBuilding = false;
			_delayBuildingPredictionUntilFrame = 0;

			// and remove it from the _queue
			_queue.doneWithHighestPriorityItem();
			_lastProductionFrame = BWAPI::Broodwar->getFrameCount();

			// don't actually loop around in here
			// TODO because we don't keep track of resources used,
			//      we wait until the next frame to build the next thing.
			//      Can cause delays in late game!
			break;
		}

		// We didn't make anything. Check for a possible production jam.
		// Jams can happen due to bugs, or due to losing prerequisites for planned items.
		if (BWAPI::Broodwar->getFrameCount() > _lastProductionFrame + Config::Macro::ProductionJamFrameLimit)
		{
			// Looks very like a jam. Clear the queue and hope for better luck next time.
			// BWAPI::Broodwar->printf("breaking a production jam");
			goOutOfBook();
		}

		// TODO not much of a loop, eh? breaks on all branches
		//      only commands and bug workarounds continue to the next item
		break;
	}
}

// May return null if no producer is found.
// NOTE closestTo defaults to BWAPI::Positions::None, meaning we don't care.
BWAPI::Unit ProductionManager::getProducer(MacroAct t, BWAPI::Position closestTo)
{
	UAB_ASSERT(!t.isCommand(), "no producer of a command");

    // get the type of unit that builds this
    BWAPI::UnitType producerType = t.whatBuilds();

    // make a set of all candidate producers
    BWAPI::Unitset candidateProducers;
    for (const auto unit : BWAPI::Broodwar->self()->getUnits())
    {
        // Reasons that a unit cannot produce the desired type:

		if (producerType != unit->getType()) { continue; }

		// TODO Due to a BWAPI 4.1.2 bug, lair research can't be done in a hive.
		//      Also spire upgrades can't be done in a greater spire.
		//      The bug is fixed in the next version, 4.2.0.
		//      When switching to a fixed version, change the above line to the following:
		// If the producerType is a lair, a hive will do as well.
		// Note: Burrow research in a hatchery can also be done in a lair or hive, but we rarely want to.
		// Ignore the possibility so that we don't accidentally waste lair time.
		//if (!(
		//	producerType == unit->getType() ||
		//	producerType == BWAPI::UnitTypes::Zerg_Lair && unit->getType() == BWAPI::UnitTypes::Zerg_Hive ||
		//  producerType == BWAPI::UnitTypes::Zerg_Spire && unit->getType() == BWAPI::UnitTypes::Zerg_Greater_Spire
		//	))
		//{
		//	continue;
		//}

        if (!unit->isCompleted())  { continue; }
        if (unit->isTraining())    { continue; }
        if (unit->isLifted())      { continue; }
        if (!unit->isPowered())    { continue; }
		if (unit->isUpgrading())   { continue; }
		if (unit->isResearching()) { continue; }
        
        // if a unit requires an addon and the producer doesn't have one
		// TODO Addons seem a bit erratic. Bugs are likely.
		// TODO What exactly is requiredUnits()? On the face of it, the story is that
		//      this code is for e.g. making tanks, built in a factory which has a machine shop.
		//      Research that requires an addon is done in the addon, a different case.
		//      Apparently wrong for e.g. ghosts, which require an addon not on the producer.
		if (t.isUnit())
		{
			bool reject = false;   // innocent until proven guilty
			typedef std::pair<BWAPI::UnitType, int> ReqPair;
			for (const ReqPair & pair : t.getUnitType().requiredUnits())
			{
				BWAPI::UnitType requiredType = pair.first;
				if (requiredType.isAddon())
				{
					if (!unit->getAddon() || (unit->getAddon()->getType() != requiredType))
					{
						reject = true;
						break;     // out of inner loop
					}
				}
			}
			if (reject)
			{
				continue;
			}
		}

        // if we haven't rejected it, add it to the set of candidates
        candidateProducers.insert(unit);
    }

	// Trick: If we're producing a worker, choose the producer (command center, nexus,
	// or larva) which is farthest from the main base. That way expansions are preferentially
	// populated with less need to transfer workers.
	if (t.isUnit() && t.getUnitType().isWorker())
	{
		return getFarthestUnitFromPosition(candidateProducers,
			InformationManager::Instance().getMyMainBaseLocation()->getPosition());
	}
	else
	{
		return getClosestUnitToPosition(candidateProducers, closestTo);
	}
}

BWAPI::Unit ProductionManager::getClosestUnitToPosition(const BWAPI::Unitset & units, BWAPI::Position closestTo)
{
    if (units.size() == 0)
    {
        return nullptr;
    }

    // if we don't care where the unit is return the first one we have
    if (closestTo == BWAPI::Positions::None)
    {
        return *(units.begin());
    }

    BWAPI::Unit closestUnit = nullptr;
    int minDist(1000000);

	for (const auto unit : units) 
    {
        UAB_ASSERT(unit != nullptr, "Unit was null");

		int distance = unit->getDistance(closestTo);
		if (distance < minDist) 
        {
			closestUnit = unit;
			minDist = distance;
		}
	}

    return closestUnit;
}

BWAPI::Unit ProductionManager::getFarthestUnitFromPosition(const BWAPI::Unitset & units, BWAPI::Position farthest)
{
	if (units.size() == 0)
	{
		return nullptr;
	}

	// if we don't care where the unit is return the first one we have
	if (farthest == BWAPI::Positions::None)
	{
		return *(units.begin());
	}

	BWAPI::Unit farthestUnit = nullptr;
	int maxDist(-1);

	for (const auto unit : units)
	{
		UAB_ASSERT(unit != nullptr, "Unit was null");

		int distance = unit->getDistance(farthest);
		if (distance > maxDist)
		{
			farthestUnit = unit;
			maxDist = distance;
		}
	}

	return farthestUnit;
}

BWAPI::Unit ProductionManager::getClosestLarvaToPosition(BWAPI::Position closestTo)
{
	BWAPI::Unitset larvas;
	for (const auto unit : BWAPI::Broodwar->self()->getUnits())
	{
		if (unit->getType() == BWAPI::UnitTypes::Zerg_Larva)
		{
			larvas.insert(unit);
		}
	}

	return getClosestUnitToPosition(larvas, closestTo);
}

// Create a unit or start research.
bool ProductionManager::create(BWAPI::Unit producer, const BuildOrderItem & item) 
{
    if (!producer)
    {
        return false;
    }

    MacroAct act = item.macroAct;

	// If it's a building other than an add-on.
	if (act.isBuilding()                                    // implies act.isUnit()
		&& !UnitUtil::IsMorphedBuildingType(act.getUnitType()))  // not morphed from another zerg building, not built
	{
		// Every once in a while, pick a new base as the "main" base to build in.
		if (act.getRace() != BWAPI::Races::Protoss || act.getUnitType() == BWAPI::UnitTypes::Protoss_Pylon)
		{
			InformationManager::Instance().maybeChooseNewMainBase();
		}

		// By default, build in the main base.
		// BuildingManager will override the location if it needs to.
		// Otherwise it will find some spot near desiredLocation.
		BWAPI::TilePosition desiredLocation = BWAPI::Broodwar->self()->getStartLocation();

		if (act.getMacroLocation() == MacroLocation::Natural)
		{
			BWTA::BaseLocation * natural = InformationManager::Instance().getMyNaturalLocation();
			if (natural)
			{
				desiredLocation = natural->getTilePosition();
			}
		}
		
		BuildingManager::Instance().addBuildingTask(act, desiredLocation, item.isGasSteal);
		return true;
	}
	// if we're dealing with a non-building unit, or a morphed zerg building
	else if (act.isUnit())
	{
		// if the race is zerg, morph the unit
			
		if (producer->morph(act.getUnitType()))
		{
			return true;
		}
		else
		{
			return false;
		}
	}
	// if we're dealing with a tech research
	else if (act.isTech())
	{
		return producer->research(act.getTechType());
	}
	else if (act.isUpgrade())
	{
		return producer->upgrade(act.getUpgradeType());
	}
	else
	{
		return false;
		UAB_ASSERT(false, "Unknown type");
	}
}

bool ProductionManager::canMakeNow(BWAPI::Unit producer, MacroAct t)
{
	UAB_ASSERT(producer != nullptr, "producer was null");

	bool canMake = meetsReservedResources(t);
	if (canMake)
	{
		if (t.isUnit())
		{
			canMake = BWAPI::Broodwar->canMake(t.getUnitType(), producer);
			if (t.getUnitType() == BWAPI::UnitTypes::Zerg_Sunken_Colony || t.getUnitType() == BWAPI::UnitTypes::Zerg_Spore_Colony)
			{
				canMake &= StateManager::Instance().creep_colony_completed > 0;
			}
		}
		else if (t.isTech())
		{
			canMake = BWAPI::Broodwar->canResearch(t.getTechType(), producer);
		}
		else if (t.isUpgrade())
		{
			canMake = BWAPI::Broodwar->canUpgrade(t.getUpgradeType(), producer);
		}
		else if (t.isCommand())
		{
			canMake = true;     // no-op
		}
		else
		{
			UAB_ASSERT(false, "Unknown type");
		}
	}

	return canMake;
}

// When the next item in the _queue is a building, this checks to see if we should move to
// its location in preparation for construction. If so, it orders the move.
// This function is here as it needs to access prodction manager's reserved resources info.
void ProductionManager::predictWorkerMovement(const Building & b)
{
    if (b.isGasSteal)
    {
        return;
    }

	// get a possible building location for the building
	if (!_haveLocationForThisBuilding)
	{
		_predictedTilePosition = BuildingManager::Instance().getBuildingLocation(b);
	}

	if (_predictedTilePosition != BWAPI::TilePositions::None)
	{
		_haveLocationForThisBuilding = true;
	}
	else
	{
		// BWAPI::Broodwar->printf("can't place building %s", b.type.getName().c_str());
		// If we can't place the building now, we probably can't place it next frame either.
		// Delay for a while before trying again. We could overstep the time limit.
		_delayBuildingPredictionUntilFrame = 12 + BWAPI::Broodwar->getFrameCount();
		return;
	}
	
	int x1 = _predictedTilePosition.x * 32;
	int y1 = _predictedTilePosition.y * 32;

	// draw a box where the building will be placed
	if (Config::Debug::DrawWorkerInfo)
    {
		int x2 = x1 + (b.type.tileWidth()) * 32;
		int y2 = y1 + (b.type.tileHeight()) * 32;
		BWAPI::Broodwar->drawBoxMap(x1, y1, x2, y2, BWAPI::Colors::Blue, false);
    }

	// where we want the worker to walk to
	BWAPI::Position walkToPosition		= BWAPI::Position(x1 + (b.type.tileWidth()/2)*32, y1 + (b.type.tileHeight()/2)*32);

	// compute how many resources we need to construct this building
	int mineralsRequired				= std::max(0, b.type.mineralPrice() - getFreeMinerals());
	int gasRequired						= std::max(0, b.type.gasPrice() - getFreeGas());

	// get a candidate worker to move to this location
	BWAPI::Unit moveWorker				= WorkerManager::Instance().getMoveWorker(walkToPosition);

	// Conditions under which to move the worker: 
	//		- there's a valid worker to move
	//		- we haven't yet assigned a worker to move to this location
	//		- the build position is valid
	//		- we will have the required resources by the time the worker gets there
	if (moveWorker &&
		!_assignedWorkerForThisBuilding &&
		_haveLocationForThisBuilding &&
		(_predictedTilePosition != BWAPI::TilePositions::None) &&
		WorkerManager::Instance().willHaveResources(mineralsRequired, gasRequired, moveWorker->getDistance(walkToPosition)) )
	{
		// we have assigned a worker
		_assignedWorkerForThisBuilding = moveWorker;

		// tell the worker manager to move this worker
		WorkerManager::Instance().setMoveWorker(moveWorker, mineralsRequired, gasRequired, walkToPosition);
	}
}

int ProductionManager::getFreeMinerals() const
{
	return BWAPI::Broodwar->self()->minerals() - BuildingManager::Instance().getReservedMinerals();
}

int ProductionManager::getFreeGas() const
{
	return BWAPI::Broodwar->self()->gas() - BuildingManager::Instance().getReservedGas();
}

void ProductionManager::executeCommand(MacroCommand command)
{
	MacroCommandType cmd = command.getType();

	if (cmd == MacroCommandType::Scout)
	{
		GameCommander::Instance().goScoutAlways();
	}
	else if (cmd == MacroCommandType::ScoutIfNeeded)
	{
		GameCommander::Instance().goScoutIfNeeded();
	}
	else if (cmd == MacroCommandType::ScoutLocation)
	{
		GameCommander::Instance().goScoutIfNeeded();
		ScoutManager::Instance().setScoutLocationOnly();
	}
	else if (cmd == MacroCommandType::StopGas)
	{
		WorkerManager::Instance().setCollectGas(false);
	}
	else if (cmd == MacroCommandType::StartGas)
	{
		WorkerManager::Instance().setCollectGas(true);
	}
	else if (cmd == MacroCommandType::GasUntil)
	{
		WorkerManager::Instance().setCollectGas(true);
		_targetGasAmount = BWAPI::Broodwar->self()->gatheredGas()
			- BWAPI::Broodwar->self()->gas()
			+ command.getAmount();
	}
	else if (cmd == MacroCommandType::StealGas)
	{
		ScoutManager::Instance().setGasSteal();
	}
	else if (cmd == MacroCommandType::ExtractorTrickDrone)
	{
		startExtractorTrick(BWAPI::UnitTypes::Zerg_Drone);
	}
	else if (cmd == MacroCommandType::ExtractorTrickZergling)
	{
		startExtractorTrick(BWAPI::UnitTypes::Zerg_Zergling);
	}
	else if (cmd == MacroCommandType::Aggressive)
	{
		CombatCommander::Instance().setAggression(true);
	}
	else if (cmd == MacroCommandType::Defensive)
	{
		CombatCommander::Instance().setAggression(false);
	}
	else if (cmd == MacroCommandType::PullWorkers)
	{
		CombatCommander::Instance().pullWorkers(command.getAmount());
	}
	else if (cmd == MacroCommandType::PullWorkersLeaving)
	{
		int nWorkers = WorkerManager::Instance().getNumMineralWorkers() + WorkerManager::Instance().getNumGasWorkers();
		CombatCommander::Instance().pullWorkers(nWorkers - command.getAmount());
	}
	else if (cmd == MacroCommandType::ReleaseWorkers)
	{
		CombatCommander::Instance().releaseWorkers();
	}
	else if (cmd == MacroCommandType::UntilFindEnemy)
	{
		BWAPI::Broodwar->printf("WAIT UNTIL FIND ENEMY");
		_untilFindEnemy = true;
	}
	else if (cmd == MacroCommandType::DroneEnemyNatural)
	{
		if (StateManager::Instance().being_terran_choke)
		{
			auto targetPos = BWAPI::Position(StateManager::Instance().anti_terran_choke_pos);
			auto worker = WorkerManager::Instance().getMoveWorker(targetPos);
			if (worker)
			{
				WorkerManager::Instance().setMoveWorker(worker, 75, 0, targetPos);
			}
		}
	}
	else if (cmd == MacroCommandType::IgnoreScoutWorker)
	{
		StateManager::Instance().ignore_scout_worker = true;
	}
	else if (cmd == MacroCommandType::WaitMineralUntil)
	{
		_targetMineralAmount = command.getAmount();
	}
	else if (cmd == MacroCommandType::KeepBuildSunken)
	{
		StateManager::Instance().keep_build_sunken = true;
	}
	else if (cmd == MacroCommandType::RallyAtNatural)
	{
		StateManager::Instance().rally_at_natural = true;
	}
	else if (cmd == MacroCommandType::StopRally)
	{
		StateManager::Instance().rally_at_natural = false;
	}
	else
	{
		UAB_ASSERT(false, "unknown MacroCommand");
	}
}

// Can we afford it, taking into account reserved resources?
bool ProductionManager::meetsReservedResources(MacroAct act)
{
	int frame = BWAPI::Broodwar->getFrameCount();
	if (act.gasPrice() == 0)
	{
		if (getFreeMinerals() <= 100 && frame - _lastCreateFrame < 10)
		{
			return false;
		}
	}
	return (act.mineralPrice() <= getFreeMinerals()) && (act.gasPrice() <= getFreeGas());
}

void ProductionManager::drawProductionInformation(int x, int y)
{
    if (!Config::Debug::DrawProductionInfo)
    {
        return;
    }

	y += 10;
	if (_extractorTrickState == ExtractorTrick::None)
	{
		if (WorkerManager::Instance().isCollectingGas())
		{
			if (_targetGasAmount)
			{
				BWAPI::Broodwar->drawTextScreen(x - 10, y, "\x04 gas %d, target %d", BWAPI::Broodwar->self()->gatheredGas(), _targetGasAmount);
			}
			else
			{
				BWAPI::Broodwar->drawTextScreen(x - 10, y, "\x04 gas %d", BWAPI::Broodwar->self()->gatheredGas());
			}
		}
		else
		{
			BWAPI::Broodwar->drawTextScreen(x - 10, y, "\x04 gas %d, stopped", BWAPI::Broodwar->self()->gatheredGas());
		}
	}
	else if (_extractorTrickState == ExtractorTrick::Start)
	{
		BWAPI::Broodwar->drawTextScreen(x - 10, y, "\x04 extractor trick: start");
	}
	else if (_extractorTrickState == ExtractorTrick::ExtractorOrdered)
	{
		BWAPI::Broodwar->drawTextScreen(x - 10, y, "\x04 extractor trick: extractor ordered");
	}
	else if (_extractorTrickState == ExtractorTrick::UnitOrdered)
	{
		BWAPI::Broodwar->drawTextScreen(x - 10, y, "\x04 extractor trick: unit ordered");
	}
	y += 2;

	// fill prod with each unit which is under construction
	std::vector<BWAPI::Unit> prod;
	for (auto & unit : BWAPI::Broodwar->self()->getUnits())
	{
        UAB_ASSERT(unit != nullptr, "Unit was null");

		if (unit->isBeingConstructed())
		{
			prod.push_back(unit);
		}
	}
	
	// sort it based on the time it was started
	std::sort(prod.begin(), prod.end(), CompareWhenStarted());

	for (auto & unit : prod) 
    {
		y += 10;

		BWAPI::UnitType t = unit->getType();
        if (t == BWAPI::UnitTypes::Zerg_Egg)
        {
            t = unit->getBuildType();
        }

		BWAPI::Broodwar->drawTextScreen(x, y, " %c%s", green, TrimRaceName(t.getName()).c_str());
		BWAPI::Broodwar->drawTextScreen(x - 35, y, "%c%6d", green, unit->getRemainingBuildTime());
	}

	_queue.drawQueueInformation(x, y+10, _outOfBook);
}

ProductionManager & ProductionManager::Instance()
{
	static ProductionManager instance;
	return instance;
}

void ProductionManager::queueGasSteal()
{
    _queue.queueAsHighestPriority(MacroAct(BWAPI::Broodwar->self()->getRace().getRefinery()), true);
	_queue.resetModified();
}

// We're zerg and doing the extractor trick to get an extra drone or pair of zerglings,
// as specified in the argument.
// Set a flag to start the procedure, and handle various error cases.
void ProductionManager::startExtractorTrick(BWAPI::UnitType type)
{
	// Only zerg can do the extractor trick.
	if (BWAPI::Broodwar->self()->getRace() != BWAPI::Races::Zerg)
	{
		return;
	}

	// If we're not supply blocked, then we may have lost units earlier.
	// We may or may not have a larva available now, so instead of finding a larva and
	// morphing the unit here, we set a special case extractor trick state to do it
	// when a larva becomes available.
	// We can't queue a unit, because when we return the caller will delete the front queue
	if (BWAPI::Broodwar->self()->supplyTotal() - BWAPI::Broodwar->self()->supplyUsed() >= 2)
	{
		if (_extractorTrickUnitType != BWAPI::UnitTypes::None)
		{
			_extractorTrickState = ExtractorTrick::MakeUnitBypass;
			_extractorTrickUnitType = type;
		}
		return;
	}
	
	// We need a free drone to execute the trick.
	if (WorkerManager::Instance().getNumMineralWorkers() <= 0)
	{
		return;
	}

	// And we need a free geyser to do it on.
	if (BuildingPlacer::Instance().getRefineryPosition() == BWAPI::TilePositions::None)
	{
		return;
	}
	
	_extractorTrickState = ExtractorTrick::Start;
	_extractorTrickUnitType = type;
}

// The extractor trick is in progress. Take the next step, when possible.
// At most one step occurs per frame.
void ProductionManager::doExtractorTrick()
{
	if (_extractorTrickState == ExtractorTrick::Start)
	{
		UAB_ASSERT(!_extractorTrickBuilding, "already have an extractor trick building");
		int nDrones = WorkerManager::Instance().getNumMineralWorkers();
		if (nDrones <= 0)
		{
			// Oops, we can't do it without a free drone. Give up.
			_extractorTrickState = ExtractorTrick::None;
		}
		// If there are "many" drones mining, assume we'll get resources to finish the trick.
		// Otherwise wait for the full 100 before we start.
		// NOTE 100 assumes we are making a drone or a pair of zerglings.
		else if (getFreeMinerals() >= 100 || (nDrones >= 6 && getFreeMinerals() >= 76))
		{
			// We also need a larva to make the drone.
			if (BWAPI::Broodwar->self()->completedUnitCount(BWAPI::UnitTypes::Zerg_Larva) > 0)
			{
				BWAPI::TilePosition loc = BWAPI::TilePosition(0, 0);     // this gets ignored
				Building & b = BuildingManager::Instance().addTrackedBuildingTask(MacroAct(BWAPI::UnitTypes::Zerg_Extractor), loc, false);
				_extractorTrickState = ExtractorTrick::ExtractorOrdered;
				_extractorTrickBuilding = &b;    // points into building manager's queue of buildings
			}
		}
	}
	else if (_extractorTrickState == ExtractorTrick::ExtractorOrdered)
	{
		if (_extractorTrickUnitType == BWAPI::UnitTypes::None)
		{
			_extractorTrickState = ExtractorTrick::UnitOrdered;
		}
		else
		{
			int supplyAvail = BWAPI::Broodwar->self()->supplyTotal() - BWAPI::Broodwar->self()->supplyUsed();
			if (supplyAvail >= 2 &&
				getFreeMinerals() >= _extractorTrickUnitType.mineralPrice() &&
				getFreeGas() >= _extractorTrickUnitType.gasPrice())
			{
				// We can build a unit now: The extractor started, or another unit died somewhere.
				// Well, there is one more condition: We need a larva.
				BWAPI::Unit larva = getClosestLarvaToPosition(BWAPI::Position(InformationManager::Instance().getMyMainBaseLocation()->getTilePosition()));
				if (larva && _extractorTrickUnitType != BWAPI::UnitTypes::None)
				{
					if (_extractorTrickUnitType == BWAPI::UnitTypes::Zerg_Zergling &&
						UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Zerg_Spawning_Pool) == 0)
					{
						// We want a zergling but don't have the tech.
						// Give up by doing nothing and moving on.
					}
					else
					{
						larva->morph(_extractorTrickUnitType);
					}
					_extractorTrickState = ExtractorTrick::UnitOrdered;
				}
			}
			else if (supplyAvail < -2)
			{
				// Uh oh, we must have lost an overlord or a hatchery. Give up by moving on.
				_extractorTrickState = ExtractorTrick::UnitOrdered;
			}
			else if (WorkerManager::Instance().getNumMineralWorkers() <= 0)
			{
				// Drone massacre, or all drones pulled to fight. Give up by moving on.
				_extractorTrickState = ExtractorTrick::UnitOrdered;
			}
		}
	}
	else if (_extractorTrickState == ExtractorTrick::UnitOrdered)
	{
		UAB_ASSERT(_extractorTrickBuilding, "no extractor to cancel");
		BuildingManager::Instance().cancelBuilding(*_extractorTrickBuilding);
		_extractorTrickState = ExtractorTrick::None;
		_extractorTrickUnitType = BWAPI::UnitTypes::None;
		_extractorTrickBuilding = nullptr;
	}
	else if (_extractorTrickState == ExtractorTrick::MakeUnitBypass)
	{
		// We did the extractor trick when we didn't need to, whether because the opening was
		// miswritten or because units were lost before we got here.
		// This special state lets us construct the unit we want anyway, bypassing the extractor.
		BWAPI::Unit larva = getClosestLarvaToPosition(BWAPI::Position(InformationManager::Instance().getMyMainBaseLocation()->getTilePosition()));
		if (larva &&
			getFreeMinerals() >= _extractorTrickUnitType.mineralPrice() &&
			getFreeGas() >= _extractorTrickUnitType.gasPrice())
		{
			larva->morph(_extractorTrickUnitType);
			_extractorTrickState = ExtractorTrick::None;
		}
	}
	else
	{
		UAB_ASSERT(false, "unexpected extractor trick state (possibly None)");
	}
}

// The next item in the queue is a building that requires a worker to construct.
// Addons and morphed buildings (e.g. lair) do not need a worker.
bool ProductionManager::nextIsBuilding() const
{
	if (_queue.isEmpty())
	{
		return false;
	}

	const MacroAct & next = _queue.getHighestPriorityItem().macroAct;

	return next.isUnit() &&
		next.getUnitType().isBuilding() &&
		!next.getUnitType().isAddon() &&
		!UnitUtil::IsMorphedBuildingType(next.getUnitType());
}

// We have finished our book line, or are breaking out of it early.
// Clear the queue, set _outOfBook, go aggressive.
void ProductionManager::goOutOfBook()
{
	_queue.clearAll();
	_outOfBook = true;
	CombatCommander::Instance().setAggression(true);
}