// NOTICE:
// Avoid running TestDeviceSwitcher with TestDevicePlugger or active full-duplex streams
// sequentially!
//
// The TestDeviceSwitcher cannot work with any test that will create an aggregate device that is
// soon being destroyed. The TestDeviceSwitcher will cache the available devices, upon it's
// created, as the candidates for the default device. Therefore, those created aggregate devices
// may be cached in TestDeviceSwitcher. However, those aggregate devices may be destroyed when
// TestDeviceSwitcher is using them or they are in the cached list of TestDeviceSwitcher.
//
// Running those tests by setting `test-threads=1` doesn't really help (e.g.,
// `cargo test test_register_device_changed_callback -- --ignored --nocapture --test-threads=1`).
// The aggregate device won't be destroyed immediately when `kAudioPlugInDestroyAggregateDevice`
// is set. As a result, the following tests requiring changing the devices will be run separately
// in the run_tests.sh script and marked by `ignore` by default.

use super::utils::{
    get_devices_info_in_scope, test_create_device_change_listener, test_device_in_scope,
    test_get_default_device, test_get_devices_in_scope,
    test_get_stream_with_default_data_callback_by_type, test_ops_stream_operation,
    test_set_default_device, Scope, StreamType, TestDevicePlugger, TestDeviceSwitcher,
};
use super::*;
use std::fmt::Debug;
use std::thread;

// Switch default devices used by the active streams, to test stream reinitialization
// ================================================================================================
#[ignore]
#[test]
fn test_switch_device() {
    test_switch_device_in_scope(Scope::Input);
    test_switch_device_in_scope(Scope::Output);
}

fn test_switch_device_in_scope(scope: Scope) {
    println!(
        "Switch default device for {:?} while the stream is working.",
        scope
    );

    // Do nothing if there is no 2 available devices at least.
    let devices = test_get_devices_in_scope(scope.clone());
    if devices.len() < 2 {
        println!("Need 2 devices for {:?} at least. Skip.", scope);
        return;
    }

    let mut device_switcher = TestDeviceSwitcher::new(scope.clone());

    let count = Arc::new(Mutex::new(0));
    let also_count = Arc::clone(&count);
    let listener = test_create_device_change_listener(scope.clone(), move |_addresses| {
        let mut cnt = also_count.lock().unwrap();
        *cnt += 1;
        NO_ERR
    });
    listener.start();

    let mut changed_watcher = Watcher::new(&count);
    test_get_started_stream_in_scope(scope.clone(), move |_stream| loop {
        thread::sleep(Duration::from_millis(500));
        changed_watcher.prepare();
        device_switcher.next();
        changed_watcher.wait_for_change();
        if changed_watcher.current_result() >= devices.len() {
            break;
        }
    });
}

