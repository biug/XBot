#include "Common.h"
#include "InformationManager.h"
#include "MapTools.h"
#include "Random.h"
#include "UnitUtil.h"
#include "StateManager.h"

using namespace XBot;

InformationManager::InformationManager()
    : _self(BWAPI::Broodwar->self())
    , _enemy(BWAPI::Broodwar->enemy())
	, _enemyProxy(false)

	, _weHaveCombatUnits(false)
	, _enemyHasAntiAir(false)
	, _enemyHasAirTech(false)
	, _enemyHasCloakTech(false)
	, _enemyHasMobileCloakTech(false)
	, _enemyHasOverlordHunters(false)
	, _enemyHasStaticDetection(false)
	, _enemyHasMobileDetection(_enemy->getRace() == BWAPI::Races::Zerg)

	, _scanned(0)
	, _cols(BWAPI::Broodwar->mapWidth())
	, _rows(BWAPI::Broodwar->mapHeight())
	, _tileAreas(_cols * _rows, nullptr)
{
	initializeTheBases();
	initializeRegionInformation();
	initializeNaturalBase();

	int maxTypeID(0);
	for (const BWAPI::UnitType & t : BWAPI::UnitTypes::allUnitTypes())
	{
		maxTypeID = maxTypeID > t.getID() ? maxTypeID : t.getID();
	}
	_numSelfUnits = std::vector<int>(maxTypeID + 1, 0);
	_numEnemyUnits = std::vector<int>(maxTypeID + 1, 0);
	_numSelfCompletedUnits = std::vector<int>(maxTypeID + 1, 0);
}

// This fills in _theBases with neutral bases. An event will place our resourceDepot.
void InformationManager::initializeTheBases()
{
	for (BWTA::BaseLocation * base : BWTA::getBaseLocations())
	{
		_theBases[base] = new UABase(base->getPosition());
	}
}

// Set up _mainBaseLocations and _occupiedLocations.
void InformationManager::initializeRegionInformation()
{
	_mainBaseLocations[_self] = BWTA::getStartLocation(_self);
	_mainBaseLocations[_enemy] = BWTA::getStartLocation(_enemy);
	_naturalBaseLocations[_self] = nullptr;
	_naturalBaseLocations[_enemy] = nullptr;

	// push that region into our occupied vector
	updateOccupiedRegions(BWTA::getRegion(_mainBaseLocations[_self]->getTilePosition()), _self);
}

// Figure out what base is our "natural expansion". In rare cases, there might be none.
// Prerequisite: Call initializeRegionInformation() first.
void InformationManager::initializeNaturalBase()
{
	updateNaturalBase(_self);
}

// A base is inferred to exist at the given position, without having been seen.
// Only enemy bases can be inferred; we see our own.
// Adjust its value to match. It is not reserved.
void InformationManager::baseInferred(BWTA::BaseLocation * base)
{
	if (_theBases[base]->owner != _self)
	{
		_theBases[base]->setOwner(nullptr, _enemy);
	}
}

// The given resource depot has been created or discovered.
// Adjust its value to match. It is not reserved.
// This accounts for the theoretical case that it might be neutral.
void InformationManager::baseFound(BWAPI::Unit depot)
{
	UAB_ASSERT(depot->getType().isResourceDepot(), "non-depot base");

	BWAPI::Player owner = BWAPI::Broodwar->neutral();

	if (depot->getPlayer() == _self || depot->getPlayer() == _enemy)
	{
		owner = depot->getPlayer();
	}

	for (BWTA::BaseLocation * base : BWTA::getBaseLocations())
	{
		if (closeEnough(base->getTilePosition(), depot->getTilePosition()))
		{
			_theBases[base]->setOwner(depot, owner);
			return;
		}
	}
}

// Something that may be a base was just destroyed.
// If it is, update the value to match.
// If the lost base was our main, choose a new one if possible.
void InformationManager::baseLost(BWAPI::TilePosition basePosition)
{
	for (BWTA::BaseLocation * base : BWTA::getBaseLocations())
	{
		if (closeEnough(base->getTilePosition(), basePosition))
		{
			_theBases[base]->setOwner(nullptr, BWAPI::Broodwar->neutral());
			if (base == getMyMainBaseLocation())
			{
				chooseNewMainBase();        // our main was lost, choose a new one
			}
			return;
		}
	}
}

// Our main base has been destroyed. Choose a new one if possible.
// Otherwise we'll keep trying to build in the old one, where the enemy may still be.
void InformationManager::chooseNewMainBase()
{
	BWTA::BaseLocation * oldMain = getMyMainBaseLocation();

	// Choose a base we own which is as far away from the old main as possible.
	// Maybe that will be safer.
	double newMainDist = 0.0;
	BWTA::BaseLocation * newMain = nullptr;

	for (BWTA::BaseLocation * base : BWTA::getBaseLocations())
	{
		if (_theBases[base]->owner == _self)
		{
			double dist = base->getAirDistance(oldMain);
			if (dist > newMainDist)
			{
				newMainDist = dist;
				newMain = base;
			}
		}
	}

	// If we didn't find a new main base, we're in deep trouble. We may as well keep the old one.
	// By decree, we always have a main base, even if it is unoccupied. It simplifies the rest.
	if (newMain)
	{
		_mainBaseLocations[_self] = newMain;
	}
}

// With some probability, randomly choose a base as the new "main" base.
void InformationManager::maybeChooseNewMainBase()
{
	// 1. Decide randomly whether to choose a new base.
	if (Random::Instance().index(2) == 0)
	{
		// 2. List my bases.
		std::vector<BWTA::BaseLocation *> myBases;
		for (BWTA::BaseLocation * base : BWTA::getBaseLocations())
		{
			if (_theBases[base]->owner == _self &&
				_theBases[base]->resourceDepot &&
				_theBases[base]->resourceDepot->isCompleted())
			{
				 myBases.push_back(base);
			}
		}

		// 3. Choose one, if there is a choice.
		if (myBases.size() > 1)
		{
			_mainBaseLocations[_self] = myBases.at(Random::Instance().index(myBases.size()));
		}
	}
}

// The given unit was just created or morphed.
// If it is a resource depot for our new base, record it.
// NOTE: It is a base only if it's in the right position according to BWTA.
// A resource depot will not be recorded if it is offset by too much.
// NOTE: This records the initial depot at the start of the game.
// There's no need to take special action to record the starting base.
// NOTE: Does not detect when a hatchery is cancelled during construction.
void InformationManager::maybeAddBase(BWAPI::Unit unit)
{
	if (unit->getType().isResourceDepot())
	{
		baseFound(unit);
	}
}

// The two possible base positions are close enough together
// that we can say they are "the same place" as a base.
bool InformationManager::closeEnough(BWAPI::TilePosition a, BWAPI::TilePosition b)
{
	return abs(a.x - b.x) <= 2 && abs(a.y - b.y) <= 2;
}

