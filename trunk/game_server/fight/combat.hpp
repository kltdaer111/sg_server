#ifndef __COMBAT_HPP__
#define __COMBAT_HPP__

#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include "tblh/SkillEffectTable.tbls.h"
#include "fight_obj.hpp"
#include "common/simple_timer.hpp"
#include "fight_common.hpp"

class buff_t;
typedef boost::shared_ptr<buff_t> buff_ptr;

class fight_hero_t;
typedef boost::shared_ptr<fight_hero_t> fight_hero_ptr;
typedef boost::weak_ptr<fight_hero_t> fight_hero_wptr;

typedef std::map<uint64_t, fight_obj_ptr> fight_obj_map;
typedef std::map<uint64_t, fight_hero_ptr> fight_hero_map;

typedef std::map<uint32_t, uint64_t> pos_hero;

typedef std::map<uint32_t, proto::common::combat_action_data> process_map;
typedef boost::shared_ptr<boost::asio::deadline_timer> boost_timer;

struct fight_camp_data
{
	uint32_t camp_obj_type	= 0;
	bool initiative	= false;		// ����
};

enum {
	camp_all = 0,
	camp_1   = 1,
	camp_2   = 2,
};

// ʹ�ü��ܵĲ�����Ϣ
struct round_use_skill_info
{
	SkillEffectTable* skill_conf = NULL;
	fight_hero_ptr attacker = NULL;
	fight_hero_ptr select = NULL;
	proto::common::combat_act_type type;
	bool trigger_by_buff = false;
	bool can_counter = false;
};

//�¼�����
struct buff_event_arg {
	fight_hero_ptr owner = NULL;
};

// �غ���ս��
class combat_t : public boost::enable_shared_from_this<combat_t>
{
public:
	combat_t(uint64_t fight_uid);
	~combat_t();

	void close();

	// ��ʼ��
	bool init(
		proto::common::fight_data& fd,
		bool can_form_before_fight,
		combat_type type);
	uint32_t gen_buff_id() { return m_gen_buff_id++; }

	combat_type get_combat_type() { return m_combat_type; }
	formation_ptr get_formation(uint32_t camp);
	// 	// �޸���
	// 	bool change_tactic(uint64_t role_uid, uint32_t tactic_id);
	// 	// �޸�����
	// 	bool change_form(formation_ptr form);

		// ��ʼloading
	bool loading_start();
	// loading����
	bool loading_end(uint64_t uid);
	// սǰ����ʼ
	bool combat_before_form_start();
	// ������в������
	bool check_all_form_end();
	// սǰ�������
	void do_form_end(uint64_t uid);
	// սǰ�������
	bool combat_before_form_end(uint64_t uid);
	// ����
	bool disposition(uint64_t uid, const proto::common::combat_form_data& data);
	// ��������
	bool initiative_disposition(uint64_t uid, const proto::common::combat_form_data& data);

	// �Ƿ��������Ŀ��
	static bool include_death_target(uint32_t type);
	// �Ƿ��������Ŀ��Ĺ�������
	static bool include_death_act_type(proto::common::combat_act_type type);
	// �Ƿ��Ѿ�Ŀ��
	static bool is_friend_target(uint32_t type);
	// �Ƿ�о�Ŀ��
	static bool is_enemy_target(uint32_t type);
	// ս�����Ž���
	bool fight_play_end(uint64_t uid, uint32_t hero_att_round);
	// �������
	bool on_role_login(uint64_t uid);
	// �������
	bool on_role_logout(uint64_t uid);
	// ����ս��
	bool update_fight(const proto::common::fight_state& state);
	bool update_fight(uint32_t camp, const proto::common::hero_state_data& state);

	// ͬ������Ѫ��
	bool sync_enemy_hp(const proto::common::hero_state_data& state);

	// ��������
	bool request_escape(uint64_t uid);
	void escape_end(uint64_t uid);

	// ����������
	void all_escape(uint32_t camp);
	uint32_t get_escape_num(uint32_t camp, uint32_t& all_num);
	// �ı�����Զ�ս��ģʽ
	bool change_role_auto_fight(bool is_auto, uint64_t uid = 0, const proto::client::hero_auto_fight_data* hero_data = NULL);
	// �ı�Ӣ���Զ�ս������
	bool change_hero_auto_fight(const proto::client::hero_auto_fight_data* hero_data = NULL);
	// �Զ�����
	bool auto_disposition(uint64_t obj_id = 0);
	// ս��ȡ��
	bool cancel();
	// ǿ��˫������ ʧ��
	bool all_fail();
	// ǿ��˫������ �ɹ�
	bool all_success();

