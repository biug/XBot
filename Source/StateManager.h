#pragma once
#include "BuildOrderQueue.h"
#include "BuildingManager.h"

namespace XBot
{
	struct StateManager
	{
		//己方已生成单位
		int larva_count = 0;						//幼虫
		int drone_count = 0;						//工蜂
		int zergling_count = 0;						//小狗
		int hydralisk_count = 0;					//刺蛇
		int lurker_count = 0;						//地刺
		int ultralisk_count = 0;					//雷兽
		int defiler_count = 0;						//蝎子
		int overlord_count = 0;						//领主
		int mutalisk_count = 0;						//飞龙
		int scourge_count = 0;						//自爆蚊
		int queen_count = 0;						//女王
		int guardian_count = 0;						//守卫者
		int devourer_count = 0;						//吞噬者

													//己方已完成单位
		int larva_completed = 0;						//幼虫
		int drone_completed = 0;						//工蜂
		int zergling_completed = 0;						//小狗
		int hydralisk_completed = 0;					//刺蛇
		int lurker_completed = 0;						//地刺
		int ultralisk_completed = 0;					//雷兽
		int defiler_completed = 0;						//蝎子
		int overlord_completed = 0;						//领主
		int mutalisk_completed = 0;						//飞龙
		int scourge_completed = 0;						//自爆蚊
		int queen_completed = 0;						//女王
		int guardian_completed = 0;						//守卫者
		int devourer_completed = 0;						//吞噬者

														//己方队列中单位
		int larva_in_queue = 0;                      //队列中幼虫
		int drone_in_queue = 0;                      //队列中工蜂
		int zergling_in_queue = 0;                   //队列中小狗
		int hydralisk_in_queue = 0;                  //队列中刺蛇
		int lurker_in_queue = 0;                     //队列中地刺
		int ultralisk_in_queue = 0;                  //队列中雷兽
		int defiler_in_queue = 0;                    //队列中蝎子
		int overlord_in_queue = 0;                   //队列中领主
		int mutalisk_in_queue = 0;                   //队列中飞龙
		int scourge_in_queue = 0;                    //队列中自爆蚊
		int queen_in_queue = 0;                      //队列中女王
		int guardian_in_queue = 0;                   //队列中守卫者
		int devourer_in_queue = 0;                   //队列中吞噬者

													 //己方单位总计
		int larva_total = 0;						//幼虫
		int drone_total = 0;						//工蜂
		int zergling_total = 0;						//小狗
		int hydralisk_total = 0;					//刺蛇
		int lurker_total = 0;						//地刺
		int ultralisk_total = 0;					//雷兽
		int defiler_total = 0;						//蝎子
		int overlord_total = 0;						//领主
		int mutalisk_total = 0;						//飞龙
		int scourge_total = 0;						//自爆蚊
		int queen_total = 0;						//女王
		int guardian_total = 0;						//守卫者
		int devourer_total = 0;						//吞噬者

													// 科技
		int muscular_arguments_total = 0;
		int grooved_spines_total = 0;
		int metabolic_boost_total = 0;
		int lurker_aspect_total = 0;
		int adrenal_glands_total = 0;
		int melee_attacks_total = 0;
		int missile_attacks_total = 0;
		int ground_carapace_total = 0;
		int flyer_attacks_total = 0;
		int flyer_carapace_total = 0;

		int muscular_arguments_in_queue = 0;
		int grooved_spines_in_queue = 0;
		int metabolic_boost_in_queue = 0;
		int lurker_aspect_in_queue = 0;
		int adrenal_glands_in_queue = 0;
		int melee_attacks_in_queue = 0;
		int missile_attacks_in_queue = 0;
		int ground_carapace_in_queue = 0;
		int flyer_attacks_in_queue = 0;
		int flyer_carapace_in_queue = 0;

		//己方已建造建筑
		int base_count = 0;							//基地
		int hatchery_count = 0;						//母巢
		int lair_count = 0;							//蜂房
		int hive_count = 0;							//兽穴
		int extractor_count = 0;                    //气矿
		int creep_colony_count = 0;                 //殖体
		int sunken_colony_count = 0;			    //地刺塔
		int spore_colony_count = 0;			    	//孢子塔
		int spawning_pool_count = 0;                //血池
		int evolution_chamber_count = 0;			//进化腔
		int hydralisk_den_count = 0;                //刺蛇洞穴
		int queens_nest_count = 0;                  //皇后巢
		int defiler_mound_count = 0;                //蝎子巢
		int spire_count = 0;                        //飞龙塔
		int greater_spire_count = 0;                //大飞龙塔
		int nydus_canal_count = 0;                  //蛹虫通道
		int ultralisk_cavern_count = 0;             //巨兽之窟

