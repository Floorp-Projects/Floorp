# Audio Mixer

[![CircleCI](https://circleci.com/gh/mozilla/audio-mixer.svg?style=svg)](https://circleci.com/gh/mozilla/audio-mixer)
[![Build & Test](https://github.com/mozilla/audio-mixer/actions/workflows/test.yml/badge.svg)](https://github.com/mozilla/audio-mixer/actions/workflows/test.yml)

Mixing audio data from any input channel layout to any output channel layout,
in a *matrix-multiplication* form.

```
output channel #1 ▸ │ Silence    │   │ 0, 0, 0, 0 │   │ FrontRight   │ ◂ input channel #1
output channel #2 ▸ │ FrontRight │ = │ R, C, 0, F │ x │ FrontCenter  │ ◂ input channel #2
output channel #3 ▸ │ FrontLeft  │   │ 0, C, L, F │   │ FrontLeft    │ ◂ input channel #3
                          ▴                 ▴         │ LowFrequency │ ◂ input channel #4
                          ┊                 ┊                ▴
                          ┊                 ┊                ┊
                      out_audio      mixing matrix m       in_audio
```

For example, the above means there are 3 output channels and 4 input channels.
The order of output channels is _Silence, FrontRight, and FrontLeft_.
The order of input channels is  _FrontRight, FrontCenter, FrontLeft, LowFrequency_.

So the output data in the _channel #2_ will be:
```

Output data of ch #2 (FrontRight) =
    R x input channel #1 (FrontRight)   +
    C x input channel #2 (FrontCenter)  +
    0 x input channel #3 (FrontLeft)    +
    F x input channel #4 (LowFrequency)
```

where the _C, F, L, R_ are mixing coefficients.
The _Silence_ channel is a unused channel in the output device,
so its channel data will always be zero.

## License

MPL-2
