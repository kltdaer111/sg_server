#include "fight_manager.hpp"
#include "hero/hero_manager.hpp"
#include "monster/monster_manager.hpp"
#include "role/role_manager.hpp"
#include "task/task_manager.hpp"
#include "config_mgr.h"
#include "common/global_id.hpp"
#include "tblh/Monster.tbls.h"
#include "config/config_manager.hpp"
#include "monster/monster_define.hpp"
#include "field_boss/field_boss_manager.hpp"
#include "tblh/attr_enum.hpp"
#include "tblh/HeroStar.tbls.h"
#include "tblh/HeroGrade.tbls.h"
#include "tblh/Hero.tbls.h"
#include "item/drop_manager.hpp"
#include "tblh/MonsterBaseAttrTable.tbls.h"
#include "pk/pk_manager.hpp"
#include "tblh/Comprehensive.tbls.h"
#include "sys_notice/sys_notice_manager.hpp"
#include "tower/tower_manager.hpp"
#include "user_troop/user_troop_mgr.hpp"
#include "tblh/errcode_enum.hpp"
#include "tblh/LifeStar.tbls.h"
#include "common/MyTuple.h"
#include "lifeforce/lifeforce_attr_manager.hpp"
#include "dungeon/dungeon_common.hpp"
#include "achieve/achieve_common.hpp"
#include "scene/scene_manager.hpp"
#include "family/family_war_scene.hpp"
#include "tblh/LevelupTable.tbls.h"
#include "log/log_wrapper.hpp"
#include "multi_time/multi_time_manager.hpp"
#include "tblh/OfflineArenaClass.tbls.h"
#include "page/page_manager.hpp"

fight_manager_t::combat_map fight_manager_t::m_combats;
std::set<uint64_t> fight_manager_t::m_remove_set;
fight_manager_t::combat_round_callback_map fight_manager_t::m_combat_round_callback;
fight_manager_t::combat_result_callback_map fight_manager_t::m_combat_result_callback;
fight_manager_t::combat_end_notify_map fight_manager_t::m_combat_end_notify_map;

uint32_t fight_manager_t::create_combat_count = 0;
uint32_t fight_manager_t::remove_combat_count = 0;

fight_manager_t::fight_manager_t()
{
	
}

fight_manager_t::~fight_manager_t()
{
	close();
}

bool fight_manager_t::init()
{
	regist_callback();
	return true;
}

void fight_manager_t::close()
{
	//check_remove();
	remove_all();
}

bool fight_manager_t::enter_fight_request(role_ptr p_role, const proto::client::cg_enter_fight_request& msg)
{
	if (p_role == NULL)
	{
		log_error("enter_fight_request p_role is null");
		return false;
	}
	uint64_t obj_id = string_util_t::string_to_uint64(msg.object_id());
	uint32_t reply_code = 0;
	switch (msg.object_type())
	{
	case proto::common::SCENEOBJECT_USER:
		{
			do 
			{
				role_ptr p_target = role_manager_t::find_role(obj_id);
				if (p_target == NULL)
				{
					log_error("enter_fight_request not found p_target[%lu]", obj_id);
					reply_code = 1;
					break;
				}

				auto p_conf = GET_CONF(Comprehensive, comprehensive_common::enter_thundermonster_range);
				uint32_t fight_radius = GET_COMPREHENSIVE_VALUE_1(p_conf);

				if (!p_role->is_nearby_object(p_target, fight_radius))
				{
					log_error("role[%lu] too far away about target [%lu]", p_role->get_uid(), obj_id);
					reply_code = 1;
					break;
				}

				if (!fight_pvp(p_role, p_target))
				{
					log_error("role[%lu] fight target [%lu] fail", p_role->get_uid(), obj_id);
					reply_code = 1;
					break;
				}
			} while (0);
		}
		break;
	case proto::common::SCENEOBJECT_MONSTER: // 明雷
		{
			do 
			{
				monster_ptr p_monster = monster_manager_t::find_monster(obj_id);
				if (p_monster == NULL)
				{
					log_error("enter_fight_request not found p_monster[%lu]", obj_id);
					reply_code = 1;
					break;
				}

				if (p_monster->is_die())
				{
					log_error("enter_fight_request p_monster[%lu] is die", obj_id);
					reply_code = errcode_enum::sys_notice_object_dead;
					break;
				}

				// 独占的，并且已经在战斗
				if (p_monster->has_flag(monster_flag::MF_EXCLUSIVE_MODE) && p_monster->get_object_state() == proto::common::object_state_fighting)
				{
					reply_code = errcode_enum::notice_exclusive_mode;
					break;
				}

				auto p_conf = GET_CONF(Comprehensive, comprehensive_common::enter_thundermonster_range);
				uint32_t fight_radius = GET_COMPREHENSIVE_VALUE_1(p_conf);

				if (!p_role->is_nearby_object(p_monster, fight_radius))
				{
					log_error("role[%lu] too far away about monser [%lu]", p_role->get_uid(), obj_id);
					reply_code = errcode_enum::notice_fight_target_too_farawary;
					break;
				}

				if (p_monster->fight_with_role(p_role) == 0)
				{
					log_error("role[%lu] fight monser [%lu] fail", p_role->get_uid(), obj_id);
					reply_code = 1;
					break;
				}
			} while (0);
		}
		break;

	default:
		reply_code = 1;
		break;
	}

	if (reply_code != 0)
	{
		proto::client::gc_enter_fight_reply reply;
		reply.set_reply_code(reply_code);
		p_role->send_msg_to_client(op_cmd::gc_enter_fight_reply, reply);
	}

	return true;
}

// bool fight_manager_t::fight_pve_c(role_ptr p_role, uint64_t against_id)
// {
// 	if (p_role == NULL)
// 	{
// 		log_error("p_role is null");
// 		return false;
// 	}
// 
// 	bool ret = true;
// 	proto::client::gc_enter_fight_notify fdn;
// 	proto::common::fight_data* fd = fdn.mutable_data();
// 	proto::common::fight_common* fc = fd->mutable_common();
// 	fc->set_fight_id(string_util_t::uint64_to_string(gen_fight_uid()));
// 
// 	formation_ptr form = p_role->m_formation_mgr.get_cur_formation();
// 	if (form == NULL)
// 	{
// 		log_error("form is null");
// 		return false;
// 	}
// 
// 	// 自己
// 	ret &= fill_fight_camp(proto::common::combat_camp_1, CAMP_SELF_UNIQUE_OFFSET, p_role, form, fd->mutable_self());
// 	// 敌人
// 	ret &= fill_fight_camp(proto::common::combat_camp_2, against_id, p_role->get_level(), fd->mutable_enemy());
// 
// 	if (ret)
// 	{
// 		p_role->send_msg_to_client(op_cmd::gc_enter_fight_notify, fdn);
// 		p_role->set_fight_data(fdn.data());
// 	}
// 
// 	return ret;
// }

// bool fight_manager_t::fight_pve_c(role_ptr p_role, monster_ptr p_monster)
// {
// 	if (p_role == NULL)
// 	{
// 		log_error("p_role is null");
// 		return false;
// 	}
// 
// 	if (p_monster == NULL)
// 	{
// 		log_error("p_monster is null");
// 		return false;
// 	}
// 
// 	bool ret = true;
// 	proto::client::gc_enter_fight_notify fdn;
// 	proto::common::fight_data* fd = fdn.mutable_data();
// 	proto::common::fight_common* fc = fd->mutable_common();
// 	fc->set_fight_id(string_util_t::uint64_to_string(gen_fight_uid()));
// 
// 	formation_ptr form = p_role->m_formation_mgr.get_cur_formation();
// 	if (form == NULL)
// 	{
// 		log_error("form is null");
// 		return false;
// 	}
// 
// 	// 自己
// 	ret &= fill_fight_camp(proto::common::combat_camp_1, CAMP_SELF_UNIQUE_OFFSET, p_role, form, fd->mutable_self());
// 	// 敌人
// 	ret &= fill_fight_camp(proto::common::combat_camp_2, CAMP_ENEMY_UNIQUE_OFFSET, p_monster, p_role->get_level(), fd->mutable_enemy());
// 
// 	if (ret)
// 	{
// 		p_role->send_msg_to_client(op_cmd::gc_enter_fight_notify, fdn);
// 		p_role->set_fight_data(fdn.data());
// 	}
// 
// 	return ret;
// }

uint64_t fight_manager_t::fight_pve(role_ptr p_role, uint64_t against_id, proto::common::fight_param* param)
{
	if (p_role == NULL)
	{
		log_error("p_role is null");
		return 0;
	}

	if (!p_role->can_enter_fight())
	{
		log_warn("role is fighting %lu", p_role->get_uid());
		return 0;
	}

	FightAgainst* against_conf = GET_CONF(FightAgainst, against_id);
	if (against_conf == NULL)
	{
		log_error("against id [%lu] not exists", against_id);
		return false;
	}

	bool ret = true;
	uint64_t fight_uid = gen_fight_uid();
	uint32_t level = 0;

	proto::common::fight_data fd;
	proto::common::fight_common* fc = fd.mutable_common();
	fc->set_fight_id(string_util_t::uint64_to_string(fight_uid));
	fc->set_fight_initiator(string_util_t::uint64_to_string(p_role->get_uid()));
	fc->mutable_param()->set_prepare_time(against_conf->prepare_time());

	proto::common::hero_state_data* self_state = NULL;
	proto::common::hero_state_data* enemy_state = NULL;
	if (NULL != param)
	{
		fc->mutable_param()->CopyFrom(*param);
		self_state = param->mutable_start()->mutable_self();
		enemy_state = param->mutable_start()->mutable_enemy();
	}
	fc->mutable_param()->set_against_id(against_id);
	uint32_t max_rounds = against_conf->max_rounds();
	if (max_rounds == 0)
	{
		auto max_rounds_conf = GET_CONF(Comprehensive, comprehensive_common::MAX_ROUND);
		max_rounds = GET_COMPREHENSIVE_VALUE_1(max_rounds_conf);
	}
	fc->mutable_param()->set_max_rounds(max_rounds);

	std::vector<role_ptr> temp_member_list;
	formation_ptr form = formation_ptr();
	// 判断组队战斗还是个人战斗 获取队友
	if (!fight_manager_t::check_fill_team_member_form_data(p_role, fc->param().type(), level, form, temp_member_list))
	{
		log_error("fill_team_member_data fail");
		return 0;
	}

	if (form == NULL)
	{
		log_error("form is null");
		return 0;
	}

	combat_type type = combat_type_single_role;
	if (temp_member_list.size() > 0)
	{
		type = combat_type_troop;
		fc->set_self_position_id(against_conf->troop_combat_position_id());
	}
	else
	{
		type = combat_type_single_role;
		fc->set_self_position_id(against_conf->combat_position_id());
	}
	fc->set_enemy_position_id(against_conf->enemy_combat_position_id());


	// 自己
	ret &= fill_fight_camp(proto::common::combat_camp_1, CAMP_SELF_UNIQUE_OFFSET, p_role, form, fd.mutable_self(),
							fc->param().type(), self_state);
	// 可能的己方队友(怪)
	ret &= fill_fight_camp(proto::common::combat_camp_1, against_conf, level, fd.mutable_self(), NULL, p_role->get_uid());

	// 填充队友的战斗数据
	for (uint32_t i = 0; i < temp_member_list.size(); ++i)
	{
		role_ptr p_member = temp_member_list[i];
		if (NULL == p_member || p_member->get_uid() == p_role->get_uid())
			continue;
		ret &= fill_fight_camp(proto::common::combat_camp_1, CAMP_SELF_UNIQUE_OFFSET, p_member, form, 
								fd.mutable_self(), fc->param().type(), self_state);
	}

	// 敌人
	ret &= fill_fight_camp(proto::common::combat_camp_2, against_conf, level, fd.mutable_enemy(), enemy_state);

	ret &= create_combat(p_role, &temp_member_list, fight_uid, fd, against_conf->can_form(), type);

	if (!ret)
	{
		return 0;
	}
	return fight_uid;
}

uint64_t fight_manager_t::fight_pve(role_ptr p_role, const proto::common::mirror_fight_data& npc, proto::common::fight_param* param /*= NULL*/)
{
	if (p_role == NULL)
	{
		log_error("p_role is null");
		return 0;
	}

	if (!p_role->can_enter_fight())
	{
		log_warn("role is fighting %lu", p_role->get_uid());
		return 0;
	}
	bool ret = true;
	uint64_t fight_uid = gen_fight_uid();

	proto::common::fight_data fd;
	proto::common::fight_common* fc = fd.mutable_common();
	fc->set_fight_id(string_util_t::uint64_to_string(fight_uid));
	fc->set_fight_initiator(string_util_t::uint64_to_string(p_role->get_uid()));

	// 根据不同战斗类型填充站位id
	auto conf = GET_CONF(Comprehensive, comprehensive_common::pvp_combat_position_id);
	uint32_t self_position_id = GET_COMPREHENSIVE_VALUE_1(conf);
	uint32_t enemy_position_id = self_position_id;

	proto::common::hero_state_data* self_state = NULL;
	proto::common::hero_state_data* enemy_state = NULL;

	if (NULL != param)
	{
		fc->mutable_param()->CopyFrom(*param);
		self_state = param->mutable_start()->mutable_self();
		enemy_state = param->mutable_start()->mutable_enemy();
	}
	auto max_rounds_conf = GET_CONF(Comprehensive, comprehensive_common::MAX_ROUND);
	uint32_t max_rounds = GET_COMPREHENSIVE_VALUE_1(max_rounds_conf);
	fc->mutable_param()->set_max_rounds(max_rounds);

	std::vector<role_ptr> temp_member_list;
	formation_ptr form = formation_ptr();
	uint32_t level = 0;

	// 判断组队战斗还是个人战斗 获取队友
	if (!fight_manager_t::check_fill_team_member_form_data(p_role, fc->param().type(), level, form, temp_member_list))
	{
		log_error("fill_team_member_data fail");
		return 0;
	}

	if (form == NULL)
	{
		log_error("form is null");
		return 0;
	}

	combat_type type = combat_type_single_role;
	if (temp_member_list.size() > 0)
	{
		type = combat_type_troop;
		if (fc->param().type() == proto::common::fight_type_expedition)
		{
			auto pos_conf1 = GET_CONF(Comprehensive, comprehensive_common::glory_team_stance);
			auto pos_conf2 = GET_CONF(Comprehensive, comprehensive_common::glory_enemy_stance);
			self_position_id = GET_COMPREHENSIVE_VALUE_1(pos_conf1);
			enemy_position_id = GET_COMPREHENSIVE_VALUE_1(pos_conf2);
		}
	}
	else
	{
		type = combat_type_single_role;
		if (fc->param().type() == proto::common::fight_type_expedition)
		{
			auto pos_conf1 = GET_CONF(Comprehensive, comprehensive_common::glory_single_stance);
			auto pos_conf2 = GET_CONF(Comprehensive, comprehensive_common::glory_enemy_stance);
			self_position_id = GET_COMPREHENSIVE_VALUE_1(pos_conf1);
			enemy_position_id = GET_COMPREHENSIVE_VALUE_1(pos_conf2);
		}
	}
	fc->set_self_position_id(self_position_id);
	fc->set_enemy_position_id(enemy_position_id);
	

	// 自己
	ret &= fill_fight_camp(proto::common::combat_camp_1, CAMP_SELF_UNIQUE_OFFSET, p_role, form, fd.mutable_self(),
							fc->param().type(), self_state);

	// 填充队友的战斗数据
	for (uint32_t i = 0; i < temp_member_list.size(); ++i)
	{
		role_ptr p_member = temp_member_list[i];
		if (NULL == p_member || p_member->get_uid() == p_role->get_uid())
			continue;
		ret &= fill_fight_camp(proto::common::combat_camp_1, CAMP_SELF_UNIQUE_OFFSET, p_member, form,
								fd.mutable_self(), fc->param().type(), self_state);
	}

	// 敌人
	ret &= fill_fight_camp(proto::common::combat_camp_2, CAMP_ENEMY_UNIQUE_OFFSET, npc, fd.mutable_enemy(), enemy_state);

	ret &= create_combat(p_role, &temp_member_list, fight_uid, fd, true, type);

	if (!ret)
	{
		return 0;
	}
	return fight_uid;
}