													//己方已建成建筑
		int base_completed = 0;							//基地
		int hatchery_completed = 0;						//母巢
		int lair_completed = 0;							//蜂房
		int hive_completed = 0;							//兽穴
		int extractor_completed = 0;                    //气矿
		int creep_colony_completed = 0;                 //殖体
		int sunken_colony_completed = 0;			    //地刺塔
		int spore_colony_completed = 0;			    	//孢子塔
		int spawning_pool_completed = 0;                //血池
		int evolution_chamber_completed = 0;			//进化腔
		int hydralisk_den_completed = 0;                //刺蛇洞穴
		int queens_nest_completed = 0;                  //皇后巢
		int defiler_mound_completed = 0;                //蝎子巢
		int spire_completed = 0;                        //飞龙塔
		int greater_spire_completed = 0;                //大飞龙塔
		int nydus_canal_completed = 0;                  //蛹虫通道
		int ultralisk_cavern_completed = 0;             //巨兽之窟

														//己方建设中建筑
		int base_being_built = 0;						//基地
		int hatchery_being_built = 0;					//母巢
		int lair_being_built = 0;						//蜂房
		int hive_being_built = 0;						//兽穴
		int extractor_being_built = 0;                  //气矿
		int creep_colony_being_built = 0;               //殖体
		int sunken_colony_being_built = 0;			    //地刺塔
		int spore_colony_being_built = 0;			   	//孢子塔
		int spawning_pool_being_built = 0;              //血池
		int evolution_chamber_being_built = 0;			//进化腔
		int hydralisk_den_being_built = 0;              //刺蛇洞穴
		int queens_nest_being_built = 0;                //皇后巢
		int defiler_mound_being_built = 0;              //蝎子巢
		int spire_being_built = 0;                      //飞龙塔
		int greater_spire_being_built = 0;              //大飞龙塔
		int nydus_canal_being_built = 0;                //蛹虫通道
		int ultralisk_cavern_being_built = 0;           //巨兽之窟

														//己方队列中建筑
		int base_in_queue = 0;						//基地
		int hatchery_in_queue = 0;					//母巢
		int lair_in_queue = 0;						//蜂房
		int hive_in_queue = 0;						//兽穴
		int extractor_in_queue = 0;                 //气矿
		int creep_colony_in_queue = 0;              //殖体
		int sunken_colony_in_queue = 0;			    //地刺塔
		int spore_colony_in_queue = 0;			    //孢子塔
		int spawning_pool_in_queue = 0;             //血池
		int evolution_chamber_in_queue = 0;			//进化腔
		int hydralisk_den_in_queue = 0;             //刺蛇洞穴
		int queens_nest_in_queue = 0;               //皇后巢
		int defiler_mound_in_queue = 0;             //蝎子巢
		int spire_in_queue = 0;                     //飞龙塔
		int greater_spire_in_queue = 0;             //大飞龙塔
		int nydus_canal_in_queue = 0;               //蛹虫通道
		int ultralisk_cavern_in_queue = 0;          //巨兽之窟

													// 等待中的建筑
		int base_waiting = 0;						//基地
		int hatchery_waiting = 0;					//母巢
		int lair_waiting = 0;						//蜂房
		int hive_waiting = 0;						//兽穴
		int extractor_waiting = 0;                 	//气矿
		int creep_colony_waiting = 0;              	//殖体
		int sunken_colony_waiting = 0;			   	//地刺塔
		int spore_colony_waiting = 0;			   	//孢子塔
		int spawning_pool_waiting = 0;             	//血池
		int evolution_chamber_waiting = 0;			//进化腔
		int hydralisk_den_waiting = 0;             	//刺蛇洞穴
		int queens_nest_waiting = 0;               	//皇后巢
		int defiler_mound_waiting = 0;             	//蝎子巢
		int spire_waiting = 0;                     	//飞龙塔
		int greater_spire_waiting = 0;             	//大飞龙塔
		int nydus_canal_waiting = 0;               	//蛹虫通道
		int ultralisk_cavern_waiting = 0;          	//巨兽之窟

													//己方建筑总计
		int base_total = 0;							//基地
		int hatchery_total = 0;						//母巢
		int lair_total = 0;							//蜂房
		int hive_total = 0;							//兽穴
		int extractor_total = 0;                    //气矿
		int creep_colony_total = 0;                 //殖体
		int sunken_colony_total = 0;			    //地刺塔
		int spore_colony_total = 0;			    	//孢子塔
		int spawning_pool_total = 0;                //血池
		int evolution_chamber_total = 0;			//进化腔
		int hydralisk_den_total = 0;                //刺蛇洞穴
		int queens_nest_total = 0;                  //皇后巢
		int defiler_mound_total = 0;                //蝎子巢
		int spire_total = 0;                        //飞龙塔
		int greater_spire_total = 0;                //大飞龙塔
		int nydus_canal_total = 0;                  //蛹虫通道
		int ultralisk_cavern_total = 0;             //巨兽之窟