void InformationManager::update()
{
	updateUnitInfo();
	updateBaseLocationInfo();
	for (const auto & area : BWEM::Map::Instance().Areas())
	{
		for (const auto & choke : area.ChokePoints())
		{
			BWAPI::Broodwar->drawCircleMap(BWAPI::Position(choke->Center()), 10, BWAPI::Colors::Red, true);
		}
	}
}

void InformationManager::updateUnitInfo() 
{
	for (const auto unit : _enemy->getUnits())
	{
		updateUnit(unit);
	}
	for (auto & num : _numEnemyUnits)
	{
		num = 0;
	}
	for (const auto & ui : _unitData[_enemy].getUnits())
	{
		++_numEnemyUnits[ui.second.type.getID()];
	}

	for (auto & num : _numSelfUnits)
	{
		num = 0;
	}
	for (auto & num : _numSelfCompletedUnits)
	{
		num = 0;
	}
	for (const auto unit : _self->getUnits())
	{
		updateUnit(unit);
		auto type = unit->getType();
		if (unit->isCompleted() && !unit->isMorphing())
		{
			++_numSelfUnits[type.getID()];
			++_numSelfCompletedUnits[type.getID()];
		}
		else
		{
			if (type == BWAPI::UnitTypes::Zerg_Egg
				|| type == BWAPI::UnitTypes::Zerg_Cocoon
				|| type == BWAPI::UnitTypes::Zerg_Lurker_Egg)
			{
				type = unit->getBuildType();
				++_numSelfUnits[type.getID()];
				if (type.isTwoUnitsInOneEgg())
				{
					++_numSelfUnits[type.getID()];
				}
			}
			else if (type.isBuilding())
			{
				++_numSelfUnits[unit->getBuildType()];
			}
		}
	}

	_unitData[_enemy].removeBadUnits();
	_unitData[_self].removeBadUnits();
}

void InformationManager::updateBaseLocationInfo() 
{
	while (_scanned < _cols * _rows)
	{
		// init tile areas
		int c = _scanned / _rows;
		int r = _scanned % _rows;
		const auto & area = BWEM::Map::Instance().GetNearestArea(BWAPI::TilePosition(c, r));
		_tileAreas[getTileIndex(c, r)] = area;
		++_scanned;
	}

	_occupiedRegions[_self].clear();
	_occupiedRegions[_enemy].clear();

	// find a choke
	auto enemyChokeBase = isEnemyBuildingOnChoke();
	if (enemyChokeBase.first)
	{
		BWAPI::Broodwar->printf("FIND A CHOKE BUILDING");
		BWAPI::Broodwar->printf("FIND A CHOKE BUILDING");
		BWAPI::Broodwar->printf("FIND A CHOKE BUILDING");
		if (!_mainBaseLocations[_enemy])
		{
			_mainBaseLocations[_enemy] = enemyChokeBase.first;
			baseInferred(enemyChokeBase.first);
			updateOccupiedRegions(BWTA::getRegion(enemyChokeBase.first->getTilePosition()), _enemy);
		}
		Config::Scout::ScoutRound = 0;
		StateManager::Instance().being_terran_choke = true;
		StateManager::Instance().anti_terran_choke_pos = enemyChokeBase.second;
	}
		
	// if we haven't found the enemy main base location yet
	if (!_mainBaseLocations[_enemy]) 
	{ 
		// how many start locations have we explored
		int exploredStartLocations = 0;
		bool baseFound = false;

		// an undexplored base location holder
		BWTA::BaseLocation * unexplored = nullptr;

		for (BWTA::BaseLocation * startLocation : BWTA::getStartLocations()) 
		{
			if (isEnemyBuildingInRegion(BWTA::getRegion(startLocation->getTilePosition()))) 
			{
				updateOccupiedRegions(BWTA::getRegion(startLocation->getTilePosition()), _enemy);

				// On a competition map, our base and the enemy base will never be in the same region.
				// If we find an enemy building in our region, it's a proxy.
				if (startLocation == getMyMainBaseLocation())
				{
					_enemyProxy = true;
				}
				else
				{
					if (Config::Debug::DrawScoutInfo)
					{
						BWAPI::Broodwar->printf("Enemy base found by seeing it");
					}

					baseFound = true;
					_mainBaseLocations[_enemy] = startLocation;
					baseInferred(startLocation);
				}
			}

			// if it's explored, increment
			// TODO If the enemy is zerg, we can be a little quicker by looking for creep.
			// TODO If we see a mineral patch that has been mined, that should be a base.
			if (BWAPI::Broodwar->isExplored(startLocation->getTilePosition())) 
			{
				exploredStartLocations++;

			// otherwise set the unexplored base
			} 
			else 
			{
				unexplored = startLocation;
			}
		}

		// if we've explored every start location except one, it's the enemy
		if (!baseFound && exploredStartLocations + 1 == BWAPI::Broodwar->getStartLocations().size()) 
		{
            if (Config::Debug::DrawScoutInfo)
            {
                BWAPI::Broodwar->printf("Enemy base found by elimination");
            }
			
			_mainBaseLocations[_enemy] = unexplored;
			baseInferred(unexplored);
			updateOccupiedRegions(BWTA::getRegion(unexplored->getTilePosition()), _enemy);
		}
	// otherwise we do know it, so push it back
	}
	else 
	{
		updateOccupiedRegions(BWTA::getRegion(_mainBaseLocations[_enemy]->getTilePosition()), _enemy);
	}
	updateNaturalBase(_enemy);

	// The enemy occupies a region if it has a building there.
	for (const auto & kv : _unitData[_enemy].getUnits())
	{
		const UnitInfo & ui(kv.second);
		BWAPI::UnitType type = ui.type;

		if (type.isBuilding()) 
		{
			updateOccupiedRegions(BWTA::getRegion(BWAPI::TilePosition(ui.lastPosition)), _enemy);
		}
	}

	// We occupy a region if we have a building there.
	for (const auto & kv : _unitData[_self].getUnits())
	{
		const UnitInfo & ui(kv.second);
		BWAPI::UnitType type = ui.type;

		if (type.isBuilding()) 
		{
			updateOccupiedRegions(BWTA::getRegion(BWAPI::TilePosition(ui.lastPosition)), _self);
		}
	}
}

