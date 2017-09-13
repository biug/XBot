#include "MicroDetectors.h"
#include "StateManager.h"

using namespace XBot;

MicroDetectors::MicroDetectors()
	: unitClosestToEnemy(nullptr)
{
}

void MicroDetectors::executeMicro(const BWAPI::Unitset & targets) 
{
	const BWAPI::Unitset & detectorUnits = getUnits();

	if (detectorUnits.empty())
	{
		return;
	}

	// NOTE targets is a list of nearby enemies.
	// Currently unused. Could use it to avoid enemy fire, among other possibilities.
	for (size_t i(0); i<targets.size(); ++i)
	{
		// do something here if there are targets
	}

	cloakedUnitMap.clear();
	BWAPI::Unitset cloakedUnits;

	// Find enemy cloaked units.
	// NOTE This code is unused, but it is potentially useful.
	for (const auto unit : BWAPI::Broodwar->enemy()->getUnits())
	{
		if (unit->getType().hasPermanentCloak() ||     // dark templar, observer
			unit->getType().isCloakable() ||           // wraith, ghost
			unit->getType() == BWAPI::UnitTypes::Terran_Vulture_Spider_Mine ||
			unit->getType() == BWAPI::UnitTypes::Zerg_Lurker ||
			unit->isBurrowed() ||
			(unit->isVisible() && !unit->isDetected()))
		{
			cloakedUnits.insert(unit);
			cloakedUnitMap[unit] = false;
		}
	}

	auto & state = StateManager::Instance();
	const auto & visit_target = state.flyer_visit_position;

	for (const auto detectorUnit : detectorUnits)
	{
		if (state.keep_build_sunken)
		{
			BWAPI::Broodwar->drawTextScreen(200, 190, "%c move detector 1", cyan);
			auto & info = InformationManager::Instance();
			if (info.getEnemyMainBaseLocation() && info.getMyMainBaseLocation())
			{
				auto myArea = info.getTileArea(info.getMyMainBaseLocation()->getTilePosition());
				auto enemyArea = info.getTileArea(info.getEnemyMainBaseLocation()->getTilePosition());
				auto path = BWEM::Map::Instance().GetPath(myArea, enemyArea);
				if (!path.empty())
				{
					BWAPI::Broodwar->drawTextScreen(200, 200, "%c move detector 2", cyan);
					Micro::SmartMove(detectorUnit, BWAPI::Position(path.front()->Center()));
				}
			}
			continue;
		}
		if (!unitClosestToEnemy || !unitClosestToEnemy->getPosition().isValid()) continue;

		// for anti cannon bot
		if (!visit_target.empty())
		{
			if (state.flyer_visit.find(detectorUnit) == state.flyer_visit.end())
			{
				state.flyer_visit[detectorUnit] = std::vector<bool>(visit_target.size(), false);
			}
			int visit_index = 0;
			auto & visit_pos = state.flyer_visit[detectorUnit];
			while (visit_index < visit_pos.size())
			{
				if (!visit_pos[visit_index]) break;
				++visit_index;
			}
			// if not visit all target
			if (visit_index < visit_pos.size())
			{
				// should visit all target
				if (detectorUnit->getDistance(visit_target[visit_index]) < 96)
				{
					visit_pos[visit_index] = true;
				}
				else
				{
					Micro::SmartMove(detectorUnit, visit_target[visit_index]);
				}
				continue;
			}
		}

		BWAPI::Position detectorPos = unitClosestToEnemy->getPosition();
		if (StateManager::Instance().being_rushed)
		{
			const auto & myMainBase = InformationManager::Instance().getMyMainBaseLocation();
			const auto & enemyMainBase = InformationManager::Instance().getEnemyMainBaseLocation();
			if (myMainBase && enemyMainBase)
			{
				const auto & area1 = InformationManager::Instance().getTileArea(myMainBase->getTilePosition());
				const auto & area2 = InformationManager::Instance().getTileArea(enemyMainBase->getTilePosition());
				if (area1 && area2)
				{
					const auto & chokes = BWEM::Map::Instance().GetPath(area1, area2);
					if (!chokes.empty())
					{
						const auto & chokePos = BWAPI::Position(chokes[0]->Center());
						BWAPI::Unit sunken = nullptr;
						for (const auto & unit : BWAPI::Broodwar->self()->getUnits())
						{
							if (unit && unit->exists() && unit->getType() == BWAPI::UnitTypes::Zerg_Sunken_Colony)
							{
								if (!sunken || sunken->getDistance(chokePos) > unit->getDistance(chokePos))
								{
									sunken = unit;
								}
							}
						}
						if (sunken)
						{
							const auto & sunkenPos = sunken->getPosition();
							detectorPos = BWAPI::Position(sunkenPos.x, sunkenPos.y + (sunkenPos.y > chokePos.y ? -6 : 6));
							if (sunkenPos.x != chokePos.x)
							{
								double range = BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Protoss ? 32.0 * 9.0 : 32.0 * 4.0;
								double k = std::abs((double)(sunkenPos.y - chokePos.y) / (double)(sunkenPos.x - chokePos.x));
								detectorPos.x = sunkenPos.x + (sunkenPos.x > chokePos.x ? -1.0 : 1.0) / sqrt(1.0 + k * k) * range;
								detectorPos.y = sunkenPos.y + (sunkenPos.y > chokePos.y ? -k : k) / sqrt(1.0 + k * k) * range;
							}
							BWAPI::Broodwar->drawCircleMap(sunkenPos, 10, BWAPI::Colors::Red, true);
							BWAPI::Broodwar->drawCircleMap(detectorPos, 10, BWAPI::Colors::Red, true);
							BWAPI::Broodwar->drawCircleMap(chokePos, 10, BWAPI::Colors::Red, true);
							Micro::SmartMove(detectorUnit, detectorPos);
						}
					}
				}
			}
		}
		// Move the detector toward the squadmate closest to the enemy.
		Micro::SmartMove(detectorUnit, detectorPos);
	}
}

// NOTE Unused but potentially useful.
BWAPI::Unit MicroDetectors::closestCloakedUnit(const BWAPI::Unitset & cloakedUnits, BWAPI::Unit detectorUnit)
{
	BWAPI::Unit closestCloaked = nullptr;
	double closestDist = 100000;

	for (const auto unit : cloakedUnits)
	{
		// if we haven't already assigned an detectorUnit to this cloaked unit
		if (!cloakedUnitMap[unit])
		{
			int dist = unit->getDistance(detectorUnit);

			if (dist < closestDist)
			{
				closestCloaked = unit;
				closestDist = dist;
			}
		}
	}

	return closestCloaked;
}