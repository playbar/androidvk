// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/NetPlayServer.h"

#include <algorithm>
#include <cinttypes>
#include <cstddef>
#include <cstdio>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <type_traits>
#include <unordered_set>
#include <vector>

#include <lzo/lzo1x.h>

#include "Common/CommonPaths.h"
#include "Common/ENetUtil.h"
#include "Common/File.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/SFMLHelper.h"
#include "Common/StringUtil.h"
#include "Common/UPnP.h"
#include "Common/Version.h"
#include "Core/Config/MainSettings.h"
#include "Core/Config/NetplaySettings.h"
#include "Core/ConfigManager.h"
#include "Core/HW/GCMemcard/GCMemcardDirectory.h"
#include "Core/HW/GCMemcard/GCMemcardRaw.h"
#include "Core/HW/Sram.h"
#include "Core/HW/WiiSave.h"
#include "Core/HW/WiiSaveStructs.h"
#include "Core/IOS/FS/FileSystem.h"
#include "Core/NetPlayClient.h"  //for NetPlayUI
#include "DiscIO/Enums.h"
#include "InputCommon/GCPadStatus.h"
#include "UICommon/GameFile.h"

#if !defined(_WIN32)
#include <sys/socket.h>
#include <sys/types.h>
#ifdef __HAIKU__
#define _BSD_SOURCE
#include <bsd/ifaddrs.h>
#elif !defined ANDROID
#include <ifaddrs.h>
#endif
#include <arpa/inet.h>
#endif

