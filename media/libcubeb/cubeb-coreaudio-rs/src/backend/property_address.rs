use super::coreaudio_sys_utils::sys::*;

pub const DEFAULT_INPUT_DEVICE_PROPERTY_ADDRESS: AudioObjectPropertyAddress =
    AudioObjectPropertyAddress {
        mSelector: kAudioHardwarePropertyDefaultInputDevice,
        mScope: kAudioObjectPropertyScopeGlobal,
        mElement: kAudioObjectPropertyElementMaster,
    };

pub const DEFAULT_OUTPUT_DEVICE_PROPERTY_ADDRESS: AudioObjectPropertyAddress =
    AudioObjectPropertyAddress {
        mSelector: kAudioHardwarePropertyDefaultOutputDevice,
        mScope: kAudioObjectPropertyScopeGlobal,
        mElement: kAudioObjectPropertyElementMaster,
    };

pub const DEVICE_IS_ALIVE_PROPERTY_ADDRESS: AudioObjectPropertyAddress =
    AudioObjectPropertyAddress {
        mSelector: kAudioDevicePropertyDeviceIsAlive,
        mScope: kAudioObjectPropertyScopeGlobal,
        mElement: kAudioObjectPropertyElementMaster,
    };

pub const DEVICES_PROPERTY_ADDRESS: AudioObjectPropertyAddress = AudioObjectPropertyAddress {
    mSelector: kAudioHardwarePropertyDevices,
    mScope: kAudioObjectPropertyScopeGlobal,
    mElement: kAudioObjectPropertyElementMaster,
};

pub const INPUT_DATA_SOURCE_PROPERTY_ADDRESS: AudioObjectPropertyAddress =
    AudioObjectPropertyAddress {
        mSelector: kAudioDevicePropertyDataSource,
        mScope: kAudioDevicePropertyScopeInput,
        mElement: kAudioObjectPropertyElementMaster,
    };

pub const OUTPUT_DATA_SOURCE_PROPERTY_ADDRESS: AudioObjectPropertyAddress =
    AudioObjectPropertyAddress {
        mSelector: kAudioDevicePropertyDataSource,
        mScope: kAudioDevicePropertyScopeOutput,
        mElement: kAudioObjectPropertyElementMaster,
    };