BWTA::BaseLocation* InformationManager::getNaturalBase(BWTA::BaseLocation* startLoc)
{
	// We'll go through the bases and pick the best one as the natural.
	BWTA::BaseLocation * bestBase = nullptr;
	double bestScore = 0.0;

	if (!startLoc) return nullptr;
	BWAPI::TilePosition homeTile = startLoc->getTilePosition();
	BWAPI::Position homePos(homeTile);

	for (BWTA::BaseLocation * base : BWTA::getBaseLocations())
	{
		double score = 0.0;

		BWAPI::TilePosition tile = base->getTilePosition();

		// The main is not the natural.
		if (tile == homeTile)
		{
			continue;
		}

		// Ww want to be close to our own base.
		double distanceFromUs = MapTools::Instance().getGroundTileDistance(BWAPI::Position(tile), homePos);

		// If it is not connected, skip it. Islands do this.
		if (!BWTA::isConnected(homeTile, tile) || distanceFromUs < 0)
		{
			continue;
		}

		// Add up the score.
		score = -distanceFromUs;

		// More resources -> better.
		score += 0.01 * base->minerals() + 0.02 * base->gas();

		if (!bestBase || score > bestScore)
		{
			bestBase = base;
			bestScore = score;
		}
	}
	return bestBase;
}

void InformationManager::updateNaturalBase(BWAPI::Player player)
{
	// if we have known, skip
	if (_naturalBaseLocations[player]) return;

	// bestBase may be null on unusual maps.
	_naturalBaseLocations[player] = getNaturalBase(_mainBaseLocations[player]);
}

void InformationManager::updateOccupiedRegions(BWTA::Region * region, BWAPI::Player player) 
{
	// if the region is valid (flying buildings may be in nullptr regions)
	if (region)
	{
		// add it to the list of occupied regions
		_occupiedRegions[player].insert(region);
	}
}

bool InformationManager::isEnemyBuildingInRegion(BWTA::Region * region) 
{
	// invalid regions aren't considered the same, but they will both be null
	if (!region)
	{
		return false;
	}

	for (const auto & kv : _unitData[_enemy].getUnits())
	{
		const UnitInfo & ui(kv.second);
		if (ui.type.isBuilding()) 
		{
			if (BWTA::getRegion(BWAPI::TilePosition(ui.lastPosition)) == region) 
			{
				return true;
			}
		}
	}

	return false;
}

std::pair<BWTA::BaseLocation*, BWAPI::TilePosition> InformationManager::isEnemyBuildingOnChoke()
{	
	// invalid if frame > 4000
	if (BWAPI::Broodwar->getFrameCount() > 4000) return { nullptr,BWAPI::TilePositions::None };
	// invalid for non-terran
	if (BWAPI::Broodwar->enemy()->getRace() != BWAPI::Races::Terran) return { nullptr,BWAPI::TilePositions::None };
	// invalid if have found choke
	if (StateManager::Instance().being_terran_choke) return { nullptr,BWAPI::TilePositions::None };

	const auto & mainArea = getTileArea(BWAPI::Broodwar->self()->getStartLocation());
	std::hash_map<const BWEM::Area *, BWTA::BaseLocation*> startAreas;
	for (const auto & start : BWTA::getStartLocations())
	{
		auto startArea = getTileArea(start->getTilePosition());
		if (startArea && startArea != mainArea) startAreas[startArea] = start;
		else if (!startArea) return { nullptr,BWAPI::TilePositions::None };
	}
	for (const auto & kv : _unitData[_enemy].getUnits())
	{
		const UnitInfo & ui(kv.second);
		BWAPI::Broodwar->drawCircleMap(ui.lastPosition, 10, BWAPI::Colors::Blue, true);
		if (!ui.type.isBuilding()) continue;
		const auto & area = getTileArea(ui.lastTilePosition);
		if (!area) continue;
		const BWEM::ChokePoint* chokePoint = nullptr;
		for (const auto & choke : area->ChokePoints())
		{
			if (ui.lastPosition.getDistance(BWAPI::Position(choke->Center())) < 192)
			{
				chokePoint = choke;
				break;
			}
		}
		if (!chokePoint) continue;
		// if building near the choke
		// first, determine if the choke is main choke or natural choke
		for (const auto & start : startAreas)
		{
			// base cannot too far
			if (start.second->getPosition().getDistance(ui.lastPosition) > 2000) continue;
			for (const auto & choke : start.first->ChokePoints())
			{
				// chokePoint is main choke
				// return natural point
				if (choke == chokePoint)
				{
					auto naturalBase = getNaturalBase(start.second);
					if (!naturalBase) continue;
					return { start.second, naturalBase->getTilePosition() };
				}
			}
			auto naturalBase = getNaturalBase(start.second);
			if (!naturalBase) continue;
			auto naturalArea = getTileArea(naturalBase->getTilePosition());
			if (!naturalArea) continue;
			for (const auto & choke : naturalArea->ChokePoints())
			{
				// chokePoint is natural choke
				if (choke == chokePoint)
				{
					// find the choke
					BWAPI::TilePosition destine = BWAPI::TilePositions::None;
					auto chokeCenter = BWAPI::TilePosition(choke->Center());
					auto naturalCenter = naturalBase->getTilePosition();
					if (chokeCenter.x == naturalCenter.x)
					{
						destine.x = chokeCenter.x;
						destine.y = chokeCenter.y < naturalCenter.y ? chokeCenter.y - 6 : chokeCenter.y + 6;
						return { start.second, destine };
					}
					else
					{
						double k = abs((double)(chokeCenter.y - naturalCenter.y) / (double)(chokeCenter.x - naturalCenter.x));
						double ratio = 1 / sqrt(1 + k * k);
						destine.x = chokeCenter.x + (chokeCenter.x < naturalCenter.x ? -ratio : ratio) * 6;
						destine.y = chokeCenter.y + (chokeCenter.y < naturalCenter.y ? -ratio : ratio) * 6 * k;
						return { start.second, destine };
					}
				}
			}
		}
	}
	return { nullptr,BWAPI::TilePositions::None };
}

const UIMap & InformationManager::getUnitInfo(BWAPI::Player player) const
{
	return getUnitData(player).getUnits();
}

std::set<BWTA::Region *> & InformationManager::getOccupiedRegions(BWAPI::Player player)
{
	return _occupiedRegions[player];
}

BWTA::BaseLocation * InformationManager::getMainBaseLocation(BWAPI::Player player)
{
	return _mainBaseLocations[player];
}

// Guaranteed non-null. If we have no bases left, it is our start location.
BWTA::BaseLocation * InformationManager::getMyMainBaseLocation()
{
	UAB_ASSERT(_mainBaseLocations[_self], "no base location");
	return _mainBaseLocations[_self];
}

// Null until the enemy base is located.
BWTA::BaseLocation * InformationManager::getEnemyMainBaseLocation()
{
	return _mainBaseLocations[_enemy];
}

// Self, enemy, or neutral.
BWAPI::Player InformationManager::getBaseOwner(BWTA::BaseLocation * base)
{
	return _theBases[base]->owner;
}

