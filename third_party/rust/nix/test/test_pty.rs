use std::io::Write;
use std::path::Path;
use std::os::unix::prelude::*;
use tempfile::tempfile;

use libc::{_exit, STDOUT_FILENO};
use nix::fcntl::{OFlag, open};
use nix::pty::*;
use nix::sys::stat;
use nix::sys::termios::*;
use nix::unistd::{write, close, pause};

/// Regression test for Issue #659
/// This is the correct way to explicitly close a `PtyMaster`
#[test]
fn test_explicit_close() {
    let mut f = {
        let m = posix_openpt(OFlag::O_RDWR).unwrap();
        close(m.into_raw_fd()).unwrap();
        tempfile().unwrap()
    };
    // This should work.  But if there's been a double close, then it will
    // return EBADF
    f.write_all(b"whatever").unwrap();
}

/// Test equivalence of `ptsname` and `ptsname_r`
#[test]
#[cfg(any(target_os = "android", target_os = "linux"))]
fn test_ptsname_equivalence() {
    let _m = ::PTSNAME_MTX.lock().expect("Mutex got poisoned by another test");

    // Open a new PTTY master
    let master_fd = posix_openpt(OFlag::O_RDWR).unwrap();
    assert!(master_fd.as_raw_fd() > 0);

    // Get the name of the slave
    let slave_name = unsafe { ptsname(&master_fd) }.unwrap() ;
    let slave_name_r = ptsname_r(&master_fd).unwrap();
    assert_eq!(slave_name, slave_name_r);
}

/// Test data copying of `ptsname`
// TODO need to run in a subprocess, since ptsname is non-reentrant
#[test]
#[cfg(any(target_os = "android", target_os = "linux"))]
fn test_ptsname_copy() {
    let _m = ::PTSNAME_MTX.lock().expect("Mutex got poisoned by another test");

    // Open a new PTTY master
    let master_fd = posix_openpt(OFlag::O_RDWR).unwrap();
    assert!(master_fd.as_raw_fd() > 0);

    // Get the name of the slave
    let slave_name1 = unsafe { ptsname(&master_fd) }.unwrap();
    let slave_name2 = unsafe { ptsname(&master_fd) }.unwrap();
    assert!(slave_name1 == slave_name2);
    // Also make sure that the string was actually copied and they point to different parts of
    // memory.
    assert!(slave_name1.as_ptr() != slave_name2.as_ptr());
}

/// Test data copying of `ptsname_r`
#[test]
#[cfg(any(target_os = "android", target_os = "linux"))]
fn test_ptsname_r_copy() {
    // Open a new PTTY master
    let master_fd = posix_openpt(OFlag::O_RDWR).unwrap();
    assert!(master_fd.as_raw_fd() > 0);

    // Get the name of the slave
    let slave_name1 = ptsname_r(&master_fd).unwrap();
    let slave_name2 = ptsname_r(&master_fd).unwrap();
    assert!(slave_name1 == slave_name2);
    assert!(slave_name1.as_ptr() != slave_name2.as_ptr());
}

/// Test that `ptsname` returns different names for different devices
#[test]
#[cfg(any(target_os = "android", target_os = "linux"))]
fn test_ptsname_unique() {
    let _m = ::PTSNAME_MTX.lock().expect("Mutex got poisoned by another test");

    // Open a new PTTY master
    let master1_fd = posix_openpt(OFlag::O_RDWR).unwrap();
    assert!(master1_fd.as_raw_fd() > 0);

    // Open a second PTTY master
    let master2_fd = posix_openpt(OFlag::O_RDWR).unwrap();
    assert!(master2_fd.as_raw_fd() > 0);

    // Get the name of the slave
    let slave_name1 = unsafe { ptsname(&master1_fd) }.unwrap();
    let slave_name2 = unsafe { ptsname(&master2_fd) }.unwrap();
    assert!(slave_name1 != slave_name2);
}

/// Test opening a master/slave PTTY pair
///
/// This is a single larger test because much of these functions aren't useful by themselves. So for
/// this test we perform the basic act of getting a file handle for a connect master/slave PTTY
/// pair.
#[test]
fn test_open_ptty_pair() {
    let _m = ::PTSNAME_MTX.lock().expect("Mutex got poisoned by another test");

    // Open a new PTTY master
    let master_fd = posix_openpt(OFlag::O_RDWR).expect("posix_openpt failed");
    assert!(master_fd.as_raw_fd() > 0);

    // Allow a slave to be generated for it
    grantpt(&master_fd).expect("grantpt failed");
    unlockpt(&master_fd).expect("unlockpt failed");

    // Get the name of the slave
    let slave_name = unsafe { ptsname(&master_fd) }.expect("ptsname failed");

    // Open the slave device
    let slave_fd = open(Path::new(&slave_name), OFlag::O_RDWR, stat::Mode::empty()).unwrap();
    assert!(slave_fd > 0);
}

