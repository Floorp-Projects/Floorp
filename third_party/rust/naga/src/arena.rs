use std::{cmp::Ordering, fmt, hash, marker::PhantomData, num::NonZeroU32, ops};

/// An unique index in the arena array that a handle points to.
/// The "non-zero" part ensures that an `Option<Handle<T>>` has
/// the same size and representation as `Handle<T>`.
type Index = NonZeroU32;

use crate::Span;
use indexmap::set::IndexSet;

/// A strongly typed reference to an arena item.
///
/// A `Handle` value can be used as an index into an [`Arena`] or [`UniqueArena`].
#[cfg_attr(feature = "serialize", derive(serde::Serialize))]
#[cfg_attr(feature = "deserialize", derive(serde::Deserialize))]
#[cfg_attr(
    any(feature = "serialize", feature = "deserialize"),
    serde(transparent)
)]
pub struct Handle<T> {
    index: Index,
    #[cfg_attr(any(feature = "serialize", feature = "deserialize"), serde(skip))]
    marker: PhantomData<T>,
}

impl<T> Clone for Handle<T> {
    fn clone(&self) -> Self {
        Handle {
            index: self.index,
            marker: self.marker,
        }
    }
}

impl<T> Copy for Handle<T> {}

impl<T> PartialEq for Handle<T> {
    fn eq(&self, other: &Self) -> bool {
        self.index == other.index
    }
}

impl<T> Eq for Handle<T> {}

impl<T> PartialOrd for Handle<T> {
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        self.index.partial_cmp(&other.index)
    }
}

impl<T> Ord for Handle<T> {
    fn cmp(&self, other: &Self) -> Ordering {
        self.index.cmp(&other.index)
    }
}

impl<T> fmt::Debug for Handle<T> {
    fn fmt(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
        write!(formatter, "[{}]", self.index)
    }
}

impl<T> hash::Hash for Handle<T> {
    fn hash<H: hash::Hasher>(&self, hasher: &mut H) {
        self.index.hash(hasher)
    }
}

impl<T> Handle<T> {
    #[cfg(test)]
    pub const DUMMY: Self = Handle {
        index: unsafe { NonZeroU32::new_unchecked(!0) },
        marker: PhantomData,
    };

    pub(crate) fn new(index: Index) -> Self {
        Handle {
            index,
            marker: PhantomData,
        }
    }

    /// Returns the zero-based index of this handle.
    pub fn index(self) -> usize {
        let index = self.index.get() - 1;
        index as usize
    }

    /// Convert a `usize` index into a `Handle<T>`.
    fn from_usize(index: usize) -> Self {
        use std::convert::TryFrom;

        let handle_index = u32::try_from(index + 1)
            .ok()
            .and_then(Index::new)
            .expect("Failed to insert into UniqueArena. Handle overflows");
        Handle::new(handle_index)
    }

    /// Convert a `usize` index into a `Handle<T>`, without range checks.
    unsafe fn from_usize_unchecked(index: usize) -> Self {
        Handle::new(Index::new_unchecked((index + 1) as u32))
    }
}

/// A strongly typed range of handles.
#[cfg_attr(feature = "serialize", derive(serde::Serialize))]
#[cfg_attr(feature = "deserialize", derive(serde::Deserialize))]
#[cfg_attr(
    any(feature = "serialize", feature = "deserialize"),
    serde(transparent)
)]
pub struct Range<T> {
    inner: ops::Range<u32>,
    #[cfg_attr(any(feature = "serialize", feature = "deserialize"), serde(skip))]
    marker: PhantomData<T>,
}

impl<T> Clone for Range<T> {
    fn clone(&self) -> Self {
        Range {
            inner: self.inner.clone(),
            marker: self.marker,
        }
    }
}

impl<T> fmt::Debug for Range<T> {
    fn fmt(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
        write!(formatter, "[{}..{}]", self.inner.start + 1, self.inner.end)
    }
}

impl<T> Iterator for Range<T> {
    type Item = Handle<T>;
    fn next(&mut self) -> Option<Self::Item> {
        if self.inner.start < self.inner.end {
            self.inner.start += 1;
            Some(Handle {
                index: NonZeroU32::new(self.inner.start).unwrap(),
                marker: self.marker,
            })
        } else {
            None
        }
    }
}

