#include "emscripten_audio.h"
#include <iostream>

extern "C" {

EMSCRIPTEN_KEEPALIVE void audio_worklet_unpause_return(void *callback_data) {
  /// Return helper to call C++ member function from a js callback
  auto &parent{*static_cast<emscripten_audio*>(callback_data)};
  parent.audio_worklet_unpause();
}

}

emscripten_audio::emscripten_audio(callback_types &&initial_callbacks)
  : callbacks{std::move(initial_callbacks)} {
  /// Initialise an Emscripten audio worklet with the given callbacks
  sample_rate = static_cast<unsigned int>(EM_ASM_DOUBLE({
    var AudioContext = window.AudioContext || window.webkitAudioContext;
    var ctx = new AudioContext();
    var sr = ctx.sampleRate;
    ctx.close();
    return sr;
  }));

  EmscriptenWebAudioCreateAttributes create_audio_context_options{
    .latencyHint{"interactive"},                                                // one of "balanced", "interactive" or "playback"
    // TODO: enum name
    .sampleRate{sample_rate},                                                   // 44100 or 48000
  };
  EMSCRIPTEN_WEBAUDIO_T emscripten_audio_context{emscripten_create_audio_context(&create_audio_context_options)};

  emscripten_start_wasm_audio_worklet_thread_async(
    emscripten_audio_context,
    audio_thread_stack.data(),
    audio_thread_stack.size(),
    [](EMSCRIPTEN_WEBAUDIO_T audio_context, bool success, void *user_data) {    // EmscriptenStartWebAudioWorkletCallback
      /// Callback that runs when audio creation is complete (either success or fail)
      auto &parent{*static_cast<emscripten_audio*>(user_data)};
      if(!success) {
        std::cerr << "ERROR: Audio worklet start failed for context " << audio_context << std::endl;
        return;
      }
      parent.context = audio_context;

      WebAudioWorkletProcessorCreateOptions worklet_create_options{
        .name{parent.worklet_name.c_str()},
        .numAudioParams{0},
        .audioParamDescriptors{nullptr},
      };
      emscripten_create_wasm_audio_worklet_processor_async(
        audio_context,
        &worklet_create_options,
        [](EMSCRIPTEN_WEBAUDIO_T audio_context, bool success, void *user_data) { // EmscriptenWorkletProcessorCreatedCallback
          /// Callback that runs when the worklet processor creation has completed (either success or fail)
          auto &parent{*static_cast<emscripten_audio*>(user_data)};
          if(!success) {
            std::cerr << "ERROR: Audio worklet processor creation failed for context " << audio_context << std::endl;
            return;
          }

          std::array<int, 1> output_channel_counts{2};                          // 1 = mono, 2 = stereo
          // TODO: enum, configurable

          EmscriptenAudioWorkletNodeCreateOptions worklet_node_create_options{
            .numberOfInputs{0},
            .numberOfOutputs{output_channel_counts.size()},
            .outputChannelCounts{output_channel_counts.data()},
          };
          EMSCRIPTEN_AUDIO_WORKLET_NODE_T audio_worklet{emscripten_create_wasm_audio_worklet_node( // create node
            audio_context,
            parent.worklet_name.c_str(),                                        // must match the name set in WebAudioWorkletProcessorCreateOptions
            &worklet_node_create_options,
            [](int num_inputs,  AudioSampleFrame const *inputs,                 // EmscriptenWorkletNodeProcessCallback
               int num_outputs, AudioSampleFrame *outputs,
               int num_params,  AudioParamFrame const *params,
               void *user_data) {
              /// Audio processing callback dispatcher
              auto &parent{*static_cast<emscripten_audio*>(user_data)};
              if(parent.callbacks.input ) parent.callbacks.input( {inputs,  static_cast<size_t>(num_inputs )});
              if(parent.callbacks.output) parent.callbacks.output({outputs, static_cast<size_t>(num_outputs)});
              if(parent.callbacks.params) parent.callbacks.params({params,  static_cast<size_t>(num_params )});
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
}

void emscripten_audio::audio_worklet_unpause() {
  /// Unpause the audio after the first user click on the canvas
  state = emscripten_audio_context_state(context);
  if(state == AUDIO_CONTEXT_STATE_RUNNING) return;                              // AUDIO_CONTEXT_STATE_SUSPENDED=0, AUDIO_CONTEXT_STATE_RUNNING=1, AUDIO_CONTEXT_STATE_CLOSED=2. AUDIO_CONTEXT_STATE_INTERRUPTED=3

  emscripten_resume_audio_context_async(
    context,
    [](EMSCRIPTEN_WEBAUDIO_T /*audio_context*/, AUDIO_CONTEXT_STATE state, void *user_data){ // EmscriptenResumeAudioContextCallback
      auto &parent{*static_cast<emscripten_audio*>(user_data)};
      parent.state = state;
      if(parent.callbacks.playback_started) parent.callbacks.playback_started();
    },
    this
  );
}