uint64_t fight_manager_t::fight_pvp(role_ptr p_role, role_ptr p_target, proto::common::fight_param* param)
{
	if (p_role == NULL)
	{
		log_error("p_role is null");
		return 0;
	}

	if (p_target == NULL)
	{
		log_error("p_target is null");
		return 0;
	}

	if (!p_role->can_enter_fight())
	{
		log_warn("role is fighting %lu", p_role->get_uid());
		return 0;
	}
	if (!p_target->can_enter_fight())
	{
		log_warn("target is fighting %lu", p_target->get_uid());
		return 0;
	}

	proto::common::fight_type fight_type = proto::common::fight_type_none;
	if (param != NULL)
		fight_type = param->type();

	check_close_role_watching_state(p_target, fight_type);

	bool ret = true;
	uint64_t fight_uid = gen_fight_uid();

	proto::common::fight_data fd;
	proto::common::fight_common* fc = fd.mutable_common();
	fc->set_fight_id(string_util_t::uint64_to_string(fight_uid));
	fc->set_fight_initiator(string_util_t::uint64_to_string(p_role->get_uid()));

	auto conf = GET_CONF(Comprehensive, comprehensive_common::pvp_combat_position_id);
	uint32_t pvp_combat_position_id = GET_COMPREHENSIVE_VALUE_1(conf);
	fc->set_self_position_id(pvp_combat_position_id);
	fc->set_enemy_position_id(pvp_combat_position_id);

	proto::common::hero_state_data* self_state = NULL;
	proto::common::hero_state_data* enemy_state = NULL;

	if (NULL != param)
	{
		fc->mutable_param()->CopyFrom(*param);
		self_state = param->mutable_start()->mutable_self();
		enemy_state = param->mutable_start()->mutable_enemy();
	}
	auto max_rounds_conf = GET_CONF(Comprehensive, comprehensive_common::MAX_ROUND);
	uint32_t max_rounds = GET_COMPREHENSIVE_VALUE_1(max_rounds_conf);
	fc->mutable_param()->set_max_rounds(max_rounds);

	formation_ptr form = formation_ptr();
	formation_ptr enemy_form = formation_ptr();

	if (fight_type == proto::common::fight_type_family_pk)
	{
		form = p_role->m_formation_mgr.get_formation(proto::common::formation_type_family);
		enemy_form = p_target->m_formation_mgr.get_formation(proto::common::formation_type_family);
	}
	else
	{
		form = p_role->m_formation_mgr.get_cur_formation();
		enemy_form = p_target->m_formation_mgr.get_cur_formation();
	}

	if (form == NULL)
	{
		log_error("form is null");
		return 0;
	}

	if (enemy_form == NULL)
	{
		log_error("enemy_form is null");
		return 0;
	}

	// 自己
	ret &= fill_fight_camp(proto::common::combat_camp_1, CAMP_SELF_UNIQUE_OFFSET, p_role, form, fd.mutable_self(),
							fight_type, self_state);
	// 敌人
	ret &= fill_fight_camp(proto::common::combat_camp_2, CAMP_ENEMY_UNIQUE_OFFSET, p_target, enemy_form, 
							fd.mutable_enemy(), fight_type, enemy_state);

	ret &= create_combat(p_role, NULL, fight_uid, fd);

	if (!ret)
	{
		return 0;
	}
	
	p_target->set_fight_uid(fight_uid);
	return fight_uid;
}

bool fight_manager_t::update_fight(uint64_t fight_uid, const proto::common::fight_state& state)
{
	combat_ptr combat = get_combat(fight_uid);
	if (NULL == combat)
	{
		return false;
	}

	return combat->update_fight(state);
}

bool fight_manager_t::check_close_role_watching_state(role_ptr p_role, proto::common::fight_type fight_type)
{
	if (NULL == p_role)
	{
		log_error("check_close_role_watching_state p_role NULL");
		return false;
	}
	switch (fight_type)
	{
	case proto::common::fight_type_pk:
	case proto::common::fight_type_family_pk:
	case proto::common::fight_type_study:
		break;
	default:
		return true;
	}
	if (p_role->get_watching_fight_uid() > 0)
	{
		p_role->set_watching_fight_uid(0);
	}
	return true;
}

bool fight_manager_t::fill_fight_camp(
	uint32_t camp_index, 
	uint64_t unique_offset, 
	role_ptr p_role, 
	formation_ptr form, 
	proto::common::fight_camp* camp,
	proto::common::fight_type fight_type,
	proto::common::hero_state_data* state)
{
	if (NULL == camp)
	{
		log_error("fill_fight_camp role camp is null");
		return false;
	}

	if (NULL == form)
	{
		log_error("fill_fight_camp form is null");
		return false;
	}

	bool ret = true;
	camp->set_camp(camp_index);
	camp->set_tactic(form->get_tactic_id());
	proto::common::obj_data* od = camp->mutable_objs();
	ret &= fill_obj_single(camp_index, p_role, od->add_obj(), fight_type);
	ret &= fill_hero_data(camp_index, unique_offset, p_role, form, fight_type, camp->mutable_heroes(), state);

	return ret;
}
// 
// bool fight_manager_t::fill_troop_fight_camp(uint32_t camp_index, uint64_t unique_offset, role_ptr p_member, proto::common::fight_camp* camp, proto::common::hero_state_data* state /*= NULL*/)
// {
// 	if (NULL == camp)
// 	{
// 		log_error("fill_fight_camp role camp is null");
// 		return false;
// 	}
// 	if (NULL == p_member)
// 	{
// 		log_error("fill_fight_camp p_member NULL");
// 		return false;
// 	}
// 
// 	formation_ptr form = p_member->m_formation_mgr.get_cur_formation();
// 	if (form == NULL)
// 	{
// 		log_error("form is null");
// 		return false;
// 	}
// 
// 	bool ret = true;
// 	proto::common::obj_data* od = camp->mutable_objs();
// 	ret &= fill_obj_single(camp_index, p_member, od->add_obj());
// 	// todo 2 是允许上阵的英雄数量 需要配置
// 	ret &= fill_troop_hero_data(camp_index, unique_offset, p_member, form, 2, camp->mutable_heroes(), state);
// 
// 	return ret;
// }

bool fight_manager_t::fill_fight_camp(
	uint32_t camp_index, 
	/*uint64_t unique_offset, */
	FightAgainst* conf,
	uint32_t level, 
	proto::common::fight_camp* camp,
	proto::common::hero_state_data* state,
	uint64_t role_uid)
{
	if (NULL == camp)
	{
		log_error("fill_fight_camp camp is null");
		return false;
	}

	if (conf == NULL)
	{
		log_error("fight_manager conf NULL");
		return false;
	}

// 	// 非剧情的对阵 不填充己方阵容数据
// 	if (camp_index == proto::common::combat_camp_1 && confs->is_story() == 0)
// 		return true;

	bool ret = true;
	camp->set_camp(camp_index);
	proto::common::obj_data* od = camp->mutable_objs();

	switch (camp_index)
	{
	case proto::common::combat_camp_1:	// 我方辅助阵
		{
			ret &= fill_hero_data(camp_index, role_uid, CAMP_SELF_HELP_UNIQUE_OFFSET, level, 1, conf->get_friend_against_ids(), camp->mutable_help_heroes(), state);
		}
		break;
	case proto::common::combat_camp_2:	// 敌方主阵和辅助阵
		{
			ret &= fill_obj_single(camp_index, conf->id(), 0, conf->main_hero_tid(), od->add_obj());
			ret &= fill_hero_data(camp_index, conf->id(), CAMP_ENEMY_UNIQUE_OFFSET, level, 0, conf->get_against_ids(), camp->mutable_heroes(), state);
			ret &= fill_hero_data(camp_index, conf->id(), CAMP_ENEMY_HELP_UNIQUE_OFFSET, level, 1, conf->get_against_ids(), camp->mutable_help_heroes());
		}
		break;
	}

	return ret;
}

// bool fight_manager_t::fill_fight_camp(
// 	uint32_t camp_index, 
// 	uint64_t unique_offset, 
// 	monster_ptr p_monster, 
// 	uint32_t level, 
// 	proto::common::fight_camp* camp,
// 	proto::common::hero_state_data* state)
// {
// 	if (p_monster == NULL)
// 	{
// 		log_error("p_monster null error");
// 		return false;
// 	}
// 
// 	if (NULL == camp)
// 	{
// 		log_error("camp is null");
// 		return false;
// 	}
// 
// 	uint64_t againstid = p_monster->get_against_form_id();
// 	FightAgainst* confs = GET_CONF(FightAgainst, againstid);
// 	if (confs == NULL)
// 	{
// 		log_error("against id [%lu] not exists", againstid);
// 		return false;
// 	}
// 
// 	bool ret = true;
// 	camp->set_camp(camp_index);
// 	proto::common::obj_data* od = camp->mutable_objs();
// //	ret &= fill_obj_single(camp_index, p_monster->get_object_id(), p_monster->get_monster_id(), od->add_obj());
// //	ret &= fill_hero_data(camp_index, unique_offset, confs, level, camp->mutable_heroes(), state);
// 	switch (camp_index)
// 	{
// 	case proto::common::combat_camp_1:
// 	{
// 		ret &= fill_hero_data(camp_index, confs->get_id(), unique_offset, level, 0, confs->get_friend_against_ids(), camp->mutable_help_heroes(), state);
// 	}
// 	break;
// 	case proto::common::combat_camp_2:
// 	{
// 		ret &= fill_obj_single(camp_index, p_monster->get_object_id(), p_monster->get_monster_id(), od->add_obj());
// 		ret &= fill_hero_data(camp_index, confs->get_id(), unique_offset, level, 0, confs->get_against_ids(), camp->mutable_heroes(), state);
// 	}
// 	break;
// 	}
// 
// 	return ret;
// }

bool fight_manager_t::fill_fight_camp(
	uint32_t camp_index, 
	uint64_t unique_offset, 
	const proto::common::mirror_fight_data& npc,
	proto::common::fight_camp* camp, 
	proto::common::hero_state_data* state /*= NULL*/)
{
	if (NULL == camp)
	{
		log_error("camp is null");
		return false;
	}

	bool ret = true;
	camp->set_camp(camp_index);
	proto::common::obj_data* od = camp->mutable_objs();
	ret &= fill_obj_single(camp_index, npc.obj(), od->add_obj());
	uint64_t master_id = string_util_t::string_to_uint64(npc.obj().object_uid());
	ret &= fill_hero_data(camp_index, unique_offset, master_id, npc.heros(), camp->mutable_heroes(), state);

	return ret;
}

bool fight_manager_t::fill_obj_single(uint32_t camp_index, role_ptr p_role, proto::common::obj_single* single, proto::common::fight_type fight_type)
{
	if (p_role == NULL)
	{
		log_error("p_role is null");
		return false;
	}
		
	if (single == NULL)
	{
		log_error("single is null");
		return false;
	}

	single->set_object_uid(string_util_t::uint64_to_string(p_role->get_uid()));
	single->set_object_type(p_role->get_object_type());
	single->set_camp(camp_index);
	single->set_name(p_role->get_name());
	single->set_level(p_role->get_level());
	single->set_head_id(p_role->get_main_hero_tid());
	// PVE 保持上次战斗的自动战斗选择
	if (fight_type == proto::common::fight_type_light_monster ||
		fight_type == proto::common::fight_type_against	||
		fight_type == proto::common::fight_type_dark_monster ||
		fight_type == proto::common::fight_type_task ||
		fight_type == proto::common::fight_type_tower ||
		fight_type == proto::common::fight_type_page_pass_pk)
	{
		single->set_auto_fight(p_role->get_auto_fight_flag());
	}

	return true;
}

bool fight_manager_t::fill_obj_single(uint32_t camp_index, uint64_t obj_id, uint32_t monster_id, uint32_t main_hero_tid, proto::common::obj_single* single)
{
	if (single == NULL)
	{
		log_error("single is null");
		return false;
	}

	single->set_object_uid(string_util_t::uint64_to_string(obj_id));
	single->set_object_type(proto::common::SCENEOBJECT_MONSTER);
	single->set_camp(camp_index);
	single->set_auto_fight(1);
	single->set_monster_id(monster_id);	// 0 表示暗雷触发
	single->set_head_id(main_hero_tid);

	return true;
}

bool fight_manager_t::fill_obj_single(uint32_t camp_index, const proto::common::obj_single& obj, proto::common::obj_single* single)
{
	if (single == NULL)
	{
		log_error("single is null");
		return false;
	}

	single->CopyFrom(obj);
	single->set_object_type(proto::common::SCENEOBJECT_MONSTER);
	single->set_camp(camp_index);
	single->set_auto_fight(1);

	return true;
}

bool fight_manager_t::fill_hero_data(
	uint32_t camp_index, 
	uint64_t unique_offset, 
	role_ptr p_role, 
	formation_ptr form,
	proto::common::fight_type fight_type,
	proto::common::hero_data* fhd,
	proto::common::hero_state_data* state)
{
	if (p_role == NULL)
	{
		log_error("p_role null error");
		return false;
	}

	if (form == NULL)
	{
		log_error("form null error");
		return false;
	}

	if (fhd == NULL)
	{
		log_error("fhd null error");
		return false;
	}

	bool ret = true;
	const std::map<uint32_t, uint64_t>& pos_hero = form->get_pos_hero();
	for (auto pos_data : pos_hero)
	{
		uint32_t pos = (pos_data.first);
		uint64_t hero_uid = pos_data.second;

		hero_ptr hero = hero_manager_t::get_hero_by_unique_id(p_role, hero_uid);
		if (hero == NULL)
		{
			// 如果是多人战斗 阵上会有队友的武将
			//log_error("formation [%u] pos [%u] hero [%lu] not exists", form->get_index(), pos, hero_uid);
			continue;
		}

		proto::common::hero_single* single = fhd->add_hero_list();
		if (single == NULL)
		{
			continue;
		}

		proto::common::hero_state* hs = NULL;
		if (NULL != state)
		{
			for (int i = 0; i < state->states_size(); ++i)
			{
				if (state->states(i).pos() == pos)
				{
					hs = state->mutable_states(i);
					break;
				}
			}
		}

		ret &= fill_hero_single(camp_index, unique_offset, hero, single, p_role, pos, fight_type, hs);
	}

	return ret;
}

// bool fight_manager_t::fill_troop_hero_data(
// 	uint32_t camp_index,
// 	uint64_t unique_offset,
// 	role_ptr p_role,
// 	formation_ptr form,
// 	uint32_t hero_num_limit,
// 	proto::common::hero_data* fhd,
// 	proto::common::hero_state_data* state)
// {
// 	if (p_role == NULL)
// 	{
// 		log_error("p_role null error");
// 		return false;
// 	}
// 
// 	if (form == NULL)
// 	{
// 		log_error("form null error");
// 		return false;
// 	}
// 
// 	if (fhd == NULL)
// 	{
// 		log_error("fhd null error");
// 		return false;
// 	}
// 
// 	bool ret = true;
// 	std::map<uint32_t, uint64_t> pos_hero = form->get_pos_hero();
// 	uint32_t count = 0;
// 	for (auto pos_data : pos_hero)
// 	{
// 		if(count >= hero_num_limit)
// 			break;
// 
// 		uint32_t pos = (pos_data.first);
// 		uint64_t hero_uid = pos_data.second;
// 
// 		hero_ptr hero = hero_manager_t::get_hero_by_unique_id(p_role, hero_uid);
// 		if (hero == NULL)
// 		{
// 			// 如果是多人战斗 阵上会有队友的武将
// 			//log_error("formation [%u] pos [%u] hero [%lu] not exists", form->get_index(), pos, hero_uid);
// 			continue;
// 		}
// 
// 		proto::common::hero_single* single = fhd->add_hero_list();
// 		if (single == NULL)
// 		{
// 			continue;
// 		}
// 
// 		proto::common::hero_state* hs = NULL;
// 		if (NULL != state)
// 		{
// 			for (int i = 0; i < state->states_size(); ++i)
// 			{
// 				if (state->states(i).pos() == pos)
// 				{
// 					hs = state->mutable_states(i);
// 					break;
// 				}
// 			}
// 		}
// 
// 		++count;
// 		ret &= fill_hero_single(camp_index, unique_offset, hero, single, p_role->get_uid(), pos, hs);
// 	}
// 
// 	return ret;
// }

