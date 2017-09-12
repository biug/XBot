#include "Squad.h"
#include "UnitUtil.h"
#include "StrategyManager.h"

using namespace XBot;

Squad::Squad()
	: _name("Default")
	, _combatSquad(false)
	, _hasAir(false)
	, _hasGround(false)
	, _hasAntiAir(false)
	, _hasAntiGround(false)
	, _attackAtMax(false)
    , _lastRetreatSwitch(0)
    , _lastRetreatSwitchVal(false)
    , _priority(0)
{
    int a = 10;   // only you can prevent linker errors
}

// A "combat" squad is any squad except the Idle squad, which is full of workers
// (and possibly unused units like unassigned overlords).
// The usual work of workers is managed by WorkerManager. If we put workers into
// another squad, we have to notify WorkerManager.
Squad::Squad(const std::string & name, SquadOrder order, size_t priority)
	: _name(name)
	, _combatSquad(name != "Idle")
	, _hasAir(false)
	, _hasGround(false)
	, _hasAntiAir(false)
	, _hasAntiGround(false)
	, _attackAtMax(false)
	, _lastRetreatSwitch(0)
    , _lastRetreatSwitchVal(false)
	, _lastVisitMain(false)
	, _lastVisitNatural(false)
    , _priority(priority)
	, _order(order)
{
}

Squad::~Squad()
{
    clear();
}

// TODO make a proper dispatch system for different orders
void Squad::update()
{
	// update all necessary unit information within this squad
	updateUnits();

	if (_units.empty())
	{
		return;
	}

	// TODO This is a crude stand-in for a real survey squad controller.
	if (_order.getType() == SquadOrderTypes::Survey)
	{
		doSurvey();
	}

	bool needToRegroup = needsToRegroup();
    
	if (Config::Debug::DrawSquadInfo && _order.isRegroupableOrder()) 
	{
		BWAPI::Broodwar->drawTextScreen(200, 350, "%c%s", white, _regroupStatus.c_str());
	}

	//add qiyue
	if (StrategyManager::Instance().getOpeningGroup() == "zergling_rush")
		needToRegroup = false;

	if (needToRegroup)
	{
		// Regroup, aka retreat. Only fighting units care about regrouping.
		BWAPI::Position regroupPosition = calcRegroupPosition();

        if (Config::Debug::DrawCombatSimulationInfo)
        {
		    BWAPI::Broodwar->drawTextScreen(200, 150, "REGROUP");
        }

		if (Config::Debug::DrawSquadInfo)
		{
			BWAPI::Broodwar->drawCircleMap(regroupPosition.x, regroupPosition.y, 20, BWAPI::Colors::Purple, true);
		}
        
		_microMelee.regroup(regroupPosition);
		_microRanged.regroup(regroupPosition);
		_microFlyers.regroup(regroupPosition);
	}
	else
	{
		// No need to regroup. Execute micro.
		_microMelee.execute(_order);
		_microRanged.execute(_order);
		_microFlyers.execute(_order);
	}

	// Lurkers never regroup, always execute their order.
	// TODO It is because regrouping works poorly. It retreats and unburrows them too often.
	_microLurkers.execute(_order);

	// The remaining non-combat micro managers try to keep units near the front line.
	if (BWAPI::Broodwar->getFrameCount() % 8 == 3)    // deliberately lag a little behind reality
	{
		BWAPI::Unit vanguard = unitClosestToEnemy();

		// Detectors.
		_microDetectors.setUnitClosestToEnemy(vanguard);
		_microDetectors.execute(_order);
	}
}

bool Squad::isEmpty() const
{
    return _units.empty();
}

size_t Squad::getPriority() const
{
    return _priority;
}

void Squad::setPriority(const size_t & priority)
{
    _priority = priority;
}

void Squad::updateUnits()
{
	setAllUnits();
	setNearEnemyUnits();
	addUnitsToMicroManagers();
}