fn test_get_started_stream_in_scope<F>(scope: Scope, operation: F)
where
    F: FnOnce(*mut ffi::cubeb_stream),
{
    use std::f32::consts::PI;
    const SAMPLE_FREQUENCY: u32 = 48_000;

    // Make sure the parameters meet the requirements of AudioUnitContext::stream_init
    // (in the comments).
    let mut stream_params = ffi::cubeb_stream_params::default();
    stream_params.format = ffi::CUBEB_SAMPLE_S16NE;
    stream_params.rate = SAMPLE_FREQUENCY;
    stream_params.prefs = ffi::CUBEB_STREAM_PREF_NONE;
    stream_params.channels = 1;
    stream_params.layout = ffi::CUBEB_LAYOUT_MONO;

    let (input_params, output_params) = match scope {
        Scope::Input => (
            &mut stream_params as *mut ffi::cubeb_stream_params,
            ptr::null_mut(),
        ),
        Scope::Output => (
            ptr::null_mut(),
            &mut stream_params as *mut ffi::cubeb_stream_params,
        ),
    };

    extern "C" fn state_callback(
        stream: *mut ffi::cubeb_stream,
        user_ptr: *mut c_void,
        state: ffi::cubeb_state,
    ) {
        assert!(!stream.is_null());
        assert!(!user_ptr.is_null());
        assert_ne!(state, ffi::CUBEB_STATE_ERROR);
    }

    extern "C" fn input_data_callback(
        stream: *mut ffi::cubeb_stream,
        user_ptr: *mut c_void,
        input_buffer: *const c_void,
        output_buffer: *mut c_void,
        nframes: i64,
    ) -> i64 {
        assert!(!stream.is_null());
        assert!(!user_ptr.is_null());
        assert!(!input_buffer.is_null());
        assert!(output_buffer.is_null());
        nframes
    }

    let mut position: i64 = 0; // TODO: Use Atomic instead.

    fn f32_to_i16_sample(x: f32) -> i16 {
        (x * f32::from(i16::max_value())) as i16
    }

    extern "C" fn output_data_callback(
        stream: *mut ffi::cubeb_stream,
        user_ptr: *mut c_void,
        input_buffer: *const c_void,
        output_buffer: *mut c_void,
        nframes: i64,
    ) -> i64 {
        assert!(!stream.is_null());
        assert!(!user_ptr.is_null());
        assert!(input_buffer.is_null());
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

    test_ops_stream_operation(
        "stream",
        ptr::null_mut(), // Use default input device.
        input_params,
        ptr::null_mut(), // Use default output device.
        output_params,
        4096, // TODO: Get latency by get_min_latency instead ?
        match scope {
            Scope::Input => Some(input_data_callback),
            Scope::Output => Some(output_data_callback),
        },
        Some(state_callback),
        &mut position as *mut i64 as *mut c_void,
        |stream| {
            assert_eq!(unsafe { OPS.stream_start.unwrap()(stream) }, ffi::CUBEB_OK);
            operation(stream);
            assert_eq!(unsafe { OPS.stream_stop.unwrap()(stream) }, ffi::CUBEB_OK);
        },
    );
}

// Plug and unplug devices, to test device collection changed callback
// ================================================================================================
#[ignore]
#[test]
fn test_plug_and_unplug_device() {
    test_plug_and_unplug_device_in_scope(Scope::Input);
    test_plug_and_unplug_device_in_scope(Scope::Output);
}

fn test_plug_and_unplug_device_in_scope(scope: Scope) {
    let default_device = test_get_default_device(scope.clone());
    if default_device.is_none() {
        println!("No device for {:?} to test", scope);
        return;
    }

    println!("Run test for {:?}", scope);
    println!("NOTICE: The test will hang if the default input or output is an aggregate device.\nWe will fix this later.");

    let default_device = default_device.unwrap();
    let is_input = test_device_in_scope(default_device, Scope::Input);
    let is_output = test_device_in_scope(default_device, Scope::Output);

    let mut context = AudioUnitContext::new();

    // Register the devices-changed callbacks.
    let input_count = Arc::new(Mutex::new(0u32));
    let also_input_count = Arc::clone(&input_count);
    let input_mtx_ptr = also_input_count.as_ref() as *const Mutex<u32>;

    assert!(context
        .register_device_collection_changed(
            DeviceType::INPUT,
            Some(input_changed_callback),
            input_mtx_ptr as *mut c_void,
        )
        .is_ok());

    let output_count = Arc::new(Mutex::new(0u32));
    let also_output_count = Arc::clone(&output_count);
    let output_mtx_ptr = also_output_count.as_ref() as *const Mutex<u32>;

    assert!(context
        .register_device_collection_changed(
            DeviceType::OUTPUT,
            Some(output_changed_callback),
            output_mtx_ptr as *mut c_void,
        )
        .is_ok());

    let mut input_watcher = Watcher::new(&input_count);
    let mut output_watcher = Watcher::new(&output_count);

    let mut device_plugger = TestDevicePlugger::new(scope).unwrap();

    // Simulate adding devices and monitor the devices-changed callbacks.
    input_watcher.prepare();
    output_watcher.prepare();

    assert!(device_plugger.plug().is_ok());

    if is_input {
        input_watcher.wait_for_change();
    }
    if is_output {
        output_watcher.wait_for_change();
    }

    // Check changed count.
    check_result(is_input, (1, 0), &input_watcher);
    check_result(is_output, (1, 0), &output_watcher);

    // Simulate removing devices and monitor the devices-changed callbacks.
    input_watcher.prepare();
    output_watcher.prepare();

    assert!(device_plugger.unplug().is_ok());

    if is_input {
        input_watcher.wait_for_change();
    }
    if is_output {
        output_watcher.wait_for_change();
    }

    check_result(is_input, (2, 0), &input_watcher);
    check_result(is_output, (2, 0), &output_watcher);

    extern "C" fn input_changed_callback(context: *mut ffi::cubeb, data: *mut c_void) {
        println!(
            "Input device collection @ {:p} is changed. Data @ {:p}",
            context, data
        );
        let count = unsafe { &*(data as *const Mutex<u32>) };
        {
            let mut guard = count.lock().unwrap();
            *guard += 1;
        }
    }

    extern "C" fn output_changed_callback(context: *mut ffi::cubeb, data: *mut c_void) {
        println!(
            "output device collection @ {:p} is changed. Data @ {:p}",
            context, data
        );
        let count = unsafe { &*(data as *const Mutex<u32>) };
        {
            let mut guard = count.lock().unwrap();
            *guard += 1;
        }
    }

    fn check_result<T: Clone + Debug + PartialEq>(
        in_scope: bool,
        expected: (T, T),
        watcher: &Watcher<T>,
    ) {
        assert_eq!(
            watcher.current_result(),
            if in_scope { expected.0 } else { expected.1 }
        );
    }
}

// Switch default devices used by the active streams, to test device changed callback
// ================================================================================================
#[ignore]
#[test]
fn test_register_device_changed_callback_to_check_default_device_changed_input() {
    test_register_device_changed_callback_to_check_default_device_changed(StreamType::INPUT);
}

#[ignore]
#[test]
fn test_register_device_changed_callback_to_check_default_device_changed_output() {
    test_register_device_changed_callback_to_check_default_device_changed(StreamType::OUTPUT);
}

#[ignore]
#[test]
fn test_register_device_changed_callback_to_check_default_device_changed_duplex() {
    test_register_device_changed_callback_to_check_default_device_changed(StreamType::DUPLEX);
}

fn test_register_device_changed_callback_to_check_default_device_changed(stm_type: StreamType) {
    println!("NOTICE: The test will hang if the default input or output is an aggregate device.\nWe will fix this later.");

    let inputs = if stm_type.contains(StreamType::INPUT) {
        let devices = test_get_devices_in_scope(Scope::Input).len();
        if devices >= 2 {
            Some(devices)
        } else {
            None
        }
    } else {
        None
    };

    let outputs = if stm_type.contains(StreamType::OUTPUT) {
        let devices = test_get_devices_in_scope(Scope::Output).len();
        if devices >= 2 {
            Some(devices)
        } else {
            None
        }
    } else {
        None
    };

    if inputs.is_none() && outputs.is_none() {
        println!("No enough devices to run the test!");
        return;
    }

    let changed_count = Arc::new(Mutex::new(0u32));
    let also_changed_count = Arc::clone(&changed_count);
    let mtx_ptr = also_changed_count.as_ref() as *const Mutex<u32>;

    test_get_stream_with_device_changed_callback(
        "stream: test callback for default device changed",
        stm_type,
        None, // Use default input device.
        None, // Use default output device.
        mtx_ptr as *mut c_void,
        state_callback,
        device_changed_callback,
        |stream| {
            // If the duplex stream uses different input and output device,
            // an aggregate device will be created and it will work for this duplex stream.
            // This aggregate device will be added into the device list, but it won't
            // be assigned to the default device, since the device list for setting
            // default device is cached upon {input, output}_device_switcher is initialized.

            let mut changed_watcher = Watcher::new(&changed_count);

            if let Some(devices) = inputs {
                let mut device_switcher = TestDeviceSwitcher::new(Scope::Input);
                for _ in 0..devices {
                    // While the stream is re-initializing for the default device switch,
                    // switching for the default device again will be ignored.
                    while stream.switching_device.load(atomic::Ordering::SeqCst) {}
                    changed_watcher.prepare();
                    device_switcher.next();
                    changed_watcher.wait_for_change();
                }
            }

            if let Some(devices) = outputs {
                let mut device_switcher = TestDeviceSwitcher::new(Scope::Output);
                for _ in 0..devices {
                    // While the stream is re-initializing for the default device switch,
                    // switching for the default device again will be ignored.
                    while stream.switching_device.load(atomic::Ordering::SeqCst) {}
                    changed_watcher.prepare();
                    device_switcher.next();
                    changed_watcher.wait_for_change();
                }
            }
        },
    );

    extern "C" fn state_callback(
        stream: *mut ffi::cubeb_stream,
        _user_ptr: *mut c_void,
        state: ffi::cubeb_state,
    ) {
        assert!(!stream.is_null());
        assert_ne!(state, ffi::CUBEB_STATE_ERROR);
    }

    extern "C" fn device_changed_callback(data: *mut c_void) {
        println!("Device change callback. data @ {:p}", data);
        let count = unsafe { &*(data as *const Mutex<u32>) };
        {
            let mut guard = count.lock().unwrap();
            *guard += 1;
        }
    }
}

// Unplug the devices used by the active streams, to test
// 1) device changed callback, or state callback
// 2) stream reinitialization that may race with stream destroying
// ================================================================================================

// Input-only stream
// -----------------

// Unplug the non-default input device for an input stream
// ------------------------------------------------------------------------------------------------

#[ignore]
#[test]
fn test_destroy_input_stream_after_unplugging_a_nondefault_input_device() {
    // The stream can be destroyed before running device-changed event handler
    test_unplug_a_device_on_an_active_stream(StreamType::INPUT, Scope::Input, false, 0);
}

#[ignore]
#[test]
fn test_suspend_input_stream_by_unplugging_a_nondefault_input_device() {
    // Expect to get an error state callback by device-changed event handler
    test_unplug_a_device_on_an_active_stream(StreamType::INPUT, Scope::Input, false, 500);
}

// Unplug the default input device for an input stream
// ------------------------------------------------------------------------------------------------
#[ignore]
#[test]
fn test_destroy_input_stream_after_unplugging_a_default_input_device() {
    // Expect to get an device-changed callback by device-changed event handler,
    // which will reinitialize the stream behind the scenes, at the same when
    // the stream is being destroyed
    test_unplug_a_device_on_an_active_stream(StreamType::INPUT, Scope::Input, true, 0);
}

#[ignore]
#[test]
fn test_reinit_input_stream_by_unplugging_a_default_input_device() {
    // Expect to get an device-changed callback by device-changed event handler,
    // which will reinitialize the stream behind the scenes
    test_unplug_a_device_on_an_active_stream(StreamType::INPUT, Scope::Input, true, 500);
}

// Output-only stream
// ------------------

// Unplug the non-default output device for an output stream
// ------------------------------------------------------------------------------------------------
// FIXME: We don't monitor the alive-status for output device currently
#[ignore]
#[test]
fn test_destroy_output_stream_after_unplugging_a_nondefault_output_device() {
    test_unplug_a_device_on_an_active_stream(StreamType::OUTPUT, Scope::Output, false, 0);
}

// FIXME: We don't monitor the alive-status for output device currently
#[ignore]
#[test]
fn test_suspend_output_stream_by_unplugging_a_nondefault_output_device() {
    test_unplug_a_device_on_an_active_stream(StreamType::OUTPUT, Scope::Output, false, 500);
}

// Unplug the default output device for an output stream
// ------------------------------------------------------------------------------------------------

#[ignore]
#[test]
fn test_destroy_output_stream_after_unplugging_a_default_output_device() {
    // Expect to get an device-changed callback by device-changed event handler,
    // which will reinitialize the stream behind the scenes, at the same when
    // the stream is being destroyed
    test_unplug_a_device_on_an_active_stream(StreamType::OUTPUT, Scope::Output, true, 0);
}

#[ignore]
#[test]
fn test_reinit_output_stream_by_unplugging_a_default_output_device() {
    // Expect to get an device-changed callback by device-changed event handler,
    // which will reinitialize the stream behind the scenes
    test_unplug_a_device_on_an_active_stream(StreamType::OUTPUT, Scope::Output, true, 500);
}

// Duplex stream
// -------------

// Unplug the non-default input device for a duplex stream
// ------------------------------------------------------------------------------------------------

#[ignore]
#[test]
fn test_destroy_duplex_stream_after_unplugging_a_nondefault_input_device() {
    // The stream can be destroyed before running device-changed event handler
    test_unplug_a_device_on_an_active_stream(StreamType::DUPLEX, Scope::Input, false, 0);
}

#[ignore]
#[test]
fn test_suspend_duplex_stream_by_unplugging_a_nondefault_input_device() {
    // Expect to get an error state callback by device-changed event handler
    test_unplug_a_device_on_an_active_stream(StreamType::DUPLEX, Scope::Input, false, 500);
}

// Unplug the non-default output device for a duplex stream
// ------------------------------------------------------------------------------------------------

// FIXME: We don't monitor the alive-status for output device currently
#[ignore]
#[test]
fn test_destroy_duplex_stream_after_unplugging_a_nondefault_output_device() {
    test_unplug_a_device_on_an_active_stream(StreamType::DUPLEX, Scope::Output, false, 0);
}

// FIXME: We don't monitor the alive-status for output device currently
#[ignore]
#[test]
fn test_suspend_duplex_stream_by_unplugging_a_nondefault_output_device() {
    test_unplug_a_device_on_an_active_stream(StreamType::DUPLEX, Scope::Output, false, 500);
}

// Unplug the non-default in-out device for a duplex stream
// ------------------------------------------------------------------------------------------------
// TODO: Implement an in-out TestDevicePlugger

// Unplug the default input device for a duplex stream
// ------------------------------------------------------------------------------------------------

#[ignore]
#[test]
fn test_destroy_duplex_stream_after_unplugging_a_default_input_device() {
    // Expect to get an device-changed callback by device-changed event handler,
    // which will reinitialize the stream behind the scenes, at the same when
    // the stream is being destroyed
    test_unplug_a_device_on_an_active_stream(StreamType::DUPLEX, Scope::Input, true, 0);
}

#[ignore]
#[test]
fn test_reinit_duplex_stream_by_unplugging_a_default_input_device() {
    // Expect to get an device-changed callback by device-changed event handler,
    // which will reinitialize the stream behind the scenes
    test_unplug_a_device_on_an_active_stream(StreamType::DUPLEX, Scope::Input, true, 500);
}

// Unplug the default ouput device for a duplex stream
// ------------------------------------------------------------------------------------------------

#[ignore]
#[test]
fn test_destroy_duplex_stream_after_unplugging_a_default_output_device() {
    // Expect to get an device-changed callback by device-changed event handler,
    // which will reinitialize the stream behind the scenes, at the same when
    // the stream is being destroyed
    test_unplug_a_device_on_an_active_stream(StreamType::DUPLEX, Scope::Output, true, 0);
}

#[ignore]
#[test]
fn test_reinit_duplex_stream_by_unplugging_a_default_output_device() {
    // Expect to get an device-changed callback by device-changed event handler,
    // which will reinitialize the stream behind the scenes
    test_unplug_a_device_on_an_active_stream(StreamType::DUPLEX, Scope::Output, true, 500);
}

fn test_unplug_a_device_on_an_active_stream(
    stream_type: StreamType,
    device_scope: Scope,
    set_device_to_default: bool,
    sleep: u64,
) {
    let has_input = test_get_default_device(Scope::Input).is_some();
    let has_output = test_get_default_device(Scope::Output).is_some();

    if stream_type.contains(StreamType::INPUT) && !has_input {
        println!("No input device for input or duplex stream.");
        return;
    }

    if stream_type.contains(StreamType::OUTPUT) && !has_output {
        println!("No output device for output or duplex stream.");
        return;
    }

    let default_device_before_plugging = test_get_default_device(device_scope.clone()).unwrap();
    println!(
        "Before plugging, default {:?} device is {}",
        device_scope, default_device_before_plugging
    );

    let mut plugger = TestDevicePlugger::new(device_scope.clone()).unwrap();
    assert!(plugger.plug().is_ok());
    assert_ne!(plugger.get_device_id(), kAudioObjectUnknown);
    println!(
        "Create plugger device: {} for {:?}",
        plugger.get_device_id(),
        device_scope
    );

    let default_device_after_plugging = test_get_default_device(device_scope.clone()).unwrap();
    println!(
        "After plugging, default {:?} device is {}",
        device_scope, default_device_after_plugging
    );

    // The new device, plugger, is possible to be set to the default device.
    // Before running the test, we need to set the default device to the correct one.
    if set_device_to_default {
        // plugger should be the default device for the test.
        // If it's not, then set it to the default device.
        if default_device_after_plugging != plugger.get_device_id() {
            let prev_def_dev =
                test_set_default_device(plugger.get_device_id(), device_scope.clone()).unwrap();
            assert_eq!(prev_def_dev, default_device_after_plugging);
        }
    } else {
        // plugger should NOT be the default device for the test.
        // If it is, reset the default device to another one.
        if default_device_after_plugging == plugger.get_device_id() {
            let prev_def_dev =
                test_set_default_device(default_device_before_plugging, device_scope.clone())
                    .unwrap();
            assert_eq!(prev_def_dev, default_device_after_plugging);
        }
    }

    // Ignore the return devices' info since we only need to print them.
    let _ = get_devices_info_in_scope(device_scope.clone());
    println!(
        "Current default {:?} device is {}",
        device_scope,
        test_get_default_device(device_scope.clone()).unwrap()
    );

    let (input_device, output_device) = match device_scope {
        Scope::Input => (
            if set_device_to_default {
                None // default input device.
            } else {
                Some(plugger.get_device_id())
            },
            None,
        ),
        Scope::Output => (
            None,
            if set_device_to_default {
                None // default output device.
            } else {
                Some(plugger.get_device_id())
            },
        ),
    };

    struct SharedData {
        changed_count: Arc<Mutex<u32>>,
        states: Arc<Mutex<Vec<ffi::cubeb_state>>>,
    }

    let mut shared_data = SharedData {
        changed_count: Arc::new(Mutex::new(0u32)),
        states: Arc::new(Mutex::new(vec![])),
    };

    test_get_stream_with_device_changed_callback(
        "stream: test stream reinit/destroy after unplugging a device",
        stream_type,
        input_device,
        output_device,
        &mut shared_data as *mut SharedData as *mut c_void,
        state_callback,
        device_changed_callback,
        |stream| {
            let mut changed_watcher = Watcher::new(&shared_data.changed_count);
            changed_watcher.prepare();
            stream.start();
            // Wait for stream data callback.
            thread::sleep(Duration::from_millis(200));
            println!(
                "Stream runs on the device {} for {:?}",
                plugger.get_device_id(),
                device_scope
            );
            let dev = plugger.get_device_id();
            assert!(plugger.unplug().is_ok());

            if set_device_to_default {
                // The stream will be reinitialized if it follows the default input or output device.
                changed_watcher.wait_for_change();
            }

            if sleep > 0 {
                println!(
                    "Wait {} ms for stream re-initialization, or state callback",
                    sleep
                );
                thread::sleep(Duration::from_millis(sleep));

                if !set_device_to_default {
                    // stream can be dropped immediately before device-changed callback
                    // so we only check the states if we wait for it explicitly.
                    let guard = shared_data.states.lock().unwrap();
                    assert!(guard.last().is_some());
                    assert_eq!(guard.last().unwrap(), &ffi::CUBEB_STATE_ERROR);
                }
            } else {
                println!("Destroy the stream immediately");
                if set_device_to_default {
                    println!("Stream re-initialization may run at the same time when stream is being destroyed");
                }
            }

            println!(
                "Device {} for {:?} has been unplugged. The default {:?} device now is {}",
                dev,
                device_scope,
                device_scope,
                test_get_default_device(device_scope.clone()).unwrap()
            );

            println!("The stream is going to be destroyed soon");
        },
    );

    extern "C" fn state_callback(
        stream: *mut ffi::cubeb_stream,
        user_ptr: *mut c_void,
        state: ffi::cubeb_state,
    ) {
        assert!(!stream.is_null());
        println!(
            "state: {}",
            match state {
                ffi::CUBEB_STATE_STARTED => "started",
                ffi::CUBEB_STATE_STOPPED => "stopped",
                ffi::CUBEB_STATE_DRAINED => "drained",
                ffi::CUBEB_STATE_ERROR => "error",
                _ => "unknown",
            }
        );
        let shared_data = unsafe { &mut *(user_ptr as *mut SharedData) };
        {
            let mut guard = shared_data.states.lock().unwrap();
            guard.push(state);
        }
    }

    extern "C" fn device_changed_callback(data: *mut c_void) {
        println!("Device change callback. data @ {:p}", data);
        let shared_data = unsafe { &mut *(data as *mut SharedData) };
        {
            let mut guard = shared_data.changed_count.lock().unwrap();
            *guard += 1;
        }
    }
}

struct Watcher<T: Clone + PartialEq> {
    watching: Arc<Mutex<T>>,
    current: Option<T>,
}

impl<T: Clone + PartialEq> Watcher<T> {
    fn new(value: &Arc<Mutex<T>>) -> Self {
        Self {
            watching: Arc::clone(value),
            current: None,
        }
    }

    fn prepare(&mut self) {
        self.current = Some(self.current_result());
    }

    fn wait_for_change(&self) {
        loop {
            if self.current_result() != self.current.clone().unwrap() {
                break;
            }
            thread::sleep(Duration::from_millis(1));
        }
    }

    fn current_result(&self) -> T {
        let guard = self.watching.lock().unwrap();
        guard.clone()
    }
}

fn test_get_stream_with_device_changed_callback<F>(
    name: &'static str,
    stm_type: StreamType,
    input_device: Option<AudioObjectID>,
    output_device: Option<AudioObjectID>,
    data: *mut c_void,
    state_callback: extern "C" fn(*mut ffi::cubeb_stream, *mut c_void, ffi::cubeb_state),
    device_changed_callback: extern "C" fn(*mut c_void),
    operation: F,
) where
    F: FnOnce(&mut AudioUnitStream),
{
    test_get_stream_with_default_data_callback_by_type(
        name,
        stm_type,
        input_device,
        output_device,
        state_callback,
        data,
        |stream| {
            assert!(stream
                .register_device_changed_callback(Some(device_changed_callback))
                .is_ok());
            operation(stream);
            assert!(stream.register_device_changed_callback(None).is_ok());
        },
    );
}