/// An arena holding some kind of component (e.g., type, constant,
/// instruction, etc.) that can be referenced.
///
/// Adding new items to the arena produces a strongly-typed [`Handle`].
/// The arena can be indexed using the given handle to obtain
/// a reference to the stored item.
#[cfg_attr(feature = "serialize", derive(serde::Serialize))]
#[cfg_attr(feature = "serialize", serde(transparent))]
#[cfg_attr(test, derive(PartialEq))]
pub struct Arena<T> {
    /// Values of this arena.
    data: Vec<T>,
    #[cfg(feature = "span")]
    #[cfg_attr(feature = "serialize", serde(skip))]
    span_info: Vec<Span>,
}

impl<T> Default for Arena<T> {
    fn default() -> Self {
        Self::new()
    }
}

impl<T: fmt::Debug> fmt::Debug for Arena<T> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.debug_map().entries(self.iter()).finish()
    }
}

impl<T> Arena<T> {
    /// Create a new arena with no initial capacity allocated.
    pub fn new() -> Self {
        Arena {
            data: Vec::new(),
            #[cfg(feature = "span")]
            span_info: Vec::new(),
        }
    }

    /// Extracts the inner vector.
    pub fn into_inner(self) -> Vec<T> {
        self.data
    }

    /// Returns the current number of items stored in this arena.
    pub fn len(&self) -> usize {
        self.data.len()
    }

    /// Returns `true` if the arena contains no elements.
    pub fn is_empty(&self) -> bool {
        self.data.is_empty()
    }

    /// Returns an iterator over the items stored in this arena, returning both
    /// the item's handle and a reference to it.
    pub fn iter(&self) -> impl DoubleEndedIterator<Item = (Handle<T>, &T)> {
        self.data
            .iter()
            .enumerate()
            .map(|(i, v)| unsafe { (Handle::from_usize_unchecked(i), v) })
    }

    /// Returns a iterator over the items stored in this arena,
    /// returning both the item's handle and a mutable reference to it.
    pub fn iter_mut(&mut self) -> impl DoubleEndedIterator<Item = (Handle<T>, &mut T)> {
        self.data
            .iter_mut()
            .enumerate()
            .map(|(i, v)| unsafe { (Handle::from_usize_unchecked(i), v) })
    }

    /// Adds a new value to the arena, returning a typed handle.
    pub fn append(&mut self, value: T, span: Span) -> Handle<T> {
        #[cfg(not(feature = "span"))]
        let _ = span;
        let index = self.data.len();
        self.data.push(value);
        #[cfg(feature = "span")]
        self.span_info.push(span);
        Handle::from_usize(index)
    }

    /// Fetch a handle to an existing type.
    pub fn fetch_if<F: Fn(&T) -> bool>(&self, fun: F) -> Option<Handle<T>> {
        self.data
            .iter()
            .position(fun)
            .map(|index| unsafe { Handle::from_usize_unchecked(index) })
    }

    /// Adds a value with a custom check for uniqueness:
    /// returns a handle pointing to
    /// an existing element if the check succeeds, or adds a new
    /// element otherwise.
    pub fn fetch_if_or_append<F: Fn(&T, &T) -> bool>(
        &mut self,
        value: T,
        span: Span,
        fun: F,
    ) -> Handle<T> {
        if let Some(index) = self.data.iter().position(|d| fun(d, &value)) {
            unsafe { Handle::from_usize_unchecked(index) }
        } else {
            self.append(value, span)
        }
    }

    /// Adds a value with a check for uniqueness, where the check is plain comparison.
    pub fn fetch_or_append(&mut self, value: T, span: Span) -> Handle<T>
    where
        T: PartialEq,
    {
        self.fetch_if_or_append(value, span, T::eq)
    }

    pub fn try_get(&self, handle: Handle<T>) -> Option<&T> {
        self.data.get(handle.index())
    }

    /// Get a mutable reference to an element in the arena.
    pub fn get_mut(&mut self, handle: Handle<T>) -> &mut T {
        self.data.get_mut(handle.index()).unwrap()
    }

    /// Get the range of handles from a particular number of elements to the end.
    pub fn range_from(&self, old_length: usize) -> Range<T> {
        Range {
            inner: old_length as u32..self.data.len() as u32,
            marker: PhantomData,
        }
    }

