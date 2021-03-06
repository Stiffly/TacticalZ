#include "Network/Client.h"

using namespace boost::asio::ip;

Client::Client(World* world, EventBroker* eventBroker)
    : Network(world, eventBroker)
{
    // Asumes root node is EntityID_Invalid
    insertIntoServerClientMaps(EntityID_Invalid, EntityID_Invalid);
    // Init timer
    m_TimeSinceSentInputs = std::clock();

    auto config = ResourceManager::Load<ConfigFile>("Config.ini");
    m_PlayerName = config->Get<std::string>("Networking.Name", "Raptorcopter");
    m_SendInputInterval = config->Get<int>("Networking.SendInputIntervalMs", 33) / 1000.0;
    LOG_INFO("Client initialized");

    m_ServerlistRequest.Connect(m_PlayerName, "192.168.1.255", 32554);
}

Client::Client(World* world, EventBroker* eventBroker, std::unique_ptr<SnapshotFilter> snapshotFilter)
    : Client(world, eventBroker)
{
    m_SnapshotFilter = std::move(snapshotFilter);
}

Client::~Client()
{ }
// Need to call connect at start
void Client::Connect(std::string address, int port)
{
    // Subscribe to events
    EVENT_SUBSCRIBE_MEMBER(m_EInputCommand, &Client::OnInputCommand);
    EVENT_SUBSCRIBE_MEMBER(m_EPlayerDamage, &Client::OnPlayerDamage);
    EVENT_SUBSCRIBE_MEMBER(m_EPlayerSpawned, &Client::OnPlayerSpawned);
    EVENT_SUBSCRIBE_MEMBER(m_EDoubleJump, &Client::OnDoubleJump);
    EVENT_SUBSCRIBE_MEMBER(m_EDashAbility, &Client::OnDashAbility);
    EVENT_SUBSCRIBE_MEMBER(m_ESearchForServers, &Client::OnSearchForServers);
    EVENT_SUBSCRIBE_MEMBER(m_EConnectRequest, &Client::OnConnectRequest);
    auto config = ResourceManager::Load<ConfigFile>("Config.ini");
    m_Address = address;
    if (address.empty()) {
        m_Address = config->Get<std::string>("Networking.Address", "127.0.0.1");
    }
    m_Port = port;
    if (port == 0) {
        m_Port = config->Get<int>("Networking.Port", 27666);
    }
}

