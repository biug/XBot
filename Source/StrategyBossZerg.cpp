#include "StrategyBossZerg.h"

#include "InformationManager.h"
#include "ProductionManager.h"
#include "ScoutManager.h"
#include "StateManager.h"
#include "UnitUtil.h"
#include "ModelWeightInit.h"

using namespace XBot;

StrategyBossZerg::StrategyBossZerg()
	: _self(BWAPI::Broodwar->self())
	, _enemy(BWAPI::Broodwar->enemy())
	, _enemyRace(_enemy->getRace())
	, _techTarget(TechUnit::None)
	, _extraDronesWanted(0)
	, _latestBuildOrder(BWAPI::Races::Zerg)
	, _emergencyGroundDefense(false)
	, _emergencyStartFrame(-1)
	, _existingSupply(-1)
	, _pendingSupply(-1)
	, _lastUpdateFrame(-1)
{
	resetTechScores();
	setUnitMix(BWAPI::UnitTypes::Zerg_Drone, BWAPI::UnitTypes::None);
	chooseAuxUnit();          // it chooses None initially
	chooseEconomyRatio();
}

// -- -- -- -- -- -- -- -- -- -- --
// Private methods.

// Calculate supply existing, pending, and used.
// FOr pending supply, we need to know about overlords just hatching.
// For supply used, the BWAPI self->supplyUsed() can be slightly wrong,
// especially when a unit is just started or just died. 
void StrategyBossZerg::updateSupply()
{
	int existingSupply = 0;
	int pendingSupply = 0;
	int supplyUsed = 0;

	for (auto & unit : _self->getUnits())
	{
		if (unit->getType() == BWAPI::UnitTypes::Zerg_Overlord)
		{
			if (unit->getOrder() == BWAPI::Orders::ZergBirth)
			{
				// Overlord is just hatching and doesn't provide supply yet.
				pendingSupply += 16;
			}
			else
			{
				existingSupply += 16;
			}
		}
		else if (unit->getType() == BWAPI::UnitTypes::Zerg_Egg)
		{
			if (unit->getBuildType() == BWAPI::UnitTypes::Zerg_Overlord) {
				pendingSupply += 16;
			}
			else if (unit->getBuildType().isTwoUnitsInOneEgg())
			{
				supplyUsed += 2 * unit->getBuildType().supplyRequired();
			}
			else
			{
				supplyUsed += unit->getBuildType().supplyRequired();
			}
		}
		else if (unit->getType() == BWAPI::UnitTypes::Zerg_Hatchery && !unit->isCompleted())
		{
			// Don't count this. Hatcheries build too slowly and provide too little.
			// pendingSupply += 2;
		}
		else if (unit->getType().isResourceDepot())
		{
			// Only counts complete hatcheries because incomplete hatcheries are checked above.
			// Also counts lairs and hives whether complete or not, of course.
			existingSupply += 2;
		}
		else
		{
			supplyUsed += unit->getType().supplyRequired();
		}
	}

	_existingSupply = std::min(existingSupply, absoluteMaxSupply);
	_pendingSupply = pendingSupply;
	_supplyUsed = supplyUsed;

	// Note: _existingSupply is less than _self->supplyTotal() when an overlord
	// has just died. In other words, it recognizes the lost overlord sooner,
	// which is better for planning.

	//if (_self->supplyUsed() != _supplyUsed)
	//{
	//	BWAPI::Broodwar->printf("official supply used /= measured supply used %d /= %d", _self->supplyUsed(), supplyUsed);
	//}
}

// Called once per frame, possibly more.
// Includes screen drawing calls.
void StrategyBossZerg::updateGameState()
{
	if (_lastUpdateFrame == BWAPI::Broodwar->getFrameCount())
	{
		// No need to update more than once per frame.
		return;
	}
	_lastUpdateFrame = BWAPI::Broodwar->getFrameCount();

	if (_emergencyGroundDefense && _lastUpdateFrame >= _emergencyStartFrame + (5 * 24))
	{
		// Danger has been past for long enough. Declare the end of the emergency.
		_emergencyGroundDefense = false;
	}

	minerals = std::max(0, _self->minerals() - BuildingManager::Instance().getReservedMinerals());
	gas = std::max(0, _self->gas() - BuildingManager::Instance().getReservedGas());

	auto & state = StateManager::Instance();
	// Unit stuff, including uncompleted units.
	nLairs = state.lair_count;
	nHives = state.hive_count;
	nHatches = state.hatchery_count + nLairs + nHives;
	nCompletedHatches = state.hatchery_completed + nLairs + nHives;
	nSpores = state.spore_colony_count;

	// nGas = number of geysers ready to mine (extractor must be complete)
	// nFreeGas = number of geysers free to be taken (no extractor, even uncompleted)
	InformationManager::Instance().getMyGasCounts(nGas, nFreeGas);

	nDrones = state.drone_count;
	nMineralDrones = WorkerManager::Instance().getNumMineralWorkers();
	nGasDrones = WorkerManager::Instance().getNumGasWorkers();
	nLarvas = state.larva_count;

	nLings = state.zergling_count;
	nHydras = state.hydralisk_count;
	nMutas = state.mutalisk_count;
	nGuardians = state.guardian_count;
	nDevourers = state.devourer_count;

	// Tech stuff. It has to be completed for the tech to be available.
	nEvo = state.evolution_chamber_completed;
	hasPool = state.spawning_pool_completed > 0;
	hasDen = state.hydralisk_den_completed > 0;
	hasSpire = state.spire_completed + state.greater_spire_count > 0;
	hasGreaterSpire = state.greater_spire_completed > 0;
	// We have lurkers if we have lurker aspect and we have a den to make the hydras.
	hasLurkers = hasDen && _self->hasResearched(BWAPI::TechTypes::Lurker_Aspect);
	hasQueensNest = state.queens_nest_completed > 0;
	hasUltra = state.ultralisk_cavern_completed > 0;
	// Enough upgrades that it is worth making ultras: Speed done, armor underway.
	hasUltraUps = _self->getUpgradeLevel(BWAPI::UpgradeTypes::Anabolic_Synthesis) != 0 &&
		(_self->getUpgradeLevel(BWAPI::UpgradeTypes::Chitinous_Plating) != 0 ||
		_self->isUpgrading(BWAPI::UpgradeTypes::Chitinous_Plating));

	// hasLair means "can research stuff in the lair", like overlord speed.
	// hasLairTech means "can do stuff that needs lair", like research lurker aspect.
	// NOTE The two are different in game, but even more different in the bot because
	//      of a BWAPI 4.1.2 bug: You can't do lair research in a hive.
	//      This code reflects the bug so we can work around it as much as possible.
	hasHiveTech = state.hive_completed > 0;
	hasLair = state.lair_completed > 0;
	hasLairTech = hasLair || nHives > 0;
	
	outOfBook = ProductionManager::Instance().isOutOfBook();
	nBases = InformationManager::Instance().getNumBases(_self);
	nFreeBases = InformationManager::Instance().getNumFreeLandBases();
	nMineralPatches = InformationManager::Instance().getMyNumMineralPatches();
	maxDrones = WorkerManager::Instance().getMaxWorkers();

	updateSupply();

	drawStrategyBossInformation();
}

// How many of our eggs will hatch into the given unit type?
// This does not adjust for zerglings or scourge, which are 2 to an egg.
int StrategyBossZerg::numInEgg(BWAPI::UnitType type) const
{
	int count = 0;

	for (const auto unit : _self->getUnits())
	{
		if (unit->getType() == BWAPI::UnitTypes::Zerg_Egg && unit->getBuildType() == type)
		{
			++count;
		}
	}

	return count;
}

// Return true if the building is in the building queue with any status.
bool StrategyBossZerg::isBeingBuilt(const BWAPI::UnitType unitType) const
{
	UAB_ASSERT(unitType.isBuilding(), "not a building");
	return BuildingManager::Instance().isBeingBuilt(unitType);
}

// Severe emergency: We are out of drones and/or hatcheries.
// Cancel items to release their resources.
// TODO pay attention to priority: the least essential first
// TODO cancel research
void StrategyBossZerg::cancelStuff(int mineralsNeeded)
{
	int mineralsSoFar = _self->minerals();

	for (BWAPI::Unit u : _self->getUnits())
	{
		if (mineralsSoFar >= mineralsNeeded)
		{
			return;
		}
		if (u->getType() == BWAPI::UnitTypes::Zerg_Egg && u->getBuildType() == BWAPI::UnitTypes::Zerg_Overlord)
		{
			if (_self->supplyTotal() - _supplyUsed >= 6)  // enough to add 3 drones
			{
				mineralsSoFar += 100;
				u->cancelMorph();
			}
		}
		else if (u->getType() == BWAPI::UnitTypes::Zerg_Egg && u->getBuildType() != BWAPI::UnitTypes::Zerg_Drone ||
			u->getType() == BWAPI::UnitTypes::Zerg_Lair && !u->isCompleted() ||
			u->getType() == BWAPI::UnitTypes::Zerg_Creep_Colony && !u->isCompleted() ||
			u->getType() == BWAPI::UnitTypes::Zerg_Evolution_Chamber && !u->isCompleted() ||
			u->getType() == BWAPI::UnitTypes::Zerg_Hydralisk_Den && !u->isCompleted() ||
			u->getType() == BWAPI::UnitTypes::Zerg_Queens_Nest && !u->isCompleted() ||
			u->getType() == BWAPI::UnitTypes::Zerg_Hatchery && !u->isCompleted() && nHatches > 1)
		{
			mineralsSoFar += u->getType().mineralPrice();
			u->cancelMorph();
		}
	}
}

