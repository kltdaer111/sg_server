#include "chat_connector.hpp"
#include "main/gate_server_fwd.hpp"
#include "main/gate_server.hpp"

USING_NS_NETWORK;
USING_NS_COMMON;

void chat_connector_t::on_connect(const tcp_socket_ptr& s)
{
    env::server->get_ios().post(boost::bind(&gate_server_t::on_add_chat, env::server, s));
}

void chat_connector_t::on_new_msg(const tcp_socket_ptr& s, const message_t& msg)
{
	if (msg.has_uid())
	{
		env::server->get_ios().post(boost::bind(&gate_server_t::on_chat_msg, env::server, msg.head.cmd, msg.body, msg.get_uid()));
	} 
	else
	{
		env::server->get_ios().post(boost::bind(&gate_server_t::on_chat_msg, env::server, msg.head.cmd, msg.body, 0));
	}
    
}

void chat_connector_t::on_close(const tcp_socket_ptr& s)
{
    env::server->get_ios().post(boost::bind(&gate_server_t::on_del_chat, env::server, s));
}

bool chat_connector_t::send_msg_to_chat(const msg_buf_ptr& msg)
{
    env::server->post_network_send_msg(get_socket(), msg);
    return true;
}