use super::utils::{
    noop_data_callback, test_audiounit_get_buffer_frame_size, test_get_default_audiounit,
    test_get_default_device, test_ops_context_operation, PropertyScope, Scope,
};
use super::*;
use std::thread;

// Ignore the test by default to avoid overwritting the buffer frame size of the device that is
// currently used by other streams in other tests.
#[ignore]
#[test]
fn test_parallel_ops_init_streams_in_parallel_input() {
    const THREADS: u32 = 50;
    create_streams_by_ops_in_parallel_with_different_latency(
        THREADS,
        StreamType::Input,
        |streams| {
            // All the latency frames should be the same value as the first stream's one, since the
            // latency frames cannot be changed if another stream is operating in parallel.
            let mut latency_frames = vec![];
            let mut in_buffer_frame_sizes = vec![];

            for stream in streams {
                latency_frames.push(stream.latency_frames);

                assert!(!stream.core_stream_data.input_unit.is_null());
                let in_buffer_frame_size = test_audiounit_get_buffer_frame_size(
                    stream.core_stream_data.input_unit,
                    Scope::Input,
                    PropertyScope::Output,
                )
                .unwrap();
                in_buffer_frame_sizes.push(in_buffer_frame_size);

                assert!(stream.core_stream_data.output_unit.is_null());
            }

            // Make sure all the latency frames are same as the first stream's one.
            for i in 0..latency_frames.len() - 1 {
                assert_eq!(latency_frames[i], latency_frames[i + 1]);
            }

            // Make sure all the buffer frame sizes on output scope of the input audiounit are same
            // as the defined latency of the first initial stream.
            for i in 0..in_buffer_frame_sizes.len() - 1 {
                assert_eq!(in_buffer_frame_sizes[i], in_buffer_frame_sizes[i + 1]);
            }
        },
    );
}

// Ignore the test by default to avoid overwritting the buffer frame size of the device that is
// currently used by other streams in other tests.
#[ignore]
#[test]
fn test_parallel_ops_init_streams_in_parallel_output() {
    const THREADS: u32 = 50;
    create_streams_by_ops_in_parallel_with_different_latency(
        THREADS,
        StreamType::Output,
        |streams| {
            // All the latency frames should be the same value as the first stream's one, since the
            // latency frames cannot be changed if another stream is operating in parallel.
            let mut latency_frames = vec![];
            let mut out_buffer_frame_sizes = vec![];

            for stream in streams {
                latency_frames.push(stream.latency_frames);

                assert!(stream.core_stream_data.input_unit.is_null());

                assert!(!stream.core_stream_data.output_unit.is_null());
                let out_buffer_frame_size = test_audiounit_get_buffer_frame_size(
                    stream.core_stream_data.output_unit,
                    Scope::Output,
                    PropertyScope::Input,
                )
                .unwrap();
                out_buffer_frame_sizes.push(out_buffer_frame_size);
            }

            // Make sure all the latency frames are same as the first stream's one.
            for i in 0..latency_frames.len() - 1 {
                assert_eq!(latency_frames[i], latency_frames[i + 1]);
            }

            // Make sure all the buffer frame sizes on input scope of the output audiounit are same
            // as the defined latency of the first initial stream.
            for i in 0..out_buffer_frame_sizes.len() - 1 {
                assert_eq!(out_buffer_frame_sizes[i], out_buffer_frame_sizes[i + 1]);
            }
        },
    );
}

