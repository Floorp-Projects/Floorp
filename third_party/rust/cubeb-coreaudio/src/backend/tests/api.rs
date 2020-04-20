use super::utils::{
    test_audiounit_get_buffer_frame_size, test_audiounit_scope_is_enabled, test_create_audiounit,
    test_device_channels_in_scope, test_device_in_scope, test_get_all_devices,
    test_get_default_audiounit, test_get_default_device, test_get_default_raw_stream,
    test_get_default_source_name, test_get_devices_in_scope, test_get_raw_context,
    ComponentSubType, PropertyScope, Scope,
};
use super::*;

// make_sized_audio_channel_layout
// ------------------------------------
#[test]
fn test_make_sized_audio_channel_layout() {
    for channels in 1..10 {
        let size = mem::size_of::<AudioChannelLayout>()
            + (channels - 1) * mem::size_of::<AudioChannelDescription>();
        let _ = make_sized_audio_channel_layout(size);
    }
}

#[test]
#[should_panic]
fn test_make_sized_audio_channel_layout_with_wrong_size() {
    // let _ = make_sized_audio_channel_layout(0);
    let one_channel_size = mem::size_of::<AudioChannelLayout>();
    let padding_size = 10;
    assert_ne!(mem::size_of::<AudioChannelDescription>(), padding_size);
    let wrong_size = one_channel_size + padding_size;
    let _ = make_sized_audio_channel_layout(wrong_size);
}

// active_streams
// update_latency_by_adding_stream
// update_latency_by_removing_stream
// ------------------------------------
#[test]
fn test_increase_and_decrease_context_streams() {
    use std::thread;
    const STREAMS: u32 = 10;

    let context = AudioUnitContext::new();
    let context_ptr_value = &context as *const AudioUnitContext as usize;

    let mut join_handles = vec![];
    for i in 0..STREAMS {
        join_handles.push(thread::spawn(move || {
            let context = unsafe { &*(context_ptr_value as *const AudioUnitContext) };
            let global_latency = context.update_latency_by_adding_stream(i);
            global_latency
        }));
    }
    let mut latencies = vec![];
    for handle in join_handles {
        latencies.push(handle.join().unwrap());
    }
    assert_eq!(context.active_streams(), STREAMS);
    check_streams(&context, STREAMS);

    check_latency(&context, latencies[0]);
    for i in 0..latencies.len() - 1 {
        assert_eq!(latencies[i], latencies[i + 1]);
    }

    let mut join_handles = vec![];
    for _ in 0..STREAMS {
        join_handles.push(thread::spawn(move || {
            let context = unsafe { &*(context_ptr_value as *const AudioUnitContext) };
            context.update_latency_by_removing_stream();
        }));
    }
    for handle in join_handles {
        let _ = handle.join();
    }
    check_streams(&context, 0);

    check_latency(&context, None);
}

fn check_streams(context: &AudioUnitContext, number: u32) {
    let guard = context.latency_controller.lock().unwrap();
    assert_eq!(guard.streams, number);
}

fn check_latency(context: &AudioUnitContext, latency: Option<u32>) {
    let guard = context.latency_controller.lock().unwrap();
    assert_eq!(guard.latency, latency);
}

// make_silent
// ------------------------------------
#[test]
fn test_make_silent() {
    let mut array = allocate_array::<u32>(10);
    for data in array.iter_mut() {
        *data = 0xFFFF;
    }

    let mut buffer = AudioBuffer::default();
    buffer.mData = array.as_mut_ptr() as *mut c_void;
    buffer.mDataByteSize = (array.len() * mem::size_of::<u32>()) as u32;
    buffer.mNumberChannels = 1;

    audiounit_make_silent(&mut buffer);
    for data in array {
        assert_eq!(data, 0);
    }
}

// minimum_resampling_input_frames
// ------------------------------------
#[test]
fn test_minimum_resampling_input_frames() {
    let input_rate = 48000_f64;
    let output_rate = 44100_f64;

    let frames = 100;
    let times = input_rate / output_rate;
    let expected = (frames as f64 * times).ceil() as usize;

    assert_eq!(
        minimum_resampling_input_frames(input_rate, output_rate, frames),
        expected
    );
}

#[test]
#[should_panic]
fn test_minimum_resampling_input_frames_zero_input_rate() {
    minimum_resampling_input_frames(0_f64, 44100_f64, 1);
}

#[test]
#[should_panic]
fn test_minimum_resampling_input_frames_zero_output_rate() {
    minimum_resampling_input_frames(48000_f64, 0_f64, 1);
}

#[test]
fn test_minimum_resampling_input_frames_equal_input_output_rate() {
    let frames = 100;
    assert_eq!(
        minimum_resampling_input_frames(44100_f64, 44100_f64, frames),
        frames
    );
}

// create_device_info
// ------------------------------------
#[test]
fn test_create_device_info_from_unknown_input_device() {
    if let Some(default_device_id) = test_get_default_device(Scope::Input) {
        let default_device = create_device_info(kAudioObjectUnknown, DeviceType::INPUT).unwrap();
        assert_eq!(default_device.id, default_device_id);
        assert_eq!(
            default_device.flags,
            device_flags::DEV_INPUT
                | device_flags::DEV_SELECTED_DEFAULT
                | device_flags::DEV_SYSTEM_DEFAULT
        );
    } else {
        println!("No input device to perform test.");
    }
}

#[test]
fn test_create_device_info_from_unknown_output_device() {
    if let Some(default_device_id) = test_get_default_device(Scope::Output) {
        let default_device = create_device_info(kAudioObjectUnknown, DeviceType::OUTPUT).unwrap();
        assert_eq!(default_device.id, default_device_id);
        assert_eq!(
            default_device.flags,
            device_flags::DEV_OUTPUT
                | device_flags::DEV_SELECTED_DEFAULT
                | device_flags::DEV_SYSTEM_DEFAULT
        );
    } else {
        println!("No output device to perform test.");
    }
}

#[test]
#[should_panic]
fn test_set_device_info_to_system_input_device() {
    let _device = create_device_info(kAudioObjectSystemObject, DeviceType::INPUT);
}

#[test]
#[should_panic]
fn test_set_device_info_to_system_output_device() {
    let _device = create_device_info(kAudioObjectSystemObject, DeviceType::OUTPUT);
}

// FIXIT: Is it ok to set input device to a nonexistent device ?
#[ignore]
#[test]
#[should_panic]
fn test_set_device_info_to_nonexistent_input_device() {
    let nonexistent_id = std::u32::MAX;
    let _device = create_device_info(nonexistent_id, DeviceType::INPUT);
}

// FIXIT: Is it ok to set output device to a nonexistent device ?
#[ignore]
#[test]
#[should_panic]
fn test_set_device_info_to_nonexistent_output_device() {
    let nonexistent_id = std::u32::MAX;
    let _device = create_device_info(nonexistent_id, DeviceType::OUTPUT);
}

// add_listener (for default output device)
// ------------------------------------
#[test]
fn test_add_listener_unknown_device() {
    extern "C" fn callback(
        _id: AudioObjectID,
        _number_of_addresses: u32,
        _addresses: *const AudioObjectPropertyAddress,
        _data: *mut c_void,
    ) -> OSStatus {
        assert!(false, "Should not be called.");
        kAudioHardwareUnspecifiedError as OSStatus
    }

    test_get_default_raw_stream(|stream| {
        let listener = device_property_listener::new(
            kAudioObjectUnknown,
            get_property_address(
                Property::HardwareDefaultOutputDevice,
                DeviceType::INPUT | DeviceType::OUTPUT,
            ),
            callback,
        );
        assert_eq!(
            stream.add_device_listener(&listener),
            kAudioHardwareBadObjectError as OSStatus
        );
    });
}