void Client::Update(double dt)
{
    m_EventBroker->Process<Client>();
    while (m_Unreliable.IsSocketAvailable()) {
        m_Unreliable.ReceivePackets();
    }
    // Packet will get real data in GetNextPacket()
    Packet parsedPacket(MessageType::Invalid);
    while (m_Unreliable.GetNextPacket(parsedPacket)) {
        if (parsedPacket.GetMessageType() == MessageType::Connect) {
            parseUDPConnect(parsedPacket);
        } else {
            parseMessageType(parsedPacket);
        }
    }

    while (m_Reliable.IsSocketAvailable()) {
        // Packet will get real data in receive
        Packet packet(MessageType::Invalid);
        m_Reliable.Receive(packet);
        if (packet.GetMessageType() == MessageType::Connect) {
            parseTCPConnect(packet);
        } else {
            parseMessageType(packet);
        }

    }

    while (m_ServerlistRequest.IsSocketAvailable()) {
        Packet packet(MessageType::Invalid);
        m_ServerlistRequest.Receive(packet);
        if (packet.GetMessageType() == MessageType::ServerlistRequest) {
            parseServerlist(packet);
        }
    }

    if (m_SearchingForServers) {
        m_TimeSearched += dt;
        if (m_SearchingTime < m_TimeSearched) {
            m_TimeSearched = 0;
            m_SearchingForServers = false;
            //displayServerlist();
            Events::DisplayServerlist e;
            e.Serverlist = m_Serverlist;
            m_EventBroker->Publish(e);
        }
    }

    if (m_IsConnected) {
        // Don't send 1 input in 1 packet, bunch em up.
        m_TimeSinceSentInputs += dt;
        if (m_SendInputInterval <  m_TimeSinceSentInputs) {
            sendInputCommands();
            m_TimeSinceSentInputs = 0;
        }
        // HACK: Send absolute player positions for now to avoid desync until we have reliable messages
        sendLocalPlayerTransform();

        hasServerTimedOut();
    }

    if (ImGui::BeginPopupModal("Disconnected", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("You have been disconnected from server.\n\n");
        ImGui::SetCursorPosX(ImGui::GetContentRegionAvailWidth() - 120);
        if (ImGui::Button("OK", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void Client::parseMessageType(Packet& packet)
{
    // Pop packetSize, sequenceNumber and packetsInSequence.
    popNetworkSegmentOfHeader(packet);

    int messageType = packet.ReadPrimitive<int>();
    if (messageType == -1)
        return;
    // Read packet ID 
    m_PreviousPacketID = m_PacketID;    // Set previous packet id
    m_PacketID = packet.ReadPrimitive<int>(); //Read new packet id
    //identifyPacketLoss();

    switch (static_cast<MessageType>(messageType)) {
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
    case MessageType::OnPlayerDamage:
        parsePlayerDamage(packet);
        break;
    case MessageType::OnDoubleJump:
        parseDoubleJump(packet);
        break;
    case MessageType::OnDashEffect:
        parseDashEffect(packet);
        break;
    case MessageType::AmmoPickup:
        parseAmmoPickup(packet);
        break;
    case MessageType::RemoveWorld:
        parseRemoveWorld(packet);
        break;
    case MessageType::KD:
        parseKDEvent(packet);
        break;
    case MessageType::Win:
        parseWin(packet);
        break;
    default:
        break;
    }
}

void Client::parseUDPConnect(Packet& packet)
{
    // Map ServerEntityID and your PlayerID
    // TODO: If this is not received send a new connect message.
    LOG_INFO("I be connected PogChamp");
}

void Client::parseTCPConnect(Packet& packet)
{
    LOG_INFO("Received TCP connect from server");
    // Pop packetSize, group, groupIndex and groupSize.
    popNetworkSegmentOfHeader(packet);

    int messageType = packet.ReadPrimitive<int>();
    // Read packet ID 
    m_PreviousPacketID = m_PacketID;    // Set previous packet id
    m_PacketID = packet.ReadPrimitive<int>(); //Read new packet id
    // parse player id and other stuff
    m_PlayerID = packet.ReadPrimitive<int>();
    LOG_INFO("A Player connected");
    // TODO: If this is not received send a new connect message.
    Packet UnreliablePacket(MessageType::Connect, m_SendPacketID);
    // Add player id and other stuff
    packet.WritePrimitive(m_PlayerID);
    m_Unreliable.Send(packet);

    //  LOG_INFO("Sent UDP Connect Server");
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
    m_Reliable.Send(packet);
}


void Client::parseServerlist(Packet& packet)
{
    // Pop packetSize, group, groupIndex and groupSize.
    popNetworkSegmentOfHeader(packet);
    packet.ReadPrimitive<int>();
    packet.ReadPrimitive<int>();

    std::string address = packet.ReadString();
    int port = packet.ReadPrimitive<int>();
    std::string serverName = packet.ReadString();
    int playersConnected = packet.ReadPrimitive<int>();
    //TODO: This should not happen when a client is connected to a server

    m_Serverlist.push_back({ address, port, serverName, playersConnected });
}

void Client::parseKick()
{
    LOG_WARNING("You have been kicked from the server.");
    disconnect();
}

void Client::parseSpawnEvents()
{
    std::vector<Events::PlayerSpawned> tempSpawn;
    for (int i = 0; i < m_PlayerSpawnEvents.size(); i++) {
        Events::PlayerSpawned e;
        if (!serverClientMapsHasEntity(m_PlayerSpawnEvents.at(i).Player.ID)) {
            tempSpawn.push_back(m_PlayerSpawnEvents.at(i));
            continue;
        }
        e.Player = EntityWrapper(m_World, m_ServerIDToClientID.at(m_PlayerSpawnEvents.at(i).Player.ID));
        //e.Spawner = EntityWrapper(m_World, m_ServerIDToClientID.at(m_PlayerSpawnEvents.at(i).Spawner.ID));
        e.PlayerID = -1;
        e.PlayerName = m_PlayerSpawnEvents.at(i).PlayerName;
        m_EventBroker->Publish(e);
    }
    m_PlayerSpawnEvents = tempSpawn;
}

void Client::parsePlayersSpawned(Packet& packet)
{
    //Events::PlayerSpawned e;
    //e.Player = EntityWrapper(m_World, m_ServerIDToClientID[packet.ReadPrimitive<EntityID>()]);
    //e.Spawner = EntityWrapper(m_World, m_ServerIDToClientID[packet.ReadPrimitive<EntityID>()]);
    //e.PlayerID = -1;
    //e.PlayerName = packet.ReadString();
    //m_EventBroker->Publish(e);

    Events::PlayerSpawned e;
    e.Player = EntityWrapper(m_World, packet.ReadPrimitive<EntityID>());
    e.Spawner = EntityWrapper(m_World, packet.ReadPrimitive<EntityID>());
    e.PlayerID = -1;
    e.PlayerName = packet.ReadString();
    m_PlayerSpawnEvents.push_back(e);
    parseSpawnEvents();
}

void Client::parseEntityDeletion(Packet & packet)
{
    EntityID entityToDelete = packet.ReadPrimitive<EntityID>();
    // TODO: What if an entity that didn't previously exist comes as a delete request and later comes in a delayed snapshot?
    if (m_ServerIDToClientID.find(entityToDelete) != m_ServerIDToClientID.end()) {
        EntityID localEntity = m_ServerIDToClientID.at(entityToDelete);
        if (m_World->ValidEntity(localEntity)) {
            if (m_World->HasComponent(localEntity, "Player")) {
                Events::PlayerDeath e;
                e.Player = EntityWrapper(m_World, localEntity);
                m_EventBroker->Publish(e);
            } else {
                m_World->DeleteEntity(localEntity);
                deleteFromServerClientMaps(entityToDelete, localEntity);
            }
        }
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

void Client::parseDoubleJump(Packet & packet)
{
    EntityID serverID = packet.ReadPrimitive<EntityID>();
    if (!serverClientMapsHasEntity(serverID)) {
        return;
    }
    Events::DoubleJump e;
    e.entityID = m_ServerIDToClientID.at(serverID);
    // If player is local player do not publish to prevent infinite feedback loop
    if (e.entityID != m_LocalPlayer.ID) {
        m_EventBroker->Publish(e);
    }
}


void Client::parseDashEffect(Packet& packet)
{
    EntityID serverID = packet.ReadPrimitive<EntityID>();
    if (!serverClientMapsHasEntity(serverID)) {
        return;
    }
    Events::DashAbility e;
    e.Player = m_ServerIDToClientID.at(serverID);
    if (e.Player != m_LocalPlayer.ID) {
        m_EventBroker->Publish(e);
    }
}

void Client::parseAmmoPickup(Packet & packet)
{
    Events::AmmoPickup e;
    e.AmmoGain = packet.ReadPrimitive<int>();
    e.Player = m_LocalPlayer;
    m_EventBroker->Publish(e);
}

void Client::parseRemoveWorld(Packet & packet)
{
    removeWorld();
    Events::Reset e;
    m_EventBroker->Publish(e);
}

void Client::parseKDEvent(Packet & packet)
{
    Events::KillDeath e;
    e.Casualty = packet.ReadPrimitive<int>();
    e.CasualtyClass = packet.ReadPrimitive<int>();
    e.CasualtyName = packet.ReadString();
    e.CasualtyTeam = packet.ReadPrimitive<int>();

    e.Killer = packet.ReadPrimitive<int>();
    e.KillerClass = packet.ReadPrimitive<int>();
    e.KillerName = packet.ReadString();
    e.KillerTeam = packet.ReadPrimitive<int>();

    m_EventBroker->Publish(e);
}

void Client::parseWin(Packet& packet)
{
    Events::Win e;
    e.TeamThatWon = packet.ReadPrimitive<int>();
    m_EventBroker->Publish(e);
}

void Client::updateFields(Packet& packet, const ComponentInfo& componentInfo, const EntityID& entityID)
{
    for (auto field : componentInfo.FieldsInOrder) {
        ComponentInfo::Field_t fieldInfo = componentInfo.Fields.at(field);
        if (fieldInfo.Type == "string") {
            std::string& value = packet.ReadString();
            m_World->GetComponent(entityID, componentInfo.Name)[fieldInfo.Name] = value;
        } else {
            memcpy(m_World->GetComponent(entityID, componentInfo.Name).Data + fieldInfo.Offset, packet.ReadData(fieldInfo.Stride), fieldInfo.Stride);
        }
    }
}

SharedComponentWrapper Client::createSharedComponent(Packet& packet, EntityID entityID, const ComponentInfo& componentInfo)
{
    // Create shared allocation
    char* data = new char[sizeof(EntityID) + componentInfo.Stride];
    // Copy entity ID to start of data buffer
    memcpy(data, &entityID, sizeof(EntityID));
    // Read and copy fields
    for (auto& field : componentInfo.FieldsInOrder) {
        ComponentInfo::Field_t fieldInfo = componentInfo.Fields.at(field);
        if (fieldInfo.Type == "string") {
            new (data + sizeof(EntityID) + fieldInfo.Offset) std::string(packet.ReadString());
        } else {
            memcpy(data + sizeof(EntityID) + fieldInfo.Offset, packet.ReadData(fieldInfo.Stride), fieldInfo.Stride);
        }
    }

    return SharedComponentWrapper(componentInfo, boost::shared_array<char>(data));
}

void Client::ignoreFields(Packet& packet, const ComponentInfo& componentInfo)
{
    for (auto field : componentInfo.FieldsInOrder) {
        ComponentInfo::Field_t fieldInfo = componentInfo.Fields.at(field);
        if (fieldInfo.Type == "string") {
            packet.ReadString();
        } else {
            packet.ReadData(fieldInfo.Stride);
        }
    }
}

void Client::parseSnapshot(Packet& packet)
{
    // Read input commands
    std::size_t numInputCommands = packet.ReadPrimitive<std::size_t>();
    for (std::size_t i = 0; i < numInputCommands; ++i) {
        Events::InputCommand e;
        e.PlayerID = packet.ReadPrimitive<EntityID>();
        EntityID player = packet.ReadPrimitive<EntityID>();
        std::string command = packet.ReadString();
        float value = packet.ReadPrimitive<float>();
        if (m_ServerIDToClientID.find(player) != m_ServerIDToClientID.end()) {
            e.Player = EntityWrapper(m_World, m_ServerIDToClientID.at(player));
            e.Command = command;
            e.Value = value;
            m_EventBroker->Publish(e);
        }
    }

    // Read world state
    while (packet.DataReadSize() < packet.Size()) {
        EntityID serverEntityID = packet.ReadPrimitive<EntityID>();
        EntityID serverParentID = packet.ReadPrimitive<EntityID>();
        std::string serverEntityName = packet.ReadString();
        int ammountOfComponents = packet.ReadPrimitive<int>();
        for (int i = 0; i < ammountOfComponents; i++) {
            std::string componentType = packet.ReadString();
            const ComponentInfo& componentInfo = m_World->GetComponents(componentType)->ComponentInfo();
            if (serverClientMapsHasEntity(serverEntityID)) {
                EntityID localEntityID = m_ServerIDToClientID.at(serverEntityID);
                EntityWrapper localEntity(m_World, localEntityID);
                // Update entity
                if (m_World->HasComponent(localEntityID, componentType)) {
                    // TODO Fix memory leak here
                    SharedComponentWrapper newComponent = createSharedComponent(packet, localEntityID, componentInfo);
                    bool shouldApply = true;
                    // Apply potential filter function
                    if (m_SnapshotFilter != nullptr) {
                        shouldApply = m_SnapshotFilter->FilterComponent(localEntity, newComponent);
                    }
                    if (shouldApply) {
                        ComponentWrapper currentComponent = m_World->GetComponent(localEntityID, componentType);
                        memcpy(currentComponent.Data, newComponent.Data, componentInfo.Stride);
                    }

                    //if (localEntity != m_LocalPlayer && !localEntity.IsChildOf(m_LocalPlayer)) {
                    //    updateFields(packet, componentInfo, localEntityID);
                    //} else {
                    //    ignoreFields(packet, componentInfo);
                    //}
                } else {
                    // Has entity but no component
                    m_World->AttachComponent(localEntityID, componentType);
                    updateFields(packet, componentInfo, localEntityID);
                }
            } else {
                // Create Entity and component
                EntityID newLocalEntityID;
                if (serverParentID == EntityID_Invalid) {
                    newLocalEntityID = m_World->CreateEntity(EntityID_Invalid);
                } else {
                    if (serverClientMapsHasEntity(serverParentID)) {
                        newLocalEntityID = m_World->CreateEntity(m_ServerIDToClientID.at(serverParentID));
                    } else {
                        newLocalEntityID = m_World->CreateEntity(EntityID_Invalid);
                    }
                }
                m_World->SetName(newLocalEntityID, serverEntityName);
                insertIntoServerClientMaps(serverEntityID, newLocalEntityID);
                m_World->AttachComponent(newLocalEntityID, componentType);
                updateFields(packet, componentInfo, newLocalEntityID);
            }
        }
        // Parent logic
        // This should be enough beacause we know that the entities arives in pre-order (there will always be a parent)
        if (serverParentID != EntityID_Invalid && serverClientMapsHasEntity(serverParentID)) {
            EntityID localEntityID = m_ServerIDToClientID.at(serverEntityID);
            if (m_World->GetParent(localEntityID) != m_ServerIDToClientID.at(serverParentID)) {
                m_World->SetParent(localEntityID, m_ServerIDToClientID.at(serverParentID));
            }
        }
    }
    parseSpawnEvents();
}

void Client::disconnect()
{
    removeWorld();
    m_IsConnected = false;
    m_PreviousPacketID = 0;
    m_PacketID = 0;
    Packet packet(MessageType::Disconnect, m_SendPacketID);
    m_Reliable.Send(packet);
    m_Unreliable.Disconnect();
    m_Reliable.Disconnect();
    Events::PlayerDisconnected e;
    e.Entity = m_LocalPlayer.ID;
    e.PlayerID = -1;
    m_EventBroker->Publish(e);
    createMainMenu();
}

bool Client::OnInputCommand(const Events::InputCommand & e)
{
    if (e.PlayerID != -1) {
        return false;
    }

    if (e.Command == "ConnectToServer") { // Connect for now
        if (e.Value > 0) {
            m_Reliable.Connect(m_PlayerName, m_Address, m_Port);
            m_Unreliable.Connect(m_PlayerName, m_Address, m_Port);
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
        if (m_IsConnected) {
            m_InputCommandBuffer.push_back(e);
        }
        //LOG_DEBUG("Client::OnInputCommand: Command is %s. Value is %f. PlayerID is %i.", e.Command.c_str(), e.Value, e.PlayerID);
        return true;
    }
    return false;
}

bool Client::OnPlayerDamage(const Events::PlayerDamage & e)
{
    if (e.Inflictor != m_LocalPlayer) {
        return false;
    }
    // Could this happen?
    //if (!clientServerMapsHasEntity(e.Inflictor.ID) 
    //    || !clientServerMapsHasEntity(e.Victim.ID)) {
    //    return;
    //}

    Packet packet(MessageType::OnPlayerDamage, m_SendPacketID);
    packet.WritePrimitive(m_ClientIDToServerID.at(e.Inflictor.ID));
    packet.WritePrimitive(m_ClientIDToServerID.at(e.Victim.ID));
    packet.WritePrimitive(e.Damage);
    m_Reliable.Send(packet);

    return true;
}

bool Client::OnPlayerSpawned(const Events::PlayerSpawned& e)
{
    if (e.PlayerID == -1) {
        m_LocalPlayer = e.Player;
    }
    return true;
}


bool Client::OnDashAbility(const Events::DashAbility& e)
{
    if (!clientServerMapsHasEntity(e.Player) || e.Player != m_LocalPlayer.ID) {
        return false;
    }
    Packet packet(MessageType::OnDashEffect);
    packet.WritePrimitive(m_ClientIDToServerID.at(e.Player));
    m_Reliable.Send(packet);
    return true;
}


bool Client::OnConnectRequest(const Events::ConnectRequest& e)
{
    removeWorld();
    if (m_Reliable.Connect(m_PlayerName, e.IP, e.Port)) {
        m_Unreliable.Connect(m_PlayerName, e.IP, e.Port);
        // The client sent a successful connect message
        return true;

    } else {
        // The client could not send a successful connect message
        createMainMenu();
        // Load the main menu again ?
        return false;
    }
    return false;
}

bool Client::OnSearchForServers(const Events::SearchForServers& e)
{
    m_SearchingForServers = true;
    m_TimeSearched = 0;
    m_Serverlist.clear();
    Packet packet(MessageType::ServerlistRequest);
    m_ServerlistRequest.Broadcast(packet, 13); // TODO: Config
    return true;
}

void Client::parsePlayerDamage(Packet& packet)
{
    Events::PlayerDamage e;
    PlayerID victimID = packet.ReadPrimitive<EntityID>();
    PlayerID inflictorID = packet.ReadPrimitive<EntityID>();
    if (!serverClientMapsHasEntity(victimID) || !serverClientMapsHasEntity(inflictorID)) {
        return;
    }
    e.Inflictor = EntityWrapper(m_World, m_ServerIDToClientID.at(victimID));
    e.Victim = EntityWrapper(m_World, m_ServerIDToClientID.at(inflictorID));
    e.Damage = packet.ReadPrimitive<double>();
    // Don't rebroadcast our own player damage events or we'll have an infinite loop!
    if (e.Inflictor != m_LocalPlayer) {
        m_EventBroker->Publish(e);
    }
}

bool Client::OnDoubleJump(Events::DoubleJump & e)
{
    if (!clientServerMapsHasEntity(e.entityID) || e.entityID != m_LocalPlayer.ID) {
        return false;
    }
    Packet packet(MessageType::OnDoubleJump);
    packet.WritePrimitive(m_ClientIDToServerID.at(e.entityID));
    m_Reliable.Send(packet);
    return true;
}

void Client::sendLocalPlayerTransform()
{
    if (!m_LocalPlayer.Valid()) {
        return;
    }

    Packet packet(MessageType::PlayerTransform, m_SendPacketID);

    ComponentWrapper cTransform = m_LocalPlayer["Transform"];
    const glm::vec3& position = cTransform["Position"];
    const glm::vec3& orientation = cTransform["Orientation"];
    packet.WritePrimitive(position.x);
    packet.WritePrimitive(position.y);
    packet.WritePrimitive(position.z);
    packet.WritePrimitive(orientation.x);
    packet.WritePrimitive(orientation.y);
    packet.WritePrimitive(orientation.z);

    bool hasAssaultWeapon = m_LocalPlayer.HasComponent("AssaultWeapon");
    packet.WritePrimitive(hasAssaultWeapon);
    if (hasAssaultWeapon) {
        ComponentWrapper cAssaultWeapon = m_LocalPlayer["AssaultWeapon"];
        packet.WritePrimitive((int)cAssaultWeapon["MagazineAmmo"]);
        packet.WritePrimitive((int)cAssaultWeapon["Ammo"]);
    }

    m_Unreliable.Send(packet);
}

void Client::identifyPacketLoss()
{
    // if no packets lost, difference should be equal to 1
    int difference = m_PacketID - m_PreviousPacketID;
    if (difference != 1) {
        LOG_INFO("%i Packet(s) were lost...", difference - 1);
    }
}

void Client::hasServerTimedOut()
{
    // Time in ms
    double timeSincePing = 1000 * (std::clock() - m_StartPingTime) / static_cast<double>(CLOCKS_PER_SEC);
    if (timeSincePing > m_TimeoutMs) {
        // Clear everything and go to menu.
        LOG_INFO("Server has timed out, returning to menu, Beep Boop.");
        ImGui::OpenPopup("Disconnected");
        disconnect();
    }
}

EntityID Client::createPlayer()
{
    EntityID entityID = m_World->CreateEntity();
    ComponentWrapper transform = m_World->AttachComponent(entityID, "Transform");
    ComponentWrapper model = m_World->AttachComponent(entityID, "Model");
    model["Resource"] = "Models/Core/UnitSphere.mesh";
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
        m_Reliable.Send(packet);
        m_InputCommandBuffer.clear();
    }
}

void Client::becomePlayer()
{
    Packet packet = Packet(MessageType::BecomePlayer, m_SendPacketID);
    m_Reliable.Send(packet);
}


void Client::displayServerlist()
{
    LOG_INFO("This is a serverlist:\n");
    for (int i = 0; i < m_Serverlist.size(); i++) {
        ServerInfo si = m_Serverlist[i];
        LOG_INFO("%s:%i\t%s\t%i\n", si.Address.c_str(), si.Port, si.Name.c_str(), si.PlayersConnected);
    }
}

void Client::createMainMenu()
{
    auto entityFile = ResourceManager::Load<EntityFile>("Schema/Entities/StartMenu.xml");
    entityFile->MergeInto(m_World);
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
