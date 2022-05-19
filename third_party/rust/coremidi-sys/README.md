# coremidi-sys

Low level Rust bindings for CoreMIDI

`generated.rs` is generated with [bindgen](https://github.com/rust-lang/rust-bindgen) 0.59.2 using the following commands:

```
export FRAMEWORKS_DIR=$(xcrun --sdk macosx --show-sdk-path)/System/Library/Frameworks

bindgen ${FRAMEWORKS_DIR}/CoreMIDI.framework/Headers/MIDIServices.h \
    --whitelist-type "MIDI.*" --whitelist-function "MIDI.*"  --whitelist-var "kMIDI.*" \
    --blacklist-type "(__)?CF.*" \
    --constified-enum ".*" --no-prepend-enum-name \
    --no-debug "MIDI(Event)?Packet.*" \
    --no-copy "MIDI(Event)?Packet.*" \
    --no-doc-comments \
    -- -F ${FRAMEWORKS_DIR} > src/generated.rs
```

As of version 3 the minimum required Rust version is 1.51 due to the use of `std::ptr::addr_of`.