// remove_listener (for default output device)
// ------------------------------------
#[test]
fn test_add_listener_then_remove_system_device() {
    extern "C" fn callback(
        _id: AudioObjectID,
        _number_of_addresses: u32,
        _addresses: *const AudioObjectPropertyAddress,
        _data: *mut c_void,
    ) -> OSStatus {
        assert!(false, "Should not be called.");
        kAudioHardwareUnspecifiedError as OSStatus
    }

    test_get_default_raw_stream(|stream| {
        let listener = device_property_listener::new(
            kAudioObjectSystemObject,
            get_property_address(
                Property::HardwareDefaultOutputDevice,
                DeviceType::INPUT | DeviceType::OUTPUT,
            ),
            callback,
        );
        assert_eq!(stream.add_device_listener(&listener), NO_ERR);
        assert_eq!(stream.remove_device_listener(&listener), NO_ERR);
    });
}

#[test]
fn test_remove_listener_without_adding_any_listener_before_system_device() {
    extern "C" fn callback(
        _id: AudioObjectID,
        _number_of_addresses: u32,
        _addresses: *const AudioObjectPropertyAddress,
        _data: *mut c_void,
    ) -> OSStatus {
        assert!(false, "Should not be called.");
        kAudioHardwareUnspecifiedError as OSStatus
    }

    test_get_default_raw_stream(|stream| {
        let listener = device_property_listener::new(
            kAudioObjectSystemObject,
            get_property_address(
                Property::HardwareDefaultOutputDevice,
                DeviceType::INPUT | DeviceType::OUTPUT,
            ),
            callback,
        );
        assert_eq!(stream.remove_device_listener(&listener), NO_ERR);
    });
}

#[test]
fn test_remove_listener_unknown_device() {
    extern "C" fn callback(
        _id: AudioObjectID,
        _number_of_addresses: u32,
        _addresses: *const AudioObjectPropertyAddress,
        _data: *mut c_void,
    ) -> OSStatus {
        assert!(false, "Should not be called.");
        kAudioHardwareUnspecifiedError as OSStatus
    }

    test_get_default_raw_stream(|stream| {
        let listener = device_property_listener::new(
            kAudioObjectUnknown,
            get_property_address(
                Property::HardwareDefaultOutputDevice,
                DeviceType::INPUT | DeviceType::OUTPUT,
            ),
            callback,
        );
        assert_eq!(
            stream.remove_device_listener(&listener),
            kAudioHardwareBadObjectError as OSStatus
        );
    });
}

// get_default_device_id
// ------------------------------------
#[test]
fn test_get_default_device_id() {
    if test_get_default_device(Scope::Input).is_some() {
        assert_ne!(
            audiounit_get_default_device_id(DeviceType::INPUT),
            kAudioObjectUnknown,
        );
    }

    if test_get_default_device(Scope::Output).is_some() {
        assert_ne!(
            audiounit_get_default_device_id(DeviceType::OUTPUT),
            kAudioObjectUnknown,
        );
    }
}

#[test]
#[should_panic]
fn test_get_default_device_id_with_unknown_type() {
    assert_eq!(
        audiounit_get_default_device_id(DeviceType::UNKNOWN),
        kAudioObjectUnknown,
    );
}

#[test]
#[should_panic]
fn test_get_default_device_id_with_inout_type() {
    assert_eq!(
        audiounit_get_default_device_id(DeviceType::INPUT | DeviceType::OUTPUT),
        kAudioObjectUnknown,
    );
}

