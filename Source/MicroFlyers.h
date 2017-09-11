#pragma once;

#include <Common.h>
#include "MicroManager.h"

namespace XBot
{
	class MicroManager;

	class MicroFlyers : public MicroManager
	{

	public:

		MicroFlyers();

		void executeMicro(const BWAPI::Unitset & targets) override;
		void assignTargets(const BWAPI::Unitset & targets);

		int getAttackPriority(BWAPI::Unit attacker, BWAPI::Unit unit) const;
		BWAPI::Unit getTarget(BWAPI::Unit meleeUnit, const BWAPI::Unitset & targets);
	};
}