// If it's the enemy base, the depot will be null if it has not been seen.
// If this is our base, there is still a chance that the depot may be null.
// And if not null, the depot may be incomplete.
BWAPI::Unit InformationManager::getBaseDepot(BWTA::BaseLocation * base)
{
	return _theBases[base]->resourceDepot;
}

BWTA::BaseLocation * InformationManager::getNaturalLocation(BWAPI::Player player)
{
	return _naturalBaseLocations[player];
}

// The natural base, whether it is taken or not.
// May be null on some maps.
BWTA::BaseLocation * InformationManager::getMyNaturalLocation()
{
	return _naturalBaseLocations[_self];
}

// The natural base, whether it is taken or not.
// May be null on some maps.
BWTA::BaseLocation * InformationManager::getEnemyNaturalLocation()
{
	return _naturalBaseLocations[_enemy];
}

const BWEM::Area * InformationManager::getTileArea(BWAPI::TilePosition tile) const
{
	if (tile.x >= 0 && tile.x < _cols && tile.y >= 0 && tile.y < _rows)
		return _tileAreas[getTileIndex(tile)];
	return nullptr;
}

// The number of bases believed owned by the given player,
// self, enemy, or neutral.
int InformationManager::getNumBases(BWAPI::Player player)
{
	int count = 0;

	for (BWTA::BaseLocation * base : BWTA::getBaseLocations())
	{
		if (_theBases[base]->owner == player)
		{
			++count;
		}
	}

	return count;
}

// The number of non-island expansions that are not yet believed taken.
int InformationManager::getNumFreeLandBases()
{
	int count = 0;

	for (BWTA::BaseLocation * base : BWTA::getBaseLocations())
	{
		if (_theBases[base]->owner == BWAPI::Broodwar->neutral() && !base->isIsland())
		{
			++count;
		}
	}

	return count;
}

// Current number of mineral patches at all of my bases.
// Decreases as patches mine out, increases as new bases are taken.
int InformationManager::getMyNumMineralPatches()
{
	int count = 0;

	for (BWTA::BaseLocation * base : BWTA::getBaseLocations())
	{
		if (_theBases[base]->owner == _self)
		{
			count += base->getMinerals().size();
		}
	}

	return count;
}

// Current number of geysers at all my completed bases, whether taken or not.
// Skip bases where the resource depot is not finished.
int InformationManager::getMyNumGeysers()
{
	int count = 0;

	for (BWTA::BaseLocation * base : BWTA::getBaseLocations())
	{
		BWAPI::Unit depot = _theBases[base]->resourceDepot;

		if (_theBases[base]->owner == _self &&
			depot &&                // should never be null, but we check anyway
			(depot->isCompleted() || UnitUtil::IsMorphedBuildingType(depot->getType())))
		{
			count += base->getGeysers().size();
		}
	}

	return count;
}

// Current number of completed refineries at my completed bases,
// and number of bare geysers available to be taken.
void InformationManager::getMyGasCounts(int & nRefineries, int & nFreeGeysers)
{
	int refineries = 0;
	int geysers = 0;

	for (BWTA::BaseLocation * base : BWTA::getBaseLocations())
	{
		BWAPI::Unit depot = _theBases[base]->resourceDepot;

		if (_theBases[base]->owner == _self &&
			depot &&                // should never be null, but we check anyway
			(depot->isCompleted() || UnitUtil::IsMorphedBuildingType(depot->getType())))
		{
			// Recalculate the base's geysers every time.
			// This is a slow but accurate way to work around the BWAPI geyser bug.
			// To save cycles, call findGeysers() only when necessary (e.g. a refinery is destroyed).
			_theBases[base]->findGeysers();

			for (const auto geyser : _theBases[base]->getGeysers())
			{
				if (geyser && geyser->exists())
				{
					if (geyser->getPlayer() == _self &&
						geyser->getType().isRefinery() &&
						geyser->isCompleted())
					{
						++refineries;
					}
					else if (geyser->getPlayer() == BWAPI::Broodwar->neutral())
					{
						++geysers;
					}
				}
			}
		}
	}

	nRefineries = refineries;
	nFreeGeysers = geysers;
}

int InformationManager::getAir2GroundSupply(BWAPI::Player player) const
{
	int supply = 0;

	for (const auto & kv : getUnitData(player).getUnits())
	{
		const UnitInfo & ui(kv.second);

		if (ui.type.isFlyer() && UnitUtil::TypeCanAttackGround(ui.type))
		{
			supply += ui.type.supplyRequired();
		}
	}

	return supply;
}

