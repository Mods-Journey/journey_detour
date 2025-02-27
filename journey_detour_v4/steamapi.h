#pragma once

#include "game.h"

#include <regex>
#include <winrt/base.h>

class RemoteUserInfo
{
public:
  char personaName[24];
  CSteamID steamID;
  RemoteUserInfo(std::string personaname, CSteamID steamid);
};

class MemberData 
{
public:
  MemberData(CSteamID memberid);
  CSteamID memberId;
  std::string memberName;

};

class LobbyData 
{
public:
  LobbyData(CSteamID lobbyid);
  CSteamID lobbyId;
  int curPlayers;
  int maxPlayers;
  int level;
};

class CLobbyListManager {
public:
  CCallResult<CLobbyListManager, LobbyMatchList_t> m_CallResultLobbyMatchList;
  void FindLobbies();
  void JoinLobby(CSteamID lobby);
  void OnLobbyMatchList(LobbyMatchList_t *pLobbyMatchList, bool bIOFailure);

  static CLobbyListManager &instance();

  std::vector<LobbyData> lobbyList;
};

class CLobbyMemberManager {
public:
  void UpdateMembers();
  void OnLobbyChange(CSteamID lobby);

  static CLobbyMemberManager &instance();
  CSteamID curLobby;
  std::vector<MemberData> memberList;
};


SIGSCAN_FUNC(ConnectionBarn__DialOut,
             "4C 89 44 24 ?? 48 89 54 24 ?? 55 53 56 57 41 54 41 55 41 56 41 "
             "57 48 8D AC 24",
             __fastcall, void *, uintptr_t connectionbarn, void *outptr,
             RemoteUserInfo somestruct);