#[test]
fn test_openpty() {
    // openpty uses ptname(3) internally
    let _m = ::PTSNAME_MTX.lock().expect("Mutex got poisoned by another test");

    let pty = openpty(None, None).unwrap();
    assert!(pty.master > 0);
    assert!(pty.slave > 0);

    // Writing to one should be readable on the other one
    let string = "foofoofoo\n";
    let mut buf = [0u8; 10];
    write(pty.master, string.as_bytes()).unwrap();
    ::read_exact(pty.slave, &mut buf);

    assert_eq!(&buf, string.as_bytes());

    // Read the echo as well
    let echoed_string = "foofoofoo\r\n";
    let mut buf = [0u8; 11];
    ::read_exact(pty.master, &mut buf);
    assert_eq!(&buf, echoed_string.as_bytes());

    let string2 = "barbarbarbar\n";
    let echoed_string2 = "barbarbarbar\r\n";
    let mut buf = [0u8; 14];
    write(pty.slave, string2.as_bytes()).unwrap();
    ::read_exact(pty.master, &mut buf);

    assert_eq!(&buf, echoed_string2.as_bytes());

    close(pty.master).unwrap();
    close(pty.slave).unwrap();
}

#[test]
fn test_openpty_with_termios() {
    // openpty uses ptname(3) internally
    let _m = ::PTSNAME_MTX.lock().expect("Mutex got poisoned by another test");

    // Open one pty to get attributes for the second one
    let mut termios = {
        let pty = openpty(None, None).unwrap();
        assert!(pty.master > 0);
        assert!(pty.slave > 0);
        let termios = tcgetattr(pty.master).unwrap();
        close(pty.master).unwrap();
        close(pty.slave).unwrap();
        termios
    };
    // Make sure newlines are not transformed so the data is preserved when sent.
    termios.output_flags.remove(OutputFlags::ONLCR);

    let pty = openpty(None, &termios).unwrap();
    // Must be valid file descriptors
    assert!(pty.master > 0);
    assert!(pty.slave > 0);

    // Writing to one should be readable on the other one
    let string = "foofoofoo\n";
    let mut buf = [0u8; 10];
    write(pty.master, string.as_bytes()).unwrap();
    ::read_exact(pty.slave, &mut buf);

    assert_eq!(&buf, string.as_bytes());

    // read the echo as well
    let echoed_string = "foofoofoo\n";
    ::read_exact(pty.master, &mut buf);
    assert_eq!(&buf, echoed_string.as_bytes());

    let string2 = "barbarbarbar\n";
    let echoed_string2 = "barbarbarbar\n";
    let mut buf = [0u8; 13];
    write(pty.slave, string2.as_bytes()).unwrap();
    ::read_exact(pty.master, &mut buf);

    assert_eq!(&buf, echoed_string2.as_bytes());

    close(pty.master).unwrap();
    close(pty.slave).unwrap();
}

#[test]
fn test_forkpty() {
    use nix::unistd::ForkResult::*;
    use nix::sys::signal::*;
    use nix::sys::wait::wait;
    // forkpty calls openpty which uses ptname(3) internally.
    let _m0 = ::PTSNAME_MTX.lock().expect("Mutex got poisoned by another test");
    // forkpty spawns a child process
    let _m1 = ::FORK_MTX.lock().expect("Mutex got poisoned by another test");

    let string = "naninani\n";
    let echoed_string = "naninani\r\n";
    let pty = forkpty(None, None).unwrap();
    match pty.fork_result {
        Child => {
            write(STDOUT_FILENO, string.as_bytes()).unwrap();
            pause();  // we need the child to stay alive until the parent calls read
            unsafe { _exit(0); }
        },
        Parent { child } => {
            let mut buf = [0u8; 10];
            assert!(child.as_raw() > 0);
            ::read_exact(pty.master, &mut buf);
            kill(child, SIGTERM).unwrap();
            wait().unwrap(); // keep other tests using generic wait from getting our child
            assert_eq!(&buf, echoed_string.as_bytes());
            close(pty.master).unwrap();
        },
    }
}
