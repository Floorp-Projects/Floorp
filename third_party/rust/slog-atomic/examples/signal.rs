#[macro_use]
extern crate slog;
extern crate slog_term;
extern crate slog_atomic;
extern crate slog_json;
extern crate slog_stream;
extern crate nix;

#[macro_use]
extern crate lazy_static;

use nix::sys::signal;
use std::sync::atomic::AtomicBool;
use std::sync::atomic::Ordering::SeqCst;
use std::time::Duration;

use std::{thread, io};
use slog::*;
use slog_atomic::*;
use slog_stream::*;

lazy_static! {
    // global atomic switch drain control
    static ref ATOMIC_DRAIN_SWITCH : AtomicSwitchCtrl<io::Error> = AtomicSwitch::new(
        Discard.map_err(|_| io::Error::new(io::ErrorKind::Other, "should no happen"))
    ).ctrl();

    // track current state of the atomic switch drain
    static ref ATOMIC_DRAIN_SWITCH_STATE : AtomicBool = AtomicBool::new(false);
}

fn atomic_drain_switch() {
    // XXX: Not atomic. Race?
    let new = !ATOMIC_DRAIN_SWITCH_STATE.load(SeqCst);
    ATOMIC_DRAIN_SWITCH_STATE.store(new, SeqCst);

    if new {
        ATOMIC_DRAIN_SWITCH.set(stream(io::stdout(), slog_json::default()))
    } else {
        ATOMIC_DRAIN_SWITCH.set(slog_term::streamer().full().stdout().build())
    }
}

extern "C" fn handle_sigusr1(_: i32) {
    atomic_drain_switch();
}

fn main() {
    unsafe {
        let sig_action = signal::SigAction::new(signal::SigHandler::Handler(handle_sigusr1),
                                                signal::SaFlags::empty(),
                                                signal::SigSet::empty());
        signal::sigaction(signal::SIGUSR1, &sig_action).unwrap();
    }

    let drain = slog::duplicate(slog_term::streamer().stderr().full().build(), ATOMIC_DRAIN_SWITCH.drain()).fuse();

    atomic_drain_switch();

    let log = Logger::root(drain, o!());

    info!(log, "logging a message every 3s");
    info!(log, "send SIGUSR1 signal to switch output with");
    let pid = nix::unistd::getpid();
    info!(log, "kill -SIGUSR1 {}", pid);
    loop {
        info!(log, "tick");
        thread::sleep(Duration::from_millis(3000));
    }
}
