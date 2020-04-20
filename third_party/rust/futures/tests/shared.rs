use futures::channel::oneshot;
use futures::executor::{block_on, LocalPool};
use futures::future::{self, FutureExt, TryFutureExt, LocalFutureObj};
use futures::task::LocalSpawn;
use std::cell::{Cell, RefCell};
use std::rc::Rc;
use std::thread;

fn send_shared_oneshot_and_wait_on_multiple_threads(threads_number: u32) {
    let (tx, rx) = oneshot::channel::<i32>();
    let f = rx.shared();
    let join_handles = (0..threads_number)
        .map(|_| {
            let cloned_future = f.clone();
            thread::spawn(move || {
                assert_eq!(block_on(cloned_future).unwrap(), 6);
            })
        })
        .collect::<Vec<_>>();

    tx.send(6).unwrap();

    assert_eq!(block_on(f).unwrap(), 6);
    for join_handle in join_handles {
        join_handle.join().unwrap();
    }
}

#[test]
fn one_thread() {
    send_shared_oneshot_and_wait_on_multiple_threads(1);
}

#[test]
fn two_threads() {
    send_shared_oneshot_and_wait_on_multiple_threads(2);
}

#[test]
fn many_threads() {
    send_shared_oneshot_and_wait_on_multiple_threads(1000);
}

#[test]
fn drop_on_one_task_ok() {
    let (tx, rx) = oneshot::channel::<u32>();
    let f1 = rx.shared();
    let f2 = f1.clone();

    let (tx2, rx2) = oneshot::channel::<u32>();

    let t1 = thread::spawn(|| {
        let f = future::try_select(f1.map_err(|_| ()), rx2.map_err(|_| ()));
        drop(block_on(f));
    });

    let (tx3, rx3) = oneshot::channel::<u32>();

    let t2 = thread::spawn(|| {
        let _ = block_on(f2.map_ok(|x| tx3.send(x).unwrap()).map_err(|_| ()));
    });

    tx2.send(11).unwrap(); // cancel `f1`
    t1.join().unwrap();

    tx.send(42).unwrap(); // Should cause `f2` and then `rx3` to get resolved.
    let result = block_on(rx3).unwrap();
    assert_eq!(result, 42);
    t2.join().unwrap();
}

#[test]
fn drop_in_poll() {
    let slot1 = Rc::new(RefCell::new(None));
    let slot2 = slot1.clone();

    let future1 = future::lazy(move |_| {
        slot2.replace(None); // Drop future
        1
    }).shared();

    let future2 = LocalFutureObj::new(Box::new(future1.clone()));
    slot1.replace(Some(future2));

    assert_eq!(block_on(future1), 1);
}

#[test]
fn peek() {
    let mut local_pool = LocalPool::new();
    let spawn = &mut local_pool.spawner();

    let (tx0, rx0) = oneshot::channel::<i32>();
    let f1 = rx0.shared();
    let f2 = f1.clone();

    // Repeated calls on the original or clone do not change the outcome.
    for _ in 0..2 {
        assert!(f1.peek().is_none());
        assert!(f2.peek().is_none());
    }

    // Completing the underlying future has no effect, because the value has not been `poll`ed in.
    tx0.send(42).unwrap();
    for _ in 0..2 {
        assert!(f1.peek().is_none());
        assert!(f2.peek().is_none());
    }

    // Once the Shared has been polled, the value is peekable on the clone.
    spawn.spawn_local_obj(LocalFutureObj::new(Box::new(f1.map(|_| ())))).unwrap();
    local_pool.run();
    for _ in 0..2 {
        assert_eq!(*f2.peek().unwrap(), Ok(42));
    }
}

struct CountClone(Rc<Cell<i32>>);

impl Clone for CountClone {
    fn clone(&self) -> Self {
        self.0.set(self.0.get() + 1);
        CountClone(self.0.clone())
    }
}

#[test]
fn dont_clone_in_single_owner_shared_future() {
    let counter = CountClone(Rc::new(Cell::new(0)));
    let (tx, rx) = oneshot::channel();

    let rx = rx.shared();

    tx.send(counter).ok().unwrap();

    assert_eq!(block_on(rx).unwrap().0.get(), 0);
}

#[test]
fn dont_do_unnecessary_clones_on_output() {
    let counter = CountClone(Rc::new(Cell::new(0)));
    let (tx, rx) = oneshot::channel();

    let rx = rx.shared();

    tx.send(counter).ok().unwrap();

    assert_eq!(block_on(rx.clone()).unwrap().0.get(), 1);
    assert_eq!(block_on(rx.clone()).unwrap().0.get(), 2);
    assert_eq!(block_on(rx).unwrap().0.get(), 2);
}