													//军事力量
		int army_supply = 0;
		int air_army_supply = 0;
		int ground_army_supply = 0;

		//敌方
		int enemy_terran_unit_count = 0;
		int enemy_protos_unit_count = 0;
		int enemy_zerg_unit_count = 0;

		//人族单位
		int enemy_marine_count = 0;					//机枪兵
		int enemy_firebat_count = 0;				//火焰兵
		int enemy_medic_count = 0;					//医疗兵
		int enemy_ghost_count = 0;					//幽灵
		int enemy_vulture_count = 0;				//雷车
		int enemy_tank_count = 0;					//坦克
		int enemy_goliath_count = 0;				//机器人
		int enemy_wraith_count = 0;					//隐飞
		int enemy_valkyrie_count = 0;				//导弹护卫舰
		int enemy_bc_count = 0;						//战列巡洋舰
		int enemy_science_vessel_count = 0;			//科学球
		int enemy_dropship_count = 0;				//运输机

													//人族建筑
		int enemy_supply_depot = 0;					//基地
		int enemy_refinery = 0;						//气矿
		int enemy_bunker_count = 0;					//地堡
		int enemy_barrack_count = 0;				//兵营
		int enemy_factory_count = 0;				//工厂
		int enemy_starport_count = 0;				//飞机场

													//神族单位
		int enemy_zealot_count = 0;					//狂热者
		int enemy_dragoon_count = 0;				//龙骑
		int enemy_ht_count = 0;						//光明圣堂
		int enemy_dt_count = 0;						//黑暗圣堂
		int enemy_reaver_count = 0;					//金甲虫
		int enemy_shuttle_count = 0;				//运输机
		int enemy_carrier_count = 0;				//航母
		int enemy_arbiter_count = 0;				//仲裁者
		int enemy_corsair_count = 0;				//海盗船
		int enemy_scout_count = 0;					//侦察机
		int enemy_archon_count = 0;					//光明执政官
		int enemy_dark_archon_count = 0;			//黑暗执政官

													//神族建筑
		int enemy_cannon_count = 0;					//光子炮塔
		int enemy_assimilator = 0;					//气矿
		int enemy_forge = 0;						//锻炉
		int enemy_gateway_count = 0;				//兵营
		int enemy_stargate_count = 0;				//星门
		int enemy_robotics_facility_count = 0;		//机械工厂

													//虫族单位
		int enemy_zergling_count = 0;				//小狗
		int enemy_hydralisk_count = 0;				//刺蛇
		int enemy_lurker_count = 0;					//地刺
		int enemy_ultralisk_count = 0;				//雷兽
		int enemy_defiler_count = 0;				//蝎子
		int enemy_mutalisk_count = 0;				//飞龙
		int enemy_queen_count = 0;					//女王
		int enemy_scourge_count = 0;				//自爆蚊
		int enemy_guardian_count = 0;				//空中卫士
		int enemy_devourer_count = 0;				//吞噬者

													//虫族建筑
		int enemy_spawning_pool_count = 0;			//孵化池
		int enemy_hydralisk_den_count = 0;			//刺蛇穴
		int enemy_evolution_chamber_count = 0;		//进化腔
		int enemy_spire_count = 0;					//飞龙塔

													//军事力量
		int enemy_army_supply = 0;
		int enemy_air_army_supply = 0;
		int enemy_ground_army_supply = 0;
		int enemy_ground_large_army_supply = 0;
		int enemy_ground_small_army_supply = 0;
		int enemy_anti_air_army_supply = 0;
		int enemy_biological_army_supply = 0;
		int enemy_attacking_army_supply = 0;
		int enemy_static_defence_count = 0;
		int enemy_proxy_building_count = 0;
		int enemy_attacking_worker_count = 0;
		int enemy_cloaked_unit_count = 0;

		//通用建筑和单位
		int enemy_base_count = 0;
		int enemy_worker_count = 0;

		//判断rush
		bool being_rushed = false;

		//判断神族堵口
		bool being_cannon = false;
		std::vector<BWAPI::Position> flyer_visit_position;
		std::hash_map<BWAPI::Unit, std::vector<bool>> flyer_visit;

		//判断人族堵口
		bool being_terran_choke = false;
		BWAPI::TilePosition anti_terran_choke_pos = BWAPI::TilePositions::None;

		bool ignore_scout_worker = false;

		bool keep_build_sunken = false;
		
		bool base_dangerous = false;
		bool natural_dangerous = false;

		bool cannon_in_enemy_base = false;

		bool rally_at_natural = false;

		void updateCurrentState(BuildOrderQueue &queue);

		bool beingMarineRushed();
		bool beingZealotRushed();
		bool beingZerglingRushed();

		static StateManager & Instance();
	};
}