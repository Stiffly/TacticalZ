#include "Network/Client.h"

using namespace boost::asio::ip;


Client::Client(ConfigFile* config) : m_Socket(m_IOService)
{
    Network::initialize();

    // Asumes root node is EntityID_Invalid
    insertIntoServerClientMaps(EntityID_Invalid, EntityID_Invalid);
    // Init timer
    m_TimeSinceSentInputs = std::clock();
    // Default is local host
    std::string address = config->Get<std::string>("Networking.Address", "127.0.0.1");
    int port = config->Get<int>("Networking.Port", 27666);
    m_ReceiverEndpoint = udp::endpoint(boost::asio::ip::address::from_string(address), port);
    // Set up network stream
    m_PlayerName = config->Get<std::string>("Networking.Name", "Raptorcopter");
    m_SendInputIntervalMs = config->Get<int>("Networking.SendInputIntervalMs", 33);

}

Client::~Client()
{ }

void Client::Start(World* world, EventBroker* eventBroker)
{
    m_EventBroker = eventBroker;
    m_World = world;

    // Subscribe to events
    EVENT_SUBSCRIBE_MEMBER(m_EInputCommand, &Client::OnInputCommand);
    EVENT_SUBSCRIBE_MEMBER(m_EPlayerDamage, &Client::OnPlayerDamage);
    EVENT_SUBSCRIBE_MEMBER(m_EPlayerSpawned, &Client::OnPlayerSpawned);

    m_Socket.connect(m_ReceiverEndpoint);
    LOG_INFO("I am client. BIP BOP");
}

void Client::Update()
{
    m_EventBroker->Process<Client>();
    readFromServer();
    if (m_IsConnected) {
        hasServerTimedOut();
        // Don't sent 1 input in 1 packet, bunch em up.
        if (m_SendInputIntervalMs < (1000 * (std::clock() - m_TimeSinceSentInputs) / (double)CLOCKS_PER_SEC)) {
            sendInputCommands();
            m_TimeSinceSentInputs = std::clock();
        }
        sendLocalPlayerTransform();
    }
    Network::Update();
}

void Client::readFromServer()
{
    while (m_Socket.available()) {
        bytesRead = receive(readBuf);
        if (bytesRead > 0) {
            Packet packet(readBuf, bytesRead);
            parseMessageType(packet);
        }
    }
}

void Client::parseMessageType(Packet& packet)
{
    int messageType = packet.ReadPrimitive<int>();
    if (messageType == -1)
        return;
    // Read packet ID 
    m_PreviousPacketID = m_PacketID;    // Set previous packet id
    m_PacketID = packet.ReadPrimitive<int>(); //Read new packet id
    identifyPacketLoss();

    switch (static_cast<MessageType>(messageType)) {
    case MessageType::Connect:
        parseConnect(packet);
        break;
    case MessageType::Ping:
        parsePing();
        break;
    case MessageType::Message:
        break;
    case MessageType::Snapshot:
        parseSnapshot(packet);
        break;
    case MessageType::Disconnect:
        break;
    case MessageType::PlayerConnected:
        parsePlayerConnected(packet);
        break;
    case MessageType::Kick:
        parseKick();
        break;
    case MessageType::OnPlayerSpawned:
        parsePlayersSpawned(packet);
        break;
    case MessageType::EntityDeleted:
        parseEntityDeletion(packet);
        break;
    case MessageType::ComponentDeleted:
        parseComponentDeletion(packet);
        break;
    default:
        break;
    }
}

void Client::parseConnect(Packet& packet)
{
    // Map ServerEntityID and your PlayerID
    LOG_INFO("I be connected PogChamp");
}

void Client::parsePlayerConnected(Packet & packet)
{
    // Map ServerEntityID and other player's PlayerID
    LOG_INFO("A Player connected");
}

void Client::parsePing()
{
    // Might miss connect message so set it here instead.
    m_IsConnected = true;
    // Time since last ping was received
    m_DurationOfPingTime = 1000 * (std::clock() - m_StartPingTime) / static_cast<double>(CLOCKS_PER_SEC);
    LOG_INFO("%i: response time with ctime(ms): %f", m_PacketID, m_DurationOfPingTime);
    m_StartPingTime = std::clock();

    Packet packet(MessageType::Ping, m_SendPacketID);
    packet.WriteString("Ping recieved");
    send(packet);
}

