extern crate itertools;

use self::itertools::iproduct;
use super::utils::{
    draining_data_callback, get_devices_info_in_scope, noop_data_callback, state_tracking_cb,
    test_device_channels_in_scope, test_get_default_device, test_ops_context_operation,
    test_ops_stream_operation, test_ops_stream_operation_on_context, Scope, StateCallbackData,
};
use super::*;
use std::thread;

// Context Operations
// ------------------------------------------------------------------------------------------------
#[test]
fn test_ops_context_init_and_destroy() {
    test_ops_context_operation("context: init and destroy", |_context_ptr| {});
}

#[test]
fn test_ops_context_backend_id() {
    test_ops_context_operation("context: backend id", |context_ptr| {
        let backend = unsafe {
            let ptr = OPS.get_backend_id.unwrap()(context_ptr);
            CStr::from_ptr(ptr).to_string_lossy().into_owned()
        };
        assert_eq!(backend, "audiounit-rust");
    });
}

#[test]
fn test_ops_context_max_channel_count() {
    test_ops_context_operation("context: max channel count", |context_ptr| {
        let output_exists = test_get_default_device(Scope::Output).is_some();
        let mut max_channel_count = 0;
        let r = unsafe { OPS.get_max_channel_count.unwrap()(context_ptr, &mut max_channel_count) };
        if output_exists {
            assert_eq!(r, ffi::CUBEB_OK);
            assert_ne!(max_channel_count, 0);
        } else {
            assert_eq!(r, ffi::CUBEB_ERROR);
            assert_eq!(max_channel_count, 0);
        }
    });
}

#[test]
fn test_ops_context_min_latency() {
    test_ops_context_operation("context: min latency", |context_ptr| {
        let output_exists = test_get_default_device(Scope::Output).is_some();
        let params = ffi::cubeb_stream_params::default();
        let mut latency = u32::max_value();
        let r = unsafe { OPS.get_min_latency.unwrap()(context_ptr, params, &mut latency) };
        if output_exists {
            assert_eq!(r, ffi::CUBEB_OK);
            assert!(latency >= SAFE_MIN_LATENCY_FRAMES);
            assert!(SAFE_MAX_LATENCY_FRAMES >= latency);
        } else {
            assert_eq!(r, ffi::CUBEB_ERROR);
            assert_eq!(latency, u32::max_value());
        }
    });
}

#[test]
fn test_ops_context_preferred_sample_rate() {
    test_ops_context_operation("context: preferred sample rate", |context_ptr| {
        let output_exists = test_get_default_device(Scope::Output).is_some();
        let mut rate = u32::max_value();
        let r = unsafe { OPS.get_preferred_sample_rate.unwrap()(context_ptr, &mut rate) };
        if output_exists {
            assert_eq!(r, ffi::CUBEB_OK);
            assert_ne!(rate, u32::max_value());
            assert_ne!(rate, 0);
        } else {
            assert_eq!(r, ffi::CUBEB_ERROR);
            assert_eq!(rate, u32::max_value());
        }
    });
}

#[test]
fn test_ops_context_supported_input_processing_params() {
    test_ops_context_operation(
        "context: supported input processing params",
        |context_ptr| {
            let mut params = ffi::CUBEB_INPUT_PROCESSING_PARAM_NONE;
            let r = unsafe {
                OPS.get_supported_input_processing_params.unwrap()(context_ptr, &mut params)
            };
            assert_eq!(r, ffi::CUBEB_OK);
            assert_eq!(
                params,
                ffi::CUBEB_INPUT_PROCESSING_PARAM_ECHO_CANCELLATION
                    | ffi::CUBEB_INPUT_PROCESSING_PARAM_NOISE_SUPPRESSION
                    | ffi::CUBEB_INPUT_PROCESSING_PARAM_AUTOMATIC_GAIN_CONTROL
            );
        },
    );
}

#[test]
fn test_ops_context_enumerate_devices_unknown() {
    test_ops_context_operation("context: enumerate devices (unknown)", |context_ptr| {
        let mut coll = ffi::cubeb_device_collection {
            device: ptr::null_mut(),
            count: 0,
        };
        assert_eq!(
            unsafe {
                OPS.enumerate_devices.unwrap()(
                    context_ptr,
                    ffi::CUBEB_DEVICE_TYPE_UNKNOWN,
                    &mut coll,
                )
            },
            ffi::CUBEB_OK
        );
        assert_eq!(coll.count, 0);
        assert_eq!(coll.device, ptr::null_mut());
        assert_eq!(
            unsafe { OPS.device_collection_destroy.unwrap()(context_ptr, &mut coll) },
            ffi::CUBEB_OK
        );
        assert_eq!(coll.count, 0);
        assert_eq!(coll.device, ptr::null_mut());
    });
}

#[test]
fn test_ops_context_enumerate_devices_input() {
    test_ops_context_operation("context: enumerate devices (input)", |context_ptr| {
        let having_input = test_get_default_device(Scope::Input).is_some();
        let mut coll = ffi::cubeb_device_collection {
            device: ptr::null_mut(),
            count: 0,
        };
        assert_eq!(
            unsafe {
                OPS.enumerate_devices.unwrap()(context_ptr, ffi::CUBEB_DEVICE_TYPE_INPUT, &mut coll)
            },
            ffi::CUBEB_OK
        );
        if having_input {
            assert_ne!(coll.count, 0);
            assert_ne!(coll.device, ptr::null_mut());
        } else {
            assert_eq!(coll.count, 0);
            assert_eq!(coll.device, ptr::null_mut());
        }
        assert_eq!(
            unsafe { OPS.device_collection_destroy.unwrap()(context_ptr, &mut coll) },
            ffi::CUBEB_OK
        );
        assert_eq!(coll.count, 0);
        assert_eq!(coll.device, ptr::null_mut());
    });
}

#[test]
fn test_ops_context_enumerate_devices_output() {
    test_ops_context_operation("context: enumerate devices (output)", |context_ptr| {
        let output_exists = test_get_default_device(Scope::Output).is_some();
        let mut coll = ffi::cubeb_device_collection {
            device: ptr::null_mut(),
            count: 0,
        };
        assert_eq!(
            unsafe {
                OPS.enumerate_devices.unwrap()(
                    context_ptr,
                    ffi::CUBEB_DEVICE_TYPE_OUTPUT,
                    &mut coll,
                )
            },
            ffi::CUBEB_OK
        );
        if output_exists {
            assert_ne!(coll.count, 0);
            assert_ne!(coll.device, ptr::null_mut());
        } else {
            assert_eq!(coll.count, 0);
            assert_eq!(coll.device, ptr::null_mut());
        }
        assert_eq!(
            unsafe { OPS.device_collection_destroy.unwrap()(context_ptr, &mut coll) },
            ffi::CUBEB_OK
        );
        assert_eq!(coll.count, 0);
        assert_eq!(coll.device, ptr::null_mut());
    });
}

#[test]
fn test_ops_context_device_collection_destroy() {
    // Destroy a dummy device collection, without calling enumerate_devices to allocate memory for the device collection
    test_ops_context_operation("context: device collection destroy", |context_ptr| {
        let mut coll = ffi::cubeb_device_collection {
            device: ptr::null_mut(),
            count: 0,
        };
        assert_eq!(
            unsafe { OPS.device_collection_destroy.unwrap()(context_ptr, &mut coll) },
            ffi::CUBEB_OK
        );
        assert_eq!(coll.device, ptr::null_mut());
        assert_eq!(coll.count, 0);
    });
}

#[test]
fn test_ops_context_register_device_collection_changed_unknown() {
    test_ops_context_operation(
        "context: register device collection changed (unknown)",
        |context_ptr| {
            assert_eq!(
                unsafe {
                    OPS.register_device_collection_changed.unwrap()(
                        context_ptr,
                        ffi::CUBEB_DEVICE_TYPE_UNKNOWN,
                        None,
                        ptr::null_mut(),
                    )
                },
                ffi::CUBEB_ERROR_INVALID_PARAMETER
            );
        },
    );
}

#[test]
fn test_ops_context_register_device_collection_changed_twice_input() {
    test_ops_context_register_device_collection_changed_twice(ffi::CUBEB_DEVICE_TYPE_INPUT);
}

#[test]
fn test_ops_context_register_device_collection_changed_twice_output() {
    test_ops_context_register_device_collection_changed_twice(ffi::CUBEB_DEVICE_TYPE_OUTPUT);
}

#[test]
fn test_ops_context_register_device_collection_changed_twice_inout() {
    test_ops_context_register_device_collection_changed_twice(
        ffi::CUBEB_DEVICE_TYPE_INPUT | ffi::CUBEB_DEVICE_TYPE_OUTPUT,
    );
}