	// ��ȡս����ʼʱ��
	uint32_t get_start_time() { return m_start_time; }
	// ��ȡս����ʱ��
	uint32_t get_total_time() { return m_total_time; }

	static bool auto_fight_inherit(uint32_t fight_type);

	static bool ignore_lineup(uint32_t fight_type);

	void auto_ignore_lineup(uint64_t uid);
	bool is_auto_ignore_lineup(uint64_t uid);
public:
	// �Ƿ����
	bool is_end();
	// �Ƿ�Ϊ����pve
	bool is_single_pve();
	// �Ƿ�Ϊ������pvp
	bool is_arena_pvp();
	// �Ƿ�Ϊ�Ǿ�����pvp
	bool is_un_arena_pvp();
	// ��ȡս��ID
	uint64_t get_fight_uid();
	// ��ȡ�غ���
	uint32_t get_round();
	// ��ȡ�佫�����غ�
	uint32_t get_hero_att_round();
	// ��ȡ�����佫��������
	uint32_t get_self_death_hero_num();
	// ��ȡ�佫�ܵ����˺�
	uint32_t get_hero_injured(uint64_t hero_uid);
	// ��ȡս������
	const proto::common::fight_data& get_fight_data();
	// ��ȡս������
	proto::common::fight_type get_fight_type() { return m_fight_data.common().param().type(); }
	// ��ȡŭ��
	uint32_t get_sp(uint64_t obj_id);
	// ����ŭ��
	void set_sp(uint64_t obj_id, uint32_t value);
	void add_sp(uint64_t obj_id, uint32_t value);
	void sub_sp(uint64_t obj_id, uint32_t value);
	// ����ŭ��
	void sub_disposition_sp(uint64_t obj_id, uint32_t value);
	uint32_t get_disposition_sp(uint64_t obj_id);
	// ���ӳ��ִ���
	void add_attack_count(uint64_t obj_id, uint32_t value = 1);
	// �Ƿ�pve
	bool is_pve(uint32_t& npc_camp);
	bool is_pve();
	// ��ȡ���ȳ��ֵ���Ӫ
	uint32_t get_initiative_camp();
	// ��ȡrole_uid����Ӫ
	uint32_t get_role_camp(uint64_t uid) const;
	// ��ȡ�����佫�������Ƿ�ȫ������
	void get_self_death_hero(uint64_t role_uid, std::set<uint64_t>& death_hero_uids) const;

	// ��ȡս�����ֵ
	int32_t get_max_att_speed();
	// �޸ı��
	void set_mark_id(uint32_t mark_id, uint64_t fight_hero_uid);
	// ��ȡ��������
	uint32_t get_team_member_size(uint64_t uid);

protected:
	// loading��ʱ������
	static void on_loading_timer(const boost::weak_ptr<combat_t> pw_combat, const boost::system::error_code& ec);
	// սǰ����ʱ������
	static void on_form_timer(const boost::weak_ptr<combat_t> pw_combat, const boost::system::error_code& ec);
	// սǰ���������ʱ������
	static void on_form_ready_timer(const boost::weak_ptr<combat_t> pw_combat, const boost::system::error_code& ec);
	// ���������ʱ������
	static void on_disposition_ready_timer(const boost::weak_ptr<combat_t> pw_combat, const boost::system::error_code& ec);
	// ׼����ʱ������
	static void on_prepare_timer(const boost::weak_ptr<combat_t> pw_combat, const boost::system::error_code& ec);
	// ���Ŷ�ʱ������
	static void on_play_timer(const boost::weak_ptr<combat_t> pw_combat, const boost::system::error_code& ec);
	// �غϿ�ʼ��ʱ������
	static void on_check_auto_dis_timer(const boost::weak_ptr<combat_t> pw_combat, const boost::system::error_code& ec);

	// loading��ʱ������
	void deal_on_loading_timer();
	// սǰ����ʱ������
	void deal_on_form_timer();
	// սǰ���������ʱ������
	void deal_on_form_ready_timer();
	// ���������ʱ������
	void deal_on_disposition_ready_timer();
	// ׼����ʱ������
	void deal_on_prepare_timer();
	// ���Ŷ�ʱ������
	void deal_on_play_timer();
	// �غϿ�ʼ��ʱ������
	void deal_on_check_auto_dis_timer();

protected:

