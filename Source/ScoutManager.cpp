#include "ScoutManager.h"
#include "ProductionManager.h"

using namespace XBot;

ScoutManager::ScoutManager() 
    : _workerScout(nullptr)
	, _scoutStatus("None")
	, _gasStealStatus("None")
	, _scoutLocationOnly(false)
    , _scoutUnderAttack(false)
	, _tryGasSteal(false)
    , _didGasSteal(false)
    , _gasStealFinished(false)
    , _currentRegionVertexIndex(-1)
    , _previousScoutHP(0)
{
}

ScoutManager & ScoutManager::Instance() 
{
	static ScoutManager instance;
	return instance;
}

void ScoutManager::update()
{
	for (const auto & pos : _enemyRegionVertices)
	{
		BWAPI::Broodwar->drawCircleMap(pos, 10, BWAPI::Colors::Cyan, true);
	}

	// If we're not scouting now, minimum effort.
	if (!_workerScout)
	{
		return;
	}

	// If the worker scout is gone, admit it.
	if (!_workerScout->exists() || _workerScout->getHitPoints() <= 0 ||   // it died
		_workerScout->getPlayer() != BWAPI::Broodwar->self())             // it got mind controlled!
	{
		_workerScout = nullptr;
		return;
	}

	// If we only want to locate the enemy base and we have, release the scout worker.
	if ((_scoutLocationOnly
		&& InformationManager::Instance().getEnemyMainBaseLocation()
		&& BWAPI::Broodwar->isExplored(InformationManager::Instance().getEnemyMainBaseLocation()->getTilePosition()))
		|| Config::Scout::ScoutRound == 0)
	{
		releaseWorkerScout();
		return;
	}

    // calculate enemy region vertices if we haven't yet
    if (_enemyRegionVertices.empty())
    {
        calculateEnemyRegionVertices();
    }

	moveScout();
    drawScoutInformation(200, 320);
}

void ScoutManager::setWorkerScout(BWAPI::Unit unit)
{
    // if we have a previous worker scout, release it back to the worker manager
	releaseWorkerScout();

    _workerScout = unit;
    WorkerManager::Instance().setScoutWorker(_workerScout);
}

// Send the scout home.
void ScoutManager::releaseWorkerScout()
{
	if (_workerScout)
	{
		WorkerManager::Instance().finishedWithWorker(_workerScout);
		_workerScout = nullptr;
	}
}

void ScoutManager::setGasSteal()
{
	_tryGasSteal = true;
}

void ScoutManager::setScoutLocationOnly()
{
	_scoutLocationOnly = true;
}

void ScoutManager::drawScoutInformation(int x, int y)
{
    if (!Config::Debug::DrawScoutInfo)
    {
        return;
    }

    BWAPI::Broodwar->drawTextScreen(x, y, "ScoutInfo: %s", _scoutStatus.c_str());
    BWAPI::Broodwar->drawTextScreen(x, y+10, "GasSteal: %s", _gasStealStatus.c_str());
    for (size_t i(0); i < _enemyRegionVertices.size(); ++i)
    {
        BWAPI::Broodwar->drawCircleMap(_enemyRegionVertices[i], 4, BWAPI::Colors::Green, false);
        BWAPI::Broodwar->drawTextMap(_enemyRegionVertices[i], "%d", i);
    }
}

void ScoutManager::moveScout()
{
    int scoutHP = _workerScout->getHitPoints() + _workerScout->getShields();
    
    gasSteal();

	// get the enemy base location, if we have one
	// Note: In case of an enemy proxy or weird map, this might be our own base. Roll with it.
	BWTA::BaseLocation * enemyBaseLocation = InformationManager::Instance().getEnemyMainBaseLocation();

    int scoutDistanceThreshold = 30;

    // if we initiated a gas steal and the worker isn't idle, 
    bool finishedConstructingGasSteal = _workerScout->isIdle() || _workerScout->isCarryingGas();
    if (!_gasStealFinished && _didGasSteal && !finishedConstructingGasSteal)
    {
        return;
    }
    // check to see if the gas steal is completed
    else if (_didGasSteal && finishedConstructingGasSteal)
    {
        _gasStealFinished = true;
    }
    
	// if we know where the enemy region is and where our scout is
	if (_workerScout && enemyBaseLocation)
	{
        int scoutDistanceToEnemy = MapTools::Instance().getGroundTileDistance(_workerScout->getPosition(), enemyBaseLocation->getPosition());
        bool scoutInRangeOfenemy = scoutDistanceToEnemy <= scoutDistanceThreshold;
        
        // we only care if the scout is under attack within the enemy region
        // this ignores if their scout worker attacks it on the way to their base
        if (scoutHP < _previousScoutHP)
        {
	        _scoutUnderAttack = true;
        }

        if (!_workerScout->isUnderAttack() && !enemyWorkerInRadius())
        {
	        _scoutUnderAttack = false;
        }

		// if the scout is in the enemy region
		if (scoutInRangeOfenemy)
		{
			if (_scoutUnderAttack)
			{
				_scoutStatus = "Under attack, fleeing";
				followPerimeter();
			}
			else
			{
				BWAPI::Unit closestWorker = enemyWorkerToHarass();

				// If configured and reasonable, harass an enemy worker.
				if (Config::Strategy::ScoutHarassEnemy && closestWorker && (!_tryGasSteal || _gasStealFinished))
				{
                    _scoutStatus = "Harass enemy worker";
                    _currentRegionVertexIndex = -1;
					Micro::SmartAttackUnit(_workerScout, closestWorker);
				}
				// otherwise keep circling the enemy region
				else
				{
                    _scoutStatus = "Following perimeter";
                    followPerimeter();  
                }
			}
		}
		// if the scout is not in the enemy region
		else if (_scoutUnderAttack)
		{
            _scoutStatus = "Under attack, fleeing";

            followPerimeter();
		}
		else
		{
            _scoutStatus = "Enemy region known, going there";

			// move to the enemy region
			followPerimeter();
        }
		
	}

	// for each start location in the level
	if (!enemyBaseLocation)
	{
		_scoutStatus = "Enemy base unknown, exploring";

		//ÄæÊ±ÕëÌ½Â·
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
			//½µÐòÁÐ->ÄæÊ±Õë
			return theta1 > theta2;
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
						Micro::SmartMove(_workerScout, BWAPI::Position(x * 32, y * 32));
						return;
					}
				}
			}
		}
	}

    _previousScoutHP = scoutHP;
}