fn test_ops_context_register_device_collection_changed_twice(devtype: u32) {
    extern "C" fn callback(_: *mut ffi::cubeb, _: *mut c_void) {}
    let label_input: &'static str = "context: register device collection changed twice (input)";
    let label_output: &'static str = "context: register device collection changed twice (output)";
    let label_inout: &'static str = "context: register device collection changed twice (inout)";
    let label = if devtype == ffi::CUBEB_DEVICE_TYPE_INPUT {
        label_input
    } else if devtype == ffi::CUBEB_DEVICE_TYPE_OUTPUT {
        label_output
    } else if devtype == ffi::CUBEB_DEVICE_TYPE_INPUT | ffi::CUBEB_DEVICE_TYPE_OUTPUT {
        label_inout
    } else {
        return;
    };

    test_ops_context_operation(label, |context_ptr| {
        // Register a callback within the defined scope.
        assert_eq!(
            unsafe {
                OPS.register_device_collection_changed.unwrap()(
                    context_ptr,
                    devtype,
                    Some(callback),
                    ptr::null_mut(),
                )
            },
            ffi::CUBEB_OK
        );

        assert_eq!(
            unsafe {
                OPS.register_device_collection_changed.unwrap()(
                    context_ptr,
                    devtype,
                    Some(callback),
                    ptr::null_mut(),
                )
            },
            ffi::CUBEB_ERROR_INVALID_PARAMETER
        );
        // Unregister
        assert_eq!(
            unsafe {
                OPS.register_device_collection_changed.unwrap()(
                    context_ptr,
                    devtype,
                    None,
                    ptr::null_mut(),
                )
            },
            ffi::CUBEB_OK
        );
    });
}

#[test]
fn test_ops_context_register_device_collection_changed() {
    extern "C" fn callback(_: *mut ffi::cubeb, _: *mut c_void) {}
    test_ops_context_operation(
        "context: register device collection changed",
        |context_ptr| {
            let devtypes: [ffi::cubeb_device_type; 3] = [
                ffi::CUBEB_DEVICE_TYPE_INPUT,
                ffi::CUBEB_DEVICE_TYPE_OUTPUT,
                ffi::CUBEB_DEVICE_TYPE_INPUT | ffi::CUBEB_DEVICE_TYPE_OUTPUT,
            ];

            for devtype in &devtypes {
                // Register a callback in the defined scoped.
                assert_eq!(
                    unsafe {
                        OPS.register_device_collection_changed.unwrap()(
                            context_ptr,
                            *devtype,
                            Some(callback),
                            ptr::null_mut(),
                        )
                    },
                    ffi::CUBEB_OK
                );

                // Unregister all callbacks regardless of the scope.
                assert_eq!(
                    unsafe {
                        OPS.register_device_collection_changed.unwrap()(
                            context_ptr,
                            ffi::CUBEB_DEVICE_TYPE_INPUT | ffi::CUBEB_DEVICE_TYPE_OUTPUT,
                            None,
                            ptr::null_mut(),
                        )
                    },
                    ffi::CUBEB_OK
                );

                // Register callback in the defined scoped again.
                assert_eq!(
                    unsafe {
                        OPS.register_device_collection_changed.unwrap()(
                            context_ptr,
                            *devtype,
                            Some(callback),
                            ptr::null_mut(),
                        )
                    },
                    ffi::CUBEB_OK
                );

                // Unregister callback within the defined scope.
                assert_eq!(
                    unsafe {
                        OPS.register_device_collection_changed.unwrap()(
                            context_ptr,
                            *devtype,
                            None,
                            ptr::null_mut(),
                        )
                    },
                    ffi::CUBEB_OK
                );
            }
        },
    );
}

#[test]
fn test_ops_context_register_device_collection_changed_with_a_duplex_stream() {
    extern "C" fn callback(_: *mut ffi::cubeb, got_called_ptr: *mut c_void) {
        let got_called = unsafe { &mut *(got_called_ptr as *mut bool) };
        *got_called = true;
    }

    test_ops_context_operation(
        "context: register device collection changed and create a duplex stream",
        |context_ptr| {
            let got_called = Box::new(false);
            let got_called_ptr = Box::into_raw(got_called);

            // Register a callback monitoring both input and output device collection.
            assert_eq!(
                unsafe {
                    OPS.register_device_collection_changed.unwrap()(
                        context_ptr,
                        ffi::CUBEB_DEVICE_TYPE_INPUT | ffi::CUBEB_DEVICE_TYPE_OUTPUT,
                        Some(callback),
                        got_called_ptr as *mut c_void,
                    )
                },
                ffi::CUBEB_OK
            );

            // The aggregate device is very likely to be created in the system
            // when creating a duplex stream. We need to make sure it won't trigger
            // the callback.
            test_default_duplex_stream_operation("duplex stream", |_stream| {
                // Do nothing but wait for device-collection change.
                thread::sleep(Duration::from_millis(200));
            });

            // Unregister the callback.
            assert_eq!(
                unsafe {
                    OPS.register_device_collection_changed.unwrap()(
                        context_ptr,
                        ffi::CUBEB_DEVICE_TYPE_INPUT | ffi::CUBEB_DEVICE_TYPE_OUTPUT,
                        None,
                        got_called_ptr as *mut c_void,
                    )
                },
                ffi::CUBEB_OK
            );

            let got_called = unsafe { Box::from_raw(got_called_ptr) };
            assert!(!got_called.as_ref());
        },
    );
}

#[test]
#[ignore]
fn test_ops_context_register_device_collection_changed_manual() {
    test_ops_context_operation(
        "(manual) context: register device collection changed",
        |context_ptr| {
            println!("context @ {:p}", context_ptr);

            struct Data {
                context: *mut ffi::cubeb,
                touched: u32, // TODO: Use AtomicU32 instead
            }

            extern "C" fn input_callback(context: *mut ffi::cubeb, user: *mut c_void) {
                println!("input > context @ {:p}", context);
                let data = unsafe { &mut (*(user as *mut Data)) };
                assert_eq!(context, data.context);
                data.touched += 1;
            }

            extern "C" fn output_callback(context: *mut ffi::cubeb, user: *mut c_void) {
                println!("output > context @ {:p}", context);
                let data = unsafe { &mut (*(user as *mut Data)) };
                assert_eq!(context, data.context);
                data.touched += 1;
            }

            let mut data = Data {
                context: context_ptr,
                touched: 0,
            };

            // Register a callback for input scope.
            assert_eq!(
                unsafe {
                    OPS.register_device_collection_changed.unwrap()(
                        context_ptr,
                        ffi::CUBEB_DEVICE_TYPE_INPUT,
                        Some(input_callback),
                        &mut data as *mut Data as *mut c_void,
                    )
                },
                ffi::CUBEB_OK
            );

            // Register a callback for output scope.
            assert_eq!(
                unsafe {
                    OPS.register_device_collection_changed.unwrap()(
                        context_ptr,
                        ffi::CUBEB_DEVICE_TYPE_OUTPUT,
                        Some(output_callback),
                        &mut data as *mut Data as *mut c_void,
                    )
                },
                ffi::CUBEB_OK
            );

            while data.touched < 2 {}
        },
    );
}

#[test]
fn test_ops_context_stream_init_no_stream_params() {
    let name = "context: stream_init with no stream params";
    test_ops_context_operation(name, |context_ptr| {
        let mut stream: *mut ffi::cubeb_stream = ptr::null_mut();
        let stream_name = CString::new(name).expect("Failed to create stream name");
        assert_eq!(
            unsafe {
                OPS.stream_init.unwrap()(
                    context_ptr,
                    &mut stream,
                    stream_name.as_ptr(),
                    ptr::null_mut(), // Use default input device.
                    ptr::null_mut(), // No input parameters.
                    ptr::null_mut(), // Use default output device.
                    ptr::null_mut(), // No output parameters.
                    4096,            // TODO: Get latency by get_min_latency instead ?
                    Some(noop_data_callback),
                    None,            // No state callback.
                    ptr::null_mut(), // No user data pointer.
                )
            },
            ffi::CUBEB_ERROR_INVALID_PARAMETER
        );
        assert!(stream.is_null());
    });
}

#[test]
fn test_ops_context_stream_init_no_input_stream_params() {
    let name = "context: stream_init with no input stream params";
    let input_device = test_get_default_device(Scope::Input);
    if input_device.is_none() {
        println!("No input device to perform input tests for \"{}\".", name);
        return;
    }
    test_ops_context_operation(name, |context_ptr| {
        let mut stream: *mut ffi::cubeb_stream = ptr::null_mut();
        let stream_name = CString::new(name).expect("Failed to create stream name");
        assert_eq!(
            unsafe {
                OPS.stream_init.unwrap()(
                    context_ptr,
                    &mut stream,
                    stream_name.as_ptr(),
                    input_device.unwrap() as ffi::cubeb_devid,
                    ptr::null_mut(), // No input parameters.
                    ptr::null_mut(), // Use default output device.
                    ptr::null_mut(), // No output parameters.
                    4096,            // TODO: Get latency by get_min_latency instead ?
                    Some(noop_data_callback),
                    None,            // No state callback.
                    ptr::null_mut(), // No user data pointer.
                )
            },
            ffi::CUBEB_ERROR_INVALID_PARAMETER
        );
        assert!(stream.is_null());
    });
}

