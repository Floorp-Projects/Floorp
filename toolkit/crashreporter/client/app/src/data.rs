/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! Data binding types are used to implement dynamic behaviors in UIs. [`Event`] is the primitive
//! type underlying most others. [Properties](Property) are what should usually be used in UI
//! models, since they have `From` impls allowing different binding behaviors to be set.

use std::cell::RefCell;
use std::rc::Rc;

/// An event which can have multiple subscribers.
///
/// The type parameter is the payload of the event.
pub struct Event<T> {
    subscribers: Rc<RefCell<Vec<Box<dyn Fn(&T)>>>>,
}

impl<T> Clone for Event<T> {
    fn clone(&self) -> Self {
        Event {
            subscribers: self.subscribers.clone(),
        }
    }
}

impl<T> Default for Event<T> {
    fn default() -> Self {
        Event {
            subscribers: Default::default(),
        }
    }
}

impl<T> std::fmt::Debug for Event<T> {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        write!(
            f,
            "{} {{ {} subscribers }}",
            std::any::type_name::<Self>(),
            self.subscribers.borrow().len()
        )
    }
}

impl<T> Event<T> {
    /// Add a callback for when the event is fired.
    pub fn subscribe<F>(&self, f: F)
    where
        F: Fn(&T) + 'static,
    {
        self.subscribers.borrow_mut().push(Box::new(f));
    }

    /// Fire the event with the given payload.
    pub fn fire(&self, payload: &T) {
        for f in self.subscribers.borrow().iter() {
            f(payload);
        }
    }
}

/// A synchronized runtime value.
///
/// Consumers can subscribe to change events on the value. Change events are fired when
/// `borrow_mut()` references are dropped.
#[derive(Default)]
pub struct Synchronized<T> {
    inner: Rc<SynchronizedInner<T>>,
}

impl<T> Clone for Synchronized<T> {
    fn clone(&self) -> Self {
        Synchronized {
            inner: self.inner.clone(),
        }
    }
}

impl<T: std::fmt::Debug> std::fmt::Debug for Synchronized<T> {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        f.debug_struct(std::any::type_name::<Self>())
            .field("current", &*self.inner.current.borrow())
            .field("change", &self.inner.change)
            .finish()
    }
}

#[derive(Default)]
struct SynchronizedInner<T> {
    current: RefCell<T>,
    change: Event<T>,
}

impl<T> Synchronized<T> {
    /// Create a new value with the given inner data.
    pub fn new(initial: T) -> Self {
        Synchronized {
            inner: Rc::new(SynchronizedInner {
                current: RefCell::new(initial),
                change: Default::default(),
            }),
        }
    }

    /// Borrow a value's data.
    pub fn borrow(&self) -> std::cell::Ref<T> {
        self.inner.current.borrow()
    }

    /// Mutably borrow a value's data.
    ///
    /// When the mutable reference is dropped, a change event is fired.
    pub fn borrow_mut(&self) -> ValueRefMut<'_, T> {
        ValueRefMut {
            value: std::mem::ManuallyDrop::new(self.inner.current.borrow_mut()),
            inner: &self.inner,
        }
    }

    /// Subscribe to change events in the value.
    pub fn on_change<F: Fn(&T) + 'static>(&self, f: F) {
        self.inner.change.subscribe(f);
    }

    /// Update another synchronized value when this one changes.
    pub fn update_on_change<U: 'static, F: Fn(&T) -> U + 'static>(
        &self,
        other: &Synchronized<U>,
        f: F,
    ) {
        let other = other.clone();
        self.on_change(move |val| {
            *other.borrow_mut() = f(val);
        });
    }

    /// Create a new synchronized value which will update when this one changes.
    pub fn mapped<U: 'static, F: Fn(&T) -> U + 'static>(&self, f: F) -> Synchronized<U> {
        let s = Synchronized::new(f(&*self.borrow()));
        self.update_on_change(&s, f);
        s
    }

    pub fn join<A: 'static, B: 'static, F: Fn(&A, &B) -> T + Clone + 'static>(
        a: &Synchronized<A>,
        b: &Synchronized<B>,
        f: F,
    ) -> Self
    where
        T: 'static,
    {
        let s = Synchronized::new(f(&*a.borrow(), &*b.borrow()));
        let update = cc! { (a,b,s) move || {
            *s.borrow_mut() = f(&*a.borrow(), &*b.borrow());
        }};
        a.on_change(cc! { (update) move |_| update()});
        b.on_change(move |_| update());
        s
    }
}

/// A runtime value that can be fetched on-demand (read-only).
///
/// Consumers call [`read`] or [`get`] to retrieve the value, while producers call [`register`] to
/// set the function which is called to retrieve the value. This is of most use for things like
/// editable text strings, where it would be unnecessarily expensive to e.g. update a
/// `Synchronized` property as the text string is changed (debouncing could be used, but if change
/// notification isn't needed then it's still unnecessary).
pub struct OnDemand<T> {
    get: Rc<RefCell<Option<Box<dyn Fn(&mut T) + 'static>>>>,
}

impl<T> Default for OnDemand<T> {
    fn default() -> Self {
        OnDemand {
            get: Default::default(),
        }
    }
}

impl<T> Clone for OnDemand<T> {
    fn clone(&self) -> Self {
        OnDemand {
            get: self.get.clone(),
        }
    }
}

impl<T> std::fmt::Debug for OnDemand<T> {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        write!(
            f,
            "{} {{ {} }}",
            std::any::type_name::<Self>(),
            if self.get.borrow().is_none() {
                "not registered"
            } else {
                "registered"
            }
        )
    }
}

