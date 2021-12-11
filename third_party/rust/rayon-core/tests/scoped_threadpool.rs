use crossbeam_utils::thread;
use rayon_core::ThreadPoolBuilder;

#[derive(PartialEq, Eq, Debug)]
struct Local(i32);

scoped_tls::scoped_thread_local!(static LOCAL: Local);

#[test]
fn missing_scoped_tls() {
    LOCAL.set(&Local(42), || {
        let pool = ThreadPoolBuilder::new()
            .build()
            .expect("thread pool created");

        // `LOCAL` is not set in the pool.
        pool.install(|| {
            assert!(!LOCAL.is_set());
        });
    });
}

#[test]
fn spawn_scoped_tls_threadpool() {
    LOCAL.set(&Local(42), || {
        LOCAL.with(|x| {
            thread::scope(|scope| {
                let pool = ThreadPoolBuilder::new()
                    .spawn_handler(move |thread| {
                        scope
                            .builder()
                            .spawn(move |_| {
                                // Borrow the same local value in the thread pool.
                                LOCAL.set(x, || thread.run())
                            })
                            .map(|_| ())
                    })
                    .build()
                    .expect("thread pool created");

                // The pool matches our local value.
                pool.install(|| {
                    assert!(LOCAL.is_set());
                    LOCAL.with(|y| {
                        assert_eq!(x, y);
                    });
                });

                // If we change our local value, the pool is not affected.
                LOCAL.set(&Local(-1), || {
                    pool.install(|| {
                        assert!(LOCAL.is_set());
                        LOCAL.with(|y| {
                            assert_eq!(x, y);
                        });
                    });
                });
            })
            .expect("scope threads ok");
            // `thread::scope` will wait for the threads to exit before returning.
        });
    });
}

#[test]
fn build_scoped_tls_threadpool() {
    LOCAL.set(&Local(42), || {
        LOCAL.with(|x| {
            ThreadPoolBuilder::new()
                .build_scoped(
                    move |thread| LOCAL.set(x, || thread.run()),
                    |pool| {
                        // The pool matches our local value.
                        pool.install(|| {
                            assert!(LOCAL.is_set());
                            LOCAL.with(|y| {
                                assert_eq!(x, y);
                            });
                        });

                        // If we change our local value, the pool is not affected.
                        LOCAL.set(&Local(-1), || {
                            pool.install(|| {
                                assert!(LOCAL.is_set());
                                LOCAL.with(|y| {
                                    assert_eq!(x, y);
                                });
                            });
                        });
                    },
                )
                .expect("thread pool created");
            // Internally, `crossbeam::scope` will wait for the threads to exit before returning.
        });
    });
}
