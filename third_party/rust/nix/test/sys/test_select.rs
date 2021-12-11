use nix::sys::select::*;
use nix::unistd::{pipe, write};
use nix::sys::signal::SigSet;
use nix::sys::time::{TimeSpec, TimeValLike};

#[test]
pub fn test_pselect() {
    let _mtx = ::SIGNAL_MTX
        .lock()
        .expect("Mutex got poisoned by another test");

    let (r1, w1) = pipe().unwrap();
    write(w1, b"hi!").unwrap();
    let (r2, _w2) = pipe().unwrap();

    let mut fd_set = FdSet::new();
    fd_set.insert(r1);
    fd_set.insert(r2);

    let timeout = TimeSpec::seconds(10);
    let sigmask = SigSet::empty();
    assert_eq!(
        1,
        pselect(None, &mut fd_set, None, None, &timeout, &sigmask).unwrap()
    );
    assert!(fd_set.contains(r1));
    assert!(!fd_set.contains(r2));
}

#[test]
pub fn test_pselect_nfds2() {
    let (r1, w1) = pipe().unwrap();
    write(w1, b"hi!").unwrap();
    let (r2, _w2) = pipe().unwrap();

    let mut fd_set = FdSet::new();
    fd_set.insert(r1);
    fd_set.insert(r2);

    let timeout = TimeSpec::seconds(10);
    assert_eq!(
        1,
        pselect(
            ::std::cmp::max(r1, r2) + 1,
            &mut fd_set,
            None,
            None,
            &timeout,
            None
        ).unwrap()
    );
    assert!(fd_set.contains(r1));
    assert!(!fd_set.contains(r2));
}