// Clean up the _units vector.
// Also notice and remember a few facts about the members of the squad.
// Note: Some units may be loaded in a bunker or transport and cannot accept orders.
//       Check unit->isLoaded() before issuing orders.
void Squad::setAllUnits()
{
	_hasAir = false;
	_hasGround = false;
	_hasAntiAir = false;
	_hasAntiGround = false;

	BWAPI::Unitset goodUnits;
	for (const auto unit : _units)
	{
		if (UnitUtil::IsValidUnit(unit))
		{
			goodUnits.insert(unit);

			if (unit->isFlying())
			{
				_hasAir = true;
			}
			else
			{
				_hasGround = true;
			}
			if (UnitUtil::CanAttackAir(unit))
			{
				_hasAntiAir = true;
			}
			if (UnitUtil::CanAttackGround(unit))
			{
				_hasAntiGround = true;
			}
		}
	}
	_units = goodUnits;
}

void Squad::setNearEnemyUnits()
{
	_nearEnemy.clear();

	for (const auto unit : _units)
	{
		if (!unit->getPosition().isValid())   // excludes loaded units
		{
			continue;
		}

		_nearEnemy[unit] = unitNearEnemy(unit);

		if (Config::Debug::DrawSquadInfo) {
			int left = unit->getType().dimensionLeft();
			int right = unit->getType().dimensionRight();
			int top = unit->getType().dimensionUp();
			int bottom = unit->getType().dimensionDown();

			int x = unit->getPosition().x;
			int y = unit->getPosition().y;

			BWAPI::Broodwar->drawBoxMap(x - left, y - top, x + right, y + bottom,
				(_nearEnemy[unit]) ? Config::Debug::ColorUnitNearEnemy : Config::Debug::ColorUnitNotNearEnemy);
		}
	}
}

void Squad::addUnitsToMicroManagers()
{
	BWAPI::Unitset meleeUnits;
	BWAPI::Unitset rangedUnits;
	BWAPI::Unitset detectorUnits;
	BWAPI::Unitset lurkerUnits;
	BWAPI::Unitset flyerUnits;

	for (const auto unit : _units)
	{
		if (unit->isCompleted() && unit->getHitPoints() > 0 && unit->exists() && !unit->isLoaded())
		{
			if (unit->getType() == BWAPI::UnitTypes::Zerg_Lurker)
			{
				lurkerUnits.insert(unit);
			}
			else if (unit->getType().isDetector() && !unit->getType().isBuilding())
			{
				detectorUnits.insert(unit);
			}
			else if (unit->getType().isFlyer() && unit->canAttack())
			{
				flyerUnits.insert(unit);
			}
			// NOTE Excludes some units: spellcasters, valkyries, corsairs, devourers.
			else if ((unit->getType().groundWeapon().maxRange() > 32) ||
				unit->getType() == BWAPI::UnitTypes::Zerg_Scourge)
			{
				rangedUnits.insert(unit);
			}
			else if (unit->getType().isWorker() && _combatSquad)
			{
				// If this is a combat squad, then workers are melee units like any other,
				// but we have to tell WorkerManager about them.
				// If it's not a combat squad, WorkerManager owns them; don't add them to a micromanager.
				WorkerManager::Instance().setCombatWorker(unit);
				meleeUnits.insert(unit);
			}
			else if (unit->getType().groundWeapon().maxRange() <= 32)
			{
				meleeUnits.insert(unit);
			}
			// NOTE Some units may fall through and not be assigned.
		}
	}

	_microMelee.setUnits(meleeUnits);
	_microFlyers.setUnits(flyerUnits);
	_microRanged.setUnits(rangedUnits);
	_microDetectors.setUnits(detectorUnits);
	_microLurkers.setUnits(lurkerUnits);
}

