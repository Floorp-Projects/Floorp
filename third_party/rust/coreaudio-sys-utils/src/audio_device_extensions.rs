use coreaudio_sys::*;

// See https://opensource.apple.com/source/WebCore/WebCore-7604.5.6/platform/spi/cf/CoreAudioSPI.h.auto.html
// Per https://github.com/WebKit/WebKit/commit/7c4c851bc80f14b4cf907f76d65baee013a45eea,
// this first appeared in MacOS 10.13 and iOS 11.0.
extern "C" {
    fn AudioDeviceDuck(
        inDevice: AudioDeviceID,
        inDuckedLevel: f32,
        inStartTime: *const AudioTimeStamp,
        inRampDuration: f32,
    ) -> OSStatus;
}

pub fn audio_device_duck(
    in_device: AudioDeviceID,
    in_ducked_level: f32,
    in_start_time: *const AudioTimeStamp,
    in_ramp_duration: f32,
) -> OSStatus {
    unsafe { AudioDeviceDuck(in_device, in_ducked_level, in_start_time, in_ramp_duration) }
}
