
use std::collections::HashMap;
use std::collections::hash_map::{Entry};
use std::hash::Hash;
use std::fmt;

/// An iterator adapter to filter out duplicate elements.
///
/// See [`.unique_by()`](../trait.Itertools.html#method.unique) for more information.
#[derive(Clone)]
#[must_use = "iterator adaptors are lazy and do nothing unless consumed"]
pub struct UniqueBy<I: Iterator, V, F> {
    iter: I,
    // Use a hashmap for the entry API
    used: HashMap<V, ()>,
    f: F,
}

impl<I, V, F> fmt::Debug for UniqueBy<I, V, F>
    where I: Iterator + fmt::Debug,
          V: fmt::Debug + Hash + Eq,
{
    debug_fmt_fields!(UniqueBy, iter, used);
}

/// Create a new `UniqueBy` iterator.
pub fn unique_by<I, V, F>(iter: I, f: F) -> UniqueBy<I, V, F>
    where V: Eq + Hash,
          F: FnMut(&I::Item) -> V,
          I: Iterator,
{
    UniqueBy {
        iter: iter,
        used: HashMap::new(),
        f: f,
    }
}

// count the number of new unique keys in iterable (`used` is the set already seen)
fn count_new_keys<I, K>(mut used: HashMap<K, ()>, iterable: I) -> usize
    where I: IntoIterator<Item=K>,
          K: Hash + Eq,
{
    let iter = iterable.into_iter();
    let current_used = used.len();
    used.extend(iter.map(|key| (key, ())));
    used.len() - current_used
}

impl<I, V, F> Iterator for UniqueBy<I, V, F>
    where I: Iterator,
          V: Eq + Hash,
          F: FnMut(&I::Item) -> V
{
    type Item = I::Item;

    fn next(&mut self) -> Option<I::Item> {
        while let Some(v) = self.iter.next() {
            let key = (self.f)(&v);
            if self.used.insert(key, ()).is_none() {
                return Some(v);
            }
        }
        None
    }

    #[inline]
    fn size_hint(&self) -> (usize, Option<usize>) {
        let (low, hi) = self.iter.size_hint();
        ((low > 0 && self.used.is_empty()) as usize, hi)
    }

    fn count(self) -> usize {
        let mut key_f = self.f;
        count_new_keys(self.used, self.iter.map(move |elt| key_f(&elt)))
    }
}

impl<I> Iterator for Unique<I>
    where I: Iterator,
          I::Item: Eq + Hash + Clone
{
    type Item = I::Item;

    fn next(&mut self) -> Option<I::Item> {
        while let Some(v) = self.iter.iter.next() {
            if let Entry::Vacant(entry) = self.iter.used.entry(v) {
                let elt = entry.key().clone();
                entry.insert(());
                return Some(elt);
            }
        }
        None
    }

    #[inline]
    fn size_hint(&self) -> (usize, Option<usize>) {
        let (low, hi) = self.iter.iter.size_hint();
        ((low > 0 && self.iter.used.is_empty()) as usize, hi)
    }

    fn count(self) -> usize {
        count_new_keys(self.iter.used, self.iter.iter)
    }
}

/// An iterator adapter to filter out duplicate elements.
///
/// See [`.unique()`](../trait.Itertools.html#method.unique) for more information.
#[derive(Clone)]
#[must_use = "iterator adaptors are lazy and do nothing unless consumed"]
pub struct Unique<I: Iterator> {
    iter: UniqueBy<I, I::Item, ()>,
}

impl<I> fmt::Debug for Unique<I>
    where I: Iterator + fmt::Debug,
          I::Item: Hash + Eq + fmt::Debug,
{
    debug_fmt_fields!(Unique, iter);
}

pub fn unique<I>(iter: I) -> Unique<I>
    where I: Iterator,
          I::Item: Eq + Hash,
{
    Unique {
        iter: UniqueBy {
            iter: iter,
            used: HashMap::new(),
            f: (),
        }
    }
}