void Client::parseKick()
{
    LOG_WARNING("You have been kicked from the server.");
    m_IsConnected = false;
}

void Client::parsePlayersSpawned(Packet& packet)
{
    Events::PlayerSpawned e;
    e.Player = EntityWrapper(m_World, m_ServerIDToClientID[packet.ReadPrimitive<EntityID>()]);
    e.Spawner = EntityWrapper(m_World, m_ServerIDToClientID[packet.ReadPrimitive<EntityID>()]);
    e.PlayerID = -1;
    m_EventBroker->Publish(e);
}

void Client::parseEntityDeletion(Packet & packet)
{
    EntityID entityToDelete = packet.ReadPrimitive<EntityID>();
    EntityID localEntity = m_ServerIDToClientID.at(entityToDelete);
    if (m_World->ValidEntity(localEntity)) {
        m_World->DeleteEntity(localEntity);
        deleteFromServerClientMaps(entityToDelete, localEntity);
    }
}

void Client::parseComponentDeletion(Packet & packet)
{
    EntityID entity = packet.ReadPrimitive<EntityID>();
    std::string componentType = packet.ReadString();
    if (m_World->HasComponent(entity, componentType)) {
        m_World->DeleteComponent(m_ServerIDToClientID.at(entity), componentType);
    }
}

// Fields with strings will not work right now
void Client::InterpolateFields(Packet& packet, const ComponentInfo& componentInfo, const EntityID& entityID, const std::string& componentType)
{
    int sizeOfFields = 0;
    for (auto field : componentInfo.FieldsInOrder) {
        ComponentInfo::Field_t fieldInfo = componentInfo.Fields.at(field);
        sizeOfFields += fieldInfo.Stride;
    }
    // Is the size correct?
    boost::shared_array<char> eventData(new char[componentInfo.Stride]);
    memcpy(eventData.get(), packet.ReadData(componentInfo.Stride), componentInfo.Stride);
    //Send event to interpolat system
    Events::Interpolate e;
    e.Entity = entityID;
    e.DataArray = eventData;
    m_EventBroker->Publish(e);

}

void Client::updateFields(Packet& packet, const ComponentInfo& componentInfo, const EntityID& entityID, const std::string& componentType)
{
    for (auto field : componentInfo.FieldsInOrder) {
        ComponentInfo::Field_t fieldInfo = componentInfo.Fields.at(field);
        if (fieldInfo.Type == "string") {
            std::string& value = packet.ReadString();
            m_World->GetComponent(entityID, componentType)[fieldInfo.Name] = value;
        } else {
            memcpy(m_World->GetComponent(entityID, componentType).Data + fieldInfo.Offset, packet.ReadData(fieldInfo.Stride), fieldInfo.Stride);
        }
    }
}

