#include "mistats.h"

#include <string>
#include <mimalloc.h>
#include <imgui.h>

IgIgPageMistats &IgIgPageMistats::instance() {
  static IgIgPageMistats IGIG_PAGE_MISTATS;
  return IGIG_PAGE_MISTATS;
}

IgIgPageMistats::IgIgPageMistats() {}

static void mi_stats_print_to_string(const char *msg, void *arg) {
  std::string *str = (std::string *)arg;
  str->append(msg);
}

void IgIgPageMistats::draw() {
  auto now = std::chrono::steady_clock::now();
  if (now - lastUpdate > std::chrono::milliseconds(100)) {
    lastUpdate = now;
    
    buf.clear();
    mi_stats_print_out(mi_stats_print_to_string, &buf);
  }

  if (ImGui::BeginTabItem("Mistats")) {
    if (ImGui::BeginChild("ScrollingRegion2", ImVec2(0, 0),
                          ImGuiChildFlags_None,
                          ImGuiWindowFlags_HorizontalScrollbar)) {
      ImGui::TextUnformatted(buf.c_str());
    }
    ImGui::EndChild();
    ImGui::EndTabItem();
  }
}