// The next item in the queue is useless and can be dropped.
// Top goal: Do not freeze the production queue by asking the impossible.
// But also try to reduce wasted production.
// NOTE Useless stuff is not always removed before it is built.
//      The order of events is: this check -> queue filling -> production.
bool StrategyBossZerg::nextInQueueIsUseless(BuildOrderQueue & queue) const
{
	if (queue.isEmpty())
	{
		return false;
	}

	auto & state = StateManager::Instance();
	const MacroAct act = queue.getHighestPriorityItem().macroAct;

	// It costs gas that we don't have and won't get.
	if (nGas == 0 && act.gasPrice() > gas &&
		!isBeingBuilt(BWAPI::UnitTypes::Zerg_Extractor) &&
		state.extractor_count == 0)
	{
		return true;
	}

	if (act.isUpgrade())
	{
		const BWAPI::UpgradeType upInQueue = act.getUpgradeType();

		// Already have it or already getting it (due to a race condition).
		if (_self->getUpgradeLevel(upInQueue) == (upInQueue).maxRepeats() || _self->isUpgrading(upInQueue))
		{
			return true;
		}

		// Lost the building for it in the meantime.
		if (upInQueue == BWAPI::UpgradeTypes::Anabolic_Synthesis || upInQueue == BWAPI::UpgradeTypes::Chitinous_Plating)
		{
			return !hasUltra;
		}

		if (upInQueue == BWAPI::UpgradeTypes::Pneumatized_Carapace)
		{
			return !hasLair;
		}

		if (upInQueue == BWAPI::UpgradeTypes::Muscular_Augments || upInQueue == BWAPI::UpgradeTypes::Grooved_Spines)
		{
			return !hasDen &&
				state.hydralisk_den_count == 0 && state.hydralisk_den_being_built == 0;
		}

		if (upInQueue == BWAPI::UpgradeTypes::Metabolic_Boost)
		{
			return !hasPool && state.spawning_pool_count == 0;
		}

		if (upInQueue == BWAPI::UpgradeTypes::Adrenal_Glands)
		{
			return !hasPool || !hasHiveTech;
		}

		// Coordinate these two with the single/double upgrading plan.
		if (upInQueue == BWAPI::UpgradeTypes::Zerg_Carapace)
		{
			return nEvo == 0;
		}
		if (upInQueue == BWAPI::UpgradeTypes::Zerg_Melee_Attacks)
		{
			// We want either 2 evos, or 1 evo and carapace is fully upgraded already.
			return !(nEvo >= 2 ||
				nEvo == 1 && _self->getUpgradeLevel(BWAPI::UpgradeTypes::Zerg_Carapace) == 3);
		}
		
		return false;
	}

	if (act.isTech())
	{
		const BWAPI::TechType techInQueue = act.getTechType();

		if (techInQueue == BWAPI::TechTypes::Lurker_Aspect)
		{
			return !hasLair && nLairs == 0 ||
				!hasDen && state.hydralisk_den_count == 0 && state.hydralisk_den_being_built == 0 ||
				_self->isResearching(BWAPI::TechTypes::Lurker_Aspect) ||
				_self->hasResearched(BWAPI::TechTypes::Lurker_Aspect);
		}

		return false;
	}
	
	// After that, we only care about units.
	if (!act.isUnit())
	{
		return false;
	}

	const BWAPI::UnitType nextInQueue = act.getUnitType();

	if (nextInQueue == BWAPI::UnitTypes::Zerg_Overlord)
	{
		// Opening book sometimes deliberately includes extra overlords.
		if (!outOfBook)
		{
			return false;
		}

		// We may have extra overlords scheduled if, for example, we just lost a lot of units.
		// This is coordinated with makeOverlords() but skips less important steps.
		int totalSupply = _existingSupply + _pendingSupply;
		int supplyExcess = totalSupply - _supplyUsed;
		return totalSupply >= absoluteMaxSupply ||
			totalSupply > 32 && supplyExcess >= totalSupply / 8 + 16;
	}
	if (nextInQueue == BWAPI::UnitTypes::Zerg_Drone)
	{
		// We are planning more than the maximum reasonable number of drones.
		// nDrones can go slightly over maxDrones when queue filling adds drones.
		// It can also go over when maxDrones decreases (bases lost, minerals mined out).
		return nDrones >= maxDrones;
	}
	if (nextInQueue == BWAPI::UnitTypes::Zerg_Zergling)
	{
		// We lost the tech.
		return !hasPool && state.spawning_pool_count == 0;
	}
	if (nextInQueue == BWAPI::UnitTypes::Zerg_Hydralisk)
	{
		// We lost the tech.
		return !hasDen && state.hydralisk_den_count == 0;
	}
	if (nextInQueue == BWAPI::UnitTypes::Zerg_Lurker)
	{
		// No hydra to morph, or we expected to have the tech and don't.
		return nHydras == 0 ||
			!_self->hasResearched(BWAPI::TechTypes::Lurker_Aspect) && !_self->isResearching(BWAPI::TechTypes::Lurker_Aspect);
	}
	if (nextInQueue == BWAPI::UnitTypes::Zerg_Mutalisk || nextInQueue == BWAPI::UnitTypes::Zerg_Scourge)
	{
		// We lost the tech.
		return !hasSpire &&
			state.spire_count == 0 &&
			state.greater_spire_count == 0;
	}
	if (nextInQueue == BWAPI::UnitTypes::Zerg_Ultralisk)
	{
		// We lost the tech.
		return !hasUltra && state.ultralisk_cavern_count == 0;
	}
	if (nextInQueue == BWAPI::UnitTypes::Zerg_Guardian || nextInQueue == BWAPI::UnitTypes::Zerg_Devourer)
	{
		// We lost the tech, or we don't have a mutalisk to morph.
		return nMutas == 0 ||
			!hasGreaterSpire && state.greater_spire_count == 0;
	}

	if (nextInQueue == BWAPI::UnitTypes::Zerg_Hatchery)
	{
		// We're planning a hatchery but no longer have the drones to support it.
		// 3 drones/hatchery is the minimum: It can support ling production.
		// Also, it may still be OK if we have lots of minerals to spend.
		return nDrones < 3 * nHatches &&
			minerals < 50 + 300 * nCompletedHatches &&
			nCompletedHatches > 0;
	}
	if (nextInQueue == BWAPI::UnitTypes::Zerg_Lair)
	{
		return !hasPool && state.spawning_pool_count == 0 ||
			state.hatchery_count == 0;
	}
	if (nextInQueue == BWAPI::UnitTypes::Zerg_Hive)
	{
		return state.queens_nest_count == 0 ||
			state.lair_count == 0 ||
			_self->isUpgrading(BWAPI::UpgradeTypes::Pneumatized_Carapace) ||
			_self->isUpgrading(BWAPI::UpgradeTypes::Ventral_Sacs);
	}
	if (nextInQueue == BWAPI::UnitTypes::Zerg_Sunken_Colony)
	{
		return !hasPool && state.spawning_pool_count == 0 ||
			state.creep_colony_count == 0 && state.creep_colony_being_built == 0;
	}
	if (nextInQueue == BWAPI::UnitTypes::Zerg_Spore_Colony)
	{
		return nEvo == 0 && state.evolution_chamber_count == 0 && state.evolution_chamber_being_built == 0 ||
			state.creep_colony_count == 0 && state.creep_colony_being_built == 0;
	}
	if (nextInQueue == BWAPI::UnitTypes::Zerg_Hydralisk_Den)
	{
		return !hasPool && state.spawning_pool_count == 0;
	}
	if (nextInQueue == BWAPI::UnitTypes::Zerg_Spire)
	{
		return !hasLairTech && state.lair_count == 0;
	}
	if (nextInQueue == BWAPI::UnitTypes::Zerg_Greater_Spire)
	{
		return nHives == 0 ||
			state.spire_count == 0;
	}
	if (nextInQueue == BWAPI::UnitTypes::Zerg_Extractor)
	{
		return nFreeGas == 0 ||												// nowhere to make an extractor
			nDrones < 1 + Config::Macro::WorkersPerRefinery * (nGas + 1);	// not enough drones to mine it
	}

	return false;
}

void StrategyBossZerg::produce(const MacroAct & act)
{
	_latestBuildOrder.add(act);
	if (act.isUnit())
	{
		++_economyTotal;
		if (act.getUnitType() == BWAPI::UnitTypes::Zerg_Drone)
		{
			++_economyDrones;
		}
	}
}

// Make a drone instead of a combat unit with this larva?
bool StrategyBossZerg::needDroneNext() const
{
	return !_emergencyGroundDefense &&
		nDrones < maxDrones &&
		double(_economyDrones) / double(1 + _economyTotal) < _economyRatio;
}

// We think we want the given unit type. What type do we really want?
// 1. If we need a drone next for the economy, return a drone instead.
// 2. If the type is a morphed type and we don't have the precursor,
//    return the precursor type instead.
// Otherwise return the requested type.
BWAPI::UnitType StrategyBossZerg::findUnitType(BWAPI::UnitType type) const
{
	if (needDroneNext())
	{
		return BWAPI::UnitTypes::Zerg_Drone;
	}

	if (type == BWAPI::UnitTypes::Zerg_Lurker && nHydras == 0)
	{
		return BWAPI::UnitTypes::Zerg_Hydralisk;
	}
	if ((type == BWAPI::UnitTypes::Zerg_Guardian || type == BWAPI::UnitTypes::Zerg_Devourer) && nMutas == 0)
	{
		return BWAPI::UnitTypes::Zerg_Mutalisk;
	}

	return type;
}

// We need overlords.
// Do this last so that nothing gets pushed in front of the overlords.
// NOTE: If you change this, coordinate the change with nextInQueueIsUseless(),
// which has a feature to recognize unneeded overlords (e.g. after big army losses).
void StrategyBossZerg::makeOverlords(BuildOrderQueue & queue)
{
	// If an overlord is next up anyway, we have nothing to do.
	// If a command is up next, it takes no supply, also nothing.
	if (!queue.isEmpty())
	{
		MacroAct act = queue.getHighestPriorityItem().macroAct;
		if (act.isCommand() || act.isUnit() && act.getUnitType() == BWAPI::UnitTypes::Zerg_Overlord)
		{
			return;
		}
	}

	int totalSupply = std::min(_existingSupply + _pendingSupply, absoluteMaxSupply);
	if (totalSupply < absoluteMaxSupply)
	{
		int supplyExcess = totalSupply - _supplyUsed;
		BWAPI::UnitType nextInQueue = queue.getNextUnit();

		// Adjust the number to account for the next queue item and pending buildings.
		if (nextInQueue != BWAPI::UnitTypes::None)
		{
			if (nextInQueue.isBuilding())
			{
				if (!UnitUtil::IsMorphedBuildingType(nextInQueue))
				{
					supplyExcess += 2;   // for the drone that will be used
				}
			}
			else
			{
				supplyExcess -= nextInQueue.supplyRequired();
			}
		}
		// The number of drones set to be used up making buildings.
		supplyExcess += 2 * BuildingManager::Instance().buildingsQueued().size();

		// If we're behind, catch up.
		for (; supplyExcess < 0; supplyExcess += 16)
		{
			queue.queueAsHighestPriority(BWAPI::UnitTypes::Zerg_Overlord);
		}
		// If we're only a little ahead, stay ahead depending on the supply.
		// This is a crude calculation. It seems not too far off.
		if (totalSupply > 20 && supplyExcess <= 0)                       // > overlord + 2 hatcheries
		{
			queue.queueAsHighestPriority(BWAPI::UnitTypes::Zerg_Overlord);
		}
		else if (totalSupply > 32 && supplyExcess <= totalSupply / 8 - 2)    // >= 2 overlords + 1 hatchery
		{
			queue.queueAsHighestPriority(BWAPI::UnitTypes::Zerg_Overlord);
		}
	}
}

// If necessary, take an emergency action and return true.
// Otherwise return false.
bool StrategyBossZerg::takeUrgentAction(BuildOrderQueue & queue)
{
	// Find the next thing remaining in the queue, but only if it is a unit.
	const BWAPI::UnitType nextInQueue = queue.getNextUnit();

	// There are no drones.
	// NOTE maxDrones is never zero. We always save one just in case.
	if (nDrones == 0)
	{
		WorkerManager::Instance().setCollectGas(false);
		BuildingManager::Instance().cancelQueuedBuildings();
		if (nHatches == 0)
		{
			// No hatcheries either. Queue drones for a hatchery and mining.
			ProductionManager::Instance().goOutOfBook();
			queue.queueAsLowestPriority(BWAPI::UnitTypes::Zerg_Drone);
			queue.queueAsLowestPriority(BWAPI::UnitTypes::Zerg_Drone);
			queue.queueAsLowestPriority(BWAPI::UnitTypes::Zerg_Hatchery);
			cancelStuff(400);
		}
		else
		{
			if (nextInQueue != BWAPI::UnitTypes::Zerg_Drone && numInEgg(BWAPI::UnitTypes::Zerg_Drone) == 0)
			{
				// Queue one drone to mine minerals.
				ProductionManager::Instance().goOutOfBook();
				queue.queueAsLowestPriority(BWAPI::UnitTypes::Zerg_Drone);
				cancelStuff(50);
			}
			BuildingManager::Instance().cancelBuildingType(BWAPI::UnitTypes::Zerg_Hatchery);
		}
		return true;
	}

	// There are no hatcheries.
	if (nHatches == 0 &&
		nextInQueue != BWAPI::UnitTypes::Zerg_Hatchery &&
		!isBeingBuilt(BWAPI::UnitTypes::Zerg_Hatchery))
	{
		ProductionManager::Instance().goOutOfBook();
		queue.queueAsLowestPriority(BWAPI::UnitTypes::Zerg_Hatchery);
		if (nDrones == 1)
		{
			ScoutManager::Instance().releaseWorkerScout();
			queue.queueAsHighestPriority(BWAPI::UnitTypes::Zerg_Drone);
			cancelStuff(350);
		}
		else {
			cancelStuff(300);
		}
		return true;
	}

	// There are < 3 drones. Make up to 3.
	// Making more than 3 breaks 4 pool openings.
	if (nDrones < 3 &&
		nextInQueue != BWAPI::UnitTypes::Zerg_Drone &&
		nextInQueue != BWAPI::UnitTypes::Zerg_Overlord)
	{
		ScoutManager::Instance().releaseWorkerScout();
		queue.queueAsHighestPriority(BWAPI::UnitTypes::Zerg_Drone);
		if (nDrones < 2)
		{
			queue.queueAsHighestPriority(BWAPI::UnitTypes::Zerg_Drone);
		}
		// Don't cancel other stuff. A drone should be mining, it's not that big an emergency.
		return true;
	}

	// There are no drones on minerals. Turn off gas collection.
	// TODO more efficient test in WorkerMan
	if (_lastUpdateFrame >= 24 &&           // give it time!
		WorkerManager::Instance().isCollectingGas() &&
		nMineralPatches > 0 &&
		WorkerManager::Instance().getNumMineralWorkers() == 0 &&
		WorkerManager::Instance().getNumReturnCargoWorkers() == 0 &&
		WorkerManager::Instance().getNumCombatWorkers() == 0 &&
		WorkerManager::Instance().getNumIdleWorkers() == 0)
	{
		// Leave the queue in place.
		ScoutManager::Instance().releaseWorkerScout();
		WorkerManager::Instance().setCollectGas(false);
		if (nHatches >= 2)
		{
			BuildingManager::Instance().cancelBuildingType(BWAPI::UnitTypes::Zerg_Hatchery);
		}
		return true;
	}

	return false;
}