bool fight_manager_t::fill_hero_data(
	uint32_t camp_index,
	uint64_t master_id,
	uint64_t unique_offset,
	uint32_t level,
	uint32_t show_type,
	const std::map<uint32_t, uint32_t> &against_map,
	proto::common::hero_data* fhd,
	proto::common::hero_state_data* state)
{
	if (fhd == NULL)
	{
		log_error("fhd null error");
		return false;
	}

	bool ret = true;
	uint32_t pos = 0;

	std::map<uint32_t, uint32_t>::const_iterator it = against_map.begin();
	for (; it != against_map.end(); ++it)
	{
		MonsterHero* conf = GET_CONF(MonsterHero, it->second);
		if (conf == NULL)
		{
			log_error("fight_manager conf NULL id[%d]", it->second);
			continue;
		}
		// 判断阵营
		if(conf->camp() != camp_index)
			continue;
		// 判断是主阵还是辅助阵
		if (show_type == 0 && conf->show_cond() != 0)
			continue;
		else if (show_type != 0 && conf->show_cond() == 0)
			continue;

		pos = it->first;
		
		proto::common::hero_single* single = fhd->add_hero_list();
		if (single == NULL)
		{
			continue;
		}

		proto::common::hero_state* hs = NULL;
		if (NULL != state)
		{
			for (int i = 0; i < state->states_size(); ++i)
			{
				if (state->states(i).pos() == it->first)
				{
					hs = state->mutable_states(i);
					break;
				}
			}
		}

		ret &= fill_hero_single(camp_index, unique_offset, conf, level, single, master_id, pos, hs);
	}

	return ret;
}

bool fight_manager_t::fill_hero_data(
	uint32_t camp_index, 
	uint64_t unique_offset, 
	uint64_t obj_id,
	const proto::common::hero_data& heros, 
	proto::common::hero_data* fhd, 
	proto::common::hero_state_data* state /*= NULL*/)
{
	if (fhd == NULL)
	{
		log_error("fhd null error");
		return false;
	}

	bool ret = true;
	uint32_t pos = 1;
	for (auto hero : heros.hero_list())
	{
		proto::common::hero_single* single = fhd->add_hero_list();
		if (single == NULL)
		{
			continue;
		}

		proto::common::hero_state* hs = NULL;
		if (NULL != state)
		{
			for (int i = 0; i < state->states_size(); ++i)
			{
				if (state->states(i).pos() == hero.fight_attr().pos())
				{
					hs = state->mutable_states(i);
					break;
				}
			}
		}

		ret &= fill_hero_single(camp_index, unique_offset, obj_id, pos++, hero, single, hs);
	}

	return ret;
}

bool fight_manager_t::fill_hero_single(
	uint32_t camp_index, 
	uint64_t unique_offset, 
	hero_ptr p_hero, 
	proto::common::hero_single* single,
	role_ptr p_role,
	uint32_t pos,
	proto::common::fight_type fight_type,
	proto::common::hero_state* state)
{
	if (p_hero == NULL)
	{
		log_error("p_hero null error");
		return false;
	}

	if (single == NULL)
	{
		log_error("single null error");
		return false;
	}
	if (p_role == NULL)
	{
		log_error("p_role null error");
		return false;
	}

	bool ret = true;
	p_hero->peek_data(single, true);
	fill_hero_skill(fight_type, p_role, p_hero, single);

	proto::common::fight_attr* fight = single->mutable_fight_attr();
	fight->set_master(string_util_t::uint64_to_string(p_role->get_uid()));
	fight->set_pos(pos);
	fight->set_camp(camp_index);
	if (NULL != state)
	{
		fight->mutable_start()->CopyFrom(*state);
	}
	
	uint64_t fight_uid = unique_offset + pos;
	proto::common::base_attr* base = single->mutable_base_attr();
	base->set_fight_hero_uid(string_util_t::uint64_to_string(fight_uid)); // 战斗时的唯一ID为起始ID+pos

	if (fight_type == proto::common::fight_type_expedition)
	{
		expedition_ptr p_expedition = p_role->get_expedition();
		if (NULL != p_expedition)
		{
			single->set_save_hp(p_expedition->get_hp(p_hero->get_unique_id()));
		}
	}
	return ret;
}

bool fight_manager_t::fill_hero_single(
	uint32_t camp_index, 
	uint64_t unique_offset,
	MonsterHero* conf, 
	uint32_t level, 
	proto::common::hero_single* single, 
	uint64_t master, 
	uint32_t pos,
	proto::common::hero_state* state)
{
	if (conf == NULL)
	{
		log_error("conf null error");
		return false;
	}

	if (single == NULL)
	{
		log_error("single null error");
		return false;
	}

	bool ret = true;
	ret &= fill_hero_base(conf, single);

	attr_map attrs;
	calc_attr(level, conf, attrs, single);
	ret &= fill_hero_skill(conf, single);

	// 第一波次站位个位数 第二波次10打头 第三波次20打头
	uint32_t wave_num = 1 + pos / 10;

	proto::common::fight_attr* fight = single->mutable_fight_attr();
	fight->set_master(string_util_t::uint64_to_string(master));
	fight->set_pos(pos);
	fight->set_camp(camp_index);
	fight->set_wave_num(wave_num);

	if (NULL != state)
	{
		fight->mutable_start()->CopyFrom(*state);
	}

	proto::common::base_attr* base = single->mutable_base_attr();
	base->set_fight_hero_uid(string_util_t::uint64_to_string(unique_offset + pos)); // 战斗时的唯一ID为起始ID+pos
	return ret;
}

bool fight_manager_t::fill_hero_single(
	uint32_t camp_index, 
	uint64_t unique_offset,
	uint64_t master,
	uint32_t pos,
	const proto::common::hero_single& hero, 
	proto::common::hero_single* single, 
	proto::common::hero_state* state /*= NULL*/)
{
	if (single == NULL)
	{
		log_error("single null error");
		return false;
	}

	bool ret = true;
	single->CopyFrom(hero);

	proto::common::fight_attr* fight = single->mutable_fight_attr();
	fight->set_camp(camp_index);
	fight->set_pos(pos);
	fight->set_master(string_util_t::uint64_to_string(master));
	if (NULL != state)
	{
		fight->mutable_start()->CopyFrom(*state);
	}

	proto::common::base_attr* base = single->mutable_base_attr();
	base->set_fight_hero_uid(string_util_t::uint64_to_string(unique_offset + pos)); // 战斗时的唯一ID为起始ID+pos
	return ret;
}

uint32_t fight_manager_t::calc_attr(hero_ptr p_hero, attr_map& attrs, proto::common::hero_single* single, bool fight)
{
	uint32_t power = 0;
	if (p_hero == NULL)
	{
		log_error("p_hero is null");
		return power;
	}
	attr_map add_attr;
	attr_map percent_attr;
	p_hero->calc_equip_attr(add_attr);
	p_hero->calc_plugin_attr(add_attr);
	p_hero->calc_role_buff_attr(percent_attr);
	power = calc_attr(add_attr, percent_attr, attrs, p_hero->get_tid(), p_hero->get_current_level(), p_hero->get_grade(), p_hero->get_star(), p_hero->get_wakeup(), p_hero->get_role_uid());
	//for (auto it : attrs)
	//{
	//	log_warn("attr[%d:%d]", it.first, it.second);
	//}
	if (single != NULL)
	{
		proto::common::fight_attr* fa = single->mutable_fight_attr();
		fa->Clear();
		
		fill_fight_attr(attrs, fa->mutable_attr());

		if (fight)
		{
			return power;
		}
		//基础属性
		attr_map base_attr;
		calc_base_attr(base_attr, p_hero->get_tid(), p_hero->get_current_level(), p_hero->get_grade(), p_hero->get_star(), p_hero->get_wakeup());
		fill_fight_attr(base_attr, fa->mutable_base_attr());
		//for (auto it : base_attr)
		//{
		//	log_warn("base_attr[%d:%d]", it.first, it.second);
		//}
		//下一等级
		attr_map next_level;
		calc_base_attr(next_level, p_hero->get_tid(), p_hero->get_current_level() + 1, p_hero->get_grade(), p_hero->get_star(), p_hero->get_wakeup());
		fill_fight_attr(next_level, fa->mutable_next_level());
		//for (auto it : next_level)
		//{
		//	log_warn("next level base_attr[%d:%d]", it.first, it.second);
		//}
		//下一品阶
		attr_map next_grade;
		calc_base_attr(next_grade, p_hero->get_tid(), p_hero->get_current_level(), p_hero->get_grade() + 1, p_hero->get_star(), p_hero->get_wakeup());
		fill_fight_attr(next_grade, fa->mutable_next_grade());
		//for (auto it : next_grade)
		//{
		//	log_warn("next grade base_attr[%d:%d]", it.first, it.second);
		//}
		//下一星级
		attr_map next_star;
		calc_base_attr(next_star, p_hero->get_tid(), p_hero->get_current_level(), p_hero->get_grade(), p_hero->get_star() + 1, p_hero->get_wakeup());
		fill_fight_attr(next_star, fa->mutable_next_star());
		//for (auto it : next_star)
		//{
		//	log_warn("next star base_attr[%d:%d]", it.first, it.second);
		//}
		//觉醒
		attr_map wakeup;
		calc_base_attr(wakeup, p_hero->get_tid(), p_hero->get_current_level(), p_hero->get_grade(), p_hero->get_star(), 1);
		fill_fight_attr(wakeup, fa->mutable_wakeup());
	}

	return power;
}

uint32_t fight_manager_t::calc_attr(const attr_map& add_attr, const attr_map& add_percent, attr_map& attrs, uint32_t hero_tid, uint32_t level, uint32_t grade, uint32_t star, uint32_t wake_up, uint64_t role_uid)
{
	attrs = add_attr;
	uint32_t power = 0;
	// 读基本表
	Hero* hero_conf = GET_CONF(Hero, hero_tid);
	if (hero_conf == NULL)
	{
		log_error("Hero [%u] not exists", hero_tid);
		return 0;
	}
	attrs[attrtype_enum::atk_range] = hero_conf->base_atk_range();
	// 1 先读星级表 读一级属性
	HeroStar* star_conf = GET_CONF(HeroStar, hash_util_t::hash_to_uint32(hero_tid, star));
	if (star_conf == NULL)
	{
		log_error("HeroStar [%u] [%u] not exists", hero_tid, star);
		return 0;
	}

	for (auto attr_pair : star_conf->attrs())
	{
		attrs[attr_pair.first] += attr_pair.second;
	}

	//命力读取属性
	lifeforce_attr_manager_t::add_single_attr(role_uid, attrs);
	

	// 读品阶表
	HeroGrade* grade_conf = GET_CONF(HeroGrade, hash_util_t::hash_to_uint32(hero_tid, grade));
	if (grade_conf == NULL)
	{
		log_error("HeroGrade [%u] [%u] not exists", hero_tid, grade);
		return 0;
	}

	for (auto attr_pair : grade_conf->attrs())
	{
		attrs[attr_pair.first] += attr_pair.second;
	}

	if (wake_up > 0)
	{
		for (auto attr_pair : hero_conf->wakeup_attr())
		{
			attrs[attr_pair.first] += attr_pair.second;
		}
	}

	attr_map attr_conf(attrs);

	
	//攻击力 = (合并后的攻击力 + 合并后的属性18 * (武将等级 - 1) / 10000) * 
	//((10000 + 攻击增加万分比 - 攻击减少万分比) / 10000);
	attrs[attrtype_enum::atk] =
		(attrs[attrtype_enum::atk] + attrs[attrtype_enum::atk_growth] * 0.0001 * (level - 1) ) *
		((10000 + attrs[attrtype_enum::atk_add_per] - attrs[attrtype_enum::atk_cut_per]) * 0.0001);


	//物理防御 = (合并后的防御 +  合并后的属性19 * (武将等级 - 1) / 10000) *
	//((10000 + 防御增加万分比 - 防御减少万分比) / 10000);
	attrs[attrtype_enum::def_physical] =
		(attrs[attrtype_enum::def_physical] + attrs[attrtype_enum::def_physical_growth] * 0.0001 * (level - 1)) *
		((10000 + attrs[attrtype_enum::def_add_per] - attrs[attrtype_enum::def_cut_per]) * 0.0001);

	//最大生命值 = (合并后的最大生命值 + 合并后的属性21 * (武将等级 - 1) / 10000) *
	//((10000 + 最大生命增加万分比 - 最大生命减少万分比) / 10000);
	attrs[attrtype_enum::max_hp] =
		(attrs[attrtype_enum::max_hp] + attrs[attrtype_enum::max_hp_growth] * 0.0001 * (level - 1)) *
		((10000 + attrs[attrtype_enum::max_hp_bonus] - attrs[attrtype_enum::hp_cut_per]) * 0.0001);

	//速度 = 合并后的速度 * ((10000 + 速度增加万分比 - 速度减少万分比) / 10000);
	attrs[attrtype_enum::speed] =
		attrs[attrtype_enum::speed] *
		((10000 + attrs[attrtype_enum::speed_add_per] - attrs[attrtype_enum::speed_cut_per]) * 0.0001);

	//暴击伤害 = 合并后的暴击伤害 * ((10000 + 暴击伤害增加万分比 - 暴击伤害减少万分比) / 10000);
	//log_warn("暴击伤害最终计算前[%d]", attrs[attrtype_enum::crit_damage]);
	attrs[attrtype_enum::crit_damage] =
		attrs[attrtype_enum::crit_damage] *
		((10000 + attrs[attrtype_enum::crit_damage_bonus] - attrs[attrtype_enum::crit_damage_cut_per]) * 0.0001);
	//log_warn("暴击伤害最终计算后[%d]", attrs[attrtype_enum::crit_damage]);
	//最大攻击 = 攻击力 * 攻击浮动 * 0.0001;
	attrs[attrtype_enum::atk_max] = std::max<int>(attrs[attrtype_enum::atk] * ( attrs[attrtype_enum::atk_range] * 0.0001 ), 1);

	// 计算万分比加成
	for (auto attr_pair : add_percent)
	{
		attrs[attr_pair.first] = attrs[attr_pair.first] + attrs[attr_pair.first] * attr_pair.second / 10000.f;
	}
	return power;
}

uint32_t fight_manager_t::calc_attr(uint32_t level, MonsterHero* conf, attr_map& attrs, proto::common::hero_single* single)
{
	if (conf == NULL)
	{
		log_error("conf null error");
		return 0;
	}

	for (auto attrpair : conf->attrs())
	{
		attrs[attrpair.first] += attrpair.second;
	}

	if (conf->is_dynamic())
	{
		MonsterBaseAttrTable* baseattr = GET_CONF(MonsterBaseAttrTable, level);
		if (NULL != baseattr)
		{
			for (auto dynpair : conf->dyn_param())
			{
				uint32_t value = (uint32_t)(baseattr->get_attr(dynpair.first) * ( dynpair.second * 0.0001 ));

				// 最大生命不能为0
				if (value == 0 && dynpair.first == attrtype_enum::max_hp)
				{
					value = 1;
				}

				attrs[dynpair.first] = value;
			}
		}
		else
		{
			log_error("MonsterBaseAttrTable null error level[%u]", level);
		}
	}

	if (single != NULL)
	{
		proto::common::fight_attr* fa = single->mutable_fight_attr();
		for (auto attr : attrs)
		{
			if (attr.second == 0)
			{
				continue;
			}

			proto::common::attr_data* ad = fa->mutable_attr();
			proto::common::attr_single* as = ad->add_attrs();
			if (as == NULL)
			{
				continue;
			}

			as->set_type(attr.first);
			as->set_value(attr.second);
		}
	}

	return 0;
}

