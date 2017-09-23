#include "MicroFlyers.h"
#include "UnitUtil.h"
#include "StateManager.h"

using namespace XBot;

// Note: Melee units are ground units only. Scourge is a "ranged" unit.

MicroFlyers::MicroFlyers()
{
}

void MicroFlyers::executeMicro(const BWAPI::Unitset & targets)
{
	assignTargets(targets);
}

void MicroFlyers::assignTargets(const BWAPI::Unitset & targets)
{
	auto & info = InformationManager::Instance();
	const BWAPI::Unitset & flyerUnits = getUnits();

	BWAPI::Unitset flyerUnitTargets;
	std::hash_set<const UnitInfo*> staticAirWeapons;
	for (const auto target : targets)
	{
		if (target->isVisible() &&
			target->isDetected() &&
			target->getPosition().isValid() &&
			target->getType() != BWAPI::UnitTypes::Zerg_Larva &&
			target->getType() != BWAPI::UnitTypes::Zerg_Egg &&
			!target->isStasised() &&
			!target->isUnderDisruptionWeb())             // melee unit can't attack under dweb
		{
			flyerUnitTargets.insert(target);
		}
	}
	for (const auto & uinfo : info.getUnitInfo(BWAPI::Broodwar->enemy()))
	{
		auto type = uinfo.second.type;
		if ((type.isBuilding() && type.airWeapon() != BWAPI::WeaponTypes::None) || type == BWAPI::UnitTypes::Terran_Bunker)
		{
			staticAirWeapons.insert(&uinfo.second);
		}
	}

	const auto myMainLoc = info.getMyMainBaseLocation();
	const auto enemyMainLoc = info.getEnemyMainBaseLocation();
	if (!enemyMainLoc || !myMainLoc)
	{
		return;
	}

	auto & state = StateManager::Instance();
	const auto & visit_target = state.flyer_visit_position;

	std::hash_map<const BWEM::Area*, int> areaFlyers;
	for (const auto & flyerUnit : flyerUnits)
	{
		auto area = info.getTileArea(flyerUnit->getTilePosition());
		areaFlyers[area] += 1;
	}
	auto baseArea = info.getTileArea(BWAPI::Broodwar->self()->getStartLocation());

	for (const auto flyerUnit : flyerUnits)
	{
		auto area = info.getTileArea(flyerUnit->getTilePosition());
		if (areaFlyers[area] < 6)
		{
			const UnitInfo* nearestAirWeapon = nullptr;
			for (const auto & staticAirWeapon : staticAirWeapons)
			{
				if (!nearestAirWeapon
					|| flyerUnit->getDistance(staticAirWeapon->lastPosition) < flyerUnit->getDistance(nearestAirWeapon->lastPosition))
				{
					nearestAirWeapon = staticAirWeapon;
				}
			}
			if (nearestAirWeapon && flyerUnit->getDistance(nearestAirWeapon->lastPosition) < nearestAirWeapon->type.airWeapon().maxRange() + 96)
			{
				auto path = BWEM::Map::Instance().GetPath(area, baseArea);
				if (!path.empty())
				{
					Micro::SmartMove(flyerUnit, BWAPI::Position(path[0]->Center()));
					continue;
				}
			}
		}

		// for anti cannon bot
		if (!visit_target.empty())
		{
			if (state.flyer_visit.find(flyerUnit) == state.flyer_visit.end())
			{
				state.flyer_visit[flyerUnit] = std::vector<bool>(visit_target.size(), false);
			}
			int visit_index = 0;
			auto & visit_pos = state.flyer_visit[flyerUnit];
			while (visit_index < visit_pos.size())
			{
				if (!visit_pos[visit_index]) break;
				++visit_index;
			}
			// if not visit all target
			if (visit_index < visit_pos.size())
			{
				// should visit all target
				if (flyerUnit->getDistance(visit_target[visit_index]) < 96)
				{
					visit_pos[visit_index] = true;
				}
				else
				{
					Micro::SmartMove(flyerUnit, visit_target[visit_index]);
				}
				continue;
			}
		}

		if (order.isCombatOrder())
		{
			if (flyerUnitTargets.empty())
			{
				// There are no targets. Move to the order position if not already close.
				if (flyerUnit->getDistance(order.getPosition()) > 90)
				{
					// UAB_ASSERT(meleeUnit->exists(), "bad worker");  // TODO temporary debugging - see Micro::SmartMove
					Micro::SmartMove(flyerUnit, order.getPosition());
				}
				continue;
			}

			// 找到最近的蚊子
			BWAPI::Unitset unitNear, scourgeNear;
			MapGrid::Instance().GetUnits(unitNear, flyerUnit->getPosition(), 96, false, true);
			for (const auto & unit : unitNear)
			{
				if (unit && unit->getType() == BWAPI::UnitTypes::Zerg_Scourge)
				{
					scourgeNear.insert(unit);
				}
			}
			BWAPI::Unit closestScourge = nullptr;
			for (const auto & scourge : scourgeNear)
			{
				if (!closestScourge ||
					flyerUnit->getDistance(scourge) < flyerUnit->getDistance(closestScourge))
				{
					closestScourge = scourge;
				}
			}
			// 如果附近有蚊子
			if (closestScourge
				&& closestScourge->getDistance(flyerUnit) < 96
				&& closestScourge->getDistance(flyerUnit) > 64)
			{
				// 和蚊子的直线连线反方向离开
				int x = flyerUnit->getPosition().x * 2 - closestScourge->getPosition().x;
				int y = flyerUnit->getPosition().y * 2 - closestScourge->getPosition().y;
				auto target = BWAPI::Position(BWAPI::Broodwar->self()->getStartLocation());
				if (x >= 0 && x < BWAPI::Broodwar->mapWidth() * 32
					&& y >= 0 && y < BWAPI::Broodwar->mapHeight() * 32)
				{
					target = BWAPI::Position(x, y);
					BWAPI::Broodwar->drawLineMap(flyerUnit->getPosition(), target, BWAPI::Colors::Purple);
				}
				Micro::SmartMove(flyerUnit, target);
			}
			// 否则正常攻击
			else
			{
				// There are targets. Pick the best one and attack it.
				// NOTE We *always* choose a target. We can't decide none are worth it and bypass them.
				//      This causes a lot of needless distraction.
				BWAPI::Unit target = getTarget(flyerUnit, flyerUnitTargets);
				if (flyerUnit->getType() == BWAPI::UnitTypes::Zerg_Mutalisk &&
					target && !target->getType().isBuilding() &&
					target->getType().airWeapon() != BWAPI::WeaponTypes::None)
				{
					Micro::MutaDanceTarget(flyerUnit, target);
				}
				else
				{
					Micro::SmartAttackUnit(flyerUnit, target);
				}
			}
		}

		if (Config::Debug::DrawUnitTargetInfo)
		{
			if (flyerUnit && flyerUnit->getPosition().isValid() && flyerUnit->getTargetPosition().isValid())
			{
				BWAPI::Broodwar->drawLineMap(flyerUnit->getPosition(), flyerUnit->getTargetPosition(),
					Config::Debug::ColorLineTarget);
			}
		}
	}
}

