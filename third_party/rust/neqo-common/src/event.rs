// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use std::{iter::Iterator, marker::PhantomData};

/// An event provider is able to generate a stream of events.
pub trait Provider {
    type Event;

    /// Get the next event.
    #[must_use]
    fn next_event(&mut self) -> Option<Self::Event>;

    /// Determine whether there are pending events.
    #[must_use]
    fn has_events(&self) -> bool;

    /// Construct an iterator that produces all events.
    fn events(&'_ mut self) -> Iter<'_, Self, Self::Event> {
        Iter::new(self)
    }
}

pub struct Iter<'a, P, E>
where
    P: ?Sized,
{
    p: &'a mut P,
    _e: PhantomData<E>,
}

impl<'a, P, E> Iter<'a, P, E>
where
    P: Provider<Event = E> + ?Sized,
{
    fn new(p: &'a mut P) -> Self {
        Self { p, _e: PhantomData }
    }
}

impl<'a, P, E> Iterator for Iter<'a, P, E>
where
    P: Provider<Event = E>,
{
    type Item = E;
    fn next(&mut self) -> Option<Self::Item> {
        self.p.next_event()
    }
}