// React to lesser emergencies.
void StrategyBossZerg::makeUrgentReaction(BuildOrderQueue & queue)
{
	auto & state = StateManager::Instance();
	// Find the next thing remaining in the queue, but only if it is a unit.
	const BWAPI::UnitType nextInQueue = queue.getNextUnit();

	// Enemy has air. Make scourge if possible (but in ZvZ only after we have some mutas).
	if (hasSpire && nGas > 0 &&
		InformationManager::Instance().enemyHasAirTech() &&
		nextInQueue != BWAPI::UnitTypes::Zerg_Scourge &&
		(_enemyRace != BWAPI::Races::Zerg || nMutas >= 5))
	{
		int totalScourge = state.scourge_count +
			2 * numInEgg(BWAPI::UnitTypes::Zerg_Scourge) +
			2 * queue.numInQueue(BWAPI::UnitTypes::Zerg_Scourge);

		// Not too much, and not too much at once. They cost a lot of gas.
		int nScourgeNeeded = std::min(18, InformationManager::Instance().nScourgeNeeded());
		int nToMake = 0;
		if (nScourgeNeeded > totalScourge && nLarvas > 0)
		{
			int nPairs = std::min(1 + gas / 75, (nScourgeNeeded - totalScourge + 1) / 2);
			int limit = 3;          // how many pairs at a time, max?
			if (nLarvas > 6 && gas > 6 * 75)
			{
				// Allow more if we have plenty of resources.
				limit = 6;
			}
			nToMake = std::min(nPairs, limit);
		}
		for (int i = 0; i < nToMake; ++i)
		{
			queue.queueAsHighestPriority(BWAPI::UnitTypes::Zerg_Scourge);
		}
		// And keep going.
	}

	int queueMinerals, queueGas;
	queue.totalCosts(queueMinerals, queueGas);

	// We have too much gas. Turn off gas collection.
	// Opening book sometimes collects extra gas on purpose.
	// This ties in via ELSE with the next check!
	if (outOfBook &&
		WorkerManager::Instance().isCollectingGas() &&
		gas >= queueGas &&
		((minerals < 100 && gas > 300) || (minerals >= 100 && gas > 3 * minerals)))
	{
		WorkerManager::Instance().setCollectGas(false);
		// And keep going.
	}

	// We're in book and should have enough gas but it's off. Something went wrong.
	// Note ELSE!
	else if (!outOfBook && queue.getNextGasCost(1) > gas &&
		!WorkerManager::Instance().isCollectingGas())
	{
		if (nGas == 0 || nDrones < 9)
		{
			// Emergency. Give up and clear the queue.
			ProductionManager::Instance().goOutOfBook();
			return;
		}
		// Not such an emergency. Turn gas on and keep going.
		WorkerManager::Instance().setCollectGas(true);
	}

	// Note ELSE!
	else if (outOfBook && queue.getNextGasCost(1) > gas && nGas > 0 && nGasDrones == 0 &&
		WorkerManager::Instance().isCollectingGas())
	{
		// Deadlock. Can't get gas. Give up and clear the queue.
		ProductionManager::Instance().goOutOfBook();
		return;
	}

	// Gas is turned off, and upcoming items cost more gas than we have. Get gas.
	// NOTE isCollectingGas() can return false when gas is in the process of being turned off,
	// and some will still be collected.
	// Note ELSE!
	else if (outOfBook && queue.getNextGasCost(4) > gas && !WorkerManager::Instance().isCollectingGas())
	{
		if (nGas > 0 && nDrones > 3 * nGas)
		{
			// Leave it to the regular queue refill to add more extractors.
			WorkerManager::Instance().setCollectGas(true);
		}
		else
		{
			// Well, we can't collect gas.
			// Make enough drones to get an extractor.
			ScoutManager::Instance().releaseWorkerScout();   // don't throw off the drone count
			if (nGas == 0 && nDrones >= 5 && nFreeGas > 0 &&
				nextInQueue != BWAPI::UnitTypes::Zerg_Extractor &&
				!isBeingBuilt(BWAPI::UnitTypes::Zerg_Extractor))
			{
				queue.queueAsHighestPriority(BWAPI::UnitTypes::Zerg_Extractor);
			}
			else if (nGas == 0 && nDrones >= 4 && isBeingBuilt(BWAPI::UnitTypes::Zerg_Extractor))
			{
				// We have an unfinished extractor. Wait for it to finish.
				// Need 4 drones so that 1 can keep mining minerals (or the rules will loop).
				WorkerManager::Instance().setCollectGas(true);
			}
			else if (nextInQueue != BWAPI::UnitTypes::Zerg_Drone && nFreeGas > 0)
			{
				queue.queueAsHighestPriority(BWAPI::UnitTypes::Zerg_Drone);
			}
		}
		// And keep going.
	}

	// We're in book and want to make zerglings next, but we also want extra drones.
	// Change the zerglings to a drone, since they have the same cost.
	// When we want extra drones, _economyDrones is decreased, so we recognize that by negative values.
	// Don't make all the extra drones in book, save a couple for later, because it could mess stuff up.
	if (!outOfBook && _economyDrones < -2 && nextInQueue == BWAPI::UnitTypes::Zerg_Zergling)
	{
		queue.removeHighestPriorityItem();
		queue.queueAsHighestPriority(BWAPI::UnitTypes::Zerg_Drone);
		++_economyDrones;
		// And keep going.
	}

	// We need a macro hatchery.
	// Division of labor: Macro hatcheries are here, expansions are regular production.
	// However, some macro hatcheries may be placed at expansions (it helps assert map control).
	// Macro hatcheries are automatic only out of book. Book openings must take care of themselves.
	const int hatcheriesUnderConstruction =
		BuildingManager::Instance().getNumUnstarted(BWAPI::UnitTypes::Zerg_Hatchery) +
		state.hatchery_count -
		state.hatchery_completed;
	if (outOfBook && minerals >= 300 && nLarvas == 0 && nHatches < 15 && nDrones > 9 &&
		nextInQueue != BWAPI::UnitTypes::Zerg_Hatchery &&
		nextInQueue != BWAPI::UnitTypes::Zerg_Overlord &&
		hatcheriesUnderConstruction <= 1 &&
		!state.being_rushed && !state.natural_dangerous)
	{
		MacroLocation loc = MacroLocation::Macro;
		if (nHatches % 2 != 0 && nFreeBases > 2)
		{
			// Expand with some macro hatcheries unless it's late game.
			loc = MacroLocation::MinOnly;
		}
		queue.queueAsHighestPriority(MacroAct(BWAPI::UnitTypes::Zerg_Hatchery, loc));
		// And keep going.
	}

	// If the enemy has cloaked stuff, consider overlord speed.
	if (InformationManager::Instance().enemyHasCloakTech())
	{
		if (hasLair &&
			minerals >= 150 && gas >= 150 &&
			_self->getUpgradeLevel(BWAPI::UpgradeTypes::Pneumatized_Carapace) == 0 &&
			!_self->isUpgrading(BWAPI::UpgradeTypes::Pneumatized_Carapace) &&
			!queue.anyInQueue(BWAPI::UpgradeTypes::Pneumatized_Carapace))
		{
			queue.queueAsHighestPriority(MacroAct(BWAPI::UpgradeTypes::Pneumatized_Carapace));
		}
		// And keep going.
	}

	// If the enemy has overlord hunters such as corsairs, prepare appropriately.
	if (InformationManager::Instance().enemyHasOverlordHunters())
	{
		if (nEvo > 0 && nDrones >= 9 && nSpores == 0 &&
			!queue.anyInQueue(BWAPI::UnitTypes::Zerg_Spore_Colony) &&
			!isBeingBuilt(BWAPI::UnitTypes::Zerg_Spore_Colony) &&
			!StateManager::Instance().flyer_visit_position.empty())	// for anti cannon bot, don't spore
		{
			queue.queueAsHighestPriority(BWAPI::UnitTypes::Zerg_Spore_Colony);
			queue.queueAsHighestPriority(BWAPI::UnitTypes::Zerg_Creep_Colony);
		}
		else if (nEvo == 0 && nDrones >= 9 && outOfBook && hasPool &&
			!queue.anyInQueue(BWAPI::UnitTypes::Zerg_Evolution_Chamber) &&
			state.evolution_chamber_total == 0)
		{
			queue.queueAsHighestPriority(BWAPI::UnitTypes::Zerg_Evolution_Chamber);
		}
		else if (!hasSpire && hasLairTech && outOfBook &&
			minerals >= 200 && gas >= 150 && nGas > 0 && nDrones > 9 &&
			!queue.anyInQueue(BWAPI::UnitTypes::Zerg_Spire) &&
			state.spire_count == 0 &&
			!isBeingBuilt(BWAPI::UnitTypes::Zerg_Spire))
		{
			queue.queueAsHighestPriority(BWAPI::UnitTypes::Zerg_Spire);
		}
		else if (hasLair &&
			minerals >= 150 && gas >= 150 &&
			_enemyRace != BWAPI::Races::Zerg &&
			_self->getUpgradeLevel(BWAPI::UpgradeTypes::Pneumatized_Carapace) == 0 &&
			!_self->isUpgrading(BWAPI::UpgradeTypes::Pneumatized_Carapace) &&
			!queue.anyInQueue(BWAPI::UpgradeTypes::Pneumatized_Carapace))
		{
			queue.queueAsHighestPriority(BWAPI::UpgradeTypes::Pneumatized_Carapace);
		}
	}
}

// We always want 9 drones and a spawning pool. Return whether any action was taken.
// This is part of freshProductionPlan().
bool StrategyBossZerg::rebuildCriticalLosses()
{
	auto & state = StateManager::Instance();
	// 1. Add up to 9 drones if we're below.
	for (int i = nDrones; i < std::min(9, maxDrones); ++i)
	{
		produce(BWAPI::UnitTypes::Zerg_Drone);
		return true;
	}

	// 2. If there is no spawning pool, we always need that.
	if (!hasPool &&
		!isBeingBuilt(BWAPI::UnitTypes::Zerg_Spawning_Pool) &&
		state.spawning_pool_count == 0)
	{
		produce(BWAPI::UnitTypes::Zerg_Spawning_Pool);
		// If we're low on drones, replace the drone.
		if (nDrones <= 9 && nDrones <= maxDrones)
		{
			produce(BWAPI::UnitTypes::Zerg_Drone);
		}
		return true;
	}
	return false;
}