// Calculates whether to regroup, aka retreat. Does combat sim if necessary.
bool Squad::needsToRegroup()
{
	if (_units.empty())
	{
		_regroupStatus = std::string("No attackers available");
		return false;
	}

	// If we are not attacking, never regroup.
	// This includes the Defend and Drop orders (among others).
	if (!_order.isRegroupableOrder())
	{
		_regroupStatus = std::string("No attack order");
		return false;
	}

	// If we're nearly maxed and have good income or cash, don't retreat.
	if (BWAPI::Broodwar->self()->supplyUsed() >= 390 &&
		(BWAPI::Broodwar->self()->minerals() > 1000 || WorkerManager::Instance().getNumMineralWorkers() > 12))
	{
		_attackAtMax = true;
	}

	if (_attackAtMax)
	{
		if (BWAPI::Broodwar->self()->supplyUsed() < 320)
		{
			_attackAtMax = false;
		}
		else
		{
			_regroupStatus = std::string("Maxed. Banzai!");
			return false;
		}
	}

	BWAPI::Unit unitClosest = unitClosestToEnemy();

	if (!unitClosest)
	{
		_regroupStatus = std::string("No closest unit");
		return false;
	}

	std::vector<UnitInfo> enemyCombatUnits;
    const auto & enemyUnitInfo = InformationManager::Instance().getUnitInfo(BWAPI::Broodwar->enemy());

	// if none of our units are in range of any enemy units, don't retreat
	bool anyInRange = false;
    for (const auto & eui : enemyUnitInfo)
    {
		for (const auto u : _units)
        {
			if (!u->exists() || u->isLoaded())
			{
				continue;
			}

			// Max of weapon range and vision range. Vision range is as long or longer, except for tanks.
			// We assume that the tanks may siege, and check the siege range of unsieged tanks.
			if (UnitUtil::CanAttack(eui.second.type, u->getType()))
			{
				int range = 0;     // range of enemy unit
				if (eui.second.type == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode ||
					eui.second.type == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode)
				{
					range = (BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode).groundWeapon().maxRange() + 128;  // plus safety fudge
				}
				else
				{
					// Sight range is >= weapon range, so we can stop here.
					range = eui.second.type.sightRange();
				}
				range += 128;    // plus some to account for our squad spreading out

				if (range >= u->getDistance(eui.second.lastPosition))
				{
					anyInRange = true;
					break;   // break out of inner loop
				}
			}
        }

		if (anyInRange)
        {
            break;       // break out of outer loop
        }
    }

    if (!anyInRange)
    {
        _regroupStatus = std::string("No enemy units in range");
        return false;
    }

	// If we most recently retreated, don't attack again until retreatDuration frames have passed.
	const int retreatDuration = 3 * 24;
	bool retreat = _lastRetreatSwitchVal && (BWAPI::Broodwar->getFrameCount() - _lastRetreatSwitch < retreatDuration);

	if (!retreat)
	{
		// All other checks are done. Finally do the expensive combat simulation.
		CombatSimulation sim;

		sim.setCombatUnits(unitClosest->getPosition(), Config::Micro::CombatRegroupRadius);

		// add qiyue
		double score = 0;
		if (StrategyManager::Instance().getOpeningGroup() == "zergling_rush"){
			score = 1;
		}
		else
			score = sim.simulateCombat();
		//BWAPI::Broodwar->printf("score is %lf", score);
		//double score = sim.simulateCombat();

		retreat = score < 0;
		_lastRetreatSwitch = BWAPI::Broodwar->getFrameCount();
		_lastRetreatSwitchVal = retreat;

	}
	
	if (retreat)
	{
		_regroupStatus = std::string("Retreat");
	}
	else
	{
		_regroupStatus = std::string("Attack");
	}

	return retreat;
}

void Squad::setSquadOrder(const SquadOrder & so)
{
	_order = so;
}

bool Squad::containsUnit(BWAPI::Unit u) const
{
    return _units.contains(u);
}

bool Squad::containsUnitType(BWAPI::UnitType t) const
{
	for (const auto u : _units)
	{
		if (u->getType() == t)
		{
			return true;
		}
	}
	return false;
}

void Squad::clear()
{
	for (const auto unit : _units)
	{
		if (unit->getType().isWorker())
		{
			WorkerManager::Instance().finishedWithWorker(unit);
		}
	}

	_units.clear();
}

bool Squad::unitNearEnemy(BWAPI::Unit unit)
{
	UAB_ASSERT(unit, "missing unit");

	BWAPI::Unitset enemyNear;

	MapGrid::Instance().GetUnits(enemyNear, unit->getPosition(), 400, false, true);

	return enemyNear.size() > 0;
}

