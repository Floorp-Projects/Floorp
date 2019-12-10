use super::utils::{
    test_get_default_device, test_get_default_raw_stream, test_get_devices_in_scope,
    test_ops_stream_operation, Scope, TestDeviceSwitcher,
};
use super::*;

#[ignore]
#[test]
fn test_switch_output_device() {
    use std::f32::consts::PI;
    use std::io;

    const SAMPLE_FREQUENCY: u32 = 48_000;

    // Do nothing if there is no 2 available output devices at least.
    let devices = test_get_devices_in_scope(Scope::Output);
    if devices.len() < 2 {
        println!("Need 2 output devices at least.");
        return;
    }

    let output_device_switcher = TestDeviceSwitcher::new(Scope::Output);

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
                        assert!(output_device_switcher.next().unwrap());
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
fn test_add_then_remove_listeners() {
    extern "C" fn callback(
        id: AudioObjectID,
        number_of_addresses: u32,
        addresses: *const AudioObjectPropertyAddress,
        data: *mut c_void,
    ) -> OSStatus {
        println!("device: {}, data @ {:p}", id, data);
        let addrs = unsafe { std::slice::from_raw_parts(addresses, number_of_addresses as usize) };
        for (i, addr) in addrs.iter().enumerate() {
            let property_selector = PropertySelector::new(addr.mSelector);
            println!(
                "address {}\n\tselector {}({})\n\tscope {}\n\telement {}",
                i, addr.mSelector, property_selector, addr.mScope, addr.mElement
            );
        }

        NO_ERR
    }

    test_get_default_raw_stream(|stream| {
        let mut listeners = Vec::new();

        let default_output_listener = device_property_listener::new(
            kAudioObjectSystemObject,
            get_property_address(
                Property::HardwareDefaultOutputDevice,
                DeviceType::INPUT | DeviceType::OUTPUT,
            ),
            callback,
        );
        listeners.push(default_output_listener);

        let default_input_listener = device_property_listener::new(
            kAudioObjectSystemObject,
            get_property_address(
                Property::HardwareDefaultInputDevice,
                DeviceType::INPUT | DeviceType::OUTPUT,
            ),
            callback,
        );
        listeners.push(default_input_listener);

        if let Some(device) = test_get_default_device(Scope::Output) {
            let output_source_listener = device_property_listener::new(
                device,
                get_property_address(Property::DeviceSource, DeviceType::OUTPUT),
                callback,
            );
            listeners.push(output_source_listener);
        }

        if let Some(device) = test_get_default_device(Scope::Input) {
            let input_source_listener = device_property_listener::new(
                device,
                get_property_address(Property::DeviceSource, DeviceType::INPUT),
                callback,
            );
            listeners.push(input_source_listener);

            let input_alive_listener = device_property_listener::new(
                device,
                get_property_address(
                    Property::DeviceIsAlive,
                    DeviceType::INPUT | DeviceType::OUTPUT,
                ),
                callback,
            );
            listeners.push(input_alive_listener);
        }

        if listeners.is_empty() {
            println!("No listeners to test.");
            return;
        }

        add_listeners(stream, &listeners);

        println!("Unplug/Plug device or switch input/output device to see the event log.\nEnter anything to finish.");
        let mut input = String::new();
        let _ = std::io::stdin().read_line(&mut input);

        remove_listeners(stream, &listeners);
    });

    fn add_listeners(stream: &AudioUnitStream, listeners: &Vec<device_property_listener>) {
        for listener in listeners {
            assert_eq!(stream.add_device_listener(listener), NO_ERR);
        }
    }

    fn remove_listeners(stream: &AudioUnitStream, listeners: &Vec<device_property_listener>) {
        for listener in listeners {
            assert_eq!(stream.remove_device_listener(listener), NO_ERR);
        }
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