// Check for possible ground attacks that we are may have trouble handling.
// React when it seems necessary, with sunkens, zerglings, or by pulling drones.
// If the opening book seems unready for the situation, break out of book.
// If a deadly attack seems impending, declare an emergency so that the
// regular production plan will concentrate on combat units.
void StrategyBossZerg::checkGroundDefenses(BuildOrderQueue & queue)
{
	auto & state = StateManager::Instance();
	// for anti cannon, don't apply sunken
	if (!StateManager::Instance().flyer_visit_position.empty())
	{
		return;
	}

	// 1. Figure out where our front defense line is.
	MacroLocation front = MacroLocation::Anywhere;
	BWAPI::Unit ourHatchery = nullptr;

	if (InformationManager::Instance().getMyNaturalLocation())
	{
		ourHatchery =
			InformationManager::Instance().getBaseDepot(InformationManager::Instance().getMyNaturalLocation());
		if (UnitUtil::IsValidUnit(ourHatchery)
			|| (ourHatchery && StateManager::Instance().being_rushed))
		{
			front = MacroLocation::Natural;
		}
	}
	if (front == MacroLocation::Anywhere)
	{
		ourHatchery =
			InformationManager::Instance().getBaseDepot(InformationManager::Instance().getMyMainBaseLocation());
		if (UnitUtil::IsValidUnit(ourHatchery))
		{
			front = MacroLocation::Macro;
		}
	}
	if (!ourHatchery || front == MacroLocation::Anywhere)
	{
		// We don't have a place to put static defense. It's that bad.
		return;
	}

	// check rush
	if (state.being_rushed || state.keep_build_sunken)
	{
		// terran
		// max 4 sunken
		if (_enemy->getRace() == BWAPI::Races::Terran)
		{
			int sunkenLimit = std::min(state.enemy_marine_count / 2.5, 4.0);
			if (state.sunken_colony_total + state.creep_colony_total < sunkenLimit && state.creep_colony_waiting == 0)
			{
				queue.queueAsHighestPriority(MacroAct(BWAPI::UnitTypes::Zerg_Creep_Colony, front));
			}
		}
		// protoss
		// max 6 sunken
		if (_enemy->getRace() == BWAPI::Races::Protoss)
		{
			int sunkenLimit = std::min(state.enemy_zealot_count + 1, 7);
			if (state.sunken_colony_total + state.creep_colony_total < sunkenLimit && state.creep_colony_waiting == 0)
			{
				queue.queueAsHighestPriority(MacroAct(BWAPI::UnitTypes::Zerg_Creep_Colony, front));
			}
		}
		// zerg
		// no limit
		if (_enemy->getRace() == BWAPI::Races::Zerg && state.keep_build_sunken)
		{
			int sunkenLimit = state.enemy_zergling_count / 5;
			if (state.sunken_colony_total + state.creep_colony_total < sunkenLimit && state.creep_colony_waiting == 0)
			{
				queue.queueAsHighestPriority(MacroAct(BWAPI::UnitTypes::Zerg_Creep_Colony, front));
			}
		}
		if (queue.numInQueue(BWAPI::UnitTypes::Zerg_Sunken_Colony) == 0 && state.creep_colony_count > 0)
		{
			queue.queueAsHighestPriority(MacroAct(BWAPI::UnitTypes::Zerg_Sunken_Colony, front));
			queue.queueAsHighestPriority(BWAPI::UnitTypes::Zerg_Drone);
		}
		return;
	}

	// 2. Count enemy ground power.
	int enemyPower = 0;
	int enemyPowerNearby = 0;
	int enemyMarines = 0;
	int enemyAcademyUnits = 0;    // count firebats and medics
	int enemyVultures = 0;
	int enemyBarracks = 0;
	bool enemyHasDrop = false;
	for (const auto & kv : InformationManager::Instance().getUnitData(_enemy).getUnits())
	{
		const UnitInfo & ui(kv.second);

		if (!ui.type.isBuilding() && !ui.type.isWorker() &&
			ui.type.groundWeapon() != BWAPI::WeaponTypes::None &&
			!ui.type.isFlyer())
		{
			enemyPower += ui.type.supplyRequired();
			if (ui.updateFrame >= _lastUpdateFrame - 30 * 24 &&          // seen in the last 30 seconds
				ui.lastPosition.isValid() &&
				ourHatchery->getDistance(ui.lastPosition) < 1500)		 // not far from our front base
			{
				enemyPowerNearby += ui.type.supplyRequired();
			}
			if (ui.type == BWAPI::UnitTypes::Terran_Marine)
			{
				++enemyMarines;
			}
			if (ui.type == BWAPI::UnitTypes::Terran_Firebat || ui.type == BWAPI::UnitTypes::Terran_Medic)
			{
				++enemyAcademyUnits;
			}
			else if (ui.type == BWAPI::UnitTypes::Terran_Vulture)
			{
				++enemyVultures;
			}
			else if (ui.type == BWAPI::UnitTypes::Terran_Dropship || ui.type == BWAPI::UnitTypes::Protoss_Shuttle)
			{
				enemyHasDrop = true;
			}
		}
		else if (ui.type == BWAPI::UnitTypes::Terran_Barracks)
		{
			++enemyBarracks;
		}
	}

	// 3. Count our anti-ground power, including air units.
	int ourPower = 0;
	int ourSunkens = 0;
	for (const BWAPI::Unit u : _self->getUnits())
	{
		if (!u->getType().isBuilding() && !u->getType().isWorker() &&
			u->getType().groundWeapon() != BWAPI::WeaponTypes::None)
		{
			ourPower += u->getType().supplyRequired();
		}
		else if (u->getType() == BWAPI::UnitTypes::Zerg_Sunken_Colony ||
			u->getType() == BWAPI::UnitTypes::Zerg_Creep_Colony)          // blindly assume it will be a sunken
		{
			if (ourHatchery->getDistance(u) < 600)
			{
				++ourSunkens;
			}
		}
	}

	int queuedSunkens =			// without checking location
		queue.numInQueue(BWAPI::UnitTypes::Zerg_Sunken_Colony) +
		BuildingManager::Instance().getNumUnstarted(BWAPI::UnitTypes::Zerg_Sunken_Colony);
	int totalSunkens = ourSunkens + queuedSunkens;
	ourPower += 5 * totalSunkens;

	// 4. React if a terran is attacking early and we're not ready.
	// This improves our chances to survive a terran BBS or other infantry attack.
	bool makeSunken = false;
	int enemyInfantry = enemyMarines + enemyAcademyUnits;
	// Fear level is an estimate of "un-countered enemy infantry units".
	int fearLevel = enemyInfantry - (nLings + 3 * totalSunkens);       // minor fear
	if (enemyAcademyUnits > 0 && enemyInfantry >= 8)
	{
		fearLevel = enemyInfantry - (nLings / 4 + 3 * totalSunkens);   // major fear
	}
	else if (enemyAcademyUnits > 0 || enemyMarines > 4)
	{
		fearLevel = enemyInfantry - (nLings / 2 + 3 * totalSunkens);   // moderate fear
	}
	BWAPI::UnitType nextUnit = queue.getNextUnit();
	if (_lastUpdateFrame < 4000 && (enemyMarines >= 2 || enemyBarracks >= 2) && totalSunkens == 0 && nLings < 6)
	{
		// BBS can create 2 marines before frame 3400, but we probably won't see them right away.
		makeSunken = true;
	}
	else if (!outOfBook &&
		enemyPowerNearby > 2 * nLings &&
		fearLevel > 0 &&
		enemyMarines + enemyAcademyUnits > 0 &&
		nextUnit != BWAPI::UnitTypes::Zerg_Spawning_Pool &&
		nextUnit != BWAPI::UnitTypes::Zerg_Zergling &&
		nextUnit != BWAPI::UnitTypes::Zerg_Creep_Colony &&
		nextUnit != BWAPI::UnitTypes::Zerg_Sunken_Colony)
	{
		// Also make zerglings and/or sunkens.
		if (fearLevel <= 4 && nLarvas > 0)
		{
			queue.queueAsHighestPriority(BWAPI::UnitTypes::Zerg_Zergling);
		}
		else
		{
			makeSunken = true;
		}
	}

	// 5. Build sunkens.
	if (hasPool && nDrones >= 9)
	{
		// The nHatches term adjusts for what we may be able to build before they arrive.
		const bool makeOne =
			makeSunken && totalSunkens < 4 ||
			enemyPower > ourPower + 6 * nHatches && !_emergencyGroundDefense && totalSunkens < 4 ||
			(enemyVultures > 0 || enemyHasDrop) && totalSunkens == 0;
		
		const bool inProgress =
			BuildingManager::Instance().getNumUnstarted(BWAPI::UnitTypes::Zerg_Sunken_Colony) > 0 ||
			BuildingManager::Instance().getNumUnstarted(BWAPI::UnitTypes::Zerg_Creep_Colony) > 0 ||
			state.creep_colony_count > 0;

		if (makeOne && !inProgress)
		{
			queue.queueAsHighestPriority(MacroAct(BWAPI::UnitTypes::Zerg_Sunken_Colony, front));
			queue.queueAsHighestPriority(BWAPI::UnitTypes::Zerg_Drone);
			queue.queueAsHighestPriority(MacroAct(BWAPI::UnitTypes::Zerg_Creep_Colony, front));
		}
	}

	// 6. Declare an emergency.
	// The nHatches term adjusts for what we may be able to build before the enemy arrives.
	if (enemyPowerNearby > ourPower + nHatches)
	{
		_emergencyGroundDefense = true;
		_emergencyStartFrame = _lastUpdateFrame;
	}
}

// If the enemy expanded or made static defense, we can spawn extra drones.
// Exception: Static defense near our base is a proxy.
void StrategyBossZerg::analyzeExtraDrones()
{
	// 50 + 1/8 overlord = 62.5 minerals per drone.
	// Let's be a little more conservative than that, since we may scout it late.
	// In ZvZ, deliberately undercompensate, because making too many drones is death.
	const double droneCost = _enemyRace == BWAPI::Races::Zerg ? 100 : 75;

	double extraDrones = 0;

	// Enemy bases beyond the main.
	int nBases = 0;
	for (BWTA::BaseLocation * base : BWTA::getBaseLocations())
	{
		if (InformationManager::Instance().getBaseOwner(base) == _enemy)
		{
			++nBases;
		}
	}
	if (nBases > 1)
	{
		extraDrones += (nBases - 1) * 300.0 / droneCost;
	}

	// Enemy static defenses.
	// We don't care whether they are completed or not.
	for (const auto & kv : InformationManager::Instance().getUnitData(_enemy).getUnits())
	{
		const UnitInfo & ui(kv.second);

		// A proxy near the main base is not static defense, it is offense.
		// The main base is guaranteed non-null.
		if (ui.type.isBuilding() &&
			ui.lastPosition.isValid() &&
			InformationManager::Instance().getMyMainBaseLocation()->getPosition().getDistance(ui.lastPosition) > 800)
		{
			if (ui.type == BWAPI::UnitTypes::Zerg_Creep_Colony)
			{
				extraDrones += 1.0 + 75.0 / droneCost;
			}
			else if (ui.type == BWAPI::UnitTypes::Zerg_Sunken_Colony || ui.type == BWAPI::UnitTypes::Zerg_Spore_Colony)
			{
				extraDrones += 1.0 + 125.0 / droneCost;
			}
			else if (ui.type == BWAPI::UnitTypes::Protoss_Photon_Cannon ||
				ui.type == BWAPI::UnitTypes::Protoss_Shield_Battery ||
				ui.type == BWAPI::UnitTypes::Terran_Missile_Turret ||
				ui.type == BWAPI::UnitTypes::Terran_Bunker)
			{
				extraDrones += ui.type.mineralPrice() / droneCost;
			}
		}
	}

	// Enemy bases/static defense may have been added or destroyed, or both.
	// We don't keep track of what is destroyed, and react only to what is added since last check.
	int nExtraDrones = int(trunc(extraDrones));
	if (nExtraDrones > _extraDronesWanted)
	{
		_economyDrones -= nExtraDrones - _extraDronesWanted;   // pretend we made fewer drones
	}
	_extraDronesWanted = nExtraDrones;
}

bool StrategyBossZerg::lairTechUnit(TechUnit techUnit) const
{
	return
		techUnit == TechUnit::Mutalisks ||
		techUnit == TechUnit::Lurkers;
}

bool StrategyBossZerg::airTechUnit(TechUnit techUnit) const
{
	return
		techUnit == TechUnit::Mutalisks ||
		techUnit == TechUnit::Guardians ||
		techUnit == TechUnit::Devourers;
}

bool StrategyBossZerg::hiveTechUnit(TechUnit techUnit) const
{
	return
		techUnit == TechUnit::Ultralisks ||
		techUnit == TechUnit::Guardians ||
		techUnit == TechUnit::Devourers;
}

// We want to build a hydra den for lurkers. Is it time yet?
// We want to time is so that when the den finishes, lurker aspect research can start right away.
bool StrategyBossZerg::lurkerDenTiming() const
{
	if (hasLairTech)
	{
		// Lair is already finished. Den can start any time.
		return true;
	}

	for (const auto unit : _self->getUnits())
	{
		// Allow extra frames for the den building drone to move and start the building.
		if (unit->getType() == BWAPI::UnitTypes::Zerg_Lair &&
			unit->getRemainingBuildTime() <= 100 + (BWAPI::UnitTypes::Zerg_Hydralisk_Den).buildTime())
		{
			return true;
		}
	}

	return false;
}

int StrategyBossZerg::techTier(TechUnit techUnit) const
{
	if (techUnit == TechUnit::Zerglings || techUnit == TechUnit::Hydralisks)
	{
		return 1;
	}

	if (techUnit == TechUnit::Lurkers || techUnit == TechUnit::Mutalisks)
	{
		// Lair tech.
		return 2;
	}

	if (techUnit == TechUnit::Ultralisks || techUnit == TechUnit::Guardians || techUnit == TechUnit::Devourers)
	{
		// Hive tech.
		return 3;
	}

	return 0;
}

void StrategyBossZerg::resetTechScores()
{
	for (int i = 0; i < int(TechUnit::Size); ++i)
	{
		techScores[i] = 0;
	}
}

// A tech unit is available for selection in the unit mix if we have the tech for it.
// That's what this routine figures out.
// It is available for selection as a tech target if we do NOT have the tech for it.
void StrategyBossZerg::setAvailableTechUnits(std::array<bool, int(TechUnit::Size)> & available)
{
	available[int(TechUnit::None)] = false;       // avoid doing nothing if at all possible

	// Tier 1.
	available[int(TechUnit::Zerglings)] = hasPool;
	available[int(TechUnit::Hydralisks)] = hasDen && nGas > 0;

	// Lair tech.
	available[int(TechUnit::Lurkers)] = hasLurkers && nGas > 0;
	available[int(TechUnit::Mutalisks)] = hasSpire && nGas > 0;

	// Hive tech.
	available[int(TechUnit::Ultralisks)] = hasUltra && hasUltraUps && nGas >= 2;
	available[int(TechUnit::Guardians)] = hasGreaterSpire && nGas >= 2;
	available[int(TechUnit::Devourers)] = hasGreaterSpire && nGas >= 2;
}

