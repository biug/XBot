#pragma once

#include "Common.h"
#include "DistanceMap.hpp"
#include "StrategyManager.h"
#include "CombatSimulation.h"
#include "SquadOrder.h"

#include "MicroMelee.h"
#include "MicroRanged.h"

#include "MicroDetectors.h"
#include "MicroLurkers.h"
#include "MicroFlyers.h"

namespace XBot
{
    
class Squad
{
    std::string         _name;
	BWAPI::Unitset      _units;
	bool				_combatSquad;
	bool				_hasAir;
	bool				_hasGround;
	bool				_hasAntiAir;
	bool				_hasAntiGround;
	std::string         _regroupStatus;
	bool				_attackAtMax;
    int                 _lastRetreatSwitch;
    bool                _lastRetreatSwitchVal;
	bool				_lastVisitMain;
	bool				_lastVisitNatural;
    size_t              _priority;
	
	SquadOrder          _order;
	MicroMelee			_microMelee;
	MicroRanged			_microRanged;
	MicroDetectors		_microDetectors;
	MicroLurkers		_microLurkers;
	MicroFlyers			_microFlyers;

	std::map<BWAPI::Unit, bool>	_nearEnemy;

	BWAPI::Unit		getRegroupUnit();
	BWAPI::Unit		unitClosestToEnemy();
    
	void			updateUnits();
	void			addUnitsToMicroManagers();
	void			setNearEnemyUnits();
	void			setAllUnits();
	
	bool			unitNearEnemy(BWAPI::Unit unit);
	bool			needsToRegroup();

	void			doSurvey();

public:

	Squad(const std::string & name, SquadOrder order, size_t priority);
	Squad();
    ~Squad();

	void                update();
	void                setSquadOrder(const SquadOrder & so);
	void                addUnit(BWAPI::Unit u);
	void                removeUnit(BWAPI::Unit u);
	void				releaseWorkers();
    bool                containsUnit(BWAPI::Unit u) const;
	bool                containsUnitType(BWAPI::UnitType t) const;
	bool                isEmpty() const;
    void                clear();
    size_t              getPriority() const;
    void                setPriority(const size_t & priority);
    const std::string & getName() const;
    
	BWAPI::Position     calcCenter();
	BWAPI::Position     calcRegroupPosition();

	const BWAPI::Unitset &  getUnits() const;
	const SquadOrder &  getSquadOrder()	const;

	const bool			hasAir()        const { return _hasAir; };
	const bool			hasGround()     const { return _hasGround; };
	const bool			hasAntiAir()    const { return _hasAntiAir; };
	const bool			hasAntiGround() const { return _hasAntiGround; };

};
}