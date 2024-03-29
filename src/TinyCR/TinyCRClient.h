/**
 * Holds TinyCRServer Class
 * @author Xiaofeng Shi, Jonne Kaunisto 
 */

#ifndef TinyCRClient_class
#define TinyCRClient_class
#include "../Socket/ClientSocket.h"
#include "../Socket/ServerSocket.h"
#include "../Socket/SocketException.h"
#include "../platform/CRIoT.h"
#include "../utils/perf_tool.h"
#include <thread>
#include <mutex>

#define DEVICE_PORT 40000

#define SUMMARY_MAX_SIZE 1024

template<typename K, class V>
class TinyCRClient
{
public:
    /**
     * Constructor for TinyCRClient
     * @param serverIP The hostname of the server
     */
    TinyCRClient(std::string serverIP)
    {
        this->serverIP = serverIP;

        statistics.addStatistic("full_updating_latency");
        statistics.addStatistic("delta_updating_latency");
    }

    /**
     * Start client, get request from server and listen to updates.
     */
    bool startClient()
    {
        requestInitialSummary();
        std::thread updatesThread (listenForUpdates, this);
        updatesThread.join();
        return true;
    }

    /**
     * Sets the server ip that the client should connect to
     * @param serverIP The ip address of the server in string form
     */
    void setServerIP(std::string serverIP)
    {
        this->serverIP = serverIP;
    }

    /**
     * Query a certificate, thread safe
     * @param key Key, which should be queried
     * @returns bool indicating if the key is revoked or unrevoked
     */
    bool queryCertificate(const K key)
    {
        queryLock.lock();
        bool result = dassVerifier.query(key) == 1;
        queryLock.unlock();
        return result;
    }

    /**
     * Queries multiple keys
     * @param keys Keys to be queried
     * @returns The values of the keys
     */
    vector<V> queryCertificates(K keys)
    {
        queryLock.lock();
        vector<V> results = dassVerifier.batch_query(keys);
        queryLock.lock();
        return results;
    }

    /**
     * Get statistic with the name of the statistic
     * @param statistic The name of the statistic
     * @result The value of the statistic
     */
    double getLatencyStatistic(std::string statistic)
    {
        return statistics.getAverageLatency(statistic);
    }

private:
    CRIoT_Data_VO<K, V> dassVerifier;

    std::string serverIP;
    std::thread summaryUpdatesThread;
    std::mutex queryLock;

    LatencyStatistics statistics;


    /**
     * Requests initial summary from the server
     */
    void requestInitialSummary()
    {
        for(int i =0;;i++)
        {
            vector<uint8_t> msg;
            try
            {
                /*initialize connnection to server*/
                ClientSocket client_socket (serverIP, 30000);
                readFullSummary(client_socket);
                break;
            }
            catch ( SocketException& e )
            {
                std::cout << "Exception was caught while requesting initial summary:" << e.description() << std::endl;
                std::cout << "Retrying #" << i << std::endl;
                sleep(5);
            }
        }
    }

    /**
     * Reads the full summary from the server.
     * @param socket The socket connected to the server
     */
    void readFullSummary(Socket socket)
    {
        vector<uint8_t> msg;
        while(true)
        {
            char* data = new char [MAXRECV + 1];
            int n_bytes = socket.recv(data);
            for(int i=0; i<n_bytes; i++)
            {
                uint8_t byte;
                memcpy(&byte, &data[i], 1);

                msg.push_back(byte);
            }
            if(data[n_bytes-1]=='h' && data[n_bytes-2]=='s' && data[n_bytes-3]=='i' && data[n_bytes-4]=='n' 
                && data[n_bytes-5]=='i' && data[n_bytes-6]=='f')
                break;
            delete[] data;
        }
        StopWatch stopWatch = StopWatch();
        dassVerifier.decode_full(msg);
        statistics.addLatency("full_updating_latency", stopWatch.stop());
        socket.send("FullDone");
        std::cout << "sent ack" << std::endl;
    }
    
    /**
     * Listens for delta updates and full updates from the server continuously
     */
    static void listenForUpdates(TinyCRClient *tinyCRClient)
    {
        std::cout << "Listening For Summary Updates at port: " << DEVICE_PORT << std::endl;
        try
        {
            // Create the socket
            ServerSocket server(DEVICE_PORT);

            while (true)
            {
                std::cout << "waiting for new connection" << std::endl;
                ServerSocket new_sock;
                server.accept(new_sock);
                tinyCRClient->queryLock.lock();

                vector<uint8_t> msg;
                while(true)
                {
                    char* data = new char [MAXRECV + 1];
                    int n_bytes = new_sock.recv(data);
                    for(int i=0; i<n_bytes; i++)
                    {
                        uint8_t byte;
                        memcpy(&byte, &data[i], 1);

                        msg.push_back(byte);
                    }
                    if(data[n_bytes-1]=='h' && data[n_bytes-2]=='s' && data[n_bytes-3]=='i' && data[n_bytes-4]=='n' 
                        && data[n_bytes-5]=='i' && data[n_bytes-6]=='f')
                        break;
                    delete[] data;
                }

                //Full Update 70 = F
                if(msg[0] == 70)
                {
                    std::cout << "Doing a full update" << std::endl;
                    StopWatch stopWatchFull = StopWatch();
                    tinyCRClient->dassVerifier.decode_full(msg);
                    tinyCRClient->statistics.addLatency("full_updating_latency", stopWatchFull.stop());
                    new_sock << "FullDone";
                }
                else
                {
                    std::cout << "Doing a summary Update: " << sizeof(msg) << std::endl;
                    StopWatch stopWatchDelta = StopWatch();
                    tinyCRClient->dassVerifier.decode_batch_summary(msg);
                    tinyCRClient->statistics.addLatency("delta_updating_latency", stopWatchDelta.stop());
                    new_sock << "SummaryDone";
                }
                tinyCRClient->queryLock.unlock();
            }
        }
        catch (SocketException &e)
        {
            std::cout << "Exception was caught while listening for updates:" << e.description() << std::endl;
            std::cout << "Exiting." << std::endl;
        }
    }
};
#endif
