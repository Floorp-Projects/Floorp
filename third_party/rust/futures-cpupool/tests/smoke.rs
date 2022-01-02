extern crate futures;
extern crate futures_cpupool;

use std::sync::atomic::{AtomicUsize, Ordering, ATOMIC_USIZE_INIT};
use std::thread;
use std::time::Duration;

use futures::future::Future;
use futures_cpupool::{CpuPool, Builder};

fn done<T: Send + 'static>(t: T) -> Box<Future<Item = T, Error = ()> + Send> {
    Box::new(futures::future::ok(t))
}

#[test]
fn join() {
    let pool = CpuPool::new(2);
    let a = pool.spawn(done(1));
    let b = pool.spawn(done(2));
    let res = a.join(b).map(|(a, b)| a + b).wait();

    assert_eq!(res.unwrap(), 3);
}

#[test]
fn select() {
    let pool = CpuPool::new(2);
    let a = pool.spawn(done(1));
    let b = pool.spawn(done(2));
    let (item1, next) = a.select(b).wait().ok().unwrap();
    let item2 = next.wait().unwrap();

    assert!(item1 != item2);
    assert!((item1 == 1 && item2 == 2) || (item1 == 2 && item2 == 1));
}

#[test]
fn threads_go_away() {
    static CNT: AtomicUsize = ATOMIC_USIZE_INIT;

    struct A;

    impl Drop for A {
        fn drop(&mut self) {
            CNT.fetch_add(1, Ordering::SeqCst);
        }
    }

    thread_local!(static FOO: A = A);

    let pool = CpuPool::new(2);
    let _handle = pool.spawn_fn(|| {
        FOO.with(|_| ());
        Ok::<(), ()>(())
    });
    drop(pool);

    for _ in 0..100 {
        if CNT.load(Ordering::SeqCst) == 1 {
            return
        }
        thread::sleep(Duration::from_millis(10));
    }
    panic!("thread didn't exit");
}

#[test]
fn lifecycle_test() {
    static NUM_STARTS: AtomicUsize = ATOMIC_USIZE_INIT;
    static NUM_STOPS: AtomicUsize = ATOMIC_USIZE_INIT;

    fn after_start() {
        NUM_STARTS.fetch_add(1, Ordering::SeqCst);
    }

    fn before_stop() {
        NUM_STOPS.fetch_add(1, Ordering::SeqCst);
    }

    let pool = Builder::new()
        .pool_size(4)
        .after_start(after_start)
        .before_stop(before_stop)
        .create();
    let _handle = pool.spawn_fn(|| {
        Ok::<(), ()>(())
    });
    drop(pool);

    for _ in 0..100 {
        if NUM_STOPS.load(Ordering::SeqCst) == 4 {
            assert_eq!(NUM_STARTS.load(Ordering::SeqCst), 4);
            return;
        }
        thread::sleep(Duration::from_millis(10));
    }
    panic!("thread didn't exit");
}

#[test]
fn thread_name() {
    let pool = Builder::new()
        .name_prefix("my-pool-")
        .create();
    let future = pool.spawn_fn(|| {
        assert!(thread::current().name().unwrap().starts_with("my-pool-"));
        Ok::<(), ()>(())
    });
    let _ = future.wait();
}