void InformationManager::drawExtendedInterface()
{
    if (!Config::Debug::DrawUnitHealthBars)
    {
        return;
    }

    int verticalOffset = -10;

    // draw enemy units
    for (const auto & kv : getUnitData(_enemy).getUnits())
	{
        const UnitInfo & ui(kv.second);

		BWAPI::UnitType type(ui.type);
        int hitPoints = ui.lastHealth;
        int shields = ui.lastShields;

        const BWAPI::Position & pos = ui.lastPosition;

        int left    = pos.x - type.dimensionLeft();
        int right   = pos.x + type.dimensionRight();
        int top     = pos.y - type.dimensionUp();
        int bottom  = pos.y + type.dimensionDown();

        if (!BWAPI::Broodwar->isVisible(BWAPI::TilePosition(ui.lastPosition)))
        {
            BWAPI::Broodwar->drawBoxMap(BWAPI::Position(left, top), BWAPI::Position(right, bottom), BWAPI::Colors::Grey, false);
            BWAPI::Broodwar->drawTextMap(BWAPI::Position(left + 3, top + 4), "%s", ui.type.getName().c_str());
        }
        
        if (!type.isResourceContainer() && type.maxHitPoints() > 0)
        {
            double hpRatio = (double)hitPoints / (double)type.maxHitPoints();
        
            BWAPI::Color hpColor = BWAPI::Colors::Green;
            if (hpRatio < 0.66) hpColor = BWAPI::Colors::Orange;
            if (hpRatio < 0.33) hpColor = BWAPI::Colors::Red;

            int ratioRight = left + (int)((right-left) * hpRatio);
            int hpTop = top + verticalOffset;
            int hpBottom = top + 4 + verticalOffset;

            BWAPI::Broodwar->drawBoxMap(BWAPI::Position(left, hpTop), BWAPI::Position(right, hpBottom), BWAPI::Colors::Grey, true);
            BWAPI::Broodwar->drawBoxMap(BWAPI::Position(left, hpTop), BWAPI::Position(ratioRight, hpBottom), hpColor, true);
            BWAPI::Broodwar->drawBoxMap(BWAPI::Position(left, hpTop), BWAPI::Position(right, hpBottom), BWAPI::Colors::Black, false);

            int ticWidth = 3;

            for (int i(left); i < right-1; i+=ticWidth)
            {
                BWAPI::Broodwar->drawLineMap(BWAPI::Position(i, hpTop), BWAPI::Position(i, hpBottom), BWAPI::Colors::Black);
            }
        }

        if (!type.isResourceContainer() && type.maxShields() > 0)
        {
            double shieldRatio = (double)shields / (double)type.maxShields();
        
            int ratioRight = left + (int)((right-left) * shieldRatio);
            int hpTop = top - 3 + verticalOffset;
            int hpBottom = top + 1 + verticalOffset;

            BWAPI::Broodwar->drawBoxMap(BWAPI::Position(left, hpTop), BWAPI::Position(right, hpBottom), BWAPI::Colors::Grey, true);
            BWAPI::Broodwar->drawBoxMap(BWAPI::Position(left, hpTop), BWAPI::Position(ratioRight, hpBottom), BWAPI::Colors::Blue, true);
            BWAPI::Broodwar->drawBoxMap(BWAPI::Position(left, hpTop), BWAPI::Position(right, hpBottom), BWAPI::Colors::Black, false);

            int ticWidth = 3;

            for (int i(left); i < right-1; i+=ticWidth)
            {
                BWAPI::Broodwar->drawLineMap(BWAPI::Position(i, hpTop), BWAPI::Position(i, hpBottom), BWAPI::Colors::Black);
            }
        }

    }

    // draw neutral units and our units
    for (auto & unit : BWAPI::Broodwar->getAllUnits())
    {
        if (unit->getPlayer() == _enemy)
        {
            continue;
        }

        const BWAPI::Position & pos = unit->getPosition();

        int left    = pos.x - unit->getType().dimensionLeft();
        int right   = pos.x + unit->getType().dimensionRight();
        int top     = pos.y - unit->getType().dimensionUp();
        int bottom  = pos.y + unit->getType().dimensionDown();

        //BWAPI::Broodwar->drawBoxMap(BWAPI::Position(left, top), BWAPI::Position(right, bottom), BWAPI::Colors::Grey, false);

        if (!unit->getType().isResourceContainer() && unit->getType().maxHitPoints() > 0)
        {
            double hpRatio = (double)unit->getHitPoints() / (double)unit->getType().maxHitPoints();
        
            BWAPI::Color hpColor = BWAPI::Colors::Green;
            if (hpRatio < 0.66) hpColor = BWAPI::Colors::Orange;
            if (hpRatio < 0.33) hpColor = BWAPI::Colors::Red;

            int ratioRight = left + (int)((right-left) * hpRatio);
            int hpTop = top + verticalOffset;
            int hpBottom = top + 4 + verticalOffset;

            BWAPI::Broodwar->drawBoxMap(BWAPI::Position(left, hpTop), BWAPI::Position(right, hpBottom), BWAPI::Colors::Grey, true);
            BWAPI::Broodwar->drawBoxMap(BWAPI::Position(left, hpTop), BWAPI::Position(ratioRight, hpBottom), hpColor, true);
            BWAPI::Broodwar->drawBoxMap(BWAPI::Position(left, hpTop), BWAPI::Position(right, hpBottom), BWAPI::Colors::Black, false);

            int ticWidth = 3;

            for (int i(left); i < right-1; i+=ticWidth)
            {
                BWAPI::Broodwar->drawLineMap(BWAPI::Position(i, hpTop), BWAPI::Position(i, hpBottom), BWAPI::Colors::Black);
            }
        }

        if (!unit->getType().isResourceContainer() && unit->getType().maxShields() > 0)
        {
            double shieldRatio = (double)unit->getShields() / (double)unit->getType().maxShields();
        
            int ratioRight = left + (int)((right-left) * shieldRatio);
            int hpTop = top - 3 + verticalOffset;
            int hpBottom = top + 1 + verticalOffset;

            BWAPI::Broodwar->drawBoxMap(BWAPI::Position(left, hpTop), BWAPI::Position(right, hpBottom), BWAPI::Colors::Grey, true);
            BWAPI::Broodwar->drawBoxMap(BWAPI::Position(left, hpTop), BWAPI::Position(ratioRight, hpBottom), BWAPI::Colors::Blue, true);
            BWAPI::Broodwar->drawBoxMap(BWAPI::Position(left, hpTop), BWAPI::Position(right, hpBottom), BWAPI::Colors::Black, false);

            int ticWidth = 3;

            for (int i(left); i < right-1; i+=ticWidth)
            {
                BWAPI::Broodwar->drawLineMap(BWAPI::Position(i, hpTop), BWAPI::Position(i, hpBottom), BWAPI::Colors::Black);
            }
        }

        if (unit->getType().isResourceContainer() && unit->getInitialResources() > 0)
        {
            
            double mineralRatio = (double)unit->getResources() / (double)unit->getInitialResources();
        
            int ratioRight = left + (int)((right-left) * mineralRatio);
            int hpTop = top + verticalOffset;
            int hpBottom = top + 4 + verticalOffset;

            BWAPI::Broodwar->drawBoxMap(BWAPI::Position(left, hpTop), BWAPI::Position(right, hpBottom), BWAPI::Colors::Grey, true);
            BWAPI::Broodwar->drawBoxMap(BWAPI::Position(left, hpTop), BWAPI::Position(ratioRight, hpBottom), BWAPI::Colors::Cyan, true);
            BWAPI::Broodwar->drawBoxMap(BWAPI::Position(left, hpTop), BWAPI::Position(right, hpBottom), BWAPI::Colors::Black, false);

            int ticWidth = 3;

            for (int i(left); i < right-1; i+=ticWidth)
            {
                BWAPI::Broodwar->drawLineMap(BWAPI::Position(i, hpTop), BWAPI::Position(i, hpBottom), BWAPI::Colors::Black);
            }
        }
    }
}

void InformationManager::drawUnitInformation(int x, int y) 
{
	if (!Config::Debug::DrawEnemyUnitInfo)
    {
        return;
    }

	char color = white;

	BWAPI::Broodwar->drawTextScreen(x, y-10, "\x03 Self Loss:\x04 Minerals: \x1f%d \x04Gas: \x07%d", _unitData[_self].getMineralsLost(), _unitData[_self].getGasLost());
    BWAPI::Broodwar->drawTextScreen(x, y, "\x03 Enemy Loss:\x04 Minerals: \x1f%d \x04Gas: \x07%d", _unitData[_enemy].getMineralsLost(), _unitData[_enemy].getGasLost());
	BWAPI::Broodwar->drawTextScreen(x, y+10, "\x04 Enemy: %s", _enemy->getName().c_str());
	BWAPI::Broodwar->drawTextScreen(x, y+20, "\x04 UNIT NAME");
	BWAPI::Broodwar->drawTextScreen(x+140, y+20, "\x04#");
	BWAPI::Broodwar->drawTextScreen(x+160, y+20, "\x04X");

	int yspace = 0;

	// for each unit in the queue
	for (BWAPI::UnitType t : BWAPI::UnitTypes::allUnitTypes()) 
	{
		int numUnits = _unitData[_enemy].getNumUnits(t);
		int numDeadUnits = _unitData[_enemy].getNumDeadUnits(t);

		if (numUnits > 0) 
		{
			if (t.isDetector())			{ color = purple; }		
			else if (t.canAttack())		{ color = red; }		
			else if (t.isBuilding())	{ color = yellow; }
			else						{ color = white; }

			BWAPI::Broodwar->drawTextScreen(x, y+40+((yspace)*10), " %c%s", color, t.getName().c_str());
			BWAPI::Broodwar->drawTextScreen(x+140, y+40+((yspace)*10), "%c%d", color, numUnits);
			BWAPI::Broodwar->drawTextScreen(x+160, y+40+((yspace++)*10), "%c%d", color, numDeadUnits);
		}
	}
}