namespace NetPlay
{
NetPlayServer::~NetPlayServer()
{
  if (is_connected)
  {
    m_do_loop = false;
    m_thread.join();
    enet_host_destroy(m_server);

    if (g_MainNetHost.get() == m_server)
    {
      g_MainNetHost.release();
    }

    if (m_traversal_client)
    {
      g_TraversalClient->m_Client = nullptr;
      ReleaseTraversalClient();
    }
  }

#ifdef USE_UPNP
  UPnP::StopPortmapping();
#endif
}

// called from ---GUI--- thread
NetPlayServer::NetPlayServer(const u16 port, const bool forward_port,
                             const NetTraversalConfig& traversal_config)
{
  //--use server time
  if (enet_initialize() != 0)
  {
    PanicAlertT("Enet Didn't Initialize");
  }

  m_pad_map.fill(-1);
  m_wiimote_map.fill(-1);

  if (traversal_config.use_traversal)
  {
    if (!EnsureTraversalClient(traversal_config.traversal_host, traversal_config.traversal_port,
                               port))
      return;

    g_TraversalClient->m_Client = this;
    m_traversal_client = g_TraversalClient.get();

    m_server = g_MainNetHost.get();

    if (g_TraversalClient->GetState() == TraversalClient::Failure)
      g_TraversalClient->ReconnectToServer();
  }
  else
  {
    ENetAddress serverAddr;
    serverAddr.host = ENET_HOST_ANY;
    serverAddr.port = port;
    m_server = enet_host_create(&serverAddr, 10, 3, 0, 0);
    if (m_server != nullptr)
      m_server->intercept = ENetUtil::InterceptCallback;
  }
  if (m_server != nullptr)
  {
    is_connected = true;
    m_do_loop = true;
    m_thread = std::thread(&NetPlayServer::ThreadFunc, this);
    m_target_buffer_size = 5;

#ifdef USE_UPNP
    if (forward_port)
      UPnP::TryPortmapping(port);
#endif
  }
}

// called from ---NETPLAY--- thread
void NetPlayServer::ThreadFunc()
{
  while (m_do_loop)
  {
    // update pings every so many seconds
    if ((m_ping_timer.GetTimeElapsed() > 1000) || m_update_pings)
    {
      m_ping_key = Common::Timer::GetTimeMs();

      sf::Packet spac;
      spac << (MessageId)NP_MSG_PING;
      spac << m_ping_key;

      m_ping_timer.Start();
      SendToClients(spac);
      m_update_pings = false;
    }

    ENetEvent netEvent;
    int net;
    if (m_traversal_client)
      m_traversal_client->HandleResends();
    net = enet_host_service(m_server, &netEvent, 1000);
    while (!m_async_queue.Empty())
    {
      {
        std::lock_guard<std::recursive_mutex> lkp(m_crit.players);
        SendToClients(m_async_queue.Front());
      }
      m_async_queue.Pop();
    }
    if (net > 0)
    {
      switch (netEvent.type)
      {
      case ENET_EVENT_TYPE_CONNECT:
      {
        ENetPeer* accept_peer = netEvent.peer;
        unsigned int error;
        {
          std::lock_guard<std::recursive_mutex> lkg(m_crit.game);
          error = OnConnect(accept_peer);
        }

        if (error)
        {
          sf::Packet spac;
          spac << (MessageId)error;
          // don't need to lock, this client isn't in the client map
          Send(accept_peer, spac);
          if (netEvent.peer->data)
          {
            delete (PlayerId*)netEvent.peer->data;
            netEvent.peer->data = nullptr;
          }
          enet_peer_disconnect_later(accept_peer, 0);
        }
      }
      break;
      case ENET_EVENT_TYPE_RECEIVE:
      {
        sf::Packet rpac;
        rpac.append(netEvent.packet->data, netEvent.packet->dataLength);

        auto it = m_players.find(*(PlayerId*)netEvent.peer->data);
        Client& client = it->second;
        if (OnData(rpac, client) != 0)
        {
          // if a bad packet is received, disconnect the client
          std::lock_guard<std::recursive_mutex> lkg(m_crit.game);
          OnDisconnect(client);

          if (netEvent.peer->data)
          {
            delete (PlayerId*)netEvent.peer->data;
            netEvent.peer->data = nullptr;
          }
        }
        enet_packet_destroy(netEvent.packet);
      }
      break;
      case ENET_EVENT_TYPE_DISCONNECT:
      {
        std::lock_guard<std::recursive_mutex> lkg(m_crit.game);
        if (!netEvent.peer->data)
          break;
        auto it = m_players.find(*(PlayerId*)netEvent.peer->data);
        if (it != m_players.end())
        {
          Client& client = it->second;
          OnDisconnect(client);

          if (netEvent.peer->data)
          {
            delete (PlayerId*)netEvent.peer->data;
            netEvent.peer->data = nullptr;
          }
        }
      }
      break;
      default:
        break;
      }
    }
  }

  // close listening socket and client sockets
  for (auto& player_entry : m_players)
  {
    delete (PlayerId*)player_entry.second.socket->data;
    player_entry.second.socket->data = nullptr;
    enet_peer_disconnect(player_entry.second.socket, 0);
  }
}

// called from ---NETPLAY--- thread
unsigned int NetPlayServer::OnConnect(ENetPeer* socket)
{
  sf::Packet rpac;
  ENetPacket* epack;
  do
  {
    epack = enet_peer_receive(socket, nullptr);
  } while (epack == nullptr);
  rpac.append(epack->data, epack->dataLength);

  // give new client first available id
  PlayerId pid = 1;
  for (auto i = m_players.begin(); i != m_players.end(); ++i)
  {
    if (i->second.pid == pid)
    {
      pid++;
      i = m_players.begin();
    }
  }
  socket->data = new PlayerId(pid);

  std::string npver;
  rpac >> npver;
  // Dolphin netplay version
  if (npver != Common::scm_rev_git_str)
    return CON_ERR_VERSION_MISMATCH;

  // game is currently running
  if (m_is_running)
    return CON_ERR_GAME_RUNNING;

  // too many players
  if (m_players.size() >= 255)
    return CON_ERR_SERVER_FULL;

  // cause pings to be updated
  m_update_pings = true;

  Client player;
  player.pid = pid;
  player.socket = socket;

  rpac >> player.revision;
  rpac >> player.name;

  enet_packet_destroy(epack);
  // try to automatically assign new user a pad
  for (PadMapping& mapping : m_pad_map)
  {
    if (mapping == -1)
    {
      mapping = player.pid;
      break;
    }
  }

  // send join message to already connected clients
  sf::Packet spac;
  spac << static_cast<MessageId>(NP_MSG_PLAYER_JOIN);
  spac << player.pid << player.name << player.revision;
  SendToClients(spac);

  // send new client success message with their id
  spac.clear();
  spac << static_cast<MessageId>(0);
  spac << player.pid;
  Send(player.socket, spac);

  // send new client the selected game
  if (m_selected_game != "")
  {
    spac.clear();
    spac << static_cast<MessageId>(NP_MSG_CHANGE_GAME);
    spac << m_selected_game;
    Send(player.socket, spac);
  }

  if (!m_host_input_authority)
  {
    // send the pad buffer value
    spac.clear();
    spac << static_cast<MessageId>(NP_MSG_PAD_BUFFER);
    spac << static_cast<u32>(m_target_buffer_size);
    Send(player.socket, spac);
  }

  // send input authority state
  spac.clear();
  spac << static_cast<MessageId>(NP_MSG_HOST_INPUT_AUTHORITY);
  spac << m_host_input_authority;
  Send(player.socket, spac);

  // sync GC SRAM with new client
  if (!g_SRAM_netplay_initialized)
  {
    SConfig::GetInstance().m_strSRAM = File::GetUserPath(F_GCSRAM_IDX);
    InitSRAM();
    g_SRAM_netplay_initialized = true;
  }
  spac.clear();
  spac << static_cast<MessageId>(NP_MSG_SYNC_GC_SRAM);
  for (size_t i = 0; i < sizeof(g_SRAM) - offsetof(Sram, settings); ++i)
  {
    spac << g_SRAM[offsetof(Sram, settings) + i];
  }
  Send(player.socket, spac);

  // sync values with new client
  for (const auto& p : m_players)
  {
    spac.clear();
    spac << static_cast<MessageId>(NP_MSG_PLAYER_JOIN);
    spac << p.second.pid << p.second.name << p.second.revision;
    Send(player.socket, spac);

    spac.clear();
    spac << static_cast<MessageId>(NP_MSG_GAME_STATUS);
    spac << p.second.pid << static_cast<u32>(p.second.game_status);
    Send(player.socket, spac);
  }

  if (Config::Get(Config::NETPLAY_ENABLE_QOS))
    player.qos_session = Common::QoSSession(player.socket);

  // add client to the player list
  {
    std::lock_guard<std::recursive_mutex> lkp(m_crit.players);
    m_players.emplace(*(PlayerId*)player.socket->data, std::move(player));
    UpdatePadMapping();  // sync pad mappings with everyone
    UpdateWiimoteMapping();
  }

  return 0;
}

// called from ---NETPLAY--- thread
unsigned int NetPlayServer::OnDisconnect(const Client& player)
{
  const PlayerId pid = player.pid;

  if (m_is_running)
  {
    for (PadMapping mapping : m_pad_map)
    {
      if (mapping == pid && pid != 1)
      {
        std::lock_guard<std::recursive_mutex> lkg(m_crit.game);
        m_is_running = false;

        sf::Packet spac;
        spac << (MessageId)NP_MSG_DISABLE_GAME;
        // this thread doesn't need players lock
        SendToClients(spac, static_cast<PlayerId>(-1));
        break;
      }
    }
  }

  sf::Packet spac;
  spac << (MessageId)NP_MSG_PLAYER_LEAVE;
  spac << pid;

  enet_peer_disconnect(player.socket, 0);

  std::lock_guard<std::recursive_mutex> lkp(m_crit.players);
  auto it = m_players.find(player.pid);
  if (it != m_players.end())
    m_players.erase(it);

  // alert other players of disconnect
  SendToClients(spac);

  for (PadMapping& mapping : m_pad_map)
  {
    if (mapping == pid)
    {
      mapping = -1;
      UpdatePadMapping();
    }
  }

  for (PadMapping& mapping : m_wiimote_map)
  {
    if (mapping == pid)
    {
      mapping = -1;
      UpdateWiimoteMapping();
    }
  }

  return 0;
}

// called from ---GUI--- thread
PadMappingArray NetPlayServer::GetPadMapping() const
{
  return m_pad_map;
}

PadMappingArray NetPlayServer::GetWiimoteMapping() const
{
  return m_wiimote_map;
}

// called from ---GUI--- thread
void NetPlayServer::SetPadMapping(const PadMappingArray& mappings)
{
  m_pad_map = mappings;
  UpdatePadMapping();
}

// called from ---GUI--- thread
void NetPlayServer::SetWiimoteMapping(const PadMappingArray& mappings)
{
  m_wiimote_map = mappings;
  UpdateWiimoteMapping();
}

// called from ---GUI--- thread and ---NETPLAY--- thread
void NetPlayServer::UpdatePadMapping()
{
  sf::Packet spac;
  spac << (MessageId)NP_MSG_PAD_MAPPING;
  for (PadMapping mapping : m_pad_map)
  {
    spac << mapping;
  }
  SendToClients(spac);
}

// called from ---NETPLAY--- thread
void NetPlayServer::UpdateWiimoteMapping()
{
  sf::Packet spac;
  spac << (MessageId)NP_MSG_WIIMOTE_MAPPING;
  for (PadMapping mapping : m_wiimote_map)
  {
    spac << mapping;
  }
  SendToClients(spac);
}

// called from ---GUI--- thread and ---NETPLAY--- thread
void NetPlayServer::AdjustPadBufferSize(unsigned int size)
{
  std::lock_guard<std::recursive_mutex> lkg(m_crit.game);

  m_target_buffer_size = size;

  // tell clients to change buffer size
  sf::Packet spac;
  spac << static_cast<MessageId>(NP_MSG_PAD_BUFFER);
  spac << static_cast<u32>(m_target_buffer_size);

  SendAsyncToClients(std::move(spac));
}

void NetPlayServer::SetHostInputAuthority(const bool enable)
{
  std::lock_guard<std::recursive_mutex> lkg(m_crit.game);

  m_host_input_authority = enable;

  // tell clients about the new value
  sf::Packet spac;
  spac << static_cast<MessageId>(NP_MSG_HOST_INPUT_AUTHORITY);
  spac << m_host_input_authority;

  SendAsyncToClients(std::move(spac));

  // resend pad buffer to clients when disabled
  if (!m_host_input_authority)
    AdjustPadBufferSize(m_target_buffer_size);
}

void NetPlayServer::SendAsyncToClients(sf::Packet&& packet)
{
  {
    std::lock_guard<std::recursive_mutex> lkq(m_crit.async_queue_write);
    m_async_queue.Push(std::move(packet));
  }
  ENetUtil::WakeupThread(m_server);
}

// called from ---NETPLAY--- thread
unsigned int NetPlayServer::OnData(sf::Packet& packet, Client& player)
{
  MessageId mid;
  packet >> mid;

  INFO_LOG(NETPLAY, "Got client message: %x", mid);

  // don't need lock because this is the only thread that modifies the players
  // only need locks for writes to m_players in this thread

  switch (mid)
  {
  case NP_MSG_CHAT_MESSAGE:
  {
    std::string msg;
    packet >> msg;

    // send msg to other clients
    sf::Packet spac;
    spac << (MessageId)NP_MSG_CHAT_MESSAGE;
    spac << player.pid;
    spac << msg;

    SendToClients(spac, player.pid);
  }
  break;

  case NP_MSG_PAD_DATA:
  {
    // if this is pad data from the last game still being received, ignore it
    if (player.current_game != m_current_game)
      break;

    sf::Packet spac;
    spac << static_cast<MessageId>(NP_MSG_PAD_DATA);

    while (!packet.endOfPacket())
    {
      PadMapping map;
      packet >> map;

      // If the data is not from the correct player,
      // then disconnect them.
      if (m_pad_map.at(map) != player.pid)
      {
        return 1;
      }

      GCPadStatus pad;
      packet >> pad.button >> pad.analogA >> pad.analogB >> pad.stickX >> pad.stickY >>
          pad.substickX >> pad.substickY >> pad.triggerLeft >> pad.triggerRight >> pad.isConnected;

      if (m_host_input_authority)
      {
        m_last_pad_status[map] = pad;

        if (!m_first_pad_status_received[map])
        {
          m_first_pad_status_received[map] = true;
          SendFirstReceivedToHost(map, true);
        }
      }
      else
      {
        spac << map << pad.button << pad.analogA << pad.analogB << pad.stickX << pad.stickY
             << pad.substickX << pad.substickY << pad.triggerLeft << pad.triggerRight
             << pad.isConnected;
      }
    }

    if (!m_host_input_authority)
      SendToClients(spac, player.pid);
  }
  break;

  case NP_MSG_PAD_HOST_POLL:
  {
    PadMapping pad_num;
    packet >> pad_num;

    sf::Packet spac;
    spac << static_cast<MessageId>(NP_MSG_PAD_DATA);

    if (pad_num < 0)
    {
      for (size_t i = 0; i < m_pad_map.size(); i++)
      {
        if (m_pad_map[i] == -1)
          continue;

        const GCPadStatus& pad = m_last_pad_status[i];
        spac << static_cast<PadMapping>(i) << pad.button << pad.analogA << pad.analogB << pad.stickX
             << pad.stickY << pad.substickX << pad.substickY << pad.triggerLeft << pad.triggerRight
             << pad.isConnected;
      }
    }
    else if (m_pad_map.at(pad_num) != -1)
    {
      const GCPadStatus& pad = m_last_pad_status[pad_num];
      spac << pad_num << pad.button << pad.analogA << pad.analogB << pad.stickX << pad.stickY
           << pad.substickX << pad.substickY << pad.triggerLeft << pad.triggerRight
           << pad.isConnected;
    }

    SendToClients(spac);
  }
  break;

  case NP_MSG_WIIMOTE_DATA:
  {
    // if this is Wiimote data from the last game still being received, ignore it
    if (player.current_game != m_current_game)
      break;

    PadMapping map = 0;
    u8 size;
    packet >> map >> size;
    std::vector<u8> data(size);
    for (size_t i = 0; i < data.size(); ++i)
      packet >> data[i];

    // If the data is not from the correct player,
    // then disconnect them.
    if (m_wiimote_map.at(map) != player.pid)
    {
      return 1;
    }

    // relay to clients
    sf::Packet spac;
    spac << (MessageId)NP_MSG_WIIMOTE_DATA;
    spac << map;
    spac << size;
    for (const u8& byte : data)
      spac << byte;

    SendToClients(spac, player.pid);
  }
  break;

  case NP_MSG_PONG:
  {
    const u32 ping = (u32)m_ping_timer.GetTimeElapsed();
    u32 ping_key = 0;
    packet >> ping_key;

    if (m_ping_key == ping_key)
    {
      player.ping = ping;
    }

    sf::Packet spac;
    spac << (MessageId)NP_MSG_PLAYER_PING_DATA;
    spac << player.pid;
    spac << player.ping;

    SendToClients(spac);
  }
  break;

  case NP_MSG_START_GAME:
  {
    packet >> player.current_game;
  }
  break;

  case NP_MSG_STOP_GAME:
  {
    if (!m_is_running)
      break;

    m_is_running = false;

    // tell clients to stop game
    sf::Packet spac;
    spac << (MessageId)NP_MSG_STOP_GAME;

    std::lock_guard<std::recursive_mutex> lkp(m_crit.players);
    SendToClients(spac);
  }
  break;

  case NP_MSG_GAME_STATUS:
  {
    u32 status;
    packet >> status;

    m_players[player.pid].game_status = static_cast<PlayerGameStatus>(status);

    // send msg to other clients
    sf::Packet spac;
    spac << static_cast<MessageId>(NP_MSG_GAME_STATUS);
    spac << player.pid;
    spac << status;

    SendToClients(spac);
  }
  break;

  case NP_MSG_IPL_STATUS:
  {
    bool status;
    packet >> status;

    m_players[player.pid].has_ipl_dump = status;
  }
  break;

  case NP_MSG_TIMEBASE:
  {
    u64 timebase = Common::PacketReadU64(packet);
    u32 frame;
    packet >> frame;

    if (m_desync_detected)
      break;

    std::vector<std::pair<PlayerId, u64>>& timebases = m_timebase_by_frame[frame];
    timebases.emplace_back(player.pid, timebase);
    if (timebases.size() >= m_players.size())
    {
      // we have all records for this frame

      if (!std::all_of(timebases.begin(), timebases.end(), [&](std::pair<PlayerId, u64> pair) {
            return pair.second == timebases[0].second;
          }))
      {
        int pid_to_blame = -1;
        for (auto pair : timebases)
        {
          if (std::all_of(timebases.begin(), timebases.end(), [&](std::pair<PlayerId, u64> other) {
                return other.first == pair.first || other.second != pair.second;
              }))
          {
            // we are the only outlier
            pid_to_blame = pair.first;
            break;
          }
        }

        sf::Packet spac;
        spac << (MessageId)NP_MSG_DESYNC_DETECTED;
        spac << pid_to_blame;
        spac << frame;
        SendToClients(spac);

        m_desync_detected = true;
      }
      m_timebase_by_frame.erase(frame);
    }
  }
  break;

  case NP_MSG_MD5_PROGRESS:
  {
    int progress;
    packet >> progress;

    sf::Packet spac;
    spac << static_cast<MessageId>(NP_MSG_MD5_PROGRESS);
    spac << player.pid;
    spac << progress;

    SendToClients(spac);
  }
  break;

  case NP_MSG_MD5_RESULT:
  {
    std::string result;
    packet >> result;

    sf::Packet spac;
    spac << static_cast<MessageId>(NP_MSG_MD5_RESULT);
    spac << player.pid;
    spac << result;

    SendToClients(spac);
  }
  break;

  case NP_MSG_MD5_ERROR:
  {
    std::string error;
    packet >> error;

    sf::Packet spac;
    spac << static_cast<MessageId>(NP_MSG_MD5_ERROR);
    spac << player.pid;
    spac << error;

    SendToClients(spac);
  }
  break;

  case NP_MSG_SYNC_SAVE_DATA:
  {
    MessageId sub_id;
    packet >> sub_id;

    switch (sub_id)
    {
    case SYNC_SAVE_DATA_SUCCESS:
    {
      if (m_start_pending)
      {
        m_save_data_synced_players++;
        if (m_save_data_synced_players >= m_players.size() - 1)
        {
          m_dialog->AppendChat(GetStringT("All players synchronized."));
          StartGame();
        }
      }
    }
    break;

    case SYNC_SAVE_DATA_FAILURE:
    {
      m_dialog->AppendChat(
          StringFromFormat(GetStringT("%s failed to synchronize.").c_str(), player.name.c_str()));
      m_dialog->OnSaveDataSyncFailure();
      m_start_pending = false;
    }
    break;

    default:
      PanicAlertT(
          "Unknown SYNC_SAVE_DATA message with id:%d received from player:%d Kicking player!",
          sub_id, player.pid);
      return 1;
    }
  }
  break;

  default:
    PanicAlertT("Unknown message with id:%d received from player:%d Kicking player!", mid,
                player.pid);
    // unknown message, kick the client
    return 1;
  }

  return 0;
}

void NetPlayServer::OnTraversalStateChanged()
{
  if (!m_dialog)
    return;

  const TraversalClient::State state = m_traversal_client->GetState();

  if (state == TraversalClient::Failure)
    m_dialog->OnTraversalError(m_traversal_client->GetFailureReason());

  m_dialog->OnTraversalStateChanged(state);
}

// called from ---GUI--- thread
void NetPlayServer::SendChatMessage(const std::string& msg)
{
  sf::Packet spac;
  spac << static_cast<MessageId>(NP_MSG_CHAT_MESSAGE);
  spac << static_cast<PlayerId>(0);  // server id always 0
  spac << msg;

  SendAsyncToClients(std::move(spac));
}

// called from ---GUI--- thread
bool NetPlayServer::ChangeGame(const std::string& game)
{
  std::lock_guard<std::recursive_mutex> lkg(m_crit.game);

  m_selected_game = game;

  // send changed game to clients
  sf::Packet spac;
  spac << static_cast<MessageId>(NP_MSG_CHANGE_GAME);
  spac << game;

  SendAsyncToClients(std::move(spac));

  return true;
}

// called from ---GUI--- thread
bool NetPlayServer::ComputeMD5(const std::string& file_identifier)
{
  sf::Packet spac;
  spac << static_cast<MessageId>(NP_MSG_COMPUTE_MD5);
  spac << file_identifier;

  SendAsyncToClients(std::move(spac));

  return true;
}

// called from ---GUI--- thread
bool NetPlayServer::AbortMD5()
{
  sf::Packet spac;
  spac << static_cast<MessageId>(NP_MSG_MD5_ABORT);

  SendAsyncToClients(std::move(spac));

  return true;
}

// called from ---GUI--- thread
void NetPlayServer::SetNetSettings(const NetSettings& settings)
{
  m_settings = settings;
}

bool NetPlayServer::DoAllPlayersHaveIPLDump() const
{
  return std::all_of(m_players.begin(), m_players.end(),
                     [](const auto& p) { return p.second.has_ipl_dump; });
}

// called from ---GUI--- thread
bool NetPlayServer::RequestStartGame()
{
  if (m_settings.m_SyncSaveData && m_players.size() > 1)
  {
    if (!SyncSaveData())
    {
      PanicAlertT("Error synchronizing save data!");
      return false;
    }

    m_start_pending = true;
  }
  else
  {
    return StartGame();
  }

  return true;
}

// called from multiple threads
bool NetPlayServer::StartGame()
{
  m_timebase_by_frame.clear();
  m_desync_detected = false;
  std::lock_guard<std::recursive_mutex> lkg(m_crit.game);
  m_current_game = Common::Timer::GetTimeMs();

  // no change, just update with clients
  if (!m_host_input_authority)
    AdjustPadBufferSize(m_target_buffer_size);

  m_first_pad_status_received.fill(false);

  const sf::Uint64 initial_rtc = GetInitialNetPlayRTC();

  const std::string region = SConfig::GetDirectoryForRegion(
      SConfig::ToGameCubeRegion(m_dialog->FindGameFile(m_selected_game)->GetRegion()));

  // tell clients to start game
  sf::Packet spac;
  spac << static_cast<MessageId>(NP_MSG_START_GAME);
  spac << m_current_game;
  spac << m_settings.m_CPUthread;
  spac << static_cast<std::underlying_type_t<PowerPC::CPUCore>>(m_settings.m_CPUcore);
  spac << m_settings.m_EnableCheats;
  spac << m_settings.m_SelectedLanguage;
  spac << m_settings.m_OverrideGCLanguage;
  spac << m_settings.m_ProgressiveScan;
  spac << m_settings.m_PAL60;
  spac << m_settings.m_DSPEnableJIT;
  spac << m_settings.m_DSPHLE;
  spac << m_settings.m_WriteToMemcard;
  spac << m_settings.m_CopyWiiSave;
  spac << m_settings.m_OCEnable;
  spac << m_settings.m_OCFactor;
  spac << m_settings.m_ReducePollingRate;
  spac << m_settings.m_EXIDevice[0];
  spac << m_settings.m_EXIDevice[1];
  spac << m_settings.m_EFBAccessEnable;
  spac << m_settings.m_BBoxEnable;
  spac << m_settings.m_ForceProgressive;
  spac << m_settings.m_EFBToTextureEnable;
  spac << m_settings.m_XFBToTextureEnable;
  spac << m_settings.m_DisableCopyToVRAM;
  spac << m_settings.m_ImmediateXFBEnable;
  spac << m_settings.m_EFBEmulateFormatChanges;
  spac << m_settings.m_SafeTextureCacheColorSamples;
  spac << m_settings.m_PerfQueriesEnable;
  spac << m_settings.m_FPRF;
  spac << m_settings.m_AccurateNaNs;
  spac << m_settings.m_SyncOnSkipIdle;
  spac << m_settings.m_SyncGPU;
  spac << m_settings.m_SyncGpuMaxDistance;
  spac << m_settings.m_SyncGpuMinDistance;
  spac << m_settings.m_SyncGpuOverclock;
  spac << m_settings.m_JITFollowBranch;
  spac << m_settings.m_FastDiscSpeed;
  spac << m_settings.m_MMU;
  spac << m_settings.m_Fastmem;
  spac << m_settings.m_SkipIPL;
  spac << m_settings.m_LoadIPLDump;
  spac << m_settings.m_VertexRounding;
  spac << m_settings.m_InternalResolution;
  spac << m_settings.m_EFBScaledCopy;
  spac << m_settings.m_FastDepthCalc;
  spac << m_settings.m_EnablePixelLighting;
  spac << m_settings.m_WidescreenHack;
  spac << m_settings.m_ForceFiltering;
  spac << m_settings.m_MaxAnisotropy;
  spac << m_settings.m_ForceTrueColor;
  spac << m_settings.m_DisableCopyFilter;
  spac << m_settings.m_DisableFog;
  spac << m_settings.m_ArbitraryMipmapDetection;
  spac << m_settings.m_ArbitraryMipmapDetectionThreshold;
  spac << m_settings.m_EnableGPUTextureDecoding;
  spac << m_settings.m_StrictSettingsSync;
  spac << initial_rtc;
  spac << m_settings.m_SyncSaveData;
  spac << region;

  SendAsyncToClients(std::move(spac));

  m_start_pending = false;
  m_is_running = true;

  return true;
}

// called from ---GUI--- thread
bool NetPlayServer::SyncSaveData()
{
  m_save_data_synced_players = 0;

  u8 save_count = 0;

  constexpr size_t exi_device_count = 2;
  for (size_t i = 0; i < exi_device_count; i++)
  {
    if (m_settings.m_EXIDevice[i] == ExpansionInterface::EXIDEVICE_MEMORYCARD ||
        SConfig::GetInstance().m_EXIDevice[i] == ExpansionInterface::EXIDEVICE_MEMORYCARDFOLDER)
    {
      save_count++;
    }
  }

  const auto game = m_dialog->FindGameFile(m_selected_game);
  if (game == nullptr)
  {
    PanicAlertT("Selected game doesn't exist in game list!");
    return false;
  }

  bool wii_save = false;
  if (m_settings.m_CopyWiiSave && (game->GetPlatform() == DiscIO::Platform::WiiDisc ||
                                   game->GetPlatform() == DiscIO::Platform::WiiWAD))
  {
    wii_save = true;
    save_count++;
  }

  {
    sf::Packet pac;
    pac << static_cast<MessageId>(NP_MSG_SYNC_SAVE_DATA);
    pac << static_cast<MessageId>(SYNC_SAVE_DATA_NOTIFY);
    pac << save_count;

    SendAsyncToClients(std::move(pac));
  }

  if (save_count == 0)
    return true;

  const std::string region =
      SConfig::GetDirectoryForRegion(SConfig::ToGameCubeRegion(game->GetRegion()));

  for (size_t i = 0; i < exi_device_count; i++)
  {
    const bool is_slot_a = i == 0;

    if (m_settings.m_EXIDevice[i] == ExpansionInterface::EXIDEVICE_MEMORYCARD)
    {
      std::string path = is_slot_a ? Config::Get(Config::MAIN_MEMCARD_A_PATH) :
                                     Config::Get(Config::MAIN_MEMCARD_B_PATH);

      MemoryCard::CheckPath(path, region, is_slot_a);

      bool mc251;
      IniFile gameIni = SConfig::LoadGameIni(game->GetGameID(), game->GetRevision());
      gameIni.GetOrCreateSection("Core")->Get("MemoryCard251", &mc251, false);

      if (mc251)
        path.insert(path.find_last_of('.'), ".251");

      sf::Packet pac;
      pac << static_cast<MessageId>(NP_MSG_SYNC_SAVE_DATA);
      pac << static_cast<MessageId>(SYNC_SAVE_DATA_RAW);
      pac << is_slot_a << region << mc251;

      if (File::Exists(path))
      {
        if (!CompressFileIntoPacket(path, pac))
          return false;
      }
      else
      {
        // No file, so we'll say the size is 0
        pac << sf::Uint64{0};
      }

      SendAsyncToClients(std::move(pac));
    }
    else if (SConfig::GetInstance().m_EXIDevice[i] ==
             ExpansionInterface::EXIDEVICE_MEMORYCARDFOLDER)
    {
      const std::string path = File::GetUserPath(D_GCUSER_IDX) + region + DIR_SEP +
                               StringFromFormat("Card %c", is_slot_a ? 'A' : 'B');

      sf::Packet pac;
      pac << static_cast<MessageId>(NP_MSG_SYNC_SAVE_DATA);
      pac << static_cast<MessageId>(SYNC_SAVE_DATA_GCI);
      pac << is_slot_a;

      if (File::IsDirectory(path))
      {
        std::vector<std::string> files =
            GCMemcardDirectory::GetFileNamesForGameID(path + DIR_SEP, game->GetGameID());

        pac << static_cast<u8>(files.size());

        for (const std::string& file : files)
        {
          pac << file.substr(file.find_last_of('/') + 1);
          if (!CompressFileIntoPacket(file, pac))
            return false;
        }
      }
      else
      {
        pac << static_cast<u8>(0);
      }

      SendAsyncToClients(std::move(pac));
    }
  }

  if (wii_save)
  {
    const auto configured_fs = IOS::HLE::FS::MakeFileSystem(IOS::HLE::FS::Location::Configured);
    const auto save = WiiSave::MakeNandStorage(configured_fs.get(), game->GetTitleID());

    sf::Packet pac;
    pac << static_cast<MessageId>(NP_MSG_SYNC_SAVE_DATA);
    pac << static_cast<MessageId>(SYNC_SAVE_DATA_WII);

    if (save->SaveExists())
    {
      const std::optional<WiiSave::Header> header = save->ReadHeader();
      const std::optional<WiiSave::BkHeader> bk_header = save->ReadBkHeader();
      const std::optional<std::vector<WiiSave::Storage::SaveFile>> files = save->ReadFiles();
      if (!header || !bk_header || !files)
        return false;

      pac << true;  // save exists

      // Header
      pac << sf::Uint64{header->tid};
      pac << header->banner_size << header->permissions << header->unk1;
      for (size_t i = 0; i < header->md5.size(); i++)
        pac << header->md5[i];
      pac << header->unk2;
      for (size_t i = 0; i < header->banner_size; i++)
        pac << header->banner[i];

      // BkHeader
      pac << bk_header->size << bk_header->magic << bk_header->ngid << bk_header->number_of_files
          << bk_header->size_of_files << bk_header->unk1 << bk_header->unk2
          << bk_header->total_size;
      for (size_t i = 0; i < bk_header->unk3.size(); i++)
        pac << bk_header->unk3[i];
      pac << sf::Uint64{bk_header->tid};
      for (size_t i = 0; i < bk_header->mac_address.size(); i++)
        pac << bk_header->mac_address[i];

      // Files
      for (const WiiSave::Storage::SaveFile& file : *files)
      {
        pac << file.mode << file.attributes << static_cast<u8>(file.type) << file.path;

        if (file.type == WiiSave::Storage::SaveFile::Type::File)
        {
          const std::optional<std::vector<u8>>& data = *file.data;
          if (!data || !CompressBufferIntoPacket(*data, pac))
            return false;
        }
      }
    }
    else
    {
      pac << false;  // save does not exist
    }

    SendAsyncToClients(std::move(pac));
  }

  return true;
}

bool NetPlayServer::CompressFileIntoPacket(const std::string& file_path, sf::Packet& packet)
{
  File::IOFile file(file_path, "rb");
  if (!file)
  {
    PanicAlertT("Failed to open file \"%s\".", file_path.c_str());
    return false;
  }

  const sf::Uint64 size = file.GetSize();
  packet << size;

  if (size == 0)
    return true;

  std::vector<u8> in_buffer(NETPLAY_LZO_IN_LEN);
  std::vector<u8> out_buffer(NETPLAY_LZO_OUT_LEN);
  std::vector<u8> wrkmem(LZO1X_1_MEM_COMPRESS);

  lzo_uint i = 0;
  while (true)
  {
    lzo_uint32 cur_len = 0;  // number of bytes to read
    lzo_uint out_len = 0;    // number of bytes to write

    if ((i + NETPLAY_LZO_IN_LEN) >= size)
    {
      cur_len = static_cast<lzo_uint32>(size - i);
    }
    else
    {
      cur_len = NETPLAY_LZO_IN_LEN;
    }

    if (cur_len <= 0)
      break;  // EOF

    if (!file.ReadBytes(in_buffer.data(), cur_len))
    {
      PanicAlertT("Error reading file: %s", file_path.c_str());
      return false;
    }

    if (lzo1x_1_compress(in_buffer.data(), cur_len, out_buffer.data(), &out_len, wrkmem.data()) !=
        LZO_E_OK)
    {
      PanicAlertT("Internal LZO Error - compression failed");
      return false;
    }

    // The size of the data to write is 'out_len'
    packet << static_cast<u32>(out_len);
    for (size_t j = 0; j < out_len; j++)
    {
      packet << out_buffer[j];
    }

    if (cur_len != NETPLAY_LZO_IN_LEN)
      break;

    i += cur_len;
  }

  // Mark end of data
  packet << static_cast<u32>(0);

  return true;
}

bool NetPlayServer::CompressBufferIntoPacket(const std::vector<u8>& in_buffer, sf::Packet& packet)
{
  const sf::Uint64 size = in_buffer.size();
  packet << size;

  if (size == 0)
    return true;

  std::vector<u8> out_buffer(NETPLAY_LZO_OUT_LEN);
  std::vector<u8> wrkmem(LZO1X_1_MEM_COMPRESS);

  lzo_uint i = 0;
  while (true)
  {
    lzo_uint32 cur_len = 0;  // number of bytes to read
    lzo_uint out_len = 0;    // number of bytes to write

    if ((i + NETPLAY_LZO_IN_LEN) >= size)
    {
      cur_len = static_cast<lzo_uint32>(size - i);
    }
    else
    {
      cur_len = NETPLAY_LZO_IN_LEN;
    }

    if (cur_len <= 0)
      break;  // end of buffer

    if (lzo1x_1_compress(&in_buffer[i], cur_len, out_buffer.data(), &out_len, wrkmem.data()) !=
        LZO_E_OK)
    {
      PanicAlertT("Internal LZO Error - compression failed");
      return false;
    }

    // The size of the data to write is 'out_len'
    packet << static_cast<u32>(out_len);
    for (size_t j = 0; j < out_len; j++)
    {
      packet << out_buffer[j];
    }

    if (cur_len != NETPLAY_LZO_IN_LEN)
      break;

    i += cur_len;
  }

  // Mark end of data
  packet << static_cast<u32>(0);

  return true;
}

void NetPlayServer::SendFirstReceivedToHost(const PadMapping map, const bool state)
{
  sf::Packet pac;
  pac << static_cast<MessageId>(NP_MSG_PAD_FIRST_RECEIVED);
  pac << map;
  pac << state;
  Send(m_players.at(1).socket, pac);
}

u64 NetPlayServer::GetInitialNetPlayRTC() const
{
  const auto& config = SConfig::GetInstance();

  if (config.bEnableCustomRTC)
    return config.m_customRTCValue;

  return Common::Timer::GetLocalTimeSinceJan1970();
}

// called from multiple threads
void NetPlayServer::SendToClients(const sf::Packet& packet, const PlayerId skip_pid)
{
  for (auto& p : m_players)
  {
    if (p.second.pid && p.second.pid != skip_pid)
    {
      Send(p.second.socket, packet);
    }
  }
}

void NetPlayServer::Send(ENetPeer* socket, const sf::Packet& packet)
{
  ENetPacket* epac =
      enet_packet_create(packet.getData(), packet.getDataSize(), ENET_PACKET_FLAG_RELIABLE);
  enet_peer_send(socket, 0, epac);
}

void NetPlayServer::KickPlayer(PlayerId player)
{
  for (auto& current_player : m_players)
  {
    if (current_player.second.pid == player)
    {
      enet_peer_disconnect(current_player.second.socket, 0);
      return;
    }
  }
}

u16 NetPlayServer::GetPort() const
{
  return m_server->address.port;
}

void NetPlayServer::SetNetPlayUI(NetPlayUI* dialog)
{
  m_dialog = dialog;
}

// called from ---GUI--- thread
std::unordered_set<std::string> NetPlayServer::GetInterfaceSet() const
{
  std::unordered_set<std::string> result;
  auto lst = GetInterfaceListInternal();
  for (auto list_entry : lst)
    result.emplace(list_entry.first);
  return result;
}

// called from ---GUI--- thread
std::string NetPlayServer::GetInterfaceHost(const std::string& inter) const
{
  char buf[16];
  sprintf(buf, ":%d", GetPort());
  auto lst = GetInterfaceListInternal();
  for (const auto& list_entry : lst)
  {
    if (list_entry.first == inter)
    {
      return list_entry.second + buf;
    }
  }
  return "?";
}

// called from ---GUI--- thread
std::vector<std::pair<std::string, std::string>> NetPlayServer::GetInterfaceListInternal() const
{
  std::vector<std::pair<std::string, std::string>> result;
#if defined(_WIN32)

#elif defined(ANDROID)
// Android has no getifaddrs for some stupid reason.  If this
// functionality ends up actually being used on Android, fix this.
#else
  ifaddrs* ifp = nullptr;
  char buf[512];
  if (getifaddrs(&ifp) != -1)
  {
    for (ifaddrs* curifp = ifp; curifp; curifp = curifp->ifa_next)
    {
      sockaddr* sa = curifp->ifa_addr;

      if (sa == nullptr)
        continue;
      if (sa->sa_family != AF_INET)
        continue;
      sockaddr_in* sai = (struct sockaddr_in*)sa;
      if (ntohl(((struct sockaddr_in*)sa)->sin_addr.s_addr) == 0x7f000001)
        continue;
      const char* ip = inet_ntop(sa->sa_family, &sai->sin_addr, buf, sizeof(buf));
      if (ip == nullptr)
        continue;
      result.emplace_back(std::make_pair(curifp->ifa_name, ip));
    }
    freeifaddrs(ifp);
  }
#endif
  if (result.empty())
    result.emplace_back(std::make_pair("!local!", "127.0.0.1"));
  return result;
}
}  // namespace NetPlay
