use super::utils::{
    test_get_devices_in_scope, test_ops_context_operation, test_ops_stream_operation, Scope,
    StreamType, TestDeviceInfo, TestDeviceSwitcher,
};
use super::*;
use std::io;
use std::sync::atomic::AtomicBool;

#[ignore]
#[test]
fn test_switch_output_device() {
    use std::f32::consts::PI;

    const SAMPLE_FREQUENCY: u32 = 48_000;

    // Do nothing if there is no 2 available output devices at least.
    let devices = test_get_devices_in_scope(Scope::Output);
    if devices.len() < 2 {
        println!("Need 2 output devices at least.");
        return;
    }

    let mut output_device_switcher = TestDeviceSwitcher::new(Scope::Output);

    // Make sure the parameters meet the requirements of AudioUnitContext::stream_init
    // (in the comments).
    let mut output_params = ffi::cubeb_stream_params::default();
    output_params.format = ffi::CUBEB_SAMPLE_S16NE;
    output_params.rate = SAMPLE_FREQUENCY;
    output_params.channels = 1;
    output_params.layout = ffi::CUBEB_LAYOUT_MONO;
    output_params.prefs = ffi::CUBEB_STREAM_PREF_NONE;

    // Used to calculate the tone's wave.
    let mut position: i64 = 0; // TODO: Use Atomic instead.

    test_ops_stream_operation(
        "stream: North American dial tone",
        ptr::null_mut(), // Use default input device.
        ptr::null_mut(), // No input parameters.
        ptr::null_mut(), // Use default output device.
        &mut output_params,
        4096, // TODO: Get latency by get_min_latency instead ?
        Some(data_callback),
        Some(state_callback),
        &mut position as *mut i64 as *mut c_void,
        |stream| {
            assert_eq!(unsafe { OPS.stream_start.unwrap()(stream) }, ffi::CUBEB_OK);
            println!("Start playing! Enter 's' to switch device. Enter 'q' to quit.");
            loop {
                let mut input = String::new();
                let _ = io::stdin().read_line(&mut input);
                assert_eq!(input.pop().unwrap(), '\n');
                match input.as_str() {
                    "s" => {
                        output_device_switcher.next();
                    }
                    "q" => {
                        println!("Quit.");
                        break;
                    }
                    x => {
                        println!("Unknown command: {}", x);
                    }
                }
            }
            assert_eq!(unsafe { OPS.stream_stop.unwrap()(stream) }, ffi::CUBEB_OK);
        },
    );

    extern "C" fn state_callback(
        stream: *mut ffi::cubeb_stream,
        user_ptr: *mut c_void,
        state: ffi::cubeb_state,
    ) {
        assert!(!stream.is_null());
        assert!(!user_ptr.is_null());
        assert_ne!(state, ffi::CUBEB_STATE_ERROR);
    }

    extern "C" fn data_callback(
        stream: *mut ffi::cubeb_stream,
        user_ptr: *mut c_void,
        _input_buffer: *const c_void,
        output_buffer: *mut c_void,
        nframes: i64,
    ) -> i64 {
        assert!(!stream.is_null());
        assert!(!user_ptr.is_null());
        assert!(!output_buffer.is_null());

        let buffer = unsafe {
            let ptr = output_buffer as *mut i16;
            let len = nframes as usize;
            slice::from_raw_parts_mut(ptr, len)
        };

        let position = unsafe { &mut *(user_ptr as *mut i64) };

        // Generate tone on the fly.
        for data in buffer.iter_mut() {
            let t1 = (2.0 * PI * 350.0 * (*position) as f32 / SAMPLE_FREQUENCY as f32).sin();
            let t2 = (2.0 * PI * 440.0 * (*position) as f32 / SAMPLE_FREQUENCY as f32).sin();
            *data = f32_to_i16_sample(0.5 * (t1 + t2));
            *position += 1;
        }

        nframes
    }

    fn f32_to_i16_sample(x: f32) -> i16 {
        (x * f32::from(i16::max_value())) as i16
    }
}

