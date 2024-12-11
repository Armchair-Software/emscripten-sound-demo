[![CI Build](https://github.com/Armchair-Software/emscripten-sound-demo/actions/workflows/ci-build.yml/badge.svg)](https://github.com/Armchair-Software/emscripten-sound-demo/actions)

# Sound Demo for the WebGPU Emscripten Armchair Engine

Description TODO

For other demos, see:
- https://github.com/Armchair-Software/webgpu-demo
- https://github.com/Armchair-Software/webgpu-demo2
- https://github.com/Armchair-Software/boids-webgpu-demo
- https://github.com/Armchair-Software/webgpu-shader-demo
- https://github.com/Armchair-Software/chatgpt-emscripten-demo

## Live demo
Live demo: https://armchair-software.github.io/emscripten-sound-demo/

This requires Firefox Nightly, or a recent version of Chrome or Chromium, with webgpu and Vulkan support explicitly enabled.

## Dependencies
- [Emscripten](https://emscripten.org/)
- CMake
- [VectorStorm](https://github.com/Armchair-Software/vectorstorm) (included)
- [LogStorm](https://github.com/VoxelStorm-Ltd/logstorm) (included)
- [Emscripten Browser Clipboard](https://github.com/Armchair-Software/emscripten-browser-clipboard) (included)
- [magic_enum](https://github.com/Neargye/magic_enum) (included)
- [dear imgui](https://github.com/ocornut/imgui) with the proposed `imgui_impl_emscripten` backend (included)

## Building
The easiest way to assemble everything (including in-tree shader resource assembly) is to use the included build script:
```sh
./build.sh
```

To launch a local server and bring up a browser:
```sh
./run.sh
```

For manual builds with CMake, and to adjust how the example is run locally, inspect the `build.sh` and `run.sh` scripts.
