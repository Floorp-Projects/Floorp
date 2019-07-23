#![no_main]
#[macro_use] extern crate libfuzzer_sys;
extern crate webrtc_sdp;

fuzz_target!(|data: &[u8]| {
    if let Ok(s) = std::str::from_utf8(data) {
        let _ = webrtc_sdp::parse_sdp(s, true);
    }
});
