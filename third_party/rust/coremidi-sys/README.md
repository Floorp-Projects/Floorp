# coremidi-sys

Low level Rust bindings for CoreMIDI

`generated.rs` is generated with [bindgen](https://github.com/rust-lang/rust-bindgen) 0.53.2 using the following command:
```
bindgen /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk/System/Library/Frameworks/CoreMIDI.framework/Headers/MIDIServices.h --whitelist-type "MIDI.*" --whitelist-function "MIDI.*"  --whitelist-var "kMIDI.*" --no-doc-comments --constified-enum ".*" --no-copy "MIDIPacket.*" --blacklist-type "(__)?CF.*" > src/generated.rs
```

As of version 3 the minimum required Rust version is 1.51 due to the use of std::ptr::addr_of.