void fight_manager_t::calc_base_attr(attr_map& attrs, uint32_t hero_tid, uint32_t level, uint32_t grade, uint32_t star, uint32_t wake_up)
{
	// 读基本表
	Hero* hero_conf = GET_CONF(Hero, hero_tid);
	if (hero_conf == NULL)
	{
		log_error("Hero [%u] not exists", hero_tid);
		return;
	}
	attrs[attrtype_enum::atk_range] = hero_conf->base_atk_range();
	// 1 先读星级表 读一级属性
	HeroStar* star_conf = GET_CONF(HeroStar, hash_util_t::hash_to_uint32(hero_tid, star));
	if (star_conf == NULL)
	{
		log_error("HeroStar [%u] [%u] not exists", hero_tid, star);
		return;
	}

	for (auto attr_pair : star_conf->attrs())
	{
		attrs[attr_pair.first] += attr_pair.second;
	}

	// 读品阶表
	HeroGrade* grade_conf = GET_CONF(HeroGrade, hash_util_t::hash_to_uint32(hero_tid, grade));
	if (grade_conf == NULL)
	{
		log_error("HeroGrade [%u] [%u] not exists", hero_tid, grade);
		return;
	}

	for (auto attr_pair : grade_conf->attrs())
	{
		attrs[attr_pair.first] += attr_pair.second;
	}

	if (wake_up > 0)
	{
		for (auto attr_pair : hero_conf->wakeup_attr())
		{
			attrs[attr_pair.first] += attr_pair.second;
		}
	}
	//攻击力 = (合并后的攻击力 + 合并后的属性18 * (武将等级 - 1) / 10000) * 
	//((10000 + 攻击增加万分比 - 攻击减少万分比) / 10000);
	attrs[attrtype_enum::atk] =
		(attrs[attrtype_enum::atk] + attrs[attrtype_enum::atk_growth] * 0.0001 * (level - 1)) *
		((10000 + attrs[attrtype_enum::atk_add_per] - attrs[attrtype_enum::atk_cut_per]) * 0.0001);


	//物理防御 = (合并后的防御 +  合并后的属性19 * (武将等级 - 1) / 10000) *
	//((10000 + 防御增加万分比 - 防御减少万分比) / 10000);
	attrs[attrtype_enum::def_physical] =
		(attrs[attrtype_enum::def_physical] + attrs[attrtype_enum::def_physical_growth] * 0.0001 * (level - 1)) *
		((10000 + attrs[attrtype_enum::def_add_per] - attrs[attrtype_enum::def_cut_per]) * 0.0001);

	//最大生命值 = (合并后的最大生命值 + 合并后的属性21 * (武将等级 - 1) / 10000) *
	//((10000 + 最大生命增加万分比 - 最大生命减少万分比) / 10000);
	attrs[attrtype_enum::max_hp] =
		(attrs[attrtype_enum::max_hp] + attrs[attrtype_enum::max_hp_growth] * 0.0001 * (level - 1)) *
		((10000 + attrs[attrtype_enum::max_hp_bonus] - attrs[attrtype_enum::hp_cut_per]) * 0.0001);

	//速度 = 合并后的速度 * ((10000 + 速度增加万分比 - 速度减少万分比) / 10000);
	attrs[attrtype_enum::speed] =
		attrs[attrtype_enum::speed] *
		((10000 + attrs[attrtype_enum::speed_add_per] - attrs[attrtype_enum::speed_cut_per]) * 0.0001);

	//暴击伤害 = 合并后的暴击伤害 * ((10000 + 暴击伤害增加万分比 - 暴击伤害减少万分比) / 10000);
	//log_warn("暴击伤害最终计算前[%d]", attrs[attrtype_enum::crit_damage]);
	attrs[attrtype_enum::crit_damage] =
		attrs[attrtype_enum::crit_damage] *
		((10000 + attrs[attrtype_enum::crit_damage_bonus] - attrs[attrtype_enum::crit_damage_cut_per]) * 0.0001);
	//log_warn("暴击伤害最终计算后[%d]", attrs[attrtype_enum::crit_damage]);
	//最大攻击 = 攻击力 * 攻击浮动 * 0.0001;
	attrs[attrtype_enum::atk_max] = std::max<int>(attrs[attrtype_enum::atk] * ( attrs[attrtype_enum::atk_range] * 0.0001 ), 1);

}

void fight_manager_t::fill_fight_attr(attr_map& attrs, proto::common::attr_data* ad)
{
	if (NULL == ad)
	{
		return;
	}

	//log_warn("计算属性填充开始<<<<<<");
	for (auto attr : attrs)
	{
		proto::common::attr_single* as = ad->add_attrs();
		if (as == NULL)
		{
			continue;
		}

		as->set_type(attr.first);
		as->set_value(attr.second);

		//log_warn("属性编号[%u] 属性值[%d]", attr.first, attr.second);
	}
	//log_warn("计算属性填充结束>>>>>>");
}

bool fight_manager_t::fill_hero_skill(proto::common::fight_type fight_type, role_ptr p_role, hero_ptr p_hero, proto::common::hero_single* single)
{
	if (p_role == NULL)
	{
		log_error("p_role is null");
		return false;
	}
	if (p_hero == NULL)
	{
		log_error("p_hero is null");
		return false;
	}

	if (single == NULL)
	{
		log_error("single is null");
		return false;
	}
	single->clear_skill();
	proto::common::skill_data* sd = single->mutable_skill();
	proto::common::skill_single* skill = NULL;

	std::vector<hero_skill> hero_skills;
	p_hero->get_fight_skill_list(hero_skills);

	for (uint32_t i = 0; i < hero_skills.size(); ++i)
	{
		skill = sd->add_skills();
		fill_hero_skill_data(skill, p_role, fight_type, p_hero->get_unique_id(), hero_skills[i].skill_id,
							 hero_skills[i].skill_level, hero_skills[i].skill_source);
	}
	return true;
}

bool fight_manager_t::fill_hero_skill(MonsterHero* conf, proto::common::hero_single* single)
{
	if (conf == NULL)
	{
		log_error("conf null error");
		return false;
	}

	if (single == NULL)
	{
		log_error("single null error");
		return false;
	}

	proto::common::skill_data* sd = single->mutable_skill();
	for (uint32_t i = 0; i < conf->get_skill_list().size(); ++i)
	{
		proto::common::skill_single* skill = sd->add_skills();
		if (NULL != skill)
		{
			fill_hero_skill_data(skill, conf->get_skill_list()[i], 1);
		}
	}
	
	return true;
}

bool fight_manager_t::fill_hero_skill(hero_ptr p_hero, proto::common::hero_single* single)
{
	if (p_hero == NULL)
	{
		log_error("p_hero is null");
		return false;
	}

	if (single == NULL)
	{
		log_error("single is null");
		return false;
	}
	single->clear_skill();
	proto::common::skill_data* sd = single->mutable_skill();
	proto::common::skill_single* skill = NULL;

	std::vector<hero_skill> hero_skills;
	p_hero->get_fight_skill_list(hero_skills);

	for (uint32_t i = 0; i < hero_skills.size(); ++i)
	{
		skill = sd->add_skills();
		skill->set_skill_id(hero_skills[i].skill_id);
		skill->set_skill_level(hero_skills[i].skill_level);
		//skill->set_skill_type(hero_skills[i].skill_source);
	}
	return true;
}

bool fight_manager_t::fill_hero_skill_data(proto::common::skill_single* skill, role_ptr p_role, 
											proto::common::fight_type fight_type, uint64_t hero_unique_id, uint32_t skill_id,
											uint32_t skill_level, uint32_t skill_type)
{
	if (NULL == skill)
	{
		log_error("fill_hero_skill_data skill NULL");
		return false;
	}
	if (NULL == p_role)
	{
		log_error("fill_hero_skill_data p_role NULL");
		return false;
	}

	SkillEffectTable *p_conf = NULL;

	skill->set_skill_id(skill_id);
	skill->set_skill_level(skill_level);
//	skill->set_skill_type(skill_type);
	p_conf = GET_CONF(SkillEffectTable, hash_util_t::hash_to_uint32_move_7(skill_id, skill_level));
	if (p_conf == NULL)
	{
		log_error("fill_hero_skill_data p_conf is null role[%lu] skill_id[%d] skill_level[%d]", skill_id, skill_level);
		return false;
	}
	else
	{
		skill->set_skill_cd(p_conf->start_cd());
	}

	if (fight_type == proto::common::fight_type_expedition)
	{
		expedition_ptr p_expedition = p_role->get_expedition();
		if (NULL != p_expedition)
		{
			int32_t skill_cd = p_expedition->get_skill_cd(hero_unique_id, skill_id);
			if (skill_cd != -1)
				skill->set_skill_cd(skill_cd);
		}
	}

	return true;
}

bool fight_manager_t::fill_hero_skill_data(proto::common::skill_single* skill, uint32_t skill_id, uint32_t skill_level)
{
	if (NULL == skill)
	{
		log_error("fight_manager_t skill NULL");
		return false;
	}

	SkillEffectTable *p_conf = NULL;

	skill->set_skill_id(skill_id);
	skill->set_skill_level(skill_level);

	p_conf = GET_CONF(SkillEffectTable, hash_util_t::hash_to_uint32_move_7(skill_id, skill_level));
	if (p_conf == NULL)
	{
		log_error("fill_hero_skill_data p_conf is null skill_id[%d] skill_level[%d]", skill_id, skill_level);
		return false;
	}
	else
	{
		skill->set_skill_cd(p_conf->start_cd());
	}
	return true;
}

bool fight_manager_t::fill_hero_base(MonsterHero* conf, proto::common::hero_single* single)
{
	if (conf == NULL)
	{
		log_error("conf null error");
		return false;
	}

	if (single == NULL)
	{
		log_error("single null error");
		return false;
	}

	proto::common::base_attr* base = single->mutable_base_attr();
	base->set_id(conf->id());
	base->set_type(hero_type_monster);
	base->set_level(conf->level());
	base->set_unique_id(string_util_t::uint32_to_string(conf->id()));
	base->set_grade_id(conf->grade());
	base->set_debut(conf->debut());
	base->set_resource_id(conf->resource_id());
	base->set_monster_type(conf->monster_type());
	base->set_scale(conf->scale());
	base->set_hero_name_id(conf->hero_name_id());
	base->set_boss_hp_plies(conf->boss_hp_plies());
	base->set_event_type(conf->event_type());
	base->set_event_param(conf->event_param());
	base->set_plot_id(conf->plot_id());
	base->set_would_show(conf->would_show());
	base->set_country(conf->camp());
	base->set_terry(conf->terry());
	return true;
}

bool fight_manager_t::finish_fight(role_ptr p_role, const proto::common::fight_result& fight_result)
{
// wsy 注释 旧的战斗代码
// 	if (p_role == NULL)
// 	{
// 		log_error("role null error");
// 		return false;
// 	}
// 
// 	const proto::common::fight_data& fd = p_role->get_fight_data();
// 	const proto::common::fight_camp enemy = fd.enemy();
// 	const proto::common::obj_data od = enemy.objs();
// 
// 	proto::common::role_change_data rcd;
// 	for (proto::common::obj_single obj : od.obj())
// 	{
// 		uint64_t object_id = string_util_t::string_to_uint64(obj.object_uid());
// 
// 		if (obj.object_type() == proto::common::SCENEOBJECT_MONSTER)
// 		{
// 			if (fight_result.result())
// 			{
// 				task_mgr_ptr p_task_mgr = p_role->get_task_mgr();
// 
// 				if (NULL != p_task_mgr)
// 				{
// 					p_task_mgr->on_finish_pve(object_id, rcd);
// 				}
// 			}
// 		}
// 	}
// 	
// 	proto::client::gc_finish_fight_reply reply;
// 	reply.set_result(fight_result.result());
// 	reply.mutable_rcd()->CopyFrom(rcd);
// 	p_role->send_msg_to_client(op_cmd::gc_finish_fight_reply, reply);
	
	return true;
}

bool fight_manager_t::create_combat(role_ptr p_role, const std::vector<role_ptr>* team_members, uint64_t fight_uid, 
									proto::common::fight_data& fd, bool can_form_before_fight, combat_type type)
{
	if (NULL == p_role)
	{
		log_error("create_combat p_role NULL");
		return false;
	}

	check_remove();

	combat_ptr combat(new combat_t(fight_uid));

	// ---------------test------------------------
	++create_combat_count;
	log_warn("fight_check create_count:%d ptr_count:%d role[%lu] fight_uid[%lu]", create_combat_count, combat.use_count(), p_role->get_uid(), fight_uid);
	//--------------------------------------------

	if (combat->init(fd, can_form_before_fight, type))
	{
		m_combats.insert(std::make_pair(fight_uid, combat));
		FIGHT_LOG("[%lu]create_combat", fight_uid);

		p_role->set_fight_uid(fight_uid);
		if (team_members != NULL)
		{
			for (uint32_t i = 0; i < team_members->size(); ++i)
			{
				role_ptr p_member = team_members->at(i);
				if (p_member != NULL)
				{
					p_member->set_fight_uid(fight_uid);
				}
			}
		}

		return true;
	}

	return false;
}

combat_ptr fight_manager_t::get_combat(uint64_t fight_uid)
{
	combat_map::iterator it = m_combats.find(fight_uid);
	if (it == m_combats.end())
	{
		return combat_ptr();
	}

	if (NULL == it->second)
	{
		return combat_ptr();
	}

	if (it->second->is_end())
	{
		return combat_ptr();
	}

	return it->second;
}

combat_ptr fight_manager_t::find_combat(uint64_t fight_uid)
{
	combat_map::iterator it = m_combats.find(fight_uid);
	if (it != m_combats.end())
	{
		return it->second;
	}

	return combat_ptr();
}

combat_ptr fight_manager_t::check_combat(role_ptr p_role, uint64_t fight_uid)
{
	if (NULL == p_role)
	{
		log_error("p_role is null");
		return combat_ptr();
	}

	if (p_role->get_fight_uid() != fight_uid)
	{
		log_error("fight_uid error role_uid:%lu role_fight_uid:%lu param_fight_uid:%lu", p_role->get_uid(), p_role->get_fight_uid(), fight_uid);
		return combat_ptr();
	}

	return get_combat(fight_uid);
}

bool fight_manager_t::combat_loading_end_ntf(role_ptr p_role, uint64_t fight_uid)
{
	if (NULL == p_role)
	{
		log_error("p_hero is null");
		return false;
	}

	combat_ptr combat = check_combat(p_role, fight_uid);
	if (NULL == combat)
	{
		log_error("loading end check combat fail %lu %lu", p_role->get_uid(), fight_uid);
		return false;
	}

	if (combat->loading_end(p_role->get_uid()))
	{
		combat->syn_combat_obj_state();
		return true;
	}
	else
	{
		return false;
	}
}

bool fight_manager_t::combat_disposition_request(role_ptr p_role, uint64_t fight_uid, const proto::common::combat_form_data& action)
{
	if (NULL == p_role)
	{
		log_error("p_role is null");
		return false;
	}

	combat_ptr combat = check_combat(p_role, fight_uid);
	if (NULL == combat)
	{
		log_error("combat form check combat fail %lu %lu", p_role->get_uid(), fight_uid);
		return false;
	}

	if (!combat->initiative_disposition(p_role->get_uid(), action))
	{
		proto::client::gc_combat_form_reply reply;
		reply.set_reply_code(1);
		p_role->send_msg_to_client(op_cmd::gc_combat_form_reply, reply);
	}

	return true;
}

bool fight_manager_t::combat_play_end_ntf(role_ptr p_role, uint64_t fight_uid, uint32_t hero_att_round)
{
	if (NULL == p_role)
	{
		log_error("p_role is null %lu", fight_uid);
		return false;
	}

	combat_ptr combat = check_combat(p_role, fight_uid);
	if (NULL == combat)
	{
		log_error("play end check combat fail %lu %lu", p_role->get_uid(), fight_uid);
		return false;
	}

	if (combat->fight_play_end(p_role->get_uid(), hero_att_round))
	{
		if (!combat->is_end())
		{
			combat->syn_combat_obj_state();
		}
		
		return true;
	}
	else
	{
		return false;
	}
}

bool fight_manager_t::combat_before_form_ready(role_ptr p_role, uint64_t fight_uid)
{
	if (NULL == p_role)
	{
		log_error("p_role is null %lu", fight_uid);
		return false;
	}

	combat_ptr combat = check_combat(p_role, fight_uid);
	if (NULL == combat)
	{
		log_error("play end check combat fail %lu %lu", p_role->get_uid(), fight_uid);
		return false;
	}

	bool bresult = combat->combat_before_form_end(p_role->get_uid());
	return bresult;
}