// Ignore the test by default to avoid overwritting the buffer frame size of the device that is
// currently used by other streams in other tests.
#[ignore]
#[test]
fn test_parallel_ops_init_streams_in_parallel_duplex() {
    const THREADS: u32 = 50;
    create_streams_by_ops_in_parallel_with_different_latency(
        THREADS,
        StreamType::Duplex,
        |streams| {
            // All the latency frames should be the same value as the first stream's one, since the
            // latency frames cannot be changed if another stream is operating in parallel.
            let mut latency_frames = vec![];
            let mut in_buffer_frame_sizes = vec![];
            let mut out_buffer_frame_sizes = vec![];

            for stream in streams {
                latency_frames.push(stream.latency_frames);

                assert!(!stream.core_stream_data.input_unit.is_null());
                let in_buffer_frame_size = test_audiounit_get_buffer_frame_size(
                    stream.core_stream_data.input_unit,
                    Scope::Input,
                    PropertyScope::Output,
                )
                .unwrap();
                in_buffer_frame_sizes.push(in_buffer_frame_size);

                assert!(!stream.core_stream_data.output_unit.is_null());
                let out_buffer_frame_size = test_audiounit_get_buffer_frame_size(
                    stream.core_stream_data.output_unit,
                    Scope::Output,
                    PropertyScope::Input,
                )
                .unwrap();
                out_buffer_frame_sizes.push(out_buffer_frame_size);
            }

            // Make sure all the latency frames are same as the first stream's one.
            for i in 0..latency_frames.len() - 1 {
                assert_eq!(latency_frames[i], latency_frames[i + 1]);
            }

            // Make sure all the buffer frame sizes on output scope of the input audiounit are same
            // as the defined latency of the first initial stream.
            for i in 0..in_buffer_frame_sizes.len() - 1 {
                assert_eq!(in_buffer_frame_sizes[i], in_buffer_frame_sizes[i + 1]);
            }

            // Make sure all the buffer frame sizes on input scope of the output audiounit are same
            // as the defined latency of the first initial stream.
            for i in 0..out_buffer_frame_sizes.len() - 1 {
                assert_eq!(out_buffer_frame_sizes[i], out_buffer_frame_sizes[i + 1]);
            }
        },
    );
}

fn create_streams_by_ops_in_parallel_with_different_latency<F>(
    amount: u32,
    stm_type: StreamType,
    callback: F,
) where
    F: FnOnce(Vec<&AudioUnitStream>),
{
    let default_input = test_get_default_device(Scope::Input);
    let default_output = test_get_default_device(Scope::Output);

    let has_input = stm_type == StreamType::Input || stm_type == StreamType::Duplex;
    let has_output = stm_type == StreamType::Output || stm_type == StreamType::Duplex;

    if has_input && default_input.is_none() {
        println!("No input device to perform the test.");
        return;
    }

    if has_output && default_output.is_none() {
        println!("No output device to perform the test.");
        return;
    }

    test_ops_context_operation("context: init and destroy", |context_ptr| {
        let context_ptr_value = context_ptr as usize;

        let mut join_handles = vec![];
        for i in 0..amount {
            // Make sure the parameters meet the requirements of AudioUnitContext::stream_init
            // (in the comments).
            let mut input_params = ffi::cubeb_stream_params::default();
            input_params.format = ffi::CUBEB_SAMPLE_FLOAT32NE;
            input_params.rate = 48_000;
            input_params.channels = 1;
            input_params.layout = ffi::CUBEB_LAYOUT_UNDEFINED;
            input_params.prefs = ffi::CUBEB_STREAM_PREF_NONE;

            let mut output_params = ffi::cubeb_stream_params::default();
            output_params.format = ffi::CUBEB_SAMPLE_FLOAT32NE;
            output_params.rate = 44100;
            output_params.channels = 2;
            output_params.layout = ffi::CUBEB_LAYOUT_UNDEFINED;
            output_params.prefs = ffi::CUBEB_STREAM_PREF_NONE;

            // Latency cannot be changed if another stream is operating in parallel. All the latecy
            // should be set to the same latency value of the first stream that is operating in the
            // context.
            let latency_frames = SAFE_MIN_LATENCY_FRAMES + i;
            assert!(latency_frames < SAFE_MAX_LATENCY_FRAMES);

            // Create many streams within the same context. The order of the stream creation
            // is random (The order of execution of the spawned threads is random.).assert!
            // It's super dangerous to pass `context_ptr_value` across threads and convert it back
            // to a pointer. However, it's the cheapest way to make sure the inside mutex works.
            let thread_name = format!("stream {} @ context {:?}", i, context_ptr);
            join_handles.push(
                thread::Builder::new()
                    .name(thread_name)
                    .spawn(move || {
                        let context_ptr = context_ptr_value as *mut ffi::cubeb;
                        let mut stream: *mut ffi::cubeb_stream = ptr::null_mut();
                        let stream_name = CString::new(format!("stream {}", i)).unwrap();
                        assert_eq!(
                            unsafe {
                                OPS.stream_init.unwrap()(
                                    context_ptr,
                                    &mut stream,
                                    stream_name.as_ptr(),
                                    ptr::null_mut(), // Use default input device.
                                    if has_input {
                                        &mut input_params
                                    } else {
                                        ptr::null_mut()
                                    },
                                    ptr::null_mut(), // Use default output device.
                                    if has_output {
                                        &mut output_params
                                    } else {
                                        ptr::null_mut()
                                    },
                                    latency_frames,
                                    Some(noop_data_callback),
                                    None,            // No state callback.
                                    ptr::null_mut(), // No user data pointer.
                                )
                            },
                            ffi::CUBEB_OK
                        );
                        assert!(!stream.is_null());
                        stream as usize
                    })
                    .unwrap(),
            );
        }

        let mut streams = vec![];
        // Wait for finishing the tasks on the different threads.
        for handle in join_handles {
            let stream_ptr_value = handle.join().unwrap();
            let stream = unsafe { Box::from_raw(stream_ptr_value as *mut AudioUnitStream) };
            streams.push(stream);
        }

        let stream_refs: Vec<&AudioUnitStream> = streams.iter().map(|stm| stm.as_ref()).collect();
        callback(stream_refs);
    });
}