impl<T> OnDemand<T> {
    /// Reads the current value.
    pub fn read(&self, value: &mut T) {
        match &*self.get.borrow() {
            None => {
                // The test UI doesn't always register OnDemand getters (only on a per-test basis),
                // so don't panic otherwise the tests will fail unnecessarily.
                #[cfg(not(test))]
                panic!("OnDemand not registered by renderer")
            }
            Some(f) => f(value),
        }
    }

    /// Get a copy of the current value.
    pub fn get(&self) -> T
    where
        T: Default,
    {
        let mut r = T::default();
        self.read(&mut r);
        r
    }

    /// Register the function to use when getting the value.
    pub fn register(&self, f: impl Fn(&mut T) + 'static) {
        *self.get.borrow_mut() = Some(Box::new(f));
    }
}

/// A UI element property.
///
/// Properties support static and dynamic value bindings.
/// * `T` can be converted to static bindings.
/// * `Synchronized<T>` can be converted to dynamic bindings which will be updated
/// bidirectionally.
/// * `OnDemand<T>` can be converted to dynamic bindings which can be queried on an as-needed
/// basis.
#[derive(Clone, Debug)]
pub enum Property<T> {
    Static(T),
    Binding(Synchronized<T>),
    ReadOnly(OnDemand<T>),
}

#[cfg(test)]
impl<T: Clone + Default + 'static> Property<T> {
    pub fn set(&self, value: T) {
        match self {
            Property::Static(_) => panic!("cannot set static property"),
            Property::Binding(s) => *s.borrow_mut() = value,
            Property::ReadOnly(o) => o.register(move |v| *v = value.clone()),
        }
    }

    pub fn get(&self) -> T {
        match self {
            Property::Static(v) => v.clone(),
            Property::Binding(s) => s.borrow().clone(),
            Property::ReadOnly(o) => o.get(),
        }
    }
}

impl<T: Default> Default for Property<T> {
    fn default() -> Self {
        Property::Static(Default::default())
    }
}

impl<T> From<T> for Property<T> {
    fn from(value: T) -> Self {
        Property::Static(value)
    }
}

impl<T> From<&Synchronized<T>> for Property<T> {
    fn from(value: &Synchronized<T>) -> Self {
        Property::Binding(value.clone())
    }
}

impl<T> From<&OnDemand<T>> for Property<T> {
    fn from(value: &OnDemand<T>) -> Self {
        Property::ReadOnly(value.clone())
    }
}

/// A mutable Value reference.
///
/// When dropped, the Value's change event will fire (_after_ demoting the RefMut to a Ref).
pub struct ValueRefMut<'a, T> {
    value: std::mem::ManuallyDrop<std::cell::RefMut<'a, T>>,
    inner: &'a SynchronizedInner<T>,
}

impl<T> std::ops::Deref for ValueRefMut<'_, T> {
    type Target = T;

    fn deref(&self) -> &Self::Target {
        &*self.value
    }
}

impl<T> std::ops::DerefMut for ValueRefMut<'_, T> {
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut *self.value
    }
}

impl<T> Drop for ValueRefMut<'_, T> {
    fn drop(&mut self) {
        unsafe { std::mem::ManuallyDrop::drop(&mut self.value) };
        self.inner.change.fire(&*self.inner.current.borrow());
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use std::sync::atomic::{AtomicUsize, Ordering::Relaxed};

    #[derive(Default, Clone)]
    struct Trace {
        count: Rc<AtomicUsize>,
    }

    impl Trace {
        fn inc(&self) {
            self.count.fetch_add(1, Relaxed);
        }

        fn set(&self, v: usize) {
            self.count.store(v, Relaxed);
        }

        fn count(&self) -> usize {
            self.count.load(Relaxed)
        }
    }

    #[test]
    fn event() {
        let t1 = Trace::default();
        let t2 = Trace::default();
        let evt = Event::default();
        evt.subscribe(cc! { (t1) move |x| {
            assert!(x == &42);
            t1.inc()
        }});
        evt.fire(&42);
        assert_eq!(t1.count(), 1);
        evt.subscribe(cc! { (t2) move |_| t2.inc() });
        evt.fire(&42);
        assert_eq!(t1.count(), 2);
        assert_eq!(t2.count(), 1);
    }

    #[test]
    fn synchronized() {
        let t1 = Trace::default();
        let s = Synchronized::<usize>::default();
        assert_eq!(*s.borrow(), 0);

        s.on_change(cc! { (t1) move |v| t1.set(*v) });
        {
            let mut s_ref = s.borrow_mut();
            *s_ref = 41;
            // Changes should only occur when the ref is dropped
            assert_eq!(t1.count(), 0);
            *s_ref = 42;
        }
        assert_eq!(t1.count(), 42);
        assert_eq!(*s.borrow(), 42);
    }

    #[test]
    fn ondemand() {
        let t1 = Trace::default();
        let d = OnDemand::<usize>::default();
        d.register(|v| *v = 42);
        {
            let mut v = 0;
            d.read(&mut v);
            assert_eq!(v, 42);
        }
        d.register(|v| *v = 10);
        assert_eq!(d.get(), 10);

        t1.inc();
        d.register(cc! { (t1) move |v| *v = t1.count() });
        assert_eq!(d.get(), 1);
        t1.set(42);
        assert_eq!(d.get(), 42);
    }
}