    /// Clears the arena keeping all allocations
    pub fn clear(&mut self) {
        self.data.clear()
    }

    pub fn get_span(&self, handle: Handle<T>) -> Span {
        #[cfg(feature = "span")]
        {
            *self
                .span_info
                .get(handle.index())
                .unwrap_or(&Span::default())
        }
        #[cfg(not(feature = "span"))]
        {
            let _ = handle;
            Span::default()
        }
    }
}

#[cfg(feature = "deserialize")]
impl<'de, T> serde::Deserialize<'de> for Arena<T>
where
    T: serde::Deserialize<'de>,
{
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: serde::Deserializer<'de>,
    {
        let data = Vec::deserialize(deserializer)?;
        #[cfg(feature = "span")]
        let span_info = std::iter::repeat(Span::default())
            .take(data.len())
            .collect();

        Ok(Self {
            data,
            #[cfg(feature = "span")]
            span_info,
        })
    }
}

impl<T> ops::Index<Handle<T>> for Arena<T> {
    type Output = T;
    fn index(&self, handle: Handle<T>) -> &T {
        &self.data[handle.index()]
    }
}

impl<T> ops::IndexMut<Handle<T>> for Arena<T> {
    fn index_mut(&mut self, handle: Handle<T>) -> &mut T {
        &mut self.data[handle.index()]
    }
}

impl<T> ops::Index<Range<T>> for Arena<T> {
    type Output = [T];
    fn index(&self, range: Range<T>) -> &[T] {
        &self.data[range.inner.start as usize..range.inner.end as usize]
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn append_non_unique() {
        let mut arena: Arena<u8> = Arena::new();
        let t1 = arena.append(0, Default::default());
        let t2 = arena.append(0, Default::default());
        assert!(t1 != t2);
        assert!(arena[t1] == arena[t2]);
    }

    #[test]
    fn append_unique() {
        let mut arena: Arena<u8> = Arena::new();
        let t1 = arena.append(0, Default::default());
        let t2 = arena.append(1, Default::default());
        assert!(t1 != t2);
        assert!(arena[t1] != arena[t2]);
    }

    #[test]
    fn fetch_or_append_non_unique() {
        let mut arena: Arena<u8> = Arena::new();
        let t1 = arena.fetch_or_append(0, Default::default());
        let t2 = arena.fetch_or_append(0, Default::default());
        assert!(t1 == t2);
        assert!(arena[t1] == arena[t2])
    }

    #[test]
    fn fetch_or_append_unique() {
        let mut arena: Arena<u8> = Arena::new();
        let t1 = arena.fetch_or_append(0, Default::default());
        let t2 = arena.fetch_or_append(1, Default::default());
        assert!(t1 != t2);
        assert!(arena[t1] != arena[t2]);
    }
}

/// An arena whose elements are guaranteed to be unique.
///
/// A `UniqueArena` holds a set of unique values of type `T`, each with an
/// associated [`Span`]. Inserting a value returns a `Handle<T>`, which can be
/// used to index the `UniqueArena` and obtain shared access to the `T` element.
/// Access via a `Handle` is an array lookup - no hash lookup is necessary.
///
/// The element type must implement `Eq` and `Hash`. Insertions of equivalent
/// elements, according to `Eq`, all return the same `Handle`.
///
/// Once inserted, elements may not be mutated.
///
/// `UniqueArena` is similar to [`Arena`]: If `Arena` is vector-like,
/// `UniqueArena` is `HashSet`-like.
pub struct UniqueArena<T> {
    set: IndexSet<T>,

    /// Spans for the elements, indexed by handle.
    ///
    /// The length of this vector is always equal to `set.len()`. `IndexSet`
    /// promises that its elements "are indexed in a compact range, without
    /// holes in the range 0..set.len()", so we can always use the indices
    /// returned by insertion as indices into this vector.
    #[cfg(feature = "span")]
    span_info: Vec<Span>,
}

impl<T> UniqueArena<T> {
    /// Create a new arena with no initial capacity allocated.
    pub fn new() -> Self {
        UniqueArena {
            set: IndexSet::new(),
            #[cfg(feature = "span")]
            span_info: Vec::new(),
        }
    }

    /// Return the current number of items stored in this arena.
    pub fn len(&self) -> usize {
        self.set.len()
    }