bool fight_manager_t::request_escape(role_ptr p_role, uint64_t fight_uid)
{
	if (NULL == p_role)
	{
		log_error("p_role is null %lu", fight_uid);
		return false;
	}

	combat_ptr combat = check_combat(p_role, fight_uid);
	if (NULL == combat)
	{
		log_error("check combat fail %lu %lu", p_role->get_uid(), fight_uid);
		return false;
	}

	return combat->request_escape(p_role->get_uid());
}

bool fight_manager_t::request_switch_auto_fight(role_ptr p_role, uint64_t fight_uid, uint32_t auto_fight,
												const proto::client::hero_auto_fight_data* hero_data)
{
	if (NULL == p_role)
	{
		log_error("p_role is null %lu", fight_uid);
		return false;
	}

	combat_ptr combat = check_combat(p_role, fight_uid);
	if (NULL == combat)
	{
		log_error("check combat fail %lu %lu", p_role->get_uid(), fight_uid);
		return false;
	}

	if(combat->change_role_auto_fight(auto_fight, p_role->get_uid(), hero_data))
		combat->auto_disposition(p_role->get_uid());
	return true;
}

uint64_t fight_manager_t::gen_fight_uid()
{
	uint64_t uid = common::g_id_allocator.gen_new_id(common::ID_ALLOCATOR_TYPE_FIGHT);
	return uid;
}

bool fight_manager_t::on_role_login(role_ptr p_role)
{
	if (NULL == p_role)
	{
		log_error("p_role is null");
		return false;
	}

	if (p_role->get_fight_uid() != 0)
	{
		combat_ptr combat = check_combat(p_role, p_role->get_fight_uid());
		if (NULL != combat)
		{
			// 跨服PVP 本服PVP 组队战斗 直接拉进战斗
			if (combat->is_single_pve())
			{
				proto::client::gc_combat_recovery_ask_req msg;
				p_role->send_msg_to_client(op_cmd::gc_combat_recovery_ask_req, msg);
			}
			else
			{
				combat->on_role_login(p_role->get_uid());
			}
		}
		else
		{
			p_role->set_fight_uid(0);
			p_role->change_object_state(proto::common::object_state_normal);
			proto::client::gc_change_sys_state_notify state_msg;
			state_msg.set_role_uid(string_util_t::uint64_to_string(p_role->get_uid()));
			state_msg.set_reply_code(0);
			state_msg.mutable_object_state()->set_obj_state(p_role->get_object_state());
			p_role->send_msg_to_client(op_cmd::gc_change_sys_state_notify, state_msg);

			// 断线重连上来的 如果战斗已结束 补发简单的战斗结果
			proto::client::gc_reconnect_combat_result_ntf ntf;
			ntf.set_target_name(p_role->m_combat_result.get_target_name());
			ntf.set_win_lose(p_role->m_combat_result.get_combat_result());
			p_role->send_msg_to_client(op_cmd::gc_reconnect_combat_result_ntf, ntf);
			p_role->m_combat_result.reset_combat_result();
		}
	}
	else
	{
		if (p_role->get_object_state() == proto::common::object_state_fighting)
		{
			p_role->change_object_state(proto::common::object_state_normal);
			proto::client::gc_change_sys_state_notify state_msg;
			state_msg.set_role_uid(string_util_t::uint64_to_string(p_role->get_uid()));
			state_msg.set_reply_code(0);
			state_msg.mutable_object_state()->set_obj_state(p_role->get_object_state());
			p_role->send_msg_to_client(op_cmd::gc_change_sys_state_notify, state_msg);

			// 断线重连上来的 如果战斗已结束 补发简单的战斗结果
			proto::client::gc_reconnect_combat_result_ntf ntf;
			ntf.set_target_name(p_role->m_combat_result.get_target_name());
			ntf.set_win_lose(p_role->m_combat_result.get_combat_result());
			p_role->send_msg_to_client(op_cmd::gc_reconnect_combat_result_ntf, ntf);
			p_role->m_combat_result.reset_combat_result();
		}
	}

	return true;
}

bool fight_manager_t::on_role_logout(role_ptr p_role)
{
	if (NULL == p_role)
	{
		log_error("p_role is null");
		return false;
	}

	if (p_role->get_fight_uid() != 0)
	{
		combat_ptr combat = get_combat(p_role->get_fight_uid());
		if (NULL != combat)
		{
			combat->on_role_logout(p_role->get_uid());
		}
	}

	return true;
}

bool fight_manager_t::on_combat_after_attack(combat_ptr combat)
{
	if (NULL == combat) {
		log_error("combat is null");
		return false;
	}

	// 目前组队回合只会调用self的，默认self是人 enemy是怪（qhj）
	// 后期如果要pvp回合统计在按类型修改
	const proto::common::fight_data& fd = combat->get_fight_data();
	combat_round_callback_map::iterator it = m_combat_round_callback.find(fd.common().param().type());
	if (it != m_combat_round_callback.end())
	{
		(it->second)(fd.common(), fd.self(), fd.enemy(), combat);
	}

	return true;
}

bool fight_manager_t::on_combat_result(const proto::common::combat_result& combat_result, combat_ptr combat, proto::client::gc_combat_result_ntf& ntf)
{
	if (NULL == combat)
	{
		log_error("combat is null");
		return false;
	}
	// 写DB的战斗log日志
	write_combat_db_log(combat_result, combat);

	fight_obj_ptr p_obj = fight_obj_ptr();

	std::pair<uint32_t,uint32_t> exp = combat->get_exp(combat_result.win_camp() == proto::common::combat_camp_1);
	
	uint32_t hero_exp = exp.first;
	uint32_t role_exp = exp.second;
	if (hero_exp > 0 || role_exp > 0)
	{
		//组队经验加成
		uint32_t troop_count = combat->get_obj_count(proto::common::combat_camp_1);
		if (troop_count == 2)
		{
			hero_exp += hero_exp * get_troop_exp_2() / 10000;
			role_exp += role_exp * get_troop_exp_2() / 10000;
		}
		else if (troop_count == 3)
		{
			hero_exp += hero_exp * get_troop_exp_3() / 10000;
			role_exp += role_exp * get_troop_exp_3() / 10000;
		}
	}

	std::vector<uint32_t> monster_vec;
	combat->get_task_monster(monster_vec);

	const proto::common::fight_data& fd = combat->get_fight_data();
	for (const proto::common::obj_single& obj : fd.self().objs().obj())
	{
		if (obj.object_type() == proto::common::SCENEOBJECT_USER)
		{
			combat_result_callback_map::iterator it = m_combat_result_callback.find(fd.common().param().type());
			if (it != m_combat_result_callback.end())
			{
				(it->second)(
					combat_result,
					obj,
					fd.common(),
					fd.self(),
					fd.enemy(),
					combat,
					ntf);
			}

			uint64_t role_uid = string_util_t::string_to_uint64(obj.object_uid());
			role_ptr role = role_manager_t::find_role(role_uid);
			p_obj = combat->find_obj(role_uid);
			if (NULL != role && NULL != p_obj)
			{
				uint32_t coeff = 100;
				if (combat_result.win_camp() == proto::common::combat_camp_1 && combat->get_fight_type() == proto::common::fight_type_dark_monster)
				{
					//暗雷怪获胜
					if (role->is_patrol())
					{
						//如果在巡逻
						const uint32_t multi_exp_buff = multi_time_manager_t::multi_exp_buff(role_uid, ntf.mutable_rcd());
						coeff = role->get_patrol_exp() + multi_exp_buff;
						role->set_patrol_count(role->get_patrol_count() + 1);
						achieve_common_t::notify_progress_state(role_uid, proto::common::ACHIEVE_PATROL_KILL_MONSTER_GROUP_NUMBER);
					}
				}
				if (role_exp > 0)
				{
					uint32_t add_role_exp = role_exp * coeff / 100;
					//加经验
                    LevelupTable *p_config = GET_CONF(LevelupTable, role->get_level());
                    if (NULL != p_config)
                    {
                        if (add_role_exp > p_config->exp_limit())
                        {
							add_role_exp = p_config->exp_limit();
                        }
                        ntf.set_role_exp(add_role_exp);
                        role->add_exp(add_role_exp, log_enum::source_type_monster_fight_pve, combat->get_against_id(), ntf.mutable_rcd());
                    }
                    else
                    {
                        log_error("level_up config not find level[%d]", role->get_level());
                    }
				}
				if (hero_exp > 0)
				{
					uint32_t add_hero_exp = hero_exp * coeff / 100;
					std::vector<uint64_t> hero_list;
					combat->get_role_heros(role_uid, hero_list);
					//log_warn("hero_list size[%d]", hero_list.size());
					for (auto hero_uid : hero_list)
					{
						hero_manager_t::add_pve_exp(role, hero_uid, add_hero_exp, &ntf);
					}
				}
				role->save_auto_fight_flag((uint32_t)p_obj->is_auto_fight());
				if (role->get_online_state() == ROLE_ONLINE_STATE_ONLINE)
				{
					proto::common::personal_info* p_info = ntf.mutable_rcd()->mutable_per_info();
					role->get_personal_info(p_info);
					role->send_msg_to_client(op_cmd::gc_combat_result_ntf, ntf);
				}
				else
				{
					uint32_t is_win = 0;
					if (combat_result.win_camp() == obj.camp())
						is_win = 1;
					role->m_combat_result.set_combat_result(is_win, combat->get_fight_uid(), "");	// todo 暂时没名字
				}
				//不论战斗输赢都计算杀怪任务
				auto p_task_mgr = role->get_task_mgr();
				if (NULL != p_task_mgr)
				{
					for (auto monster_id : monster_vec)
					{
						p_task_mgr->try_change_task_state(TASK_TARGET_TYPE_SPECIAL_MONSTER, *ntf.mutable_rcd(), monster_id, 1);
					}
				}
			}
		}
	}

	for (const proto::common::obj_single& obj : fd.enemy().objs().obj())
	{
		if (obj.object_type() == proto::common::SCENEOBJECT_USER)
		{
			combat_result_callback_map::iterator it = m_combat_result_callback.find(fd.common().param().type());
			if (it != m_combat_result_callback.end())
			{
				(it->second)(
					combat_result,
					obj,
					fd.common(),
					fd.enemy(),
					fd.self(),
					combat,
					ntf);
			}

			uint64_t role_uid = string_util_t::string_to_uint64(obj.object_uid());
			role_ptr role = role_manager_t::find_role(role_uid);
			p_obj = combat->find_obj(role_uid);
			if (NULL != role && NULL != p_obj)
			{
				role->save_auto_fight_flag((uint32_t)p_obj->is_auto_fight());
				if (role->get_online_state() == ROLE_ONLINE_STATE_ONLINE)
				{
					ntf.mutable_rcd()->mutable_per_info()->Clear();
					role->send_msg_to_client(op_cmd::gc_combat_result_ntf, ntf);
				}
				else
				{
					uint32_t is_win = 0;
					if (combat_result.win_camp() == obj.camp())
						is_win = 1;
					role->m_combat_result.set_combat_result(is_win, combat->get_fight_uid(), "");	// todo 暂时没名字
				}
			}
		}
	}
	// 给观战玩家发送结算消息
	const std::set<uint64_t>& watching_role_list = combat->get_watching_role_list();
	std::set<uint64_t>::const_iterator iter = watching_role_list.begin();
	for (; iter != watching_role_list.end(); ++iter)
	{
		role_ptr role = role_manager_t::find_role(*iter);
		if (role != NULL)
		{
			ntf.mutable_rcd()->mutable_per_info()->Clear();
			role->send_msg_to_client(op_cmd::gc_combat_result_ntf, ntf);
		}
	}

	on_combat_end(combat_result, combat);
	return true;
}

bool fight_manager_t::on_combat_end(const proto::common::combat_result& combat_result, combat_ptr combat)
{
	if (NULL == combat)
	{
		log_error("combat is null");
		return false;
	}

	const proto::common::fight_data& fd = combat->get_fight_data();
	combat_end_notify_map::iterator it = m_combat_end_notify_map.find(fd.common().param().type());
	if (it != m_combat_end_notify_map.end())
	{
		(it->second)(combat_result, combat);
	}
	
	FIGHT_LOG("[%lu]combat_end", combat->get_fight_uid());
	m_remove_set.insert(combat->get_fight_uid());

	return true;
}

void fight_manager_t::write_combat_db_log(const proto::common::combat_result& combat_result, combat_ptr combat)
{
	if (NULL == combat)
	{
		log_error("combat is null");
		return;
	}
	const proto::common::fight_data& fd = combat->get_fight_data();
	// 记录日志
	uint64_t role_uid = string_util_t::string_to_uint64(fd.common().fight_initiator());
	uint32_t role_level = 0;
	role_ptr p_role = role_manager_t::find_role(role_uid);
	if (NULL == p_role)
	{
		log_error("write_combat_db_log p_role[%lu] NULL", role_uid);
	}
	else
		role_level = p_role->get_level();

	uint64_t fight_uid = string_util_t::string_to_uint64(fd.common().fight_id());
	uint32_t combat_type = combat->get_combat_type();
	uint32_t is_win = combat_result.win_camp() == proto::common::combat_camp_1;
	std::vector<uint64_t> teammember_log;
	std::vector<uint64_t> targets_log;
	combat->get_camp_fight_obj_uid(proto::common::combat_camp_1, teammember_log);
	combat->get_camp_fight_obj_uid(proto::common::combat_camp_2, targets_log);
	uint32_t now_time = common::time_util_t::now_time();
	log_wrapper_t::send_role_combat_log(role_uid, role_level, fd.common().param().type(), combat_type, teammember_log, targets_log, fight_uid, is_win, combat->get_start_time(), now_time);
}

void fight_manager_t::watch_combat(role_ptr p_role, uint64_t watching_role_uid, uint64_t fight_uid, bool b_is_troop_back)
{
	proto::client::gc_watch_combat_reply reply;
	uint32_t error_code = errcode_enum::error_ok;
	uint32_t watch_num = 1;

	role_ptr p_watching_role = role_manager_t::find_role(watching_role_uid);
	if (p_watching_role == NULL)
	{
		log_error("combat p_watching_role is null [%lu]", watching_role_uid);
		return;
	}

	uint64_t find_fight_uid = fight_uid;
	if (find_fight_uid == 0)
		find_fight_uid = p_watching_role->get_fight_uid();

	combat_ptr p_combat = fight_manager_t::check_combat(p_watching_role, find_fight_uid);
	std::vector<role_ptr> team_member_list;
	// 战斗还在
	if (NULL != p_combat)
	{
		auto conf = GET_CONF(Comprehensive, comprehensive_common::combat_watch_switch);
		uint32_t watch_flag = GET_COMPREHENSIVE_VALUE_1(conf);

		if ( watch_flag == 0 &&
			(p_combat->get_fight_type() == proto::common::fight_type_task ||
			 p_combat->get_fight_type() == proto::common::fight_type_against ||
			 p_combat->get_fight_type() == proto::common::fight_type_dark_monster) )
		{
			error_code = errcode_enum::notice_combat_cannot_watch;
		}
		else
		{
			if (p_role->get_troop_id() != 0)
			{
				troop_ptr p_troop = game_troop_mgr_t::get_troop(p_role->get_troop_id());
				if (p_troop != NULL)
				{
					if (p_troop->get_leader_id() == p_role->get_uid())
					{
						const troop_t::troop_member_vec &member_list = p_troop->get_members();
						role_ptr p_member = role_ptr();
						watch_num = member_list.size();

						for (uint32_t i = 0; i < member_list.size(); ++i)
						{
							troop_t::troop_member_ptr p_troop_member = member_list[i];
							if (NULL == p_troop_member)
							{
								log_error("p_troop_member NULL");
								continue;
							}
							if (p_troop_member->get_uid() == p_role->get_uid() || p_troop_member->get_state() != 0 ||
								p_troop_member->get_state() != proto::common::troop_state_normal)
								continue;
							else
								p_member = role_manager_t::find_role(p_troop_member->get_uid());

							team_member_list.push_back(p_member);
						}
					}
				}
				else
				{
					error_code = errcode_enum::notice_trade_not_troop;
				}
			}

			if (error_code == errcode_enum::error_ok)
			{
				if (!b_is_troop_back)
				{
					if (!p_combat->can_watch(watch_num))
					{
						error_code = errcode_enum::notice_combat_max_watching;
					}
				}
				if (error_code == errcode_enum::error_ok)
				{
					error_code = p_role->set_watching_fight_uid(p_watching_role->get_fight_uid());

					if (error_code == errcode_enum::error_ok)
					{
						p_combat->add_watching_role(p_role->get_uid());
						role_ptr p_member = NULL;
						for (uint32_t i = 0; i < team_member_list.size(); ++i)
						{
							p_member = team_member_list[i];
							if (NULL == p_member)
							{
								log_error("watch_combat p_member_role NULL uid[%lu]", p_member->get_uid());
								continue;
							}
							if (p_member->set_watching_fight_uid(p_watching_role->get_fight_uid()))
								p_combat->add_watching_role(p_member->get_uid());
						}

						p_combat->fill_combat_data(reply.mutable_combat_data(), NULL, true);
						p_combat->fill_combat_recovery_data(reply.mutable_recovery_data());
					}
				}
			}
		}
	}
	else
		error_code = errcode_enum::notice_combat_not_find;

	if (error_code != errcode_enum::error_ok)
	{
		log_error("troop_back error code[%d]", error_code);
	}
	if (b_is_troop_back)
	{
		return;
	}

	reply.set_reply_code(error_code);
	p_role->send_msg_to_client(op_cmd::gc_watch_combat_reply, reply);

	if (error_code == errcode_enum::error_ok)
	{
		role_ptr p_member = NULL;
		for (uint32_t i = 0; i < team_member_list.size(); ++i)
		{
			p_member = team_member_list[i];
			if (NULL == p_member)
			{
				log_error("watch_combat p_member_role NULL uid[%lu]", p_member->get_uid());
				continue;
			}
			p_member->send_msg_to_client(op_cmd::gc_watch_combat_reply, reply);
		}
	}
}