void InformationManager::drawMapInformation()
{
    if (!Config::Debug::DrawBWTAInfo)
    {
        return;
    }

	// iterate through all the base locations, and draw their outlines.
	for (std::set<BWTA::BaseLocation*>::const_iterator i = BWTA::getBaseLocations().begin(); i != BWTA::getBaseLocations().end(); i++)
	{
		BWAPI::TilePosition p = (*i)->getTilePosition();
		BWAPI::Position c = (*i)->getPosition();

		//draw outline of center location
		BWAPI::Broodwar->drawBoxMap(p.x * 32, p.y * 32, p.x * 32 + 4 * 32, p.y * 32 + 3 * 32, BWAPI::Colors::Blue);

		//draw a circle at each mineral patch
		for (BWAPI::Unitset::iterator j = (*i)->getStaticMinerals().begin(); j != (*i)->getStaticMinerals().end(); j++)
		{
			BWAPI::Position q = (*j)->getInitialPosition();
			BWAPI::Broodwar->drawCircleMap(q.x, q.y, 30, BWAPI::Colors::Cyan);
		}

		//draw the outlines of vespene geysers
		for (BWAPI::Unitset::iterator j = (*i)->getGeysers().begin(); j != (*i)->getGeysers().end(); j++)
		{
			BWAPI::TilePosition q = (*j)->getTilePosition();
			BWAPI::Broodwar->drawBoxMap(q.x * 32, q.y * 32, q.x * 32 + 4 * 32, q.y * 32 + 2 * 32, BWAPI::Colors::Orange);
		}

		//if this is an island expansion, draw a yellow circle around the base location
		if ((*i)->isIsland())
			BWAPI::Broodwar->drawCircleMap(c, 80, BWAPI::Colors::Yellow);
	}

	//we will iterate through all the regions and draw the polygon outline of it in green.
	for (std::set<BWTA::Region*>::const_iterator r = BWTA::getRegions().begin(); r != BWTA::getRegions().end(); r++)
	{
		BWTA::Polygon p = (*r)->getPolygon();
		for (int j = 0; j<(int)p.size(); j++)
		{
			BWAPI::Position point1 = p[j];
			BWAPI::Position point2 = p[(j + 1) % p.size()];
			BWAPI::Broodwar->drawLineMap(point1, point2, BWAPI::Colors::Green);
		}
	}

	//we will visualize the chokepoints with red lines
	for (std::set<BWTA::Region*>::const_iterator r = BWTA::getRegions().begin(); r != BWTA::getRegions().end(); r++)
	{
		for (std::set<BWTA::Chokepoint*>::const_iterator c = (*r)->getChokepoints().begin(); c != (*r)->getChokepoints().end(); c++)
		{
			BWAPI::Position point1 = (*c)->getSides().first;
			BWAPI::Position point2 = (*c)->getSides().second;
			BWAPI::Broodwar->drawLineMap(point1, point2, BWAPI::Colors::Red);
		}
	}
}

void InformationManager::drawBaseInformation(int x, int y)
{
	if (!Config::Debug::DrawBaseInfo)
	{
		return;
	}

	int yy = y;

	BWAPI::Broodwar->drawTextScreen(x, yy, "%cBases", white);

	for (auto * base : BWTA::getBaseLocations())
	{
		yy += 10;

		char color = gray;

		char reservedChar = ' ';
		if (_theBases[base]->reserved)
		{
			reservedChar = '*';
		}

		char inferredChar = ' ';
		BWAPI::Player player = _theBases[base]->owner;
		if (player == _self)
		{
			color = green;
		}
		else if (player == _enemy)
		{
			color = orange;
			if (_theBases[base]->resourceDepot == nullptr)
			{
				inferredChar = '?';
			}
		}

		char baseCode = ' ';
		if (base == getMyMainBaseLocation())
		{
			baseCode = 'M';
		}
		else if (base == _naturalBaseLocations[_self])
		{
			baseCode = 'N';
		}

		BWAPI::TilePosition pos = base->getTilePosition();
		BWAPI::Broodwar->drawTextScreen(x-8, yy, "%c%c", white, reservedChar);
		BWAPI::Broodwar->drawTextScreen(x, yy, "%c%d, %d%c%c", color, pos.x, pos.y, inferredChar, baseCode);
	}
}

void InformationManager::updateUnit(BWAPI::Unit unit)
{
    if (unit->getPlayer() == _self || unit->getPlayer() == _enemy)
    {
		_unitData[unit->getPlayer()].updateUnit(unit);
	}
}

bool InformationManager::isValidUnit(BWAPI::Unit unit) 
{
	// we only care about our units and enemy units
	if (unit->getPlayer() != _self && unit->getPlayer() != _enemy) 
	{
		return false;
	}

	// if it's a weird unit, don't bother
	if (unit->getType() == BWAPI::UnitTypes::None || unit->getType() == BWAPI::UnitTypes::Unknown ||
		unit->getType() == BWAPI::UnitTypes::Zerg_Larva || unit->getType() == BWAPI::UnitTypes::Zerg_Egg) 
	{
		return false;
	}

	// if the position isn't valid throw it out
	if (!unit->getPosition().isValid()) 
	{
		return false;	
	}

	return true;
}

void InformationManager::onUnitDestroy(BWAPI::Unit unit) 
{ 
	if (unit->getPlayer() == _self || unit->getPlayer() == _enemy)
	{
		_unitData[unit->getPlayer()].removeUnit(unit);

		// If it may be a base, remove that base.
		if (unit->getType().isResourceDepot())
		{
			baseLost(unit->getTilePosition());
		}
	}
}

