#include "StateManager.h"
#include "InformationManager.h"

using namespace XBot;

StateManager & StateManager::Instance()
{
	static StateManager instance;
	return instance;
}

void StateManager::updateCurrentState(BuildOrderQueue &queue)
{
	auto self = BWAPI::Broodwar->self();
	auto enemy = BWAPI::Broodwar->enemy();

	//我方
	larva_count = InformationManager::Instance().getNumSelfUnits(BWAPI::UnitTypes::Zerg_Larva);
	drone_count = InformationManager::Instance().getNumSelfUnits(BWAPI::UnitTypes::Zerg_Drone);
	zergling_count = InformationManager::Instance().getNumSelfUnits(BWAPI::UnitTypes::Zerg_Zergling);
	hydralisk_count = InformationManager::Instance().getNumSelfUnits(BWAPI::UnitTypes::Zerg_Hydralisk);
	lurker_count = InformationManager::Instance().getNumSelfUnits(BWAPI::UnitTypes::Zerg_Lurker);
	ultralisk_count = InformationManager::Instance().getNumSelfUnits(BWAPI::UnitTypes::Zerg_Ultralisk);
	defiler_count = InformationManager::Instance().getNumSelfUnits(BWAPI::UnitTypes::Zerg_Defiler);
	overlord_count = InformationManager::Instance().getNumSelfUnits(BWAPI::UnitTypes::Zerg_Overlord);
	mutalisk_count = InformationManager::Instance().getNumSelfUnits(BWAPI::UnitTypes::Zerg_Mutalisk);
	scourge_count = InformationManager::Instance().getNumSelfUnits(BWAPI::UnitTypes::Zerg_Scourge);
	queen_count = InformationManager::Instance().getNumSelfUnits(BWAPI::UnitTypes::Zerg_Queen);
	guardian_count = InformationManager::Instance().getNumSelfUnits(BWAPI::UnitTypes::Zerg_Guardian);
	devourer_count = InformationManager::Instance().getNumSelfUnits(BWAPI::UnitTypes::Zerg_Devourer);

	larva_completed = InformationManager::Instance().getNumSelfCompletedUnits(BWAPI::UnitTypes::Zerg_Larva);
	drone_completed = InformationManager::Instance().getNumSelfCompletedUnits(BWAPI::UnitTypes::Zerg_Drone);
	zergling_completed = InformationManager::Instance().getNumSelfCompletedUnits(BWAPI::UnitTypes::Zerg_Zergling);
	hydralisk_completed = InformationManager::Instance().getNumSelfCompletedUnits(BWAPI::UnitTypes::Zerg_Hydralisk);
	lurker_completed = InformationManager::Instance().getNumSelfCompletedUnits(BWAPI::UnitTypes::Zerg_Lurker);
	ultralisk_completed = InformationManager::Instance().getNumSelfCompletedUnits(BWAPI::UnitTypes::Zerg_Ultralisk);
	defiler_completed = InformationManager::Instance().getNumSelfCompletedUnits(BWAPI::UnitTypes::Zerg_Defiler);
	overlord_completed = InformationManager::Instance().getNumSelfCompletedUnits(BWAPI::UnitTypes::Zerg_Overlord);
	mutalisk_completed = InformationManager::Instance().getNumSelfCompletedUnits(BWAPI::UnitTypes::Zerg_Mutalisk);
	scourge_completed = InformationManager::Instance().getNumSelfCompletedUnits(BWAPI::UnitTypes::Zerg_Scourge);
	queen_completed = InformationManager::Instance().getNumSelfCompletedUnits(BWAPI::UnitTypes::Zerg_Queen);
	guardian_completed = InformationManager::Instance().getNumSelfCompletedUnits(BWAPI::UnitTypes::Zerg_Guardian);
	devourer_completed = InformationManager::Instance().getNumSelfCompletedUnits(BWAPI::UnitTypes::Zerg_Devourer);

	larva_in_queue = queue.numInQueue(BWAPI::UnitTypes::Zerg_Larva);		//幼虫
	drone_in_queue = queue.numInQueue(BWAPI::UnitTypes::Zerg_Drone);		//工蜂
	zergling_in_queue = queue.numInQueue(BWAPI::UnitTypes::Zerg_Zergling) * 2;	//小狗
	hydralisk_in_queue = queue.numInQueue(BWAPI::UnitTypes::Zerg_Hydralisk);//刺蛇
	lurker_in_queue = queue.numInQueue(BWAPI::UnitTypes::Zerg_Lurker);		//地刺
	ultralisk_in_queue = queue.numInQueue(BWAPI::UnitTypes::Zerg_Ultralisk);//雷兽
	defiler_in_queue = queue.numInQueue(BWAPI::UnitTypes::Zerg_Defiler);	//蝎子
	overlord_in_queue = queue.numInQueue(BWAPI::UnitTypes::Zerg_Overlord);	//领主
	mutalisk_in_queue = queue.numInQueue(BWAPI::UnitTypes::Zerg_Mutalisk);	//飞龙
	scourge_in_queue = queue.numInQueue(BWAPI::UnitTypes::Zerg_Scourge) * 2;	//自爆蚊
	queen_in_queue = queue.numInQueue(BWAPI::UnitTypes::Zerg_Queen);		//女王
	guardian_in_queue = queue.numInQueue(BWAPI::UnitTypes::Zerg_Guardian);	//守卫者
	devourer_in_queue = queue.numInQueue(BWAPI::UnitTypes::Zerg_Devourer);	//吞噬者

	larva_total = larva_count + larva_in_queue;
	drone_total = drone_count + drone_in_queue;
	zergling_total = zergling_count + zergling_in_queue;
	hydralisk_total = hydralisk_count + hydralisk_in_queue;
	lurker_total = lurker_count + lurker_in_queue;
	ultralisk_total = ultralisk_count + ultralisk_in_queue;
	defiler_total = defiler_count + defiler_in_queue;
	overlord_total = overlord_count + overlord_in_queue;
	mutalisk_total = mutalisk_count + mutalisk_in_queue;
	scourge_total = scourge_count + scourge_in_queue;
	queen_total = queen_count + queen_in_queue;
	guardian_total = guardian_count + guardian_in_queue;
	devourer_total = devourer_count + devourer_in_queue;

	metabolic_boost_in_queue = queue.numInQueue(BWAPI::UpgradeTypes::Metabolic_Boost);
	lurker_aspect_in_queue = queue.numInQueue(BWAPI::TechTypes::Lurker_Aspect);
	adrenal_glands_in_queue = queue.numInQueue(BWAPI::UpgradeTypes::Adrenal_Glands);
	grooved_spines_in_queue = queue.numInQueue(BWAPI::UpgradeTypes::Grooved_Spines);
	muscular_arguments_in_queue = queue.numInQueue(BWAPI::UpgradeTypes::Muscular_Augments);
	melee_attacks_in_queue = queue.numInQueue(BWAPI::UpgradeTypes::Zerg_Melee_Attacks);
	missile_attacks_in_queue = queue.numInQueue(BWAPI::UpgradeTypes::Zerg_Missile_Attacks);
	ground_carapace_in_queue = queue.numInQueue(BWAPI::UpgradeTypes::Zerg_Carapace);
	flyer_attacks_in_queue = queue.numInQueue(BWAPI::UpgradeTypes::Zerg_Flyer_Attacks);
	flyer_carapace_in_queue = queue.numInQueue(BWAPI::UpgradeTypes::Zerg_Flyer_Carapace);

	metabolic_boost_total = queue.numInQueue(BWAPI::UpgradeTypes::Metabolic_Boost)
		+ self->getUpgradeLevel(BWAPI::UpgradeTypes::Metabolic_Boost)
		+ self->isUpgrading(BWAPI::UpgradeTypes::Metabolic_Boost);
	lurker_aspect_total = queue.numInQueue(BWAPI::TechTypes::Lurker_Aspect)
		+ self->hasResearched(BWAPI::TechTypes::Lurker_Aspect)
		+ self->isResearching(BWAPI::TechTypes::Lurker_Aspect);
	adrenal_glands_total = queue.numInQueue(BWAPI::UpgradeTypes::Adrenal_Glands)
		+ self->getUpgradeLevel(BWAPI::UpgradeTypes::Adrenal_Glands)
		+ self->isUpgrading(BWAPI::UpgradeTypes::Adrenal_Glands);
	grooved_spines_total = queue.numInQueue(BWAPI::UpgradeTypes::Grooved_Spines)
		+ self->getUpgradeLevel(BWAPI::UpgradeTypes::Grooved_Spines)
		+ self->isUpgrading(BWAPI::UpgradeTypes::Grooved_Spines);
	muscular_arguments_total = queue.numInQueue(BWAPI::UpgradeTypes::Muscular_Augments)
		+ self->getUpgradeLevel(BWAPI::UpgradeTypes::Muscular_Augments)
		+ self->isUpgrading(BWAPI::UpgradeTypes::Muscular_Augments);
	melee_attacks_total = queue.numInQueue(BWAPI::UpgradeTypes::Zerg_Melee_Attacks)
		+ self->getUpgradeLevel(BWAPI::UpgradeTypes::Zerg_Melee_Attacks)
		+ self->isUpgrading(BWAPI::UpgradeTypes::Zerg_Melee_Attacks);
	missile_attacks_total = queue.numInQueue(BWAPI::UpgradeTypes::Zerg_Missile_Attacks)
		+ self->getUpgradeLevel(BWAPI::UpgradeTypes::Zerg_Missile_Attacks)
		+ self->isUpgrading(BWAPI::UpgradeTypes::Zerg_Missile_Attacks);
	ground_carapace_total = queue.numInQueue(BWAPI::UpgradeTypes::Zerg_Carapace)
		+ self->getUpgradeLevel(BWAPI::UpgradeTypes::Zerg_Carapace)
		+ self->isUpgrading(BWAPI::UpgradeTypes::Zerg_Carapace);
	flyer_attacks_total = queue.numInQueue(BWAPI::UpgradeTypes::Zerg_Flyer_Attacks)
		+ self->getUpgradeLevel(BWAPI::UpgradeTypes::Zerg_Flyer_Attacks)
		+ self->isUpgrading(BWAPI::UpgradeTypes::Zerg_Flyer_Attacks);
	flyer_carapace_total = queue.numInQueue(BWAPI::UpgradeTypes::Zerg_Flyer_Carapace)
		+ self->getUpgradeLevel(BWAPI::UpgradeTypes::Zerg_Flyer_Carapace)
		+ self->isUpgrading(BWAPI::UpgradeTypes::Zerg_Flyer_Carapace);

	//建筑
	hatchery_count = InformationManager::Instance().getNumSelfUnits(BWAPI::UnitTypes::Zerg_Hatchery);
	lair_count = InformationManager::Instance().getNumSelfUnits(BWAPI::UnitTypes::Zerg_Lair);
	hive_count = InformationManager::Instance().getNumSelfUnits(BWAPI::UnitTypes::Zerg_Hive);
	base_count = hatchery_count + lair_count + hive_count;
	extractor_count = InformationManager::Instance().getNumSelfUnits(BWAPI::UnitTypes::Zerg_Extractor);
	creep_colony_count = InformationManager::Instance().getNumSelfUnits(BWAPI::UnitTypes::Zerg_Creep_Colony);
	sunken_colony_count = InformationManager::Instance().getNumSelfUnits(BWAPI::UnitTypes::Zerg_Sunken_Colony);
	spore_colony_count = InformationManager::Instance().getNumSelfUnits(BWAPI::UnitTypes::Zerg_Spore_Colony);
	spawning_pool_count = InformationManager::Instance().getNumSelfUnits(BWAPI::UnitTypes::Zerg_Spawning_Pool);
	evolution_chamber_count = InformationManager::Instance().getNumSelfUnits(BWAPI::UnitTypes::Zerg_Evolution_Chamber);
	hydralisk_den_count = InformationManager::Instance().getNumSelfUnits(BWAPI::UnitTypes::Zerg_Hydralisk_Den);
	queens_nest_count = InformationManager::Instance().getNumSelfUnits(BWAPI::UnitTypes::Zerg_Queens_Nest);
	defiler_mound_count = InformationManager::Instance().getNumSelfUnits(BWAPI::UnitTypes::Zerg_Defiler_Mound);
	spire_count = InformationManager::Instance().getNumSelfUnits(BWAPI::UnitTypes::Zerg_Spire);
	greater_spire_count = InformationManager::Instance().getNumSelfUnits(BWAPI::UnitTypes::Zerg_Greater_Spire);
	nydus_canal_count = InformationManager::Instance().getNumSelfUnits(BWAPI::UnitTypes::Zerg_Nydus_Canal);
	ultralisk_cavern_count = InformationManager::Instance().getNumSelfUnits(BWAPI::UnitTypes::Zerg_Ultralisk_Cavern);

	hatchery_completed = InformationManager::Instance().getNumSelfCompletedUnits(BWAPI::UnitTypes::Zerg_Hatchery);
	lair_completed = InformationManager::Instance().getNumSelfCompletedUnits(BWAPI::UnitTypes::Zerg_Lair);
	hive_completed = InformationManager::Instance().getNumSelfCompletedUnits(BWAPI::UnitTypes::Zerg_Hive);
	base_completed = hatchery_completed + lair_count + hive_count;
	extractor_completed = InformationManager::Instance().getNumSelfCompletedUnits(BWAPI::UnitTypes::Zerg_Extractor);
	creep_colony_completed = InformationManager::Instance().getNumSelfCompletedUnits(BWAPI::UnitTypes::Zerg_Creep_Colony);
	sunken_colony_completed = InformationManager::Instance().getNumSelfCompletedUnits(BWAPI::UnitTypes::Zerg_Sunken_Colony);
	spore_colony_completed = InformationManager::Instance().getNumSelfCompletedUnits(BWAPI::UnitTypes::Zerg_Spore_Colony);
	spawning_pool_completed = InformationManager::Instance().getNumSelfCompletedUnits(BWAPI::UnitTypes::Zerg_Spawning_Pool);
	evolution_chamber_completed = InformationManager::Instance().getNumSelfCompletedUnits(BWAPI::UnitTypes::Zerg_Evolution_Chamber);
	hydralisk_den_completed = InformationManager::Instance().getNumSelfCompletedUnits(BWAPI::UnitTypes::Zerg_Hydralisk_Den);
	queens_nest_completed = InformationManager::Instance().getNumSelfCompletedUnits(BWAPI::UnitTypes::Zerg_Queens_Nest);
	defiler_mound_completed = InformationManager::Instance().getNumSelfCompletedUnits(BWAPI::UnitTypes::Zerg_Defiler_Mound);
	spire_completed = InformationManager::Instance().getNumSelfCompletedUnits(BWAPI::UnitTypes::Zerg_Spire);
	greater_spire_completed = InformationManager::Instance().getNumSelfCompletedUnits(BWAPI::UnitTypes::Zerg_Greater_Spire);
	nydus_canal_completed = InformationManager::Instance().getNumSelfCompletedUnits(BWAPI::UnitTypes::Zerg_Nydus_Canal);
	ultralisk_cavern_completed = InformationManager::Instance().getNumSelfCompletedUnits(BWAPI::UnitTypes::Zerg_Ultralisk_Cavern);

	hatchery_in_queue = queue.numInQueue(BWAPI::UnitTypes::Zerg_Hatchery);
	lair_in_queue = queue.numInQueue(BWAPI::UnitTypes::Zerg_Lair);
	hive_in_queue = queue.numInQueue(BWAPI::UnitTypes::Zerg_Hive);
	base_in_queue = hatchery_in_queue + lair_in_queue + hive_in_queue;
	extractor_in_queue = queue.numInQueue(BWAPI::UnitTypes::Zerg_Extractor);
	creep_colony_in_queue = queue.numInQueue(BWAPI::UnitTypes::Zerg_Creep_Colony);
	sunken_colony_in_queue = queue.numInQueue(BWAPI::UnitTypes::Zerg_Sunken_Colony);
	spore_colony_in_queue = queue.numInQueue(BWAPI::UnitTypes::Zerg_Spore_Colony);
	spawning_pool_in_queue = queue.numInQueue(BWAPI::UnitTypes::Zerg_Spawning_Pool);
	evolution_chamber_in_queue = queue.numInQueue(BWAPI::UnitTypes::Zerg_Evolution_Chamber);
	hydralisk_den_in_queue = queue.numInQueue(BWAPI::UnitTypes::Zerg_Hydralisk_Den);
	queens_nest_in_queue = queue.numInQueue(BWAPI::UnitTypes::Zerg_Queens_Nest);
	defiler_mound_in_queue = queue.numInQueue(BWAPI::UnitTypes::Zerg_Defiler_Mound);
	spire_in_queue = queue.numInQueue(BWAPI::UnitTypes::Zerg_Spire);
	greater_spire_in_queue = queue.numInQueue(BWAPI::UnitTypes::Zerg_Greater_Spire);
	nydus_canal_in_queue = queue.numInQueue(BWAPI::UnitTypes::Zerg_Nydus_Canal);
	ultralisk_cavern_in_queue = queue.numInQueue(BWAPI::UnitTypes::Zerg_Ultralisk_Cavern);

	hatchery_being_built = BuildingManager::Instance().getNumUnstarted(BWAPI::UnitTypes::Zerg_Hatchery);
	lair_being_built = BuildingManager::Instance().getNumUnstarted(BWAPI::UnitTypes::Zerg_Lair);
	hive_being_built = BuildingManager::Instance().getNumUnstarted(BWAPI::UnitTypes::Zerg_Hive);
	base_being_built = hatchery_being_built + lair_being_built + hive_being_built;
	extractor_being_built = BuildingManager::Instance().getNumUnstarted(BWAPI::UnitTypes::Zerg_Extractor);
	creep_colony_being_built = BuildingManager::Instance().getNumUnstarted(BWAPI::UnitTypes::Zerg_Creep_Colony);
	sunken_colony_being_built = BuildingManager::Instance().getNumUnstarted(BWAPI::UnitTypes::Zerg_Sunken_Colony);
	spore_colony_being_built = BuildingManager::Instance().getNumUnstarted(BWAPI::UnitTypes::Zerg_Spore_Colony);
	spawning_pool_being_built = BuildingManager::Instance().getNumUnstarted(BWAPI::UnitTypes::Zerg_Spawning_Pool);
	evolution_chamber_being_built = BuildingManager::Instance().getNumUnstarted(BWAPI::UnitTypes::Zerg_Evolution_Chamber);
	hydralisk_den_being_built = BuildingManager::Instance().getNumUnstarted(BWAPI::UnitTypes::Zerg_Hydralisk_Den);
	queens_nest_being_built = BuildingManager::Instance().getNumUnstarted(BWAPI::UnitTypes::Zerg_Queens_Nest);
	defiler_mound_being_built = BuildingManager::Instance().getNumUnstarted(BWAPI::UnitTypes::Zerg_Defiler_Mound);
	spire_being_built = BuildingManager::Instance().getNumUnstarted(BWAPI::UnitTypes::Zerg_Spire);
	greater_spire_being_built = BuildingManager::Instance().getNumUnstarted(BWAPI::UnitTypes::Zerg_Greater_Spire);
	nydus_canal_being_built = BuildingManager::Instance().getNumUnstarted(BWAPI::UnitTypes::Zerg_Nydus_Canal);
	ultralisk_cavern_being_built = BuildingManager::Instance().getNumUnstarted(BWAPI::UnitTypes::Zerg_Ultralisk_Cavern);

	hatchery_waiting = hatchery_in_queue + hatchery_being_built;
	lair_waiting = lair_in_queue + lair_being_built;
	hive_waiting = hive_in_queue + hive_being_built;
	base_waiting = hatchery_waiting + lair_waiting + hive_waiting;
	extractor_waiting = extractor_in_queue + extractor_being_built;
	creep_colony_waiting = creep_colony_in_queue + creep_colony_being_built;
	sunken_colony_waiting = sunken_colony_in_queue + sunken_colony_being_built;
	spore_colony_waiting = spore_colony_in_queue + spore_colony_being_built;
	spawning_pool_waiting = spawning_pool_in_queue + spawning_pool_being_built;
	evolution_chamber_waiting = evolution_chamber_in_queue + evolution_chamber_being_built;
	hydralisk_den_waiting = hydralisk_den_in_queue + hydralisk_den_being_built;
	queens_nest_waiting = queens_nest_in_queue + queens_nest_being_built;
	defiler_mound_waiting = defiler_mound_in_queue + defiler_mound_being_built;
	spire_waiting = spire_in_queue + spire_being_built;
	greater_spire_waiting = greater_spire_in_queue + greater_spire_being_built;
	nydus_canal_waiting = nydus_canal_in_queue + nydus_canal_being_built;
	ultralisk_cavern_waiting = ultralisk_cavern_in_queue + ultralisk_cavern_being_built;

	hatchery_total = hatchery_count + hatchery_in_queue + hatchery_being_built;
	lair_total = lair_count + lair_in_queue + lair_being_built;
	hive_total = hive_count + hive_in_queue + hive_being_built;
	base_total = hatchery_total + lair_total + hive_total;
	extractor_total = extractor_count + extractor_in_queue + extractor_being_built;
	creep_colony_total = creep_colony_count + creep_colony_in_queue + creep_colony_being_built;
	sunken_colony_total = sunken_colony_count + sunken_colony_in_queue + sunken_colony_being_built;
	spore_colony_total = spore_colony_count + spore_colony_in_queue + spore_colony_being_built;
	spawning_pool_total = spawning_pool_count + spawning_pool_in_queue + spawning_pool_being_built;
	evolution_chamber_total = evolution_chamber_count + evolution_chamber_in_queue + evolution_chamber_being_built;
	hydralisk_den_total = hydralisk_den_count + hydralisk_den_in_queue + hydralisk_den_being_built;
	queens_nest_total = queens_nest_count + queens_nest_in_queue + queens_nest_being_built;
	defiler_mound_total = defiler_mound_count + defiler_mound_in_queue + defiler_mound_being_built;
	spire_total = spire_count + spire_in_queue + spire_being_built;
	greater_spire_total = greater_spire_count + greater_spire_in_queue + greater_spire_being_built;
	nydus_canal_total = nydus_canal_count + nydus_canal_in_queue + nydus_canal_being_built;
	ultralisk_cavern_total = ultralisk_cavern_count + ultralisk_cavern_in_queue + ultralisk_cavern_being_built;

	//军事力量
	army_supply = 0;
	air_army_supply = 0;
	ground_army_supply = 0;

	//敌方
	enemy_terran_unit_count = 0;
	enemy_protos_unit_count = 0;
	enemy_zerg_unit_count = 0;

	enemy_marine_count = InformationManager::Instance().getNumEnemyUnits(BWAPI::UnitTypes::Terran_Marine);
	enemy_firebat_count = InformationManager::Instance().getNumEnemyUnits(BWAPI::UnitTypes::Terran_Firebat);
	enemy_medic_count = InformationManager::Instance().getNumEnemyUnits(BWAPI::UnitTypes::Terran_Medic);
	enemy_ghost_count = InformationManager::Instance().getNumEnemyUnits(BWAPI::UnitTypes::Terran_Ghost);
	enemy_vulture_count = InformationManager::Instance().getNumEnemyUnits(BWAPI::UnitTypes::Terran_Vulture);
	enemy_tank_count = InformationManager::Instance().getNumEnemyUnits(BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode)
		+ InformationManager::Instance().getNumEnemyUnits(BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode);
	enemy_goliath_count = InformationManager::Instance().getNumEnemyUnits(BWAPI::UnitTypes::Terran_Goliath);
	enemy_wraith_count = InformationManager::Instance().getNumEnemyUnits(BWAPI::UnitTypes::Terran_Wraith);
	enemy_valkyrie_count = InformationManager::Instance().getNumEnemyUnits(BWAPI::UnitTypes::Terran_Valkyrie);
	enemy_bc_count = InformationManager::Instance().getNumEnemyUnits(BWAPI::UnitTypes::Terran_Battlecruiser);
	enemy_science_vessel_count = InformationManager::Instance().getNumEnemyUnits(BWAPI::UnitTypes::Terran_Science_Vessel);
	enemy_dropship_count = InformationManager::Instance().getNumEnemyUnits(BWAPI::UnitTypes::Terran_Dropship);

	enemy_supply_depot = InformationManager::Instance().getNumEnemyUnits(BWAPI::UnitTypes::Terran_Supply_Depot);
	enemy_refinery = InformationManager::Instance().getNumEnemyUnits(BWAPI::UnitTypes::Terran_Refinery);
	enemy_bunker_count = InformationManager::Instance().getNumEnemyUnits(BWAPI::UnitTypes::Terran_Bunker);
	enemy_barrack_count = InformationManager::Instance().getNumEnemyUnits(BWAPI::UnitTypes::Terran_Barracks);
	enemy_factory_count = InformationManager::Instance().getNumEnemyUnits(BWAPI::UnitTypes::Terran_Factory);
	enemy_starport_count = InformationManager::Instance().getNumEnemyUnits(BWAPI::UnitTypes::Terran_Starport);

	enemy_zealot_count = InformationManager::Instance().getNumEnemyUnits(BWAPI::UnitTypes::Protoss_Zealot);					//狂热者
	enemy_dragoon_count = InformationManager::Instance().getNumEnemyUnits(BWAPI::UnitTypes::Protoss_Dragoon);					//龙骑
	enemy_ht_count = InformationManager::Instance().getNumEnemyUnits(BWAPI::UnitTypes::Protoss_High_Templar);					//光明圣堂
	enemy_dt_count = InformationManager::Instance().getNumEnemyUnits(BWAPI::UnitTypes::Protoss_Dark_Templar);					//黑暗圣堂
	enemy_reaver_count = InformationManager::Instance().getNumEnemyUnits(BWAPI::UnitTypes::Protoss_Reaver);					//金甲虫
	enemy_shuttle_count = InformationManager::Instance().getNumEnemyUnits(BWAPI::UnitTypes::Protoss_Shuttle);					//运输机
	enemy_carrier_count = InformationManager::Instance().getNumEnemyUnits(BWAPI::UnitTypes::Protoss_Carrier);					//航母
	enemy_arbiter_count = InformationManager::Instance().getNumEnemyUnits(BWAPI::UnitTypes::Protoss_Arbiter);					//仲裁者
	enemy_corsair_count = InformationManager::Instance().getNumEnemyUnits(BWAPI::UnitTypes::Protoss_Corsair);					//海盗船
	enemy_scout_count = InformationManager::Instance().getNumEnemyUnits(BWAPI::UnitTypes::Protoss_Scout);						//侦察机

	enemy_assimilator = InformationManager::Instance().getNumEnemyUnits(BWAPI::UnitTypes::Protoss_Assimilator);
	enemy_forge = InformationManager::Instance().getNumEnemyUnits(BWAPI::UnitTypes::Protoss_Forge);
	enemy_cannon_count = InformationManager::Instance().getNumEnemyUnits(BWAPI::UnitTypes::Protoss_Photon_Cannon);
	enemy_gateway_count = InformationManager::Instance().getNumEnemyUnits(BWAPI::UnitTypes::Protoss_Gateway);
	enemy_stargate_count = InformationManager::Instance().getNumEnemyUnits(BWAPI::UnitTypes::Protoss_Stargate);
	enemy_robotics_facility_count = InformationManager::Instance().getNumEnemyUnits(BWAPI::UnitTypes::Protoss_Robotics_Facility);

	enemy_zergling_count = InformationManager::Instance().getNumEnemyUnits(BWAPI::UnitTypes::Zerg_Zergling);				//小狗
	enemy_hydralisk_count = InformationManager::Instance().getNumEnemyUnits(BWAPI::UnitTypes::Zerg_Hydralisk);			//刺蛇
	enemy_lurker_count = InformationManager::Instance().getNumEnemyUnits(BWAPI::UnitTypes::Zerg_Lurker);					//地刺
	enemy_ultralisk_count = InformationManager::Instance().getNumEnemyUnits(BWAPI::UnitTypes::Zerg_Ultralisk);			//雷兽
	enemy_defiler_count = InformationManager::Instance().getNumEnemyUnits(BWAPI::UnitTypes::Zerg_Defiler);				//蝎子
	enemy_mutalisk_count = InformationManager::Instance().getNumEnemyUnits(BWAPI::UnitTypes::Zerg_Mutalisk);				//飞龙
	enemy_queen_count = InformationManager::Instance().getNumEnemyUnits(BWAPI::UnitTypes::Zerg_Queen);					//女王

	enemy_spawning_pool_count = InformationManager::Instance().getNumEnemyUnits(BWAPI::UnitTypes::Zerg_Spawning_Pool);
	enemy_hydralisk_den_count = InformationManager::Instance().getNumEnemyUnits(BWAPI::UnitTypes::Zerg_Hydralisk_Den);
	enemy_evolution_chamber_count = InformationManager::Instance().getNumEnemyUnits(BWAPI::UnitTypes::Zerg_Evolution_Chamber);
	enemy_spire_count = InformationManager::Instance().getNumEnemyUnits(BWAPI::UnitTypes::Zerg_Spire);

	enemy_army_supply = 0;
	enemy_air_army_supply = 0;
	enemy_ground_army_supply = 0;
	enemy_ground_large_army_supply = 0;
	enemy_ground_small_army_supply = 0;
	enemy_anti_air_army_supply = 0;
	enemy_biological_army_supply = 0;
	enemy_static_defence_count = 0;
	enemy_proxy_building_count = 0;
	enemy_attacking_army_supply = 0;
	enemy_attacking_worker_count = 0;
	enemy_cloaked_unit_count = 0;

	//通用建筑和单位
	enemy_worker_count = 0;
	enemy_base_count = 0;

	//敌方
	for (const auto & u : InformationManager::Instance().getUnitInfo(enemy))
	{
		auto info = u.second;
		auto type = info.type;
		switch (type.getRace().getID())
		{
		case BWAPI::Races::Enum::Terran:
			++enemy_terran_unit_count;
			break;
		case BWAPI::Races::Enum::Protoss:
			++enemy_protos_unit_count;
			break;
		case BWAPI::Races::Enum::Zerg:
			++enemy_zerg_unit_count;
			break;
		}

		//军事力量
		if (!type.isWorker() && !type.isBuilding() && type.canAttack()) {
			if (type.isFlyer()) {
				enemy_air_army_supply += type.supplyRequired();
			}
			else {
				enemy_ground_army_supply += type.supplyRequired();
				switch (type.getID())
				{
				case BWAPI::UnitSizeTypes::Enum::Large:
					enemy_ground_large_army_supply += type.supplyRequired();
					break;
				case BWAPI::UnitSizeTypes::Enum::Small:
					enemy_ground_small_army_supply += type.supplyRequired();
					break;
				}
			}
			enemy_army_supply += type.supplyRequired();
			if (type.airWeapon())
				enemy_anti_air_army_supply += type.supplyRequired();
			if (type.isOrganic())
				enemy_biological_army_supply += type.supplyRequired();

		}
		switch (type.getID())
		{
		case BWAPI::UnitTypes::Enum::Terran_Missile_Turret:
			++enemy_static_defence_count;
			break;
		case BWAPI::UnitTypes::Enum::Protoss_Photon_Cannon:
			++enemy_static_defence_count;
			break;
		case BWAPI::UnitTypes::Enum::Zerg_Spore_Colony:
			++enemy_static_defence_count;
			break;
		case BWAPI::UnitTypes::Enum::Zerg_Sunken_Colony:
			++enemy_static_defence_count;
			break;

		}
		if (type.isWorker())
			++enemy_worker_count;
		else if (type.isResourceDepot())
			++enemy_base_count;
	}
	enemy_army_supply /= 2;
	enemy_air_army_supply /= 2;
	enemy_ground_army_supply /= 2;
	enemy_ground_large_army_supply /= 2;
	enemy_ground_small_army_supply /= 2;
	enemy_anti_air_army_supply /= 2;
	enemy_biological_army_supply /= 2;
	enemy_attacking_army_supply /= 2;

	if (lurker_completed >= 2 || hydralisk_completed >= 9 || mutalisk_completed >= 6)
	{
		being_rushed = false;
	}
	else if (!being_rushed)
	{
		being_rushed = beingMarineRushed() || beingZealotRushed();
		if (being_rushed)
		{
			BWAPI::Broodwar->printf("BEING RUSHED!!!");
			BWAPI::Broodwar->printf("BEING RUSHED!!!");
			BWAPI::Broodwar->printf("BEING RUSHED!!!");
		}
	}

	being_cannon = Config::AntiCannon::CannonProtoss.find(BWAPI::Broodwar->enemy()->getName()) != Config::AntiCannon::CannonProtoss.end();
	if (being_cannon && flyer_visit_position.empty())
	{
		const auto myMain = InformationManager::Instance().getMyMainBaseLocation();
		const auto enemyMain = InformationManager::Instance().getEnemyMainBaseLocation();
		const auto enemyNatural = InformationManager::Instance().getEnemyNaturalLocation();
		if (myMain && enemyMain && enemyNatural)
		{
			const auto myMainPos = myMain->getPosition();
			const auto enemyMainPos = enemyMain->getPosition();
			const auto enemyNaturalPos = enemyNatural->getPosition();
			const auto mapCenter = BWAPI::Position(BWAPI::Broodwar->mapWidth() * 16, BWAPI::Broodwar->mapHeight() * 16);

			std::string mapName = BWAPI::Broodwar->mapFileName();
			int idx = 0;
			if (!mapName.empty())
			{
				while ((idx = mapName.find(' ')) != std::string::npos)
				{
					mapName.erase(idx, 1);
				}
			}
			std::transform(mapName.begin(), mapName.end(), mapName.begin(), ::tolower);

			BWTA::BaseLocation * enemyThird = nullptr;
			for (const auto & base : BWTA::getBaseLocations())
			{
				if (base == enemyMain || base == enemyNatural) continue;
				if (!enemyThird || enemyThird->getPosition().getDistance(enemyMainPos) > base->getPosition().getDistance(enemyMainPos))
				{
					enemyThird = base;
				}
			}

			if (mapName.find("benzene") != std::string::npos && enemyThird)
			{
				// add third base
				flyer_visit_position.push_back(enemyThird->getPosition());
			}

			if (mapName.find("destination") != std::string::npos
				|| mapName.find("python") != std::string::npos
				|| mapName.find("heartbreakridge") != std::string::npos)
			{
				// do nothing
			}

			if ((mapName.find("taucross") != std::string::npos
				|| mapName.find("aztec") != std::string::npos
				|| mapName.find("fightingspirit") != std::string::npos
				|| mapName.find("jade") != std::string::npos
				|| mapName.find("lamancha") != std::string::npos
				|| mapName.find("electriccircuit") != std::string::npos
				|| mapName.find("fortress") != std::string::npos)
				&& enemyThird)
			{
				double myMain2EnemyNatural = myMainPos.getDistance(enemyNaturalPos);
				double enemyMain2EnemyNatural = enemyMainPos.getDistance(enemyNaturalPos);
				double myMain2EnemyMain = myMainPos.getDistance(enemyMainPos);
				// 钝角
				if (myMain2EnemyNatural * myMain2EnemyNatural + enemyMain2EnemyNatural * enemyMain2EnemyNatural <
					myMain2EnemyMain * myMain2EnemyMain)
				{
					// add third base
					flyer_visit_position.push_back(enemyThird->getPosition());
				}
				else
				{
					// do nothing
				}
			}

			if (mapName.find("neomoonglaive") != std::string::npos && enemyThird)
			{
				double myMain2EnemyNatural = myMainPos.getDistance(enemyNaturalPos);
				double enemyMain2EnemyNatural = enemyMainPos.getDistance(enemyNaturalPos);
				double myMain2EnemyMain = myMainPos.getDistance(enemyMainPos);
				// 钝角
				if (myMain2EnemyNatural * myMain2EnemyNatural + enemyMain2EnemyNatural * enemyMain2EnemyNatural <
					myMain2EnemyMain * myMain2EnemyMain)
				{
					// add center
					flyer_visit_position.push_back((enemyMainPos + enemyThird->getPosition()) / 2);
				}
				else
				{
					// do nothing
				}
			}

			if ((mapName.find("roadrunner") != std::string::npos
				|| mapName.find("icarus") != std::string::npos)
				&& enemyThird)
			{
				double myMain2EnemyNatural = myMainPos.getDistance(enemyNaturalPos);
				double enemyMain2EnemyNatural = enemyMainPos.getDistance(enemyNaturalPos);
				double myMain2EnemyMain = myMainPos.getDistance(enemyMainPos);
				// cross
				auto mid = (myMainPos + enemyMainPos) / 2;
				if (mid.getDistance(mapCenter) < 300)
				{
					// do nothing
				}
				// 钝角
				else if (myMain2EnemyNatural * myMain2EnemyNatural + enemyMain2EnemyNatural * enemyMain2EnemyNatural <
					myMain2EnemyMain * myMain2EnemyMain)
				{
					// add third
					flyer_visit_position.push_back(enemyThird->getPosition());
				}
				else
				{
					// do nothing
				}
			}

			if (mapName.find("andromeda") != std::string::npos && enemyThird)
			{
				const auto enemyThirdPos = enemyThird->getPosition();
				// vertical
				if (abs(myMainPos.x - enemyMainPos.x) < 800)
				{
					// add edge
					flyer_visit_position.push_back(
					BWAPI::Position(enemyThirdPos.x < 2000 ? 1 : (BWAPI::Broodwar->mapWidth() - 1) * 32, enemyThirdPos.y));
				}
				// horizon
				else if (abs(myMainPos.y - enemyMainPos.y) < 800)
				{
					// do nothing
				}
				// cross
				else
				{
					// add edge
					flyer_visit_position.push_back(
						BWAPI::Position(enemyThirdPos.x < 2000 ? 1 : (BWAPI::Broodwar->mapWidth() - 1) * 32, enemyThirdPos.y));
				}
			}

			if (mapName.find("circuitbreaker") != std::string::npos && enemyThird)
			{
				// vertical
				if (abs(myMainPos.x - enemyMainPos.x) < 800)
				{
					// add center
					flyer_visit_position.push_back((mapCenter + enemyThird->getPosition()) / 2);
					// add third base
					flyer_visit_position.push_back(enemyThird->getPosition());
				}
				// horizon
				else if (abs(myMainPos.y - enemyMainPos.y) < 800)
				{
					// do nothing
				}
				// cross
				else
				{
					// add third base
					flyer_visit_position.push_back(enemyThird->getPosition());
				}
			}

			if (mapName.find("empireofthesun") != std::string::npos && enemyThird)
			{
				// vertical
				if (abs(myMainPos.x - enemyMainPos.x) < 800)
				{
					// do nothing
				}
				// horizon
				else if (abs(myMainPos.y - enemyMainPos.y) < 800)
				{
					// add center
					flyer_visit_position.push_back((mapCenter + enemyThird->getPosition()) / 2);
					// add third base
					flyer_visit_position.push_back(enemyThird->getPosition());
				}
				// cross
				else
				{
					// add third base
					flyer_visit_position.push_back(enemyThird->getPosition());
				}
			}

			// last visit enemy Main
			flyer_visit_position.push_back(enemyMain->getPosition());
		}
	}
	if (!flyer_visit_position.empty())
	{
		BWAPI::Broodwar->drawLineMap(BWAPI::Position(BWAPI::Broodwar->self()->getStartLocation()), flyer_visit_position[0], BWAPI::Colors::Cyan);
		for (int i = 0; i < flyer_visit_position.size() - 1; ++i)
		{
			BWAPI::Broodwar->drawLineMap(flyer_visit_position[i], flyer_visit_position[i + 1], BWAPI::Colors::Cyan);
		}
	}

	if (mutalisk_completed >= 12) keep_build_sunken = false;

	auto base = InformationManager::Instance().getMyMainBaseLocation();
	if (base)
	{
		int enemyInBase = 0;
		for (const auto & uinfo : InformationManager::Instance().getUnitInfo(BWAPI::Broodwar->enemy()))
		{
			if (uinfo.second.lastPosition.getDistance(base->getPosition()) < 256)
			{
				++enemyInBase;
			}
		}
		int ratio = 0;
		switch (BWAPI::Broodwar->enemy()->getRace().getID())
		{
		case BWAPI::Races::Enum::Terran:
			ratio = 4;
			break;
		case BWAPI::Races::Enum::Zerg:
			ratio = 6;
			break;
		case BWAPI::Races::Enum::Protoss:
			ratio = 2;
			break;
		default:
			ratio = 2;
		}
		if (enemyInBase - sunken_colony_completed * ratio >= 6)
		{
			base_dangerous = true;
		}
	}

	auto natural = InformationManager::Instance().getMyNaturalLocation();
	if (natural)
	{
		int enemyInNatural = 0;
		for (const auto & uinfo : InformationManager::Instance().getUnitInfo(BWAPI::Broodwar->enemy()))
		{
			if (uinfo.second.lastPosition.getDistance(natural->getPosition()) < 256)
			{
				++enemyInNatural;
			}
		}
		if (enemyInNatural > 12)
		{
			natural_dangerous = true;
		}
	}

	auto enemyBase = InformationManager::Instance().getEnemyMainBaseLocation();
	if (enemyBase && !cannon_in_enemy_base)
	{
		for (const auto & uinfo : InformationManager::Instance().getUnitInfo(BWAPI::Broodwar->enemy()))
		{
			if (uinfo.second.type == BWAPI::UnitTypes::Protoss_Photon_Cannon &&
				uinfo.second.lastPosition.getDistance(enemyBase->getPosition()) < 350)
			{
				cannon_in_enemy_base = true;
			}
		}
	}
}