// convert_channel_layout
// ------------------------------------
#[test]
fn test_convert_channel_layout() {
    let pairs = [
        (vec![kAudioObjectUnknown], vec![mixer::Channel::Silence]),
        (
            vec![kAudioChannelLabel_Mono],
            vec![mixer::Channel::FrontCenter],
        ),
        (
            vec![kAudioChannelLabel_Mono, kAudioChannelLabel_LFEScreen],
            vec![mixer::Channel::FrontCenter, mixer::Channel::LowFrequency],
        ),
        (
            vec![kAudioChannelLabel_Left, kAudioChannelLabel_Right],
            vec![mixer::Channel::FrontLeft, mixer::Channel::FrontRight],
        ),
        (
            vec![
                kAudioChannelLabel_Left,
                kAudioChannelLabel_Right,
                kAudioChannelLabel_Unknown,
            ],
            vec![
                mixer::Channel::FrontLeft,
                mixer::Channel::FrontRight,
                mixer::Channel::Silence,
            ],
        ),
        (
            vec![
                kAudioChannelLabel_Left,
                kAudioChannelLabel_Right,
                kAudioChannelLabel_Unused,
            ],
            vec![
                mixer::Channel::FrontLeft,
                mixer::Channel::FrontRight,
                mixer::Channel::Silence,
            ],
        ),
        (
            vec![
                kAudioChannelLabel_Left,
                kAudioChannelLabel_Right,
                kAudioChannelLabel_ForeignLanguage,
            ],
            vec![
                mixer::Channel::FrontLeft,
                mixer::Channel::FrontRight,
                mixer::Channel::Silence,
            ],
        ),
        // The SMPTE layouts.
        (
            vec![
                kAudioChannelLabel_Left,
                kAudioChannelLabel_Right,
                kAudioChannelLabel_LFEScreen,
            ],
            vec![
                mixer::Channel::FrontLeft,
                mixer::Channel::FrontRight,
                mixer::Channel::LowFrequency,
            ],
        ),
        (
            vec![
                kAudioChannelLabel_Left,
                kAudioChannelLabel_Right,
                kAudioChannelLabel_Center,
            ],
            vec![
                mixer::Channel::FrontLeft,
                mixer::Channel::FrontRight,
                mixer::Channel::FrontCenter,
            ],
        ),
        (
            vec![
                kAudioChannelLabel_Left,
                kAudioChannelLabel_Right,
                kAudioChannelLabel_Center,
                kAudioChannelLabel_LFEScreen,
            ],
            vec![
                mixer::Channel::FrontLeft,
                mixer::Channel::FrontRight,
                mixer::Channel::FrontCenter,
                mixer::Channel::LowFrequency,
            ],
        ),
        (
            vec![
                kAudioChannelLabel_Left,
                kAudioChannelLabel_Right,
                kAudioChannelLabel_CenterSurround,
            ],
            vec![
                mixer::Channel::FrontLeft,
                mixer::Channel::FrontRight,
                mixer::Channel::BackCenter,
            ],
        ),
        (
            vec![
                kAudioChannelLabel_Left,
                kAudioChannelLabel_Right,
                kAudioChannelLabel_CenterSurround,
                kAudioChannelLabel_LFEScreen,
            ],
            vec![
                mixer::Channel::FrontLeft,
                mixer::Channel::FrontRight,
                mixer::Channel::BackCenter,
                mixer::Channel::LowFrequency,
            ],
        ),
        (
            vec![
                kAudioChannelLabel_Left,
                kAudioChannelLabel_Right,
                kAudioChannelLabel_Center,
                kAudioChannelLabel_CenterSurround,
            ],
            vec![
                mixer::Channel::FrontLeft,
                mixer::Channel::FrontRight,
                mixer::Channel::FrontCenter,
                mixer::Channel::BackCenter,
            ],
        ),
        (
            vec![
                kAudioChannelLabel_Left,
                kAudioChannelLabel_Right,
                kAudioChannelLabel_Center,
                kAudioChannelLabel_CenterSurround,
                kAudioChannelLabel_LFEScreen,
            ],
            vec![
                mixer::Channel::FrontLeft,
                mixer::Channel::FrontRight,
                mixer::Channel::FrontCenter,
                mixer::Channel::BackCenter,
                mixer::Channel::LowFrequency,
            ],
        ),
        (
            vec![
                kAudioChannelLabel_Left,
                kAudioChannelLabel_Right,
                kAudioChannelLabel_LeftSurroundDirect,
                kAudioChannelLabel_RightSurroundDirect,
            ],
            vec![
                mixer::Channel::FrontLeft,
                mixer::Channel::FrontRight,
                mixer::Channel::SideLeft,
                mixer::Channel::SideRight,
            ],
        ),
        (
            vec![
                kAudioChannelLabel_Left,
                kAudioChannelLabel_Right,
                kAudioChannelLabel_LeftSurroundDirect,
                kAudioChannelLabel_RightSurroundDirect,
                kAudioChannelLabel_LFEScreen,
            ],
            vec![
                mixer::Channel::FrontLeft,
                mixer::Channel::FrontRight,
                mixer::Channel::SideLeft,
                mixer::Channel::SideRight,
                mixer::Channel::LowFrequency,
            ],
        ),
        (
            vec![
                kAudioChannelLabel_Left,
                kAudioChannelLabel_Right,
                kAudioChannelLabel_LeftSurround,
                kAudioChannelLabel_RightSurround,
            ],
            vec![
                mixer::Channel::FrontLeft,
                mixer::Channel::FrontRight,
                mixer::Channel::BackLeft,
                mixer::Channel::BackRight,
            ],
        ),
        (
            vec![
                kAudioChannelLabel_Left,
                kAudioChannelLabel_Right,
                kAudioChannelLabel_LeftSurround,
                kAudioChannelLabel_RightSurround,
                kAudioChannelLabel_LFEScreen,
            ],
            vec![
                mixer::Channel::FrontLeft,
                mixer::Channel::FrontRight,
                mixer::Channel::BackLeft,
                mixer::Channel::BackRight,
                mixer::Channel::LowFrequency,
            ],
        ),
        (
            vec![
                kAudioChannelLabel_Left,
                kAudioChannelLabel_Right,
                kAudioChannelLabel_Center,
                kAudioChannelLabel_LeftSurroundDirect,
                kAudioChannelLabel_RightSurroundDirect,
            ],
            vec![
                mixer::Channel::FrontLeft,
                mixer::Channel::FrontRight,
                mixer::Channel::FrontCenter,
                mixer::Channel::SideLeft,
                mixer::Channel::SideRight,
            ],
        ),
        (
            vec![
                kAudioChannelLabel_Left,
                kAudioChannelLabel_Right,
                kAudioChannelLabel_Center,
                kAudioChannelLabel_LeftSurroundDirect,
                kAudioChannelLabel_RightSurroundDirect,
                kAudioChannelLabel_LFEScreen,
            ],
            vec![
                mixer::Channel::FrontLeft,
                mixer::Channel::FrontRight,
                mixer::Channel::FrontCenter,
                mixer::Channel::SideLeft,
                mixer::Channel::SideRight,
                mixer::Channel::LowFrequency,
            ],
        ),
        (
            vec![
                kAudioChannelLabel_Left,
                kAudioChannelLabel_Right,
                kAudioChannelLabel_LeftSurround,
                kAudioChannelLabel_RightSurround,
                kAudioChannelLabel_Center,
            ],
            vec![
                mixer::Channel::FrontLeft,
                mixer::Channel::FrontRight,
                mixer::Channel::BackLeft,
                mixer::Channel::BackRight,
                mixer::Channel::FrontCenter,
            ],
        ),
        (
            vec![
                kAudioChannelLabel_Left,
                kAudioChannelLabel_Right,
                kAudioChannelLabel_LeftSurround,
                kAudioChannelLabel_RightSurround,
                kAudioChannelLabel_Center,
                kAudioChannelLabel_LFEScreen,
            ],
            vec![
                mixer::Channel::FrontLeft,
                mixer::Channel::FrontRight,
                mixer::Channel::BackLeft,
                mixer::Channel::BackRight,
                mixer::Channel::FrontCenter,
                mixer::Channel::LowFrequency,
            ],
        ),
        (
            vec![
                kAudioChannelLabel_Left,
                kAudioChannelLabel_Right,
                kAudioChannelLabel_Center,
                kAudioChannelLabel_LFEScreen,
                kAudioChannelLabel_CenterSurround,
                kAudioChannelLabel_LeftSurroundDirect,
                kAudioChannelLabel_RightSurroundDirect,
            ],
            vec![
                mixer::Channel::FrontLeft,
                mixer::Channel::FrontRight,
                mixer::Channel::FrontCenter,
                mixer::Channel::LowFrequency,
                mixer::Channel::BackCenter,
                mixer::Channel::SideLeft,
                mixer::Channel::SideRight,
            ],
        ),
        (
            vec![
                kAudioChannelLabel_Left,
                kAudioChannelLabel_Right,
                kAudioChannelLabel_Center,
                kAudioChannelLabel_LFEScreen,
                kAudioChannelLabel_LeftSurround,
                kAudioChannelLabel_RightSurround,
                kAudioChannelLabel_LeftSurroundDirect,
                kAudioChannelLabel_RightSurroundDirect,
            ],
            vec![
                mixer::Channel::FrontLeft,
                mixer::Channel::FrontRight,
                mixer::Channel::FrontCenter,
                mixer::Channel::LowFrequency,
                mixer::Channel::BackLeft,
                mixer::Channel::BackRight,
                mixer::Channel::SideLeft,
                mixer::Channel::SideRight,
            ],
        ),
    ];

    const MAX_CHANNELS: usize = 10;
    // A Rust mapping structure of the AudioChannelLayout with MAX_CHANNELS channels
    // https://github.com/phracker/MacOSX-SDKs/blob/master/MacOSX10.13.sdk/System/Library/Frameworks/CoreAudio.framework/Versions/A/Headers/CoreAudioTypes.h#L1332
    #[repr(C)]
    struct TestLayout {
        tag: AudioChannelLayoutTag,
        map: AudioChannelBitmap,
        number_channel_descriptions: UInt32,
        channel_descriptions: [AudioChannelDescription; MAX_CHANNELS],
    }

    impl Default for TestLayout {
        fn default() -> Self {
            Self {
                tag: AudioChannelLayoutTag::default(),
                map: AudioChannelBitmap::default(),
                number_channel_descriptions: UInt32::default(),
                channel_descriptions: [AudioChannelDescription::default(); MAX_CHANNELS],
            }
        }
    }

    let mut layout = TestLayout::default();
    layout.tag = kAudioChannelLayoutTag_UseChannelDescriptions;

    for (labels, expected_layout) in pairs.iter() {
        assert!(labels.len() <= MAX_CHANNELS);
        layout.number_channel_descriptions = labels.len() as u32;
        for (idx, label) in labels.iter().enumerate() {
            layout.channel_descriptions[idx].mChannelLabel = *label;
        }
        let layout_ref = unsafe { &(*(&layout as *const TestLayout as *const AudioChannelLayout)) };
        assert_eq!(
            &audiounit_convert_channel_layout(layout_ref),
            expected_layout
        );
    }
}

