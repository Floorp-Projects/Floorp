use super::utils::{test_get_default_device, test_ops_stream_operation, Scope};
use super::*;
use std::sync::atomic::AtomicI64;

#[test]
fn test_dial_tone() {
    use std::f32::consts::PI;
    use std::thread;
    use std::time::Duration;

    const SAMPLE_FREQUENCY: u32 = 48_000;

    // Do nothing if there is no available output device.
    if test_get_default_device(Scope::Output).is_none() {
        println!("No output device.");
        return;
    }

    // Make sure the parameters meet the requirements of AudioUnitContext::stream_init
    // (in the comments).
    let mut output_params = ffi::cubeb_stream_params::default();
    output_params.format = ffi::CUBEB_SAMPLE_S16NE;
    output_params.rate = SAMPLE_FREQUENCY;
    output_params.channels = 1;
    output_params.layout = ffi::CUBEB_LAYOUT_MONO;
    output_params.prefs = ffi::CUBEB_STREAM_PREF_NONE;

    struct Closure {
        buffer_size: AtomicI64,
        phase: i64,
    }
    let mut closure = Closure {
        buffer_size: AtomicI64::new(0),
        phase: 0,
    };
    let closure_ptr = &mut closure as *mut Closure as *mut c_void;

    test_ops_stream_operation(
        "stream: North American dial tone",
        ptr::null_mut(), // Use default input device.
        ptr::null_mut(), // No input parameters.
        ptr::null_mut(), // Use default output device.
        &mut output_params,
        4096, // TODO: Get latency by get_min_latency instead ?
        Some(data_callback),
        Some(state_callback),
        closure_ptr,
        |stream| {
            assert_eq!(unsafe { OPS.stream_start.unwrap()(stream) }, ffi::CUBEB_OK);

            #[derive(Debug)]
            enum State {
                WaitingForStart,
                PositionIncreasing,
                Paused,
                Resumed,
                End,
            }
            let mut state = State::WaitingForStart;
            let mut position: u64 = 0;
            let mut prev_position: u64 = 0;
            let mut count = 0;
            const CHECK_COUNT: i32 = 10;
            loop {
                thread::sleep(Duration::from_millis(50));
                assert_eq!(
                    unsafe { OPS.stream_get_position.unwrap()(stream, &mut position) },
                    ffi::CUBEB_OK
                );
                println!(
                    "State: {:?}, position: {}, previous position: {}",
                    state, position, prev_position
                );
                match &mut state {
                    State::WaitingForStart => {
                        // It's expected to have 0 for a few iterations here: the stream can take
                        // some time to start.
                        if position != prev_position {
                            assert!(position > prev_position);
                            prev_position = position;
                            state = State::PositionIncreasing;
                        }
                    }
                    State::PositionIncreasing => {
                        // wait a few iterations, check monotony
                        if position != prev_position {
                            assert!(position > prev_position);
                            prev_position = position;
                            count += 1;
                            if count > CHECK_COUNT {
                                state = State::Paused;
                                count = 0;
                                assert_eq!(
                                    unsafe { OPS.stream_stop.unwrap()(stream) },
                                    ffi::CUBEB_OK
                                );
                                // Update the position once paused.
                                assert_eq!(
                                    unsafe {
                                        OPS.stream_get_position.unwrap()(stream, &mut position)
                                    },
                                    ffi::CUBEB_OK
                                );
                                prev_position = position;
                            }
                        }
                    }
                    State::Paused => {
                        // The cubeb_stream_stop call above should synchrously stop the callbacks,
                        // hence the clock, the assert below must always holds, modulo the client
                        // side interpolation.
                        assert!(
                            position == prev_position
                                || position - prev_position
                                    <= closure.buffer_size.load(Ordering::SeqCst) as u64
                        );
                        count += 1;
                        prev_position = position;
                        if count > CHECK_COUNT {
                            state = State::Resumed;
                            count = 0;
                            assert_eq!(unsafe { OPS.stream_start.unwrap()(stream) }, ffi::CUBEB_OK);
                        }
                    }
                    State::Resumed => {
                        // wait a few iterations, this can take some time to start
                        if position != prev_position {
                            assert!(position > prev_position);
                            prev_position = position;
                            count += 1;
                            if count > CHECK_COUNT {
                                state = State::End;
                                count = 0;
                                assert_eq!(
                                    unsafe { OPS.stream_stop.unwrap()(stream) },
                                    ffi::CUBEB_OK
                                );
                                assert_eq!(
                                    unsafe {
                                        OPS.stream_get_position.unwrap()(stream, &mut position)
                                    },
                                    ffi::CUBEB_OK
                                );
                                prev_position = position;
                            }
                        }
                    }
                    State::End => {
                        // The cubeb_stream_stop call above should synchrously stop the callbacks,
                        // hence the clock, the assert below must always holds, modulo the client
                        // side interpolation.
                        assert!(
                            position == prev_position
                                || position - prev_position
                                    <= closure.buffer_size.load(Ordering::SeqCst) as u64
                        );
                        if position == prev_position {
                            count += 1;
                            if count > CHECK_COUNT {
                                break;
                            }
                        }
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

        let closure = unsafe { &mut *(user_ptr as *mut Closure) };

        closure.buffer_size.store(nframes, Ordering::SeqCst);

        // Generate tone on the fly.
        for data in buffer.iter_mut() {
            let t1 = (2.0 * PI * 350.0 * (closure.phase) as f32 / SAMPLE_FREQUENCY as f32).sin();
            let t2 = (2.0 * PI * 440.0 * (closure.phase) as f32 / SAMPLE_FREQUENCY as f32).sin();
            *data = f32_to_i16_sample(0.45 * (t1 + t2));
            closure.phase += 1;
        }

        nframes
    }

    fn f32_to_i16_sample(x: f32) -> i16 {
        (x * f32::from(i16::max_value())) as i16
    }
}
