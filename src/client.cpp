#include <vector>
#include "../include/TinyCRClient.h"
#include <thread>
#include <stdlib.h>
#include "../include/ClientSocket.h"
#include "../include/SocketException.h"
#include <regex>
#define COMMAND_PORT 60000

TinyCRClient<uint64_t, uint32_t>client("localhost");

void runClientThread()
{
	client.startClient();
}

int main(int argc, char **argv)
{

	std::thread clientThread (runClientThread);

	ServerSocket server(COMMAND_PORT);
	std::cout << "Listening For Commands on Port: " << COMMAND_PORT << "\n";
	//implement something to take commands and stuff
	std::regex rgx("show ([0-9]+)");

	while(true)
	{
		try
        {
			ServerSocket new_sock;
			server.accept(new_sock);
			std::string data;
            new_sock >> data;

			std::smatch matches;
			if(!std::regex_search(data, matches, rgx)) 
			{
				new_sock << "Command not recognized";
				continue;
			
			}

			std::string str_num = matches[1];
			uint32_t num = stoul(str_num);

			bool v = client.queryCertificate(num);
			if(v)
			{
				new_sock << str_num << " is valid"<< "\n";
			}
			else
			{
				new_sock << str_num << " is not valid"<< "\n";
			}
		}
		catch (SocketException &e)
		{
			std::cout << "Exception was caught:" << e.description() << "\n";
		}  
	}

	clientThread.join();
}

