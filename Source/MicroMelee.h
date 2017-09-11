#pragma once;

#include <Common.h>
#include "MicroManager.h"

namespace XBot
{
class MicroManager;

class MicroMelee : public MicroManager
{

public:

	MicroMelee();

	void executeMicro(const BWAPI::Unitset & targets);
	void assignTargets(const BWAPI::Unitset & targets);

	int getAttackPriority(BWAPI::Unit attacker, BWAPI::Unit unit) const;
	BWAPI::Unit getTarget(BWAPI::Unit meleeUnit, const BWAPI::Unitset & targets);
	bool meleeUnitShouldRetreat(BWAPI::Unit meleeUnit, const BWAPI::Unitset & targets);
};
}