	// ��ʼ��ս������
	bool init_fight_obj(proto::common::obj_data* od);
	// ��ʼ��loading�׶ε�ս���佫(���������ӽ�ս����Ӫ�� ֻ�������ݵĳ�ʼ�� �����ͻ���չʾ)
	bool init_loading_fight_hero(proto::common::hero_data* hd);
	// ��ʼ��ս���佫
	bool init_fight_hero(proto::common::hero_data* hd);
	// ��ʼ�������佫
	bool init_fight_help_hero(proto::common::hero_data* hd);
	// �Զ�loading
	bool auto_loading();
	// �Զ�����
	bool auto_play();

protected:
	// ��սǰ����˫�����ݵĲ�����Ϣ
	bool reset_all_camp_form_data();
	// ��սǰ������һ���ݵĲ�����Ϣ
	bool reset_single_camp_form_data(uint32_t camp);
	// �غϿ�ʼǰ
	bool round_start_before();
	// �غϿ�ʼ
	bool round_start();
	// ս����ʼ�¼�
	void fight_start_event();
	// ���ҵ�ǰ�����佫
	void find_cur_att_hero();
	// ��ʼ׼��
	bool prepare_start();
	// ׼������
	bool prepare_end();
	// ��ʼ����
	bool play_start();
	// ���Ž���
	bool play_end();
	// �غϽ���
	bool round_end();
	// ս������
	bool combat_end();
	// �������ֶ���
	void add_first_att_hero_list(fight_hero_ptr p_fight_hero);
	// �Ƴ����ֶ���
	void remove_first_att_hero_list(fight_hero_ptr p_fight_hero);
	// �������ֶ���
	void sort_first_att_hero_list();
	// ��������ٶ�ֵ����
	void sort_att_speed_list(std::vector<fight_hero_ptr> &res);

	// ����¼�
	void check_event(combat_event_step_type type);
	// ��鲨��ת��
	bool check_wave_change();
	// ��������佫�¼�
	void check_hide_event(combat_event_step_type type);
	// ���������佫��ս��
	void add_hide_hero_to_combat(fight_hero_ptr p_hero);
	// ���ս���Ƿ����
	bool check_combat_end();
	// ���loading�Ƿ����
	bool check_loading_finish();
	// ��ⲿ���Ƿ����
	bool check_disposition_finish();
	// ��ⲥ���Ƿ����
	bool check_play_finish();
	//����Ƿ���ǰ����ս��
	bool check_end_fight_ahead();
	//�Ƿ������һ������
	bool is_last_wave() { return m_wave_heros.size() == 0; }

	void set_win_info(proto::common::combat_camp win_camp);
public:
	void gm_win_fight();

	void gm_clear_all_hero_cd();

	// ������ս������ҹ㲥��Ϣ
	template<typename T_MSG>
	bool send_msg_to_all(uint16_t cmd, const T_MSG& protobuf_msg);
	// ������ս����һ����Ӫ��ҹ㲥��Ϣ
	template<typename T_MSG>
	bool send_msg_to_camp(uint32_t camp, uint16_t cmd, const T_MSG& protobuf_msg);

	// ���combat_data����
	void fill_combat_data(
		proto::common::combat_data* data,
		const proto::common::combat_action_data* process = NULL,
		bool syn_fight_data = false);

	// ���ͳ������
	void fill_count_data(proto::common::combat_count_data* data);

	// ������״̬
	void fill_combat_obj_state(proto::common::fight_obj_state_data* data);

	// ����佫����˳��
	void fill_combat_hero_att_order(proto::common::combat_hero_att_order* data, const std::vector<fight_hero_ptr>& res);

	// ���ս���ָ�����
	void fill_combat_recovery_data(proto::common::fight_recovery_data* data);

	// ����ս������
	fight_obj_ptr find_obj(uint64_t uid);
	// ����ս���佫
	fight_hero_ptr find_hero(uint64_t uid);
	// �ٻ�
	fight_hero_ptr summon_hero(proto::common::hero_data* ntf);

