#ifndef UDPClient_h__
#define UDPClient_h__
#include <map>
#include <algorithm>

#include <boost/asio.hpp>
#include "Network/NetworkClient.h"

class UDPClient : public NetworkClient
{
public:
    UDPClient();
    ~UDPClient();

    bool Connect(std::string playerName, std::string address, int port);
    void Disconnect();
    void Receive(Packet& packet);
    void ReceivePackets();
    void Send(Packet& packet);
    void Broadcast(Packet& packet, int port);
    bool IsSocketAvailable();
    // Returns false if no packets are available
    bool GetNextPacket(Packet& packet);
private:
    typedef std::map<unsigned int, std::vector<std::pair<int, boost::shared_ptr<char>>>> PacketMap;
    // Assio UDP logic
    boost::asio::io_service m_IOService;
    boost::asio::ip::udp::endpoint m_ReceiverEndpoint;
    boost::shared_ptr<boost::asio::ip::udp::socket> m_Socket;
    int m_LastReceivedSnapshotGroup = 0;
    int readBuffer();
    void readPartOfPacket();
    PacketID m_SendPacketID = 0;
    //map:(packetGroup, vector:(pair:(groupIndex, packetData)))
    PacketMap m_PacketSegmentMap;
    bool hasReceivedPacket(int packetGroup, int groupIndex);
    // 2^19
    const int m_SizeOfSocketBuffer = 524288;
};

#endif