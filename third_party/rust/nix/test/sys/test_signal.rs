use libc;
use nix::Error;
use nix::sys::signal::*;
use nix::unistd::*;
use std::sync::atomic::{AtomicBool, Ordering};

#[test]
fn test_kill_none() {
    kill(getpid(), None).expect("Should be able to send signal to myself.");
}

#[test]
fn test_killpg_none() {
    killpg(getpgrp(), None)
        .expect("Should be able to send signal to my process group.");
}

#[test]
fn test_old_sigaction_flags() {
    extern "C" fn handler(_: ::libc::c_int) {}
    let act = SigAction::new(
        SigHandler::Handler(handler),
        SaFlags::empty(),
        SigSet::empty(),
    );
    let oact = unsafe { sigaction(SIGINT, &act) }.unwrap();
    let _flags = oact.flags();
    let oact = unsafe { sigaction(SIGINT, &act) }.unwrap();
    let _flags = oact.flags();
}

#[test]
fn test_sigprocmask_noop() {
    sigprocmask(SigmaskHow::SIG_BLOCK, None, None)
        .expect("this should be an effective noop");
}

#[test]
fn test_sigprocmask() {
    let _m = ::SIGNAL_MTX.lock().expect("Mutex got poisoned by another test");

    // This needs to be a signal that rust doesn't use in the test harness.
    const SIGNAL: Signal = Signal::SIGCHLD;

    let mut old_signal_set = SigSet::empty();
    sigprocmask(SigmaskHow::SIG_BLOCK, None, Some(&mut old_signal_set))
        .expect("expect to be able to retrieve old signals");

    // Make sure the old set doesn't contain the signal, otherwise the following
    // test don't make sense.
    assert_eq!(old_signal_set.contains(SIGNAL), false,
               "the {:?} signal is already blocked, please change to a \
                different one", SIGNAL);

    // Now block the signal.
    let mut signal_set = SigSet::empty();
    signal_set.add(SIGNAL);
    sigprocmask(SigmaskHow::SIG_BLOCK, Some(&signal_set), None)
        .expect("expect to be able to block signals");

    // And test it again, to make sure the change was effective.
    old_signal_set.clear();
    sigprocmask(SigmaskHow::SIG_BLOCK, None, Some(&mut old_signal_set))
        .expect("expect to be able to retrieve old signals");
    assert_eq!(old_signal_set.contains(SIGNAL), true,
               "expected the {:?} to be blocked", SIGNAL);

    // Reset the signal.
    sigprocmask(SigmaskHow::SIG_UNBLOCK, Some(&signal_set), None)
        .expect("expect to be able to block signals");
}

lazy_static! {
    static ref SIGNALED: AtomicBool = AtomicBool::new(false);
}

extern fn test_sigaction_handler(signal: libc::c_int) {
    let signal = Signal::from_c_int(signal).unwrap();
    SIGNALED.store(signal == Signal::SIGINT, Ordering::Relaxed);
}

extern fn test_sigaction_action(_: libc::c_int, _: *mut libc::siginfo_t, _: *mut libc::c_void) {
}

#[test]
fn test_signal() {
    let _m = ::SIGNAL_MTX.lock().expect("Mutex got poisoned by another test");

    unsafe { signal(Signal::SIGINT, SigHandler::SigIgn) }.unwrap();
    raise(Signal::SIGINT).unwrap();
    assert_eq!(unsafe { signal(Signal::SIGINT, SigHandler::SigDfl) }.unwrap(), SigHandler::SigIgn);

    let handler = SigHandler::Handler(test_sigaction_handler);
    assert_eq!(unsafe { signal(Signal::SIGINT, handler) }.unwrap(), SigHandler::SigDfl);
    raise(Signal::SIGINT).unwrap();
    assert!(SIGNALED.load(Ordering::Relaxed));
    assert_eq!(unsafe { signal(Signal::SIGINT, SigHandler::SigDfl) }.unwrap(), handler);

    let action_handler = SigHandler::SigAction(test_sigaction_action);
    assert_eq!(unsafe { signal(Signal::SIGINT, action_handler) }.unwrap_err(), Error::UnsupportedOperation);

    // Restore default signal handler
    unsafe { signal(Signal::SIGINT, SigHandler::SigDfl) }.unwrap();
}
