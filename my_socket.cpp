#include "stdafx.h"

#include <stdio.h>
#include <stdlib.h>
#include <sstream>

#include "my_socket.h"

namespace my_util
{
	//std::string版本
	StrVec string_split(const std::string& _src, const std::string& _pattern)
	{
		std::string::size_type pos;
		std::string src(_src);
		src += _pattern;		//扩展字符串以方便操作
		int size = src.size();
		StrVec result;
	 
		for(int i = 0; i < size; i++)
		{
			pos = src.find(_pattern, i);
			if(pos < size)
			{
				std::string s = src.substr(i, pos-i);
				result.push_back(s);
				i = pos + _pattern.size() - 1;
			 }
		 }
		 return result;
	}
}

RPCInterface::rpc_func_map_type RPCInterface::map_func_;

MySocket::~MySocket()
{}

bool MySocket::_init()
{
#ifdef _WIN32
	WSADATA wsa_data;
	if(NO_ERROR != WSAStartup(MAKEWORD(2, 2), &wsa_data))
		return false;
#endif

	socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if(-1 == socket_)
	{
#ifdef _WIN32
		WSACleanup();
#endif
		return false;
	}

	return true;
}
void MySocket::_close()
{
#ifdef _WIN32
	closesocket(socket_);
	WSACleanup();
#else
	::close(socket_);
#endif
}

ServerSocket::ServerSocket()
{
#ifdef _WIN32
	mutex_ = CreateMutexA(NULL, FALSE, "socket_mutex");
#else
	pthread_mutex_init(&mutex_, NULL);
#endif
    recv_func_ = NULL;
}

ServerSocket::~ServerSocket()
{
	std::map<int, ClientInfo>::iterator it;
	for(it = map_client_.begin(); it != map_client_.end(); it++)
	{
#ifdef _WIN32
		closesocket(it->second.socket_);
#else
		::close(it->second.socket_);
#endif
	}
	map_client_.clear();

	_close();
}

bool ServerSocket::_bind()
{
	sockaddr_in service_add;
	service_add.sin_family = AF_INET;
#ifdef _WIN32
	service_add.sin_addr.s_addr = inet_addr("127.0.0.1");
#else
	service_add.sin_addr.s_addr = 0;
#endif
	service_add.sin_port = htons(27016);
	if(::bind(socket_, (sockaddr*)&service_add, sizeof(service_add)) == -1)
	{
		_close();
		return false;
	}
	return true;
}

bool ServerSocket::_listen()
{
	if(::listen(socket_, 1) == -1)
	{
		_close();
		return false;
	}
	return true;
}

SOCKET_T ServerSocket::_client_accept(bool block)
{
	sockaddr_in addr_recv;
#ifdef _WIN32
	int len_addr = sizeof(addr_recv);
#else
	socklen_t len_addr = sizeof(addr_recv);
#endif
	SOCKET_T socket = ::accept(socket_, (sockaddr*)&addr_recv, &len_addr);
	if(-1 == socket)
		return -1;

	printf("id:%d, %s connect... ", socket, inet_ntoa(addr_recv.sin_addr));

	if(!block)
	{
#ifdef _WIN32
		u_long model = 1;
		ioctlsocket(socket, FIONBIO, &model);
#else
		fcntl(socket, F_SETFL, O_NONBLOCK);
#endif
	}

	return socket;
}

bool ServerSocket::_client_add(SOCKET_T _socket, bool thread)
{
	ClientInfo ci;
	ci.id_ = int(_socket);
	ci.socket_ = _socket;
	map_client_.insert(std::make_pair<int, ClientInfo>(ci.id_, ci));

	printf("client sum: %d\n", map_client_.size());

	if(thread)
	{
		thread_client_id_ = int(_socket);
#ifdef _WIN32
		HANDLE thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)&ServerSocket::thread_recv, (void*)this, 0, NULL);
		CloseHandle(thread);
#else
		pthread_t thread;
		pthread_create(&thread, NULL, thread_recv, (void*)this);
		pthread_join(thread, NULL);
#endif
	}

	return true;
}
void ServerSocket::set_recv_func(RECV_FUNC _recv_func)
{
    recv_func_ = _recv_func;
}

bool ServerSocket::client_send(const int _id, const char* _content)
{
	if(map_client_.find(_id) != map_client_.end())
	{
		SOCKET_T& socket = map_client_[_id].socket_;
		::send(socket, _content, strlen(_content), 0);
	}
	return true;
}

void ServerSocket::_client_close(const int _id)
{
	if(map_client_.find(_id) != map_client_.end())
#ifdef _WIN32
		::closesocket(map_client_[_id].socket_);
#else
		::close(map_client_[_id].socket_);
#endif
}

bool ServerSocket::_client_recv(const int _id)
{
#ifdef _WIN32
	WaitForSingleObject(mutex_, INFINITE);
#else
	pthread_mutex_trylock(&mutex_);
#endif

	std::map<int, ClientInfo>::iterator it = map_client_.find(_id);
	if(it == map_client_.end())
		return false;

	const int BUFF_SIZE = 1024;
	char buff[BUFF_SIZE] = {0};

	int ret_val = ::recv(it->second.socket_, buff, BUFF_SIZE, 0);
    it->second.content_ = buff;
	if(-1 == ret_val)
	{
		printf("client disconnect...\n");
		_client_close(it->first);
	}

    if(recv_func_ != NULL)
        (*recv_func_)(_id, buff);

	/*
	std::vector<std::string> vec_content = my_util::string_split(buff);
	printf("recv: %d %s %d\n", _id, buff, vec_content.size());
	if(!vec_content.empty())
	{
		int type = atoi(vec_content[0].c_str());
		switch(type)
		{
			case 0:
				printf("%d: %s, %d\n", _id, buff, strlen(buff));
				break;
			case 1:
				{
					if(vec_content.size() > 2)
					{
						int talk_id = atoi(vec_content[1].c_str());
						std::stringstream ss;
						ss << talk_id << " talk to you: " << vec_content[2];
						client_send(talk_id, ss.str().c_str());
					}
				}
				break;
			case 2:
				{
					std::string func_name = vec_content[1];
					rpc_func_call(this, func_name);
				}
				break;
		}
	}
	*/

#ifdef _WIN32
	ReleaseMutex(mutex_);
#else
	pthread_mutex_unlock(&mutex_);
#endif
	return (-1 != ret_val);
}

