#pragma once
#include <array>
#include <functional>
#include <span>
#include <string>
#include <emscripten/webaudio.h>

extern "C" {
EMSCRIPTEN_KEEPALIVE void audio_worklet_unpause_return(void *callback_data);
}

class emscripten_audio {
public:
  struct callback_types {
    std::function<void()> playback_started{};
    std::function<void(std::span<AudioSampleFrame const>)> input{};
    std::function<void(std::span<AudioSampleFrame      >)> output{};
    std::function<void(std::span<AudioParamFrame const >)> params{};
  };

  struct construction_options {
    callback_types callbacks{};
  };

private:
  struct alignas(16) {
    std::array<uint8_t, 4096> audio_thread_stack;
  };

  EMSCRIPTEN_WEBAUDIO_T context{};
  std::string worklet_name{"emscripten-audio-worklet"};
  unsigned int sample_rate{0};
  AUDIO_CONTEXT_STATE state{AUDIO_CONTEXT_STATE_SUSPENDED};
  // TODO: enum, getter

public:
  callback_types callbacks;

  emscripten_audio(construction_options &&options = {.callbacks{.playback_started{}, .input{}, .output{}, .params{}}});

private:
  void audio_worklet_unpause();
  friend void audio_worklet_unpause_return(void *callback_data);
};