void Client::parseSnapshot(Packet& packet)
{
    while (packet.DataReadSize() < packet.Size()) {
        EntityID serverEntityID = packet.ReadPrimitive<EntityID>();
        EntityID serverParentID = packet.ReadPrimitive<EntityID>();
        std::string serverEntityName = packet.ReadString();
        int ammountOfComponents = packet.ReadPrimitive<int>();
        for (int i = 0; i < ammountOfComponents; i++) {
            std::string componentType = packet.ReadString();
            ComponentInfo componentInfo = m_World->GetComponents(componentType)->ComponentInfo();
            if (serverClientMapsHasEntity(serverEntityID)) {
                EntityID localEntityID = m_ServerIDToClientID.at(serverEntityID);
                // Update entity
                if (m_World->HasComponent(localEntityID, componentType)) {
                    // Update component
                    if (componentType == "Transform") {
                        // Interpolate only transform components
                        InterpolateFields(packet, componentInfo, localEntityID, componentType);
                    } else {
                        // Set component values
                        updateFields(packet, componentInfo, localEntityID, componentType);
                    }
                } else {
                    // Has entity but no component
                    m_World->AttachComponent(localEntityID, componentType);
                    updateFields(packet, componentInfo, localEntityID, componentType);
                }
            } else {
                // Create Entity and component
                EntityID newLocalEntityID;
                if (serverParentID == EntityID_Invalid) {
                    newLocalEntityID = m_World->CreateEntity(EntityID_Invalid);
                } else {
                    newLocalEntityID = m_World->CreateEntity(m_ServerIDToClientID.at(serverParentID));
                }
                m_World->SetName(newLocalEntityID, serverEntityName);
                insertIntoServerClientMaps(serverEntityID, newLocalEntityID);
                m_World->AttachComponent(newLocalEntityID, componentType);
                updateFields(packet, componentInfo, newLocalEntityID, componentType);
            }
        }
        // Parent logic
        // This should be enough beacause we know that the entities arives in pre-order (there will always be a parent)
        if (serverParentID != EntityID_Invalid) {
            EntityID localEntityID = m_ServerIDToClientID.at(serverEntityID);
            m_World->SetParent(localEntityID, m_ServerIDToClientID.at(serverParentID));
        }
    }
}

int Client::receive(char* data)
{
    boost::system::error_code error;

    int bytesReceived = m_Socket.receive_from(boost
        ::asio::buffer((void*)data, INPUTSIZE),
        m_ReceiverEndpoint,
        0, error);
    // Network Debug data
    if (isReadingData) {
        m_NetworkData.TotalDataReceived += bytesReceived;
        m_NetworkData.DataReceivedThisInterval += bytesReceived;
        m_NetworkData.AmountOfMessagesReceived++;
    }
    if (error) {
        //LOG_ERROR("receive: %s", error.message().c_str());
    }
    return bytesReceived;
}

void Client::send(Packet& packet)
{
    m_Socket.send_to(boost::asio::buffer(
        packet.Data(),
        packet.Size()),
        m_ReceiverEndpoint, 0);
    // Network Debug data
    if (isReadingData) {
        m_NetworkData.TotalDataSent += packet.Size();
        m_NetworkData.DataSentThisInterval += packet.Size();
        m_NetworkData.AmountOfMessagesSent++;
    }
}

void Client::connect()
{
    Packet packet(MessageType::Connect, m_SendPacketID);
    packet.WriteString(m_PlayerName);
    m_StartPingTime = std::clock();
    send(packet);
}

void Client::disconnect()
{
    m_PreviousPacketID = 0;
    m_PacketID = 0;
    Packet packet(MessageType::Disconnect, m_SendPacketID);
    send(packet);
}

bool Client::OnInputCommand(const Events::InputCommand & e)
{
    if (e.Command == "ConnectToServer") { // Connect for now
        if (e.Value > 0) {
            connect();
        }
        //LOG_DEBUG("Client::OnInputCommand: Command is %s. Value is %f. PlayerID is %i.", e.Command.c_str(), e.Value, e.PlayerID);
        return true;
    } else if (e.Command == "DisconnectFromServer") {
        if (e.Value > 0) {
            disconnect();
        }
        return true;
    } else if (e.Command == "SwitchToPlayer") {
        if (e.Value > 0) {
            becomePlayer();
        }
    } else if (e.Command == "LogNetworkBandwidth") {
        if (e.Value > 0) {
            // Save to file if we no longer want to read data.
            if (isReadingData) {
                saveToFile();
            }
            isReadingData = !isReadingData;
            m_SaveDataTimer = std::clock();
        }
    } else {
        m_InputCommandBuffer.push_back(e);
        //LOG_DEBUG("Client::OnInputCommand: Command is %s. Value is %f. PlayerID is %i.", e.Command.c_str(), e.Value, e.PlayerID);
        return true;
    }
    return false;
}

bool Client::OnPlayerDamage(const Events::PlayerDamage & e)
{
    Packet packet(MessageType::OnPlayerDamage, m_SendPacketID);
    packet.WritePrimitive(e.Damage);
    packet.WritePrimitive(e.Player.ID);
    send(packet);
    return false;
}