#[test]
fn test_ops_context_stream_init_no_output_stream_params() {
    let name = "context: stream_init with no output stream params";
    let output_device = test_get_default_device(Scope::Output);
    if output_device.is_none() {
        println!("No output device to perform output tests for \"{}\".", name);
        return;
    }
    test_ops_context_operation(name, |context_ptr| {
        let mut stream: *mut ffi::cubeb_stream = ptr::null_mut();
        let stream_name = CString::new(name).expect("Failed to create stream name");
        assert_eq!(
            unsafe {
                OPS.stream_init.unwrap()(
                    context_ptr,
                    &mut stream,
                    stream_name.as_ptr(),
                    ptr::null_mut(), // Use default input device.
                    ptr::null_mut(), // No input parameters.
                    output_device.unwrap() as ffi::cubeb_devid,
                    ptr::null_mut(), // No output parameters.
                    4096,            // TODO: Get latency by get_min_latency instead ?
                    Some(noop_data_callback),
                    None,            // No state callback.
                    ptr::null_mut(), // No user data pointer.
                )
            },
            ffi::CUBEB_ERROR_INVALID_PARAMETER
        );
        assert!(stream.is_null());
    });
}

#[test]
fn test_ops_context_stream_init_no_data_callback() {
    let name = "context: stream_init with no data callback";
    test_ops_context_operation(name, |context_ptr| {
        let mut stream: *mut ffi::cubeb_stream = ptr::null_mut();
        let stream_name = CString::new(name).expect("Failed to create stream name");

        let mut output_params = ffi::cubeb_stream_params::default();
        output_params.format = ffi::CUBEB_SAMPLE_FLOAT32NE;
        output_params.rate = 44100;
        output_params.channels = 2;
        output_params.layout = ffi::CUBEB_LAYOUT_UNDEFINED;
        output_params.prefs = ffi::CUBEB_STREAM_PREF_NONE;

        assert_eq!(
            unsafe {
                OPS.stream_init.unwrap()(
                    context_ptr,
                    &mut stream,
                    stream_name.as_ptr(),
                    ptr::null_mut(), // Use default input device.
                    ptr::null_mut(), // No input parameters.
                    ptr::null_mut(), // Use default output device.
                    &mut output_params,
                    4096,            // TODO: Get latency by get_min_latency instead ?
                    None,            // No data callback.
                    None,            // No state callback.
                    ptr::null_mut(), // No user data pointer.
                )
            },
            ffi::CUBEB_ERROR_INVALID_PARAMETER
        );
        assert!(stream.is_null());
    });
}

#[test]
fn test_ops_context_stream_init_channel_rate_combinations() {
    let name = "context: stream_init with various channels and rates";
    test_ops_context_operation(name, |context_ptr| {
        let mut stream: *mut ffi::cubeb_stream = ptr::null_mut();
        let stream_name = CString::new(name).expect("Failed to create stream name");

        const MAX_NUM_CHANNELS: u32 = 32;
        let channel_values: Vec<u32> = vec![1, 2, 3, 4, 6];
        let freq_values: Vec<u32> = vec![16000, 24000, 44100, 48000];
        let is_float_values: Vec<bool> = vec![false, true];

        for (channels, freq, is_float) in iproduct!(channel_values, freq_values, is_float_values) {
            assert!(channels < MAX_NUM_CHANNELS);
            println!("--------------------------");
            println!(
                "Testing {} channel(s), {} Hz, {}\n",
                channels,
                freq,
                if is_float { "float" } else { "short" }
            );

            let mut output_params = ffi::cubeb_stream_params::default();
            output_params.format = if is_float {
                ffi::CUBEB_SAMPLE_FLOAT32NE
            } else {
                ffi::CUBEB_SAMPLE_S16NE
            };
            output_params.rate = freq;
            output_params.channels = channels;
            output_params.layout = ffi::CUBEB_LAYOUT_UNDEFINED;
            output_params.prefs = ffi::CUBEB_STREAM_PREF_NONE;

            assert_eq!(
                unsafe {
                    OPS.stream_init.unwrap()(
                        context_ptr,
                        &mut stream,
                        stream_name.as_ptr(),
                        ptr::null_mut(), // Use default input device.
                        ptr::null_mut(), // No input parameters.
                        ptr::null_mut(), // Use default output device.
                        &mut output_params,
                        4096, // TODO: Get latency by get_min_latency instead ?
                        Some(noop_data_callback), // No data callback.
                        None, // No state callback.
                        ptr::null_mut(), // No user data pointer.
                    )
                },
                ffi::CUBEB_OK
            );
            assert!(!stream.is_null());

            unsafe { OPS.stream_destroy.unwrap()(stream) };
        }
    });
}

// Stream Operations
// ------------------------------------------------------------------------------------------------
fn test_default_output_stream_operation_on_context_with_callback<F>(
    name: &'static str,
    context_ptr: *mut ffi::cubeb,
    data_callback: ffi::cubeb_data_callback,
    operation: F,
) where
    F: FnOnce(*mut ffi::cubeb_stream),
{
    // Make sure the parameters meet the requirements of AudioUnitContext::stream_init
    // (in the comments).
    let mut output_params = ffi::cubeb_stream_params::default();
    output_params.format = ffi::CUBEB_SAMPLE_FLOAT32NE;
    output_params.rate = 44100;
    output_params.channels = 2;
    output_params.layout = ffi::CUBEB_LAYOUT_UNDEFINED;
    output_params.prefs = ffi::CUBEB_STREAM_PREF_NONE;

    test_ops_stream_operation_on_context(
        name,
        context_ptr,
        ptr::null_mut(), // Use default input device.
        ptr::null_mut(), // No input parameters.
        ptr::null_mut(), // Use default output device.
        &mut output_params,
        4096, // TODO: Get latency by get_min_latency instead ?
        data_callback,
        None,            // No state callback.
        ptr::null_mut(), // No user data pointer.
        operation,
    );
}

fn test_default_output_stream_operation_with_callback<F>(
    name: &'static str,
    data_callback: ffi::cubeb_data_callback,
    operation: F,
) where
    F: FnOnce(*mut ffi::cubeb_stream),
{
    test_ops_context_operation("context: default output stream operation", |context_ptr| {
        test_default_output_stream_operation_on_context_with_callback(
            name,
            context_ptr,
            data_callback,
            operation,
        );
    });
}

