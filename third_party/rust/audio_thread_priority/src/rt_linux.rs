/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Widely copied from dbus-rs/dbus/examples/rtkit.rs */

extern crate dbus;
extern crate libc;

use std::cmp;
use std::error::Error;

use dbus::{Connection, BusType, Props, MessageItem, Message};

const DBUS_SOCKET_TIMEOUT: i32 = 10_000;
const RT_PRIO_DEFAULT: u32 = 10;
// This is different from libc::pid_t, which is 32 bits, and is defined in sys/types.h.
#[allow(non_camel_case_types)]
type kernel_pid_t = libc::c_long;

/*#[derive(Debug)]*/
pub struct RtPriorityHandleInternal {
    /// Process-local thread id, used to restore scheduler characteristics.
    pthread_id: libc::pthread_t,
    /// The scheduler originaly associated with this thread (probably SCHED_OTHER).
    policy: libc::c_int,
    /// The initial priority for this thread.
    param: libc::sched_param,
}

fn item_as_i64(i: MessageItem) -> Result<i64, Box<dyn Error>> {
    match i {
        MessageItem::Int32(i) => Ok(i as i64),
        MessageItem::Int64(i) => Ok(i),
        _ => Err(Box::from(&*format!("Property is not integer ({:?})", i)))
    }
}

fn rtkit_set_realtime(c: &Connection, thread: u64, prio: u32) -> Result<(), Box<dyn Error>> {
    let mut m = Message::new_method_call("org.freedesktop.RealtimeKit1",
                                         "/org/freedesktop/RealtimeKit1",
                                         "org.freedesktop.RealtimeKit1",
                                         "MakeThreadRealtime")?;
    m.append_items(&[thread.into(), prio.into()]);
    c.send_with_reply_and_block(m, DBUS_SOCKET_TIMEOUT)?;
    return Ok(());
}

fn make_realtime(tid: kernel_pid_t, requested_slice_us: u64, prio: u32) -> Result<u32, Box<dyn Error>> {
    let c = Connection::get_private(BusType::System)?;

    let p = Props::new(&c, "org.freedesktop.RealtimeKit1", "/org/freedesktop/RealtimeKit1",
        "org.freedesktop.RealtimeKit1", DBUS_SOCKET_TIMEOUT);

    // Make sure we don't fail by wanting too much
    let max_prio = item_as_i64(p.get("MaxRealtimePriority")?)?;
    if max_prio < 0 {
        return Err(Box::from("invalid negative MaxRealtimePriority"));
    }
    let prio = cmp::min(prio, max_prio as u32);

    // Enforce RLIMIT_RTPRIO, also a must before asking rtkit for rtprio
    let max_rttime = item_as_i64(p.get("RTTimeUSecMax")?)?;
    if max_rttime < 0 {
        return Err(Box::from("invalid negative RTTimeUSecMax"));
    }

    // Only take what we need, or cap at the system limit, no further.
    let rttime_request = cmp::min(requested_slice_us, max_rttime as u64);

    // Set a soft limit to the limit requested, to be able to handle going over the limit using
    // SIXCPU. Set the hard limit to the maxium slice to prevent getting SIGKILL.
    let new_limit = libc::rlimit64 { rlim_cur: rttime_request,
                                     rlim_max: max_rttime as u64 };
    let mut old_limit = new_limit;
    if unsafe { libc::getrlimit64(libc::RLIMIT_RTTIME, &mut old_limit) } < 0 {
        return Err(Box::from("getrlimit failed"));
    }
    if unsafe { libc::setrlimit64(libc::RLIMIT_RTTIME, &new_limit) } < 0 {
        return Err(Box::from("setrlimit failed"));
    }

    // Finally, let's ask rtkit to make us realtime
    let r = rtkit_set_realtime(&c, tid as u64, prio);

    if r.is_err() {
        unsafe { libc::setrlimit64(libc::RLIMIT_RTTIME, &old_limit) };
        return Err(Box::from("could not set process as real-time."));
    }

    Ok(prio)
}

pub fn promote_current_thread_to_real_time_internal(audio_buffer_frames: u32,
                                                    audio_samplerate_hz: u32) -> Result<RtPriorityHandleInternal, ()>
{
    let thread_id = unsafe { libc::syscall(libc::SYS_gettid) };
    let pthread_id = unsafe { libc::pthread_self() };
    let mut param = unsafe { std::mem::zeroed::<libc::sched_param>() };
    let mut policy = 0;

    if unsafe { libc::pthread_getschedparam(pthread_id, &mut policy, &mut param) } < 0 {
        error!("pthread_getschedparam error {}", pthread_id);
        return Err(());
    }

    let buffer_frames = if audio_buffer_frames > 0 {
        audio_buffer_frames
    } else {
        // 50ms slice. This "ought to be enough for anybody".
        audio_samplerate_hz / 20
    };
    let budget_us = (buffer_frames * 1_000_000 / audio_samplerate_hz) as u64;
    let handle = RtPriorityHandleInternal { pthread_id, policy, param};
    let r = make_realtime(thread_id, budget_us, RT_PRIO_DEFAULT);
    if r.is_err() {
        warn!("Could not make thread real-time.");
        return Err(());
    }
    return Ok(handle);
}

pub fn demote_current_thread_from_real_time_internal(rt_priority_handle: RtPriorityHandleInternal)
                                            -> Result<(), ()> {
    assert!(unsafe { libc::pthread_self() } == rt_priority_handle.pthread_id);

    if unsafe { libc::pthread_setschedparam(rt_priority_handle.pthread_id,
                                            rt_priority_handle.policy,
                                            &rt_priority_handle.param) } < 0 {
        warn!("could not demote thread {}", rt_priority_handle.pthread_id);
        return Err(());
    }
    return Ok(());
}