void fight_manager_t::cancel_watch_combat(role_ptr p_role, bool b_send_msg /* = false */)
{
	if (NULL == p_role)
	{
		log_error("watch_combat p_role NULL");
		return;
	}

	proto::client::gc_cancel_watch_combat_reply reply;
	uint32_t error_code = errcode_enum::error_ok;

	std::vector<role_ptr> team_member_list;

	if (p_role->get_troop_id() != 0)
	{
		troop_ptr p_troop = game_troop_mgr_t::get_troop(p_role->get_troop_id());
		if (p_troop != NULL && p_troop->get_leader_id() == p_role->get_uid())
		{
			const troop_t::troop_member_vec &member_list = p_troop->get_members();
			role_ptr p_member = role_ptr();

			for (uint32_t i = 0; i < member_list.size(); ++i)
			{
				troop_t::troop_member_ptr p_troop_member = member_list[i];
				if (NULL == p_troop_member)
				{
					log_error("p_troop_member NULL");
					continue;
				}
				if (p_troop_member->get_uid() == p_role->get_uid() || p_troop_member->get_state() != 0)
					continue;
				else
					p_member = role_manager_t::find_role(p_troop_member->get_uid());

				team_member_list.push_back(p_member);
			}
		}
	}
	if (p_role->set_watching_fight_uid(0) == errcode_enum::error_ok)
	{
		if (b_send_msg)
		{
			reply.set_reply_code(error_code);
			p_role->send_msg_to_client(op_cmd::gc_cancel_watch_combat_reply, reply);
		}

		role_ptr p_member = NULL;
		for (uint32_t i = 0; i < team_member_list.size(); ++i)
		{
			p_member = team_member_list[i];
			if (NULL == p_member)
			{
				log_error("watch_combat p_member_role NULL uid[%lu]", p_member->get_uid());
				continue;
			}
			p_member->set_watching_fight_uid(0);
			if (b_send_msg)
			{
				p_member->send_msg_to_client(op_cmd::gc_cancel_watch_combat_reply, reply);
			}
		}
	}
}

void fight_manager_t::check_remove()
{
	combat_ptr p_combat = NULL;
	combat_map::iterator iter;
	for (auto fight_uid : m_remove_set)
	{
		iter = m_combats.find(fight_uid);
		if (iter != m_combats.end())
		{
			p_combat = iter->second;
			if (NULL != p_combat)
				p_combat->close();

			m_combats.erase(fight_uid);
			++remove_combat_count;
			log_warn("fight_check remove_count:%d ptr_count:%d fight_uid[%lu]", remove_combat_count, p_combat.use_count(), fight_uid);
		}
	}

	m_remove_set.clear();
}

void fight_manager_t::remove_all()
{
	combat_ptr p_combat = NULL;
	combat_map::iterator iter = m_combats.begin();

	for (; iter != m_combats.end();)
	{
		p_combat = iter->second;
		if (NULL != p_combat)
		{
			++remove_combat_count;
			p_combat->close();
		}
		iter = m_combats.erase(iter);
		log_warn("fight_check remove_count:%d ptr_count:%d fight_uid[%lu]", remove_combat_count, p_combat.use_count(), p_combat->get_fight_uid());
	}
	m_combats.clear();
	m_remove_set.clear();
}

bool fight_manager_t::regist_callback()
{
	// round callback
	m_combat_round_callback.insert(std::make_pair(proto::common::fight_type_field_boss, fight_manager_t::on_attack_field_boss));
	m_combat_round_callback.insert(std::make_pair(proto::common::fight_type_family_war, fight_manager_t::on_attack_family_long));

	// combat result callback
	m_combat_result_callback.insert(std::make_pair(proto::common::fight_type_task, fight_manager_t::on_task_fight_result));
	m_combat_result_callback.insert(std::make_pair(proto::common::fight_type_dark_monster, fight_manager_t::on_dark_fight_result));
	m_combat_result_callback.insert(std::make_pair(proto::common::fight_type_family_war, fight_manager_t::on_family_long_result));
	m_combat_result_callback.insert(std::make_pair(proto::common::fight_type_light_monster, fight_manager_t::on_light_fight_result));
	m_combat_result_callback.insert(std::make_pair(proto::common::fight_type_pk, fight_manager_t::on_pk_result));
	m_combat_result_callback.insert(std::make_pair(proto::common::fight_type_family_pk, fight_manager_t::on_family_pk_result));
	m_combat_result_callback.insert(std::make_pair(proto::common::fight_type_tower, fight_manager_t::on_tower_result));
	m_combat_result_callback.insert(std::make_pair(proto::common::fight_type_expedition, fight_manager_t::on_expedition_result));
	m_combat_result_callback.insert(std::make_pair(proto::common::fight_type_page_pass_pk, fight_manager_t::on_page_pass_fight_result));
	m_combat_result_callback.insert(std::make_pair(proto::common::fight_type_against, fight_manager_t::on_against_fight_result));
	m_combat_result_callback.insert(std::make_pair(proto::common::fight_type_offline_arena, fight_manager_t::on_offline_arena_result));
	
	// combat end callback
	m_combat_end_notify_map.insert(std::make_pair(proto::common::fight_type_cross_arena_rank, fight_manager_t::on_cross_arena_rank_end));
	m_combat_end_notify_map.insert(std::make_pair(proto::common::fight_type_world_cup, fight_manager_t::on_cross_arena_rank_end));
	m_combat_end_notify_map.insert(std::make_pair(proto::common::fight_type_study, fight_manager_t::on_fight_study_end));
	m_combat_end_notify_map.insert(std::make_pair(proto::common::fight_type_challenge, fight_manager_t::on_challenge_end));
	m_combat_end_notify_map.insert(std::make_pair(proto::common::fight_type_light_monster, fight_manager_t::on_light_fight_end));
	m_combat_end_notify_map.insert(std::make_pair(proto::common::fight_type_family_war, fight_manager_t::on_family_long_end));
	m_combat_end_notify_map.insert(std::make_pair(proto::common::fight_type_field_boss, fight_manager_t::on_field_boss_end));

	return true;
}

int32_t fight_manager_t::check_find_empty_hero_pos(proto::common::hero_data *hero_data, uint32_t pos)
{
	if (NULL == hero_data)
	{
		log_error("fight check_hero_pos hero_data NULL");
		return pos;
	}

	std::set<uint32_t> hero_pos_list;
	for (int i = 1; i <= battle_max_grid; ++i)
	{
		hero_pos_list.insert(i);
	}

	uint32_t hero_pos = 0;
	bool is_repeat = false;
	for (int i = 0; i < hero_data->hero_list_size(); ++i)
	{
		hero_pos = hero_data->hero_list(i).fight_attr().pos();
		hero_pos_list.erase(hero_pos);
		if (pos == hero_pos)
		{
			is_repeat = true;
		}
	}

	if (is_repeat)
	{
		std::set<uint32_t>::iterator iter = hero_pos_list.begin();
		if (iter == hero_pos_list.end())
			return pos;
		
		return *iter;
	}
	return pos;
}

bool fight_manager_t::on_attack_field_boss(
	const proto::common::fight_common& common,
	const proto::common::fight_camp& self_camp, 
	const proto::common::fight_camp& enemy_camp, 
	combat_ptr combat)
{
	if (NULL == combat) {
		log_error("combat is null");
		return false;
	}

	uint64_t object_id = string_util_t::string_to_uint64(common.param().light().object_id());
	if (object_id == 0) {
		log_error("common have no light_fight object id = 0");
		return false;
	}

	uint64_t fight_id = string_util_t::string_to_uint64(common.fight_id());
	
	field_boss_manager_t::fight_role_hurt(object_id, fight_id, self_camp, enemy_camp, combat);

	return true;
}

bool fight_manager_t::on_attack_family_long(
	const proto::common::fight_common& common, 
	const proto::common::fight_camp& self_camp, 
	const proto::common::fight_camp& enemy_camp, 
	combat_ptr combat)
{
	if (NULL == combat)
	{
		log_error("combat is null");
		return false;
	}

	if (self_camp.objs().obj_size() == 0) {
		log_error("on_attack_family_long obj_size zero");
		return false;
	}

	// 家族战不能组队取第一个self_camp
	uint64_t role_uid = string_util_t::string_to_uint64(self_camp.objs().obj(0).object_uid());

	uint64_t scene_id = string_util_t::string_to_uint64(common.param().light().object_id());
	const scene_ptr& p_scene = scene_manager_t::find_scene(scene_id);
	if (NULL == p_scene)
	{
		log_error("scene[%lu] not find", scene_id);
		return false;
	}

	family_war_scene_ptr p_family_war_scene = p_scene->get_family_war_scene_ptr();
	if (NULL == p_family_war_scene)
	{
		log_error("scene[%lu] not family_war_scene", scene_id);
		return false;
	}

	uint64_t fight_id = string_util_t::string_to_uint64(common.fight_id());

	p_family_war_scene->attack_long_round(role_uid, fight_id, enemy_camp, combat);

	return true;
}

bool fight_manager_t::on_task_fight_result(
	const proto::common::combat_result& result,
	const proto::common::obj_single& self, 
	const proto::common::fight_common& common, 
	const proto::common::fight_camp& self_camp, 
	const proto::common::fight_camp& enemy_camp, 
	combat_ptr combat, 
	proto::client::gc_combat_result_ntf& ntf)
{
	if (NULL == combat)
	{
		log_error("combat is null");
		return false;
	}

	uint64_t role_uid = string_util_t::string_to_uint64(self.object_uid());
	role_ptr role = role_manager_t::find_role(role_uid);
	if (NULL == role)
	{
		log_error("role is null [%lu]", role_uid);
		return false;
	}

	proto::common::role_change_data rcd;
	for (proto::common::obj_single obj : enemy_camp.objs().obj())
	{
		uint64_t object_id = string_util_t::string_to_uint64(obj.object_uid());
		if (obj.object_type() == proto::common::SCENEOBJECT_MONSTER)
		{
			if (result.win_camp() == self.camp())
			{
				task_mgr_ptr p_task_mgr = role->get_task_mgr();
				if (NULL != p_task_mgr)
				{
					p_task_mgr->on_finish_pve(object_id, rcd);
				}

				FightAgainst* p_against_table = GET_CONF(FightAgainst, common.param().against_id());
				if (NULL == p_against_table)
				{
					log_error("role[%lu] p_against_table[%u] null error", role_uid, common.param().against_id());
					return false;
				}

				if (p_against_table->drop() != 0)
				{
					if (!drop_manager_t::drop(role, p_against_table->drop(), log_enum::source_type_task_fight_win,
						proto::common::drop_sys_type_pve, 0, &rcd))
					{
						log_error("role[%lu] give task drop error", role_uid);
						return false;
					}
				}
			}
		}
	}

	ntf.mutable_rcd()->CopyFrom(rcd);

	return true;
}

bool fight_manager_t::on_dark_fight_result(
	const proto::common::combat_result& result,
	const proto::common::obj_single& self, 
	const proto::common::fight_common& common, 
	const proto::common::fight_camp& self_camp, 
	const proto::common::fight_camp& enemy_camp, 
	combat_ptr combat,
	proto::client::gc_combat_result_ntf& ntf)
{
	if (NULL == combat)
	{
		log_error("combat is null");
		return false;
	}

	uint64_t role_uid = string_util_t::string_to_uint64(self.object_uid());
	role_ptr p_role = role_manager_t::find_role(role_uid);
	if (NULL == p_role)
	{
		log_error("role is null [%lu]", role_uid);
		return false;
	}

	if (!common.param().has_dark())
	{
		log_error("common have no dark fight info!");
		return false;
	}

	uint32_t monster_id = common.param().dark().monster_id();
	if (monster_id == 0)
	{
		log_error("common have no dark fight monster id = 0");
		return false;
	}

	proto::common::role_change_data rcd;	// 怪物的掉落
	for (proto::common::obj_single obj : enemy_camp.objs().obj())
	{
		if (obj.object_type() == proto::common::SCENEOBJECT_MONSTER)
		{
			if (result.win_camp() == self.camp())
			{
				FightAgainst* p_against_table = GET_CONF(FightAgainst, common.param().against_id());
				if (NULL == p_against_table)
				{
					log_error("p_against_table[%u] null error", common.param().against_id());
					return false;
				}

				if (p_against_table->drop() != 0)
				{
					if (!drop_manager_t::drop(p_role, p_against_table->drop(), log_enum::source_type_dark_monster_die,
											  proto::common::drop_sys_type_pve, monster_id, &rcd))
					{
						log_error("role[%lu] give dark monster[%u] drop error", p_role->get_uid(), monster_id);
						return false;
					}
				}

				// 任务触发的话，任务更新
				if (common.param().dark().has_task_id())
				{
					task_mgr_ptr p_task_mgr = p_role->get_task_mgr();
					if (NULL != p_task_mgr)
					{
						proto::common::role_change_data task_rcd;
						p_task_mgr->try_change_task_state(TASK_TARGET_TYPE_MAP_DARK_MONSTER, task_rcd, p_role->get_map_tid(), 1);
					}
				}
			}
		}
	}

	ntf.mutable_rcd()->CopyFrom(rcd);
	return true;
}

bool fight_manager_t::on_family_long_result(const proto::common::combat_result& result, const proto::common::obj_single& self, const proto::common::fight_common& common, const proto::common::fight_camp& self_camp, const proto::common::fight_camp& enemy_camp, combat_ptr combat, proto::client::gc_combat_result_ntf& ntf)
{
	if (NULL == combat)
	{
		log_error("combat is null");
		return false;
	}

	if (!common.param().has_light())
	{
		log_error("common have no light fight info!");
		return false;
	}

	uint64_t scene_id = string_util_t::string_to_uint64(common.param().light().object_id());
	if (scene_id == 0)
	{
		log_error("on_family_long_result have no light_fight object id = 0");
		return false;
	}

	const scene_ptr& p_scene = scene_manager_t::find_scene(scene_id);
	if (NULL == p_scene)
	{
		log_error("scene[%lu] not find", scene_id);
		return false;
	}

	family_war_scene_ptr p_family_war_scene = p_scene->get_family_war_scene_ptr();
	if (NULL == p_family_war_scene)
	{
		log_error("scene[%lu] not family_war_scene", scene_id);
		return false;
	}

	uint64_t role_uid = string_util_t::string_to_uint64(self.object_uid());

	uint64_t fight_id = string_util_t::string_to_uint64(common.fight_id());

	p_family_war_scene->attack_long_end(role_uid, fight_id, combat);

	return true;
}