// Ignore the test by default to avoid overwritting the buffer frame size of the device that is
// currently used by other streams in other tests.
#[ignore]
#[test]
fn test_parallel_init_streams_in_parallel_input() {
    const THREADS: u32 = 10;
    create_streams_in_parallel_with_different_latency(THREADS, StreamType::Input, |streams| {
        // All the latency frames should be the same value as the first stream's one, since the
        // latency frames cannot be changed if another stream is operating in parallel.
        let mut latency_frames = vec![];
        let mut in_buffer_frame_sizes = vec![];

        for stream in streams {
            latency_frames.push(stream.latency_frames);

            assert!(!stream.core_stream_data.input_unit.is_null());
            let in_buffer_frame_size = test_audiounit_get_buffer_frame_size(
                stream.core_stream_data.input_unit,
                Scope::Input,
                PropertyScope::Output,
            )
            .unwrap();
            in_buffer_frame_sizes.push(in_buffer_frame_size);

            assert!(stream.core_stream_data.output_unit.is_null());
        }

        // Make sure all the latency frames are same as the first stream's one.
        for i in 0..latency_frames.len() - 1 {
            assert_eq!(latency_frames[i], latency_frames[i + 1]);
        }

        // Make sure all the buffer frame sizes on output scope of the input audiounit are same
        // as the defined latency of the first initial stream.
        for i in 0..in_buffer_frame_sizes.len() - 1 {
            assert_eq!(in_buffer_frame_sizes[i], in_buffer_frame_sizes[i + 1]);
        }
    });
}

// Ignore the test by default to avoid overwritting the buffer frame size of the device that is
// currently used by other streams in other tests.
#[ignore]
#[test]
fn test_parallel_init_streams_in_parallel_output() {
    const THREADS: u32 = 10;
    create_streams_in_parallel_with_different_latency(THREADS, StreamType::Output, |streams| {
        // All the latency frames should be the same value as the first stream's one, since the
        // latency frames cannot be changed if another stream is operating in parallel.
        let mut latency_frames = vec![];
        let mut out_buffer_frame_sizes = vec![];

        for stream in streams {
            latency_frames.push(stream.latency_frames);

            assert!(stream.core_stream_data.input_unit.is_null());

            assert!(!stream.core_stream_data.output_unit.is_null());
            let out_buffer_frame_size = test_audiounit_get_buffer_frame_size(
                stream.core_stream_data.output_unit,
                Scope::Output,
                PropertyScope::Input,
            )
            .unwrap();
            out_buffer_frame_sizes.push(out_buffer_frame_size);
        }

        // Make sure all the latency frames are same as the first stream's one.
        for i in 0..latency_frames.len() - 1 {
            assert_eq!(latency_frames[i], latency_frames[i + 1]);
        }

        // Make sure all the buffer frame sizes on input scope of the output audiounit are same
        // as the defined latency of the first initial stream.
        for i in 0..out_buffer_frame_sizes.len() - 1 {
            assert_eq!(out_buffer_frame_sizes[i], out_buffer_frame_sizes[i + 1]);
        }
    });
}