	// ͬ��ս������
	void syn_combat_data(
		const proto::common::combat_action_data* process = NULL,
		uint64_t uid = 0,
		bool syn_fight_data = false);

	// ͬ�����״̬
	void syn_combat_obj_state(uint64_t uid = 0);

	// ͬ���ָ�ս������
	void syn_combat_recovery_data(uint64_t uid = 0);

	// ���һ���Ƿ�ȫ������
	bool check_camp_all_dead(uint32_t camp);
	// ��ȡ����佫����
	uint32_t get_alive_hero_num(uint32_t camp);
	// ��ȡ�ٻ���λ��
	bool get_summon_pos(uint32_t camp, uint32_t& pos);
	// ���������¼�
	void on_master_die(uint32_t camp, uint64_t master);
	//���ָ��Ӣ������
	bool check_camp_hero_dead(uint32_t camp, uint32_t monst_id);
	//�������Ӣ��Ѫ��
	uint32_t get_camp_owner_hp_rate(uint32_t camp);
	//���ȫ��Ӣ��ƽ��Ѫ��
	uint32_t get_camp_team_avage_hp_reate(uint32_t camp);
	//�����Ӫ2,1��λѪ��
	uint32_t get_camp_hero_pos_hp_rate(uint32_t camp, uint32_t pos);
public:
	//��ȡ����ٶ��佫
	fight_hero_ptr get_max_speed_attacker();
	// ����˳��(���)
	bool get_alive_attack_order(std::vector<fight_hero_ptr>& res);
	// ȫ���佫�ĳ���˳��
	bool get_all_attack_order(std::vector<fight_hero_ptr>& res);
	// ���㲥��ʱ��
	uint32_t get_fight_play_time(uint32_t round);
	// ��ʱ����ս��
	static void on_end_timer(const boost::weak_ptr<combat_t> pw_combat, const boost::system::error_code& ec);
	//�����������ٶ�	
	void reset_max_speed();

	void cancel_obj_auto_fight(uint64_t uid);
public:
	// ��ȡ����ս���佫
	const fight_hero_map& get_all_heros() { return m_heros; }
	fight_hero_map& get_all_heros_ptr() { return m_heros; }
	// ��ȡ���ܹ���Ŀ����Ӫ�佫
	const pos_hero* get_skill_target_camp_pos_hero(fight_hero_ptr attaker, SkillEffectTable* skill_conf);
	// ��ȡ���ܹ���Ŀ����Ӫ�佫
	void get_skill_target_heroes(fight_hero_ptr attaker, SkillEffectTable* skill_conf, std::vector<fight_hero_ptr>& result);
	// ��⼼��Ŀ������
	bool check_skill_target_type(SkillEffectTable* skill_conf, fight_hero_ptr attacker, fight_hero_ptr target, bool disposition = false);
	// ��ȡĳ��Ӫ�佫
	const pos_hero* get_camp_pos_hero(uint32_t camp);
	// ��ȡĳ��Ӫĳλ���佫
	fight_hero_ptr get_hero_by_camp_and_pos(uint32_t camp, uint32_t pos);
	// ��ȡĳ��Ӫĳtid�佫
	fight_hero_ptr get_hero_by_camp_and_tid(uint32_t camp, uint32_t tid);
	// ��ȡĳ��Ӫ�佫
	bool get_camp_heros(std::vector<fight_hero_ptr>& result, uint32_t camp, uint32_t except_pos = 0);
	// �����ȡĳ��Ӫ��һ������佫���������Ϊ�Լ���Ӫ�����ʱ���ų��Լ�
	fight_hero_ptr get_random_alive_hero_by_camp(uint32_t camp);
	// ��ȡĳ��Ӫ����佫
	bool get_camp_alive_heros(std::vector<fight_hero_ptr>& result, uint32_t camp, uint32_t except_pos = 0);
	// ��յз���Ӫ�����佫
	void clear_enemy_camp_death_heros();
	// �ж���Ӫ�Ƿ���������
	bool is_escape_camp(uint32_t camp);
	// ��ȡս�������˵ı�������
	bool fill_initiator_save_data(proto::common::fight_save_data* p_data);
	// ��ȡNPC����Ӫ�����ı������� hero_uidΪkey��hpΪvalue
	bool fill_enemy_save_data(proto::common::fight_save_data* p_data);
	// ��ȡ���Ͽ�λ��(1-9˳�����)
	uint32_t get_empty_pos(uint32_t camp);
	// ����Ŀ��
	fight_hero_ptr find_default_target(fight_hero_ptr attacker, SkillEffectTable* skill_conf);

