#include "lobbybrowser.h"
#include "hud.h"
#include "game.h"
#include "igig.h"
#include "steamapi.h"

IgIgLobbyBrowser::IgIgLobbyBrowser() {}
IgIgLobbyBrowser &IgIgLobbyBrowser::instance() {
  static IgIgLobbyBrowser IgIgLobbyBrowser;
  return IgIgLobbyBrowser;
}

void IgIgLobbyBrowser::draw() {
  if (ImGui::BeginTabItem("LobbyBrowser")) {
    ImVec2 tabContentRegion = ImGui::GetContentRegionAvail();
    float tableHeight = tabContentRegion.y * 0.5f;
    // Using this as a base value to create width that is a factor of the size
    // of our font
    float textBaseWidth = ImGui::CalcTextSize("A").x;

    ImGui::Checkbox("Toggle ESP", &IgIgHud::instance().shouldDrawESP);

    if (std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - lastLobbyUpdate)
                .count() > 500) {
      CLobbyListManager::instance().FindLobbies();
      CLobbyMemberManager::instance().UpdateMembers();
      lastLobbyUpdate = std::chrono::steady_clock::now();
    }

    if (ImGui::BeginTable(
            "Lobbies", 4,
            ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersV |
                ImGuiTableFlags_BordersOuterH | ImGuiTableFlags_Resizable |
                ImGuiTableFlags_RowBg | ImGuiTableFlags_NoBordersInBody,
            ImVec2(0.0f, tableHeight))) {


      ImGui::TableSetupScrollFreeze(0, 1); // Make table headers always visible

      ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_NoHide);
      ImGui::TableSetupColumn("Level", ImGuiTableColumnFlags_WidthFixed,
                              textBaseWidth * 24.0f);
      ImGui::TableSetupColumn("CurPlayers", ImGuiTableColumnFlags_WidthFixed,
                              textBaseWidth * 24.0f);
      ImGui::TableSetupColumn("MaxPlayers", ImGuiTableColumnFlags_WidthFixed,
                              textBaseWidth * 24.0f);
      ImGui::TableHeadersRow();

      for (const LobbyData &lobby : CLobbyListManager::instance().lobbyList) {
        
        bool shouldHighlightRow =
            (CSteamJourney::instance().GetCurrentLobby().ConvertToUint64() ==
             lobby.lobbyId.ConvertToUint64());

        std::string lobbyIdDisplay = fmt::format(
            "{}", lobby.lobbyId.ConvertToUint64());
        ImGui::TableNextColumn();

        if (shouldHighlightRow) {
          ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
                                 IM_COL32(0, 128, 0, 255));
        }
        ImGui::TreeNodeEx(lobbyIdDisplay.c_str(),
                          ImGuiTreeNodeFlags_Leaf |
                                       ImGuiTreeNodeFlags_NoTreePushOnOpen |
                                       ImGuiTreeNodeFlags_SpanFullWidth |
                                       ImGuiTreeNodeFlags_SpanAllColumns);
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
          // do something on click
          CLobbyListManager::instance().JoinLobby(lobby.lobbyId);
        }

        ImGui::TableNextColumn();
        ImGui::Text("%d",lobby.level);
        ImGui::TableNextColumn();
        ImGui::Text("%d",lobby.curPlayers);
        ImGui::TableNextColumn();
        ImGui::Text("%d",lobby.maxPlayers);


      }

      ImGui::EndTable();
    }

    ImGui::Separator();

    if (ImGui::BeginTable(
            "Players", 4,
            ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersV |
                ImGuiTableFlags_BordersOuterH | ImGuiTableFlags_Resizable |
                ImGuiTableFlags_RowBg | ImGuiTableFlags_NoBordersInBody,
            ImVec2(0.0f, tableHeight))) {
      ImGui::TableSetupScrollFreeze(0, 1); // Make table headers always visible

      ImGui::TableSetupColumn("Username", ImGuiTableColumnFlags_NoHide);
      ImGui::TableSetupColumn("SteamID", ImGuiTableColumnFlags_WidthFixed,
                              textBaseWidth * 24.0f);
      ImGui::TableSetupColumn("nah", ImGuiTableColumnFlags_WidthFixed,
                              textBaseWidth * 24.0f);
      ImGui::TableSetupColumn("something", ImGuiTableColumnFlags_WidthFixed,
                              textBaseWidth * 24.0f);
      ImGui::TableHeadersRow();

      for (const MemberData &member : CLobbyMemberManager::instance().memberList) {
        ImGui::TableNextColumn();

        ImGui::TreeNodeEx(
            member.memberName.c_str(),
            ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen |
                ImGuiTreeNodeFlags_SpanFullWidth |
                ImGuiTreeNodeFlags_SpanAllColumns);
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
          // do something on click
          RemoteUserInfo userinfo1(member.memberName.c_str(), member.memberId);
          void *someptr;
          ConnectionBarn__DialOut(CJourneyMatchmaker::instance().base() + 1536,
                                  &someptr, userinfo1);

        }

        ImGui::TableNextColumn();
        ImGui::Text("%d", member.memberId.ConvertToUint64());
        ImGui::TableNextColumn();
        ImGui::Text("test");
        ImGui::TableNextColumn();
        ImGui::Text("test");
      }

      ImGui::EndTable();
    }


    ImGui::EndTabItem();
  }
}