// Ignore the test by default to avoid overwritting the buffer frame size of the device that is
// currently used by other streams in other tests.
#[ignore]
#[test]
fn test_parallel_init_streams_in_parallel_duplex() {
    const THREADS: u32 = 10;
    create_streams_in_parallel_with_different_latency(THREADS, StreamType::Duplex, |streams| {
        // All the latency frames should be the same value as the first stream's one, since the
        // latency frames cannot be changed if another stream is operating in parallel.
        let mut latency_frames = vec![];
        let mut in_buffer_frame_sizes = vec![];
        let mut out_buffer_frame_sizes = vec![];

        for stream in streams {
            latency_frames.push(stream.latency_frames);

            assert!(!stream.core_stream_data.input_unit.is_null());
            let in_buffer_frame_size = test_audiounit_get_buffer_frame_size(
                stream.core_stream_data.input_unit,
                Scope::Input,
                PropertyScope::Output,
            )
            .unwrap();
            in_buffer_frame_sizes.push(in_buffer_frame_size);

            assert!(!stream.core_stream_data.output_unit.is_null());
            let out_buffer_frame_size = test_audiounit_get_buffer_frame_size(
                stream.core_stream_data.output_unit,
                Scope::Output,
                PropertyScope::Input,
            )
            .unwrap();
            out_buffer_frame_sizes.push(out_buffer_frame_size);
        }

        // Make sure all the latency frames are same as the first stream's one.
        for i in 0..latency_frames.len() - 1 {
            assert_eq!(latency_frames[i], latency_frames[i + 1]);
        }

        // Make sure all the buffer frame sizes on output scope of the input audiounit are same
        // as the defined latency of the first initial stream.
        for i in 0..in_buffer_frame_sizes.len() - 1 {
            assert_eq!(in_buffer_frame_sizes[i], in_buffer_frame_sizes[i + 1]);
        }

        // Make sure all the buffer frame sizes on input scope of the output audiounit are same
        // as the defined latency of the first initial stream.
        for i in 0..out_buffer_frame_sizes.len() - 1 {
            assert_eq!(out_buffer_frame_sizes[i], out_buffer_frame_sizes[i + 1]);
        }
    });
}

fn create_streams_in_parallel_with_different_latency<F>(
    amount: u32,
    stm_type: StreamType,
    callback: F,
) where
    F: FnOnce(Vec<&AudioUnitStream>),
{
    let default_input = test_get_default_device(Scope::Input);
    let default_output = test_get_default_device(Scope::Output);

    let has_input = stm_type == StreamType::Input || stm_type == StreamType::Duplex;
    let has_output = stm_type == StreamType::Output || stm_type == StreamType::Duplex;

    if has_input && default_input.is_none() {
        println!("No input device to perform the test.");
        return;
    }

    if has_output && default_output.is_none() {
        println!("No output device to perform the test.");
        return;
    }

    let mut context = AudioUnitContext::new();

    let context_ptr_value = &mut context as *mut AudioUnitContext as usize;

    let mut join_handles = vec![];
    for i in 0..amount {
        // Make sure the parameters meet the requirements of AudioUnitContext::stream_init
        // (in the comments).
        let mut input_params = ffi::cubeb_stream_params::default();
        input_params.format = ffi::CUBEB_SAMPLE_FLOAT32NE;
        input_params.rate = 48_000;
        input_params.channels = 1;
        input_params.layout = ffi::CUBEB_LAYOUT_UNDEFINED;
        input_params.prefs = ffi::CUBEB_STREAM_PREF_NONE;

        let mut output_params = ffi::cubeb_stream_params::default();
        output_params.format = ffi::CUBEB_SAMPLE_FLOAT32NE;
        output_params.rate = 44100;
        output_params.channels = 2;
        output_params.layout = ffi::CUBEB_LAYOUT_UNDEFINED;
        output_params.prefs = ffi::CUBEB_STREAM_PREF_NONE;

        // Latency cannot be changed if another stream is operating in parallel. All the latecy
        // should be set to the same latency value of the first stream that is operating in the
        // context.
        let latency_frames = SAFE_MIN_LATENCY_FRAMES + i;
        assert!(latency_frames < SAFE_MAX_LATENCY_FRAMES);

        // Create many streams within the same context. The order of the stream creation
        // is random. (The order of execution of the spawned threads is random.)
        // It's super dangerous to pass `context_ptr_value` across threads and convert it back
        // to a reference. However, it's the cheapest way to make sure the inside mutex works.
        let thread_name = format!("stream {} @ context {:?}", i, context_ptr_value);
        join_handles.push(
            thread::Builder::new()
                .name(thread_name)
                .spawn(move || {
                    let context = unsafe { &mut *(context_ptr_value as *mut AudioUnitContext) };
                    let input_params = unsafe { StreamParamsRef::from_ptr(&mut input_params) };
                    let output_params = unsafe { StreamParamsRef::from_ptr(&mut output_params) };
                    let stream = context
                        .stream_init(
                            None,
                            ptr::null_mut(), // Use default input device.
                            if has_input { Some(input_params) } else { None },
                            ptr::null_mut(), // Use default output device.
                            if has_output {
                                Some(output_params)
                            } else {
                                None
                            },
                            latency_frames,
                            Some(noop_data_callback),
                            None,            // No state callback.
                            ptr::null_mut(), // No user data pointer.
                        )
                        .unwrap();
                    assert!(!stream.as_ptr().is_null());
                    let stream_ptr_value = stream.as_ptr() as usize;
                    // Prevent the stream from being destroyed by leaking this stream.
                    mem::forget(stream);
                    stream_ptr_value
                })
                .unwrap(),
        );
    }

    let mut streams = vec![];
    // Wait for finishing the tasks on the different threads.
    for handle in join_handles {
        let stream_ptr_value = handle.join().unwrap();
        // Retake the leaked stream.
        let stream = unsafe { Box::from_raw(stream_ptr_value as *mut AudioUnitStream) };
        streams.push(stream);
    }

    let stream_refs: Vec<&AudioUnitStream> = streams.iter().map(|stm| stm.as_ref()).collect();
    callback(stream_refs);
}

