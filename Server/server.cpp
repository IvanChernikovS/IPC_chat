#include <iostream>
#include <string>
#include <map>
#include <queue>
#include <algorithm>
#include <thread>
#include <mutex>
#include <chrono>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

static const std::string SERVER_FIFO_NAME = "/tmp/server_fifo_name";
static const std::string CLIENT_FIFO_NAME = "/tmp/client_fifo_";
static std::mutex g_mutex;

struct ClientInfo
{
    int m_clientFifoFd;
    std::string m_clientName;
};

struct ClientPacket
{
    int m_size;
    int m_clientFifoFd;
    pid_t m_clientPid;
    char m_data[256];
};

struct Message
{
    std::string m_message;
    pid_t m_pid;
};

struct ServerPacket
{
    int m_size;
    char m_payload[256];
};

std::map<pid_t, ClientInfo> clientList;
std::queue<Message> messageQueue;

bool Broadcast(const Message& message)
{
    for(const auto& client: clientList)
    {
        if(client.first != message.m_pid)
        {
            ServerPacket packet;

            packet.m_size = message.m_message.size();
            sprintf(packet.m_payload, message.m_message.c_str(), std::min(packet.m_size, 256));

            if(write(client.second.m_clientFifoFd, &packet, sizeof(packet)) < 0)
            {
                perror("Failed to write");
                return false;
            }
        }
    }

    return true;
}

void PollMessageQueue()
{
    while(true)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        if(!messageQueue.empty())
        {
            Message message = messageQueue.front();
            messageQueue.pop();

            if(!Broadcast(message))
            {
                exit(EXIT_FAILURE);
            }
        }
    }
}

bool HasClient(pid_t clientPid)
{
    if(auto it = clientList.find(clientPid); it != clientList.end())
    {
        return true;
    }
    return false;
}

Message PrepareMessage(const std::string& name, const std::string& message, pid_t id)
{
    Message msg;

    msg.m_pid = id;
    msg.m_message = name + " : " + message + ";";

    return msg;
}

void Register(const ClientPacket& data)
{
    std::lock_guard<std::mutex> lock(g_mutex);

    ClientInfo clientInfo;

    clientInfo.m_clientName = data.m_data;
    clientInfo.m_clientFifoFd = data.m_clientFifoFd;

    std::string clientFifo = CLIENT_FIFO_NAME + std::to_string(data.m_clientPid);
    clientInfo.m_clientFifoFd = open(clientFifo.c_str(), O_WRONLY);

    clientList.emplace(data.m_clientPid, clientInfo);

    std::cout << data.m_data << " was registered." << std::endl;
}

void PushToQueue(const Message& message)
{
    std::lock_guard<std::mutex> lock(g_mutex);

    messageQueue.push(message);
}

int main()
{
    ClientPacket clientData;
    Message message;
    int serverFifoFd;

    unlink(SERVER_FIFO_NAME.c_str());

    std::cout << "Creating client->server pipe on " << SERVER_FIFO_NAME << std::endl;

    if(mkfifo(SERVER_FIFO_NAME.c_str(), 0777) == -1)
    {
        perror("Unable to create a server pipe!");
        exit(EXIT_FAILURE);
    }

    std::cout << "Open client->server pipe." << std::endl;

    if(serverFifoFd = open(SERVER_FIFO_NAME.c_str(), O_RDONLY | O_NONBLOCK); serverFifoFd == -1)
    {
        perror("Server pipe not open");
        exit(EXIT_FAILURE);
    }

    std::cout << "Client->server is open. File descriptor: " << std::to_string(serverFifoFd) << std::endl;

    std::thread broadcastThread(PollMessageQueue);

    while(true)
    {
        if(int len = read(serverFifoFd, &clientData, sizeof(clientData)); len > 0)
        {
            clientData.m_data[clientData.m_size] = '\0';

            if(HasClient(clientData.m_clientPid))
            {
                message = PrepareMessage(clientList[clientData.m_clientPid].m_clientName,
                        std::string(clientData.m_data),
                        clientData.m_clientPid);
                PushToQueue(message);
            }
            else
            {
                Register(clientData);
            }
        }
        else if(len < 0 && errno != EAGAIN)
        {
            perror("Message can't be read");
            break;
        }
    }

    broadcastThread.join();

    close(serverFifoFd);
    unlink(SERVER_FIFO_NAME.c_str());
    exit(EXIT_SUCCESS);
}