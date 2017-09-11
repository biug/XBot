#include "UABase.h"

using namespace XBot;

// Should be called at the beginning of the game and never again.
// It picks out the geysers under that assumption.
// NOTE It's theoretically possible for a geyser to belong to more than one base.
//      That should not happen on a competitive map, though.
UABase::UABase(BWAPI::Position pos)
	: position(pos)
	, resourceDepot(nullptr)
	, owner(BWAPI::Broodwar->neutral())
	, reserved(false)
{
	findGeysers();
}

void UABase::findGeysers()
{
	for (auto unit : BWAPI::Broodwar->getNeutralUnits())
	{
		if ((unit->getType() == BWAPI::UnitTypes::Resource_Vespene_Geyser || unit->getType().isRefinery()) &&
			unit->getPosition().isValid() &&
			unit->getDistance(position) < 400)
		{
			geysers.insert(unit);
		}
	}
}

void UABase::setOwner(BWAPI::Unit depot, BWAPI::Player player)
{
	resourceDepot = depot;
	owner = player;
	reserved = false;
}