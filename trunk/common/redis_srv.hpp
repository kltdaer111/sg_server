#pragma once

#include <boost/scoped_ptr.hpp>
#include <boost/thread.hpp>
#include <boost/asio/deadline_timer.hpp>

#include "redis_unit.hpp"
#include "redis_common.hpp"
/**
* \brief redis������������redis���ݼ����Լ�redis���ݱ��浽mysql�Ĳ���
*/

class redis_read_unit_t;
typedef boost::shared_ptr<redis_read_unit_t> redis_read_unit_ptr;
typedef boost::weak_ptr<redis_read_unit_t> redis_read_unit_wptr;

class redis_db_read_t;
typedef boost::shared_ptr<redis_db_read_t> redis_db_read_ptr;
typedef boost::weak_ptr<redis_db_read_t> redis_db_read_wptr;

class redis_save_unit_t;
typedef boost::shared_ptr<redis_save_unit_t> redis_save_unit_ptr;

class redis_srv_t : public redis_base_t
{
public:
	redis_srv_t();
	virtual ~redis_srv_t();

public:
	// ��ʼ��
	bool init(const char* host, // redis ip
		int32_t port,			// redis port
		const char* passwd,		// redis auth
		int32_t db,				// redis select
		const sql_table *db_table, // ��Ҫ���ص��ֶ�
		uint32_t thread_num,	// �߳���
		uint32_t read_num,		// ��ȡ������Ԫ
		uint32_t save_interval, // ������
		uint32_t save_count_limit, // ���α�������
		uint32_t save_num,		// ���洦����Ԫ
		struct ConnectionPool_S* zdb_pool);

	// �ر�
	void close(bool is_del = true);

	// �Ƿ��ʼ�����
	bool is_init_done() const { return m_is_init_done; }

	// �Ƿ�ر�
	bool is_closed() const { return m_is_closed; }

public:
	struct field_data
	{
		field_data(const std::string& name, DB_TYPE type, bool mapping)
			: field_name(name)
			, field_type(type)
			, need_mapping(mapping)
		{
		}

		std::string field_name;
		DB_TYPE field_type;
		bool need_mapping;
	};
	typedef std::vector<field_data> field_list;
	typedef std::map<std::string, DB_TYPE> field_type_map;

	struct table_data_t
	{
		bool m_is_loaded = false;	// �Ƿ���Ҫ�������
		uint32_t m_row_count = 0;	// ���ص�����
		field_list m_field_list;	// �ֶ��б�
		field_type_map m_filed_map; // �ֶ�����ӳ��
		std::string m_where;
	};

	typedef std::map<std::string, table_data_t> table_list;

	typedef std::vector<redis_read_unit_ptr> server_read_unit_list;
	typedef std::vector<redis_save_unit_ptr> server_save_unit_list;

	// ��ȡ���ṹ
	const table_data_t& get_table_field_cols(const std::string& table);
	const field_type_map& get_table_field_map(const std::string& table);
	const table_list& get_table_list() const { return m_table_list; }

protected:
	// �������ṹ
	virtual bool init_table_field(struct ConnectionPool_S* zdb_pool, const sql_table *db_table);

	// ��ʼ����ȡ�߳��б�
	virtual bool init_read_unit_list(const char* host, int32_t port, const char* passwd, int32_t db, struct ConnectionPool_S* zdb_pool, uint32_t num);

	// ��ʼ�������߳��б�
	virtual bool init_save_unit_list(const char* host, int32_t port, const char* passwd, int32_t db, struct ConnectionPool_S* zdb_pool, uint32_t save_interval, uint32_t save_count_limit, uint32_t num);

public:
	// ��ȡ��������
	void unit_read_finish(const redis_read_unit_ptr& p_read_unit_ptr);

protected:
	// ���redis״̬
	uint32_t get_redis_state();
	// ��mysql��������
	bool start_read_from_db();
	// ���˶�ȡ����������Ķ������ݶ�ȡ
	virtual bool start_read_extra_data() { return true; }
	// �������Ƿ����
	void check_read_db_done();
	// ���һ���߳��Ƿ����
	void do_read_finish(const redis_read_unit_wptr& p_wunit);
	// ��ȡ�����Ļص�
	virtual void read_db_done();

protected:
	// ���ص�ǰ���ݿ�� key ������
	uint32_t get_db_size();
	// ɾ����ǰdb(����ʹ��)
	void do_flush_db();

protected:
	boost::asio::io_service m_io_service;
	boost::asio::io_service::strand m_strand;
	boost::scoped_ptr<boost::asio::io_service::work> m_work;
	boost::thread_group m_thread_group;

	table_list m_table_list;				// ���ṹ�б�

	server_read_unit_list m_read_unit_list;		// ��ȡ�������߳�
	server_save_unit_list m_save_unit_list;		// ����������߳�

	bool m_is_init_done = false;
	bool m_is_closed = true;
	static table_data_t const_error_list;
};

//////////////////////////////////////////////////////////////////////////
class redis_read_unit_t : public redis_unit_t, public boost::enable_shared_from_this<redis_read_unit_t>
{
public:
	explicit redis_read_unit_t(boost::asio::io_service& ios, redis_srv_t& server);
	virtual ~redis_read_unit_t();

public:
	redis_read_unit_ptr get_read_unit_ptr() { return shared_from_this(); }

	virtual redis_db_read_ptr get_db_read_ptr() { return redis_db_read_ptr(); }

public:
	// ���������ȡsql
	void post_batch_read(const std::string& table, uint32_t cursor, uint32_t row_num);
	void batch_read_sql(const std::string& table, uint32_t cursor, uint32_t row_num);
	bool do_batch_read(const std::string& table, uint32_t cursor, uint32_t row_num);

	bool is_batch_read_done() { return m_batch_read_task == m_done_batch_task; }

protected:
	void add_batch_read_task();
	void add_done_batch_task();

protected:
	uint32_t m_batch_read_task = 0;
	uint32_t m_done_batch_task = 0;

protected:
	redis_srv_t& m_redis_server;
};

//////////////////////////////////////////////////////////////////////////
class redis_save_unit_t : public redis_unit_t, public boost::enable_shared_from_this<redis_save_unit_t>
{
public:
	explicit redis_save_unit_t(boost::asio::io_service& ios, redis_srv_t& server);
	virtual ~redis_save_unit_t();

	// ��ʼ��
	virtual bool init(const char* host, int32_t port, const char* passwd, int32_t db, struct ConnectionPool_S* zdb_pool, uint32_t save_interval, uint32_t save_count_limit);

public:
	// ���浽sql��ʱ��
	bool start_save_to_sql_timer();
	void stop_save_to_sql_timer();
	void save_to_sql_timer_callback(const boost::system::error_code& ec);

	// ��������δ������ֶ�
	void do_save_all();

protected:
	enum save_result_type
	{
		save_result_success = 0,	// ����ɹ�
		save_result_empty = 1,		// ���滺���б�����
		save_result_error = 2,		// ���淢�����ش���
	};

	save_result_type do_save_one(const Connection_T& conn);

protected:
	uint32_t m_save_interval = 1;			// ��ʱ�����
	uint32_t m_save_count_limit = 500;		// ���α�����������
	boost::asio::deadline_timer m_save_to_sql_timer;	// ��ʱ��
	bool m_save_cancel = true;

protected:
	redis_srv_t& m_redis_server;
};