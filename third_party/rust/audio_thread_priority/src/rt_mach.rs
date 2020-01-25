/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

use mach::kern_return::{kern_return_t, KERN_SUCCESS};
use mach::port::mach_port_t;
use mach::message::mach_msg_type_number_t;
use crate::mach_sys::*;
use std::mem::size_of;
use mach::mach_time::{mach_timebase_info_data_t, mach_timebase_info};
use libc::{pthread_t, pthread_self};

extern "C" {
    fn pthread_mach_thread_np(tid: pthread_t) -> mach_port_t;
    // Those functions are commented out in thread_policy.h but somehow it works just fine !?
    fn thread_policy_set(thread: thread_t,
                         flavor: thread_policy_flavor_t,
                         policy_info: thread_policy_t,
                         count: mach_msg_type_number_t)
                         -> kern_return_t;
    fn thread_policy_get(thread: thread_t,
                         flavor: thread_policy_flavor_t,
                         policy_info: thread_policy_t,
                         count: &mut mach_msg_type_number_t,
                         get_default: &mut boolean_t)
                         -> kern_return_t;
}

// can't use size_of in const fn just now in stable, use a macro for now.
macro_rules! THREAD_TIME_CONSTRAINT_POLICY_COUNT {
    () => {
        (size_of::<thread_time_constraint_policy_data_t>() / size_of::<integer_t>()) as u32;
    }
}

#[derive(Debug)]
pub struct RtPriorityHandleInternal {
    tid: mach_port_t,
    previous_time_constraint_policy: thread_time_constraint_policy_data_t,
}

impl RtPriorityHandleInternal {
    pub fn new() -> RtPriorityHandleInternal {
        return RtPriorityHandleInternal {
            tid: 0,
            previous_time_constraint_policy: thread_time_constraint_policy_data_t {
                period: 0,
                computation: 0,
                constraint: 0,
                preemptible: 0
            }
        }
    }
}

pub fn demote_current_thread_from_real_time_internal(rt_priority_handle: RtPriorityHandleInternal)
                                            -> Result<(), ()> {
    unsafe {
        let rv: kern_return_t;
        let mut h = rt_priority_handle;
        rv = thread_policy_set(h.tid,
                               THREAD_TIME_CONSTRAINT_POLICY,
                               (&mut h.previous_time_constraint_policy) as *mut _ as
                               thread_policy_t,
                               THREAD_TIME_CONSTRAINT_POLICY_COUNT!());
        if rv != KERN_SUCCESS as i32 {
            warn!("thread demotion error: thread_policy_set: RT");
            return Err(());
        }

        warn!("thread {} priority restored.", h.tid);
    }

    return Ok(());
}

pub fn promote_current_thread_to_real_time_internal(audio_buffer_frames: u32,
                                           audio_samplerate_hz: u32)
                                           -> Result<RtPriorityHandleInternal, ()> {

    let mut rt_priority_handle = RtPriorityHandleInternal::new();

    let buffer_frames = if audio_buffer_frames > 0 {
        audio_buffer_frames
    } else {
        audio_samplerate_hz / 20
    };

    unsafe {
        let tid: mach_port_t = pthread_mach_thread_np(pthread_self());
        let mut rv: kern_return_t;
        let mut time_constraints = thread_time_constraint_policy_data_t {
            period: 0,
            computation: 0,
            constraint: 0,
            preemptible: 0,
        };
        let mut count: mach_msg_type_number_t;


        // Get current thread attributes, to revert back to the correct setting later if needed.
        rt_priority_handle.tid = tid;

        // false: we want to get the current value, not the default value. If this is `false` after
        // returning, it means there are no current settings because of other factor, and the
        // default was returned instead.
        let mut get_default: boolean_t = 0;
        count = THREAD_TIME_CONSTRAINT_POLICY_COUNT!();
        rv = thread_policy_get(tid,
                               THREAD_TIME_CONSTRAINT_POLICY,
                               (&mut time_constraints) as *mut _ as thread_policy_t,
                               &mut count,
                               &mut get_default);

        if rv != KERN_SUCCESS as i32 {
            error!("thread promotion error: thread_policy_get: time_constraint");
            return Err(());
        }

        rt_priority_handle.previous_time_constraint_policy = time_constraints;

        let cb_duration = buffer_frames as f32 / (audio_samplerate_hz as f32) * 1000.;
        // The multiplicators are somwhat arbitrary for now.

        let mut timebase_info = mach_timebase_info_data_t { denom: 0, numer: 0 };
        mach_timebase_info(&mut timebase_info);

        let ms2abs: f32 = ((timebase_info.denom as f32) / timebase_info.numer as f32) * 1000000.;

        time_constraints = thread_time_constraint_policy_data_t {
            period: (cb_duration * ms2abs) as u32,
            computation: (0.3 * ms2abs) as u32, // fixed 300us computation time
            constraint: (cb_duration * ms2abs) as u32,
            preemptible: 1, // true
        };

        rv = thread_policy_set(tid,
                               THREAD_TIME_CONSTRAINT_POLICY,
                               (&mut time_constraints) as *mut _ as thread_policy_t,
                               THREAD_TIME_CONSTRAINT_POLICY_COUNT!());
        if rv != KERN_SUCCESS as i32 {
            warn!("thread promotion error: thread_policy_set: time_constraint");
            return Err(());
        }

        info!("thread {} bumped to real time priority.", tid);
    }

    Ok(rt_priority_handle)
}
