/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::sync::{Arc, Condvar, Mutex};

pub struct StateCallback<T> {
    callback: Arc<Mutex<Option<Box<dyn FnOnce(T) + Send>>>>,
    observer: Arc<Mutex<Option<Box<dyn FnOnce() + Send>>>>,
    condition: Arc<(Mutex<bool>, Condvar)>,
}

impl<T> StateCallback<T> {
    // This is used for the Condvar, which requires this kind of construction
    #[allow(clippy::mutex_atomic)]
    pub fn new(cb: Box<dyn FnOnce(T) + Send>) -> Self {
        Self {
            callback: Arc::new(Mutex::new(Some(cb))),
            observer: Arc::new(Mutex::new(None)),
            condition: Arc::new((Mutex::new(true), Condvar::new())),
        }
    }

    pub fn add_uncloneable_observer(&mut self, obs: Box<dyn FnOnce() + Send>) {
        let mut opt = self.observer.lock().unwrap();
        if opt.is_some() {
            error!("Replacing an already-set observer.")
        }
        opt.replace(obs);
    }

    pub fn call(&self, rv: T) {
        if let Some(cb) = self.callback.lock().unwrap().take() {
            cb(rv);

            if let Some(obs) = self.observer.lock().unwrap().take() {
                obs();
            }
        }

        let (lock, cvar) = &*self.condition;
        let mut pending = lock.lock().unwrap();
        *pending = false;
        cvar.notify_all();
    }

    pub fn wait(&self) {
        let (lock, cvar) = &*self.condition;
        let _useless_guard = cvar
            .wait_while(lock.lock().unwrap(), |pending| *pending)
            .unwrap();
    }
}

impl<T> Clone for StateCallback<T> {
    fn clone(&self) -> Self {
        Self {
            callback: self.callback.clone(),
            observer: Arc::new(Mutex::new(None)),
            condition: self.condition.clone(),
        }
    }
}

#[cfg(test)]
mod tests {
    use super::StateCallback;
    use std::sync::atomic::{AtomicUsize, Ordering};
    use std::sync::{Arc, Barrier};
    use std::thread;

    #[test]
    fn test_statecallback_is_single_use() {
        let counter = Arc::new(AtomicUsize::new(0));
        let counter_clone = counter.clone();
        let sc = StateCallback::new(Box::new(move |_| {
            counter_clone.fetch_add(1, Ordering::SeqCst);
        }));

        assert_eq!(counter.load(Ordering::SeqCst), 0);
        for _ in 0..10 {
            sc.call(());
            assert_eq!(counter.load(Ordering::SeqCst), 1);
        }

        for _ in 0..10 {
            sc.clone().call(());
            assert_eq!(counter.load(Ordering::SeqCst), 1);
        }
    }

    #[test]
    fn test_statecallback_observer_is_single_use() {
        let counter = Arc::new(AtomicUsize::new(0));
        let counter_clone = counter.clone();
        let mut sc = StateCallback::<()>::new(Box::new(move |_| {}));

        sc.add_uncloneable_observer(Box::new(move || {
            counter_clone.fetch_add(1, Ordering::SeqCst);
        }));

        assert_eq!(counter.load(Ordering::SeqCst), 0);
        for _ in 0..10 {
            sc.call(());
            assert_eq!(counter.load(Ordering::SeqCst), 1);
        }

        for _ in 0..10 {
            sc.clone().call(());
            assert_eq!(counter.load(Ordering::SeqCst), 1);
        }
    }

    #[test]
    fn test_statecallback_observer_only_runs_for_completing_callback() {
        let cb_counter = Arc::new(AtomicUsize::new(0));
        let cb_counter_clone = cb_counter.clone();
        let sc = StateCallback::new(Box::new(move |_| {
            cb_counter_clone.fetch_add(1, Ordering::SeqCst);
        }));

        let obs_counter = Arc::new(AtomicUsize::new(0));

        for _ in 0..10 {
            let obs_counter_clone = obs_counter.clone();
            let mut c = sc.clone();
            c.add_uncloneable_observer(Box::new(move || {
                obs_counter_clone.fetch_add(1, Ordering::SeqCst);
            }));

            c.call(());

            assert_eq!(cb_counter.load(Ordering::SeqCst), 1);
            assert_eq!(obs_counter.load(Ordering::SeqCst), 1);
        }
    }

    #[test]
    #[allow(clippy::redundant_clone)]
    fn test_statecallback_observer_unclonable() {
        let mut sc = StateCallback::<()>::new(Box::new(move |_| {}));
        sc.add_uncloneable_observer(Box::new(move || {}));

        assert!(sc.observer.lock().unwrap().is_some());
        // This is deliberate, to force an extra clone
        assert!(sc.clone().observer.lock().unwrap().is_none());
    }

    #[test]
    fn test_statecallback_wait() {
        let sc = StateCallback::<()>::new(Box::new(move |_| {}));
        let barrier = Arc::new(Barrier::new(2));

        {
            let c = sc.clone();
            let b = barrier.clone();
            thread::spawn(move || {
                b.wait();
                c.call(());
            });
        }

        barrier.wait();
        sc.wait();
    }
}