// Choose a target from the set. Never return null!
BWAPI::Unit MicroFlyers::getTarget(BWAPI::Unit rangedUnit, const BWAPI::Unitset & targets)
{
	int bestScore = -999999;
	BWAPI::Unit bestTarget = nullptr;

	for (const auto target : targets)
	{
		// not attack too far

		int priority = getAttackPriority(rangedUnit, target);     // 0..12
		int range = rangedUnit->getDistance(target);           // 0..map size in pixels
		int toGoal = target->getDistance(order.getPosition());  // 0..map size in pixels

																// Let's say that 1 priority step is worth 160 pixels (5 tiles).
																// We care about unit-target range and target-order position distance.
		int score = 5 * 32 * priority - range - toGoal / 2;

		// Adjust for special features.
		// This could adjust for relative speed and direction, so that we don't chase what we can't catch.
		if (rangedUnit->isInWeaponRange(target))
		{
			score += 4 * 32;
		}
		else if (!target->isMoving())
		{
			if (target->isSieged() ||
				target->getOrder() == BWAPI::Orders::Sieging ||
				target->getOrder() == BWAPI::Orders::Unsieging)
			{
				score += 48;
			}
			else
			{
				score += 24;
			}
		}
		else if (target->isBraking())
		{
			score += 16;
		}
		else if (target->getType().topSpeed() >= rangedUnit->getType().topSpeed())
		{
			score -= 5 * 32;
		}

		// Prefer targets that are already hurt.
		if (target->getType().getRace() == BWAPI::Races::Protoss && target->getShields() == 0)
		{
			score += 32;
		}
		if (target->getHitPoints() < target->getType().maxHitPoints())
		{
			score += 24;
		}

		BWAPI::DamageType damage = UnitUtil::GetWeapon(rangedUnit, target).damageType();
		if (damage == BWAPI::DamageTypes::Explosive)
		{
			if (target->getType().size() == BWAPI::UnitSizeTypes::Large)
			{
				score += 32;
			}
		}
		else if (damage == BWAPI::DamageTypes::Concussive)
		{
			if (target->getType().size() == BWAPI::UnitSizeTypes::Small)
			{
				score += 32;
			}
		}

		if (score > bestScore)
		{
			bestScore = score;
			bestTarget = target;
		}
	}

	return bestTarget;
}

