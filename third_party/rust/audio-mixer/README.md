# Audio Mixer

[![Build Status](https://travis-ci.com/ChunMinChang/audio-mixer.svg?branch=master)](https://travis-ci.com/ChunMinChang/audio-mixer)

Mixing audio data from any input channel layout to any output channel layout.

The code here is a refactored version from [cubeb][cubeb]'s [cubeb_mixer(_C/C++_)][cubeb_mixer], which adapts the code from [FFmpeg libswresample's rematrix.c][rematrix] .
The original implementation is in [cubeb-coreaudio-rs][cubeb-coreaudio-rs]'s [PR23][pr23]

## License

MPL-2

[cubeb]: https://github.com/kinetiknz/cubeb
[cubeb_mixer]: https://github.com/kinetiknz/cubeb/blob/aa636017aca8f79ebc95fb9f03b9333eaf9fd8fc/src/cubeb_mixer.cpp
[rematrix]: https://github.com/FFmpeg/FFmpeg/blob/4fa2d5a692f40c398a299acf2c6a20f5b98a3708/libswresample/rematrix.c
[cubeb-coreaudio-rs]: https://github.com/ChunMinChang/cubeb-coreaudio-rs/
[pr23]: https://github.com/ChunMinChang/cubeb-coreaudio-rs/pull/23