bool StateManager::beingMarineRushed()
{
	auto enemyName = BWAPI::Broodwar->enemy()->getName();
	if (enemyName == "UAlbertaBot")
	{
		int frameCount = BWAPI::Broodwar->getFrameCount();
		int playerCount = BWAPI::Broodwar->getStartLocations().size();
		int frameLimit = 2800;
		if (playerCount == 3)
		{
			frameLimit = 3500;
		}
		else if (playerCount == 4)
		{
			frameLimit = 4000;
		}
		return (frameCount <= frameLimit && enemy_barrack_count >= 2 && enemy_refinery == 0);
	}
	return false;
}

bool StateManager::beingZealotRushed()
{
	auto enemyName = BWAPI::Broodwar->enemy()->getName();
	if (enemyName == "UAlbertaBot")
	{
		int frameCount = BWAPI::Broodwar->getFrameCount();
		int playerCount = BWAPI::Broodwar->getStartLocations().size();
		int frameLimit = 2800;
		if (playerCount == 3)
		{
			frameLimit = 3500;
		}
		else if (playerCount == 4)
		{
			frameLimit = 4000;
		}
		return (frameCount <= frameLimit && enemy_gateway_count >= 2 && enemy_assimilator == 0 && enemy_forge == 0);
	}
	return false;
}

bool StateManager::beingZerglingRushed()
{
	int frameCount = BWAPI::Broodwar->getFrameCount();
	return (frameCount <= 3200 && enemy_zergling_count >= 6 && zergling_completed < 6)
		|| (frameCount > 3200 && (zergling_completed == 0
			|| (double)enemy_zergling_count / (double)zergling_completed >= 2.5));
}