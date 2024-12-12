#include <iostream>
#include <functional>
#include <map>
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#include <emscripten/webaudio.h>
#include <imgui/imgui_impl_wgpu.h>
#include "logstorm/logstorm.h"
#include "gui/gui_renderer.h"
#include "render/webgpu_renderer.h"

class game_manager {
  logstorm::manager logger{logstorm::manager::build_with_sink<logstorm::sink::console>()}; // logging system
  render::webgpu_renderer renderer{logger};                                     // WebGPU rendering system
  gui::gui_renderer gui{logger};                                                // GUI top level

  struct alignas(16) {
    std::array<uint8_t, 4096> audio_thread_stack;
  };

  EMSCRIPTEN_WEBAUDIO_T audio_context{};
  std::string audio_worklet_name{"Test Audio Worklet"};
  unsigned int sample_rate{0};
  AUDIO_CONTEXT_STATE audio_state{AUDIO_CONTEXT_STATE_SUSPENDED};

  void loop_main();

public:
  game_manager();
  ~game_manager();

private:
  game_manager(game_manager const&) = delete;
  void operator=(game_manager const&) = delete;

public:
  void audio_worklet_unpause();
};


namespace {

extern "C" {

EMSCRIPTEN_KEEPALIVE void audio_worklet_unpause_return(void *callback_data) {
  auto &parent{*static_cast<game_manager*>(callback_data)};
  parent.audio_worklet_unpause();
}

}

} // anonymous namespace

game_manager::game_manager() {
  /// Run the game
  sample_rate = static_cast<unsigned int>(EM_ASM_DOUBLE({
    var AudioContext = window.AudioContext || window.webkitAudioContext;
    var ctx = new AudioContext();
    var sr = ctx.sampleRate;
    ctx.close();
    return sr;
  }));
  logger << "DEBUG: sample_rate " << sample_rate;

  EmscriptenWebAudioCreateAttributes create_audio_context_options{
    .latencyHint{"interactive"},                                                // one of "balanced", "interactive" or "playback"
    // TODO: enum name
    .sampleRate{sample_rate},                                                   // 44100 or 48000
  };
  EMSCRIPTEN_WEBAUDIO_T emscripten_audio_context{emscripten_create_audio_context(&create_audio_context_options)};
  logger << "DEBUG: Audio context: emscripten_audio_context " << emscripten_audio_context;

  emscripten_start_wasm_audio_worklet_thread_async(
    emscripten_audio_context,
    audio_thread_stack.data(),
    audio_thread_stack.size(),
    [](EMSCRIPTEN_WEBAUDIO_T audio_context, bool success, void *user_data) {    // EmscriptenStartWebAudioWorkletCallback
      /// Callback that runs when audio creation is complete (either success or fail)
      auto &parent{*static_cast<game_manager*>(user_data)};
      if(!success) {
        parent.logger << "ERROR: Audio worklet start failed for context " << audio_context;
        return;
      }
      parent.audio_context = audio_context;

      WebAudioWorkletProcessorCreateOptions worklet_create_options{
        .name{parent.audio_worklet_name.c_str()},
        .numAudioParams{0},
        .audioParamDescriptors{nullptr},
      };
      emscripten_create_wasm_audio_worklet_processor_async(
        audio_context,
        &worklet_create_options,
        [](EMSCRIPTEN_WEBAUDIO_T audio_context, bool success, void *user_data) { // EmscriptenWorkletProcessorCreatedCallback
          /// Callback that runs when the worklet processor creation has completed (either success or fail)
          auto &parent{*static_cast<game_manager*>(user_data)};
          if(!success) {
            parent.logger << "ERROR: Audio worklet processor creation failed for context " << audio_context;
            return;
          }

          std::array<int, 1> output_channel_counts{1};                          // 1 = mono, 2 = stereo
          // TODO: enum

          EmscriptenAudioWorkletNodeCreateOptions worklet_node_create_options{
            .numberOfInputs{0},
            .numberOfOutputs{output_channel_counts.size()},
            .outputChannelCounts{output_channel_counts.data()},
          };
          EMSCRIPTEN_AUDIO_WORKLET_NODE_T audio_worklet{emscripten_create_wasm_audio_worklet_node( // create node
            audio_context,
            parent.audio_worklet_name.c_str(),                                  // must match the name set in WebAudioWorkletProcessorCreateOptions
            &worklet_node_create_options,
            [](int /*num_inputs*/, AudioSampleFrame const */*inputs*/,          // EmscriptenWorkletNodeProcessCallback
               int num_outputs,    AudioSampleFrame *outputs,
               int /*num_params*/, AudioParamFrame const */*params*/,
               void *user_data) {
              /// Audio generation callback
              auto &parent{*static_cast<game_manager*>(user_data)};

              for(int i{0}; i != num_outputs; ++i) {
                auto const &output{outputs[i]};
                for(int j{0}; j != output.samplesPerChannel * output.numberOfChannels; ++j) {
                  output.data[j] = emscripten_random() * 0.2 - 0.1;             // scale down audio volume by factor of 0.2, raw noise can be really loud otherwise
                }
              }

              return true;                                                      // keep the graph output going
            },
            &parent
          )};

          emscripten_audio_node_connect(audio_worklet, audio_context, 0, 0);    // connect the node to an audio destination.  EMSCRIPTEN_WEBAUDIO_T source, EMSCRIPTEN_WEBAUDIO_T destination, int outputIndex, int inputIndex

          // register a once-only click handler to unpause audio, on the first click on the canvas
          EM_ASM({
            canvas.addEventListener("click", (event) => {
              Module["ccall"]('audio_worklet_unpause_return', null, ['number'], [$0]);
            }, {once : true});
          }, &parent);
        },
        &parent
      );
    },
    this
  );

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

void game_manager::audio_worklet_unpause() {
  /// Unpause the audio after the first user click on the canvas
  logger << "Audio: Resuming playback after first user interaction";
  if(emscripten_audio_context_state(audio_context) == AUDIO_CONTEXT_STATE_RUNNING) return; // AUDIO_CONTEXT_STATE_SUSPENDED=0, AUDIO_CONTEXT_STATE_RUNNING=1, AUDIO_CONTEXT_STATE_CLOSED=2. AUDIO_CONTEXT_STATE_INTERRUPTED=3
  //emscripten_resume_audio_context_sync(audio_context);
  emscripten_resume_audio_context_async(
    audio_context,
    [](EMSCRIPTEN_WEBAUDIO_T audio_context, AUDIO_CONTEXT_STATE state, void *user_data){ // EmscriptenResumeAudioContextCallback
      auto &parent{*static_cast<game_manager*>(user_data)};
      parent.logger << "DEBUG: Audio: Playback state " << state;
      parent.audio_state = state;
    },
    this
  );
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