fn test_default_output_stream_operation<F>(name: &'static str, operation: F)
where
    F: FnOnce(*mut ffi::cubeb_stream),
{
    test_default_output_stream_operation_with_callback(name, Some(noop_data_callback), operation);
}

fn test_default_duplex_stream_operation_on_context_with_callback<F>(
    name: &'static str,
    context_ptr: *mut ffi::cubeb,
    data_callback: ffi::cubeb_data_callback,
    operation: F,
) where
    F: FnOnce(*mut ffi::cubeb_stream),
{
    // Make sure the parameters meet the requirements of AudioUnitContext::stream_init
    // (in the comments).
    let mut input_params = ffi::cubeb_stream_params::default();
    input_params.format = ffi::CUBEB_SAMPLE_FLOAT32NE;
    input_params.rate = 48000;
    input_params.channels = 1;
    input_params.layout = ffi::CUBEB_LAYOUT_UNDEFINED;
    input_params.prefs = ffi::CUBEB_STREAM_PREF_NONE;

    let mut output_params = ffi::cubeb_stream_params::default();
    output_params.format = ffi::CUBEB_SAMPLE_FLOAT32NE;
    output_params.rate = 44100;
    output_params.channels = 2;
    output_params.layout = ffi::CUBEB_LAYOUT_UNDEFINED;
    output_params.prefs = ffi::CUBEB_STREAM_PREF_NONE;

    test_ops_stream_operation_on_context(
        name,
        context_ptr,
        ptr::null_mut(), // Use default input device.
        &mut input_params,
        ptr::null_mut(), // Use default output device.
        &mut output_params,
        4096, // TODO: Get latency by get_min_latency instead ?
        data_callback,
        None,            // No state callback.
        ptr::null_mut(), // No user data pointer.
        operation,
    );
}

fn test_default_duplex_stream_operation_with_callback<F>(
    name: &'static str,
    data_callback: ffi::cubeb_data_callback,
    operation: F,
) where
    F: FnOnce(*mut ffi::cubeb_stream),
{
    test_ops_context_operation("context: default duplex stream operation", |context_ptr| {
        test_default_duplex_stream_operation_on_context_with_callback(
            name,
            context_ptr,
            data_callback,
            operation,
        );
    });
}

fn test_default_duplex_stream_operation<F>(name: &'static str, operation: F)
where
    F: FnOnce(*mut ffi::cubeb_stream),
{
    test_default_duplex_stream_operation_with_callback(name, Some(noop_data_callback), operation);
}

fn test_stereo_input_duplex_stream_operation_on_context_with_callback<F>(
    name: &'static str,
    context_ptr: *mut ffi::cubeb,
    data_callback: ffi::cubeb_data_callback,
    operation: F,
) where
    F: FnOnce(*mut ffi::cubeb_stream),
{
    let mut input_devices = get_devices_info_in_scope(Scope::Input);
    input_devices.retain(|d| test_device_channels_in_scope(d.id, Scope::Input).unwrap_or(0) >= 2);
    if input_devices.is_empty() {
        println!("No stereo input device present. Skipping stereo-input test.");
        return;
    }

    let mut input_params = ffi::cubeb_stream_params::default();
    input_params.format = ffi::CUBEB_SAMPLE_FLOAT32NE;
    input_params.rate = 48000;
    input_params.channels = 2;
    input_params.layout = ffi::CUBEB_LAYOUT_UNDEFINED;
    input_params.prefs = ffi::CUBEB_STREAM_PREF_NONE;

    let mut output_params = ffi::cubeb_stream_params::default();
    output_params.format = ffi::CUBEB_SAMPLE_FLOAT32NE;
    output_params.rate = 48000;
    output_params.channels = 2;
    output_params.layout = ffi::CUBEB_LAYOUT_UNDEFINED;
    output_params.prefs = ffi::CUBEB_STREAM_PREF_NONE;

    test_ops_stream_operation_on_context(
        name,
        context_ptr,
        input_devices[0].id as ffi::cubeb_devid,
        &mut input_params,
        ptr::null_mut(), // Use default output device.
        &mut output_params,
        4096, // TODO: Get latency by get_min_latency instead ?
        data_callback,
        None,            // No state callback.
        ptr::null_mut(), // No user data pointer.
        operation,
    );
}

fn test_stereo_input_duplex_stream_operation_with_callback<F>(
    name: &'static str,
    data_callback: ffi::cubeb_data_callback,
    operation: F,
) where
    F: FnOnce(*mut ffi::cubeb_stream),
{
    test_ops_context_operation(
        "context: stereo input duplex stream operation",
        |context_ptr| {
            test_stereo_input_duplex_stream_operation_on_context_with_callback(
                name,
                context_ptr,
                data_callback,
                operation,
            );
        },
    );
}

fn test_stereo_input_duplex_stream_operation<F>(name: &'static str, operation: F)
where
    F: FnOnce(*mut ffi::cubeb_stream),
{
    test_stereo_input_duplex_stream_operation_with_callback(
        name,
        Some(noop_data_callback),
        operation,
    );
}

fn test_default_input_voice_stream_operation_on_context_with_callback<F>(
    name: &'static str,
    context_ptr: *mut ffi::cubeb,
    data_callback: ffi::cubeb_data_callback,
    operation: F,
) where
    F: FnOnce(*mut ffi::cubeb_stream),
{
    // Make sure the parameters meet the requirements of AudioUnitContext::stream_init
    // (in the comments).
    let mut input_params = ffi::cubeb_stream_params::default();
    input_params.format = ffi::CUBEB_SAMPLE_FLOAT32NE;
    input_params.rate = 44100;
    input_params.channels = 1;
    input_params.layout = ffi::CUBEB_LAYOUT_UNDEFINED;
    input_params.prefs = ffi::CUBEB_STREAM_PREF_VOICE;

    test_ops_stream_operation_on_context(
        name,
        context_ptr,
        ptr::null_mut(), // Use default input device.
        &mut input_params,
        ptr::null_mut(), // Use default output device.
        ptr::null_mut(), // No output parameters.
        4096,            // TODO: Get latency by get_min_latency instead ?
        data_callback,
        None,            // No state callback.
        ptr::null_mut(), // No user data pointer.
        operation,
    );
}

fn test_default_input_voice_stream_operation_on_context<F>(
    name: &'static str,
    context_ptr: *mut ffi::cubeb,
    operation: F,
) where
    F: FnOnce(*mut ffi::cubeb_stream),
{
    test_default_input_voice_stream_operation_on_context_with_callback(
        name,
        context_ptr,
        Some(noop_data_callback),
        operation,
    );
}

fn test_default_input_voice_stream_operation_with_callback<F>(
    name: &'static str,
    data_callback: ffi::cubeb_data_callback,
    operation: F,
) where
    F: FnOnce(*mut ffi::cubeb_stream),
{
    test_ops_context_operation(
        "context: default input voice stream operation",
        |context_ptr| {
            test_default_input_voice_stream_operation_on_context_with_callback(
                name,
                context_ptr,
                data_callback,
                operation,
            );
        },
    );
}

fn test_default_input_voice_stream_operation<F>(name: &'static str, operation: F)
where
    F: FnOnce(*mut ffi::cubeb_stream),
{
    test_default_input_voice_stream_operation_with_callback(
        name,
        Some(noop_data_callback),
        operation,
    );
}

fn test_default_duplex_voice_stream_operation_on_context_with_callback<F>(
    name: &'static str,
    context_ptr: *mut ffi::cubeb,
    data_callback: ffi::cubeb_data_callback,
    operation: F,
) where
    F: FnOnce(*mut ffi::cubeb_stream),
{
    // Make sure the parameters meet the requirements of AudioUnitContext::stream_init
    // (in the comments).
    let mut input_params = ffi::cubeb_stream_params::default();
    input_params.format = ffi::CUBEB_SAMPLE_FLOAT32NE;
    input_params.rate = 44100;
    input_params.channels = 1;
    input_params.layout = ffi::CUBEB_LAYOUT_UNDEFINED;
    input_params.prefs = ffi::CUBEB_STREAM_PREF_VOICE;

    let mut output_params = ffi::cubeb_stream_params::default();
    output_params.format = ffi::CUBEB_SAMPLE_FLOAT32NE;
    output_params.rate = 48000;
    output_params.channels = 2;
    output_params.layout = ffi::CUBEB_LAYOUT_UNDEFINED;
    output_params.prefs = ffi::CUBEB_STREAM_PREF_VOICE;

    test_ops_stream_operation_on_context(
        name,
        context_ptr,
        ptr::null_mut(), // Use default input device.
        &mut input_params,
        ptr::null_mut(), // Use default output device.
        &mut output_params,
        4096, // TODO: Get latency by get_min_latency instead ?
        data_callback,
        None,            // No state callback.
        ptr::null_mut(), // No user data pointer.
        operation,
    );
}

fn test_default_duplex_voice_stream_operation_on_context<F>(
    name: &'static str,
    context_ptr: *mut ffi::cubeb,
    operation: F,
) where
    F: FnOnce(*mut ffi::cubeb_stream),
{
    test_default_duplex_voice_stream_operation_on_context_with_callback(
        name,
        context_ptr,
        Some(noop_data_callback),
        operation,
    );
}

fn test_default_duplex_voice_stream_operation_with_callback<F>(
    name: &'static str,
    data_callback: ffi::cubeb_data_callback,
    operation: F,
) where
    F: FnOnce(*mut ffi::cubeb_stream),
{
    test_ops_context_operation("context: duplex voice stream operation", |context_ptr| {
        test_default_duplex_voice_stream_operation_on_context_with_callback(
            name,
            context_ptr,
            data_callback,
            operation,
        );
    });
}

fn test_default_duplex_voice_stream_operation<F>(name: &'static str, operation: F)
where
    F: FnOnce(*mut ffi::cubeb_stream),
{
    test_default_duplex_voice_stream_operation_with_callback(
        name,
        Some(noop_data_callback),
        operation,
    );
}

fn test_stereo_input_duplex_voice_stream_operation<F>(name: &'static str, operation: F)
where
    F: FnOnce(*mut ffi::cubeb_stream),
{
    let mut input_devices = get_devices_info_in_scope(Scope::Input);
    input_devices.retain(|d| test_device_channels_in_scope(d.id, Scope::Input).unwrap_or(0) >= 2);
    if input_devices.is_empty() {
        println!("No stereo input device present. Skipping stereo-input test.");
        return;
    }

    let mut input_params = ffi::cubeb_stream_params::default();
    input_params.format = ffi::CUBEB_SAMPLE_FLOAT32NE;
    input_params.rate = 44100;
    input_params.channels = 2;
    input_params.layout = ffi::CUBEB_LAYOUT_UNDEFINED;
    input_params.prefs = ffi::CUBEB_STREAM_PREF_VOICE;

    let mut output_params = ffi::cubeb_stream_params::default();
    output_params.format = ffi::CUBEB_SAMPLE_FLOAT32NE;
    output_params.rate = 44100;
    output_params.channels = 2;
    output_params.layout = ffi::CUBEB_LAYOUT_UNDEFINED;
    output_params.prefs = ffi::CUBEB_STREAM_PREF_VOICE;

    test_ops_stream_operation(
        name,
        input_devices[0].id as ffi::cubeb_devid,
        &mut input_params,
        ptr::null_mut(), // Use default output device.
        &mut output_params,
        4096, // TODO: Get latency by get_min_latency instead ?
        Some(noop_data_callback),
        None,            // No state callback.
        ptr::null_mut(), // No user data pointer.
        operation,
    );
}

#[test]
fn test_ops_stream_init_and_destroy() {
    test_default_output_stream_operation("stream: init and destroy", |_stream| {});
}

#[test]
fn test_ops_stream_start() {
    test_default_output_stream_operation("stream: start", |stream| {
        assert_eq!(unsafe { OPS.stream_start.unwrap()(stream) }, ffi::CUBEB_OK);
    });
}

#[test]
fn test_ops_stream_stop() {
    test_default_output_stream_operation("stream: stop", |stream| {
        assert_eq!(unsafe { OPS.stream_stop.unwrap()(stream) }, ffi::CUBEB_OK);
    });
}

#[test]
fn test_ops_stream_drain() {
    test_default_output_stream_operation_with_callback(
        "stream: drain",
        Some(draining_data_callback),
        |stream| {
            assert_eq!(unsafe { OPS.stream_start.unwrap()(stream) }, ffi::CUBEB_OK);
            thread::sleep(Duration::from_millis(10));
        },
    );
}

#[test]
fn test_ops_stream_position() {
    test_default_output_stream_operation("stream: position", |stream| {
        let mut position = u64::max_value();
        assert_eq!(
            unsafe { OPS.stream_get_position.unwrap()(stream, &mut position) },
            ffi::CUBEB_OK
        );
        assert_eq!(position, 0);
    });
}

#[test]
fn test_ops_stream_latency() {
    test_default_output_stream_operation("stream: latency", |stream| {
        let mut latency = u32::max_value();
        assert_eq!(
            unsafe { OPS.stream_get_latency.unwrap()(stream, &mut latency) },
            ffi::CUBEB_OK
        );
        assert_ne!(latency, u32::max_value());
    });
}

#[test]
fn test_ops_stream_set_volume() {
    test_default_output_stream_operation("stream: set volume", |stream| {
        assert_eq!(
            unsafe { OPS.stream_set_volume.unwrap()(stream, 0.5) },
            ffi::CUBEB_OK
        );
    });
}

#[test]
fn test_ops_stream_current_device() {
    test_default_output_stream_operation("stream: get current device and destroy it", |stream| {
        if test_get_default_device(Scope::Input).is_none()
            || test_get_default_device(Scope::Output).is_none()
        {
            println!("stream_get_current_device only works when the machine has both input and output devices");
            return;
        }

        let mut device: *mut ffi::cubeb_device = ptr::null_mut();
        if unsafe { OPS.stream_get_current_device.unwrap()(stream, &mut device) } != ffi::CUBEB_OK {
            // It can happen when we fail to get the device source.
            println!("stream_get_current_device fails. Skip this test.");
            return;
        }

        assert!(!device.is_null());
        // Uncomment the below to print out the results.
        // let deviceref = unsafe { DeviceRef::from_ptr(device) };
        // println!(
        //     "output: {}",
        //     deviceref.output_name().unwrap_or("(no device name)")
        // );
        // println!(
        //     "input: {}",
        //     deviceref.input_name().unwrap_or("(no device name)")
        // );
        assert_eq!(
            unsafe { OPS.stream_device_destroy.unwrap()(stream, device) },
            ffi::CUBEB_OK
        );
    });
}

#[test]
fn test_ops_stream_device_destroy() {
    test_default_output_stream_operation("stream: destroy null device", |stream| {
        assert_eq!(
            unsafe { OPS.stream_device_destroy.unwrap()(stream, ptr::null_mut()) },
            ffi::CUBEB_OK // It returns OK anyway.
        );
    });
}

pub extern "C" fn reiniting_and_erroring_data_callback(
    stream: *mut ffi::cubeb_stream,
    _user_ptr: *mut c_void,
    _input_buffer: *const c_void,
    output_buffer: *mut c_void,
    nframes: i64,
) -> i64 {
    assert!(!stream.is_null());

    let stm = unsafe { &mut *(stream as *mut AudioUnitStream) };

    // Feed silence data to output buffer
    if !output_buffer.is_null() {
        let channels = stm.core_stream_data.output_stream_params.channels();
        let samples = nframes as usize * channels as usize;
        let sample_size = cubeb_sample_size(stm.core_stream_data.output_stream_params.format());
        unsafe {
            ptr::write_bytes(output_buffer, 0, samples * sample_size);
        }
    }

    // Trigger an async reinit before the backend handles the error below.
    // This scenario could happen in the backend's internal input callback.
    stm.reinit_async();

    ffi::CUBEB_ERROR.into()
}

#[test]
fn test_ops_stream_racy_reinit() {
    // Make sure the parameters meet the requirements of AudioUnitContext::stream_init
    // (in the comments).
    let mut input_params = ffi::cubeb_stream_params::default();
    input_params.format = ffi::CUBEB_SAMPLE_FLOAT32NE;
    input_params.rate = 48000;
    input_params.channels = 1;
    input_params.layout = ffi::CUBEB_LAYOUT_UNDEFINED;
    input_params.prefs = ffi::CUBEB_STREAM_PREF_NONE;

    let mut output_params = ffi::cubeb_stream_params::default();
    output_params.format = ffi::CUBEB_SAMPLE_FLOAT32NE;
    output_params.rate = 44100;
    output_params.channels = 2;
    output_params.layout = ffi::CUBEB_LAYOUT_UNDEFINED;
    output_params.prefs = ffi::CUBEB_STREAM_PREF_NONE;

    let mut data = StateCallbackData::default();
    test_ops_stream_operation(
        "stream: racy reinit",
        ptr::null_mut(), // Use default input device.
        &mut input_params,
        ptr::null_mut(), // Use default output device.
        &mut output_params,
        4096, // TODO: Get latency by get_min_latency instead ?
        Some(reiniting_and_erroring_data_callback),
        Some(state_tracking_cb),
        &mut data as *mut StateCallbackData as *mut c_void,
        |stream| {
            assert_eq!(unsafe { OPS.stream_start.unwrap()(stream) }, ffi::CUBEB_OK);
            while data.error_cnt() == 0 && data.stopped_cnt() == 0 {
                thread::sleep(Duration::from_millis(1));
            }
            assert_eq!(unsafe { OPS.stream_stop.unwrap()(stream) }, ffi::CUBEB_OK);
        },
    );
    assert_eq!(data.started_cnt(), 1);
    assert_eq!(data.stopped_cnt(), 0);
    assert_eq!(data.drained_cnt(), 0);
    assert_eq!(data.error_cnt(), 1);
}

#[test]
fn test_ops_stream_register_device_changed_callback() {
    extern "C" fn callback(_: *mut c_void) {}

    test_default_output_stream_operation("stream: register device changed callback", |stream| {
        assert_eq!(
            unsafe { OPS.stream_register_device_changed_callback.unwrap()(stream, Some(callback)) },
            ffi::CUBEB_OK
        );
        assert_eq!(
            unsafe { OPS.stream_register_device_changed_callback.unwrap()(stream, Some(callback)) },
            ffi::CUBEB_ERROR_INVALID_PARAMETER
        );
        assert_eq!(
            unsafe { OPS.stream_register_device_changed_callback.unwrap()(stream, None) },
            ffi::CUBEB_OK
        );
    });
}

#[test]
fn test_ops_stereo_input_duplex_stream_init_and_destroy() {
    test_stereo_input_duplex_stream_operation(
        "stereo-input duplex stream: init and destroy",
        |_stream| {},
    );
}

#[test]
fn test_ops_stereo_input_duplex_stream_start() {
    test_stereo_input_duplex_stream_operation("stereo-input duplex stream: start", |stream| {
        assert_eq!(unsafe { OPS.stream_start.unwrap()(stream) }, ffi::CUBEB_OK);
    });
}

#[test]
fn test_ops_stereo_input_duplex_stream_stop() {
    test_stereo_input_duplex_stream_operation("stereo-input duplex stream: stop", |stream| {
        assert_eq!(unsafe { OPS.stream_stop.unwrap()(stream) }, ffi::CUBEB_OK);
    });
}

#[test]
fn test_ops_stereo_input_duplex_stream_drain() {
    test_stereo_input_duplex_stream_operation_with_callback(
        "stereo-input duplex stream: drain",
        Some(draining_data_callback),
        |stream| {
            assert_eq!(unsafe { OPS.stream_start.unwrap()(stream) }, ffi::CUBEB_OK);
            thread::sleep(Duration::from_millis(10));
        },
    );
}

#[test]
fn test_ops_input_voice_stream_init_and_destroy() {
    test_default_input_voice_stream_operation("input voice stream: init and destroy", |stream| {
        let stm = unsafe { &mut *(stream as *mut AudioUnitStream) };
        assert_eq!(
            stm.core_stream_data.using_voice_processing_unit(),
            macos_kernel_major_version().unwrap() != MACOS_KERNEL_MAJOR_VERSION_MONTEREY
        );
    });
}

#[test]
fn test_ops_input_voice_stream_start() {
    test_default_input_voice_stream_operation("input voice stream: start", |stream| {
        assert_eq!(unsafe { OPS.stream_start.unwrap()(stream) }, ffi::CUBEB_OK);
        let stm = unsafe { &mut *(stream as *mut AudioUnitStream) };
        assert_eq!(
            stm.core_stream_data.using_voice_processing_unit(),
            macos_kernel_major_version().unwrap() != MACOS_KERNEL_MAJOR_VERSION_MONTEREY
        );
    });
}

#[test]
fn test_ops_input_voice_stream_stop() {
    test_default_input_voice_stream_operation("input voice stream: stop", |stream| {
        assert_eq!(unsafe { OPS.stream_stop.unwrap()(stream) }, ffi::CUBEB_OK);
        let stm = unsafe { &mut *(stream as *mut AudioUnitStream) };
        assert_eq!(
            stm.core_stream_data.using_voice_processing_unit(),
            macos_kernel_major_version().unwrap() != MACOS_KERNEL_MAJOR_VERSION_MONTEREY
        );
    });
}

#[test]
fn test_ops_duplex_voice_stream_init_and_destroy() {
    test_default_duplex_voice_stream_operation("duplex voice stream: init and destroy", |stream| {
        let stm = unsafe { &mut *(stream as *mut AudioUnitStream) };
        assert_eq!(
            stm.core_stream_data.using_voice_processing_unit(),
            macos_kernel_major_version().unwrap() != MACOS_KERNEL_MAJOR_VERSION_MONTEREY
        );
    });
}

#[test]
fn test_ops_duplex_voice_stream_start() {
    test_default_duplex_voice_stream_operation("duplex voice stream: start", |stream| {
        assert_eq!(unsafe { OPS.stream_start.unwrap()(stream) }, ffi::CUBEB_OK);
        let stm = unsafe { &mut *(stream as *mut AudioUnitStream) };
        assert_eq!(
            stm.core_stream_data.using_voice_processing_unit(),
            macos_kernel_major_version().unwrap() != MACOS_KERNEL_MAJOR_VERSION_MONTEREY
        );
    });
}

#[test]
fn test_ops_duplex_voice_stream_stop() {
    test_default_duplex_voice_stream_operation("duplex voice stream: stop", |stream| {
        assert_eq!(unsafe { OPS.stream_stop.unwrap()(stream) }, ffi::CUBEB_OK);
        let stm = unsafe { &mut *(stream as *mut AudioUnitStream) };
        assert_eq!(
            stm.core_stream_data.using_voice_processing_unit(),
            macos_kernel_major_version().unwrap() != MACOS_KERNEL_MAJOR_VERSION_MONTEREY
        );
    });
}

#[test]
fn test_ops_duplex_voice_stream_drain() {
    test_default_duplex_voice_stream_operation_with_callback(
        "duplex voice stream: drain",
        Some(draining_data_callback),
        |stream| {
            assert_eq!(unsafe { OPS.stream_start.unwrap()(stream) }, ffi::CUBEB_OK);
            let stm = unsafe { &mut *(stream as *mut AudioUnitStream) };
            assert_eq!(
                stm.core_stream_data.using_voice_processing_unit(),
                macos_kernel_major_version().unwrap() != MACOS_KERNEL_MAJOR_VERSION_MONTEREY
            );
            thread::sleep(Duration::from_millis(10));
        },
    );
}

#[test]
#[ignore]
fn test_ops_timing_sensitive_multiple_voice_stream_init_and_destroy() {
    if macos_kernel_major_version().unwrap() == MACOS_KERNEL_MAJOR_VERSION_MONTEREY {
        // We disable VPIO on Monterey.
        return;
    }
    let start = Instant::now();
    let mut t1 = start;
    let mut t2 = start;
    let mut t3 = start;
    let mut t4 = start;
    let mut t5 = start;
    let mut t6 = start;
    let mut t7 = start;
    let mut t8 = start;
    let mut t9 = start;
    let mut t10 = start;
    test_ops_context_operation("multiple duplex voice streams", |context_ptr| {
        // First stream uses vpio, creates the shared vpio unit.
        test_default_duplex_voice_stream_operation_on_context(
            "multiple voice streams: stream 1, duplex",
            context_ptr,
            |stream| {
                let stm = unsafe { &mut *(stream as *mut AudioUnitStream) };
                assert!(stm.core_stream_data.using_voice_processing_unit());

                // Two concurrent vpio streams are supported.
                test_default_input_voice_stream_operation_on_context(
                    "multiple voice streams: stream 2, input-only",
                    context_ptr,
                    |stream| {
                        let stm = unsafe { &mut *(stream as *mut AudioUnitStream) };
                        assert!(stm.core_stream_data.using_voice_processing_unit());

                        // Three concurrent vpio streams are supported.
                        test_default_duplex_voice_stream_operation_on_context(
                            "multiple voice streams: stream 3, duplex",
                            context_ptr,
                            |stream| {
                                let stm = unsafe { &mut *(stream as *mut AudioUnitStream) };
                                assert!(stm.core_stream_data.using_voice_processing_unit());
                            },
                        );
                    },
                );
            },
        );
        t1 = Instant::now();
        // Fourth stream uses vpio, allows reuse of one already created.
        test_default_duplex_voice_stream_operation_on_context(
            "multiple voice streams: stream 4, duplex",
            context_ptr,
            |stream| {
                let stm = unsafe { &mut *(stream as *mut AudioUnitStream) };
                assert!(stm.core_stream_data.using_voice_processing_unit());
                t2 = Instant::now();

                // Fifth stream uses vpio, allows reuse of one already created.
                test_default_duplex_voice_stream_operation_on_context(
                    "multiple voice streams: stream 5, duplex",
                    context_ptr,
                    |stream| {
                        let stm = unsafe { &mut *(stream as *mut AudioUnitStream) };
                        assert!(stm.core_stream_data.using_voice_processing_unit());
                        t3 = Instant::now();

                        // Sixth stream uses vpio, allows reuse of one already created.
                        test_default_input_voice_stream_operation_on_context(
                            "multiple voice streams: stream 6, input-only",
                            context_ptr,
                            |stream| {
                                let stm = unsafe { &mut *(stream as *mut AudioUnitStream) };
                                assert!(stm.core_stream_data.using_voice_processing_unit());
                                t4 = Instant::now();

                                // Seventh stream uses vpio, but is created anew.
                                test_default_input_voice_stream_operation_on_context(
                                    "multiple voice streams: stream 7, input-only",
                                    context_ptr,
                                    |stream| {
                                        let stm = unsafe { &mut *(stream as *mut AudioUnitStream) };
                                        assert!(stm.core_stream_data.using_voice_processing_unit());
                                        t5 = Instant::now();
                                    },
                                );
                                t6 = Instant::now();
                            },
                        );
                        t7 = Instant::now();
                    },
                );
                t8 = Instant::now();
            },
        );
        t9 = Instant::now();
    });
    t10 = Instant::now();

    let reuse_vpio_1 = t2 - t1;
    let reuse_vpio_2 = t3 - t2;
    let reuse_vpio_3 = t4 - t3;
    let create_standalone_vpio = t5 - t4;
    assert!(
        create_standalone_vpio > reuse_vpio_1 * 2,
        "Failed create_standalone_vpio={}s > reuse_vpio_1={}s * 2",
        create_standalone_vpio.as_secs_f32(),
        reuse_vpio_1.as_secs_f32()
    );
    assert!(
        create_standalone_vpio > reuse_vpio_2 * 2,
        "Failed create_standalone_vpio={}s > reuse_vpio_2={}s * 2",
        create_standalone_vpio.as_secs_f32(),
        reuse_vpio_2.as_secs_f32()
    );
    assert!(
        create_standalone_vpio > reuse_vpio_3 * 2,
        "Failed create_standalone_vpio={}s > reuse_vpio_3={}s * 2",
        create_standalone_vpio.as_secs_f32(),
        reuse_vpio_3.as_secs_f32()
    );

    let recycle_vpio_1 = t6 - t5;
    let recycle_vpio_2 = t7 - t6;
    let recycle_vpio_3 = t8 - t7;
    let recycle_vpio_4 = t9 - t8;
    let dispose_vpios = t10 - t9;
    assert!(
        dispose_vpios > recycle_vpio_1 * 2,
        "Failed dispose_vpios={}s > recycle_vpio_1 ={}s * 2",
        dispose_vpios.as_secs_f32(),
        recycle_vpio_1.as_secs_f32()
    );
    assert!(
        dispose_vpios > recycle_vpio_2 * 2,
        "Failed dispose_vpios={}s > recycle_vpio_2 ={}s * 2",
        dispose_vpios.as_secs_f32(),
        recycle_vpio_2.as_secs_f32()
    );
    assert!(
        dispose_vpios > recycle_vpio_3 * 2,
        "Failed dispose_vpios={}s > recycle_vpio_3 ={}s * 2",
        dispose_vpios.as_secs_f32(),
        recycle_vpio_3.as_secs_f32()
    );
    assert!(
        dispose_vpios > recycle_vpio_4 * 2,
        "Failed dispose_vpios={}s > recycle_vpio_4 ={}s * 2",
        dispose_vpios.as_secs_f32(),
        recycle_vpio_4.as_secs_f32()
    );
}

#[test]
#[ignore]
fn test_ops_timing_sensitive_multiple_duplex_voice_stream_start() {
    if macos_kernel_major_version().unwrap() == MACOS_KERNEL_MAJOR_VERSION_MONTEREY {
        // We disable VPIO on Monterey.
        return;
    }
    test_ops_context_operation("multiple duplex voice streams", |context_ptr| {
        let start = Instant::now();
        // First stream uses vpio, creates the shared vpio unit.
        test_default_duplex_voice_stream_operation_on_context(
            "multiple duplex voice streams: stream 1",
            context_ptr,
            |stream| {
                let stm = unsafe { &mut *(stream as *mut AudioUnitStream) };
                assert!(stm.core_stream_data.using_voice_processing_unit());
                assert_eq!(unsafe { OPS.stream_start.unwrap()(stream) }, ffi::CUBEB_OK);
            },
        );
        let d1 = start.elapsed();
        // Second stream uses vpio, allows reuse of the one already created.
        test_default_duplex_voice_stream_operation_on_context(
            "multiple duplex voice streams: stream 2",
            context_ptr,
            |stream| {
                let stm = unsafe { &mut *(stream as *mut AudioUnitStream) };
                assert!(stm.core_stream_data.using_voice_processing_unit());
                assert_eq!(unsafe { OPS.stream_start.unwrap()(stream) }, ffi::CUBEB_OK);
            },
        );
        let d2 = start.elapsed() - d1;
        // d1 being significantly longer than d2 is proof we reuse vpio.
        assert!(
            d1 > d2 * 2,
            "Failed d1={}s > d2={}s * s",
            d1.as_secs_f32(),
            d2.as_secs_f32()
        );
    });
}

#[test]
#[ignore]
fn test_ops_timing_sensitive_multiple_duplex_voice_stream_params() {
    if macos_kernel_major_version().unwrap() == MACOS_KERNEL_MAJOR_VERSION_MONTEREY {
        // We disable VPIO on Monterey.
        return;
    }
    test_ops_context_operation("multiple duplex voice streams with params", |context_ptr| {
        let start = Instant::now();
        // First stream uses vpio, creates the shared vpio unit.
        test_default_duplex_voice_stream_operation_on_context(
            "multiple duplex voice streams: stream 1",
            context_ptr,
            |stream| {
                let stm = unsafe { &mut *(stream as *mut AudioUnitStream) };
                assert!(stm.core_stream_data.using_voice_processing_unit());
                assert_eq!(
                    unsafe {
                        OPS.stream_set_input_processing_params.unwrap()(
                            stream,
                            ffi::CUBEB_INPUT_PROCESSING_PARAM_ECHO_CANCELLATION
                                | ffi::CUBEB_INPUT_PROCESSING_PARAM_NOISE_SUPPRESSION,
                        )
                    },
                    ffi::CUBEB_OK
                );
                assert_eq!(
                    unsafe { OPS.stream_set_input_mute.unwrap()(stream, 1) },
                    ffi::CUBEB_OK
                );
            },
        );
        let d1 = start.elapsed();
        // Second stream uses vpio, allows reuse of the one already created.
        test_default_duplex_voice_stream_operation_on_context(
            "multiple duplex voice streams: stream 2",
            context_ptr,
            |stream| {
                let stm = unsafe { &mut *(stream as *mut AudioUnitStream) };
                assert!(stm.core_stream_data.using_voice_processing_unit());
                let queue = stm.queue.clone();
                // Test that input processing params does not carry over when reusing vpio.
                let mut bypass: u32 = 0;
                let r = queue
                    .run_sync(|| {
                        audio_unit_get_property(
                            stm.core_stream_data.input_unit,
                            kAUVoiceIOProperty_BypassVoiceProcessing,
                            kAudioUnitScope_Global,
                            AU_IN_BUS,
                            &mut bypass,
                            &mut mem::size_of::<u32>(),
                        )
                    })
                    .unwrap();
                assert_eq!(r, NO_ERR);
                assert_eq!(bypass, 1);

                // Test that input mute state does not carry over when reusing vpio.
                let mut mute: u32 = 0;
                let r = queue
                    .run_sync(|| {
                        audio_unit_get_property(
                            stm.core_stream_data.input_unit,
                            kAUVoiceIOProperty_MuteOutput,
                            kAudioUnitScope_Global,
                            AU_IN_BUS,
                            &mut mute,
                            &mut mem::size_of::<u32>(),
                        )
                    })
                    .unwrap();
                assert_eq!(r, NO_ERR);
                assert_eq!(mute, 0);
            },
        );
        let d2 = start.elapsed() - d1;
        // d1 being significantly longer than d2 is proof we reuse vpio.
        assert!(
            d1 > d2 * 2,
            "Failed d1={}s > d2={}s * 2",
            d1.as_secs_f32(),
            d2.as_secs_f32()
        );
    });
}

#[test]
fn test_ops_duplex_voice_stream_set_input_mute() {
    if macos_kernel_major_version().unwrap() == MACOS_KERNEL_MAJOR_VERSION_MONTEREY {
        // We disable VPIO on Monterey.
        return;
    }
    test_default_duplex_voice_stream_operation("duplex voice stream: mute", |stream| {
        assert_eq!(
            unsafe { OPS.stream_set_input_mute.unwrap()(stream, 1) },
            ffi::CUBEB_OK
        );
        let stm = unsafe { &mut *(stream as *mut AudioUnitStream) };
        assert!(stm.core_stream_data.using_voice_processing_unit());
    });
}

#[test]
fn test_ops_duplex_voice_stream_set_input_mute_before_start() {
    if macos_kernel_major_version().unwrap() == MACOS_KERNEL_MAJOR_VERSION_MONTEREY {
        // We disable VPIO on Monterey.
        return;
    }
    test_default_duplex_voice_stream_operation(
        "duplex voice stream: mute before start",
        |stream| {
            assert_eq!(
                unsafe { OPS.stream_set_input_mute.unwrap()(stream, 1) },
                ffi::CUBEB_OK
            );
            assert_eq!(unsafe { OPS.stream_start.unwrap()(stream) }, ffi::CUBEB_OK);
            let stm = unsafe { &mut *(stream as *mut AudioUnitStream) };
            assert!(stm.core_stream_data.using_voice_processing_unit());
        },
    );
}

#[test]
fn test_ops_duplex_voice_stream_set_input_mute_before_start_with_reinit() {
    if macos_kernel_major_version().unwrap() == MACOS_KERNEL_MAJOR_VERSION_MONTEREY {
        // We disable VPIO on Monterey.
        return;
    }
    test_default_duplex_voice_stream_operation(
        "duplex voice stream: mute before start with reinit",
        |stream| {
            assert_eq!(
                unsafe { OPS.stream_set_input_mute.unwrap()(stream, 1) },
                ffi::CUBEB_OK
            );
            assert_eq!(unsafe { OPS.stream_start.unwrap()(stream) }, ffi::CUBEB_OK);

            // Hacky cast, but testing this here was simplest for now.
            let stm = unsafe { &mut *(stream as *mut AudioUnitStream) };
            assert!(stm.core_stream_data.using_voice_processing_unit());
            stm.reinit_async();
            let queue = stm.queue.clone();
            let mut mute_after_reinit = false;
            queue.run_sync(|| {
                let mut mute: u32 = 0;
                let r = audio_unit_get_property(
                    stm.core_stream_data.input_unit,
                    kAUVoiceIOProperty_MuteOutput,
                    kAudioUnitScope_Global,
                    AU_IN_BUS,
                    &mut mute,
                    &mut mem::size_of::<u32>(),
                );
                assert_eq!(r, NO_ERR);
                mute_after_reinit = mute == 1;
            });
            assert_eq!(mute_after_reinit, true);
        },
    );
}

#[test]
fn test_ops_duplex_voice_stream_set_input_mute_after_start() {
    if macos_kernel_major_version().unwrap() == MACOS_KERNEL_MAJOR_VERSION_MONTEREY {
        // We disable VPIO on Monterey.
        return;
    }
    test_default_duplex_voice_stream_operation("duplex voice stream: mute after start", |stream| {
        assert_eq!(unsafe { OPS.stream_start.unwrap()(stream) }, ffi::CUBEB_OK);
        assert_eq!(
            unsafe { OPS.stream_set_input_mute.unwrap()(stream, 1) },
            ffi::CUBEB_OK
        );
        let stm = unsafe { &mut *(stream as *mut AudioUnitStream) };
        assert!(stm.core_stream_data.using_voice_processing_unit());
    });
}

#[test]
fn test_ops_duplex_voice_stream_set_input_processing_params() {
    if macos_kernel_major_version().unwrap() == MACOS_KERNEL_MAJOR_VERSION_MONTEREY {
        // We disable VPIO on Monterey.
        return;
    }
    test_default_duplex_voice_stream_operation("duplex voice stream: processing", |stream| {
        let params: ffi::cubeb_input_processing_params =
            ffi::CUBEB_INPUT_PROCESSING_PARAM_ECHO_CANCELLATION
                | ffi::CUBEB_INPUT_PROCESSING_PARAM_NOISE_SUPPRESSION
                | ffi::CUBEB_INPUT_PROCESSING_PARAM_AUTOMATIC_GAIN_CONTROL;
        assert_eq!(
            unsafe { OPS.stream_set_input_processing_params.unwrap()(stream, params) },
            ffi::CUBEB_OK
        );
        let stm = unsafe { &mut *(stream as *mut AudioUnitStream) };
        assert!(stm.core_stream_data.using_voice_processing_unit());
    });
}

#[test]
fn test_ops_duplex_voice_stream_set_input_processing_params_before_start() {
    if macos_kernel_major_version().unwrap() == MACOS_KERNEL_MAJOR_VERSION_MONTEREY {
        // We disable VPIO on Monterey.
        return;
    }
    test_default_duplex_voice_stream_operation(
        "duplex voice stream: processing before start",
        |stream| {
            let params: ffi::cubeb_input_processing_params =
                ffi::CUBEB_INPUT_PROCESSING_PARAM_ECHO_CANCELLATION
                    | ffi::CUBEB_INPUT_PROCESSING_PARAM_NOISE_SUPPRESSION
                    | ffi::CUBEB_INPUT_PROCESSING_PARAM_AUTOMATIC_GAIN_CONTROL;
            assert_eq!(
                unsafe { OPS.stream_set_input_processing_params.unwrap()(stream, params) },
                ffi::CUBEB_OK
            );
            assert_eq!(unsafe { OPS.stream_start.unwrap()(stream) }, ffi::CUBEB_OK);
            let stm = unsafe { &mut *(stream as *mut AudioUnitStream) };
            assert!(stm.core_stream_data.using_voice_processing_unit());
        },
    );
}

#[test]
fn test_ops_duplex_voice_stream_set_input_processing_params_before_start_with_reinit() {
    if macos_kernel_major_version().unwrap() == MACOS_KERNEL_MAJOR_VERSION_MONTEREY {
        // We disable VPIO on Monterey.
        return;
    }
    test_default_duplex_voice_stream_operation(
        "duplex voice stream: processing before start with reinit",
        |stream| {
            let params: ffi::cubeb_input_processing_params =
                ffi::CUBEB_INPUT_PROCESSING_PARAM_ECHO_CANCELLATION
                    | ffi::CUBEB_INPUT_PROCESSING_PARAM_NOISE_SUPPRESSION
                    | ffi::CUBEB_INPUT_PROCESSING_PARAM_AUTOMATIC_GAIN_CONTROL;
            assert_eq!(
                unsafe { OPS.stream_set_input_processing_params.unwrap()(stream, params) },
                ffi::CUBEB_OK
            );
            assert_eq!(unsafe { OPS.stream_start.unwrap()(stream) }, ffi::CUBEB_OK);

            // Hacky cast, but testing this here was simplest for now.
            let stm = unsafe { &mut *(stream as *mut AudioUnitStream) };
            assert!(stm.core_stream_data.using_voice_processing_unit());
            stm.reinit_async();
            let queue = stm.queue.clone();
            let mut params_after_reinit: ffi::cubeb_input_processing_params =
                ffi::CUBEB_INPUT_PROCESSING_PARAM_NONE;
            queue.run_sync(|| {
                let mut params: ffi::cubeb_input_processing_params =
                    ffi::CUBEB_INPUT_PROCESSING_PARAM_NONE;
                let mut agc: u32 = 0;
                let r = audio_unit_get_property(
                    stm.core_stream_data.input_unit,
                    kAUVoiceIOProperty_VoiceProcessingEnableAGC,
                    kAudioUnitScope_Global,
                    AU_IN_BUS,
                    &mut agc,
                    &mut mem::size_of::<u32>(),
                );
                assert_eq!(r, NO_ERR);
                if agc == 1 {
                    params = params | ffi::CUBEB_INPUT_PROCESSING_PARAM_AUTOMATIC_GAIN_CONTROL;
                }
                let mut bypass: u32 = 0;
                let r = audio_unit_get_property(
                    stm.core_stream_data.input_unit,
                    kAUVoiceIOProperty_BypassVoiceProcessing,
                    kAudioUnitScope_Global,
                    AU_IN_BUS,
                    &mut bypass,
                    &mut mem::size_of::<u32>(),
                );
                assert_eq!(r, NO_ERR);
                if bypass == 0 {
                    params = params
                        | ffi::CUBEB_INPUT_PROCESSING_PARAM_ECHO_CANCELLATION
                        | ffi::CUBEB_INPUT_PROCESSING_PARAM_NOISE_SUPPRESSION;
                }
                params_after_reinit = params;
            });
            assert_eq!(params, params_after_reinit);
        },
    );
}

#[test]
fn test_ops_duplex_voice_stream_set_input_processing_params_after_start() {
    if macos_kernel_major_version().unwrap() == MACOS_KERNEL_MAJOR_VERSION_MONTEREY {
        // We disable VPIO on Monterey.
        return;
    }
    test_default_duplex_voice_stream_operation(
        "duplex voice stream: processing after start",
        |stream| {
            let stm = unsafe { &mut *(stream as *mut AudioUnitStream) };
            assert!(stm.core_stream_data.using_voice_processing_unit());
            assert_eq!(unsafe { OPS.stream_start.unwrap()(stream) }, ffi::CUBEB_OK);
            let params: ffi::cubeb_input_processing_params =
                ffi::CUBEB_INPUT_PROCESSING_PARAM_ECHO_CANCELLATION
                    | ffi::CUBEB_INPUT_PROCESSING_PARAM_NOISE_SUPPRESSION
                    | ffi::CUBEB_INPUT_PROCESSING_PARAM_AUTOMATIC_GAIN_CONTROL;
            assert_eq!(
                unsafe { OPS.stream_set_input_processing_params.unwrap()(stream, params) },
                ffi::CUBEB_OK
            );
        },
    );
}

#[test]
fn test_ops_stereo_input_duplex_voice_stream_init_and_destroy() {
    test_stereo_input_duplex_voice_stream_operation(
        "stereo-input duplex voice stream: init and destroy",
        |stream| {
            let stm = unsafe { &mut *(stream as *mut AudioUnitStream) };
            assert_eq!(
                stm.core_stream_data.using_voice_processing_unit(),
                macos_kernel_major_version().unwrap() != MACOS_KERNEL_MAJOR_VERSION_MONTEREY
            );
        },
    );
}

#[test]
fn test_ops_stereo_input_duplex_voice_stream_start() {
    test_stereo_input_duplex_voice_stream_operation(
        "stereo-input duplex voice stream: start",
        |stream| {
            let stm = unsafe { &mut *(stream as *mut AudioUnitStream) };
            assert_eq!(
                stm.core_stream_data.using_voice_processing_unit(),
                macos_kernel_major_version().unwrap() != MACOS_KERNEL_MAJOR_VERSION_MONTEREY
            );
            assert_eq!(unsafe { OPS.stream_start.unwrap()(stream) }, ffi::CUBEB_OK);
        },
    );
}

#[test]
fn test_ops_stereo_input_duplex_voice_stream_stop() {
    test_stereo_input_duplex_voice_stream_operation(
        "stereo-input duplex voice stream: stop",
        |stream| {
            let stm = unsafe { &mut *(stream as *mut AudioUnitStream) };
            assert_eq!(
                stm.core_stream_data.using_voice_processing_unit(),
                macos_kernel_major_version().unwrap() != MACOS_KERNEL_MAJOR_VERSION_MONTEREY
            );
            assert_eq!(unsafe { OPS.stream_stop.unwrap()(stream) }, ffi::CUBEB_OK);
        },
    );
}