// get the attack priority of a type
int MicroFlyers::getAttackPriority(BWAPI::Unit attacker, BWAPI::Unit target) const
{
	auto & state = StateManager::Instance();
	BWAPI::UnitType targetType = target->getType();
	double ratio = (1.25 - (double)target->getHitPoints() / (double)targetType.maxHitPoints());

	// if anti cannon
	if (!state.flyer_visit_position.empty())
	{
		if (targetType == BWAPI::UnitTypes::Protoss_Dragoon)
		{
			return 200;
		}
		else if (targetType == BWAPI::UnitTypes::Protoss_Corsair)
		{
			return 190;
		}
		else if (targetType == BWAPI::UnitTypes::Protoss_Stargate)
		{
			if (target->isCompleted()) return 180;
			return 170;
		}
		else if (targetType == BWAPI::UnitTypes::Protoss_Fleet_Beacon)
		{
			return 160;
		}
		// attack only have enough mutalisk
		else if (targetType == BWAPI::UnitTypes::Protoss_Photon_Cannon && state.mutalisk_completed > 12)
		{
			return 110;
		}
		else if (targetType == BWAPI::UnitTypes::Protoss_Photon_Cannon)
		{
			// if cannon in main
			const auto enemyMain = InformationManager::Instance().getEnemyMainBaseLocation();
			if (enemyMain)
			{
				if (target->getDistance(enemyMain->getPosition()) < 300)
				{
					return 210;
				}
			}
		}
	}

	if (attacker->getType() == BWAPI::UnitTypes::Zerg_Scourge)
	{
		switch (targetType.getID())
		{
		case BWAPI::UnitTypes::Enum::Zerg_Scourge:
		case BWAPI::UnitTypes::Enum::Zerg_Overlord:
		case BWAPI::UnitTypes::Enum::Protoss_Observer:
		case BWAPI::UnitTypes::Enum::Protoss_Interceptor:
			return 1;
		default:
		{
			if (targetType.isFlyer())
			{
				return 100;
			}
			return 0;
		}
		}
	}
	int fire = (targetType.airWeapon() != BWAPI::WeaponTypes::None ? targetType.airWeapon().damageAmount() : 0)
		+ (targetType.groundWeapon() != BWAPI::WeaponTypes::None ? targetType.groundWeapon().damageAmount() : 0);
	if (attacker->getType() == BWAPI::UnitTypes::Zerg_Guardian)
	{
		if (targetType.isFlyer()) return 0;
		return fire * ratio;
	}
	else if (attacker->getType() == BWAPI::UnitTypes::Zerg_Devourer)
	{
		if (!targetType.isFlyer()) return 0;
		return fire * ratio;
	}
	// mutalisk
	else
	{
		// 打农民
		if (targetType.isWorker())
		{
			return 100;
		}
		// 打防空塔
		else if (targetType.isBuilding() && targetType.airWeapon() != BWAPI::WeaponTypes::None)
		{
			return 105;
		}
		// 打地堡
		else if (targetType == BWAPI::UnitTypes::Terran_Bunker)
		{
			return 105;
		}
		// 不打宿主
		else if (targetType == BWAPI::UnitTypes::Zerg_Overlord)
		{
			return 1;
		}
		// 打地堡
		else if (targetType.isBuilding() && targetType.groundWeapon() != BWAPI::WeaponTypes::None)
		{
			return 90;
		}
		// 打建筑
		else if (targetType.isBuilding())
		{
			return 2;
		}
		// 打单位
		else
		{
			return fire * ratio;
		}
	}
}