// Only returns units believed to be completed.
void InformationManager::getNearbyForce(std::vector<UnitInfo> & unitInfo, BWAPI::Position p, BWAPI::Player player, int radius) 
{
	// for each unit we know about for that player
	for (const auto & kv : getUnitData(player).getUnits())
	{
		const UnitInfo & ui(kv.second);

		// if it's a combat unit we care about
		// and it's finished! 
		if (UnitUtil::IsCombatSimUnit(ui.type) && ui.completed)
		{
			// Determine its attack range, plus a small fudge factor.
			// TODO find range using the same method as UnitUtil
			int range = 0;
			if (ui.type.groundWeapon() != BWAPI::WeaponTypes::None)
			{
				range = ui.type.groundWeapon().maxRange() + 40;
			}
			// NOTE Ignores air weapon! Units with air attack only must be inside the radius to be
			// included (except turrets and spores, because they are also detectors).

			// if it can attack into the radius we care about
			if (ui.lastPosition.getDistance(p) <= (radius + range))
			{
				// add it to the vector
				unitInfo.push_back(ui);
			}
		}
		else if (ui.type.isDetector() && ui.lastPosition.getDistance(p) <= (radius + 250))
        {
			unitInfo.push_back(ui);
        }
	}
}

int InformationManager::getNumSelfUnits(BWAPI::UnitType type) const
{
	return _numSelfUnits[type.getID()];
}

int InformationManager::getNumEnemyUnits(BWAPI::UnitType type) const
{
	return _numEnemyUnits[type.getID()];
}

int InformationManager::getNumSelfCompletedUnits(BWAPI::UnitType type) const
{
	return _numSelfCompletedUnits[type];
}

const UnitData & InformationManager::getUnitData(BWAPI::Player player) const
{
    return _unitData.find(player)->second;
}

bool InformationManager::isBaseReserved(BWTA::BaseLocation * base)
{
	return _theBases[base]->reserved;
}

void InformationManager::reserveBase(BWTA::BaseLocation * base)
{
	_theBases[base]->reserved = true;
}

void InformationManager::unreserveBase(BWTA::BaseLocation * base)
{
	_theBases[base]->reserved = false;
}

void InformationManager::unreserveBase(BWAPI::TilePosition baseTilePosition)
{
	for (BWTA::BaseLocation * base : BWTA::getBaseLocations())
	{
		if (closeEnough(base->getTilePosition(), baseTilePosition))
		{
			_theBases[base]->reserved = false;
			return;
		}
	}

	UAB_ASSERT(false,"trying to unreserve a non-base");
}

// We have complated combat units (excluding workers).
// This is a latch, initially false and set true forever when we get our first combat units.
bool InformationManager::weHaveCombatUnits()
{
	// Latch: Once we have combat units, pretend we always have them.
	if (_weHaveCombatUnits)
	{
		return true;
	}

	for (const auto u : _self->getUnits())
	{
		if (!u->getType().isWorker() &&
			!u->getType().isBuilding() &&
			u->isCompleted() &&
			u->getType() != BWAPI::UnitTypes::Zerg_Larva &&
			u->getType() != BWAPI::UnitTypes::Zerg_Overlord)
		{
			_weHaveCombatUnits = true;
			return true;
		}
	}

	return false;
}

// Enemy has mobile units that can shoot up, or the tech to produce them.
bool InformationManager::enemyHasAntiAir()
{
	// Latch: Once they're known to have the tech, they always have it.
	if (_enemyHasAntiAir)
	{
		return true;
	}

	for (const auto & kv : getUnitData(_enemy).getUnits())
	{
		const UnitInfo & ui(kv.second);

		if (
			// For terran, anything other than SCV, command center, depot is a hit.
			// Surely nobody makes ebay before barracks!
			(_enemy->getRace() == BWAPI::Races::Terran &&
			ui.type != BWAPI::UnitTypes::Terran_SCV &&
			ui.type != BWAPI::UnitTypes::Terran_Command_Center &&
			ui.type != BWAPI::UnitTypes::Terran_Supply_Depot)

			||

			// Otherwise, any mobile unit that has an air weapon.
			(!ui.type.isBuilding() && ui.type.airWeapon() != BWAPI::WeaponTypes::None)

			||

			// Or a building for making such a unit.
			ui.type == BWAPI::UnitTypes::Protoss_Cybernetics_Core ||
			ui.type == BWAPI::UnitTypes::Protoss_Stargate ||
			ui.type == BWAPI::UnitTypes::Protoss_Fleet_Beacon ||
			ui.type == BWAPI::UnitTypes::Protoss_Arbiter_Tribunal ||
			ui.type == BWAPI::UnitTypes::Zerg_Hydralisk_Den ||
			ui.type == BWAPI::UnitTypes::Zerg_Spire ||
			ui.type == BWAPI::UnitTypes::Zerg_Greater_Spire

			)
		{
			_enemyHasAntiAir = true;
			return true;
		}
	}

	return false;
}

// Enemy has air units or air-producing tech.
// Overlords and lifted buildings are excluded.
// A queen's nest is not air tech--it's usually a prerequisite for hive
// rather than to make queens. So we have to see a queen for it to count.
// Protoss robo fac and terran starport are taken to imply air units.
bool InformationManager::enemyHasAirTech()
{
	// Latch: Once they're known to have the tech, they always have it.
	if (_enemyHasAirTech)
	{
		return true;
	}

	for (const auto & kv : getUnitData(_enemy).getUnits())
	{
		const UnitInfo & ui(kv.second);

		if ((ui.type.isFlyer() && ui.type != BWAPI::UnitTypes::Zerg_Overlord) ||
			ui.type == BWAPI::UnitTypes::Terran_Starport ||
			ui.type == BWAPI::UnitTypes::Terran_Control_Tower ||
			ui.type == BWAPI::UnitTypes::Terran_Science_Facility ||
			ui.type == BWAPI::UnitTypes::Terran_Covert_Ops ||
			ui.type == BWAPI::UnitTypes::Terran_Physics_Lab ||
			ui.type == BWAPI::UnitTypes::Protoss_Stargate ||
			ui.type == BWAPI::UnitTypes::Protoss_Arbiter_Tribunal ||
			ui.type == BWAPI::UnitTypes::Protoss_Fleet_Beacon ||
			ui.type == BWAPI::UnitTypes::Protoss_Robotics_Facility ||
			ui.type == BWAPI::UnitTypes::Protoss_Robotics_Support_Bay ||
			ui.type == BWAPI::UnitTypes::Protoss_Observatory ||
			ui.type == BWAPI::UnitTypes::Zerg_Spire ||
			ui.type == BWAPI::UnitTypes::Zerg_Greater_Spire)
		{
			_enemyHasAirTech = true;
			return true;
		}
	}

	return false;
}