bool fight_manager_t::on_light_fight_result(
	const proto::common::combat_result& result, 
	const proto::common::obj_single& self, 
	const proto::common::fight_common& common, 
	const proto::common::fight_camp& self_camp, 
	const proto::common::fight_camp& enemy_camp, 
	combat_ptr combat, 
	proto::client::gc_combat_result_ntf& ntf)
{
	if (NULL == combat)
	{
		log_error("combat is null");
		return false;
	}

	uint64_t role_uid = string_util_t::string_to_uint64(self.object_uid());
	// 先处理不管role在不在线的逻辑
	// todo

	// 再处理role在线的逻辑
	role_ptr p_role = role_manager_t::find_role(role_uid);
	if (NULL == p_role)
	{
		log_error("role is null [%lu]", role_uid);
		return false;
	}

	if (!common.param().has_light())
	{
		log_error("common have no light fight info!");
		return false;
	}

	uint32_t monster_tid = common.param().light().monster_id();
	if (monster_tid == 0)
	{
		log_error("common have no light fight monster id = 0");
		return false;
	}

	proto::common::role_change_data rcd;	// 怪物的掉落
	for (proto::common::obj_single obj : enemy_camp.objs().obj())
	{
		if (obj.object_type() == proto::common::SCENEOBJECT_MONSTER)
		{
			if (result.win_camp() == self.camp())
			{
				task_mgr_ptr p_task_mgr = p_role->get_task_mgr();
				if (NULL != p_task_mgr)
				{
					proto::common::role_change_data task_rcd;
					p_task_mgr->try_change_task_state(TASK_TARGET_TYPE_KILL_VISIBLE_MONSTER, task_rcd, monster_tid, 1);
				}

				// 在副本中的处理
				if (p_role->is_in_dungeon())
				{
					uint32_t dungeon_id = p_role->get_role_scene_info().get_dungeon_id();
					
					DungeonTable* conf = GET_CONF(DungeonTable, dungeon_id);
					// 日常副本才记录
					if (NULL != conf && conf->dungeon_type() == dungeon::dungeon_type_daily)
					{
						// 得到怪物限制次数
						uint32_t limit_num = conf->get_reward_monster(monster_tid);

						// 如果有限制并且刷新期内已经等于限制次数了，不给奖励
						if (limit_num > 0)
						{
							if (p_role->get_dungeon_data().get_monster_kill_times(dungeon_id, monster_tid) >= limit_num)
							{
								proto::client::gc_dungeon_reward_limit_notify notify;
								notify.set_reply_code(common::errcode_enum::dungeon_reward_limit);
								notify.set_monster_tid(monster_tid);
								p_role->send_msg_to_client(op_cmd::gc_dungeon_reward_limit_notify, notify);
								continue;
							}

							p_role->get_dungeon_data().add_one_kill_times(conf, monster_tid);
						}
					}
				}

				FightAgainst* p_against_table = GET_CONF(FightAgainst, common.param().against_id());
				if (NULL == p_against_table)
				{
					log_error("role[%lu] p_against_table[%u] null error", role_uid, common.param().against_id());
					return false;
				}

				if (p_against_table->drop() != 0)
				{
					if (!drop_manager_t::drop(p_role, p_against_table->drop(), log_enum::source_type_light_monster_win,
						proto::common::drop_sys_type_pve, monster_tid, &rcd))
					{
						log_error("role[%lu] give light monster[%u] drop error", p_role->get_uid(), monster_tid);
						return false;
					}
				}
			}
		}
	}

	ntf.mutable_rcd()->CopyFrom(rcd);

	return true;
}

bool fight_manager_t::on_against_fight_result(
	const proto::common::combat_result& result,
	const proto::common::obj_single& self,
	const proto::common::fight_common& common,
	const proto::common::fight_camp& self_camp,
	const proto::common::fight_camp& enemy_camp,
	combat_ptr combat,
	proto::client::gc_combat_result_ntf& ntf)
{
	if (NULL == combat)
	{
		log_error("combat is null");
		return false;
	}

	uint64_t role_uid = string_util_t::string_to_uint64(self.object_uid());
	// 先处理不管role在不在线的逻辑
	// todo

	// 再处理role在线的逻辑
	role_ptr p_role = role_manager_t::find_role(role_uid);
	if (NULL == p_role)
	{
		log_error("role is null [%lu]", role_uid);
		return false;
	}
	
	proto::common::role_change_data rcd;
	for (proto::common::obj_single obj : enemy_camp.objs().obj())
	{
		if (obj.object_type() == proto::common::SCENEOBJECT_MONSTER)
		{
			if (result.win_camp() == self.camp())
			{
				FightAgainst* p_against_table = GET_CONF(FightAgainst, common.param().against_id());
				if (NULL == p_against_table)
				{
					log_error("role[%lu] p_against_table[%u] null error", role_uid, common.param().against_id());
					return false;
				}

				if (p_against_table->drop() != 0)
				{
					if (!drop_manager_t::drop(p_role, p_against_table->drop(), log_enum::source_type_against_pve,
						proto::common::drop_sys_type_pve, p_against_table->id(), &rcd))
					{
						log_error("role[%lu] give against[%d] drop error", p_role->get_uid(), p_against_table->id());
						return false;
					}
				}
			}
		}
	}

	ntf.mutable_rcd()->CopyFrom(rcd);

	return true;
}

bool fight_manager_t::on_pk_result(const proto::common::combat_result& result, const proto::common::obj_single& self, const proto::common::fight_common& common, const proto::common::fight_camp& self_camp, const proto::common::fight_camp& enemy_camp, combat_ptr combat, proto::client::gc_combat_result_ntf& ntf)
{
	if (NULL == combat)
	{
		log_error("combat is null");
		return false;
	}

	uint64_t role_uid = string_util_t::string_to_uint64(self.object_uid());
	if (enemy_camp.objs().obj_size() == 0)
	{
		log_error("enemy_camp is empty");
		return false;
	}
	uint64_t enemy_uid = string_util_t::string_to_uint64(enemy_camp.objs().obj(0).object_uid());
	bool is_win = result.win_camp() == self.camp();
	if (self.camp() == proto::common::combat_camp_1)
	{
		//攻击方结果
		return pk_manager_t::attacker_pk_result(role_uid, enemy_uid, is_win, ntf.mutable_rcd());
	}
	else
	{
		//被攻击方结果
		return pk_manager_t::defenser_pk_result(role_uid, enemy_uid, is_win, ntf.mutable_rcd());
	}
}

bool fight_manager_t::on_family_pk_result(const proto::common::combat_result& result, const proto::common::obj_single& self, const proto::common::fight_common& common, const proto::common::fight_camp& self_camp, const proto::common::fight_camp& enemy_camp, combat_ptr combat, proto::client::gc_combat_result_ntf& ntf)
{
	if (NULL == combat)
	{
		log_error("combat is null");
		return false;
	}

	uint64_t role_uid = string_util_t::string_to_uint64(self.object_uid());
	if (enemy_camp.objs().obj_size() == 0)
	{
		log_error("enemy_camp is empty");
		return false;
	}

	role_ptr p_self = role_manager_t::find_role(role_uid);
	if (NULL == p_self)
	{
		log_error("NULL == p_self[%llu]", role_uid);
		return false;
	}

	scene_ptr p_scene = scene_manager_t::find_scene(p_self->get_scene_id());
	if (NULL == p_scene)
	{
		log_error("role[%lu] scene not find", role_uid);
		return false;
	}

	uint64_t enemy_uid = string_util_t::string_to_uint64(enemy_camp.objs().obj(0).object_uid());

	role_ptr p_enemy = role_manager_t::find_role(enemy_uid);
	if (NULL == p_enemy)
	{
		log_error("NULL == p_enemy[%llu]", enemy_uid);
		return false;
	}

	if (result.win_camp() == self.camp())
	{
		p_scene->on_role_kill_other(p_self, p_enemy);
	}
	else
	{
		p_scene->on_other_kill_me(p_self, p_enemy);
	}

	return true;
}

bool fight_manager_t::on_tower_result(const proto::common::combat_result& result, const proto::common::obj_single& self, const proto::common::fight_common& common, const proto::common::fight_camp& self_camp, const proto::common::fight_camp& enemy_camp, combat_ptr combat, proto::client::gc_combat_result_ntf& ntf)
{
	if (NULL == combat)
	{
		log_error("combat is null");
		return false;
	}

	uint64_t role_uid = string_util_t::string_to_uint64(self.object_uid());
	// 先处理不管role在不在线的逻辑
	// todo

	// 再处理role在线的逻辑
	role_ptr p_role = role_manager_t::find_role(role_uid);
	if (NULL == p_role)
	{
		log_error("role is null [%lu]", role_uid);
		return false;
	}
	uint64_t trigger_id = common.param().trigger_id();
	tower_manager_t::fight_result(p_role, result.win_camp() == self.camp(), trigger_id, ntf.mutable_rcd());

	if (result.win_camp() == self.camp())
	{
		FightAgainst* p_against_table = GET_CONF(FightAgainst, common.param().against_id());
		if (NULL == p_against_table)
		{
			log_error("role[%lu] p_against_table[%u] null error", role_uid, common.param().against_id());
			return false;
		}

		if (p_against_table->drop() != 0)
		{
			if (!drop_manager_t::drop(p_role, p_against_table->drop(), log_enum::source_type_tower_fight_win,
				proto::common::drop_sys_type_pve, 0, ntf.mutable_rcd()))
			{
				log_error("role[%lu] give tower drop[%u] error", role_uid, p_against_table->drop());
				return false;
			}
		}
	}
	return true;
}

bool fight_manager_t::on_expedition_result(const proto::common::combat_result& result, const proto::common::obj_single& self, const proto::common::fight_common& common, const proto::common::fight_camp& self_camp, const proto::common::fight_camp& enemy_camp, combat_ptr combat, proto::client::gc_combat_result_ntf& ntf)
{
	if (NULL == combat)
	{
		log_error("combat is null");
		return false;
	}
	if (self_camp.objs().obj_size() == 0)
	{
		log_error("self_camp is null");
		return false;
	}
	uint64_t role_uid = string_util_t::string_to_uint64(self.object_uid());
	uint64_t initiator_uid = string_util_t::string_to_uint64(common.fight_initiator());
	if (role_uid != initiator_uid)
	{
		//只处理发起人的逻辑
		return true;
	}
	// 先处理不管role在不在线的逻辑
	// todo

	// 再处理role在线的逻辑
	role_ptr p_role = role_manager_t::find_role(role_uid);
	if (NULL == p_role)
	{
		log_error("role is null [%lu]", role_uid);
		return false;
	}
	auto p_expedition = p_role->get_expedition();
	if (NULL == p_expedition)
	{
		log_error("role is null [%lu]", role_uid);
		return false;
	}
	if (combat->is_escape_camp(proto::common::combat_camp_1))
	{
		//玩家逃跑不记录数据，不做战斗结算
		proto::common::fight_save_data fsd;
		proto::common::fight_save_data esd;
		std::vector<uint64_t> helpers;
		p_expedition->fight_result(common.param().stage_id(), false, fsd, esd, helpers);
		return true;
	}
	std::vector<uint64_t> helpers;
	for (int32_t i = 0; i < self_camp.objs().obj_size(); ++i)
	{
		uint64_t obj_uid = string_util_t::string_to_uint64(self_camp.objs().obj(i).object_uid());
		if (obj_uid != role_uid)
		{
			helpers.push_back(obj_uid);
		}
	}

	proto::common::fight_save_data fsd;
	combat->fill_initiator_save_data(&fsd);
	proto::common::fight_save_data esd;
	combat->fill_enemy_save_data(&esd);

	// 更新布阵 死亡的从远征布阵移除
	formation_ptr p_formation = p_role->m_formation_mgr.get_formation(proto::common::formation_type_expedition);
	if (NULL == p_formation)
	{
		log_error("on_expedition_result expedition_formation NULL");
		return false;
	}

	for (int i = 0; i < fsd.hero_list_size(); ++i)
	{
		if (fsd.hero_list(i).hp() == 0)
		{
			p_formation->remove(fsd.hero_list(i).hero_uid());
		}
	}
	proto::client::gc_update_formation_reply reply;
	reply.set_reply_code(0);
	p_role->m_formation_mgr.peek_datas(proto::common::formation_type_expedition, reply.mutable_formation());
	p_role->send_msg_to_client(op_cmd::gc_update_formation_reply, reply);

	p_expedition->fight_result(common.param().stage_id(), result.win_camp() == self.camp(), fsd, esd, helpers);
	return true;
}

bool fight_manager_t::on_page_pass_fight_result(
	const proto::common::combat_result& result, 
	const proto::common::obj_single& self, 
	const proto::common::fight_common& common, 
	const proto::common::fight_camp& self_camp, 
	const proto::common::fight_camp& enemy_camp, 
	combat_ptr combat, proto::client::gc_combat_result_ntf& ntf)
{
	if (NULL == combat)
	{
		log_error("combat is null");
		return false;
	}

	uint64_t role_uid = string_util_t::string_to_uint64(self.object_uid());
	role_ptr p_role = role_manager_t::find_role(role_uid);
	if (NULL == p_role)
	{
		log_error("role is null [%lu]", role_uid);
		return false;
	}

	proto::common::role_change_data rcd;	// 怪物的掉落
	for (proto::common::obj_single obj : enemy_camp.objs().obj())
	{
		if (obj.object_type() == proto::common::SCENEOBJECT_MONSTER)
		{
			if (result.win_camp() == self.camp())
			{
				FightAgainst* p_against_table = GET_CONF(FightAgainst, common.param().against_id());
				if (NULL == p_against_table)
				{
					log_error("role[%lu] p_against_table[%u] null error", role_uid, common.param().against_id());
					return false;
				}

				if (p_against_table->drop() != 0)
				{
					if (!drop_manager_t::drop(p_role, p_against_table->drop(), log_enum::source_type_page_pass_fight_pve,
						proto::common::drop_sys_type_pve, 0, &rcd))
					{
						log_error("role[%lu] give page pass pve monster[%u] drop error", p_role->get_uid(), 0);
						return false;
					}
				}
				page_manager_t::check_page_elite_task_status(p_role->get_uid(), common.param().page_id(), common.param().against_id());
			}
		}
	}
	ntf.mutable_rcd()->CopyFrom(rcd);
	return true;
}