// get_preferred_channel_layout
// ------------------------------------
#[test]
fn test_get_preferred_channel_layout_output() {
    match test_get_default_audiounit(Scope::Output) {
        Some(unit) => assert!(!audiounit_get_preferred_channel_layout(unit.get_inner()).is_empty()),
        None => println!("No output audiounit for test."),
    }
}

// get_current_channel_layout
// ------------------------------------
#[test]
fn test_get_current_channel_layout_output() {
    match test_get_default_audiounit(Scope::Output) {
        Some(unit) => assert!(!audiounit_get_current_channel_layout(unit.get_inner()).is_empty()),
        None => println!("No output audiounit for test."),
    }
}

// create_stream_description
// ------------------------------------
#[test]
fn test_create_stream_description() {
    let mut channels = 0;
    for (bits, format, flags) in [
        (
            16_u32,
            ffi::CUBEB_SAMPLE_S16LE,
            kAudioFormatFlagIsSignedInteger,
        ),
        (
            16_u32,
            ffi::CUBEB_SAMPLE_S16BE,
            kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsBigEndian,
        ),
        (32_u32, ffi::CUBEB_SAMPLE_FLOAT32LE, kAudioFormatFlagIsFloat),
        (
            32_u32,
            ffi::CUBEB_SAMPLE_FLOAT32BE,
            kAudioFormatFlagIsFloat | kAudioFormatFlagIsBigEndian,
        ),
    ]
    .iter()
    {
        let bytes = bits / 8;
        channels += 1;

        let mut raw = ffi::cubeb_stream_params::default();
        raw.format = *format;
        raw.rate = 48_000;
        raw.channels = channels;
        raw.layout = ffi::CUBEB_LAYOUT_UNDEFINED;
        raw.prefs = ffi::CUBEB_STREAM_PREF_NONE;
        let params = StreamParams::from(raw);
        let description = create_stream_description(&params).unwrap();
        assert_eq!(description.mFormatID, kAudioFormatLinearPCM);
        assert_eq!(
            description.mFormatFlags,
            flags | kLinearPCMFormatFlagIsPacked
        );
        assert_eq!(description.mSampleRate as u32, raw.rate);
        assert_eq!(description.mChannelsPerFrame, raw.channels);
        assert_eq!(description.mBytesPerFrame, bytes * raw.channels);
        assert_eq!(description.mFramesPerPacket, 1);
        assert_eq!(description.mBytesPerPacket, bytes * raw.channels);
        assert_eq!(description.mReserved, 0);
    }
}

// create_default_audiounit
// ------------------------------------
#[test]
fn test_create_default_audiounit() {
    let flags_list = [
        device_flags::DEV_UNKNOWN,
        device_flags::DEV_INPUT,
        device_flags::DEV_OUTPUT,
        device_flags::DEV_INPUT | device_flags::DEV_OUTPUT,
        device_flags::DEV_INPUT | device_flags::DEV_SYSTEM_DEFAULT,
        device_flags::DEV_OUTPUT | device_flags::DEV_SYSTEM_DEFAULT,
        device_flags::DEV_INPUT | device_flags::DEV_OUTPUT | device_flags::DEV_SYSTEM_DEFAULT,
    ];

    for flags in flags_list.iter() {
        let unit = create_default_audiounit(*flags).unwrap();
        assert!(!unit.is_null());
        // Destroy the AudioUnits
        unsafe {
            AudioUnitUninitialize(unit);
            AudioComponentInstanceDispose(unit);
        }
    }
}

// enable_audiounit_scope
// ------------------------------------
#[test]
fn test_enable_audiounit_scope() {
    // It's ok to enable and disable the scopes of input or output
    // for the unit whose subtype is kAudioUnitSubType_HALOutput
    // even when there is no available input or output devices.
    if let Some(unit) = test_create_audiounit(ComponentSubType::HALOutput) {
        assert!(enable_audiounit_scope(unit.get_inner(), DeviceType::OUTPUT, true).is_ok());
        assert!(enable_audiounit_scope(unit.get_inner(), DeviceType::OUTPUT, false).is_ok());
        assert!(enable_audiounit_scope(unit.get_inner(), DeviceType::INPUT, true).is_ok());
        assert!(enable_audiounit_scope(unit.get_inner(), DeviceType::INPUT, false).is_ok());
    } else {
        println!("No audiounit to perform test.");
    }
}

#[test]
fn test_enable_audiounit_scope_for_default_output_unit() {
    if let Some(unit) = test_create_audiounit(ComponentSubType::DefaultOutput) {
        assert_eq!(
            enable_audiounit_scope(unit.get_inner(), DeviceType::OUTPUT, true).unwrap_err(),
            kAudioUnitErr_InvalidProperty
        );
        assert_eq!(
            enable_audiounit_scope(unit.get_inner(), DeviceType::OUTPUT, false).unwrap_err(),
            kAudioUnitErr_InvalidProperty
        );
        assert_eq!(
            enable_audiounit_scope(unit.get_inner(), DeviceType::INPUT, true).unwrap_err(),
            kAudioUnitErr_InvalidProperty
        );
        assert_eq!(
            enable_audiounit_scope(unit.get_inner(), DeviceType::INPUT, false).unwrap_err(),
            kAudioUnitErr_InvalidProperty
        );
    }
}

#[test]
#[should_panic]
fn test_enable_audiounit_scope_with_null_unit() {
    let unit: AudioUnit = ptr::null_mut();
    assert!(enable_audiounit_scope(unit, DeviceType::INPUT, false).is_err());
}

// create_audiounit
// ------------------------------------
#[test]
fn test_for_create_audiounit() {
    let flags_list = [
        device_flags::DEV_INPUT,
        device_flags::DEV_OUTPUT,
        device_flags::DEV_INPUT | device_flags::DEV_SYSTEM_DEFAULT,
        device_flags::DEV_OUTPUT | device_flags::DEV_SYSTEM_DEFAULT,
    ];

    let default_input = test_get_default_device(Scope::Input);
    let default_output = test_get_default_device(Scope::Output);

    for flags in flags_list.iter() {
        let mut device = device_info::default();
        device.flags |= *flags;

        // Check the output scope is enabled.
        if device.flags.contains(device_flags::DEV_OUTPUT) && default_output.is_some() {
            let device_id = default_output.clone().unwrap();
            device.id = device_id;
            let unit = create_audiounit(&device).unwrap();
            assert!(!unit.is_null());
            assert!(test_audiounit_scope_is_enabled(unit, Scope::Output));

            // For default output device, the input scope is enabled
            // if it's also a input device. Otherwise, it's disabled.
            if device
                .flags
                .contains(device_flags::DEV_INPUT | device_flags::DEV_SYSTEM_DEFAULT)
            {
                assert_eq!(
                    test_device_in_scope(device_id, Scope::Input),
                    test_audiounit_scope_is_enabled(unit, Scope::Input)
                );

                // Destroy the audioUnit.
                unsafe {
                    AudioUnitUninitialize(unit);
                    AudioComponentInstanceDispose(unit);
                }
                continue;
            }

            // Destroy the audioUnit.
            unsafe {
                AudioUnitUninitialize(unit);
                AudioComponentInstanceDispose(unit);
            }
        }

        // Check the input scope is enabled.
        if device.flags.contains(device_flags::DEV_INPUT) && default_input.is_some() {
            let device_id = default_input.clone().unwrap();
            device.id = device_id;
            let unit = create_audiounit(&device).unwrap();
            assert!(!unit.is_null());
            assert!(test_audiounit_scope_is_enabled(unit, Scope::Input));
            // Destroy the audioUnit.
            unsafe {
                AudioUnitUninitialize(unit);
                AudioComponentInstanceDispose(unit);
            }
        }
    }
}

