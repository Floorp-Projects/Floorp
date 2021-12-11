#![cfg(feature = "use_std")]

use crate::MinMaxResult;
use std::collections::HashMap;
use std::cmp::Ordering;
use std::hash::Hash;
use std::iter::Iterator;
use std::ops::{Add, Mul};

/// A wrapper to allow for an easy [`into_grouping_map_by`](crate::Itertools::into_grouping_map_by)
#[derive(Clone, Debug)]
pub struct MapForGrouping<I, F>(I, F);

impl<I, F> MapForGrouping<I, F> {
    pub(crate) fn new(iter: I, key_mapper: F) -> Self {
        Self(iter, key_mapper)
    }
}

impl<K, V, I, F> Iterator for MapForGrouping<I, F>
    where I: Iterator<Item = V>,
          K: Hash + Eq,
          F: FnMut(&V) -> K,
{
    type Item = (K, V);
    fn next(&mut self) -> Option<Self::Item> {
        self.0.next().map(|val| ((self.1)(&val), val))
    }
}

/// Creates a new `GroupingMap` from `iter`
pub fn new<I, K, V>(iter: I) -> GroupingMap<I>
    where I: Iterator<Item = (K, V)>,
          K: Hash + Eq,
{
    GroupingMap { iter }
}

/// `GroupingMapBy` is an intermediate struct for efficient group-and-fold operations.
/// 
/// See [`GroupingMap`] for more informations.
#[must_use = "GroupingMapBy is lazy and do nothing unless consumed"]
pub type GroupingMapBy<I, F> = GroupingMap<MapForGrouping<I, F>>;

/// `GroupingMap` is an intermediate struct for efficient group-and-fold operations.
/// It groups elements by their key and at the same time fold each group
/// using some aggregating operation.
/// 
/// No method on this struct performs temporary allocations.
#[derive(Clone, Debug)]
#[must_use = "GroupingMap is lazy and do nothing unless consumed"]
pub struct GroupingMap<I> {
    iter: I,
}

