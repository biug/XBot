#pragma once

#include "Common.h"
#include "MapGrid.h"
#include "SquadOrder.h"
#include "InformationManager.h"
#include "Micro.h"

namespace XBot
{

class MicroManager
{
	BWAPI::Unitset		_units;

protected:
	
	SquadOrder			order;

	virtual void        executeMicro(const BWAPI::Unitset & targets) = 0;
	bool				buildScarabOrInterceptor(BWAPI::Unit u) const;
	bool                checkPositionWalkable(BWAPI::Position pos);
	bool                unitNearEnemy(BWAPI::Unit unit);
	bool                unitNearChokepoint(BWAPI::Unit unit) const;

	bool				mobilizeUnit(BWAPI::Unit unit) const;      // unsiege or unburrow
	bool				immobilizeUnit(BWAPI::Unit unit) const;    // siege or burrow
	bool				unstickStuckUnit(BWAPI::Unit unit);

	void                drawOrderText();

public:
						MicroManager();
    virtual				~MicroManager(){}

	const BWAPI::Unitset & getUnits() const;
	bool				containsType(BWAPI::UnitType type) const;
	BWAPI::Position     calcCenter() const;

	void				setUnits(const BWAPI::Unitset & u);
	void				execute(const SquadOrder & order);
	void				regroup(const BWAPI::Position & regroupPosition) const;

};
}