#[test]
#[should_panic]
fn test_create_audiounit_with_unknown_scope() {
    let device = device_info::default();
    let _unit = create_audiounit(&device);
}

// clamp_latency
// ------------------------------------
#[test]
fn test_clamp_latency() {
    let range = 0..2 * SAFE_MAX_LATENCY_FRAMES;
    assert!(range.start < SAFE_MIN_LATENCY_FRAMES);
    // assert!(range.end < SAFE_MAX_LATENCY_FRAMES);
    for latency_frames in range {
        let clamp = clamp_latency(latency_frames);
        assert!(clamp >= SAFE_MIN_LATENCY_FRAMES);
        assert!(clamp <= SAFE_MAX_LATENCY_FRAMES);
    }
}

// set_buffer_size_sync
// ------------------------------------
#[test]
fn test_set_buffer_size_sync() {
    test_set_buffer_size_by_scope(Scope::Input);
    test_set_buffer_size_by_scope(Scope::Output);
    fn test_set_buffer_size_by_scope(scope: Scope) {
        let unit = test_get_default_audiounit(scope.clone());
        if unit.is_none() {
            println!("No audiounit for {:?}.", scope);
            return;
        }
        let unit = unit.unwrap();
        let prop_scope = match scope {
            Scope::Input => PropertyScope::Output,
            Scope::Output => PropertyScope::Input,
        };
        let mut buffer_frames = test_audiounit_get_buffer_frame_size(
            unit.get_inner(),
            scope.clone(),
            prop_scope.clone(),
        )
        .unwrap();
        assert_ne!(buffer_frames, 0);
        buffer_frames *= 2;
        assert!(
            set_buffer_size_sync(unit.get_inner(), scope.clone().into(), buffer_frames).is_ok()
        );
        let new_buffer_frames =
            test_audiounit_get_buffer_frame_size(unit.get_inner(), scope.clone(), prop_scope)
                .unwrap();
        assert_eq!(buffer_frames, new_buffer_frames);
    }
}

#[test]
#[should_panic]
fn test_set_buffer_size_sync_for_input_with_null_input_unit() {
    test_set_buffer_size_sync_by_scope_with_null_unit(Scope::Input);
}

#[test]
#[should_panic]
fn test_set_buffer_size_sync_for_output_with_null_output_unit() {
    test_set_buffer_size_sync_by_scope_with_null_unit(Scope::Output);
}

fn test_set_buffer_size_sync_by_scope_with_null_unit(scope: Scope) {
    let unit: AudioUnit = ptr::null_mut();
    assert!(set_buffer_size_sync(unit, scope.into(), 2048).is_err());
}

// get_volume, set_volume
// ------------------------------------
#[test]
fn test_stream_get_volume() {
    if let Some(unit) = test_get_default_audiounit(Scope::Output) {
        let expected_volume: f32 = 0.5;
        set_volume(unit.get_inner(), expected_volume);
        assert_eq!(expected_volume, get_volume(unit.get_inner()).unwrap());
    } else {
        println!("No output audiounit.");
    }
}

// convert_uint32_into_string
// ------------------------------------
#[test]
fn test_convert_uint32_into_string() {
    let empty = convert_uint32_into_string(0);
    assert_eq!(empty, CString::default());

    let data: u32 = ('R' as u32) << 24 | ('U' as u32) << 16 | ('S' as u32) << 8 | 'T' as u32;
    let data_string = convert_uint32_into_string(data);
    assert_eq!(data_string, CString::new("RUST").unwrap());
}

// get_default_datasource_string
// ------------------------------------
#[test]
fn test_get_default_device_name() {
    test_get_default_device_name_in_scope(Scope::Input);
    test_get_default_device_name_in_scope(Scope::Output);

    fn test_get_default_device_name_in_scope(scope: Scope) {
        if let Some(name) = test_get_default_source_name(scope.clone()) {
            let source = audiounit_get_default_datasource_string(scope.into())
                .unwrap()
                .into_string()
                .unwrap();
            assert_eq!(name, source);
        } else {
            println!("No source name for {:?}", scope);
        }
    }
}

// is_device_a_type_of
// ------------------------------------
#[test]
fn test_is_device_a_type_of() {
    test_is_device_in_scope(Scope::Input);
    test_is_device_in_scope(Scope::Output);

    fn test_is_device_in_scope(scope: Scope) {
        if let Some(device) = test_get_default_device(scope.clone()) {
            assert!(is_device_a_type_of(device, scope.into()));
        } else {
            println!("No device for {:?}.", scope);
        }
    }
}

// get_channel_count
// ------------------------------------
#[test]
fn test_get_channel_count() {
    test_channel_count(Scope::Input);
    test_channel_count(Scope::Output);

    fn test_channel_count(scope: Scope) {
        if let Some(device) = test_get_default_device(scope.clone()) {
            let channels = get_channel_count(device, DeviceType::from(scope.clone())).unwrap();
            assert!(channels > 0);
            assert_eq!(
                channels,
                test_device_channels_in_scope(device, scope).unwrap()
            );
        } else {
            println!("No device for {:?}.", scope);
        }
    }
}

#[test]
fn test_get_channel_count_of_input_for_a_output_only_deivce() {
    let devices = test_get_devices_in_scope(Scope::Output);
    for device in devices {
        // Skip in-out devices.
        if test_device_in_scope(device, Scope::Input) {
            continue;
        }
        let count = get_channel_count(device, DeviceType::INPUT).unwrap();
        assert_eq!(count, 0);
    }
}

#[test]
fn test_get_channel_count_of_output_for_a_input_only_deivce() {
    let devices = test_get_devices_in_scope(Scope::Input);
    for device in devices {
        // Skip in-out devices.
        if test_device_in_scope(device, Scope::Output) {
            continue;
        }
        let count = get_channel_count(device, DeviceType::OUTPUT).unwrap();
        assert_eq!(count, 0);
    }
}

#[test]
#[should_panic]
fn test_get_channel_count_of_unknown_device() {
    assert_eq!(
        get_channel_count(kAudioObjectUnknown, DeviceType::OUTPUT).unwrap_err(),
        Error::error()
    );
}

#[test]
fn test_get_channel_count_of_inout_type() {
    test_channel_count(Scope::Input);
    test_channel_count(Scope::Output);

    fn test_channel_count(scope: Scope) {
        if let Some(device) = test_get_default_device(scope.clone()) {
            assert_eq!(
                // Get a kAudioHardwareUnknownPropertyError in get_channel_count actually.
                get_channel_count(device, DeviceType::INPUT | DeviceType::OUTPUT).unwrap_err(),
                Error::error()
            );
        } else {
            println!("No device for {:?}.", scope);
        }
    }
}

#[test]
#[should_panic]
fn test_get_channel_count_of_unknwon_type() {
    test_channel_count(Scope::Input);
    test_channel_count(Scope::Output);

    fn test_channel_count(scope: Scope) {
        if let Some(device) = test_get_default_device(scope.clone()) {
            assert_eq!(
                get_channel_count(device, DeviceType::UNKNOWN).unwrap_err(),
                Error::error()
            );
        } else {
            panic!("Panic by default: No device for {:?}.", scope);
        }
    }
}