// Decide what units counter the protoss unit mix.
void StrategyBossZerg::vProtossTechScores()
{
	// Bias.
	techScores[int(TechUnit::Hydralisks)] = 11;
	techScores[int(TechUnit::Ultralisks)] = 25;   // default hive tech
	techScores[int(TechUnit::Guardians)]  =  6;   // other hive tech
	techScores[int(TechUnit::Devourers)]  =  1;   // other hive tech

	// Hysteresis.
	if (_techTarget != TechUnit::None)
	{
		techScores[int(_techTarget)] += 11;
	}

	for (const auto & kv : InformationManager::Instance().getUnitData(_enemy).getUnits())
	{
		const UnitInfo & ui(kv.second);

		if (!ui.type.isWorker() && !ui.type.isBuilding() && ui.type != BWAPI::UnitTypes::Protoss_Interceptor)
		{
			// The base score: Enemy mobile combat units.
			techScores[int(TechUnit::Hydralisks)] += ui.type.supplyRequired();   // hydras vs. all
			if (StateManager::Instance().being_rushed)
			{
				techScores[int(TechUnit::Lurkers)] += ui.type.supplyProvided();
			}
			if (ui.type.isFlyer())
			{
				// Enemy air units.
				techScores[int(TechUnit::Devourers)] += ui.type.supplyRequired();
				if (BWAPI::UnitTypes::Protoss_Corsair)
				{
					// Specialized splash anti-air unit.
					techScores[int(TechUnit::Mutalisks)] -= ui.type.supplyRequired();
					techScores[int(TechUnit::Devourers)] += 4;
				}
			}
			else
			{
				// Enemy ground units.
				techScores[int(TechUnit::Zerglings)] += ui.type.supplyRequired();
				techScores[int(TechUnit::Lurkers)] += ui.type.supplyRequired();
				techScores[int(TechUnit::Ultralisks)] += ui.type.supplyRequired() + 2;
				techScores[int(TechUnit::Guardians)] += ui.type.supplyRequired() + 1;
			}

			// Various adjustments to the score.
			if (ui.type.airWeapon() == BWAPI::WeaponTypes::None)
			{
				// Enemy units that cannot shoot up.

				techScores[int(TechUnit::Mutalisks)] += ui.type.supplyRequired();
				techScores[int(TechUnit::Guardians)] += ui.type.supplyRequired();

				// Stuff that extra-favors spire.
				if (ui.type == BWAPI::UnitTypes::Protoss_High_Templar ||
					ui.type == BWAPI::UnitTypes::Protoss_Shuttle ||
					ui.type == BWAPI::UnitTypes::Protoss_Observer ||
					ui.type == BWAPI::UnitTypes::Protoss_Reaver)
				{
					techScores[int(TechUnit::Mutalisks)] += ui.type.supplyRequired();

					// And other adjustments for some of the units.
					if (ui.type == BWAPI::UnitTypes::Protoss_High_Templar)
					{
						// OK, not hydras versus high templar.
						techScores[int(TechUnit::Hydralisks)] -= ui.type.supplyRequired() + 1;
						techScores[int(TechUnit::Guardians)] -= 1;
					}
					else if (ui.type == BWAPI::UnitTypes::Protoss_Reaver)
					{
						techScores[int(TechUnit::Hydralisks)] -= 4;
						// Reavers eat lurkers, yum.
						techScores[int(TechUnit::Lurkers)] -= ui.type.supplyRequired();
					}
				}
			}

			if (ui.type == BWAPI::UnitTypes::Protoss_Archon ||
				ui.type == BWAPI::UnitTypes::Protoss_Dragoon ||
				ui.type == BWAPI::UnitTypes::Protoss_Scout)
			{
				// Enemy units that counter air units but suffer against hydras.
				techScores[int(TechUnit::Hydralisks)] += 2;
				if (ui.type == BWAPI::UnitTypes::Protoss_Dragoon)
				{
					techScores[int(TechUnit::Zerglings)] += 2;  // lings are also OK vs goons
				}
				else if (ui.type == BWAPI::UnitTypes::Protoss_Archon)
				{
					techScores[int(TechUnit::Zerglings)] -= 4;  // but bad against archons
				}
			}
		}
		else if (ui.type == BWAPI::UnitTypes::Protoss_Photon_Cannon)
		{
			// Hydralisks are efficient against cannons.
			techScores[int(TechUnit::Hydralisks)] += 2;
			techScores[int(TechUnit::Lurkers)] -= 3;
			techScores[int(TechUnit::Ultralisks)] += 6;
			techScores[int(TechUnit::Guardians)]  += 4;
		}
		else if (ui.type == BWAPI::UnitTypes::Protoss_Robotics_Facility)
		{
			// Observers are quick to get if they already have robo.
			techScores[int(TechUnit::Lurkers)] -= 4;
			// Spire is good against anything from the robo fac.
			techScores[int(TechUnit::Mutalisks)] += 6;
		}
		else if (ui.type == BWAPI::UnitTypes::Protoss_Robotics_Support_Bay)
		{
			// Reavers eat lurkers.
			techScores[int(TechUnit::Lurkers)] -= 4;
			// Spire is especially good against reavers.
			techScores[int(TechUnit::Mutalisks)] += 8;
		}
	}
}

// Decide what units counter the terran unit mix.
// Hint: It's usually mutas. Hydras are good versus goliaths and air.
void StrategyBossZerg::vTerranTechScores()
{
	// Bias.
	techScores[int(TechUnit::Mutalisks)]  = 11;   // default lair tech
	techScores[int(TechUnit::Ultralisks)] = 25;   // default hive tech
	techScores[int(TechUnit::Guardians)]  =  9;   // other hive tech
	techScores[int(TechUnit::Devourers)]  =  6;   // other hive tech

	// Hysteresis.
	if (_techTarget != TechUnit::None)
	{
		techScores[int(_techTarget)] += 13;
	}

	for (const auto & kv : InformationManager::Instance().getUnitData(_enemy).getUnits())
	{
		const UnitInfo & ui(kv.second);
		
		if (ui.type == BWAPI::UnitTypes::Terran_Marine ||
			ui.type == BWAPI::UnitTypes::Terran_Medic ||
			ui.type == BWAPI::UnitTypes::Terran_Ghost)
		{
			techScores[int(TechUnit::Zerglings)] += 2;
			if (ui.type == BWAPI::UnitTypes::Terran_Medic)
			{
				// Medics make other infantry much more effective vs ground, especially vs tier 1.
				techScores[int(TechUnit::Zerglings)] -= 1;
				techScores[int(TechUnit::Hydralisks)] -= 1;
			}
			techScores[int(TechUnit::Mutalisks)] += 1;
			techScores[int(TechUnit::Lurkers)] += 2;
			techScores[int(TechUnit::Guardians)] += 1;
			techScores[int(TechUnit::Ultralisks)] += 3;
		}
		else if (ui.type == BWAPI::UnitTypes::Terran_Firebat)
		{
			techScores[int(TechUnit::Zerglings)] -= 2;
			techScores[int(TechUnit::Hydralisks)] += 2;
			techScores[int(TechUnit::Mutalisks)] += 2;
			techScores[int(TechUnit::Lurkers)] += 2;
			techScores[int(TechUnit::Guardians)] += 1;
			techScores[int(TechUnit::Ultralisks)] += 4;
		}
		else if (ui.type == BWAPI::UnitTypes::Terran_Vulture_Spider_Mine)
		{
			techScores[int(TechUnit::Zerglings)] -= 1;
			techScores[int(TechUnit::Lurkers)] -= 1;
			techScores[int(TechUnit::Ultralisks)] -= 1;
			techScores[int(TechUnit::Mutalisks)] += 2;
			techScores[int(TechUnit::Guardians)] += 1;
		}
		else if (ui.type == BWAPI::UnitTypes::Terran_Vulture)
		{
			techScores[int(TechUnit::Zerglings)] -= 2;
			techScores[int(TechUnit::Hydralisks)] += 2;
			techScores[int(TechUnit::Lurkers)] -= 2;
			techScores[int(TechUnit::Mutalisks)] += 2;
			techScores[int(TechUnit::Ultralisks)] += 1;
		}
		else if (ui.type == BWAPI::UnitTypes::Terran_Goliath)
		{
			techScores[int(TechUnit::Hydralisks)] += 3;
			techScores[int(TechUnit::Lurkers)] -= 3;
			techScores[int(TechUnit::Mutalisks)] -= 2;
			techScores[int(TechUnit::Guardians)] -= 1;
			techScores[int(TechUnit::Ultralisks)] += 6;
		}
		else if (ui.type == BWAPI::UnitTypes::Terran_Wraith)
		{
			techScores[int(TechUnit::Hydralisks)] += 2;
			techScores[int(TechUnit::Lurkers)] -= 2;
			techScores[int(TechUnit::Guardians)] -= 2;
			techScores[int(TechUnit::Devourers)] += 2;
		}
		else if (ui.type == BWAPI::UnitTypes::Terran_Valkyrie ||
			ui.type == BWAPI::UnitTypes::Terran_Battlecruiser)
		{
			techScores[int(TechUnit::Hydralisks)] += 3;
			techScores[int(TechUnit::Guardians)] -= 2;
			techScores[int(TechUnit::Devourers)] += 6;
		}
		else if (ui.type == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode ||
			ui.type == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode)
		{
			techScores[int(TechUnit::Mutalisks)] += 6;
			techScores[int(TechUnit::Guardians)] += 6;
			techScores[int(TechUnit::Lurkers)] -= 5;
			techScores[int(TechUnit::Ultralisks)] += 2;
		}
		else if (ui.type == BWAPI::UnitTypes::Terran_Missile_Turret)
		{
			techScores[int(TechUnit::Zerglings)] += 1;
			techScores[int(TechUnit::Hydralisks)] += 1;
			techScores[int(TechUnit::Lurkers)] -= 1;
			techScores[int(TechUnit::Ultralisks)] += 2;
		}
		else if (ui.type == BWAPI::UnitTypes::Terran_Bunker)
		{
			techScores[int(TechUnit::Ultralisks)] += 4;
			techScores[int(TechUnit::Guardians)] += 5;
		}
		else if (ui.type == BWAPI::UnitTypes::Terran_Science_Vessel)
		{
			techScores[int(TechUnit::Ultralisks)] += 1;
		}
		else if (ui.type == BWAPI::UnitTypes::Terran_Dropship)
		{
			techScores[int(TechUnit::Mutalisks)] += 8;
			techScores[int(TechUnit::Ultralisks)] += 1;
		}
	}
}

// Decide what units counter the zerg unit mix.
// Don't make hydras versus zerg.
void StrategyBossZerg::vZergTechScores()
{
	// Bias.
	techScores[int(TechUnit::Zerglings)]  = 1;
	techScores[int(TechUnit::Mutalisks)]  = 3;   // default lair tech
	techScores[int(TechUnit::Ultralisks)] = 5;   // default hive tech

	// Hysteresis.
	if (_techTarget != TechUnit::None)
	{
		techScores[int(_techTarget)] += 3;
	}

	// NOTE Nothing decreases the zergling score or increases the hydra score.
	//      We never go hydra in ZvZ.
	//      But after getting hive we may go lurkers.
	for (const auto & kv : InformationManager::Instance().getUnitData(_enemy).getUnits())
	{
		const UnitInfo & ui(kv.second);

		if (ui.type == BWAPI::UnitTypes::Zerg_Sunken_Colony)
		{
			techScores[int(TechUnit::Mutalisks)] += 2;
			techScores[int(TechUnit::Ultralisks)] += 1;
			techScores[int(TechUnit::Guardians)] += 3;
		}
		else if (ui.type == BWAPI::UnitTypes::Zerg_Spore_Colony)
		{
			techScores[int(TechUnit::Zerglings)] += 1;
			techScores[int(TechUnit::Ultralisks)] += 2;
			techScores[int(TechUnit::Guardians)] += 2;
		}
		else if (ui.type == BWAPI::UnitTypes::Zerg_Zergling)
		{
			techScores[int(TechUnit::Mutalisks)] += 1;
			if (hasHiveTech)
			{
				techScores[int(TechUnit::Lurkers)] += 1;
			}
		}
		else if (ui.type == BWAPI::UnitTypes::Zerg_Mutalisk)
		{
			techScores[int(TechUnit::Lurkers)] -= 2;
			techScores[int(TechUnit::Guardians)] -= 1;
			techScores[int(TechUnit::Devourers)] += 3;
		}
		else if (ui.type == BWAPI::UnitTypes::Zerg_Scourge)
		{
			techScores[int(TechUnit::Ultralisks)] += 1;
			techScores[int(TechUnit::Guardians)] -= 1;
			techScores[int(TechUnit::Devourers)] -= 1;
		}
		else if (ui.type == BWAPI::UnitTypes::Zerg_Guardian)
		{
			techScores[int(TechUnit::Lurkers)] -= 2;
			techScores[int(TechUnit::Mutalisks)] += 2;
			techScores[int(TechUnit::Devourers)] += 1;
		}
		else if (ui.type == BWAPI::UnitTypes::Zerg_Devourer)
		{
			techScores[int(TechUnit::Mutalisks)] -= 2;
			techScores[int(TechUnit::Ultralisks)] += 1;
			techScores[int(TechUnit::Guardians)] -= 2;
			techScores[int(TechUnit::Devourers)] += 1;
		}
	}
}