	//���һ��ŵ�Ŀ�꣬�ǻþ�״̬
	fight_hero_ptr find_alive_target_of_not_mirage(fight_hero_ptr attacker, SkillEffectTable* skill_conf);

	// ����Ŀ��
	bool find_target(
		fight_hero_ptr attaker,
		SkillEffectTable* skill_conf,
		std::vector<fight_hero_ptr>& targets,
		std::vector<fight_hero_ptr>& spurting_heroes,
		proto::common::combat_act_type type,
		fight_hero_ptr select = NULL);

	// ����Ŀ��
	bool find_target_by_skill_conf(
		fight_hero_ptr attaker,
		SkillEffectTable* skill_conf,
		std::vector<fight_hero_ptr>& targets,
		std::vector<fight_hero_ptr>& spurting_heroes,
		fight_hero_ptr select,
		proto::common::combat_act_type type);

	// ���Ŀ��
	bool find_random_target(
		fight_hero_ptr attaker,
		SkillEffectTable* skill_conf,
		std::vector<fight_hero_ptr>& targets,
		fight_hero_ptr select,
		uint32_t num = 1,
		bool b_has_select = true);

	// ����Ŀ��
	bool find_spurting_target(
		fight_hero_ptr attaker,
		SkillEffectTable* skill_conf,
		std::vector<fight_hero_ptr>& targets,
		std::vector<fight_hero_ptr>& spurting_heroes,
		fight_hero_ptr select,
		uint32_t num = 1);

	// 	// �����е�����Ŀ��
	// 	bool find_vertical_target(
	// 		fight_hero_ptr attaker,
	// 		SkillEffectTable* skill_conf,
	// 		std::vector<fight_hero_ptr>& targets,
	// 		fight_hero_ptr select);
	// 
	// 	// �����ŵ�����Ŀ��
	// 	bool find_horizon_target(
	// 		fight_hero_ptr attaker,
	// 		SkillEffectTable* skill_conf,
	// 		std::vector<fight_hero_ptr>& targets,
	// 		fight_hero_ptr select);

		// ����Ŀ��
	bool find_all_target(
		fight_hero_ptr attaker,
		SkillEffectTable* skill_conf,
		std::vector<fight_hero_ptr>& targets,
		fight_hero_ptr select);

	// Ѫ�����ٵ�Ŀ��(�������)
	bool find_min_hp_target(
		fight_hero_ptr attaker,
		SkillEffectTable* skill_conf,
		std::vector<fight_hero_ptr>& targets,
		fight_hero_ptr select);

	// Ѫ�����ٵļ���Ŀ��
	bool find_some_min_hp_target(
		fight_hero_ptr attaker,
		SkillEffectTable* skill_conf,
		std::vector<fight_hero_ptr>& targets,
		fight_hero_ptr select);


	// Ѫ������ָ��ֵ�ļ���Ŀ��
	bool find_some_grater_hp_target(
		fight_hero_ptr attaker,
		SkillEffectTable* skill_conf,
		std::vector<fight_hero_ptr>& targets,
		fight_hero_ptr select);

	// ����Ŀ��
	fight_hero_ptr select_confused_target(fight_hero_ptr attaker);

	// ��������ߵ�Ŀ��  
	fight_hero_ptr select_max_atk_target(fight_hero_ptr attaker, SkillEffectTable* skill_conf);
	// ְҵΪ4��5��6��Ŀ�꣨��Ƥ��
	fight_hero_ptr select_weak_target(fight_hero_ptr attaker, SkillEffectTable* skill_conf);

	// Ѫ�����ٵ�1��Ŀ��
	fight_hero_ptr select_min_hp_target(fight_hero_ptr attaker, SkillEffectTable* skill_conf);
	// Ѫ�����ٵ�1��Ŀ��
	fight_hero_ptr select_min_hp_target(uint32_t camp, uint64_t except = 0);
	//Ѫ������1��Ŀ��
	fight_hero_ptr select_max_hp_target(uint32_t camp);
	// Ѫ�����ٵļ���Ŀ��
	bool select_min_hp_target(fight_hero_ptr attaker, SkillEffectTable* skill_conf, std::vector<fight_hero_ptr>& targets);