#[derive(Debug, PartialEq)]
enum StreamType {
    Input,
    Output,
    Duplex,
}

// This is used to interfere other active streams.
// From this testing, it's ok to set the buffer frame size of a device that is currently used by
// other tests. It works on OSX 10.13, not sure if it works on other versions.
// However, other tests may check the buffer frame size they set at the same time,
// so we ignore this by default incase those checks fail.
#[ignore]
#[test]
fn test_set_buffer_frame_size_in_parallel() {
    test_set_buffer_frame_size_in_parallel_in_scope(Scope::Input);
    test_set_buffer_frame_size_in_parallel_in_scope(Scope::Output);
}

fn test_set_buffer_frame_size_in_parallel_in_scope(scope: Scope) {
    const THREADS: u32 = 100;

    let unit = test_get_default_audiounit(scope.clone());
    if unit.is_none() {
        println!("No unit for {:?}", scope);
        return;
    }

    let (unit_scope, unit_element, prop_scope) = match scope {
        Scope::Input => (kAudioUnitScope_Output, AU_IN_BUS, PropertyScope::Output),
        Scope::Output => (kAudioUnitScope_Input, AU_OUT_BUS, PropertyScope::Input),
    };

    let mut units = vec![];
    let mut join_handles = vec![];
    for i in 0..THREADS {
        let latency_frames = SAFE_MIN_LATENCY_FRAMES + i;
        assert!(latency_frames < SAFE_MAX_LATENCY_FRAMES);
        units.push(test_get_default_audiounit(scope.clone()).unwrap());
        let unit_value = units.last().unwrap().get_inner() as usize;
        join_handles.push(thread::spawn(move || {
            let status = audio_unit_set_property(
                unit_value as AudioUnit,
                kAudioDevicePropertyBufferFrameSize,
                unit_scope,
                unit_element,
                &latency_frames,
                mem::size_of::<u32>(),
            );
            (latency_frames, status)
        }));
    }

    let mut latencies = vec![];
    let mut statuses = vec![];
    for handle in join_handles {
        let (latency, status) = handle.join().unwrap();
        latencies.push(latency);
        statuses.push(status);
    }

    let mut buffer_frames_list = vec![];
    for unit in units.iter() {
        buffer_frames_list.push(unit.get_buffer_frame_size(scope.clone(), prop_scope.clone()));
    }

    for status in statuses {
        assert_eq!(status, NO_ERR);
    }

    for i in 0..buffer_frames_list.len() - 1 {
        assert_eq!(buffer_frames_list[i], buffer_frames_list[i + 1]);
    }
}