// Calculate scores used to decide on tech target and unit mix.
void StrategyBossZerg::calculateTechScores()
{
	resetTechScores();

	if (_enemyRace == BWAPI::Races::Protoss)
	{
		vProtossTechScores();
	}
	else if (_enemyRace == BWAPI::Races::Terran)
	{
		vTerranTechScores();
	}
	else if (_enemyRace == BWAPI::Races::Zerg)
	{
		vZergTechScores();
	}

	// Undetected lurkers are more valuable.
	if (!InformationManager::Instance().enemyHasMobileDetection())
	{
		if (!InformationManager::Instance().enemyHasStaticDetection())
		{
			techScores[int(TechUnit::Lurkers)] += 5;
		}

		if (techScores[int(TechUnit::Lurkers)] == 0)
		{
			techScores[int(TechUnit::Lurkers)] = 3;
		}
		else
		{
			techScores[int(TechUnit::Lurkers)] = 3 * techScores[int(TechUnit::Lurkers)] / 2;
		}
	}

	// Otherwise enemy went random and we haven't seen any enemy unit yet.
	// Leave all the tech scores as 0 and go with the defaults.
}

// Choose the next tech to aim for, whether sooner or later.
// This tells freshProductionPlan() what to move toward, not when to take each step.
void StrategyBossZerg::chooseTechTarget()
{
	// Mark which tech units are available as targets.
	// First: If we already have it, it's not a target.
	std::array<bool, int(TechUnit::Size)> targetTaken;
	setAvailableTechUnits(targetTaken);

	// Second: If we don't have either lair tech yet, and they're not both useless,
	// then don't jump ahead to hive tech.
	if (!hasSpire && !hasLurkers && techScores[int(TechUnit::Mutalisks)] > 0 && techScores[int(TechUnit::Lurkers)] > 0)
	{
		targetTaken[int(TechUnit::Ultralisks)] = true;
		targetTaken[int(TechUnit::Guardians)] = true;
		targetTaken[int(TechUnit::Devourers)] = true;
	}

	// Third: In ZvZ, don't make hydras ever, and make lurkers only after hive.
	if (_enemyRace == BWAPI::Races::Zerg)
	{
		targetTaken[int(TechUnit::Hydralisks)] = true;
		if (!hasHiveTech)
		{
			targetTaken[int(TechUnit::Lurkers)] = true;
		}
	}

	// Find the best taken tech unit at tech tier 1 and tech tier 2.
	int maxAtTier1 = -99990;
	int maxAtTier2 = -99999;

	for (int i = int(TechUnit::None); i < int(TechUnit::Size); ++i)
	{
		if (targetTaken[i] && techScores[i] > maxAtTier1 && techTier(TechUnit(i)) == 1)
		{
			maxAtTier1 = techScores[i];
		}
		if (targetTaken[i] && techScores[i] > maxAtTier2 && techTier(TechUnit(i)) == 2)
		{
			maxAtTier2 = techScores[i];
		}
	}

	// Default. Value at the start of the game and after all tech is available.
	_techTarget = TechUnit::None;

	// Return the maximum of non-taken targets, if any.
	// Exception: Skip a tech tier if a taken target at that tier beats all non-taken targets
	// at the tier. In that case, we want to go up to the next tier.
	int techScore = -99999;
	for (int i = int(TechUnit::None); i < int(TechUnit::Size); ++i)
	{
		if (!targetTaken[i] && techScores[i] > techScore)
		{
			if (techTier(TechUnit(i)) == 1 && techScores[i] >= maxAtTier1 ||
				techTier(TechUnit(i)) == 2 && techScores[i] >= maxAtTier2 ||
				techTier(TechUnit(i)) == 3)
			{
				_techTarget = TechUnit(i);
				techScore = techScores[i];
			}
		}
	}
}

// Set _mineralUnit and _gasUnit depending on our tech and the game situation.
// This tells freshProductionPlan() what units to make.
void StrategyBossZerg::chooseUnitMix()
{
	// Mark which tech units are available for the unit mix.
	// If we have the tech for it, it can be in the unit mix.
	std::array<bool, int(TechUnit::Size)> available;
	setAvailableTechUnits(available);
	
	// Find the best available unit to be the main unit of the mix.
	TechUnit bestUnit = TechUnit::None;
	int techScore = -99999;
	for (int i = int(TechUnit::None); i < int(TechUnit::Size); ++i)
	{
		if (available[i] && techScores[i] > techScore)
		{
			bestUnit = TechUnit(i);
			techScore = techScores[i];
		}
	}

	// Defaults in case no unit type is available.
	BWAPI::UnitType minUnit = BWAPI::UnitTypes::Zerg_Drone;
	BWAPI::UnitType gasUnit = BWAPI::UnitTypes::None;

	// bestUnit is one unit of the mix. The other we fill in as reasonable.
	if (bestUnit == TechUnit::Zerglings)
	{
		if (hasPool)
		{
			minUnit = BWAPI::UnitTypes::Zerg_Zergling;
		}
	}
	else if (bestUnit == TechUnit::Hydralisks)
	{
		if (hasPool)
		{
			minUnit = BWAPI::UnitTypes::Zerg_Zergling;
		}
		gasUnit = BWAPI::UnitTypes::Zerg_Hydralisk;
	}
	else if (bestUnit == TechUnit::Lurkers)
	{
		if (!hasPool)
		{
			minUnit = BWAPI::UnitTypes::Zerg_Hydralisk;
		}
		else if (nGas >= 2 &&
			techScores[int(TechUnit::Hydralisks)] > 0 &&
			techScores[int(TechUnit::Hydralisks)] > 2 * techScores[int(TechUnit::Zerglings)])
		{
			minUnit = BWAPI::UnitTypes::Zerg_Hydralisk;
		}
		else
		{
			minUnit = BWAPI::UnitTypes::Zerg_Zergling;
		}
		gasUnit = BWAPI::UnitTypes::Zerg_Lurker;
	}
	else if (bestUnit == TechUnit::Mutalisks)
	{
		if (!hasPool && hasDen)
		{
			minUnit = BWAPI::UnitTypes::Zerg_Hydralisk;
		}
		else if (hasDen && nGas >= 2 &&
			techScores[int(TechUnit::Hydralisks)] > 0 &&
			techScores[int(TechUnit::Hydralisks)] > 2 * (5 + techScores[int(TechUnit::Zerglings)]))
		{
			minUnit = BWAPI::UnitTypes::Zerg_Hydralisk;
		}
		else if (hasPool)
		{
			minUnit = BWAPI::UnitTypes::Zerg_Zergling;
		}
		gasUnit = BWAPI::UnitTypes::Zerg_Mutalisk;
	}
	else if (bestUnit == TechUnit::Ultralisks)
	{
		if (!hasPool && hasDen)
		{
			minUnit = BWAPI::UnitTypes::Zerg_Hydralisk;
		}
		else if (hasDen && nGas >= 4 &&
			techScores[int(TechUnit::Hydralisks)] > 0 &&
			techScores[int(TechUnit::Hydralisks)] > 2 * (5 + techScores[int(TechUnit::Zerglings)]))
		{
			minUnit = BWAPI::UnitTypes::Zerg_Hydralisk;
		}
		else if (hasPool)
		{
			minUnit = BWAPI::UnitTypes::Zerg_Zergling;
		}
		gasUnit = BWAPI::UnitTypes::Zerg_Ultralisk;
	}
	else if (bestUnit == TechUnit::Guardians)
	{
		if (!hasPool && hasDen)
		{
			minUnit = BWAPI::UnitTypes::Zerg_Hydralisk;
		}
		else if (hasDen && nGas >= 3 && techScores[int(TechUnit::Hydralisks)] > techScores[int(TechUnit::Zerglings)])
		{
			minUnit = BWAPI::UnitTypes::Zerg_Hydralisk;
		}
		else if (hasPool)
		{
			minUnit = BWAPI::UnitTypes::Zerg_Zergling;
		}
		gasUnit = BWAPI::UnitTypes::Zerg_Ultralisk;
	}
	else if (bestUnit == TechUnit::Devourers)
	{
		// We want an anti-air unit in the mix to maximize the effect of the acid spores.
		if (hasDen && techScores[int(TechUnit::Hydralisks)] > techScores[int(TechUnit::Mutalisks)])
		{
			minUnit = BWAPI::UnitTypes::Zerg_Hydralisk;
		}
		else
		{
			minUnit = BWAPI::UnitTypes::Zerg_Mutalisk;
		}
		gasUnit = BWAPI::UnitTypes::Zerg_Devourer;
	}

	setUnitMix(minUnit, gasUnit);
}

// An auxiliary unit may be made in smaller numbers alongside the main unit mix.
// Case 1: We're preparing a morphed-unit tech and want some units to morph later.
// Case 2: We have a tech that can play a useful secondary role.
// NOTE This is a hack to tide the bot over until better production decisions can be made.
void StrategyBossZerg::chooseAuxUnit()
{
	const int maxAuxGuardians = 8;
	const int maxAuxDevourers = 4;

	// The default is no aux unit.
	_auxUnit = BWAPI::UnitTypes::None;
	_auxUnitCount = 0;

	// Case 1: Getting a morphed unit tech.
	if (_techTarget == TechUnit::Lurkers &&
		hasDen &&
		_mineralUnit != BWAPI::UnitTypes::Zerg_Hydralisk &&
		_gasUnit != BWAPI::UnitTypes::Zerg_Hydralisk)
	{
		_auxUnit = BWAPI::UnitTypes::Zerg_Hydralisk;
		_auxUnitCount = 4;
	}
	else if ((_techTarget == TechUnit::Guardians || _techTarget == TechUnit::Devourers) &&
		hasSpire &&
		hasHiveTech &&
		_gasUnit != BWAPI::UnitTypes::Zerg_Mutalisk)
	{
		_auxUnit = BWAPI::UnitTypes::Zerg_Mutalisk;
		_auxUnitCount = 6;
	}
	// Case 2: Secondary tech.
	else if (hasGreaterSpire &&
		_gasUnit != BWAPI::UnitTypes::Zerg_Guardian &&
		techScores[int(TechUnit::Guardians)] > 0 &&
		nGuardians < maxAuxGuardians)
	{
		_auxUnit = BWAPI::UnitTypes::Zerg_Guardian;
		_auxUnitCount = std::min(maxAuxGuardians, techScores[int(TechUnit::Guardians)] / 3);
	}
	else if (hasGreaterSpire &&
		(nHydras >= 8 || nMutas >= 6) &&
		_gasUnit != BWAPI::UnitTypes::Zerg_Devourer &&
		techScores[int(TechUnit::Devourers)] > 0 &&
		nDevourers < maxAuxDevourers)
	{
		_auxUnit = BWAPI::UnitTypes::Zerg_Devourer;
		_auxUnitCount = std::min(maxAuxDevourers, techScores[int(TechUnit::Devourers)] / 3);
	}
	else if (hasLurkers &&
		_gasUnit != BWAPI::UnitTypes::Zerg_Lurker &&
		techScores[int(TechUnit::Lurkers)] > 0)
	{
		_auxUnit = BWAPI::UnitTypes::Zerg_Lurker;
		_auxUnitCount = InformationManager::Instance().getNumSelfCompletedUnits(_mineralUnit) / 20;
	}
}

