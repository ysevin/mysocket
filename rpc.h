#pragma once

#include <string>
#include <map>

class RPCInterface;

typedef void (RPCInterface::*RPC_FUNC)(void* _param);

#define RPC_FUNC_MAP(name)	\
	map_func_.insert(std::make_pair<std::string, RPC_FUNC>(#name, &RPCInterface::name));	\

#define RPC_FUNC_REGISTER(name)	\
	virtual void name(void* _param) = 0;	\

#define RPC_FUNC_REGISTER_SERVER(name)	\
	void name(void* _param){}	\

#define RPC_FUNC_REGISTER_CLIENT(name)	\
	void name(void* _param)	\
	{	\
		client_send(#name, _param);	\
	}	\

class RPCInterface
{
public:
	RPCInterface()
	{
		RPC_FUNC_MAP(rpc_func)
		RPC_FUNC_MAP(rpc_func_2)
	}

	RPC_FUNC_REGISTER(rpc_func)
	RPC_FUNC_REGISTER(rpc_func_2)

protected:
	static void rpc_func_call(RPCInterface* _invoker, const std::string& func_name, void* _param = NULL)
	{
		if(map_func_.find(func_name) != map_func_.end())
		{
			RPC_FUNC func = map_func_[func_name];
			(_invoker->*func)(_param);
		}
	}
private:
	typedef std::map<std::string, RPC_FUNC> rpc_func_map_type;
	static rpc_func_map_type map_func_;
};

class RPCServer : public RPCInterface
{
public:
	RPC_FUNC_REGISTER_SERVER(rpc_func)
	RPC_FUNC_REGISTER_SERVER(rpc_func_2)
};

class RPCClient : public RPCInterface
{
public:
	virtual void client_send(const char* _func_name, void* _param) {}

	RPC_FUNC_REGISTER_CLIENT(rpc_func)
	RPC_FUNC_REGISTER_CLIENT(rpc_func_2)
};

//modify abc
//
//wahaha
