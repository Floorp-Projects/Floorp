use std::{
    cell::{RefCell, UnsafeCell},
    cmp::Ordering,
    pin::Pin,
    task::Context,
    task::Poll,
    task::Waker,
};

use crate::generator::{BundleIterator, BundleStream};
use crate::pin_cell::{PinCell, PinMut};
use chunky_vec::ChunkyVec;
use futures::{ready, Stream};

pub struct Cache<I, R>
where
    I: Iterator,
{
    iter: RefCell<I>,
    items: UnsafeCell<ChunkyVec<I::Item>>,
    res: std::marker::PhantomData<R>,
}

impl<I, R> Cache<I, R>
where
    I: Iterator,
{
    pub fn new(iter: I) -> Self {
        Self {
            iter: RefCell::new(iter),
            items: Default::default(),
            res: std::marker::PhantomData,
        }
    }

    pub fn len(&self) -> usize {
        unsafe {
            let items = self.items.get();
            (*items).len()
        }
    }

    pub fn get(&self, index: usize) -> Option<&I::Item> {
        unsafe {
            let items = self.items.get();
            (*items).get(index)
        }
    }

    /// Push, immediately getting a reference to the element
    pub fn push_get(&self, new_value: I::Item) -> &I::Item {
        unsafe {
            let items = self.items.get();
            (*items).push_get(new_value)
        }
    }
}

impl<I, R> Cache<I, R>
where
    I: BundleIterator + Iterator,
{
    pub fn prefetch(&self) {
        self.iter.borrow_mut().prefetch_sync();
    }
}

pub struct CacheIter<'a, I, R>
where
    I: Iterator,
{
    cache: &'a Cache<I, R>,
    curr: usize,
}

impl<'a, I, R> Iterator for CacheIter<'a, I, R>
where
    I: Iterator,
{
    type Item = &'a I::Item;

    fn next(&mut self) -> Option<Self::Item> {
        let cache_len = self.cache.len();
        match self.curr.cmp(&cache_len) {
            Ordering::Less => {
                // Cached value
                self.curr += 1;
                self.cache.get(self.curr - 1)
            }
            Ordering::Equal => {
                // Get the next item from the iterator
                let item = self.cache.iter.borrow_mut().next();
                self.curr += 1;
                if let Some(item) = item {
                    Some(self.cache.push_get(item))
                } else {
                    None
                }
            }
            Ordering::Greater => {
                // Ran off the end of the cache
                None
            }
        }
    }
}

impl<'a, I, R> IntoIterator for &'a Cache<I, R>
where
    I: Iterator,
{
    type Item = &'a I::Item;
    type IntoIter = CacheIter<'a, I, R>;

    fn into_iter(self) -> Self::IntoIter {
        CacheIter {
            cache: self,
            curr: 0,
        }
    }
}

////////////////////////////////////////////////////////////////////////////////

pub struct AsyncCache<S, R>
where
    S: Stream,
{
    stream: PinCell<S>,
    items: UnsafeCell<ChunkyVec<S::Item>>,
    // TODO: Should probably be an SmallVec<[Waker; 1]> or something? I guess
    // multiple pending wakes are not really all that common.
    pending_wakes: RefCell<Vec<Waker>>,
    res: std::marker::PhantomData<R>,
}

impl<S, R> AsyncCache<S, R>
where
    S: Stream,
{
    pub fn new(stream: S) -> Self {
        Self {
            stream: PinCell::new(stream),
            items: Default::default(),
            pending_wakes: Default::default(),
            res: std::marker::PhantomData,
        }
    }

    pub fn len(&self) -> usize {
        unsafe {
            let items = self.items.get();
            (*items).len()
        }
    }

    pub fn get(&self, index: usize) -> Poll<Option<&S::Item>> {
        unsafe {
            let items = self.items.get();
            (*items).get(index).into()
        }
    }

    /// Push, immediately getting a reference to the element
    pub fn push_get(&self, new_value: S::Item) -> &S::Item {
        unsafe {
            let items = self.items.get();
            (*items).push_get(new_value)
        }
    }

    pub fn stream(&self) -> AsyncCacheStream<'_, S, R> {
        AsyncCacheStream {
            cache: self,
            curr: 0,
        }
    }
}

impl<S, R> AsyncCache<S, R>
where
    S: BundleStream + Stream,
{
    pub async fn prefetch(&self) {
        let pin = unsafe { Pin::new_unchecked(&self.stream) };
        unsafe { PinMut::as_mut(&mut pin.borrow_mut()).get_unchecked_mut() }
            .prefetch_async()
            .await
    }
}

impl<S, R> AsyncCache<S, R>
where
    S: Stream,
{
    // Helper function that gets the next value from wrapped stream.
    fn poll_next_item(&self, cx: &mut Context<'_>) -> Poll<Option<S::Item>> {
        let pin = unsafe { Pin::new_unchecked(&self.stream) };
        let poll = PinMut::as_mut(&mut pin.borrow_mut()).poll_next(cx);
        if poll.is_ready() {
            let wakers = std::mem::take(&mut *self.pending_wakes.borrow_mut());
            for waker in wakers {
                waker.wake();
            }
        } else {
            self.pending_wakes.borrow_mut().push(cx.waker().clone());
        }
        poll
    }
}

pub struct AsyncCacheStream<'a, S, R>
where
    S: Stream,
{
    cache: &'a AsyncCache<S, R>,
    curr: usize,
}

impl<'a, S, R> Stream for AsyncCacheStream<'a, S, R>
where
    S: Stream,
{
    type Item = &'a S::Item;

    fn poll_next(
        mut self: std::pin::Pin<&mut Self>,
        cx: &mut std::task::Context<'_>,
    ) -> Poll<Option<Self::Item>> {
        let cache_len = self.cache.len();
        match self.curr.cmp(&cache_len) {
            Ordering::Less => {
                // Cached value
                self.curr += 1;
                self.cache.get(self.curr - 1)
            }
            Ordering::Equal => {
                // Get the next item from the stream
                let item = ready!(self.cache.poll_next_item(cx));
                self.curr += 1;
                if let Some(item) = item {
                    Some(self.cache.push_get(item)).into()
                } else {
                    None.into()
                }
            }
            Ordering::Greater => {
                // Ran off the end of the cache
                None.into()
            }
        }
    }
}