// Set the economy ratio according to the enemy race.
// If the enemy went random, the enemy race may change!
// This resets the drone/economy counts, so don't call it too often
// or you will get nothing but drones.
void StrategyBossZerg::chooseEconomyRatio()
{
	if (_enemyRace == BWAPI::Races::Zerg)
	{
		setEconomyRatio(0.15);
	}
	else if (_enemyRace == BWAPI::Races::Terran)
	{
		setEconomyRatio(0.45);
	}
	else if (_enemyRace == BWAPI::Races::Protoss)
	{
		setEconomyRatio(0.35);
	}
	else
	{
		// Enemy went random, race is still unknown. Choose cautiously.
		// We should find the truth soon enough.
		setEconomyRatio(0.20);
	}
}

// Choose current unit mix and next tech target to aim for.
// Called when the queue is empty and no future production is planned yet.
void StrategyBossZerg::chooseStrategy()
{
	calculateTechScores();  // the others depend on these values
	chooseTechTarget();
	chooseUnitMix();
	chooseAuxUnit();        // must be after the unit mix is set

	// Reset the economy ratio if the enemy's race has changed.
	// It can change from Unknown to another race if the enemy went random.
	if (_enemyRace != _enemy->getRace())
	{
		_enemyRace = _enemy->getRace();
		chooseEconomyRatio();
	}
}

std::string StrategyBossZerg::techTargetToString(TechUnit target)
{
	if (target == TechUnit::Zerglings) return "Lings";
	if (target == TechUnit::Hydralisks) return "Hydras";
	if (target == TechUnit::Lurkers) return "Lurkers";
	if (target == TechUnit::Mutalisks) return "Mutas";
	if (target == TechUnit::Ultralisks) return "Ultras";
	if (target == TechUnit::Guardians) return "Guardians";
	if (target == TechUnit::Devourers) return "Devourers";
	return "[none]";
}

// Draw various internal information bits, by default on the right side left of Bases.
void StrategyBossZerg::drawStrategyBossInformation()
{
	if (!Config::Debug::DrawStrategyBossInfo)
	{
		return;
	}

	const int x = 500;
	int y = 30;

	BWAPI::Broodwar->drawTextScreen(x, y, "%cStrat Boss", white);
	y += 13;
	BWAPI::Broodwar->drawTextScreen(x, y, "%cbases %c%d/%d", yellow, cyan, nBases, nBases+nFreeBases);
	y += 10;
	BWAPI::Broodwar->drawTextScreen(x, y, "%cpatches %c%d", yellow, cyan, nMineralPatches);
	y += 10;
	BWAPI::Broodwar->drawTextScreen(x, y, "%cgeysers %c%d+%d", yellow, cyan, nGas, nFreeGas);
	y += 10;
	BWAPI::Broodwar->drawTextScreen(x, y, "%cdrones%c %d/%d", yellow, cyan, nDrones, maxDrones);
	y += 10;
	BWAPI::Broodwar->drawTextScreen(x, y, "%c mins %c%d", yellow, cyan, nMineralDrones);
	y += 10;
	BWAPI::Broodwar->drawTextScreen(x, y, "%c gas %c%d", yellow, cyan, nGasDrones);
	y += 10;
	BWAPI::Broodwar->drawTextScreen(x, y, "%c react%c +%d", yellow, cyan, _extraDronesWanted);
	y += 10;
	BWAPI::Broodwar->drawTextScreen(x, y, "%clarvas %c%d", yellow, cyan, nLarvas);
	y += 13;
	if (outOfBook)
	{
		BWAPI::Broodwar->drawTextScreen(x, y, "%cecon %c%.2f", yellow, cyan, _economyRatio);
		for (int i = 1 + int(TechUnit::None); i < int(TechUnit::Size); ++i)
		{
			y += 10;
			std::array<bool, int(TechUnit::Size)> available;
			setAvailableTechUnits(available);

			BWAPI::Broodwar->drawTextScreen(x, y, "%c%s%c%s %c%d",
				white, available[i] ? "* " : "",
				orange, techTargetToString(TechUnit(i)).c_str(),
				cyan, techScores[i]);
		}
		y += 10;
		BWAPI::Broodwar->drawTextScreen(x, y, "%c%s", green, UnitTypeName(_mineralUnit).c_str());
		y += 10;
		BWAPI::Broodwar->drawTextScreen(x, y, "%c%s", green, UnitTypeName(_gasUnit).c_str());
		if (_auxUnit != BWAPI::UnitTypes::None)
		{
			y += 10;
			BWAPI::Broodwar->drawTextScreen(x, y, "%c%d%c %s", cyan, _auxUnitCount, green, UnitTypeName(_auxUnit).c_str());
		}
		if (_techTarget != TechUnit::None)
		{
			y += 10;
			BWAPI::Broodwar->drawTextScreen(x, y, "%cplan %c%s", white, green,
				techTargetToString(_techTarget).c_str());
		}
	}
	else
	{
		BWAPI::Broodwar->drawTextScreen(x, y, "%c[book]", white);
	}
	if (_emergencyGroundDefense)
	{
		y += 13;
		BWAPI::Broodwar->drawTextScreen(x, y, "%cEMERGENCY", red);
	}
}

// -- -- -- -- -- -- -- -- -- -- --
// Public methods.

StrategyBossZerg & StrategyBossZerg::Instance()
{
	static StrategyBossZerg instance;
	return instance;
}

// Set the unit mix.
// The mineral unit will can be set to Drone, but cannot be None.
// The mineral unit must be less gas-intensive than the gas unit.
// The idea is to make as many gas units as gas allows, and use any extra minerals
// on the mineral units (which may want gas too).
void StrategyBossZerg::setUnitMix(BWAPI::UnitType minUnit, BWAPI::UnitType gasUnit)
{
	_mineralUnit = minUnit;
	_gasUnit = gasUnit;
}

void StrategyBossZerg::setEconomyRatio(double ratio)
{
	_economyRatio = ratio;
	_economyDrones = 0;
	_economyTotal = 0;
}

// Solve urgent production issues. Called once per frame.
// If we're in trouble, clear the production queue and/or add emergency actions.
// Or if we just need overlords, make them.
// This routine is allowed to take direct actions or cancel stuff to get or preserve resources.
void StrategyBossZerg::handleUrgentProductionIssues(BuildOrderQueue & queue)
{
	updateGameState();

	while (nextInQueueIsUseless(queue))
	{
		// BWAPI::Broodwar->printf("removing useless %s", queue.getHighestPriorityItem().macroAct.getName().c_str());

		if (queue.getHighestPriorityItem().macroAct.isUnit() &&
			queue.getHighestPriorityItem().macroAct.getUnitType() == BWAPI::UnitTypes::Zerg_Hatchery)
		{
			// We only cancel a hatchery in case of dire emergency. Get the scout drone back home.
			ScoutManager::Instance().releaseWorkerScout();
			// Also cancel hatcheries already sent away for.
			BuildingManager::Instance().cancelBuildingType(BWAPI::UnitTypes::Zerg_Hatchery);
		}
		queue.removeHighestPriorityItem();
	}

	// Check for the most urgent actions once per frame.
	if (takeUrgentAction(queue))
	{
		// These are serious emergencies, and it's no help to check further.
		makeOverlords(queue);
	}
	else
	{
		// Check for less urgent reactions less often.
		int frameOffset = BWAPI::Broodwar->getFrameCount() % 32;
		if (frameOffset == 0)
		{
			makeUrgentReaction(queue);
			makeOverlords(queue);
		}
		else if (frameOffset == 16)
		{
			checkGroundDefenses(queue);
			makeOverlords(queue);
		}
		else if (frameOffset == 24)
		{
			analyzeExtraDrones();      // no need to make overlords
		}
	}
}