	// �ж�����͵�1��Ŀ��
	fight_hero_ptr select_min_att_speed_target(uint32_t camp);

	//��ȡָ��id�佫
	bool select_hero_id_target(std::vector<fight_hero_ptr>& result, uint32_t camp, uint32_t hero_id);

	//��ȡĳ�����佫
	fight_hero_ptr select_hero_by_attr(uint32_t camp, uint64_t  except, uint32_t attr_type, fight_hero_ptr attaker, SkillEffectTable *skill_conf, bool is_max = true);

public:
	// ս���佫����
	bool on_fight_hero_dead(fight_hero_ptr hero);
	// ��ȡ��ǰ����
	proto::common::combat_action_data* get_cur_process();
	// ����ս����Ϊ
	void add_combat_act(proto::common::combat_act_type type, uint32_t value, uint64_t target, uint32_t remain_value);
	// ��ȡ��������
	proto::common::combat_act_step get_attack_step() { return m_attack_step; }
	// ���ù�������
	void set_attack_step(proto::common::combat_act_step step) { m_attack_step = step; }

	fight_hero_ptr get_attack_hero();

	//��ȡ��ʵ�¼�
	uint32_t get_real_event(uint32_t event);
protected:

	uint64_t get_attack_obj_uid();

	// ������Ƿ���Ч
	bool check_tactic_valid(uint32_t camp);
	// ִ����Ч��
	bool do_tactic_buff(uint32_t camp, bool is_add);

	//----------------------------------------��ֻ�þ���----------------------------------
public:
	// ��ȡ����ID
	uint32_t get_against_id();

	// ��ȡ����
	std::pair<uint32_t, uint32_t> get_exp(bool is_win);

	// ��ȡ�������
	uint32_t get_obj_count(uint32_t camp);

	// ��ȡ���������佫�б�,����Ϊhero_uid��vector
	void get_role_heros(uint64_t role_uid, std::vector<uint64_t>& hero_list);

	// ��ȡɱ���Ĺ����б� ������, ����Ϊ�����佫���е�task_sign�����ʶ��vector
	void get_task_monster(std::vector<uint32_t>& monster_list);

	void fight_hero_on_init();

	fight_hero_ptr get_att_fight_hero();
	void get_camp_fight_obj_uid(uint32_t camp, std::vector<uint64_t>& obj_uid_list);

	// �Ƿ�ɹ�ս
	bool can_watch(uint32_t watch_num = 0);
	uint32_t get_max_watch_num(uint32_t fight_type);
	// ���ӹ�ս���
	void add_watching_role(uint64_t uid) { m_watching_roles.insert(uid); }
	// ��ȡ��ս���
	const std::set<uint64_t>& get_watching_role_list() { return m_watching_roles; }
	// �Ƴ���ս��� 
	// ���øú������ⲿ���������ֵ�����++!!!
	void remove_watching_role(uint64_t uid) { m_watching_roles.erase(uid); }
	// �Ƴ�ȫ����ս���
	void remove_all_watching_roles();
	// ��ȡ��ս�����
	uint32_t get_watching_role_num() { return m_watching_roles.size(); }

	// ���ӱ��غ�ʹ�õļ��ܶ���
	void add_round_use_skill_list(const round_use_skill_info& skill_info);
	// ִ�б��غ�ʹ�õļ��ܶ���
	void deal_round_use_skill_list();
	// �Ƴ�ȫ�����غ�ʹ�õļ��ܶ���
	void remove_all_round_use_skill_list();

	uint32_t get_round_add_buff_count() { return m_round_add_buff_count; }
	void add_round_buff_add_count() { ++m_round_add_buff_count; }
	//�Ƿ�������������������ս��	
	bool is_allow_reconnect();

	//����һ�غ�����
	void on_round_end();

	void add_beattack_hero(fight_hero_ptr fight_hero);

	//��ʼ��չ����û���Ǻ�,���ڵ÷�2���ӿڣ�ʱ���������ˣ�
	void add_hero_event(uint64_t hero_uid, const std::set<uint32_t> *event_list, uint32_t buff_id);

	void del_hero_event(uint64_t hero_uid, const std::set<uint32_t> *event_list, uint32_t buff_id);

	void add_hero_event(uint64_t hero_uid, uint32_t event, uint32_t buff_id);

