extern crate deque;
extern crate rand;

use std::boxed::Box;
use std::mem;
use std::thread::{self, JoinHandle};
use std::sync::atomic::{AtomicBool, ATOMIC_BOOL_INIT, AtomicUsize, ATOMIC_USIZE_INIT};
use std::sync::atomic::Ordering::SeqCst;

use deque::{Data, Abort, Empty, Worker, Stealer};
use rand::Rng;

#[test]
fn smoke() {
    let (w, s) = deque::new::<isize>();
    assert_eq!(w.pop(), None);
    assert_eq!(s.steal(), Empty);
    w.push(1);
    assert_eq!(w.pop(), Some(1));
    w.push(1);
    assert_eq!(s.steal(), Data(1));
    w.push(1);
    assert_eq!(s.clone().steal(), Data(1));
}

#[test]
fn stealpush() {
    static AMT: isize = 100000;
    let (w, s) = deque::new::<isize>();
    let t = thread::spawn(move || {
        let mut left = AMT;
        while left > 0 {
            match s.steal() {
                Data(i) => {
                    assert_eq!(i, 1);
                    left -= 1;
                }
                Abort | Empty => {}
            }
        }
    });

    for _ in 0..AMT {
        w.push(1);
    }

    t.join().unwrap();
}

#[test]
fn stealpush_large() {
    static AMT: isize = 100000;
    let (w, s) = deque::new::<(isize, isize)>();
    let t = thread::spawn(move || {
        let mut left = AMT;
        while left > 0 {
            match s.steal() {
                Data((1, 10)) => { left -= 1; }
                Data(..) => panic!(),
                Abort | Empty => {}
            }
        }
    });

    for _ in 0..AMT {
        w.push((1, 10));
    }

    t.join().unwrap();
}

#[derive(Clone, Copy)]
struct UnsafeAtomicUsize(*mut AtomicUsize);

unsafe impl Send for UnsafeAtomicUsize { }

fn stampede(w: Worker<Box<isize>>, s: Stealer<Box<isize>>,
            nthreads: isize, amt: usize) {
    for _ in 0..amt {
        w.push(Box::new(20));
    }
    let mut remaining = AtomicUsize::new(amt);
    let unsafe_remaining = UnsafeAtomicUsize(&mut remaining);

    let threads = (0..nthreads).map(|_| {
        let s = s.clone();
        thread::spawn(move || {
            unsafe {
                let UnsafeAtomicUsize(unsafe_remaining) = unsafe_remaining;
                while (*unsafe_remaining).load(SeqCst) > 0 {
                    match s.steal() {
                        Data(ref i) if **i == 20 => {
                            (*unsafe_remaining).fetch_sub(1, SeqCst);
                        }
                        Data(..) => panic!(),
                        Abort | Empty => {}
                    }
                }
            }
        })
    }).collect::<Vec<JoinHandle<()>>>();

    while remaining.load(SeqCst) > 0 {
        match w.pop() {
            Some(ref i) if **i == 20 => { remaining.fetch_sub(1, SeqCst); }
            Some(..) => panic!(),
            None => {}
        }
    }

    for thread in threads.into_iter() {
        thread.join().unwrap();
    }
}

#[test]
fn run_stampede() {
    let (w, s) = deque::new::<Box<isize>>();
    stampede(w, s, 8, 10000);
}

#[test]
fn many_stampede() {
    static AMT: usize = 4;
    let threads = (0..AMT).map(|_| {
        let (w, s) = deque::new::<Box<isize>>();
        thread::spawn(move || {
            stampede(w, s, 4, 10000);
        })
    }).collect::<Vec<JoinHandle<()>>>();

    for thread in threads.into_iter() {
        thread.join().unwrap();
    }
}

#[test]
fn stress() {
    static AMT: isize = 100000;
    static NTHREADS: isize = 8;
    static DONE: AtomicBool = ATOMIC_BOOL_INIT;
    static HITS: AtomicUsize = ATOMIC_USIZE_INIT;
    let (w, s) = deque::new::<isize>();

    let threads = (0..NTHREADS).map(|_| {
        let s = s.clone();
        thread::spawn(move || {
            loop {
                match s.steal() {
                    Data(2) => { HITS.fetch_add(1, SeqCst); }
                    Data(..) => panic!(),
                    _ if DONE.load(SeqCst) => break,
                    _ => {}
                }
            }
        })
    }).collect::<Vec<JoinHandle<()>>>();

    let mut rng = rand::thread_rng();
    let mut expected = 0;
    while expected < AMT {
        if rng.gen_range(0, 3) == 2 {
            match w.pop() {
                None => {}
                Some(2) => { HITS.fetch_add(1, SeqCst); },
                Some(_) => panic!(),
            }
        } else {
            expected += 1;
            w.push(2);
        }
    }

    while HITS.load(SeqCst) < AMT as usize {
        match w.pop() {
            None => {}
            Some(2) => { HITS.fetch_add(1, SeqCst); },
            Some(_) => panic!(),
        }
    }
    DONE.store(true, SeqCst);

    for thread in threads.into_iter() {
        thread.join().unwrap();
    }

    assert_eq!(HITS.load(SeqCst), expected as usize);
}

#[test]
#[cfg_attr(windows, ignore)] // apparently windows scheduling is weird?
fn no_starvation() {
    static AMT: isize = 10000;
    static NTHREADS: isize = 4;
    static DONE: AtomicBool = ATOMIC_BOOL_INIT;
    let (w, s) = deque::new::<(isize, usize)>();

    let (threads, hits): (Vec<_>, Vec<_>) = (0..NTHREADS).map(|_| {
        let s = s.clone();
        let unique_box = Box::new(AtomicUsize::new(0));
        let thread_box = UnsafeAtomicUsize(unsafe {
            *mem::transmute::<&Box<AtomicUsize>,
                              *const *mut AtomicUsize>(&unique_box)
        });
        (thread::spawn(move || {
            unsafe {
                let UnsafeAtomicUsize(thread_box) = thread_box;
                loop {
                    match s.steal() {
                        Data((1, 2)) => {
                            (*thread_box).fetch_add(1, SeqCst);
                        }
                        Data(..) => panic!(),
                        _ if DONE.load(SeqCst) => break,
                        _ => {}
                    }
                }
            }
        }), unique_box)
    }).unzip();

    let mut rng = rand::thread_rng();
    let mut myhit = false;
    'outer: loop {
        for _ in 0..rng.gen_range(0, AMT) {
            if !myhit && rng.gen_range(0, 3) == 2 {
                match w.pop() {
                    None => {}
                    Some((1, 2)) => myhit = true,
                    Some(_) => panic!(),
                }
            } else {
                w.push((1, 2));
            }
        }

        for slot in hits.iter() {
            let amt = slot.load(SeqCst);
            if amt == 0 { continue 'outer; }
        }
        if myhit {
            break
        }
    }

    DONE.store(true, SeqCst);

    for thread in threads.into_iter() {
        thread.join().unwrap();
    }
}

struct Unclonable;

#[test]
fn clone_stealer_of_unclonable_type() {
    let (_, s) = deque::new::<Unclonable>();
    let _ = s.clone();
}
