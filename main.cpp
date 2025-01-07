#include <iostream>
#include <imgui/imgui_impl_wgpu.h>
#include "logstorm/logstorm.h"
#include "gui/gui_renderer.h"
#include "render/webgpu_renderer.h"
#include "emscripten_audio.h"

class game_manager {
  struct audio_generator {
    bool started{false};
    float sample_rate{0.0f};                                                    // set by set_sample_rate
    float target_tone_frequency{440.0f};
    float target_volume{0.3f};

    float phase{0.0f};
    float phase_increment{0};                                                   // set by set_sample_rate
    float current_volume{0.0};

    void set_sample_rate(unsigned int sample_rate);

    void output(std::span<AudioSampleFrame> outputs);
  };

  logstorm::manager logger{logstorm::manager::build_with_sink<logstorm::sink::emscripten_out>()}; // logging system
  render::webgpu_renderer renderer{logger};                                     // WebGPU rendering system
  gui::gui_renderer gui{logger};                                                // GUI top level
  emscripten_audio audio{{
    .callbacks{
      .playback_started{[&]{
        on_playback_started();
      }},
    },
  }};
  audio_generator tone_generator;

public:
  game_manager();
  ~game_manager();

  void loop_main();

private:
  game_manager(game_manager const&) = delete;
  void operator=(game_manager const&) = delete;

  void on_playback_started();
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
}

game_manager::~game_manager() {
  /// Destructor for debugging purposes only
  assert(false && "Game manager destructing, this should never happen!");
}

void game_manager::loop_main() {
  /// Main pseudo-loop
  gui.draw(
    tone_generator.started,
    tone_generator.sample_rate,
    tone_generator.target_tone_frequency,
    tone_generator.target_volume,
    tone_generator.phase,
    tone_generator.phase_increment,
    tone_generator.current_volume
  );
  renderer.draw();
}

void game_manager::on_playback_started() {
  /// Playback started callback
  logger << "Audio: Starting playback after first user interaction";
  tone_generator.set_sample_rate(audio.get_sample_rate());
  audio.callbacks.processing = [&](std::span<AudioSampleFrame const> /*inputs*/,
                                   std::span<AudioSampleFrame> outputs,
                                   std::span<AudioParamFrame const > /*params*/){
    tone_generator.output(outputs);
  };
  tone_generator.started = true;
}

void game_manager::audio_generator::set_sample_rate(unsigned int new_sample_rate) {
  sample_rate = static_cast<float>(new_sample_rate);
  phase_increment = target_tone_frequency * 2.0f * boost::math::constants::pi<float>() / sample_rate;
}

void game_manager::audio_generator::output(std::span<AudioSampleFrame> outputs) {
  // interpolate towards the target frequency and volume values
  float const target_phase_increment{target_tone_frequency * 2.0f * boost::math::constants::pi<float>() / sample_rate};
  phase_increment = phase_increment * 0.95f + 0.05f * target_phase_increment;
  current_volume = current_volume * 0.95f + 0.05f * target_volume;

  // produce a sine wave tone of desired frequency to all output channels
  for(auto const &output : outputs) {
    for(int i{0}; i != output.samplesPerChannel; ++i) {
      float const sample{static_cast<float>(std::sin(phase)) * current_volume};
      phase += phase_increment;
      for(int channel{0}; channel != output.numberOfChannels; ++channel) {
        output.data[channel * output.samplesPerChannel + i] = sample;
      }
    }
  }

  // range reduce to keep precision around zero
  phase = std::fmod(phase, 2.0f * boost::math::constants::pi<float>());
}

auto main()->int {
  try {
    new game_manager;

  } catch (std::exception const &e) {
    std::cerr << "Exception: " << e.what() << std::endl;
    EM_ASM(alert("Error: Press F12 to see console for details."));
  }

  return EXIT_SUCCESS;
}