	void del_hero_event(uint64_t hero_uid, uint32_t event, uint32_t buff_id);

	const std::map<uint32_t, std::map<uint64_t, uint32_t>> &hero_event_map() { return m_hero_event_map; }

	void close_sync_user_combat_data(uint64_t uid);

	void open_sync_user_combat_data(uint64_t uid);

	proto::common::combat_step get_round_step() { return m_step; }

	proto::common::combat_state get_round_state() { return m_state; }

	void set_is_replay(uint32_t  type);

	bool is_replay() { return m_is_replay; }

	void clear_last_combat_log();

	void set_level(uint32_t level) { m_level = level; }

	uint32_t get_level() { return m_level; }

	// ¼����������
	void peek_fight_obj(proto::common::fight_type type, proto::common::video_obj_base* obj1, proto::common::video_obj_base* obj2);
	void peek_combat_log(proto::common::fight_video_data& video_data);
public:
	void set_hero_skill_cd(fight_hero_ptr p_hero);

	std::map<uint64_t, fight_hero_ptr> m_hero_skill_cd_list;		// ����cd
public:
	uint8_t get_fight_data_type() { return m_fight_data_type; }
	void set_fight_data_type(uint8_t type) { m_fight_data_type = type;  }
	uint8_t m_fight_data_type = 0;	//0,ʵʱPVP  1������� 2,��������
private:

	uint64_t m_fight_uid = 0;															// ս��ID
	proto::common::combat_step m_step = proto::common::combat_step_loading;				// ս������
	proto::common::combat_state m_state = proto::common::combat_state_prepare;				// �غ�״̬
	combat_type	m_combat_type = combat_type_single_role;                                // ս������ ���˻����
	uint32_t m_round = 1;																// �غ���
	uint32_t m_wave = 1;																// ����
	uint32_t m_start_time = 0;															// ��ʼʱ��
	uint32_t m_total_time = 0;															// ս����ʱ��	
	uint32_t m_hero_att_round = 0;														// �佫�����غ�
	uint32_t m_gen_buff_id = 1;															// buff����ID
	uint32_t m_end_time = 0;															// ��ǰ���̻���״̬����ʱ��
	int32_t m_max_att_speed = 0;														// ����ս�����ַ�ֵ
	uint32_t m_story_addi_time = 0;														// ս���еľ���Ӱ���ʱ��
	uint32_t m_round_add_buff_count = 0;												// buff��ȫУ��
	bool m_can_form_before_fight = true;												// �Ƿ��սǰ����
	fight_obj_map m_objs;																// ����ս������
	fight_hero_map m_heros;																// ����ս���佫
	fight_hero_map m_hide_heros;														// ����ս���佫
	fight_hero_map m_wave_heros;														// ����ս���佫
	proto::common::fight_data m_fight_data;												// ս����ʼ����
	std::map<uint32_t, pos_hero> m_form;												// ����
	std::vector<fight_hero_ptr> m_first_att_hero_list;									// ���ȳ��ֵ��佫
	std::vector<fight_hero_ptr> m_alive_hero_list;										// ���ŵ��佫
	std::vector<round_use_skill_info> m_round_use_skill_list;							// ���غϼ���ʹ�õļ��ܶ���
	fight_hero_ptr m_p_att_fight_hero;													// ���غϹ����佫
	std::map<uint64_t, fight_hero_ptr> m_beattack_hero_list;									// ���غϱ������佫����
	process_map m_process;																// ս������
	proto::common::combat_act_step m_attack_step = proto::common::combat_act_step_none;	// ��������
	std::map<uint32_t, fight_camp_data> m_camp_data;									// ˫����Ӫ����
	proto::common::combat_result m_combat_result;										// ս�����
public:
	static uint32_t get_init_fight_constant() { return init_fight_constant; }
	
	static uint32_t get_init_crit_multiple() { return init_crit_multiple;  }

	static uint32_t get_min_crit_multiple() { return min_crit_multiple; }
	
	static uint32_t get_max_crit_multiple() { return max_crit_multiple; }
private:
	std::set<uint64_t>	m_watching_roles;													// ��ս���
	simple_timer_t	m_timer;																// ��ʱ��
	simple_timer_t	m_dis_timer;															// ����ʱ��
	boost_timer		m_end_timer;															// ������ʱ��
	bool			m_is_pass_dis_timer = false;											//�Ƿ����ù�����ʱ��
	