// get_range_of_sample_rates
// ------------------------------------
#[test]
fn test_get_range_of_sample_rates() {
    test_get_range_of_sample_rates_in_scope(Scope::Input);
    test_get_range_of_sample_rates_in_scope(Scope::Output);

    fn test_get_range_of_sample_rates_in_scope(scope: Scope) {
        if let Some(device) = test_get_default_device(scope.clone()) {
            let ranges = test_get_available_samplerate_of_device(device);
            for range in ranges {
                // Surprisingly, we can get the input/output sample rates from a non-input/non-output device.
                check_samplerates(range);
            }
        } else {
            println!("No device for {:?}.", scope);
        }
    }

    fn test_get_available_samplerate_of_device(id: AudioObjectID) -> Vec<(f64, f64)> {
        let scopes = [
            DeviceType::INPUT,
            DeviceType::OUTPUT,
            DeviceType::INPUT | DeviceType::OUTPUT,
        ];
        let mut ranges = Vec::new();
        for scope in scopes.iter() {
            ranges.push(get_range_of_sample_rates(id, *scope).unwrap());
        }
        ranges
    }

    fn check_samplerates((min, max): (f64, f64)) {
        assert!(min > 0.0);
        assert!(max > 0.0);
        assert!(min <= max);
    }
}

// get_presentation_latency
// ------------------------------------
#[test]
fn test_get_device_presentation_latency() {
    test_get_device_presentation_latencies_in_scope(Scope::Input);
    test_get_device_presentation_latencies_in_scope(Scope::Output);

    fn test_get_device_presentation_latencies_in_scope(scope: Scope) {
        if let Some(device) = test_get_default_device(scope.clone()) {
            // TODO: The latencies very from devices to devices. Check nothing here.
            let latency = get_presentation_latency(device, scope.clone().into());
            println!(
                "present latency on the device {} in scope {:?}: {}",
                device, scope, latency
            );
        } else {
            println!("No device for {:?}.", scope);
        }
    }
}

// get_device_group_id
// ------------------------------------
#[test]
fn test_get_device_group_id() {
    if let Some(device) = test_get_default_device(Scope::Input) {
        match get_device_group_id(device, DeviceType::INPUT) {
            Ok(id) => println!("input group id: {:?}", id),
            Err(e) => println!("No input group id. Error: {}", e),
        }
    } else {
        println!("No input device.");
    }

    if let Some(device) = test_get_default_device(Scope::Output) {
        match get_device_group_id(device, DeviceType::OUTPUT) {
            Ok(id) => println!("output group id: {:?}", id),
            Err(e) => println!("No output group id. Error: {}", e),
        }
    } else {
        println!("No output device.");
    }
}

#[test]
fn test_get_same_group_id_for_builtin_device_pairs() {
    use std::collections::HashMap;

    const IMIC: u32 = 0x696D_6963; // "imic"
    const ISPK: u32 = 0x6973_706B; // "ispk"
    const EMIC: u32 = 0x656D_6963; // "emic"
    const HDPN: u32 = 0x6864_706E; // "hdpn"
    let pairs = [(IMIC, ISPK), (EMIC, HDPN)];

    // TODO: group_ids will have collision if one input device and one output device
    //       has same source value.
    let mut group_ids = HashMap::<u32, String>::new();
    let input_devices = test_get_devices_in_scope(Scope::Input);
    for device in input_devices.iter() {
        match get_device_source(*device, DeviceType::INPUT) {
            Ok(source) => match get_device_group_id(*device, DeviceType::INPUT) {
                Ok(id) => assert!(group_ids
                    .insert(source, id.into_string().unwrap())
                    .is_none()),
                Err(e) => assert!(group_ids.insert(source, format!("Error {}", e)).is_none()),
            },
            _ => {} // do nothing when failing to get source.
        }
    }
    let output_devices = test_get_devices_in_scope(Scope::Output);
    for device in output_devices.iter() {
        match get_device_source(*device, DeviceType::OUTPUT) {
            Ok(source) => match get_device_group_id(*device, DeviceType::OUTPUT) {
                Ok(id) => assert!(group_ids
                    .insert(source, id.into_string().unwrap())
                    .is_none()),
                Err(e) => assert!(group_ids.insert(source, format!("Error {}", e)).is_none()),
            },
            _ => {} // do nothing when failing to get source.
        }
    }

    for (dev1, dev2) in pairs.iter() {
        let id1 = group_ids.get(dev1);
        let id2 = group_ids.get(dev2);

        if id1.is_some() && id2.is_some() {
            assert_eq!(id1, id2);
        }

        group_ids.remove(dev1);
        group_ids.remove(dev2);
    }
}

#[test]
#[should_panic]
fn test_get_device_group_id_by_unknown_device() {
    assert!(get_device_group_id(kAudioObjectUnknown, DeviceType::INPUT).is_err());
}

// get_device_label
// ------------------------------------
#[test]
fn test_get_device_label() {
    if let Some(device) = test_get_default_device(Scope::Input) {
        let name = get_device_label(device, DeviceType::INPUT).unwrap();
        println!("input device label: {}", name.into_string());
    } else {
        println!("No input device.");
    }

    if let Some(device) = test_get_default_device(Scope::Output) {
        let name = get_device_label(device, DeviceType::OUTPUT).unwrap();
        println!("output device label: {}", name.into_string());
    } else {
        println!("No output device.");
    }
}

#[test]
#[should_panic]
fn test_get_device_label_by_unknown_device() {
    assert!(get_device_label(kAudioObjectUnknown, DeviceType::INPUT).is_err());
}

// get_device_global_uid
// ------------------------------------
#[test]
fn test_get_device_global_uid() {
    // Input device.
    if let Some(input) = test_get_default_device(Scope::Input) {
        let uid = get_device_global_uid(input).unwrap();
        let uid = uid.into_string();
        assert!(!uid.is_empty());
    }

    // Output device.
    if let Some(output) = test_get_default_device(Scope::Output) {
        let uid = get_device_global_uid(output).unwrap();
        let uid = uid.into_string();
        assert!(!uid.is_empty());
    }
}

#[test]
#[should_panic]
fn test_get_device_global_uid_by_unknwon_device() {
    // Unknown device.
    assert!(get_device_global_uid(kAudioObjectUnknown).is_err());
}

