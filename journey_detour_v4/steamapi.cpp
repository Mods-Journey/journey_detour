#include "steamapi.h"

#include "game.h"




SIGSCAN_FUNC(JoinLobbyFunc,
             "48 8B C4 55 53 57 48 8D 68 ?? 48 81 EC ?? ?? ?? ?? 48 8B 3D",
             __fastcall, void, uintptr_t pSteamJourney, CSteamID lobby)

SIGSCAN_HOOK(OnLobbyEnterCallback,
             "40 55 53 56 57 41 54 41 55 41 56 48 8D AC 24 ?? ?? ?? ?? 48 81 "
             "EC ?? ?? ?? ?? 45 33 F6",
             __fastcall, void, uintptr_t pSteamJourney, LobbyEnter_t *pLobbyMatchList,
             bool bIOFailure)
{
  CLobbyMemberManager::instance().OnLobbyChange(
      pLobbyMatchList->m_ulSteamIDLobby);
  return OnLobbyEnterCallback(pSteamJourney, pLobbyMatchList, bIOFailure);
}


SIGSCAN_HOOK(OnPlayerJoinEvent, "48 89 6C 24 ?? 56 48 83 EC ?? 80 79",
             __fastcall, void, uintptr_t a1, uintptr_t* a2) 
{
  uint8_t* v4 = (uint8_t *)a2[48];
  uint8 flag1 = v4[1057];
  uint8 flag2 = v4[1058];
  spdlog::info("Received OnPlayerJoin Event! Current state: {},{}",flag1,flag2 );
  v4[1057] = 1;
  v4[1058] = 1;
  return OnPlayerJoinEvent(a1, a2);
}


void CLobbyListManager::OnLobbyMatchList(LobbyMatchList_t *pLobbyMatchList,
                                       bool bIOFailure) {
  // lobby list has been retrieved from Steam back-end, use results
  if (bIOFailure) {
    spdlog::info("Failed to retrieve lobby list.");
    return;
  }

  int lobbyCount = pLobbyMatchList->m_nLobbiesMatching;
  //spdlog::info("Found {} lobbies.", lobbyCount);
  lobbyList.clear();
  for (int i = 0; i < lobbyCount; i++) {
    CSteamID lobbyID = SteamMatchmaking()->GetLobbyByIndex(i);
    lobbyList.push_back(LobbyData(lobbyID));
  }
  std::sort(lobbyList.begin(), lobbyList.end(),
            [](LobbyData& a, LobbyData& b) {  return a.level < b.level; });
}



void CLobbyListManager::FindLobbies() {
  SteamAPICall_t hSteamAPICall = SteamMatchmaking()->RequestLobbyList();
  m_CallResultLobbyMatchList.Set(hSteamAPICall, this,
                                 &CLobbyListManager::OnLobbyMatchList);
}

void CLobbyListManager::JoinLobby(CSteamID lobby) {
  Matchmaker_JoinRoomFromId(CJourneyMatchmaker::instance().base(), lobby);
  
}


CLobbyListManager &CLobbyListManager::instance() {
  static CLobbyListManager CLobbyListManager;
  return CLobbyListManager;
}

MemberData::MemberData(CSteamID memberid) 
{ 
  memberId = memberid;
  memberName = SteamFriends()->GetFriendPersonaName(memberid);
}



LobbyData::LobbyData(CSteamID lobbyid)
{
  lobbyId = lobbyid;
  char key[k_nMaxLobbyKeyLength];
  char value[k_cubChatMetadataMax];
  curPlayers = SteamMatchmaking()->GetNumLobbyMembers(lobbyid);
  maxPlayers = SteamMatchmaking()->GetLobbyMemberLimit(lobbyid);
  SteamMatchmaking()->GetLobbyDataByIndex(lobbyid, 0, key, sizeof(key), value,
                                          sizeof(value));
  level = std::stoi(value);
}



void CLobbyMemberManager::UpdateMembers() 
{
  if (curLobby.ConvertToUint64() == 0)
  {
    return;
  }
    
  int numLobbyMembers = SteamMatchmaking()->GetNumLobbyMembers(curLobby);
  memberList.clear();
  for (int i = 0; i < numLobbyMembers; i++) {
    memberList.push_back(
        MemberData(
        SteamMatchmaking()->GetLobbyMemberByIndex(curLobby, i)));
  }
  std::sort(memberList.begin(), memberList.end(),
      [](MemberData &a, MemberData &b) { return a.memberId.ConvertToUint64() < b.memberId.ConvertToUint64(); });
}

void CLobbyMemberManager::OnLobbyChange(CSteamID lobby) 
{ 
  if (lobby == curLobby) {
    spdlog::info("[CLobbyMember] Not calling OnLobbyChange cause we are "
                 "already checking the new lobby!");
  }
  spdlog::info("[CLobbyMember] OnLobbyChange: {}", lobby.ConvertToUint64());
  spdlog::info("[CLobbyMember] SteamJourneyLobby: {}", CSteamJourney::instance().GetCurrentLobby().ConvertToUint64());
  curLobby = lobby;
  UpdateMembers();
}

CLobbyMemberManager &CLobbyMemberManager::instance() {
  static CLobbyMemberManager CLobbyMemberManager;
  return CLobbyMemberManager;
}

RemoteUserInfo::RemoteUserInfo(std::string personaname, CSteamID steamid) 
{
  strncpy_s(personaName, personaname.c_str(), 24);
  personaName[23] = 0;
  steamID = steamid;

}