void ScoutManager::followPerimeter()
{
    BWAPI::Position fleeTo = getFleePosition();

    if (Config::Debug::DrawScoutInfo)
    {
        BWAPI::Broodwar->drawCircleMap(fleeTo, 5, BWAPI::Colors::Red, true);
    }

	Micro::SmartMove(_workerScout, fleeTo);
}

void ScoutManager::gasSteal()
{
    if (!_tryGasSteal)
    {
        _gasStealStatus = "Not using gas steal";
        return;
    }

    if (_didGasSteal)
    {
        return;
    }

    if (!_workerScout)
    {
        _gasStealStatus = "No worker scout";
        return;
    }

    BWTA::BaseLocation * enemyBaseLocation = InformationManager::Instance().getEnemyMainBaseLocation();
    if (!enemyBaseLocation)
    {
        _gasStealStatus = "No enemy base location found";
        return;
    }

    BWAPI::Unit enemyGeyser = getEnemyGeyser();
    if (!enemyGeyser)
    {
        _gasStealStatus = "Not exactly 1 enemy geyser";
        return;
    }

    if (!_didGasSteal)
    {
        ProductionManager::Instance().queueGasSteal();
        _didGasSteal = true;
        Micro::SmartMove(_workerScout, enemyGeyser->getPosition());
        _gasStealStatus = "Stealing gas";
    }
}

// Choose an enemy worker to harass, or none.
BWAPI::Unit ScoutManager::enemyWorkerToHarass() const
{
	// First look for any enemy worker that is building.
	for (const auto unit : BWAPI::Broodwar->enemy()->getUnits())
	{
		if (unit->getType().isWorker() && unit->isConstructing())
		{
			return unit;
		}
	}

	BWAPI::Unit enemyWorker = nullptr;
	int maxDist = 600;    // ignore any beyond this range

	BWAPI::Unit geyser = getEnemyGeyser();

	// Failing that, find the enemy worker closest to the gas.
	for (const auto unit : BWAPI::Broodwar->enemy()->getUnits())
	{
		if (unit->getType().isWorker())
		{
			int dist = unit->getDistance(geyser);

			if (dist < maxDist)
			{
				maxDist = dist;
				enemyWorker = unit;
			}
		}
	}

	return enemyWorker;
}

// If there is exactly 1 geyser in the enemy base, return it.
// If there's 0 we can't steal it, and if >1 then it's no use to steal it.
BWAPI::Unit ScoutManager::getEnemyGeyser() const
{
	BWTA::BaseLocation * enemyBaseLocation = InformationManager::Instance().getEnemyMainBaseLocation();

	BWAPI::Unitset geysers = enemyBaseLocation->getGeysers();
	if (geysers.size() == 1)
	{
		return *(geysers.begin());
	}

	return nullptr;
}

bool ScoutManager::enemyWorkerInRadius()
{
	for (const auto unit : BWAPI::Broodwar->enemy()->getUnits())
	{
		if (unit->getType().isWorker() && (unit->getDistance(_workerScout) < 300))
		{
			return true;
		}
	}

	return false;
}

int ScoutManager::getClosestVertexIndex(BWAPI::Unit unit)
{
    int closestIndex = -1;
    int closestDistance = 10000000;

    for (size_t i(0); i < _enemyRegionVertices.size(); ++i)
    {
        int dist = unit->getDistance(_enemyRegionVertices[i]);
        if (dist < closestDistance)
        {
            closestDistance = dist;
            closestIndex = i;
        }
    }

    return closestIndex;
}