bool Client::OnPlayerSpawned(const Events::PlayerSpawned& e)
{
    if (e.PlayerID == -1) {
        m_LocalPlayer = e.Player;
    }
    return true;
}

void Client::sendLocalPlayerTransform()
{
    if (!m_LocalPlayer.Valid()) {
        return;
    }

    ComponentWrapper cTransform = m_LocalPlayer["Transform"];
    glm::vec3& position = cTransform["Position"];
    glm::vec3& orientation = cTransform["Orientation"];
    Packet packet(MessageType::PlayerTransform, m_SendPacketID);
    packet.WritePrimitive(position.x);
    packet.WritePrimitive(position.y);
    packet.WritePrimitive(position.z);
    packet.WritePrimitive(orientation.x);
    packet.WritePrimitive(orientation.y);
    packet.WritePrimitive(orientation.z);
    send(packet);
}

void Client::identifyPacketLoss()
{
    // if no packets lost, difference should be equal to 1
    int difference = m_PacketID - m_PreviousPacketID;
    if (difference != 1) {
        LOG_INFO("%i Packet(s) were lost...", difference - 1);
    }
}

bool Client::hasServerTimedOut()
{
    // Time in ms
    float timeSincePing = 1000 * (std::clock() - m_StartPingTime) / static_cast<double>(CLOCKS_PER_SEC);
    if (timeSincePing > m_TimeoutMs) {
        // Clear everything and go to menu.
        LOG_INFO("Server has timed out, returning to menu, Beep Boop.");
        m_IsConnected = false;
        return true;
    }
    return false;
}

EntityID Client::createPlayer()
{
    EntityID entityID = m_World->CreateEntity();
    ComponentWrapper transform = m_World->AttachComponent(entityID, "Transform");
    ComponentWrapper model = m_World->AttachComponent(entityID, "Model");
    model["Resource"] = "Models/Core/UnitSphere.obj";
    ComponentWrapper player = m_World->AttachComponent(entityID, "Player");
    return entityID;
}

void Client::sendInputCommands()
{
    if (m_InputCommandBuffer.size() > 0) {
        Packet packet(MessageType::OnInputCommand, m_SendPacketID);
        for (int i = 0; i < m_InputCommandBuffer.size(); i++) {
            packet.WriteString(m_InputCommandBuffer[i].Command);
            packet.WritePrimitive(m_InputCommandBuffer[i].Value);
        }
        send(packet);
        m_InputCommandBuffer.clear();
    }
}

void Client::becomePlayer()
{
    Packet packet = Packet(MessageType::BecomePlayer, m_SendPacketID);
    send(packet);
}

bool Client::clientServerMapsHasEntity(EntityID clientEntityID)
{
    if (m_ClientIDToServerID.find(clientEntityID) != m_ClientIDToServerID.end()) {
        if (m_World->ValidEntity(clientEntityID)) {
            return true;
        }
        EntityID serverEntityID = m_ClientIDToServerID.at(clientEntityID);
        deleteFromServerClientMaps(serverEntityID, clientEntityID);
    }
    return false;
}

bool Client::serverClientMapsHasEntity(EntityID serverEntityID)
{
    if (m_ServerIDToClientID.find(serverEntityID) != m_ServerIDToClientID.end()) {
        EntityID localEntityID = m_ServerIDToClientID.at(serverEntityID);
        if (m_World->ValidEntity(localEntityID)) {
            return true;
        }
        deleteFromServerClientMaps(serverEntityID, localEntityID);
    }
    return false;
}

void Client::insertIntoServerClientMaps(EntityID serverEntityID, EntityID clientEntityID)
{
    m_ServerIDToClientID.insert(std::make_pair(serverEntityID, clientEntityID));
    m_ClientIDToServerID.insert(std::make_pair(clientEntityID, serverEntityID));

}

void Client::deleteFromServerClientMaps(EntityID serverEntityID, EntityID clientEntityID)
{
    m_ServerIDToClientID.erase(serverEntityID);
    m_ClientIDToServerID.erase(clientEntityID);
}
