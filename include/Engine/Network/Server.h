#ifndef Server_h__
#define Server_h__

#include <string>
#include <ctime>

#include <glm/common.hpp>
#include <boost/asio/ip/udp.hpp>

#include "Network/MessageType.h"
#include "Network/PlayerDefinition.h"
#include "Core/World.h"
#include "Core/EventBroker.h"
#include "Network/Network.h"
#include "Input/EInputCommand.h"
#include "Core/EPlayerDamage.h"

class Server : public Network
{
public:
    Server();
    ~Server();
    void Start(World* m_world, EventBroker *eventBroker) override;
    void Update() override;
private:
    // UDP logic
    boost::asio::ip::udp::endpoint m_ReceiverEndpoint;
    boost::asio::io_service m_IOService;
    boost::asio::ip::udp::socket m_Socket;

    // Sending messages to client logic
    PlayerDefinition m_PlayerDefinitions[MAXCONNECTIONS];
    std::vector<PlayerDefinition> m_ConnectedUsers;
    char readBuffer[INPUTSIZE] = { 0 };
    int bytesRead = 0;
    // time for previouse message
    std::clock_t previousePingMessage = std::clock();
    std::clock_t previousSnapshotMessage = std::clock();
    std::clock_t timOutTimer = std::clock();
    // How often we send messages (milliseconds)
    int intervalMs = 1000;
    int snapshotInterval = 50;
    int checkTimeOutInterval = 100;

    //Timers
    std::clock_t m_StartPingTime;

    // Game logic
    World* m_World;
    EventBroker* m_EventBroker;
    
    // Packet loss logic
    unsigned int m_PacketID = 0;
    unsigned int m_PreviousPacketID = 0;

    // Private member functions
    int  receive(char* data, size_t length);
    void readFromClients();
    void send(Packet& packet, int playerID);
    void send(Packet& packet);
    void broadcast(Packet& packet);
    void sendSnapshot();
    void sendPing();
    void checkForTimeOuts();
    void disconnect(int i);
    void parseMessageType(Packet& packet);
    void parseOnInputCommand(Packet& packet);
    void parseOnPlayerDamage(Packet& packet);
    void parseConnect(Packet& packet);
    void parseDisconnect();
    void parseClientPing();
    void parseServerPing();
    void identifyPacketLoss();
    EntityID createPlayer();
    int GetPlayerIDFromEndpoint(boost::asio::ip::udp::endpoint endpoint);
    // Debug event
    EventRelay<Server, Events::InputCommand> m_EInputCommand;
    bool OnInputCommand(const Events::InputCommand& e);
};

#endif