    /// Return `true` if the arena contains no elements.
    pub fn is_empty(&self) -> bool {
        self.set.is_empty()
    }

    /// Clears the arena, keeping all allocations.
    pub fn clear(&mut self) {
        self.set.clear();
        #[cfg(feature = "span")]
        self.span_info.clear();
    }

    /// Return the span associated with `handle`.
    ///
    /// If a value has been inserted multiple times, the span returned is the
    /// one provided with the first insertion.
    ///
    /// If the `span` feature is not enabled, always return `Span::default`.
    /// This can be detected with [`Span::is_defined`].
    pub fn get_span(&self, handle: Handle<T>) -> Span {
        #[cfg(feature = "span")]
        {
            *self
                .span_info
                .get(handle.index())
                .unwrap_or(&Span::default())
        }
        #[cfg(not(feature = "span"))]
        {
            let _ = handle;
            Span::default()
        }
    }
}

impl<T: Eq + hash::Hash> UniqueArena<T> {
    /// Returns an iterator over the items stored in this arena, returning both
    /// the item's handle and a reference to it.
    pub fn iter(&self) -> impl DoubleEndedIterator<Item = (Handle<T>, &T)> {
        self.set.iter().enumerate().map(|(i, v)| {
            let position = i + 1;
            let index = unsafe { Index::new_unchecked(position as u32) };
            (Handle::new(index), v)
        })
    }

    /// Insert a new value into the arena.
    ///
    /// Return a [`Handle<T>`], which can be used to index this arena to get a
    /// shared reference to the element.
    ///
    /// If this arena already contains an element that is `Eq` to `value`,
    /// return a `Handle` to the existing element, and drop `value`.
    ///
    /// When the `span` feature is enabled, if `value` is inserted into the
    /// arena, associate `span` with it. An element's span can be retrieved with
    /// the [`get_span`] method.
    ///
    /// [`Handle<T>`]: Handle
    /// [`get_span`]: UniqueArena::get_span
    pub fn insert(&mut self, value: T, span: Span) -> Handle<T> {
        let (index, added) = self.set.insert_full(value);

        #[cfg(feature = "span")]
        {
            if added {
                debug_assert!(index == self.span_info.len());
                self.span_info.push(span);
            }

            debug_assert!(self.set.len() == self.span_info.len());
        }

        #[cfg(not(feature = "span"))]
        let _ = (span, added);

        Handle::from_usize(index)
    }

    /// Return this arena's handle for `value`, if present.
    ///
    /// If this arena already contains an element equal to `value`,
    /// return its handle. Otherwise, return `None`.
    pub fn get(&self, value: &T) -> Option<Handle<T>> {
        self.set
            .get_index_of(value)
            .map(|index| unsafe { Handle::from_usize_unchecked(index) })
    }

    /// Return this arena's value at `handle`, if that is a valid handle.
    pub fn get_handle(&self, handle: Handle<T>) -> Option<&T> {
        self.set.get_index(handle.index())
    }
}

impl<T> Default for UniqueArena<T> {
    fn default() -> Self {
        Self::new()
    }
}

impl<T: fmt::Debug + Eq + hash::Hash> fmt::Debug for UniqueArena<T> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.debug_map().entries(self.iter()).finish()
    }
}

impl<T> ops::Index<Handle<T>> for UniqueArena<T> {
    type Output = T;
    fn index(&self, handle: Handle<T>) -> &T {
        &self.set[handle.index()]
    }
}

#[cfg(feature = "serialize")]
impl<T> serde::Serialize for UniqueArena<T>
where
    T: Eq + hash::Hash,
    T: serde::Serialize,
{
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: serde::Serializer,
    {
        self.set.serialize(serializer)
    }
}

#[cfg(feature = "deserialize")]
impl<'de, T> serde::Deserialize<'de> for UniqueArena<T>
where
    T: Eq + hash::Hash,
    T: serde::Deserialize<'de>,
{
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: serde::Deserializer<'de>,
    {
        let set = IndexSet::deserialize(deserializer)?;
        #[cfg(feature = "span")]
        let span_info = std::iter::repeat(Span::default()).take(set.len()).collect();

        Ok(Self {
            set,
            #[cfg(feature = "span")]
            span_info,
        })
    }
}
