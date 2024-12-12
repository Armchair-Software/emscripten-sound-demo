#include "gui_renderer.h"
#include <emscripten/html5.h>
#include <imgui/imgui.h>
#include <imgui/imgui_impl_emscripten.h>
#include <imgui/imgui_impl_wgpu.h>
#include "logstorm/logstorm.h"

namespace gui {

gui_renderer::gui_renderer(logstorm::manager &this_logger)
  :logger{this_logger} {
  /// Construct the top level GUI and initialise ImGUI
  logger << "GUI: Initialising";
  #ifndef NDEBUG
    IMGUI_CHECKVERSION();
  #endif // NDEBUG
  ImGui::CreateContext();
  auto &imgui_io{ImGui::GetIO()};

  imgui_io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  imgui_io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
}

void gui_renderer::init(ImGui_ImplWGPU_InitInfo &imgui_wgpu_info) {
  /// Any additional initialisation that needs to occur after WebGPU has been initialised
  ImGui_ImplWGPU_Init(&imgui_wgpu_info);
  ImGui_ImplEmscripten_Init();

  clipboard.set_imgui_callbacks();
}

void gui_renderer::draw(bool started, float sample_rate, float &target_tone_frequency, float &target_volume, float phase, float phase_increment, float current_volume) const {
  /// Render the top level GUI
  ImGui_ImplWGPU_NewFrame();
  ImGui_ImplEmscripten_NewFrame();
  ImGui::NewFrame();

  if(!ImGui::Begin("Parameters")) {
    ImGui::End();
    return;
  }
  ImGui::SetWindowSize({550, 180});

  if(started) {
    ImGui::BeginDisabled();
    ImGui::InputFloat("Sample rate", &sample_rate, 0.0f, 0.0f, "%.0f", ImGuiInputTextFlags_ReadOnly);
    ImGui::EndDisabled();
    ImGui::DragFloat("Target tone frequency", &target_tone_frequency, 2.0f, 0.0f, 96'000.0f, "%.0f");
    ImGui::SliderFloat("Target volume", &target_volume, 0.0f, 1.0f);
    ImGui::BeginDisabled();
    ImGui::SliderFloat("Current volume", &current_volume, 0.0f, 1.0f);
    ImGui::InputFloat("Phase", &phase, 0.0f, 0.0f, "%.3f", ImGuiInputTextFlags_ReadOnly);
    ImGui::InputFloat("Phase increment", &phase_increment, 0.0f, 0.0f, "%.3f", ImGuiInputTextFlags_ReadOnly);
    ImGui::EndDisabled();
  } else {
    ImGui::TextUnformatted("Autoplay disabled - click on the window to start sound generator.");
  }

  ImGui::End();

  ImGui::Render();                                                              // finalise draw data (actual rendering of draw data is done by the renderer later)
}

}
