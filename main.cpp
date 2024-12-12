#include <iostream>
#include <imgui/imgui_impl_wgpu.h>
#include "logstorm/logstorm.h"
#include "gui/gui_renderer.h"
#include "render/webgpu_renderer.h"
#include "emscripten_audio.h"

class game_manager {
  logstorm::manager logger{logstorm::manager::build_with_sink<logstorm::sink::console>()}; // logging system
  render::webgpu_renderer renderer{logger};                                     // WebGPU rendering system
  gui::gui_renderer gui{logger};                                                // GUI top level
  emscripten_audio audio{{
    .playback_started{[&]{
      on_playback_started();
    }},
    .output{[&](std::span<AudioSampleFrame> outputs){
      audio_output(outputs);
    }},
  }};

  void loop_main();

public:
  game_manager();
  ~game_manager();

private:
  game_manager(game_manager const&) = delete;
  void operator=(game_manager const&) = delete;

  void on_playback_started();
  void audio_output(std::span<AudioSampleFrame> outputs);
};

game_manager::game_manager() {
  /// Run the game
  renderer.init(
    [&](render::webgpu_renderer::webgpu_data const& webgpu){
      ImGui_ImplWGPU_InitInfo imgui_wgpu_info;
      imgui_wgpu_info.Device = webgpu.device.Get();
      imgui_wgpu_info.RenderTargetFormat = static_cast<WGPUTextureFormat>(webgpu.surface_preferred_format);
      imgui_wgpu_info.DepthStencilFormat = static_cast<WGPUTextureFormat>(webgpu.depth_texture_format);

      gui.init(imgui_wgpu_info);
    },
    [&]{
      loop_main();
    }
  );
  std::unreachable();
}

game_manager::~game_manager() {
  /// Destructor for debugging purposes only
  assert(false && "Game manager destructing, this should never happen!");
}

void game_manager::loop_main() {
  /// Main pseudo-loop
  gui.draw();
  renderer.draw();
}

void game_manager::on_playback_started() {
  /// Playback started callback
  logger << "Audio: Starting playback after first user interaction";
}

void game_manager::audio_output(std::span<AudioSampleFrame> outputs) {
  /// Audio generation function
  for(auto const &output : outputs) {
    for(int j{0}; j != output.samplesPerChannel * output.numberOfChannels; ++j) {
      output.data[j] = emscripten_random() * 0.2 - 0.1;                         // scale down audio volume by factor of 0.2, raw noise can be really loud otherwise
    }
  }
}

auto main()->int {
  try {
    game_manager game;
    std::unreachable();

  } catch (std::exception const &e) {
    std::cerr << "Exception: " << e.what() << std::endl;
    EM_ASM(alert("Error: Press F12 to see console for details."));
  }

  return EXIT_SUCCESS;
}