#[ignore]
#[test]
fn test_device_collection_change() {
    const DUMMY_PTR: *mut c_void = 0xDEAD_BEEF as *mut c_void;
    let mut context = AudioUnitContext::new();
    println!("Context allocated @ {:p}", &context);

    extern "C" fn input_changed_callback(context: *mut ffi::cubeb, data: *mut c_void) {
        println!(
            "Input device collection @ {:p} is changed. Data @ {:p}",
            context, data
        );
        assert_eq!(data, DUMMY_PTR);
    }

    extern "C" fn output_changed_callback(context: *mut ffi::cubeb, data: *mut c_void) {
        println!(
            "output device collection @ {:p} is changed. Data @ {:p}",
            context, data
        );
        assert_eq!(data, DUMMY_PTR);
    }

    context.register_device_collection_changed(
        DeviceType::INPUT,
        Some(input_changed_callback),
        DUMMY_PTR,
    );

    context.register_device_collection_changed(
        DeviceType::OUTPUT,
        Some(output_changed_callback),
        DUMMY_PTR,
    );

    println!("Unplug/Plug device to see the event log.\nEnter anything to finish.");
    let mut input = String::new();
    let _ = std::io::stdin().read_line(&mut input);
}

#[ignore]
#[test]
fn test_stream_tester() {
    test_ops_context_operation("context: stream tester", |context_ptr| {
        let mut stream_ptr: *mut ffi::cubeb_stream = ptr::null_mut();
        let enable_loopback = AtomicBool::new(false);
        loop {
            println!(
                "commands:\n\
                 \t'q': quit\n\
                 \t'c': create a stream\n\
                 \t'd': destroy a stream\n\
                 \t's': start the created stream\n\
                 \t't': stop the created stream\n\
                 \t'r': register a device changed callback\n\
                 \t'l': set loopback (DUPLEX-only)\n\
                 \t'v': set volume\n\
                 \t'm': set input mute\n\
                 \t'p': set input processing"
            );

            let mut command = String::new();
            let _ = io::stdin().read_line(&mut command);
            assert_eq!(command.pop().unwrap(), '\n');

            match command.as_str() {
                "q" => {
                    println!("Quit.");
                    destroy_stream(&mut stream_ptr);
                    break;
                }
                "c" => create_stream(&mut stream_ptr, context_ptr, &enable_loopback),
                "d" => destroy_stream(&mut stream_ptr),
                "s" => start_stream(stream_ptr),
                "t" => stop_stream(stream_ptr),
                "r" => register_device_change_callback(stream_ptr),
                "l" => set_loopback(stream_ptr, &enable_loopback),
                "v" => set_volume(stream_ptr),
                "m" => set_input_mute(stream_ptr),
                "p" => set_input_processing(stream_ptr),
                x => println!("Unknown command: {}", x),
            }
        }
    });

    fn start_stream(stream_ptr: *mut ffi::cubeb_stream) {
        if stream_ptr.is_null() {
            println!("No stream can start.");
            return;
        }
        assert_eq!(
            unsafe { OPS.stream_start.unwrap()(stream_ptr) },
            ffi::CUBEB_OK
        );
        println!("Stream {:p} started.", stream_ptr);
    }

    fn stop_stream(stream_ptr: *mut ffi::cubeb_stream) {
        if stream_ptr.is_null() {
            println!("No stream can stop.");
            return;
        }
        assert_eq!(
            unsafe { OPS.stream_stop.unwrap()(stream_ptr) },
            ffi::CUBEB_OK
        );
        println!("Stream {:p} stopped.", stream_ptr);
    }

    fn set_volume(stream_ptr: *mut ffi::cubeb_stream) {
        if stream_ptr.is_null() {
            println!("No stream can set volume.");
            return;
        }
        const VOL: f32 = 0.5;
        assert_eq!(
            unsafe { OPS.stream_set_volume.unwrap()(stream_ptr, VOL) },
            ffi::CUBEB_OK
        );
        println!("Set stream {:p} volume to {}", stream_ptr, VOL);
    }

    fn set_loopback(stream_ptr: *mut ffi::cubeb_stream, enable_loopback: &AtomicBool) {
        if stream_ptr.is_null() {
            println!("No stream can set loopback.");
            return;
        }
        let stm = unsafe { &mut *(stream_ptr as *mut AudioUnitStream) };
        if !stm.core_stream_data.has_input() || !stm.core_stream_data.has_output() {
            println!("Duplex stream needed to set loopback");
            return;
        }
        let mut loopback: Option<bool> = None;
        while loopback.is_none() {
            println!("Select action:\n1) Enable loopback, 2) Disable loopback");
            let mut input = String::new();
            let _ = io::stdin().read_line(&mut input);
            assert_eq!(input.pop().unwrap(), '\n');
            loopback = match input.as_str() {
                "1" => Some(true),
                "2" => Some(false),
                _ => {
                    println!("Invalid action. Select again.\n");
                    None
                }
            }
        }
        let loopback = loopback.unwrap();
        enable_loopback.store(loopback, Ordering::SeqCst);
        println!(
            "Loopback {} for stream {:p}",
            if loopback { "enabled" } else { "disabled" },
            stream_ptr
        );
    }

    fn set_input_mute(stream_ptr: *mut ffi::cubeb_stream) {
        if stream_ptr.is_null() {
            println!("No stream can set input mute.");
            return;
        }
        let stm = unsafe { &mut *(stream_ptr as *mut AudioUnitStream) };
        if !stm.core_stream_data.has_input() {
            println!("Input stream needed to set loopback");
            return;
        }
        let mut mute: Option<bool> = None;
        while mute.is_none() {
            println!("Select action:\n1) Mute, 2) Unmute");
            let mut input = String::new();
            let _ = io::stdin().read_line(&mut input);
            assert_eq!(input.pop().unwrap(), '\n');
            mute = match input.as_str() {
                "1" => Some(true),
                "2" => Some(false),
                _ => {
                    println!("Invalid action. Select again.\n");
                    None
                }
            }
        }
        let mute = mute.unwrap();
        let res = unsafe { OPS.stream_set_input_mute.unwrap()(stream_ptr, mute.into()) };
        println!(
            "{} set stream {:p} input {}",
            if res == ffi::CUBEB_OK {
                "Successfully"
            } else {
                "Failed to"
            },
            stream_ptr,
            if mute { "mute" } else { "unmute" }
        );
    }

    fn set_input_processing(stream_ptr: *mut ffi::cubeb_stream) {
        if stream_ptr.is_null() {
            println!("No stream can set input processing.");
            return;
        }
        let stm = unsafe { &mut *(stream_ptr as *mut AudioUnitStream) };
        if !stm.core_stream_data.using_voice_processing_unit() {
            println!("Duplex stream with voice processing needed to set input processing params");
            return;
        }
        let mut params = InputProcessingParams::NONE;
        {
            let mut bypass = u32::from(true);
            let mut size: usize = mem::size_of::<u32>();
            assert_eq!(
                audio_unit_get_property(
                    stm.core_stream_data.input_unit,
                    kAudioUnitProperty_BypassEffect,
                    kAudioUnitScope_Global,
                    AU_IN_BUS,
                    &mut bypass,
                    &mut size,
                ),
                NO_ERR
            );
            assert_eq!(size, mem::size_of::<u32>());
            if bypass == 0 {
                params.set(InputProcessingParams::ECHO_CANCELLATION, true);
                params.set(InputProcessingParams::NOISE_SUPPRESSION, true);
            }
            let mut agc = u32::from(false);
            let mut size: usize = mem::size_of::<u32>();
            assert_eq!(
                audio_unit_get_property(
                    stm.core_stream_data.input_unit,
                    kAUVoiceIOProperty_VoiceProcessingEnableAGC,
                    kAudioUnitScope_Global,
                    AU_IN_BUS,
                    &mut agc,
                    &mut size,
                ),
                NO_ERR
            );
            assert_eq!(size, mem::size_of::<u32>());
            if agc == 1 {
                params.set(InputProcessingParams::AUTOMATIC_GAIN_CONTROL, true);
            }
        }
        let mut done = false;
        while !done {
            println!(
                "Supported params: {:?}\nCurrent params: {:?}\nSelect action:\n\
                 \t1) Set None\n\
                 \t2) Toggle Echo Cancellation\n\
                 \t3) Toggle Noise Suppression\n\
                 \t4) Toggle Automatic Gain Control\n\
                 \t5) Toggle Voice Isolation\n\
                 \t6) Set All\n\
                 \t0) Done",
                stm.context.supported_input_processing_params().unwrap(),
                params
            );
            let mut input = String::new();
            let _ = io::stdin().read_line(&mut input);
            assert_eq!(input.pop().unwrap(), '\n');
            match input.as_str() {
                "1" => params = InputProcessingParams::NONE,
                "2" => params.toggle(InputProcessingParams::ECHO_CANCELLATION),
                "3" => params.toggle(InputProcessingParams::NOISE_SUPPRESSION),
                "4" => params.toggle(InputProcessingParams::AUTOMATIC_GAIN_CONTROL),
                "5" => params.toggle(InputProcessingParams::VOICE_ISOLATION),
                "6" => params = InputProcessingParams::all(),
                "0" => done = true,
                _ => println!("Invalid action. Select again.\n"),
            }
        }
        let res =
            unsafe { OPS.stream_set_input_processing_params.unwrap()(stream_ptr, params.bits()) };
        println!(
            "{} set stream {:p} input processing params to {:?}",
            if res == ffi::CUBEB_OK {
                "Successfully"
            } else {
                "Failed to"
            },
            stream_ptr,
            params,
        );
    }

    fn register_device_change_callback(stream_ptr: *mut ffi::cubeb_stream) {
        extern "C" fn callback(user_ptr: *mut c_void) {
            println!("user pointer @ {:p}", user_ptr);
            assert!(user_ptr.is_null());
        }

        if stream_ptr.is_null() {
            println!("No stream for registering the callback.");
            return;
        }
        assert_eq!(
            unsafe {
                OPS.stream_register_device_changed_callback.unwrap()(stream_ptr, Some(callback))
            },
            ffi::CUBEB_OK
        );
        println!("Stream {:p} now has a device change callback.", stream_ptr);
    }

    fn destroy_stream(stream_ptr: &mut *mut ffi::cubeb_stream) {
        if stream_ptr.is_null() {
            println!("No need to destroy stream.");
            return;
        }
        unsafe {
            OPS.stream_destroy.unwrap()(*stream_ptr);
        }
        println!("Stream {:p} destroyed.", *stream_ptr);
        *stream_ptr = ptr::null_mut();
    }

    fn create_stream(
        stream_ptr: &mut *mut ffi::cubeb_stream,
        context_ptr: *mut ffi::cubeb,
        enable_loopback: &AtomicBool,
    ) {
        if !stream_ptr.is_null() {
            println!("Stream has been created.");
            return;
        }

        let mut stream_type = StreamType::empty();
        while stream_type.is_empty() {
            println!("Select stream type:\n1) Input 2) Output 3) In-Out Duplex 4) Back");
            let mut input = String::new();
            let _ = io::stdin().read_line(&mut input);
            assert_eq!(input.pop().unwrap(), '\n');
            stream_type = match input.as_str() {
                "1" => StreamType::INPUT,
                "2" => StreamType::OUTPUT,
                "3" => StreamType::DUPLEX,
                "4" => {
                    println!("Do nothing.");
                    return;
                }
                _ => {
                    println!("Invalid type. Select again.\n");
                    StreamType::empty()
                }
            }
        }

        let device_selector = |scope: Scope| -> AudioObjectID {
            loop {
                println!(
                    "Select {} device:\n",
                    if scope == Scope::Input {
                        "input"
                    } else {
                        "output"
                    }
                );
                let mut list = vec![];
                list.push(kAudioObjectUnknown);
                println!("{:>4}: System default", 0);
                let devices = test_get_devices_in_scope(scope.clone());
                for (idx, device) in devices.iter().enumerate() {
                    list.push(*device);
                    let info = TestDeviceInfo::new(*device, scope.clone());
                    println!(
                        "{:>4}: {}\n\tAudioObjectID: {}\n\tuid: {}",
                        idx + 1,
                        info.label,
                        device,
                        info.uid
                    );
                }

                let mut input = String::new();
                io::stdin().read_line(&mut input).unwrap();
                let n: usize = match input.trim().parse() {
                    Err(_) => {
                        println!("Invalid option. Try again.\n");
                        continue;
                    }
                    Ok(n) => n,
                };
                if n >= list.len() {
                    println!("Invalid option. Try again.\n");
                    continue;
                }
                return list[n];
            }
        };

        let mut input_params = get_dummy_stream_params(Scope::Input);
        let mut output_params = get_dummy_stream_params(Scope::Output);

        let (input_device, input_stream_params) = if stream_type.contains(StreamType::INPUT) {
            (
                device_selector(Scope::Input),
                &mut input_params as *mut ffi::cubeb_stream_params,
            )
        } else {
            (
                kAudioObjectUnknown, /* default input device */
                ptr::null_mut(),
            )
        };

        let (output_device, output_stream_params) = if stream_type.contains(StreamType::OUTPUT) {
            (
                device_selector(Scope::Output),
                &mut output_params as *mut ffi::cubeb_stream_params,
            )
        } else {
            (
                kAudioObjectUnknown, /* default output device */
                ptr::null_mut(),
            )
        };

        let stream_name = CString::new("stream tester").unwrap();

        assert_eq!(
            unsafe {
                OPS.stream_init.unwrap()(
                    context_ptr,
                    stream_ptr,
                    stream_name.as_ptr(),
                    input_device as ffi::cubeb_devid,
                    input_stream_params,
                    output_device as ffi::cubeb_devid,
                    output_stream_params,
                    4096, // latency
                    Some(data_callback),
                    Some(state_callback),
                    enable_loopback as *const AtomicBool as *mut c_void, // user pointer
                )
            },
            ffi::CUBEB_OK
        );
        assert!(!stream_ptr.is_null());
        println!("Stream {:p} created.", *stream_ptr);

        extern "C" fn state_callback(
            stream: *mut ffi::cubeb_stream,
            _user_ptr: *mut c_void,
            state: ffi::cubeb_state,
        ) {
            assert!(!stream.is_null());
            let s = State::from(state);
            println!("state: {:?}", s);
        }

        extern "C" fn data_callback(
            stream: *mut ffi::cubeb_stream,
            user_ptr: *mut c_void,
            input_buffer: *const c_void,
            output_buffer: *mut c_void,
            nframes: i64,
        ) -> i64 {
            assert!(!stream.is_null());

            let enable_loopback = unsafe { &mut *(user_ptr as *mut AtomicBool) };
            let loopback = enable_loopback.load(Ordering::SeqCst);
            if loopback && !input_buffer.is_null() && !output_buffer.is_null() {
                // Dupe the mono input to stereo
                let stm = unsafe { &mut *(stream as *mut AudioUnitStream) };
                assert_eq!(stm.core_stream_data.input_stream_params.channels(), 1);
                let channels = stm.core_stream_data.output_stream_params.channels() as usize;
                let sample_size =
                    cubeb_sample_size(stm.core_stream_data.output_stream_params.format());
                for f in 0..(nframes as usize) {
                    let input_offset = f * sample_size;
                    let output_offset = input_offset * channels;
                    for c in 0..channels {
                        unsafe {
                            ptr::copy(
                                input_buffer.add(input_offset) as *const u8,
                                output_buffer.add(output_offset + (sample_size * c)) as *mut u8,
                                sample_size,
                            )
                        };
                    }
                }
            } else if !output_buffer.is_null() {
                // Feed silence data to output buffer
                let stm = unsafe { &mut *(stream as *mut AudioUnitStream) };
                let channels = stm.core_stream_data.output_stream_params.channels();
                let samples = nframes as usize * channels as usize;
                let sample_size =
                    cubeb_sample_size(stm.core_stream_data.output_stream_params.format());
                unsafe {
                    ptr::write_bytes(output_buffer, 0, samples * sample_size);
                }
            }

            nframes
        }

        fn get_dummy_stream_params(scope: Scope) -> ffi::cubeb_stream_params {
            // The stream format for input and output must be same.
            const STREAM_FORMAT: u32 = ffi::CUBEB_SAMPLE_FLOAT32NE;

            // Make sure the parameters meet the requirements of AudioUnitContext::stream_init
            // (in the comments).
            let mut stream_params = ffi::cubeb_stream_params::default();
            stream_params.prefs = ffi::CUBEB_STREAM_PREF_VOICE;
            let (format, rate, channels, layout) = match scope {
                Scope::Input => (STREAM_FORMAT, 48000, 1, ffi::CUBEB_LAYOUT_MONO),
                Scope::Output => (STREAM_FORMAT, 44100, 2, ffi::CUBEB_LAYOUT_STEREO),
            };
            stream_params.format = format;
            stream_params.rate = rate;
            stream_params.channels = channels;
            stream_params.layout = layout;
            stream_params
        }
    }
}