void* ServerSocket::thread_recv(void* _param)
{
	ServerSocket* pthis = (ServerSocket*)_param;
	int client_id = pthis->thread_client_id_;
	while(true)
	{
		if(!pthis->_client_recv(client_id))
			break;
	}
	return NULL;
}
bool ServerSocket::create(RECV_FUNC _recv_func)
{
	_init();
	_bind();
	_listen();

	set_recv_func(_recv_func);

	return true;
}

bool ServerSocket::run()
{
	while(true)
	{
		SOCKET_T socket = _client_accept(true);
		if(-1 != socket)
			_client_add(socket, true);
	}
	return true;
}
bool ServerSocket::run_nonblock()
{
	fd_set readset;

#ifdef _WIN32
	u_long block = 1;
	ioctlsocket(socket_, FIONBIO, &block);
#else
	fcntl(socket_, F_SETFL, O_NONBLOCK);
#endif
	while(true)
	{
		FD_ZERO(&readset);
		FD_SET(socket_, &readset);

		int max_fd = socket_;
		std::map<int, ClientInfo>::iterator it = map_client_.begin();
		for(; it != map_client_.end(); it++)
		{
			FD_SET(it->second.socket_, &readset);
			if(max_fd < int(it->second.socket_))
				max_fd = int(it->second.socket_);
		}

		if(::select(max_fd + 1, &readset, NULL, NULL, NULL) < 0)
			printf("select failed\n");

		if(FD_ISSET(socket_, &readset))
		{
			SOCKET_T socket = _client_accept(false);
			_client_add(socket, false);
		}

		for(it = map_client_.begin(); it != map_client_.end();)
		{
			std::map<int, ClientInfo>::iterator it_next = it;
			it_next++;
			if(FD_ISSET(it->second.socket_, &readset))
			{
				if(!_client_recv(it->first))
					map_client_.erase(it);
			}
			it = it_next;
		}
	}
	return true;
}
bool ServerSocket::run_delay_recv()
{
	SOCKET_T socket = _client_accept(true);
	if(-1 == socket)
		printf("accept failed");
	_client_add(socket, false);

	char input[512] = {0};
	while(true)
	{
		fgets(input, 512, stdin);
		if(strcmp(input, "q\n") == 0)
			break;
		else if(strcmp(input, "r\n") == 0)
			_client_recv(int(socket));
		else
			client_send(int(socket), input);
	}
	return true;
}

void RPCClientImpl::client_send(const char* _func_name, void* _param)
{
	std::string msg("2:");
	msg += _func_name;
	client_.send(msg.c_str());
}

bool ClientSocket::_connect(const char* _ip, const int _port)
{
	server_socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if(-1 == server_socket_)
	{
		_close();
		return false;
	}

	sockaddr_in addr_recv;
	addr_recv.sin_family = AF_INET;
	addr_recv.sin_addr.s_addr = inet_addr(_ip);
	addr_recv.sin_port = htons(_port);

	int ret = ::connect(server_socket_, (sockaddr*)&addr_recv, sizeof(addr_recv));
	if(ret == -1)
	{
		_close();
		return false;
	}

	printf("connent %s succ...\n", _ip);

#ifdef _WIN32
	HANDLE thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)&ClientSocket::thread_recv, (void*)this, 0, NULL);
	if(thread == NULL)
		return false;
	CloseHandle(thread);
#else
	pthread_t thread;
	pthread_create(&thread, NULL, thread_recv, (void*)this);
	pthread_join(thread, NULL);
#endif

	return true;
}

ClientSocket::~ClientSocket()
{
	_close();
}

bool ClientSocket::send(const char* _content)
{
	::send(server_socket_, _content, strlen(_content), 0);

	return true;
}

void* ClientSocket::thread_recv(void* _param)
{
	ClientSocket* cs = (ClientSocket*)_param;
	const int BUFF_SIZE = 1024;
	char buff[BUFF_SIZE];
	while(true)
	{
		memset(buff, 0, BUFF_SIZE);
		int ret_val = ::recv(cs->server_socket_, buff, BUFF_SIZE, 0);
		printf("recv: %s %d %d\n", buff, strlen(buff), ret_val);

		if(cs->recv_func_ != NULL)
        	(*(cs->recv_func_))(buff);

		if(-1 == ret_val)
		{
			printf("server disconnect...\n");
			return 0;
		}
	}
	//return 1;
}

bool ClientSocket::create(const char* _ip, const int _port, RECV_FUNC _recv_func)
{
	_init();
	recv_func_ = _recv_func;
	return _connect(_ip, _port);
}

bool ClientSocket::run()
{
	char str[256];
	while(true)
	{
		memset(str, 0, 256);
		scanf("%s", str);
		if(strcmp(str, "q") == 0)
			break;
		if(strcmp(str, "rpc") == 0)
			server_rpc_call.rpc_func(NULL);
		else if(strcmp(str, "rpc2") == 0)
			server_rpc_call.rpc_func_2(NULL);
		else
			send(str);
	}
	return true;
}
