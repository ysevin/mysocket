#include <stdio.h>
#include "../my_socket.h"

void on_recv(int _id, const char* _content)
{
	printf("%d, %s\n", _id, _content);
}

int main(int argc, char* argv[])
{
	ServerSocket server;
	server.create(on_recv);
	server.run();
}