impl<I, K, V> GroupingMap<I>
    where I: Iterator<Item = (K, V)>,
          K: Hash + Eq,
{
    /// This is the generic way to perform any operation on a `GroupingMap`.
    /// It's suggested to use this method only to implement custom operations
    /// when the already provided ones are not enough.
    /// 
    /// Groups elements from the `GroupingMap` source by key and applies `operation` to the elements
    /// of each group sequentially, passing the previously accumulated value, a reference to the key
    /// and the current element as arguments, and stores the results in an `HashMap`.
    ///
    /// The `operation` function is invoked on each element with the following parameters:
    ///  - the current value of the accumulator of the group if there is currently one;
    ///  - a reference to the key of the group this element belongs to;
    ///  - the element from the source being aggregated;
    /// 
    /// If `operation` returns `Some(element)` then the accumulator is updated with `element`,
    /// otherwise the previous accumulation is discarded.
    ///
    /// Return a `HashMap` associating the key of each group with the result of aggregation of
    /// that group's elements. If the aggregation of the last element of a group discards the
    /// accumulator then there won't be an entry associated to that group's key.
    /// 
    /// ```
    /// use itertools::Itertools;
    /// 
    /// let data = vec![2, 8, 5, 7, 9, 0, 4, 10];
    /// let lookup = data.into_iter()
    ///     .into_grouping_map_by(|&n| n % 4)
    ///     .aggregate(|acc, _key, val| {
    ///         if val == 0 || val == 10 {
    ///             None
    ///         } else {
    ///             Some(acc.unwrap_or(0) + val)
    ///         }
    ///     });
    /// 
    /// assert_eq!(lookup[&0], 4);        // 0 resets the accumulator so only 4 is summed
    /// assert_eq!(lookup[&1], 5 + 9);
    /// assert_eq!(lookup.get(&2), None); // 10 resets the accumulator and nothing is summed afterward
    /// assert_eq!(lookup[&3], 7);
    /// assert_eq!(lookup.len(), 3);      // The final keys are only 0, 1 and 2
    /// ```
    pub fn aggregate<FO, R>(self, mut operation: FO) -> HashMap<K, R>
        where FO: FnMut(Option<R>, &K, V) -> Option<R>,
    {
        let mut destination_map = HashMap::new();

        self.iter.for_each(|(key, val)| {
            let acc = destination_map.remove(&key);
            if let Some(op_res) = operation(acc, &key, val) {
                destination_map.insert(key, op_res);
            }
        });

        destination_map
    }

    /// Groups elements from the `GroupingMap` source by key and applies `operation` to the elements
    /// of each group sequentially, passing the previously accumulated value, a reference to the key
    /// and the current element as arguments, and stores the results in a new map.
    ///
    /// `init` is the value from which will be cloned the initial value of each accumulator.
    ///
    /// `operation` is a function that is invoked on each element with the following parameters:
    ///  - the current value of the accumulator of the group;
    ///  - a reference to the key of the group this element belongs to;
    ///  - the element from the source being accumulated.
    ///
    /// Return a `HashMap` associating the key of each group with the result of folding that group's elements.
    /// 
    /// ```
    /// use itertools::Itertools;
    /// 
    /// let lookup = (1..=7)
    ///     .into_grouping_map_by(|&n| n % 3)
    ///     .fold(0, |acc, _key, val| acc + val);
    /// 
    /// assert_eq!(lookup[&0], 3 + 6);
    /// assert_eq!(lookup[&1], 1 + 4 + 7);
    /// assert_eq!(lookup[&2], 2 + 5);
    /// assert_eq!(lookup.len(), 3);
    /// ```
    pub fn fold<FO, R>(self, init: R, mut operation: FO) -> HashMap<K, R>
        where R: Clone,
              FO: FnMut(R, &K, V) -> R,
    {
        self.aggregate(|acc, key, val| {
            let acc = acc.unwrap_or_else(|| init.clone());
            Some(operation(acc, key, val))
        })
    }

    /// Groups elements from the `GroupingMap` source by key and applies `operation` to the elements
    /// of each group sequentially, passing the previously accumulated value, a reference to the key
    /// and the current element as arguments, and stores the results in a new map.
    ///
    /// This is similar to [`fold`] but the initial value of the accumulator is the first element of the group.
    ///
    /// `operation` is a function that is invoked on each element with the following parameters:
    ///  - the current value of the accumulator of the group;
    ///  - a reference to the key of the group this element belongs to;
    ///  - the element from the source being accumulated.
    ///
    /// Return a `HashMap` associating the key of each group with the result of folding that group's elements.
    /// 
    /// [`fold`]: GroupingMap::fold
    /// 
    /// ```
    /// use itertools::Itertools;
    /// 
    /// let lookup = (1..=7)
    ///     .into_grouping_map_by(|&n| n % 3)
    ///     .fold_first(|acc, _key, val| acc + val);
    /// 
    /// assert_eq!(lookup[&0], 3 + 6);
    /// assert_eq!(lookup[&1], 1 + 4 + 7);
    /// assert_eq!(lookup[&2], 2 + 5);
    /// assert_eq!(lookup.len(), 3);
    /// ```
    pub fn fold_first<FO>(self, mut operation: FO) -> HashMap<K, V>
        where FO: FnMut(V, &K, V) -> V,
    {
        self.aggregate(|acc, key, val| {
            Some(match acc {
                Some(acc) => operation(acc, key, val),
                None => val,
            })
        })
    }

    /// Groups elements from the `GroupingMap` source by key and collects the elements of each group in
    /// an instance of `C`. The iteration order is preserved when inserting elements. 
    /// 
    /// Return a `HashMap` associating the key of each group with the collection containing that group's elements.
    /// 
    /// ```
    /// use itertools::Itertools;
    /// use std::collections::HashSet;
    /// 
    /// let lookup = vec![0, 1, 2, 3, 4, 5, 6, 2, 3, 6].into_iter()
    ///     .into_grouping_map_by(|&n| n % 3)
    ///     .collect::<HashSet<_>>();
    /// 
    /// assert_eq!(lookup[&0], vec![0, 3, 6].into_iter().collect::<HashSet<_>>());
    /// assert_eq!(lookup[&1], vec![1, 4].into_iter().collect::<HashSet<_>>());
    /// assert_eq!(lookup[&2], vec![2, 5].into_iter().collect::<HashSet<_>>());
    /// assert_eq!(lookup.len(), 3);
    /// ```
    pub fn collect<C>(self) -> HashMap<K, C>
        where C: Default + Extend<V>,
    {
        let mut destination_map = HashMap::new();

        self.iter.for_each(|(key, val)| {
            destination_map.entry(key).or_insert_with(C::default).extend(Some(val));
        });

        destination_map
    }

    /// Groups elements from the `GroupingMap` source by key and finds the maximum of each group.
    /// 
    /// If several elements are equally maximum, the last element is picked.
    /// 
    /// Returns a `HashMap` associating the key of each group with the maximum of that group's elements.
    /// 
    /// ```
    /// use itertools::Itertools;
    /// 
    /// let lookup = vec![1, 3, 4, 5, 7, 8, 9, 12].into_iter()
    ///     .into_grouping_map_by(|&n| n % 3)
    ///     .max();
    /// 
    /// assert_eq!(lookup[&0], 12);
    /// assert_eq!(lookup[&1], 7);
    /// assert_eq!(lookup[&2], 8);
    /// assert_eq!(lookup.len(), 3);
    /// ```
    pub fn max(self) -> HashMap<K, V>
        where V: Ord,
    {
        self.max_by(|_, v1, v2| V::cmp(v1, v2))
    }

    /// Groups elements from the `GroupingMap` source by key and finds the maximum of each group
    /// with respect to the specified comparison function.
    /// 
    /// If several elements are equally maximum, the last element is picked.
    /// 
    /// Returns a `HashMap` associating the key of each group with the maximum of that group's elements.
    /// 
    /// ```
    /// use itertools::Itertools;
    /// 
    /// let lookup = vec![1, 3, 4, 5, 7, 8, 9, 12].into_iter()
    ///     .into_grouping_map_by(|&n| n % 3)
    ///     .max_by(|_key, x, y| y.cmp(x));
    /// 
    /// assert_eq!(lookup[&0], 3);
    /// assert_eq!(lookup[&1], 1);
    /// assert_eq!(lookup[&2], 5);
    /// assert_eq!(lookup.len(), 3);
    /// ```
    pub fn max_by<F>(self, mut compare: F) -> HashMap<K, V>
        where F: FnMut(&K, &V, &V) -> Ordering,
    {
        self.fold_first(|acc, key, val| match compare(key, &acc, &val) {
            Ordering::Less | Ordering::Equal => val,
            Ordering::Greater => acc
        })
    }

    /// Groups elements from the `GroupingMap` source by key and finds the element of each group
    /// that gives the maximum from the specified function.
    /// 
    /// If several elements are equally maximum, the last element is picked.
    /// 
    /// Returns a `HashMap` associating the key of each group with the maximum of that group's elements.
    /// 
    /// ```
    /// use itertools::Itertools;
    /// 
    /// let lookup = vec![1, 3, 4, 5, 7, 8, 9, 12].into_iter()
    ///     .into_grouping_map_by(|&n| n % 3)
    ///     .max_by_key(|_key, &val| val % 4);
    /// 
    /// assert_eq!(lookup[&0], 3);
    /// assert_eq!(lookup[&1], 7);
    /// assert_eq!(lookup[&2], 5);
    /// assert_eq!(lookup.len(), 3);
    /// ```
    pub fn max_by_key<F, CK>(self, mut f: F) -> HashMap<K, V>
        where F: FnMut(&K, &V) -> CK,
              CK: Ord,
    {
        self.max_by(|key, v1, v2| f(key, &v1).cmp(&f(key, &v2)))
    }

    /// Groups elements from the `GroupingMap` source by key and finds the minimum of each group.
    /// 
    /// If several elements are equally minimum, the first element is picked.
    /// 
    /// Returns a `HashMap` associating the key of each group with the minimum of that group's elements.
    /// 
    /// ```
    /// use itertools::Itertools;
    /// 
    /// let lookup = vec![1, 3, 4, 5, 7, 8, 9, 12].into_iter()
    ///     .into_grouping_map_by(|&n| n % 3)
    ///     .min();
    /// 
    /// assert_eq!(lookup[&0], 3);
    /// assert_eq!(lookup[&1], 1);
    /// assert_eq!(lookup[&2], 5);
    /// assert_eq!(lookup.len(), 3);
    /// ```
    pub fn min(self) -> HashMap<K, V>
        where V: Ord,
    {
        self.min_by(|_, v1, v2| V::cmp(v1, v2))
    }

    /// Groups elements from the `GroupingMap` source by key and finds the minimum of each group
    /// with respect to the specified comparison function.
    /// 
    /// If several elements are equally minimum, the first element is picked.
    /// 
    /// Returns a `HashMap` associating the key of each group with the minimum of that group's elements.
    /// 
    /// ```
    /// use itertools::Itertools;
    /// 
    /// let lookup = vec![1, 3, 4, 5, 7, 8, 9, 12].into_iter()
    ///     .into_grouping_map_by(|&n| n % 3)
    ///     .min_by(|_key, x, y| y.cmp(x));
    /// 
    /// assert_eq!(lookup[&0], 12);
    /// assert_eq!(lookup[&1], 7);
    /// assert_eq!(lookup[&2], 8);
    /// assert_eq!(lookup.len(), 3);
    /// ```
    pub fn min_by<F>(self, mut compare: F) -> HashMap<K, V>
        where F: FnMut(&K, &V, &V) -> Ordering,
    {
        self.fold_first(|acc, key, val| match compare(key, &acc, &val) {
            Ordering::Less | Ordering::Equal => acc,
            Ordering::Greater => val
        })
    }

    /// Groups elements from the `GroupingMap` source by key and finds the element of each group
    /// that gives the minimum from the specified function.
    /// 
    /// If several elements are equally minimum, the first element is picked.
    /// 
    /// Returns a `HashMap` associating the key of each group with the minimum of that group's elements.
    /// 
    /// ```
    /// use itertools::Itertools;
    /// 
    /// let lookup = vec![1, 3, 4, 5, 7, 8, 9, 12].into_iter()
    ///     .into_grouping_map_by(|&n| n % 3)
    ///     .min_by_key(|_key, &val| val % 4);
    /// 
    /// assert_eq!(lookup[&0], 12);
    /// assert_eq!(lookup[&1], 4);
    /// assert_eq!(lookup[&2], 8);
    /// assert_eq!(lookup.len(), 3);
    /// ```
    pub fn min_by_key<F, CK>(self, mut f: F) -> HashMap<K, V>
        where F: FnMut(&K, &V) -> CK,
              CK: Ord,
    {
        self.min_by(|key, v1, v2| f(key, &v1).cmp(&f(key, &v2)))
    }

    /// Groups elements from the `GroupingMap` source by key and find the maximum and minimum of
    /// each group.
    /// 
    /// If several elements are equally maximum, the last element is picked.
    /// If several elements are equally minimum, the first element is picked.
    /// 
    /// See [.minmax()](crate::Itertools::minmax) for the non-grouping version.
    /// 
    /// Differences from the non grouping version:
    /// - It never produces a `MinMaxResult::NoElements`
    /// - It doesn't have any speedup
    /// 
    /// Returns a `HashMap` associating the key of each group with the minimum and maximum of that group's elements.
    /// 
    /// ```
    /// use itertools::Itertools;
    /// use itertools::MinMaxResult::{OneElement, MinMax};
    /// 
    /// let lookup = vec![1, 3, 4, 5, 7, 9, 12].into_iter()
    ///     .into_grouping_map_by(|&n| n % 3)
    ///     .minmax();
    /// 
    /// assert_eq!(lookup[&0], MinMax(3, 12));
    /// assert_eq!(lookup[&1], MinMax(1, 7));
    /// assert_eq!(lookup[&2], OneElement(5));
    /// assert_eq!(lookup.len(), 3);
    /// ```
    pub fn minmax(self) -> HashMap<K, MinMaxResult<V>>
        where V: Ord,
    {
        self.minmax_by(|_, v1, v2| V::cmp(v1, v2))
    }

    /// Groups elements from the `GroupingMap` source by key and find the maximum and minimum of
    /// each group with respect to the specified comparison function.
    /// 
    /// If several elements are equally maximum, the last element is picked.
    /// If several elements are equally minimum, the first element is picked.
    /// 
    /// It has the same differences from the non-grouping version as `minmax`.
    /// 
    /// Returns a `HashMap` associating the key of each group with the minimum and maximum of that group's elements.
    /// 
    /// ```
    /// use itertools::Itertools;
    /// use itertools::MinMaxResult::{OneElement, MinMax};
    /// 
    /// let lookup = vec![1, 3, 4, 5, 7, 9, 12].into_iter()
    ///     .into_grouping_map_by(|&n| n % 3)
    ///     .minmax_by(|_key, x, y| y.cmp(x));
    /// 
    /// assert_eq!(lookup[&0], MinMax(12, 3));
    /// assert_eq!(lookup[&1], MinMax(7, 1));
    /// assert_eq!(lookup[&2], OneElement(5));
    /// assert_eq!(lookup.len(), 3);
    /// ```
    pub fn minmax_by<F>(self, mut compare: F) -> HashMap<K, MinMaxResult<V>>
        where F: FnMut(&K, &V, &V) -> Ordering,
    {
        self.aggregate(|acc, key, val| {
            Some(match acc {
                Some(MinMaxResult::OneElement(e)) => {
                    if compare(key, &val, &e) == Ordering::Less {
                        MinMaxResult::MinMax(val, e)
                    } else {
                        MinMaxResult::MinMax(e, val)
                    }
                }
                Some(MinMaxResult::MinMax(min, max)) => {
                    if compare(key, &val, &min) == Ordering::Less {
                        MinMaxResult::MinMax(val, max)
                    } else if compare(key, &val, &max) != Ordering::Less {
                        MinMaxResult::MinMax(min, val)
                    } else {
                        MinMaxResult::MinMax(min, max)
                    }
                }
                None => MinMaxResult::OneElement(val),
                Some(MinMaxResult::NoElements) => unreachable!(),
            })
        })
    }

    /// Groups elements from the `GroupingMap` source by key and find the elements of each group
    /// that gives the minimum and maximum from the specified function.
    /// 
    /// If several elements are equally maximum, the last element is picked.
    /// If several elements are equally minimum, the first element is picked.
    /// 
    /// It has the same differences from the non-grouping version as `minmax`.
    /// 
    /// Returns a `HashMap` associating the key of each group with the minimum and maximum of that group's elements.
    /// 
    /// ```
    /// use itertools::Itertools;
    /// use itertools::MinMaxResult::{OneElement, MinMax};
    /// 
    /// let lookup = vec![1, 3, 4, 5, 7, 9, 12].into_iter()
    ///     .into_grouping_map_by(|&n| n % 3)
    ///     .minmax_by_key(|_key, &val| val % 4);
    /// 
    /// assert_eq!(lookup[&0], MinMax(12, 3));
    /// assert_eq!(lookup[&1], MinMax(4, 7));
    /// assert_eq!(lookup[&2], OneElement(5));
    /// assert_eq!(lookup.len(), 3);
    /// ```
    pub fn minmax_by_key<F, CK>(self, mut f: F) -> HashMap<K, MinMaxResult<V>>
        where F: FnMut(&K, &V) -> CK,
              CK: Ord,
    {
        self.minmax_by(|key, v1, v2| f(key, &v1).cmp(&f(key, &v2)))
    }
    
    /// Groups elements from the `GroupingMap` source by key and sums them.
    /// 
    /// This is just a shorthand for `self.fold_first(|acc, _, val| acc + val)`.
    /// It is more limited than `Iterator::sum` since it doesn't use the `Sum` trait.
    /// 
    /// Returns a `HashMap` associating the key of each group with the sum of that group's elements.
    /// 
    /// ```
    /// use itertools::Itertools;
    /// 
    /// let lookup = vec![1, 3, 4, 5, 7, 8, 9, 12].into_iter()
    ///     .into_grouping_map_by(|&n| n % 3)
    ///     .sum();
    /// 
    /// assert_eq!(lookup[&0], 3 + 9 + 12);
    /// assert_eq!(lookup[&1], 1 + 4 + 7);
    /// assert_eq!(lookup[&2], 5 + 8);
    /// assert_eq!(lookup.len(), 3);
    /// ```
    pub fn sum(self) -> HashMap<K, V>
        where V: Add<V, Output = V>
    {
        self.fold_first(|acc, _, val| acc + val)
    }

    /// Groups elements from the `GroupingMap` source by key and multiply them.
    /// 
    /// This is just a shorthand for `self.fold_first(|acc, _, val| acc * val)`.
    /// It is more limited than `Iterator::product` since it doesn't use the `Product` trait.
    /// 
    /// Returns a `HashMap` associating the key of each group with the product of that group's elements.
    /// 
    /// ```
    /// use itertools::Itertools;
    /// 
    /// let lookup = vec![1, 3, 4, 5, 7, 8, 9, 12].into_iter()
    ///     .into_grouping_map_by(|&n| n % 3)
    ///     .product();
    /// 
    /// assert_eq!(lookup[&0], 3 * 9 * 12);
    /// assert_eq!(lookup[&1], 1 * 4 * 7);
    /// assert_eq!(lookup[&2], 5 * 8);
    /// assert_eq!(lookup.len(), 3);
    /// ```
    pub fn product(self) -> HashMap<K, V>
        where V: Mul<V, Output = V>,
    {
        self.fold_first(|acc, _, val| acc * val)
    }
}
