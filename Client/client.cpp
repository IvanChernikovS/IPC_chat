#include <iostream>
#include <string>
#include <thread>
#include <mutex>
#include <algorithm>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

std::string SERVER_FIFO_NAME = "/tmp/server_fifo_name";
std::string CLIENT_FIFO_NAME = "/tmp/client_fifo_";
std::mutex g_mutex;

struct ClientPacket
{
    int m_size;
    int m_clientFifoFd;
    pid_t m_clientPid;
    char m_data[256];
};

struct ServerPacket
{
    int size;
    char payload[256];
};

void Print(const std::string& message)
{
    std::lock_guard<std::mutex> lock(g_mutex);

    std::cout << message << std::endl;
}

void PollPipe(int clientFifoFd)
{
    ServerPacket packet;

    while(true)
    {
        if(int len = read(clientFifoFd, &packet, sizeof(packet)); len > 0)
        {
            packet.payload[packet.size] = '\0';
            Print(std::string(packet.payload));
        }
        else if(len < 0 && errno != EAGAIN)
        {
            perror("Message can't be read");
            exit(EXIT_FAILURE);
        }

    }
}

bool SendToServer(pid_t pid, int clientFifoFd, int serverFifoFd, const std::string& msg)
{
    ClientPacket clientPacket;

    clientPacket.m_clientPid = pid;
    clientPacket.m_clientFifoFd = clientFifoFd;
    clientPacket.m_size = std::min(static_cast<int>(msg.size()), 256);

    sprintf(clientPacket.m_data, msg.c_str(), clientPacket.m_size);

    if(write(serverFifoFd, &clientPacket, sizeof(clientPacket)) < 0)
    {
        perror("Message not sent");
        return false;
    }
    return true;
}

int main()
{
    int clientFifoFd = 0;
    int serverFifoFd = 0;
    std::string msg;

    if(serverFifoFd = open(SERVER_FIFO_NAME.c_str(), O_WRONLY); serverFifoFd == -1)
    {
        perror("Server didn't open");
        exit(EXIT_FAILURE);
    }

    pid_t pid = getpid();
    std::string clientFifo = CLIENT_FIFO_NAME + std::to_string(pid);

    unlink(clientFifo.c_str());

    if(mkfifo(clientFifo.c_str(), 0777) == -1)
    {
        std::cout << "Sorry, can't make a pipe" << clientFifo << std::endl;
        exit(EXIT_FAILURE);
    }

    if(clientFifoFd = open(clientFifo.c_str(), O_RDONLY | O_NONBLOCK); clientFifoFd == -1)
    {
        perror("Server didn't open");
        exit(EXIT_FAILURE);
    }

    std::cout << "Please, Register:" << std::endl;
    std::cin >> msg;

    SendToServer(pid, clientFifoFd, serverFifoFd, msg);

    std::thread thread(PollPipe, clientFifoFd);

    std::cout << "Please, enter your message:" << std::endl;

    while(true)
    {
        std::cin >> msg;

        if(!SendToServer(pid, clientFifoFd, serverFifoFd, msg))
        {
            perror("Message not sent");
            break;
        }
    }

    thread.join();

    close(serverFifoFd);
    unlink(clientFifo.c_str());
    exit(EXIT_SUCCESS);
}