// This test is good for "can I benefit from detection?"
bool InformationManager::enemyHasCloakTech()
{
	// Latch: Once they're known to have the tech, they always have it.
	if (_enemyHasCloakTech)
	{
		return true;
	}

	for (const auto & kv : getUnitData(_enemy).getUnits())
	{
		const UnitInfo & ui(kv.second);

		if (ui.type.hasPermanentCloak() ||                             // DT, observer
			ui.type.isCloakable() ||                                   // wraith, ghost
			ui.type == BWAPI::UnitTypes::Terran_Vulture_Spider_Mine ||
			ui.type == BWAPI::UnitTypes::Protoss_Citadel_of_Adun ||    // assume DT
			ui.type == BWAPI::UnitTypes::Protoss_Templar_Archives ||   // assume DT
			ui.type == BWAPI::UnitTypes::Protoss_Observatory ||
			ui.type == BWAPI::UnitTypes::Protoss_Arbiter_Tribunal ||
			ui.type == BWAPI::UnitTypes::Protoss_Arbiter ||
			ui.type == BWAPI::UnitTypes::Zerg_Lurker ||
			ui.type == BWAPI::UnitTypes::Zerg_Lurker_Egg ||
			ui.unit->isBurrowed())
		{
			_enemyHasCloakTech = true;
			return true;
		}
	}

	return false;
}

// This test is better for "do I need detection to live?"
// It doesn't worry about spider mines, observers, or burrowed units except lurkers.
bool InformationManager::enemyHasMobileCloakTech()
{
	// Latch: Once they're known to have the tech, they always have it.
	if (_enemyHasMobileCloakTech)
	{
		return true;
	}

	for (const auto & kv : getUnitData(_enemy).getUnits())
	{
		const UnitInfo & ui(kv.second);

		if (ui.type.isCloakable() ||                                   // wraith, ghost
			ui.type == BWAPI::UnitTypes::Protoss_Dark_Templar ||
			ui.type == BWAPI::UnitTypes::Protoss_Citadel_of_Adun ||    // assume DT
			ui.type == BWAPI::UnitTypes::Protoss_Templar_Archives ||   // assume DT
			ui.type == BWAPI::UnitTypes::Protoss_Arbiter_Tribunal ||
			ui.type == BWAPI::UnitTypes::Protoss_Arbiter ||
			ui.type == BWAPI::UnitTypes::Zerg_Lurker ||
			ui.type == BWAPI::UnitTypes::Zerg_Lurker_Egg)
		{
			_enemyHasMobileCloakTech = true;
			return true;
		}
	}

	return false;
}

// Enemy has air units good for hunting down overlords.
// A stargate counts, but not a fleet beacon or arbiter tribunal.
// A starport does not count; it may well be for something else.
bool InformationManager::enemyHasOverlordHunters()
{
	// Latch: Once they're known to have the tech, they always have it.
	if (_enemyHasOverlordHunters)
	{
		return true;
	}

	for (const auto & kv : getUnitData(_enemy).getUnits())
	{
		const UnitInfo & ui(kv.second);

		if (ui.type == BWAPI::UnitTypes::Terran_Wraith ||
			ui.type == BWAPI::UnitTypes::Terran_Valkyrie ||
			ui.type == BWAPI::UnitTypes::Terran_Battlecruiser ||
			ui.type == BWAPI::UnitTypes::Protoss_Corsair ||
			ui.type == BWAPI::UnitTypes::Protoss_Scout ||
			ui.type == BWAPI::UnitTypes::Protoss_Carrier ||
			ui.type == BWAPI::UnitTypes::Protoss_Stargate ||
			ui.type == BWAPI::UnitTypes::Zerg_Spire ||
			ui.type == BWAPI::UnitTypes::Zerg_Greater_Spire ||
			ui.type == BWAPI::UnitTypes::Zerg_Mutalisk ||
			ui.type == BWAPI::UnitTypes::Zerg_Scourge)
		{
			_enemyHasOverlordHunters = true;
			return true;
		}
	}

	return false;
}

// Enemy has spore colonies, photon cannons, turrets, or spider mines.
// Spider mines only catch cloaked ground units, so this routine is not ideal for countering wraiths.
bool InformationManager::enemyHasStaticDetection()
{
	// Latch: Once they're known to have the tech, they always have it.
	if (_enemyHasStaticDetection)
	{
		return true;
	}

	for (const auto & kv : getUnitData(_enemy).getUnits())
	{
		const UnitInfo & ui(kv.second);

		if (ui.type == BWAPI::UnitTypes::Terran_Missile_Turret ||
			ui.type == BWAPI::UnitTypes::Terran_Vulture_Spider_Mine ||
			ui.type == BWAPI::UnitTypes::Protoss_Photon_Cannon ||
			ui.type == BWAPI::UnitTypes::Zerg_Spore_Colony)
		{
			_enemyHasStaticDetection = true;
			return true;
		}
	}

	return false;
}

// Enemy has overlords, observers, comsat, or science vessels.
bool InformationManager::enemyHasMobileDetection()
{
	// Latch: Once they're known to have the tech, they always have it.
	if (_enemyHasMobileDetection)
	{
		return true;
	}

	// If the enemy is zerg, they have overlords.
	// If they went random, we may not have known until now.
	if (_enemy->getRace() == BWAPI::Races::Zerg)
	{
		_enemyHasMobileDetection = true;
		return true;
	}

	for (const auto & kv : getUnitData(_enemy).getUnits())
	{
		const UnitInfo & ui(kv.second);

		if (ui.type == BWAPI::UnitTypes::Terran_Comsat_Station ||
			ui.type == BWAPI::UnitTypes::Terran_Science_Facility ||
			ui.type == BWAPI::UnitTypes::Terran_Science_Vessel ||
			ui.type == BWAPI::UnitTypes::Protoss_Observatory ||
			ui.type == BWAPI::UnitTypes::Protoss_Observer)
		{
			_enemyHasMobileDetection = true;
			return true;
		}
	}

	return false;
}

// Zerg specific calculation: How many scourge hits are needed
// to kill the enemy's known air fleet?
// This counts individual units--you get 2 scourge per egg.
// One scourge does 110 normal damage.
// NOTE: This ignores air armor, which might make a difference in rare cases.
int InformationManager::nScourgeNeeded()
{
	int count = 0;

	for (const auto & kv : getUnitData(_enemy).getUnits())
	{
		const UnitInfo & ui(kv.second);

		// A few unit types should not usually be scourged. Skip them.
		if (ui.type.isFlyer() &&
			ui.type != BWAPI::UnitTypes::Zerg_Overlord &&
			ui.type != BWAPI::UnitTypes::Zerg_Scourge &&
			ui.type != BWAPI::UnitTypes::Protoss_Interceptor)
		{
			int hp = ui.type.maxHitPoints() + ui.type.maxShields();      // assume the worst
			count += (hp + 109) / 110;
		}
	}

	return count;
}

InformationManager & InformationManager::Instance()
{
	static InformationManager instance;
	return instance;
}
