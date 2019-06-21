/* This example asks the rtkit service to make our thread realtime priority.
   Rtkit puts a few limitations on us to let us become realtime, such as setting
   RLIMIT_RTTIME correctly, hence the syscalls. */

extern crate dbus;
extern crate libc;

use std::cmp;
use libc::c_int;

use dbus::{Connection, BusType, Props, MessageItem, Message};

/* External C stuff. Currently only working for x86_64 */

#[allow(non_upper_case_globals)]
#[cfg(target_arch = "x86_64")]
static SYS_gettid: c_int = 186;

static RLIMIT_RTTIME: c_int = 15;

#[repr(C)]
#[cfg(target_arch = "x86_64")]
#[derive(Copy, Clone)]
struct rlimit {
    rlim_cur: u64,
    rlim_max: u64,
}

extern "C" {
    fn syscall(num: c_int, ...) -> c_int;

    fn getrlimit(resource: c_int, rlim: *mut rlimit) -> c_int;
    fn setrlimit(resource: c_int, rlim: *const rlimit) -> c_int;
}

fn item_as_i64(i: MessageItem) -> Result<i64, Box<std::error::Error>> {
    match i {
        MessageItem::Int32(i) => Ok(i as i64),
        MessageItem::Int64(i) => Ok(i),
        _ => Err(Box::from(&*format!("Property is not integer ({:?})", i)))
    }
}

fn rtkit_set_realtime(c: &Connection, thread: u64, prio: u32) -> Result<(), ::dbus::Error> {
    let mut m = Message::new_method_call("org.freedesktop.RealtimeKit1", "/org/freedesktop/RealtimeKit1",
        "org.freedesktop.RealtimeKit1", "MakeThreadRealtime").unwrap();
    m.append_items(&[thread.into(), prio.into()]);
    let mut r = try!(c.send_with_reply_and_block(m, 10000));
    r.as_result().map(|_| ())
}

fn make_realtime(prio: u32) -> Result<u32, Box<std::error::Error>> {
    let c = try!(Connection::get_private(BusType::System));

    let p = Props::new(&c, "org.freedesktop.RealtimeKit1", "/org/freedesktop/RealtimeKit1",
        "org.freedesktop.RealtimeKit1", 10000);

    // Make sure we don't fail by wanting too much
    let max_prio = try!(item_as_i64(try!(p.get("MaxRealtimePriority")))) as u32;
    let prio = cmp::min(prio, max_prio);

    // Enforce RLIMIT_RTPRIO, also a must before asking rtkit for rtprio
    let max_rttime = try!(item_as_i64(try!(p.get("RTTimeNSecMax")))) as u64;
    let new_limit = rlimit { rlim_cur: max_rttime, rlim_max: max_rttime };
    let mut old_limit = new_limit;
    if unsafe { getrlimit(RLIMIT_RTTIME, &mut old_limit) } < 0 {
        return Err(Box::from("getrlimit failed"));
    }
    if unsafe { setrlimit(RLIMIT_RTTIME, &new_limit) } < 0 {
        return Err(Box::from("setrlimit failed"));
    }

    // Finally, let's ask rtkit to make us realtime
    let thread_id = unsafe { syscall(SYS_gettid) };
    let r = rtkit_set_realtime(&c, thread_id as u64, prio);

    if r.is_err() {
        unsafe { setrlimit(RLIMIT_RTTIME, &old_limit) };
    }

    try!(r);
    Ok(prio)
}


fn main() {
    match make_realtime(5) {
        Ok(n) => println!("Got rtprio, level {}", n),
        Err(e) => println!("No rtprio: {}", e),
    }
}
