#pragma once

#include "Common.h"
#include "BWTA.h"

#include "UABase.h"
#include "UnitData.h"

namespace XBot
{

class InformationManager 
{
	BWAPI::Player	_self;
    BWAPI::Player	_enemy;

	int				_scanned;
	int				_cols, _rows;

	bool			_enemyProxy;

	bool			_weHaveCombatUnits;
	bool			_enemyHasAntiAir;
	bool			_enemyHasAirTech;
	bool			_enemyHasCloakTech;
	bool			_enemyHasMobileCloakTech;
	bool			_enemyHasOverlordHunters;
	bool			_enemyHasStaticDetection;
	bool			_enemyHasMobileDetection;

	std::vector<int>						_numSelfUnits;
	std::vector<int>						_numEnemyUnits;
	std::vector<int>						_numSelfCompletedUnits;
	std::vector<const BWEM::Area *>			_tileAreas;

	std::map<BWAPI::Player, UnitData>                   _unitData;
	std::map<BWAPI::Player, BWTA::BaseLocation *>       _mainBaseLocations;
	std::map<BWAPI::Player, BWTA::BaseLocation *>		_naturalBaseLocations;  // whether taken yet or not
	std::map<BWAPI::Player, std::set<BWTA::Region *> >  _occupiedRegions;        // contains any building
	std::map<BWTA::BaseLocation *, UABase *>				_theBases;

	InformationManager();

	void					initializeTheBases();
	void                    initializeRegionInformation();
	void					initializeNaturalBase();

	int						getTileIndex(int x, int y) const { return x * _rows + y; }
	int						getTileIndex(BWAPI::TilePosition a) const { return getTileIndex(a.x, a.y); }
	bool					isTileValid(int x, int y) const { return x >= 0 && x < _cols && y >= 0 && y < _rows; }

	void					baseInferred(BWTA::BaseLocation * base);
	void					baseFound(BWAPI::Unit depot);
	void					baseLost(BWAPI::TilePosition basePosition);
	void					maybeAddBase(BWAPI::Unit unit);
	bool					closeEnough(BWAPI::TilePosition a, BWAPI::TilePosition b);
	void					chooseNewMainBase();

	void                    updateUnit(BWAPI::Unit unit);
    void                    updateUnitInfo();
    void                    updateBaseLocationInfo();
	void					updateNaturalBase(BWAPI::Player player);
	BWTA::BaseLocation*		getNaturalBase(BWTA::BaseLocation* base);
    void                    updateOccupiedRegions(BWTA::Region * region,BWAPI::Player player);
    bool                    isValidUnit(BWAPI::Unit unit);

public:

    void                    update();

    // event driven stuff
	void					onUnitShow(BWAPI::Unit unit)        { updateUnit(unit); maybeAddBase(unit); }
    void					onUnitHide(BWAPI::Unit unit)        { updateUnit(unit); }
	void					onUnitCreate(BWAPI::Unit unit)		{ updateUnit(unit); maybeAddBase(unit); }
    void					onUnitComplete(BWAPI::Unit unit)    { updateUnit(unit); }
	void					onUnitMorph(BWAPI::Unit unit)       { updateUnit(unit); maybeAddBase(unit); }
    void					onUnitRenegade(BWAPI::Unit unit)    { updateUnit(unit); }
    void					onUnitDestroy(BWAPI::Unit unit);

	bool												isEnemyBuildingInRegion(BWTA::Region * region);
	std::pair<BWTA::BaseLocation*, BWAPI::TilePosition>	isEnemyBuildingOnChoke();

	const BWEM::Area *		getTileArea(BWAPI::TilePosition tile) const;
	int						getNumSelfUnits(BWAPI::UnitType type) const;
	int						getNumEnemyUnits(BWAPI::UnitType type) const;
	int						getNumSelfCompletedUnits(BWAPI::UnitType type) const;

    void                    getNearbyForce(std::vector<UnitInfo> & unitInfo,BWAPI::Position p,BWAPI::Player player,int radius);

    const UIMap &           getUnitInfo(BWAPI::Player player) const;

	std::set<BWTA::Region *> &  getOccupiedRegions(BWAPI::Player player);

	BWTA::BaseLocation *	getMyMainBaseLocation();
	BWTA::BaseLocation *	getEnemyMainBaseLocation();
	BWTA::BaseLocation *    getMainBaseLocation(BWAPI::Player player);
	BWAPI::Player			getBaseOwner(BWTA::BaseLocation * base);
	BWAPI::Unit 			getBaseDepot(BWTA::BaseLocation * base);
	BWTA::BaseLocation *	getMyNaturalLocation();
	BWTA::BaseLocation *	getEnemyNaturalLocation();
	BWTA::BaseLocation *    getNaturalLocation(BWAPI::Player player);
	int						getNumBases(BWAPI::Player player);
	int						getNumFreeLandBases();
	int						getMyNumMineralPatches();
	int						getMyNumGeysers();
	void					getMyGasCounts(int & nRefineries, int & nFreeGeysers);

	void					maybeChooseNewMainBase();

	bool					isBaseReserved(BWTA::BaseLocation * base);
	void					reserveBase(BWTA::BaseLocation * base);
	void					unreserveBase(BWTA::BaseLocation * base);
	void					unreserveBase(BWAPI::TilePosition baseTilePosition);

	int						getAir2GroundSupply(BWAPI::Player player) const;

	bool					weHaveCombatUnits();

	bool					enemyHasAntiAir();
	bool					enemyHasAirTech();
	bool                    enemyHasCloakTech();
	bool                    enemyHasMobileCloakTech();
	bool					enemyHasOverlordHunters();
	bool					enemyHasStaticDetection();
	bool					enemyHasMobileDetection();

	int						nScourgeNeeded();           // zerg specific

    void                    drawExtendedInterface();
    void                    drawUnitInformation(int x,int y);
    void                    drawMapInformation();
	void					drawBaseInformation(int x, int y);

    const UnitData &        getUnitData(BWAPI::Player player) const;

	// yay for singletons!
	static InformationManager & Instance();
};
}