// Called when the queue is empty, which means that we are out of book.
// Fill up the production queue with new stuff.
BuildOrder & StrategyBossZerg::freshProductionPlan()
{
	auto & state = StateManager::Instance();
	auto & info = InformationManager::Instance();

	updateGameState();
	chooseStrategy();
	int larvasLeft = nLarvas;
	int mineralsLeft = minerals;
	const int armorUps = _self->getUpgradeLevel(BWAPI::UpgradeTypes::Zerg_Carapace);
	// If we're making gas units, short on gas, and not gathering gas, fix that first.
	if (_gasUnit != BWAPI::UnitTypes::None &&
		gas < _gasUnit.gasPrice() &&
		!WorkerManager::Instance().isCollectingGas())
	{
		produce(MacroCommandType::StartGas);			//qi: �ɼ�gas����������ͦ�õ�
	}

	// Get zergling speed if at all sensible.			//qi: С���Ƽ���������
	if (hasPool && nDrones >= 9 && nGas > 0 &&
		(nLings >= 6 || _mineralUnit == BWAPI::UnitTypes::Zerg_Zergling) &&
		_self->getUpgradeLevel(BWAPI::UpgradeTypes::Metabolic_Boost) == 0 &&
		!_self->isUpgrading(BWAPI::UpgradeTypes::Metabolic_Boost))
	{
		produce(BWAPI::UpgradeTypes::Metabolic_Boost);
		mineralsLeft -= 100;
	}

	// Ditto zergling attack rate.						//qi: С���Ƽ���������
	if (hasPool && hasHiveTech && nDrones >= 12 && nGas > 0 &&
		(nLings >= 8 || _mineralUnit == BWAPI::UnitTypes::Zerg_Zergling) &&
		_self->getUpgradeLevel(BWAPI::UpgradeTypes::Adrenal_Glands) == 0 &&
		!_self->isUpgrading(BWAPI::UpgradeTypes::Adrenal_Glands))
	{
		produce(BWAPI::UpgradeTypes::Adrenal_Glands);
		mineralsLeft -= 200;
	}

	// Get hydralisk den if it's next.					//qi: ��ʱ�������������
	if ((_techTarget == TechUnit::Hydralisks || _techTarget == TechUnit::Lurkers && lurkerDenTiming()) &&
		!hasDen && hasPool && nDrones >= 10 && nGas > 0 &&
		state.hydralisk_den_total == 0)
	{
		produce(BWAPI::UnitTypes::Zerg_Hydralisk_Den);
		mineralsLeft -= 100;
	}

	// Get hydra speed and range if they make sense.	//qi: ���߿Ƽ���������
	if (hasDen && nDrones >= 11 && nGas > 0 &&
		// Lurker aspect has priority, but we can get hydra upgrades until the lair starts.
		(_techTarget != TechUnit::Lurkers || nLairs + nHives == 0) &&
		(_mineralUnit == BWAPI::UnitTypes::Zerg_Hydralisk || _gasUnit == BWAPI::UnitTypes::Zerg_Hydralisk))
	{
		if (_self->getUpgradeLevel(BWAPI::UpgradeTypes::Muscular_Augments) == 0 &&
			!_self->isUpgrading(BWAPI::UpgradeTypes::Muscular_Augments))
		{
			produce(BWAPI::UpgradeTypes::Muscular_Augments);
			mineralsLeft -= 150;
		}
		else if (nHydras >= 3 &&
			_self->getUpgradeLevel(BWAPI::UpgradeTypes::Muscular_Augments) != 0 &&
			_self->getUpgradeLevel(BWAPI::UpgradeTypes::Grooved_Spines) == 0 &&
			!_self->isUpgrading(BWAPI::UpgradeTypes::Grooved_Spines))
		{
			produce(BWAPI::UpgradeTypes::Grooved_Spines);
			mineralsLeft -= 150;
		}
	}

	// Get lurker aspect if it's next.				//qi: �Ƿ�����Ǳ����
	if (hasDen && hasLairTech && nDrones >= 9 && nGas > 0 &&
		_techTarget == TechUnit::Lurkers &&
		!_self->hasResearched(BWAPI::TechTypes::Lurker_Aspect) &&
		!_self->isResearching(BWAPI::TechTypes::Lurker_Aspect))
	{
		produce(BWAPI::TechTypes::Lurker_Aspect);
		mineralsLeft -= 200;
	}

	// Make a lair. Make it earlier in ZvZ. Make it later if we only want it for hive units.
	if ((lairTechUnit(_techTarget) || hiveTechUnit(_techTarget) || armorUps > 0) &&
		hasPool && nLairs + nHives == 0 && nGas > 0 &&
		(nDrones >= (StateManager::Instance().being_rushed ? 10 : 12) || _enemyRace == BWAPI::Races::Zerg && nDrones >= 9))
	{
		produce(BWAPI::UnitTypes::Zerg_Lair);
		mineralsLeft -= 150;
	}

	// Make a spire. Make it earlier in ZvZ.
	if (!hasSpire && airTechUnit(_techTarget) && hasLairTech && nGas > 0 &&
		(nDrones >= 13 || _enemyRace == BWAPI::Races::Zerg && nDrones >= 9) &&
		!isBeingBuilt(BWAPI::UnitTypes::Zerg_Spire) &&
		state.spire_count == 0)
	{
		produce(BWAPI::UnitTypes::Zerg_Spire);
		mineralsLeft -= 200;
	}

	// Make a greater spire.
	if ((_techTarget == TechUnit::Guardians || _techTarget == TechUnit::Devourers) &&
		hasHiveTech && hasSpire && !hasGreaterSpire && nGas >= 2 && nDrones >= 15 &&
		state.greater_spire_count == 0)
	{
		produce(BWAPI::UnitTypes::Zerg_Greater_Spire);
		mineralsLeft -= 100;
	}

	// Make a queen's nest. Make it later versus zerg.
	if (!hasQueensNest && hasLair && nGas >= 2 && !_emergencyGroundDefense &&
		(hiveTechUnit(_techTarget) && nDrones >= 16 ||
			armorUps == 2 ||
			nDrones >= 24 ||
			_enemyRace != BWAPI::Races::Zerg && nDrones >= 20) &&
		!isBeingBuilt(BWAPI::UnitTypes::Zerg_Queens_Nest) &&
		state.queens_nest_total == 0)
	{
		produce(BWAPI::UnitTypes::Zerg_Queens_Nest);
		mineralsLeft -= 150;
	}

	// Make a hive.
	// Ongoing lair research will delay the hive.
	if ((hiveTechUnit(_techTarget) || armorUps >= 2) &&
		nHives == 0 && hasLair && hasQueensNest && nDrones >= 16 && nGas >= 2 &&
		!_self->isUpgrading(BWAPI::UpgradeTypes::Pneumatized_Carapace) &&
		!_self->isUpgrading(BWAPI::UpgradeTypes::Ventral_Sacs))
	{
		produce(BWAPI::UnitTypes::Zerg_Hive);
		mineralsLeft -= 200;
	}

	// Move toward ultralisks.					// qi: ��������
	if (_techTarget == TechUnit::Ultralisks && !hasUltra && hasHiveTech && nDrones >= 24 && nGas >= 3 &&
		!isBeingBuilt(BWAPI::UnitTypes::Zerg_Ultralisk_Cavern) &&
		state.ultralisk_cavern_count == 0)
	{
		produce(BWAPI::UnitTypes::Zerg_Ultralisk_Cavern);
		mineralsLeft -= 150;
	}
	else if (hasUltra && nDrones >= 24 && nGas >= 3)
	{
		if (_self->getUpgradeLevel(BWAPI::UpgradeTypes::Anabolic_Synthesis) == 0 &&
			!_self->isUpgrading(BWAPI::UpgradeTypes::Anabolic_Synthesis))
		{
			produce(BWAPI::UpgradeTypes::Anabolic_Synthesis);
			mineralsLeft -= 200;
		}
		else if (_self->getUpgradeLevel(BWAPI::UpgradeTypes::Anabolic_Synthesis) != 0 &&
			_self->getUpgradeLevel(BWAPI::UpgradeTypes::Chitinous_Plating) == 0 &&
			!_self->isUpgrading(BWAPI::UpgradeTypes::Chitinous_Plating))
		{
			produce(BWAPI::UpgradeTypes::Chitinous_Plating);
			mineralsLeft -= 150;
		}
	}

	// We want to expand.				// qi: ����
	// Division of labor: Expansions are here, macro hatcheries are "urgent production issues".
	// However, some macro hatcheries may be placed at expansions.
	if (nDrones > nMineralPatches + 3 * nGas && nFreeBases > 0 &&
		state.hatchery_waiting == 0 &&
		!state.being_rushed &&
		!state.natural_dangerous)
	{
		MacroLocation loc = MacroLocation::Expo;
		// Be a little generous with minonly expansions
		if (_gasUnit == BWAPI::UnitTypes::None || nHatches % 2 == 0)
		{
			loc = MacroLocation::MinOnly;
		}
		produce(MacroAct(BWAPI::UnitTypes::Zerg_Hatchery, loc));
		mineralsLeft -= 300;
	}

	// Get gas. If necessary, expand for it.
	// A. If we have enough economy, get gas.
	if (nGas == 0 && gas < 300 && nFreeGas > 0 && nDrones >= 9 && hasPool &&
		!isBeingBuilt(BWAPI::UnitTypes::Zerg_Extractor))
	{
		produce(BWAPI::UnitTypes::Zerg_Extractor);
		if (!WorkerManager::Instance().isCollectingGas())
		{
			produce(MacroCommandType::StartGas);
		}
		mineralsLeft -= 50;
	}
	// B. Or make more extractors if we have a low ratio of gas to minerals.
	else if (_gasUnit != BWAPI::UnitTypes::None &&
		nFreeGas > 0 &&
		nDrones > 3 * InformationManager::Instance().getNumBases(_self) + 3 * nGas + 4 &&
		(minerals + 100) / (gas + 100) >= 3 &&
		!isBeingBuilt(BWAPI::UnitTypes::Zerg_Extractor))
	{
		produce(BWAPI::UnitTypes::Zerg_Extractor);
		mineralsLeft -= 50;
	}
	// C. At least make a second extractor if we're going muta.
	else if (hasPool && _gasUnit == BWAPI::UnitTypes::Zerg_Mutalisk && nGas < 2 && nFreeGas > 0 && nDrones >= 10 &&
		!isBeingBuilt(BWAPI::UnitTypes::Zerg_Extractor))
	{
		produce(BWAPI::UnitTypes::Zerg_Extractor);
		mineralsLeft -= 50;
	}
	// D. Or expand if we are out of free geysers.
	else if ((_mineralUnit.gasPrice() > 0 || _gasUnit != BWAPI::UnitTypes::None) &&
		nFreeGas == 0 && nFreeBases > 0 &&
		nDrones > 3 * InformationManager::Instance().getNumBases(_self) + 3 * nGas + 5 &&
		(minerals + 100) / (gas + 100) >= 3 && minerals > 350 &&
		!isBeingBuilt(BWAPI::UnitTypes::Zerg_Extractor) &&
		state.hatchery_waiting == 0 &&
		!state.being_rushed && !state.natural_dangerous)
	{
		// This asks for a gas base, but we didn't check whether any are available.
		// If none are left, we'll get a mineral only.
		produce(BWAPI::UnitTypes::Zerg_Hatchery);
		mineralsLeft -= 300;
	}
	// E. Or being rushed
	else if ((state.being_rushed || state.natural_dangerous) && mineralsLeft > 450 && state.hatchery_waiting == 0 && state.hatchery_count == state.hatchery_completed)
	{
		produce(MacroAct(BWAPI::UnitTypes::Zerg_Hatchery, MacroLocation::Macro));
		mineralsLeft -= 300;
	}

	// Prepare an evo chamber or two.				// qi������ǻ
	// Terran doesn't want the first evo until after den or spire.
	if (hasPool && nGas > 0 && !_emergencyGroundDefense &&
		state.evolution_chamber_total < 2)
	{
		if (nEvo == 0 && nDrones >= 18 && (_enemyRace != BWAPI::Races::Terran || hasDen || hasSpire || hasUltra) ||
			nEvo == 1 && nDrones >= 30 && nGas >= 2 && (hasDen || hasSpire || hasUltra) && _self->isUpgrading(BWAPI::UpgradeTypes::Zerg_Carapace))
		{
			produce(BWAPI::UnitTypes::Zerg_Evolution_Chamber);
			mineralsLeft -= 75;
		}
	}

	// If we're in reasonable shape, get carapace upgrades.		//qi: ����ǻ�ĵ��沿��װ������
	// Coordinate upgrades with the nextInQueueIsUseless() check.
	if (nEvo > 0 && nDrones >= 12 && nGas > 0 && !_emergencyGroundDefense &&
		hasPool &&
		!_self->isUpgrading(BWAPI::UpgradeTypes::Zerg_Carapace))
	{
		if (armorUps == 0 ||
			armorUps == 1 && hasLairTech ||
			armorUps == 2 && hasHiveTech)
		{
			// But delay if we're going mutas and don't have many yet. They want the resources.
			if (!(hasSpire && _gasUnit == BWAPI::UnitTypes::Zerg_Mutalisk && nMutas < 6))
			{
				produce(BWAPI::UpgradeTypes::Zerg_Carapace);
				mineralsLeft -= 150;     // not correct for upgrades 2 or 3
			}
		}
	}

	// If we have 2 evos, or if carapace upgrades are done, also get melee attack.		// qi: ����ǻ�Ĺ���������
	// Coordinate upgrades with the nextInQueueIsUseless() check.
	if ((nEvo >= 2 || nEvo > 0 && armorUps == 3) && nDrones >= 14 && nGas >= 2 && !_emergencyGroundDefense &&
		hasPool && (hasDen || hasSpire || hasUltra) &&
		!_self->isUpgrading(BWAPI::UpgradeTypes::Zerg_Melee_Attacks))
	{
		int attackUps = _self->getUpgradeLevel(BWAPI::UpgradeTypes::Zerg_Melee_Attacks);
		if (attackUps == 0 ||
			attackUps == 1 && hasLairTech ||
			attackUps == 2 && hasHiveTech)
		{
			// But delay if we're going mutas and don't have many yet. They want the resources.
			if (!(hasSpire && _gasUnit == BWAPI::UnitTypes::Zerg_Mutalisk && nMutas < 6))
			{
				produce(BWAPI::UpgradeTypes::Zerg_Melee_Attacks);
				mineralsLeft -= 100;   // not correct for upgrades 2 or 3
			}
		}
	}

	// Before the main production, squeeze out one aux unit, if we want one. Only one per call.
	if (_auxUnit != BWAPI::UnitTypes::None &&
		info.getNumSelfUnits(_auxUnit) < _auxUnitCount &&
		larvasLeft > 0 &&
		info.getNumSelfCompletedUnits(_mineralUnit) > 2 &&
		!_emergencyGroundDefense)
	{
		BWAPI::UnitType auxType = findUnitType(_auxUnit);
		if (mineralsLeft > _auxUnit.mineralPrice())
		{
			produce(auxType);
			--larvasLeft;
			mineralsLeft -= auxType.mineralPrice();
		}
	}

	// If we have resources left, make units too.										//qi: ��������֮�󣬻���ʣ��
	// Substitute in drones according to _economyRatio (findUnitType() does this).
	// NOTE Gas usage above in the code is not counted at all.
	if (_gasUnit == BWAPI::UnitTypes::None ||
		gas < _gasUnit.gasPrice() ||
		double(info.getNumSelfUnits(_mineralUnit)) / double(info.getNumSelfUnits(_gasUnit)) < 0.1)
	{
		// Only the mineral unit.
		while (larvasLeft >= 0 && mineralsLeft >= 0)
		{
			BWAPI::UnitType type = findUnitType(_mineralUnit);
			produce(type);
			--larvasLeft;
			mineralsLeft -= type.mineralPrice();
		}
	}
	else
	{
		// Make both units. The mineral unit may also need gas.
		// Make as many gas units as gas allows, mixing in mineral units as possible.
		// NOTE nGasUnits can be wrong for morphed units like lurkers!
		int nGasUnits = 1 + gas / _gasUnit.gasPrice();    // number remaining to make
		bool gasUnitNext = true;
		while (larvasLeft >= 0 && mineralsLeft >= 0)
		{
			BWAPI::UnitType type;
			if (nGasUnits > 0 && gasUnitNext)
			{
				type = findUnitType(_gasUnit);
				// If we expect to want mineral units, mix them in.
				if (nGasUnits < larvasLeft && nGasUnits * _gasUnit.mineralPrice() < mineralsLeft)
				{
					gasUnitNext = false;
				}
				if (type == _gasUnit)
				{
					--nGasUnits;
				}
			}
			else
			{
				type = findUnitType(_mineralUnit);
				gasUnitNext = true;
			}
			produce(type);
			--larvasLeft;
			mineralsLeft -= type.mineralPrice();
		}
		// Try for extra zerglings from the dregs, especially if we are low on gas.
		if (_mineralUnit != BWAPI::UnitTypes::Zerg_Zergling &&
			hasPool &&
			(_emergencyGroundDefense || gas < 100 && mineralsLeft >= 100 || gas >= 100 && mineralsLeft > 4 * gas))
		{
			while (larvasLeft > 0 && mineralsLeft >= 50)
			{
				produce(BWAPI::UnitTypes::Zerg_Zergling);
				--larvasLeft;
				mineralsLeft -= 50;
			}
		}
	}
	return _latestBuildOrder;
}
