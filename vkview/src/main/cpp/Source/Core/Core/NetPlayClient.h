// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <SFML/Network/Packet.hpp>
#include <array>
#include <chrono>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/Event.h"
#include "Common/SPSCQueue.h"
#include "Common/TraversalClient.h"
#include "Core/NetPlayProto.h"
#include "InputCommon/GCPadStatus.h"

namespace UICommon
{
class GameFile;
}

namespace NetPlay
{
class NetPlayUI
{
public:
  virtual ~NetPlayUI() {}
  virtual void BootGame(const std::string& filename) = 0;
  virtual void StopGame() = 0;
  virtual bool IsHosting() const = 0;

  virtual void Update() = 0;
  virtual void AppendChat(const std::string& msg) = 0;

  virtual void OnMsgChangeGame(const std::string& filename) = 0;
  virtual void OnMsgStartGame() = 0;
  virtual void OnMsgStopGame() = 0;
  virtual void OnPadBufferChanged(u32 buffer) = 0;
  virtual void OnHostInputAuthorityChanged(bool enabled) = 0;
  virtual void OnDesync(u32 frame, const std::string& player) = 0;
  virtual void OnConnectionLost() = 0;
  virtual void OnConnectionError(const std::string& message) = 0;
  virtual void OnTraversalError(TraversalClient::FailureReason error) = 0;
  virtual void OnTraversalStateChanged(TraversalClient::State state) = 0;
  virtual void OnSaveDataSyncFailure() = 0;

  virtual bool IsRecording() = 0;
  virtual std::string FindGame(const std::string& game) = 0;
  virtual std::shared_ptr<const UICommon::GameFile> FindGameFile(const std::string& game) = 0;
  virtual void ShowMD5Dialog(const std::string& file_identifier) = 0;
  virtual void SetMD5Progress(int pid, int progress) = 0;
  virtual void SetMD5Result(int pid, const std::string& result) = 0;
  virtual void AbortMD5() = 0;
};

enum class PlayerGameStatus
{
  Unknown,
  Ok,
  NotFound
};

class Player
{
public:
  PlayerId pid;
  std::string name;
  std::string revision;
  u32 ping;
  PlayerGameStatus game_status;

  bool IsHost() const { return pid == 1; }
};

class NetPlayClient : public TraversalClientClient
{
public:
  void ThreadFunc();
  void SendAsync(sf::Packet&& packet);

  NetPlayClient(const std::string& address, const u16 port, NetPlayUI* dialog,
                const std::string& name, const NetTraversalConfig& traversal_config);
  ~NetPlayClient();

  void GetPlayerList(std::string& list, std::vector<int>& pid_list);
  std::vector<const Player*> GetPlayers();
  const NetSettings& GetNetSettings() const;

  // Called from the GUI thread.
  bool IsConnected() const { return m_is_connected; }
  bool StartGame(const std::string& path);
  bool StopGame();
  void Stop();
  bool ChangeGame(const std::string& game);
  void SendChatMessage(const std::string& msg);
  void RequestStopGame();

  // Send and receive pads values
  bool WiimoteUpdate(int _number, u8* data, const u8 size, u8 reporting_mode);
  bool GetNetPads(int pad_nb, bool from_vi, GCPadStatus* pad_status);

  u64 GetInitialRTCValue() const;

  void OnTraversalStateChanged() override;
  void OnConnectReady(ENetAddress addr) override;
  void OnConnectFailed(u8 reason) override;

  bool IsFirstInGamePad(int ingame_pad) const;
  int NumLocalPads() const;

  int InGamePadToLocalPad(int ingame_pad) const;
  int LocalPadToInGamePad(int localPad) const;

  static void SendTimeBase();
  bool DoAllPlayersHaveGame();

  const PadMappingArray& GetPadMapping() const;
  const PadMappingArray& GetWiimoteMapping() const;

  void AdjustPadBufferSize(unsigned int size);

protected:
  void ClearBuffers();

  struct
  {
    std::recursive_mutex game;
    // lock order
    std::recursive_mutex players;
    std::recursive_mutex async_queue_write;
  } m_crit;

  Common::SPSCQueue<sf::Packet, false> m_async_queue;

  std::array<Common::SPSCQueue<GCPadStatus>, 4> m_pad_buffer;
  std::array<Common::SPSCQueue<NetWiimote>, 4> m_wiimote_buffer;

  std::array<bool, 4> m_first_pad_status_received{};

  std::chrono::time_point<std::chrono::steady_clock> m_buffer_under_target_last;

  NetPlayUI* m_dialog = nullptr;

  ENetHost* m_client = nullptr;
  ENetPeer* m_server = nullptr;
  std::thread m_thread;

  std::string m_selected_game;
  Common::Flag m_is_running{false};
  Common::Flag m_do_loop{true};

  // In non-host input authority mode, this is how many packets each client should
  // try to keep in-flight to the other clients. In host input authority mode, this is how
  // many incoming input packets need to be queued up before the client starts
  // speeding up the game to drain the buffer.
  unsigned int m_target_buffer_size = 20;
  bool m_host_input_authority = false;

  Player* m_local_player = nullptr;

  u32 m_current_game = 0;

  PadMappingArray m_pad_map;
  PadMappingArray m_wiimote_map;

  bool m_is_recording = false;

private:
  enum class ConnectionState
  {
    WaitingForTraversalClientConnection,
    WaitingForTraversalClientConnectReady,
    Connecting,
    WaitingForHelloResponse,
    Connected,
    Failure
  };

  bool LocalPlayerHasControllerMapped() const;

  void SendStartGamePacket();
  void SendStopGamePacket();

  void SyncSaveDataResponse(bool success);
  bool DecompressPacketIntoFile(sf::Packet& packet, const std::string& file_path);
  std::optional<std::vector<u8>> DecompressPacketIntoBuffer(sf::Packet& packet);

  bool PollLocalPad(int local_pad, sf::Packet& packet);
  void SendPadHostPoll(PadMapping pad_num);

  void UpdateDevices();
  void AddPadStateToPacket(int in_game_pad, const GCPadStatus& np, sf::Packet& packet);
  void SendWiimoteState(int in_game_pad, const NetWiimote& nw);
  unsigned int OnData(sf::Packet& packet);
  void Send(const sf::Packet& packet);
  void Disconnect();
  bool Connect();
  void ComputeMD5(const std::string& file_identifier);
  void DisplayPlayersPing();
  u32 GetPlayersMaxPing() const;

  bool m_is_connected = false;
  ConnectionState m_connection_state = ConnectionState::Failure;

  PlayerId m_pid = 0;
  NetSettings m_net_settings{};
  std::map<PlayerId, Player> m_players;
  std::string m_host_spec;
  std::string m_player_name;
  bool m_connecting = false;
  TraversalClient* m_traversal_client = nullptr;
  std::thread m_MD5_thread;
  bool m_should_compute_MD5 = false;
  Common::Event m_gc_pad_event;
  Common::Event m_wii_pad_event;
  Common::Event m_first_pad_status_received_event;
  u8 m_sync_save_data_count = 0;
  u8 m_sync_save_data_success_count = 0;

  u64 m_initial_rtc = 0;
  u32 m_timebase_frame = 0;
};

void NetPlay_Enable(NetPlayClient* const np);
void NetPlay_Disable();
}  // namespace NetPlay