BWAPI::Position Squad::calcCenter()
{
    if (_units.empty())
    {
        if (Config::Debug::DrawSquadInfo)
        {
            BWAPI::Broodwar->printf("Squad::calcCenter() of empty squad");
        }
        return BWAPI::Position(0,0);
    }

	BWAPI::Position accum(0,0);
	for (const auto unit : _units)
	{
		if (unit->getPosition().isValid())
		{
			accum += unit->getPosition();
		}
	}
	return BWAPI::Position(accum.x / _units.size(), accum.y / _units.size());
}

BWAPI::Position Squad::calcRegroupPosition()
{
	BWAPI::Position regroup(0,0);

	int minDist = 100000;

	// Retreat to the location of the squad unit not near the enemy which is
	// closest to the order position.
	// NOTE May retreat somewhere silly if the chosen unit was newly produced.
	//      Zerg sometimes retreats back and forth through the enemy when new
	//      zerg units are produced in bases on opposite sides.
	for (const auto unit : _units)
	{
		// Count combat units only. Bug fix originally thanks to AIL, it's been rewritten a bit since then.
		if (!_nearEnemy[unit] &&
			!unit->getType().isDetector() &&
			unit->getType() != BWAPI::UnitTypes::Terran_Medic &&
			unit->getPosition().isValid())    // excludes loaded units
		{
			int dist = unit->getDistance(_order.getPosition());
			if (dist < minDist)
			{
				// If the squad has any ground units, don't try to retreat to the position of an air unit
				// which is flying in a place that a ground unit cannot reach.
				if (!_hasGround || -1 != MapTools::Instance().getGroundTileDistance(unit->getPosition(), _order.getPosition()))
				{
					minDist = dist;
					regroup = unit->getPosition();
				}
			}
		}
	}

	// Failing that, retreat to a base we own.
	if (regroup == BWAPI::Position(0,0))
	{
		// Retreat to the main base (guaranteed not null, even if the buildings were destroyed).
		BWTA::BaseLocation * base = InformationManager::Instance().getMyMainBaseLocation();

		// If the natural has been taken, retreat there instead.
		BWTA::BaseLocation * natural = InformationManager::Instance().getMyNaturalLocation();
		if (natural && InformationManager::Instance().getBaseOwner(natural) == BWAPI::Broodwar->self())
		{
			base = natural;
		}
		return BWTA::getRegion(base->getTilePosition())->getCenter();
	}
	return regroup;
}

// Return the unit closest to the order position (not actually closest to the enemy).
BWAPI::Unit Squad::unitClosestToEnemy()
{
	BWAPI::Unit closest = nullptr;
	int closestDist = 100000;

	UAB_ASSERT(_order.getPosition().isValid(), "bad order position");

	for (const auto unit : _units)
	{
		// Non-combat units should be ignored for this calculation.
		if (unit->getType().isDetector() ||
			!unit->getPosition().isValid() ||       // includes units loaded into bunkers or transports
			unit->getType() == BWAPI::UnitTypes::Terran_Medic)
		{
			continue;
		}

		int dist;
		if (_hasGround)
		{
			// A ground or air-ground squad. Use ground distance.
			// It is -1 if no ground path exists.
			dist = MapTools::Instance().getGroundDistance(unit->getPosition(), _order.getPosition());
		}
		else
		{
			// An all-air squad. Use air distance (which is what unit->getDistance() gives).
			dist = unit->getDistance(_order.getPosition());
		}

		if (dist < closestDist && dist != -1)
		{
			closest = unit;
			closestDist = dist;
		}
	}

	return closest;
}

const BWAPI::Unitset & Squad::getUnits() const	
{ 
	return _units; 
} 

const SquadOrder & Squad::getSquadOrder() const			
{ 
	return _order; 
}

void Squad::addUnit(BWAPI::Unit u)
{
	_units.insert(u);
}

void Squad::removeUnit(BWAPI::Unit u)
{
	if (_combatSquad && u->getType().isWorker())
	{
		WorkerManager::Instance().finishedWithWorker(u);
	}
	_units.erase(u);
}