	uint32_t m_sync_hurt_time = 0;														//ͬ��ʱ��
	std::map<std::string, proto::common::hero_state> m_hurt_hp_map;						//ͬ��Ѫ��
	static uint32_t s_sync_tick_msec;													//ͬ�����(����)

	std::map<uint32_t, std::map<uint64_t, uint32_t>> m_hero_event_map;					//�¼�����Ӣ��  <�¼�����FIGHT_EVENT, <hero��ID, ��hero���¼�����>>

	static uint32_t init_fight_constant;		//��ʼ�˺�����
	static uint32_t min_crit_multiple;			//��ʼ��ͱ���
	static uint32_t max_crit_multiple;			//��ʼ�����
	static uint32_t init_crit_multiple;			//��ʼ����
	
	bool m_is_new_wave = false;					//��������в��κ�����һ�ι����ٶȵ���ʷ��������
	bool m_is_replay = false;					//�Ƿ�ط�
	bool m_level = 0;							//ս���й���ȼ�

public:
	//��ȡ������camp��Ϣ
	uint8_t get_real_camp_type(uint32_t type, uint32_t user_camp);

	void add_public_buff(uint64_t uid, buff_ptr  p_buff, uint32_t type, uint32_t user_camp );

	void set_public_buff_off(uint64_t uid);
	void set_public_buff_on(uint64_t uid);

	void reset_use_public_buff();
	void add_camp_buff( buff_ptr p_buff );
	//p_triger �����¼����ˣ� p_event_owner �¼�ִ����
	void do_public_buff( uint32_t event, fight_hero_ptr p_trigger, fight_hero_ptr p_event_owner, uint32_t camp );
	void run_public_buff(uint32_t comp_type, uint32_t event, fight_hero_ptr p_trigger, fight_hero_ptr p_event_owner, buff_ptr p_buff, uint32_t step );
	void add_public_event(uint32_t buff_tid, const std::set<uint32_t> *event_list);
	void distribute_special_buff();
	void clear_phase_run_info( uint64_t uid );

	//�⻷buff
	typedef std::map<uint32_t, buff_ptr> buff_map_t;
	typedef std::multimap<uint32_t, buff_ptr> buff_multimap_t;

	std::map<uint64_t, buff_map_t> hero_buff_map;	//ÿ����ҷŵĹ⻷buffmap, ( buff_map_t ��key ��type_id);
	std::map<uint32_t, buff_multimap_t> level_buff_map;	//ÿ����ҷŵĹ⻷buffmap  ( buff_map_t ��key ��level);

	std::map<uint32_t, buff_ptr> camp1_buff;		//��camp1Ӣ�۵�buff
	std::map<uint32_t, buff_ptr> camp2_buff;		//��camp2Ӣ�۵�buff
	std::map<uint32_t, buff_ptr> all_buff;			//����������Ч��buff

	std::map<uint32_t, std::set<uint32_t>> public_event_list;		//�⻷buff�¼�
};

typedef boost::shared_ptr<combat_t> combat_ptr;
typedef boost::weak_ptr<combat_t> combat_wptr;


template<typename T_MSG>
bool combat_t::send_msg_to_all(uint16_t cmd, const T_MSG& protobuf_msg)
{
	for (auto objpair : m_objs)
	{
		if (NULL == objpair.second)
			continue;
		// �����������״̬����Ҫ����Ϣ����ȥ��
		if ( objpair.second->is_escape_end() || !objpair.second->is_send_combat() )
			continue;

		objpair.second->send_msg_to_client(cmd, protobuf_msg);

	}
	std::set<uint64_t>::iterator iter = m_watching_roles.begin();
	for (; iter != m_watching_roles.end(); ++iter)
	{
		role_ptr role = role_manager_t::find_role(*iter);
		if (NULL == role)
		{
			return false;
		}

		role->send_msg_to_client(cmd, protobuf_msg);
	}

	return true;
}

template<typename T_MSG>
bool combat_t::send_msg_to_camp(uint32_t camp, uint16_t cmd, const T_MSG& protobuf_msg)
{
	for (auto objpair : m_objs)
	{
		if (NULL != objpair.second && objpair.second->get_camp() == camp)
		{
			objpair.second->send_msg_to_client(cmd, protobuf_msg);
		}
	}

	return true;
}

#endif//__COMBAT_HPP__