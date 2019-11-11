use nix::poll::{EventFlags, poll, PollFd};
use nix::unistd::{write, pipe, close};

#[test]
fn test_poll() {
    let (r, w) = pipe().unwrap();
    let mut fds = [PollFd::new(r, EventFlags::POLLIN)];

    // Poll an idle pipe.  Should timeout
    let nfds = poll(&mut fds, 100).unwrap();
    assert_eq!(nfds, 0);
    assert!(!fds[0].revents().unwrap().contains(EventFlags::POLLIN));

    write(w, b".").unwrap();

    // Poll a readable pipe.  Should return an event.
    let nfds = poll(&mut fds, 100).unwrap();
    assert_eq!(nfds, 1);
    assert!(fds[0].revents().unwrap().contains(EventFlags::POLLIN));
}

#[test]
fn test_poll_debug() {
    assert_eq!(format!("{:?}", PollFd::new(0, EventFlags::empty())),
               "PollFd { fd: 0, events: (empty), revents: (empty) }");
    assert_eq!(format!("{:?}", PollFd::new(1, EventFlags::POLLIN)),
               "PollFd { fd: 1, events: POLLIN, revents: (empty) }");

    // Testing revents requires doing some I/O
    let (r, w) = pipe().unwrap();
    let mut fds = [PollFd::new(r, EventFlags::POLLIN)];
    write(w, b" ").unwrap();
    close(w).unwrap();
    poll(&mut fds, -1).unwrap();
    assert_eq!(format!("{:?}", fds[0]),
               format!("PollFd {{ fd: {}, events: POLLIN, revents: POLLIN | POLLHUP }}", r));
    close(r).unwrap();
}

// ppoll(2) is the same as poll except for how it handles timeouts and signals.
// Repeating the test for poll(2) should be sufficient to check that our
// bindings are correct.
#[cfg(any(target_os = "android",
          target_os = "dragonfly",
          target_os = "freebsd",
          target_os = "linux"))]
#[test]
fn test_ppoll() {
    use nix::poll::ppoll;
    use nix::sys::signal::SigSet;
    use nix::sys::time::{TimeSpec, TimeValLike};

    let timeout = TimeSpec::milliseconds(1);
    let (r, w) = pipe().unwrap();
    let mut fds = [PollFd::new(r, EventFlags::POLLIN)];

    // Poll an idle pipe.  Should timeout
    let nfds = ppoll(&mut fds, timeout, SigSet::empty()).unwrap();
    assert_eq!(nfds, 0);
    assert!(!fds[0].revents().unwrap().contains(EventFlags::POLLIN));

    write(w, b".").unwrap();

    // Poll a readable pipe.  Should return an event.
    let nfds = ppoll(&mut fds, timeout, SigSet::empty()).unwrap();
    assert_eq!(nfds, 1);
    assert!(fds[0].revents().unwrap().contains(EventFlags::POLLIN));
}
