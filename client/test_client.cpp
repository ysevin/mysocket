#include <stdio.h>
#include <sstream>
#include <windows.h>
#include "../my_socket.h"

void on_recv(const char* _content)
{
	printf("recv: %s\n", _content);
}

int main(int argc, char* argv[])
{
	ClientSocket client;
	bool succ; 
	if(argc > 1)
		succ = client.create(argv[1], 27016, NULL);
	else
		succ = client.create("127.0.0.1", 27016, NULL);
	if(!succ)
		printf("connent failed");

	int i = 0;
	while(true)
	{
		std::stringstream ss;
		ss << client.get_id() << "|" << i++ ;
		client.send(ss.str().c_str());
		Sleep(100);
	}
}