// create_cubeb_device_info
// destroy_cubeb_device_info
// ------------------------------------
#[test]
fn test_create_cubeb_device_info() {
    use std::collections::VecDeque;

    test_create_device_from_hwdev_in_scope(Scope::Input);
    test_create_device_from_hwdev_in_scope(Scope::Output);

    fn test_create_device_from_hwdev_in_scope(scope: Scope) {
        if let Some(device) = test_get_default_device(scope.clone()) {
            let is_input = test_device_in_scope(device, Scope::Input);
            let is_output = test_device_in_scope(device, Scope::Output);
            let mut results = test_create_device_infos_by_device(device);
            assert_eq!(results.len(), 2);
            // Input device type:
            let input_result = results.pop_front().unwrap();
            if is_input {
                let mut input_device_info = input_result.unwrap();
                check_device_info_by_device(&input_device_info, device, Scope::Input);
                destroy_cubeb_device_info(&mut input_device_info);
            } else {
                assert_eq!(input_result.unwrap_err(), Error::error());
            }
            // Output device type:
            let output_result = results.pop_front().unwrap();
            if is_output {
                let mut output_device_info = output_result.unwrap();
                check_device_info_by_device(&output_device_info, device, Scope::Output);
                destroy_cubeb_device_info(&mut output_device_info);
            } else {
                assert_eq!(output_result.unwrap_err(), Error::error());
            }
        } else {
            println!("No device for {:?}.", scope);
        }
    }

    fn test_create_device_infos_by_device(
        id: AudioObjectID,
    ) -> VecDeque<std::result::Result<ffi::cubeb_device_info, Error>> {
        let dev_types = [DeviceType::INPUT, DeviceType::OUTPUT];
        let mut results = VecDeque::new();
        for dev_type in dev_types.iter() {
            results.push_back(create_cubeb_device_info(id, *dev_type));
        }
        results
    }

    fn check_device_info_by_device(info: &ffi::cubeb_device_info, id: AudioObjectID, scope: Scope) {
        assert!(!info.devid.is_null());
        assert!(mem::size_of_val(&info.devid) >= mem::size_of::<AudioObjectID>());
        assert_eq!(info.devid as AudioObjectID, id);
        assert!(!info.device_id.is_null());
        assert!(!info.friendly_name.is_null());
        assert!(!info.group_id.is_null());

        // TODO: Hit a kAudioHardwareUnknownPropertyError for AirPods
        // assert!(!info.vendor_name.is_null());

        // FIXIT: The device is defined to input-only or output-only, but some device is in-out!
        assert_eq!(info.device_type, DeviceType::from(scope.clone()).bits());
        assert_eq!(info.state, ffi::CUBEB_DEVICE_STATE_ENABLED);
        // TODO: The preference is set when the device is default input/output device if the device
        //       info is created from input/output scope. Should the preference be set if the
        //       device is a default input/output device if the device info is created from
        //       output/input scope ? The device may be a in-out device!
        assert_eq!(info.preferred, get_cubeb_device_pref(id, scope));

        assert_eq!(info.format, ffi::CUBEB_DEVICE_FMT_ALL);
        assert_eq!(info.default_format, ffi::CUBEB_DEVICE_FMT_F32NE);
        assert!(info.max_channels > 0);
        assert!(info.min_rate <= info.max_rate);
        assert!(info.min_rate <= info.default_rate);
        assert!(info.default_rate <= info.max_rate);

        assert!(info.latency_lo > 0);
        assert!(info.latency_hi > 0);
        assert!(info.latency_lo <= info.latency_hi);

        fn get_cubeb_device_pref(id: AudioObjectID, scope: Scope) -> ffi::cubeb_device_pref {
            let default_device = test_get_default_device(scope);
            if default_device.is_some() && default_device.unwrap() == id {
                ffi::CUBEB_DEVICE_PREF_ALL
            } else {
                ffi::CUBEB_DEVICE_PREF_NONE
            }
        }
    }
}

#[test]
#[should_panic]
fn test_create_device_info_by_unknown_device() {
    assert!(create_cubeb_device_info(kAudioObjectUnknown, DeviceType::OUTPUT).is_err());
}

#[test]
#[should_panic]
fn test_create_device_info_with_unknown_type() {
    test_create_device_info_with_unknown_type_by_scope(Scope::Input);
    test_create_device_info_with_unknown_type_by_scope(Scope::Output);

    fn test_create_device_info_with_unknown_type_by_scope(scope: Scope) {
        if let Some(device) = test_get_default_device(scope.clone()) {
            assert!(create_cubeb_device_info(device, DeviceType::UNKNOWN).is_err());
        } else {
            panic!("Panic by default: No device for {:?}.", scope);
        }
    }
}

#[test]
#[should_panic]
fn test_device_destroy_empty_device() {
    let mut device = ffi::cubeb_device_info::default();

    assert!(device.device_id.is_null());
    assert!(device.group_id.is_null());
    assert!(device.friendly_name.is_null());
    assert!(device.vendor_name.is_null());

    // `friendly_name` must be set.
    destroy_cubeb_device_info(&mut device);

    assert!(device.device_id.is_null());
    assert!(device.group_id.is_null());
    assert!(device.friendly_name.is_null());
    assert!(device.vendor_name.is_null());
}

#[test]
fn test_create_device_from_hwdev_with_inout_type() {
    test_create_device_from_hwdev_with_inout_type_by_scope(Scope::Input);
    test_create_device_from_hwdev_with_inout_type_by_scope(Scope::Output);

    fn test_create_device_from_hwdev_with_inout_type_by_scope(scope: Scope) {
        if let Some(device) = test_get_default_device(scope.clone()) {
            // Get a kAudioHardwareUnknownPropertyError in get_channel_count actually.
            assert!(
                create_cubeb_device_info(device, DeviceType::INPUT | DeviceType::OUTPUT).is_err()
            );
        } else {
            println!("No device for {:?}.", scope);
        }
    }
}

// is_aggregate_device
// ------------------------------------
#[test]
fn test_is_aggregate_device() {
    let mut aggregate_name = String::from(PRIVATE_AGGREGATE_DEVICE_NAME);
    aggregate_name.push_str("_something");
    let aggregate_name_cstring = CString::new(aggregate_name).unwrap();

    let mut info = ffi::cubeb_device_info::default();
    info.friendly_name = aggregate_name_cstring.as_ptr();
    assert!(is_aggregate_device(&info));

    let non_aggregate_name_cstring = CString::new("Hello World!").unwrap();
    info.friendly_name = non_aggregate_name_cstring.as_ptr();
    assert!(!is_aggregate_device(&info));
}

// get_devices_of_type
// ------------------------------------
#[test]
fn test_get_devices_of_type() {
    use std::collections::HashSet;

    let all_devices = audiounit_get_devices_of_type(DeviceType::INPUT | DeviceType::OUTPUT);
    let input_devices = audiounit_get_devices_of_type(DeviceType::INPUT);
    let output_devices = audiounit_get_devices_of_type(DeviceType::OUTPUT);

    let mut expected_all = test_get_all_devices();
    expected_all.sort();
    assert_eq!(all_devices, expected_all);
    for device in all_devices.iter() {
        if test_device_in_scope(*device, Scope::Input) {
            assert!(input_devices.contains(device));
        }
        if test_device_in_scope(*device, Scope::Output) {
            assert!(output_devices.contains(device));
        }
    }

    let input: HashSet<AudioObjectID> = input_devices.iter().cloned().collect();
    let output: HashSet<AudioObjectID> = output_devices.iter().cloned().collect();
    let union: HashSet<AudioObjectID> = input.union(&output).cloned().collect();
    let mut union_devices: Vec<AudioObjectID> = union.iter().cloned().collect();
    union_devices.sort();
    assert_eq!(all_devices, union_devices);
}

#[test]
#[should_panic]
fn test_get_devices_of_type_unknown() {
    let no_devs = audiounit_get_devices_of_type(DeviceType::UNKNOWN);
    assert!(no_devs.is_empty());
}

