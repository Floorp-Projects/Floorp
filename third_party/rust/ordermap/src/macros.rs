
#[macro_export]
/// Create an `OrderMap` from a list of key-value pairs
///
/// ## Example
///
/// ```
/// #[macro_use] extern crate ordermap;
/// # fn main() {
///
/// let map = ordermap!{
///     "a" => 1,
///     "b" => 2,
/// };
/// assert_eq!(map["a"], 1);
/// assert_eq!(map["b"], 2);
/// assert_eq!(map.get("c"), None);
///
/// // "a" is the first key
/// assert_eq!(map.keys().next(), Some(&"a"));
/// # }
/// ```
macro_rules! ordermap {
    (@single $($x:tt)*) => (());
    (@count $($rest:expr),*) => (<[()]>::len(&[$(ordermap!(@single $rest)),*]));

    ($($key:expr => $value:expr,)+) => { ordermap!($($key => $value),+) };
    ($($key:expr => $value:expr),*) => {
        {
            let _cap = ordermap!(@count $($key),*);
            let mut _map = $crate::OrderMap::with_capacity(_cap);
            $(
                _map.insert($key, $value);
            )*
            _map
        }
    };
}

#[macro_export]
/// Create an `OrderSet` from a list of values
///
/// ## Example
///
/// ```
/// #[macro_use] extern crate ordermap;
/// # fn main() {
///
/// let set = orderset!{
///     "a",
///     "b",
/// };
/// assert!(set.contains("a"));
/// assert!(set.contains("b"));
/// assert!(!set.contains("c"));
///
/// // "a" is the first value
/// assert_eq!(set.iter().next(), Some(&"a"));
/// # }
/// ```
macro_rules! orderset {
    (@single $($x:tt)*) => (());
    (@count $($rest:expr),*) => (<[()]>::len(&[$(orderset!(@single $rest)),*]));

    ($($value:expr,)+) => { orderset!($($value),+) };
    ($($value:expr),*) => {
        {
            let _cap = orderset!(@count $($value),*);
            let mut _set = $crate::OrderSet::with_capacity(_cap);
            $(
                _set.insert($value);
            )*
            _set
        }
    };
}

// generate all the Iterator methods by just forwarding to the underlying
// self.iter and mapping its element.
macro_rules! iterator_methods {
    // $map_elt is the mapping function from the underlying iterator's element
    // same mapping function for both options and iterators
    ($map_elt:expr) => {
        fn next(&mut self) -> Option<Self::Item> {
            self.iter.next().map($map_elt)
        }

        fn size_hint(&self) -> (usize, Option<usize>) {
            self.iter.size_hint()
        }

        fn count(self) -> usize {
            self.iter.len()
        }

        fn nth(&mut self, n: usize) -> Option<Self::Item> {
            self.iter.nth(n).map($map_elt)
        }

        fn last(mut self) -> Option<Self::Item> {
            self.next_back()
        }

        fn collect<C>(self) -> C
            where C: FromIterator<Self::Item>
        {
            // NB: forwarding this directly to standard iterators will
            // allow it to leverage unstable traits like `TrustedLen`.
            self.iter.map($map_elt).collect()
        }
    }
}

macro_rules! double_ended_iterator_methods {
    // $map_elt is the mapping function from the underlying iterator's element
    // same mapping function for both options and iterators
    ($map_elt:expr) => {
        fn next_back(&mut self) -> Option<Self::Item> {
            self.iter.next_back().map($map_elt)
        }
    }
}
