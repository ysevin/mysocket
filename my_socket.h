#pragma once

#ifdef _WIN32
#include <winsock2.h>
#else
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <string.h>
#endif

#include <vector>
#include <string>

#include "rpc.h"

#ifdef _WIN32
#pragma comment(lib, "Ws2_32.lib")
#endif

#ifdef _WIN32
typedef SOCKET SOCKET_T;
#else
typedef	int SOCKET_T;
#endif

namespace my_util
{
    typedef std::vector<std::string> StrVec;
	//std::string版本
	StrVec string_split(const std::string& _src, const std::string& _pattern = ",");		//scanf的输入会把","当两次输入
}

class MySocket
{
public:
	MySocket():socket_(-1){}
	virtual ~MySocket();

protected:
	bool _init();
	void _close();

public:
	int	get_id() {return int(socket_);}

protected:
	SOCKET_T socket_;
};


class ServerSocket : public MySocket, public RPCServer
{
public:
	struct ClientInfo
	{
		int id_;
		SOCKET_T socket_;
		std::string content_;

		ClientInfo():socket_(-1), id_(0){}
	};
	typedef void (*RECV_FUNC)(int ,const char*);

	ServerSocket();
	virtual ~ServerSocket();
private:

	bool _bind();
	bool _listen();

	SOCKET_T _client_accept(bool block);
	bool _client_add(SOCKET_T _socket, bool thread);
	void _client_close(const int _id);
	bool _client_recv(const int _id);

public:
	bool create(RECV_FUNC _recv_func);
    void set_recv_func(RECV_FUNC _recv_func);

	bool client_send(const int _id, const char* _content);
    int client_num() {return map_client_.size();}

	bool run();
	bool run_nonblock();
	bool run_delay_recv();

	static void* thread_recv(void* _param);

public:
	void rpc_func(void* _param)
	{
		printf("server_func\n");
	}
	void rpc_func_2(void* _param)
	{
		printf("server_func_2\n");
	}

private:
#ifdef _WIN32
	HANDLE mutex_;
#else
	pthread_mutex_t mutex_;
#endif
	int	thread_client_id_;

	std::map<int, ClientInfo> map_client_;			//客户端列表
    RECV_FUNC recv_func_;

};

class ClientSocket;

class RPCClientImpl : public RPCClient
{
public:
	RPCClientImpl(ClientSocket& _client):client_(_client){}

	void client_send(const char* _func_name, void* _param);

private:
	ClientSocket& client_;
};

class ClientSocket : public MySocket
{
	friend class RPCClientImpl;

public:
	ClientSocket():server_socket_(-1), server_rpc_call(*this)
	{}
	virtual ~ClientSocket();

private:
	bool _connect(const char* _ip, const int _port);

public:
	typedef void (*RECV_FUNC)(const char*);

	bool create(const char* _ip, const int _port, RECV_FUNC _recv_func);
	bool send(const char* _content);

	bool run();

	static void* thread_recv(void* _param);

private:
	SOCKET_T server_socket_;
	RECV_FUNC recv_func_;

	RPCClientImpl server_rpc_call;
};