// add_devices_changed_listener
// ------------------------------------
#[test]
fn test_add_devices_changed_listener() {
    use std::collections::HashMap;

    extern "C" fn inout_callback(_: *mut ffi::cubeb, _: *mut c_void) {}
    extern "C" fn in_callback(_: *mut ffi::cubeb, _: *mut c_void) {}
    extern "C" fn out_callback(_: *mut ffi::cubeb, _: *mut c_void) {}

    let mut map: HashMap<DeviceType, extern "C" fn(*mut ffi::cubeb, *mut c_void)> = HashMap::new();
    map.insert(DeviceType::INPUT, in_callback);
    map.insert(DeviceType::OUTPUT, out_callback);
    map.insert(DeviceType::INPUT | DeviceType::OUTPUT, inout_callback);

    test_get_raw_context(|context| {
        for (devtype, callback) in map.iter() {
            assert!(get_devices_changed_callback(context, Scope::Input).is_none());
            assert!(get_devices_changed_callback(context, Scope::Output).is_none());

            // Register a callback within a specific scope.
            assert!(context
                .add_devices_changed_listener(*devtype, Some(*callback), ptr::null_mut())
                .is_ok());

            if devtype.contains(DeviceType::INPUT) {
                let cb = get_devices_changed_callback(context, Scope::Input);
                assert!(cb.is_some());
                assert_eq!(cb.unwrap(), *callback);
            } else {
                let cb = get_devices_changed_callback(context, Scope::Input);
                assert!(cb.is_none());
            }

            if devtype.contains(DeviceType::OUTPUT) {
                let cb = get_devices_changed_callback(context, Scope::Output);
                assert!(cb.is_some());
                assert_eq!(cb.unwrap(), *callback);
            } else {
                let cb = get_devices_changed_callback(context, Scope::Output);
                assert!(cb.is_none());
            }

            // Unregister the callbacks within all scopes.
            assert!(context
                .remove_devices_changed_listener(DeviceType::INPUT | DeviceType::OUTPUT)
                .is_ok());

            assert!(get_devices_changed_callback(context, Scope::Input).is_none());
            assert!(get_devices_changed_callback(context, Scope::Output).is_none());
        }
    });
}

#[test]
#[should_panic]
fn test_add_devices_changed_listener_in_unknown_scope() {
    extern "C" fn callback(_: *mut ffi::cubeb, _: *mut c_void) {}

    test_get_raw_context(|context| {
        let _ = context.add_devices_changed_listener(
            DeviceType::UNKNOWN,
            Some(callback),
            ptr::null_mut(),
        );
    });
}

#[test]
#[should_panic]
fn test_add_devices_changed_listener_with_none_callback() {
    test_get_raw_context(|context| {
        for devtype in &[DeviceType::INPUT, DeviceType::OUTPUT] {
            assert!(context
                .add_devices_changed_listener(*devtype, None, ptr::null_mut())
                .is_ok());
        }
    });
}

// remove_devices_changed_listener
// ------------------------------------
#[test]
fn test_remove_devices_changed_listener() {
    use std::collections::HashMap;

    extern "C" fn in_callback(_: *mut ffi::cubeb, _: *mut c_void) {}
    extern "C" fn out_callback(_: *mut ffi::cubeb, _: *mut c_void) {}

    let mut map: HashMap<DeviceType, extern "C" fn(*mut ffi::cubeb, *mut c_void)> = HashMap::new();
    map.insert(DeviceType::INPUT, in_callback);
    map.insert(DeviceType::OUTPUT, out_callback);

    test_get_raw_context(|context| {
        for (devtype, _callback) in map.iter() {
            assert!(get_devices_changed_callback(context, Scope::Input).is_none());
            assert!(get_devices_changed_callback(context, Scope::Output).is_none());

            // Register callbacks within all scopes.
            for (scope, listener) in map.iter() {
                assert!(context
                    .add_devices_changed_listener(*scope, Some(*listener), ptr::null_mut())
                    .is_ok());
            }

            let input_callback = get_devices_changed_callback(context, Scope::Input);
            assert!(input_callback.is_some());
            assert_eq!(
                input_callback.unwrap(),
                *(map.get(&DeviceType::INPUT).unwrap())
            );
            let output_callback = get_devices_changed_callback(context, Scope::Output);
            assert!(output_callback.is_some());
            assert_eq!(
                output_callback.unwrap(),
                *(map.get(&DeviceType::OUTPUT).unwrap())
            );

            // Unregister the callbacks within one specific scopes.
            assert!(context.remove_devices_changed_listener(*devtype).is_ok());

            if devtype.contains(DeviceType::INPUT) {
                let cb = get_devices_changed_callback(context, Scope::Input);
                assert!(cb.is_none());
            } else {
                let cb = get_devices_changed_callback(context, Scope::Input);
                assert!(cb.is_some());
                assert_eq!(cb.unwrap(), *(map.get(&DeviceType::INPUT).unwrap()));
            }

            if devtype.contains(DeviceType::OUTPUT) {
                let cb = get_devices_changed_callback(context, Scope::Output);
                assert!(cb.is_none());
            } else {
                let cb = get_devices_changed_callback(context, Scope::Output);
                assert!(cb.is_some());
                assert_eq!(cb.unwrap(), *(map.get(&DeviceType::OUTPUT).unwrap()));
            }

            // Unregister the callbacks within all scopes.
            assert!(context
                .remove_devices_changed_listener(DeviceType::INPUT | DeviceType::OUTPUT)
                .is_ok());
        }
    });
}

#[test]
fn test_remove_devices_changed_listener_without_adding_listeners() {
    test_get_raw_context(|context| {
        for devtype in &[
            DeviceType::INPUT,
            DeviceType::OUTPUT,
            DeviceType::INPUT | DeviceType::OUTPUT,
        ] {
            assert!(context.remove_devices_changed_listener(*devtype).is_ok());
        }
    });
}

#[test]
fn test_remove_devices_changed_listener_within_all_scopes() {
    use std::collections::HashMap;

    extern "C" fn inout_callback(_: *mut ffi::cubeb, _: *mut c_void) {}
    extern "C" fn in_callback(_: *mut ffi::cubeb, _: *mut c_void) {}
    extern "C" fn out_callback(_: *mut ffi::cubeb, _: *mut c_void) {}

    let mut map: HashMap<DeviceType, extern "C" fn(*mut ffi::cubeb, *mut c_void)> = HashMap::new();
    map.insert(DeviceType::INPUT, in_callback);
    map.insert(DeviceType::OUTPUT, out_callback);
    map.insert(DeviceType::INPUT | DeviceType::OUTPUT, inout_callback);

    test_get_raw_context(|context| {
        for (devtype, callback) in map.iter() {
            assert!(get_devices_changed_callback(context, Scope::Input).is_none());
            assert!(get_devices_changed_callback(context, Scope::Output).is_none());

            assert!(context
                .add_devices_changed_listener(*devtype, Some(*callback), ptr::null_mut())
                .is_ok());

            if devtype.contains(DeviceType::INPUT) {
                let cb = get_devices_changed_callback(context, Scope::Input);
                assert!(cb.is_some());
                assert_eq!(cb.unwrap(), *callback);
            }

            if devtype.contains(DeviceType::OUTPUT) {
                let cb = get_devices_changed_callback(context, Scope::Output);
                assert!(cb.is_some());
                assert_eq!(cb.unwrap(), *callback);
            }

            assert!(context
                .remove_devices_changed_listener(DeviceType::INPUT | DeviceType::OUTPUT)
                .is_ok());

            assert!(get_devices_changed_callback(context, Scope::Input).is_none());
            assert!(get_devices_changed_callback(context, Scope::Output).is_none());
        }
    });
}

fn get_devices_changed_callback(
    context: &AudioUnitContext,
    scope: Scope,
) -> ffi::cubeb_device_collection_changed_callback {
    let devices_guard = context.devices.lock().unwrap();
    match scope {
        Scope::Input => devices_guard.input.changed_callback,
        Scope::Output => devices_guard.output.changed_callback,
    }
}