BWAPI::Position ScoutManager::getFleePosition()
{
    UAB_ASSERT_WARNING(!_enemyRegionVertices.empty(), "should have enemy region vertices");
    
    BWTA::BaseLocation * enemyBaseLocation = InformationManager::Instance().getEnemyMainBaseLocation();

    // if this is the first flee, we will not have a previous perimeter index
    if (_currentRegionVertexIndex == -1)
    {
        // so return the closest position in the polygon
        int closestPolygonIndex = getClosestVertexIndex(_workerScout);

        UAB_ASSERT_WARNING(closestPolygonIndex != -1, "Couldn't find a closest vertex");

        if (closestPolygonIndex == -1)
        {
            return BWAPI::Position(BWAPI::Broodwar->self()->getStartLocation());
        }

        // set the current index so we know how to iterate if we are still fleeing later
        _currentRegionVertexIndex = closestPolygonIndex;
        return _enemyRegionVertices[closestPolygonIndex];
    }

    // if we are still fleeing from the previous frame, get the next location if we are close enough
    double distanceFromCurrentVertex = _enemyRegionVertices[_currentRegionVertexIndex].getDistance(_workerScout->getPosition());

    // keep going to the next vertex in the perimeter until we get to one we're far enough from to issue another move command
    while (distanceFromCurrentVertex < 128)
    {
		if (Config::Scout::ScoutRound <= 0)
		{
			return BWAPI::Position(BWAPI::Broodwar->self()->getStartLocation());
		}
		_currentRegionVertexIndex = (_currentRegionVertexIndex + 1) % _enemyRegionVertices.size();

		if (_currentRegionVertexIndex == 0)
		{
			--Config::Scout::ScoutRound;
		}

        distanceFromCurrentVertex = _enemyRegionVertices[_currentRegionVertexIndex].getDistance(_workerScout->getPosition());
    }

    return _enemyRegionVertices[_currentRegionVertexIndex];
}

void ScoutManager::calculateEnemyRegionVertices()
{
    BWTA::BaseLocation * enemyBaseLocation = InformationManager::Instance().getEnemyMainBaseLocation();

    if (!enemyBaseLocation)
    {
        return;
    }

    BWTA::Region * enemyRegion = enemyBaseLocation->getRegion();

    if (!enemyRegion)
    {
        return;
    }
	const auto & enemyArea = InformationManager::Instance().getTileArea(enemyBaseLocation->getTilePosition());
	if (!enemyArea)
	{
		return;
	}
	const BWEM::Base * enemyBase = nullptr;
	for (const auto & base : enemyArea->Bases())
	{
		if (base.Location().getDistance(enemyBaseLocation->getTilePosition()) < 5)
		{
			enemyBase = &base;
			break;
		}
	}
	if (!enemyBase)
	{
		return;
	}

	const auto baseCenterPos = enemyBaseLocation->getPosition();
	int mineralX = 0, mineralY = 0, mineralCount = 0;
	for (const auto & mineral : enemyBase->Minerals())
	{
		double dist = std::sqrt(std::pow(mineral->Pos().x - baseCenterPos.x, 2) + std::pow(mineral->Pos().y - baseCenterPos.y, 2));
		if (dist < 320)
		{
			++mineralCount;
			mineralX += mineral->Pos().x;
			mineralY += mineral->Pos().y;
		}
	}
	auto mineralCenterPos = baseCenterPos;
	if (mineralCount > 0)
	{
		mineralCenterPos = BWAPI::Position(mineralX / mineralCount, mineralY / mineralCount);
	}
	auto geyserCenterPos = BWAPI::Position(enemyBase->Geysers().front()->TopLeft());
	for (const auto & geyser : enemyBase->Geysers())
	{
		if (geyser->Pos().getDistance(baseCenterPos) < 320)
		{
			geyserCenterPos = BWAPI::Position(geyser->TopLeft());
			break;
		}
	}
	const auto patrolCenterPos = BWAPI::Position(2.5*baseCenterPos.x - 1.5*mineralCenterPos.x, 2.5*baseCenterPos.y - 1.5*mineralCenterPos.y);
	const auto P0 = geyserCenterPos + patrolCenterPos - baseCenterPos;
	const auto geyserNearPos = (P0 + geyserCenterPos) / 2;
	const auto geyserMirrorPos = baseCenterPos * 2 - geyserCenterPos;
	const auto geyserNearMirrorPos = (baseCenterPos + patrolCenterPos - geyserNearPos);

	_enemyRegionVertices.clear();

	_enemyRegionVertices.push_back(patrolCenterPos);
	_enemyRegionVertices.push_back(geyserNearMirrorPos);
	_enemyRegionVertices.push_back(geyserMirrorPos);
	_enemyRegionVertices.push_back(geyserNearPos);
	_enemyRegionVertices.push_back(patrolCenterPos);
}