#include <vector>
#include "TinyCR/TinyCRClient.h"
#include <thread>
#include <stdlib.h>
#include "Socket/ClientSocket.h"
#include "Socket/SocketException.h"
#include <regex>
#define COMMAND_PORT 60000

TinyCRClient<uint64_t, uint32_t>client("localhost");

typedef std::string (*CommandFunction)(std::string data);
std::unordered_map<std::string, CommandFunction> commandsMap;
bool running = true;


/*
 * Function for running the client in a thread
 */
void runClientThread()
{
    client.startClient();
}

std::string showCommand(std::string data)
{
    std::regex rgx("show ([0-9]+)");

    std::smatch matches;
    if(!std::regex_search(data, matches, rgx)) 
    {
        return "Inputs are badly formed\n";	
    }

    uint32_t num = stoul(matches[1]);

    bool v = client.queryCertificate(num);
    std::string response = matches[1];
    if(v)
    {
        response += " is revoked\n";
    }
    else
    {
        response +=  " is unrevoked\n";
    }
    return response;
}

std::string getCommand(std::string data)
{
    std::regex rgx("get (\\w+)");

    std::smatch matches;
    if(!std::regex_search(data, matches, rgx)) 
    {
        return "Inputs are badly formed\n";	
    }
    return std::to_string(client.getLatencyStatistic(matches[1])) + "\n";
}

std::string pingCommand(std::string data)
{
    return "pong";
}

std::string exitCommand(std::string data)
{
    running = false;
    return "exited";
}

static void listenForCommands()
{
    ServerSocket server(COMMAND_PORT);
    std::cout << "Listening For Commands on Port: " << COMMAND_PORT << std::endl;
    //implement something to take commands and stuff
    std::regex commandRgx("(^\\w+)");

    while(running)
    {
        try
        {
            ServerSocket new_sock;
            server.accept(new_sock);
            std::string data;
            new_sock >> data;

            std::smatch matches;
            if(!std::regex_search(data, matches, commandRgx)) 
            {
                new_sock << "Command not recognized";
                continue;
            }

            std::string command = matches[1];

            if(commandsMap.find(command) != commandsMap.end())
            {
                std::string response =commandsMap[command](data);
                new_sock << response;
            }   
            else
            {
                new_sock << "Bad input";
            }
    
        }
        catch (SocketException &e)
        {
            std::cout << "Exception was caught:" << e.description() << std::endl;
        }  
    }
}

/*
 * Runs the client, no command line inputs
 */
int main(int argc, char *argv[])
{
    if (argc == 2)
    {
        client.setServerIP(argv[1]);
    }

    commandsMap["show"] = &showCommand;
    commandsMap["get"] = &getCommand;
    commandsMap["ping"] = &pingCommand;
    commandsMap["exi"] = &exitCommand;
    std::thread clientThread (runClientThread);
    listenForCommands();

    //clientThread.join();
}