// Remove all workers from the squad, releasing them back to WorkerManager.
void Squad::releaseWorkers()
{
	UAB_ASSERT(_combatSquad, "Idle squad should not release workers");

	for (const auto unit : _units)
	{
		if (unit->getType().isWorker())
		{
			removeUnit(unit);
		}
	}
}

const std::string & Squad::getName() const
{
    return _name;
}

void Squad::doSurvey()
{
	BWAPI::Unit surveyor = *(_units.begin());
	if (surveyor && surveyor->exists())
	{
		//如果没发现基地
		const auto & enemyMainLoc = InformationManager::Instance().getEnemyMainBaseLocation();
		if (!enemyMainLoc || !BWAPI::Broodwar->isExplored(enemyMainLoc->getTilePosition()))
		{
			//顺时针探路
			std::vector<BWAPI::TilePosition> starts;
			for (const auto start : BWEM::Map::Instance().StartingLocations())
			{
				starts.push_back(start);
			}
			auto cmp = [](const BWAPI::TilePosition baseTP1, const BWAPI::TilePosition baseTP2) {
				BWAPI::Position center(BWAPI::Broodwar->mapWidth() * 16, BWAPI::Broodwar->mapHeight() * 16);
				BWAPI::Position baseP1(baseTP1), baseP2(baseTP2);
				int dx1 = baseP1.x - center.x, dy1 = baseP1.y - center.y;
				int dx2 = baseP2.x - center.x, dy2 = baseP2.y - center.y;
				double theta1 = std::atan2(dy1, dx1);
				double theta2 = std::atan2(dy2, dx2);
				//升序列->顺时针
				return theta1 < theta2;
			};
			std::sort(starts.begin(), starts.end(), cmp);
			auto iter = std::find(starts.begin(), starts.end(), BWAPI::Broodwar->self()->getStartLocation());
			std::reverse(starts.begin(), iter);
			std::reverse(iter++, starts.end());
			std::reverse(starts.begin(), starts.end());
			for (const auto start : starts)
			{
				// if we haven't explored it yet
				for (int x = start.x - 2; x <= start.x + 2; ++x)
				{
					for (int y = start.y - 2; y <= start.y + 2; ++y)
					{
						if (!BWAPI::Broodwar->isExplored(x, y))
						{
							Micro::SmartMove(surveyor, BWAPI::Position(x * 32, y * 32));
							return;
						}
					}
				}
			}
			Micro::SmartMove(surveyor, _order.getPosition());
		}
		else
		{
			bool airWeapon = false;
			BWAPI::Unitset enemys;
			MapGrid::Instance().GetUnits(enemys, surveyor->getPosition(), 300, false, true);
			for (const auto & enemy : enemys)
			{
				if (enemy->getType().airWeapon() != BWAPI::WeaponTypes::None)
				{
					airWeapon = true;
					break;
				}
			}
			if (surveyor->getDistance(enemyMainLoc->getPosition()) > 64 && !_lastVisitMain)
			{
				Micro::SmartMove(surveyor, enemyMainLoc->getPosition());
			}
			if (surveyor->getDistance(enemyMainLoc->getPosition()) < 64 && !_lastVisitMain)
			{
				BWAPI::Broodwar->printf("visit main");
				_lastVisitMain = true;
			}
			auto enemyNaturalLoc = InformationManager::Instance().getEnemyNaturalLocation();
			if (airWeapon && enemyNaturalLoc)
			{
				if (surveyor->getDistance(enemyNaturalLoc->getPosition()) > 64 && _lastVisitMain && !_lastVisitNatural)
				{
					Micro::SmartMove(surveyor, enemyNaturalLoc->getPosition());
				}
				if (surveyor->getDistance(enemyNaturalLoc->getPosition()) < 64 && _lastVisitMain && !_lastVisitNatural)
				{
					BWAPI::Broodwar->printf("visit natural");
					_lastVisitNatural = true;
				}
				auto home = BWAPI::Position(BWAPI::Broodwar->self()->getStartLocation());
				if (surveyor->getDistance(home) > 64 && _lastVisitMain && _lastVisitNatural)
				{
					Micro::SmartMove(surveyor, home);
				}
			}
		}
	}
}