bool fight_manager_t::on_offline_arena_result(const proto::common::combat_result& result, const proto::common::obj_single& self, const proto::common::fight_common& common, const proto::common::fight_camp& self_camp, const proto::common::fight_camp& enemy_camp, combat_ptr combat, proto::client::gc_combat_result_ntf& ntf)
{
	if (NULL == combat)
	{
		log_error("combat is null");
		return false;
	}
	if (self_camp.objs().obj_size() == 0)
	{
		log_error("self_camp is null");
		return false;
	}
	uint64_t role_uid = string_util_t::string_to_uint64(self.object_uid());
	uint64_t initiator_uid = string_util_t::string_to_uint64(common.fight_initiator());
	if (role_uid != initiator_uid)
	{
		//只处理发起人的逻辑
		return true;
	}
	// 先处理不管role在不在线的逻辑
	// todo
	proto::server::ge_offline_arena_fight_reply reply;
	// 再处理role在线的逻辑
	role_ptr p_role = role_manager_t::find_role(role_uid);
	if (NULL == p_role)
	{
		log_error("role is null [%lu]", role_uid);
		reply.set_is_win(0);
		env::server->send_msg_to_center(op_cmd::ge_offline_arena_fight_reply, role_uid, reply);
		return false;
	}
	// 更新离线竞技场布阵
	formation_ptr p_formation = p_role->m_formation_mgr.get_formation(proto::common::formation_type_offline_arena);
	if (NULL == p_formation)
	{
		log_error("on_expedition_result expedition_formation NULL");
		return false;
	}
	proto::server::ge_offline_arena_form_notify ntf_form;
	proto::common::mirror_fight_data* p_data = ntf_form.mutable_form_data();
	//布阵
	proto::common::obj_single* p_obj = p_data->mutable_obj();
	p_obj->set_object_uid(string_util_t::uint64_to_string(p_role->get_uid()));
	p_obj->set_name(p_role->get_name());
	p_obj->set_level(p_role->get_level());
	p_obj->set_object_type(proto::common::SCENEOBJECT_USER);
	p_obj->set_country(p_role->get_country_id());
	p_obj->set_life_level(p_role->get_lifestate_level());
	p_obj->set_fighting(p_formation->calc_fighting());
	p_obj->set_head_id(p_role->get_main_hero_plugin());
	p_formation->peek_fight_data(p_data->mutable_heros());
	p_data->set_formation_id(proto::common::formation_type_offline_arena);
	env::server->send_msg_to_center(op_cmd::ge_offline_arena_form_notify, p_role->get_uid(), ntf_form);
	uint32_t cur_class = common.param().offline_arena_class();
	auto p_class_conf = GET_CONF(OfflineArenaClass, cur_class);
	if (NULL == p_class_conf)
	{
		log_error("NULL == p_class_conf OfflineArenaClass[%d]", cur_class);
		return false;
	}
	//战斗结算
	if (result.win_camp() == self.camp())
	{
		reply.set_is_win(1);
		drop_manager_t::drop(p_role, p_class_conf->win_award(), log_enum::source_type_offline_arena_win, proto::common::drop_sys_type_offline_arena, 0, ntf.mutable_rcd());
	}
	else
	{
		reply.set_is_win(0);
		drop_manager_t::drop(p_role, p_class_conf->lose_award(), log_enum::source_type_offline_arena_lose, proto::common::drop_sys_type_offline_arena, 0, ntf.mutable_rcd());
	}
	env::server->send_msg_to_center(op_cmd::ge_offline_arena_fight_reply, role_uid, reply);
	return true;
}

void fight_manager_t::get_win_lose_obj(const proto::common::combat_result& combat_result, combat_ptr combat,
										  proto::common::obj_single* win_obj, proto::common::obj_single* lose_obj,
										  proto::common::hero_data *win_heros, proto::common::hero_data *lose_heros )
{
	if (NULL == combat)
	{
		log_error("combat is null");
		return;
	}
	if (NULL == win_obj)
	{
		log_error("win_obj NULL");
		return;
	}
	if (NULL == lose_obj)
	{
		log_error("lose_obj NULL");
		return;
	}
	if (NULL == win_heros)
	{
		log_error("win_heros NULL");
		return;
	}
	if (NULL == lose_heros)
	{
		log_error("lose_heros NULL");
		return;
	}

	uint64_t role_uid = 0;
	const proto::common::fight_data& fd = combat->get_fight_data();
	for (const proto::common::obj_single& obj : fd.self().objs().obj())
	{
		role_uid = string_util_t::string_to_uint64(obj.object_uid());
		role_ptr p_role = role_manager_t::find_role(role_uid);
		if (NULL != p_role)
		{
			p_role->syn_cross_id(0);
			role_manager_t::del_role(p_role->get_uid(), KICK_ROLE_REASON_ARENA_BATTLE_END);
		}

		if (combat_result.win_camp() == obj.camp())
		{
			win_obj->CopyFrom(obj);
			win_heros->CopyFrom(fd.self().heroes());
		}
		else
		{
			lose_obj->CopyFrom(obj);
			lose_heros->CopyFrom(fd.self().heroes());
		}
	}

	for (const proto::common::obj_single& obj : fd.enemy().objs().obj())
	{
		role_uid = string_util_t::string_to_uint64(obj.object_uid());
		role_ptr p_role = role_manager_t::find_role(role_uid);
		if (NULL != p_role)
		{
			p_role->syn_cross_id(0);
			role_manager_t::del_role(p_role->get_uid(), KICK_ROLE_REASON_ARENA_BATTLE_END);
		}

		if (combat_result.win_camp() == obj.camp())
		{
			win_obj->CopyFrom(obj);
			win_heros->CopyFrom(fd.enemy().heroes());
		}
		else
		{
			lose_obj->CopyFrom(obj);
			lose_heros->CopyFrom(fd.enemy().heroes());
		}
	}
}

bool fight_manager_t::on_cross_fight_test_end(const proto::common::combat_result& combat_result, combat_ptr combat)
{
	if (NULL == combat)
	{
		log_error("combat is null");
		return false;
	}

	const proto::common::fight_data& fd = combat->get_fight_data();
	for (const proto::common::obj_single& obj : fd.self().objs().obj())
	{
		uint64_t role_uid = string_util_t::string_to_uint64(obj.object_uid());
		role_ptr p_role = role_manager_t::find_role(role_uid);
		if (NULL != p_role)
		{
			p_role->syn_cross_id(0);
			role_manager_t::del_role(p_role->get_uid(), KICK_ROLE_REASON_CROSS_END);
		}
	}

	for (const proto::common::obj_single& obj : fd.enemy().objs().obj())
	{
		uint64_t role_uid = string_util_t::string_to_uint64(obj.object_uid());
		role_ptr p_role = role_manager_t::find_role(role_uid);
		if (NULL != p_role)
		{
			p_role->syn_cross_id(0);
			role_manager_t::del_role(p_role->get_uid(), KICK_ROLE_REASON_CROSS_END);
		}
	}

	return true;
}

bool fight_manager_t::on_cross_arena_rank_end(const proto::common::combat_result& combat_result, combat_ptr combat)
{
	if (NULL == combat)
	{
		log_error("combat is null");
		return false;
	}

	proto::server::sr_arena_battle_result_notify reply;

	const proto::common::fight_data& fd = combat->get_fight_data();
	uint32_t arena_ai_id = fd.common().param().arena_ai().arena_ai_id();
	reply.mutable_fight()->mutable_arena_ai()->set_arena_ai_id(arena_ai_id);

	get_win_lose_obj(combat_result, combat, reply.mutable_win_obj(), reply.mutable_lose_obj(), 
					 reply.mutable_win_heros(), reply.mutable_lose_heros());
	env::server->send_msg_to_area(op_cmd::sr_arena_battle_result_notify, 0, reply);

	return true;
}

bool fight_manager_t::on_fight_study_end(const proto::common::combat_result& combat_result, combat_ptr combat)
{
	if (NULL == combat)
	{
		log_error("combat is null");
		return false;
	}

	uint64_t win_uid = 0;
	uint64_t lose_uid = 0;

	const proto::common::fight_data& fd = combat->get_fight_data();
	for (const proto::common::obj_single& obj : fd.self().objs().obj())
	{

		uint64_t role_uid = string_util_t::string_to_uint64(obj.object_uid());

		if (combat_result.win_camp() == obj.camp())
		{
			win_uid = role_uid;
		}
		else
		{
			lose_uid = role_uid;
		}
	}

	for (const proto::common::obj_single& obj : fd.enemy().objs().obj())
	{
		uint64_t role_uid = string_util_t::string_to_uint64(obj.object_uid());

		if (combat_result.win_camp() == obj.camp())
		{
			win_uid = role_uid;
		}
		else
		{
			lose_uid = role_uid;
		}
	}

	role_ptr p_win_role = (win_uid != 0 ? role_manager_t::find_role(win_uid) : NULL);
	role_ptr p_lose_role = (lose_uid != 0 ? role_manager_t::find_role(lose_uid) : NULL);

	if (NULL != p_win_role && NULL != p_lose_role)
	{
		BROADCAST_SYS_NOTICE(sys_notice_enum::sys_notice_8, win_uid, 0, 0, p_win_role->get_name().c_str(), p_lose_role->get_name().c_str())
	}
	if (NULL != p_win_role)
	{
		task_mgr_ptr p_task_mgr = p_win_role->get_task_mgr();
		if (NULL != p_task_mgr)
		{
			proto::common::role_change_data rcd;
			p_task_mgr->try_change_task_state(TASK_TARGET_TYPE_PRACTISE, rcd, 1, 1);
		}
	}
	if (NULL != p_lose_role)
	{
		task_mgr_ptr p_task_mgr = p_lose_role->get_task_mgr();
		if (NULL != p_task_mgr)
		{
			proto::common::role_change_data rcd;
			p_task_mgr->try_change_task_state(TASK_TARGET_TYPE_PRACTISE, rcd, 1, 1);
		}
	}
	return true;
}

bool fight_manager_t::on_challenge_end(const proto::common::combat_result& combat_result, combat_ptr combat)
{
	if (NULL == combat)
	{
		log_error("combat is null");
		return false;
	}

	uint32_t result = 0;
	if (combat_result.win_camp() == proto::common::combat_camp_1)
	{
		result = proto::common::challenge_state_win;
	}
	else
	{
		result = proto::common::challenge_state_lose;
	}
	uint64_t challenger_uid = 0;
	const proto::common::fight_data& fd = combat->get_fight_data();
	for (const proto::common::obj_single& obj : fd.self().objs().obj())
	{
		challenger_uid = string_util_t::string_to_uint64(obj.object_uid());
	}
	uint64_t target_uid = 0;
	for (const proto::common::obj_single& obj : fd.enemy().objs().obj())
	{
		target_uid = string_util_t::string_to_uint64(obj.object_uid());
	}
	achieve_common_t::notify_progress_state(challenger_uid, proto::common::Achieve_Event_Type::ACHIEVE_EMPIRE_CITY_CHALLENGE_NUMBER);
	achieve_common_t::notify_progress_state(target_uid, proto::common::Achieve_Event_Type::ACHIEVE_EMPIRE_CITY_CHALLENGE_NUMBER);
	proto::server::ge_challenge_fight_reply reply;
	reply.set_result(result);
	env::server->send_msg_to_center(op_cmd::ge_challenge_fight_reply, challenger_uid, reply);
	return true;
}

bool fight_manager_t::on_light_fight_end(const proto::common::combat_result& combat_result, combat_ptr combat)
{
	if (NULL == combat)
	{
		log_error("combat is null");
		return false;
	}

	const proto::common::fight_data& fd = combat->get_fight_data();

	uint64_t monster_uid = string_util_t::string_to_uint64(fd.common().param().light().object_id());
	monster_ptr p_monster = monster_manager_t::find_monster(monster_uid);
	if (NULL == p_monster)
	{
		log_error("monster[%lu] is not find", monster_uid);
		return false;
	}

	uint32_t combat_total_time = combat->get_total_time() / 1000;

	// 怪物是否胜利了
	p_monster->leave_fight(combat_result.win_camp() == proto::common::combat_camp_2, combat_total_time, combat->get_self_death_hero_num());

	return true;
}

bool fight_manager_t::on_family_long_end(const proto::common::combat_result& combat_result, combat_ptr combat)
{
	if (NULL == combat)
	{
		log_error("combat is null");
		return false;
	}

	// 玩家打死了龙柱
	if (combat_result.win_camp() == proto::common::combat_camp_1)
	{
		const proto::common::fight_data& fd = combat->get_fight_data();

		uint64_t scene_id = string_util_t::string_to_uint64(fd.common().param().light().object_id());
		if (scene_id == 0)
		{
			log_error("on_family_long_end common have no light_fight object id = 0");
			return false;
		}

		const scene_ptr& p_scene = scene_manager_t::find_scene(scene_id);
		if (NULL == p_scene)
		{
			log_error("scene[%lu] not find", scene_id);
			return false;
		}

		family_war_scene_ptr p_family_war_scene = p_scene->get_family_war_scene_ptr();
		if (NULL == p_family_war_scene)
		{
			log_error("scene[%lu] not family_war_scene", scene_id);
			return false;
		}

		p_family_war_scene->family_war_long_death();
	}

	return true;
}

bool fight_manager_t::on_field_boss_end(const proto::common::combat_result& combat_result, combat_ptr combat)
{
	if (NULL == combat) {
		log_error("combat is null");
		return false;
	}

	const proto::common::fight_data& fd = combat->get_fight_data();
	uint64_t fight_id = string_util_t::string_to_uint64(fd.common().fight_id());
	uint64_t object_id = string_util_t::string_to_uint64(fd.common().param().light().object_id());
	field_boss_manager_t::role_leave_fight(object_id, fight_id, fd.self());

	return true;
}

bool fight_manager_t::check_fill_team_member_form_data(role_ptr p_role, uint32_t fight_type, uint32_t &level, formation_ptr &form, std::vector<role_ptr> &team_member_list)
{
	if (NULL == p_role)
	{
		log_error("fight_manager_t p_role NULL");
		return false;
	}

	bool is_troop_fight = false;
	if (p_role->get_troop_id() != 0)
	{
		troop_ptr p_troop = game_troop_mgr_t::get_troop(p_role->get_troop_id());
		if (NULL == p_troop)
		{
			log_error("p_troop NULL troop_id[%lu]", p_role->get_troop_id());
			return false;
		}
		// 只有队长才允许触发队伍战斗
		if (p_troop->get_leader_id() == p_role->get_uid())
		{
			troop_t::troop_member_vec member_list;
			p_troop->get_normal_members(member_list);
			if (member_list.size() > 1)
			{
				form = p_troop->get_formation_mgr().get_cur_formation();

				role_ptr p_member = role_ptr();

				for (uint32_t i = 0; i < member_list.size(); ++i)
				{
					troop_t::troop_member_ptr p_troop_member = member_list[i];
					if (NULL == p_troop_member)
					{
						log_error("p_troop_member NULL");
						continue;
					}
					if(p_troop_member->get_state() != 0)
						continue;

					if (p_troop_member->get_uid() == p_role->get_uid())
						p_member = p_role;
					else
						p_member = role_manager_t::find_role(p_troop_member->get_uid());

					if (NULL == p_member)
					{
						log_error("p_member_role NULL uid[%lu]", p_troop_member->get_uid());
						continue;
					}
					if (!p_member->can_enter_fight())
					{
						return false;
					}

					team_member_list.push_back(p_member);
				}

				// 生成队伍等级
				if (team_member_list.empty()) {
					log_error("check_fill_team_member_form_data member_list_size = 0");
					return false;
				} else if (team_member_list.size() == 1) {
					if (team_member_list[0] != NULL) level = team_member_list[0]->get_level();
				} else {
					uint32_t size = team_member_list.size();
					uint32_t temp_level = 0;
					for (uint32_t i = 0; i < size; ++i) {
						if (team_member_list[i] && team_member_list[i]->get_level() > 0) {
							temp_level += pow(team_member_list[i]->get_level(), size);
						}
					}

					temp_level /= size;

					level = pow(temp_level, 1.0f / size);
				}

				// 如果组队布阵中没有武将 按照队伍布阵规则填充队员武将
				//if (form->get_pos_hero().size() == 0)
				{
					if (!p_troop->fill_member_hero_to_formation())
					{
						log_error("fight_manager fill_member_hero_to_formation error");
						return false;
					}
				}

				p_troop->notify_formation();
				is_troop_fight = true;
			}
		}
		else
		{
			troop_t::troop_member_ptr p_member_self = p_troop->get_member(p_role->get_uid());
			if (NULL == p_member_self)
			{
				log_error("p_member_self NULL role_uid[%lu]", p_role->get_uid());
				return false;
			}
			if (p_member_self->get_state() == proto::common::troop_state_normal)
				return false;
		}
	}

	if(!is_troop_fight)
	{
		switch (fight_type)
		{
		case proto::common::fight_type_expedition:
			form = p_role->m_formation_mgr.get_formation(proto::common::formation_type_expedition);
			break;
		case proto::common::fight_type_family_war:
		case proto::common::fight_type_family_pk:
			form = p_role->m_formation_mgr.get_formation(proto::common::formation_type_family);
			break;
		default:
			form = p_role->m_formation_mgr.get_cur_formation();
			break;
		}
		level = p_role->get_level();
	}
	return true;
}

uint32_t fight_manager_t::get_troop_exp_2()
{
	auto troop_exp_2 = GET_CONF(Comprehensive, comprehensive_common::team_exp_times_2);
	uint32_t val = GET_COMPREHENSIVE_VALUE_1(troop_exp_2);
	return val;
}

uint32_t fight_manager_t::get_troop_exp_3()
{
	auto troop_exp_3 = GET_CONF(Comprehensive, comprehensive_common::team_exp_times_3);
	uint32_t val = GET_COMPREHENSIVE_VALUE_1(troop_exp_3);
	return val;
}

