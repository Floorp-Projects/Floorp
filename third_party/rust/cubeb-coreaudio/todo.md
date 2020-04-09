# TO DO

## General

- Some of bugs are found when adding tests. Search *FIXIT* to find them.
- Remove `#[allow(non_camel_case_types)]`, `#![allow(unused_assignments)]`, `#![allow(unused_must_use)]`
- Use `ErrorChain`
- Centralize the error log in one place
- Support `enumerate_devices` with in-out type?
- Monitor `kAudioDevicePropertyDeviceIsAlive` for output device.
- Create a wrapper for `CFArrayCreateMutable` like what we do for `CFMutableDictionaryRef`
- Create a wrapper for property listener’s callback
- Use `Option<AggregateDevice>` rather than `AggregateDevice` for `aggregate_device` in `CoreStreamData`

### Type of stream

- Use `Option<device_info>` rather than `device_info` for `{input, output}_device` in `CoreStreamData`
- Use `Option<StreamParams>` rather than `StreamParams` for `{input, output}_stream_params` in `CoreStreamData`
- Same as `{input, output}_desc`, `{input, output}_hw_rate`, ...etc
- It would much clearer if we have a `struct X` to wrap all the above stuff and use `input_x` and `output_x` in `CoreStreamData`

### Generics

## Separate the stream implementation from the interface

The goal is to separate the audio stream into two parts(modules).
One is _inner_, the other is _outer_.

- The _outer_ stream implements the cubeb interface, based on the _inner_ stream.
- The _inner_ stream implements the stream operations based on the _CoreAudio_ APIs.
Now the _outer_ stream is named `AudioUnitStream`, the _inner_ stream is named `CoreStreamData`.

The problem now is that we don't have a clear boundry of the data ownership
between the _outer_ stream and _inner_ stream. They access the data owned by the other.

- `audiounit_property_listener_callback` is tied to _outer_ stream
but the event listeners are in _inner_ stream
- `audiounit_input_callback`, `audiounit_output_callback` are registered by the _inner_ stream
but the main logic are tied to _outer_ stream

### Callback separation

- Create static callbacks in _inner_ stream
- Render _inner_ stream's callbacks to _outer_ stream's callbacks

### Reinitialization

If the _outer_ stream and the _inner_ stream are separate properly,
when we need to reinitialize the stream, we can just drop the _inner_ stream
and create a new one. It's easier than the current implementation.

## Aggregate device

### Usage policy

- [BMO 1563475][bmo1563475]: Only use _aggregate device_ when the mic is a input-only and the speaker is output-only device.
- Test if we should do drift compensation.
- Add a test for `should_use_aggregate_device`
  - Create a dummy stream and check
  - Check again after reinit
    - Input only: expect false
    - Output only: expect false
    - Duplex
    - Default input and output are different and they are mic-only and speaker-only: expect true
    - Otherwise: expect false

[bmo1563475]: https://bugzilla.mozilla.org/show_bug.cgi?id=1563475#c4

### Get sub devices

- A better pattern for `AggregateDevice::get_sub_devices`

### Set sub devices

- We will add overlapping devices between `input_sub_devices` and `output_sub_devices`.
  - if they are same device
  - if either one of them or both of them are aggregate devices

### Setting master device

- We always set the master device to the first subdevice of the default output device
  but the output device (forming the aggregate device) may not be the default output device
- Check if the first subdevice of the default output device is in the list of
  sub devices list of the aggregate device
- Check the `name: CFStringRef` of the master device is not `NULL`

### Mixer

- Don't force output device to mono or stereo when the output device has one or two channel
  - unless the output devicv is _Bose QC35, mark 1 and 2_.

## Interface to system types and APIs

- Check if we need `AudioDeviceID` and `AudioObjectID` at the same time
- Create wrapper for `AudioObjectGetPropertyData(Size)` with _qualifier_ info
- Create wrapper for `CF` related types
- Create wrapper struct for `AudioObjectId`
  - Add `get_data`, `get_data_size`, `set_data`
- Create wrapper struct for `AudioUnit`
  - Implement `get_data`, `set_data`
- Create wrapper for `audio_unit_{add, remove}_property_listener`, `audio_object_{add, remove}_property_listener` and their callbacks
  - Add/Remove listener with generic `*mut T` data, fire their callback with generic `*mut T` data

## [Cubeb Interface][cubeb-rs]

- Implement `From` trait for `enum cubeb_device_type` so we can use `devtype.into()` to get `ffi::CUBEB_DEVICE_TYPE_*`.
- Implement `to_owned` in [`StreamParamsRef`][cubeb-rs-stmparamsref]
- Check the passed parameters like what [cubeb.c][cubeb] does!
  - Check the input `StreamParams` parameters properly, or we will set a invalid format into `AudioUnit`.
  - For example, for a duplex stream, the format of the input stream and output stream should be same.
      Using different stream formats will cause memory corruption
      since our resampler assumes the types (_short_ or _float_) of input stream (buffer) and output stream (buffer) are same
      (The resampler will use the format of the input stream if it exists, otherwise it uses the format of the output stream).
  - In fact, we should check **all** the parameters properly so we can make sure we don't mess up the streams/devices settings!

[cubeb-rs]: https://github.com/djg/cubeb-rs "cubeb-rs"
[cubeb-rs-stmparamsref]: https://github.com/djg/cubeb-rs/blob/78ed9459b8ac2ca50ea37bb72f8a06847eb8d379/cubeb-core/src/stream.rs#L61 "StreamParamsRef"

## Test

- Rewrite some tests under _cubeb/test/*_ in _Rust_ as part of the integration tests
  - Add tests for capturing/recording, output, duplex streams
- Update the manual tests
  - Those tests are created in the middle of the development. Thay might be not outdated now.
