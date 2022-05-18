use utils::duration::DurationSigned;

use super::*;
use crate::{formats::Strictness, rust::StringWithSeparator, Separator};
use std::{
    collections::{BTreeMap, BTreeSet, BinaryHeap, HashMap, HashSet, LinkedList, VecDeque},
    fmt::Display,
    hash::{BuildHasher, Hash},
    time::{Duration, SystemTime},
};

impl<'a, T, U> SerializeAs<&'a T> for &'a U
where
    U: SerializeAs<T>,
    T: ?Sized,
    U: ?Sized,
{
    fn serialize_as<S>(source: &&'a T, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        SerializeAsWrap::<T, U>::new(source).serialize(serializer)
    }
}

impl<T, U> SerializeAs<Box<T>> for Box<U>
where
    U: SerializeAs<T>,
{
    fn serialize_as<S>(source: &Box<T>, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        SerializeAsWrap::<T, U>::new(source).serialize(serializer)
    }
}

impl<T, U> SerializeAs<Option<T>> for Option<U>
where
    U: SerializeAs<T>,
{
    fn serialize_as<S>(source: &Option<T>, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        match *source {
            Some(ref value) => serializer.serialize_some(&SerializeAsWrap::<T, U>::new(value)),
            None => serializer.serialize_none(),
        }
    }
}

macro_rules! seq_impl {
    ($ty:ident < T $(: $tbound1:ident $(+ $tbound2:ident)*)* $(, $typaram:ident : $bound:ident $(+ $bound2:ident)*)* >) => {
        impl<T, U $(, $typaram)*> SerializeAs<$ty<T $(, $typaram)*>> for $ty<U $(, $typaram)*>
        where
            U: SerializeAs<T>,
            $(T: ?Sized + $tbound1 $(+ $tbound2)*,)*
            $($typaram: ?Sized + $bound $(+ $bound2)*,)*
        {
            fn serialize_as<S>(source: &$ty<T $(, $typaram)*>, serializer: S) -> Result<S::Ok, S::Error>
            where
                S: Serializer,
            {
                serializer.collect_seq(source.iter().map(|item| SerializeAsWrap::<T, U>::new(item)))
            }
        }
    }
}

type BoxedSlice<T> = Box<[T]>;
type Slice<T> = [T];
seq_impl!(BinaryHeap<T: Ord + Sized>);
seq_impl!(BoxedSlice<T>);
seq_impl!(BTreeSet<T: Ord + Sized>);
seq_impl!(HashSet<T: Eq + Hash + Sized, H: BuildHasher + Sized>);
seq_impl!(LinkedList<T>);
seq_impl!(Slice<T>);
seq_impl!(Vec<T>);
seq_impl!(VecDeque<T>);

macro_rules! map_impl {
    ($ty:ident < K $(: $kbound1:ident $(+ $kbound2:ident)*)*, V $(, $typaram:ident : $bound:ident)* >) => {
        impl<K, KU, V, VU $(, $typaram)*> SerializeAs<$ty<K, V $(, $typaram)*>> for $ty<KU, VU $(, $typaram)*>
        where
            KU: SerializeAs<K>,
            VU: SerializeAs<V>,
            $(K: $kbound1 $(+ $kbound2)*,)*
            $($typaram: $bound,)*
        {
            fn serialize_as<S>(source: &$ty<K, V $(, $typaram)*>, serializer: S) -> Result<S::Ok, S::Error>
            where
                S: Serializer,
            {
                serializer.collect_map(source.iter().map(|(k, v)| (SerializeAsWrap::<K, KU>::new(k), SerializeAsWrap::<V, VU>::new(v))))
            }
        }
    }
}

map_impl!(BTreeMap<K: Ord, V>);
map_impl!(HashMap<K: Eq + Hash, V, H: BuildHasher>);

impl<T> SerializeAs<T> for DisplayFromStr
where
    T: Display,
{
    fn serialize_as<S>(source: &T, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        crate::rust::display_fromstr::serialize(source, serializer)
    }
}

macro_rules! tuple_impl {
    ($len:literal $($n:tt $t:ident $tas:ident)+) => {
        impl<$($t, $tas,)+> SerializeAs<($($t,)+)> for ($($tas,)+)
        where
            $($tas: SerializeAs<$t>,)+
        {
            fn serialize_as<S>(tuple: &($($t,)+), serializer: S) -> Result<S::Ok, S::Error>
            where
                S: Serializer,
            {
                use serde::ser::SerializeTuple;
                let mut tup = serializer.serialize_tuple($len)?;
                $(
                    tup.serialize_element(&SerializeAsWrap::<$t, $tas>::new(&tuple.$n))?;
                )+
                tup.end()
            }
        }
    };
}

tuple_impl!(1 0 T0 As0);
tuple_impl!(2 0 T0 As0 1 T1 As1);
tuple_impl!(3 0 T0 As0 1 T1 As1 2 T2 As2);
tuple_impl!(4 0 T0 As0 1 T1 As1 2 T2 As2 3 T3 As3);
tuple_impl!(5 0 T0 As0 1 T1 As1 2 T2 As2 3 T3 As3 4 T4 As4);
tuple_impl!(6 0 T0 As0 1 T1 As1 2 T2 As2 3 T3 As3 4 T4 As4 5 T5 As5);
tuple_impl!(7 0 T0 As0 1 T1 As1 2 T2 As2 3 T3 As3 4 T4 As4 5 T5 As5 6 T6 As6);
tuple_impl!(8 0 T0 As0 1 T1 As1 2 T2 As2 3 T3 As3 4 T4 As4 5 T5 As5 6 T6 As6 7 T7 As7);
tuple_impl!(9 0 T0 As0 1 T1 As1 2 T2 As2 3 T3 As3 4 T4 As4 5 T5 As5 6 T6 As6 7 T7 As7 8 T8 As8);
tuple_impl!(10 0 T0 As0 1 T1 As1 2 T2 As2 3 T3 As3 4 T4 As4 5 T5 As5 6 T6 As6 7 T7 As7 8 T8 As8 9 T9 As9);
tuple_impl!(11 0 T0 As0 1 T1 As1 2 T2 As2 3 T3 As3 4 T4 As4 5 T5 As5 6 T6 As6 7 T7 As7 8 T8 As8 9 T9 As9 10 T10 As10);
tuple_impl!(12 0 T0 As0 1 T1 As1 2 T2 As2 3 T3 As3 4 T4 As4 5 T5 As5 6 T6 As6 7 T7 As7 8 T8 As8 9 T9 As9 10 T10 As10 11 T11 As11);
tuple_impl!(13 0 T0 As0 1 T1 As1 2 T2 As2 3 T3 As3 4 T4 As4 5 T5 As5 6 T6 As6 7 T7 As7 8 T8 As8 9 T9 As9 10 T10 As10 11 T11 As11 12 T12 As12);
tuple_impl!(14 0 T0 As0 1 T1 As1 2 T2 As2 3 T3 As3 4 T4 As4 5 T5 As5 6 T6 As6 7 T7 As7 8 T8 As8 9 T9 As9 10 T10 As10 11 T11 As11 12 T12 As12 13 T13 As13);
tuple_impl!(15 0 T0 As0 1 T1 As1 2 T2 As2 3 T3 As3 4 T4 As4 5 T5 As5 6 T6 As6 7 T7 As7 8 T8 As8 9 T9 As9 10 T10 As10 11 T11 As11 12 T12 As12 13 T13 As13 14 T14 As14);
tuple_impl!(16 0 T0 As0 1 T1 As1 2 T2 As2 3 T3 As3 4 T4 As4 5 T5 As5 6 T6 As6 7 T7 As7 8 T8 As8 9 T9 As9 10 T10 As10 11 T11 As11 12 T12 As12 13 T13 As13 14 T14 As14 15 T15 As15);

macro_rules! array_impl {
    ($len:literal) => {
        impl<T, As> SerializeAs<[T; $len]> for [As; $len]
        where
            As: SerializeAs<T>,
        {
            fn serialize_as<S>(array: &[T; $len], serializer: S) -> Result<S::Ok, S::Error>
            where
                S: Serializer,
            {
                use serde::ser::SerializeTuple;
                let mut arr = serializer.serialize_tuple($len)?;
                for elem in array {
                    arr.serialize_element(&SerializeAsWrap::<T, As>::new(elem))?;
                }
                arr.end()
            }
        }
    };
}

array_impl!(0);
array_impl!(1);
array_impl!(2);
array_impl!(3);
array_impl!(4);
array_impl!(5);
array_impl!(6);
array_impl!(7);
array_impl!(8);
array_impl!(9);
array_impl!(10);
array_impl!(11);
array_impl!(12);
array_impl!(13);
array_impl!(14);
array_impl!(15);
array_impl!(16);
array_impl!(17);
array_impl!(18);
array_impl!(19);
array_impl!(20);
array_impl!(21);
array_impl!(22);
array_impl!(23);
array_impl!(24);
array_impl!(25);
array_impl!(26);
array_impl!(27);
array_impl!(28);
array_impl!(29);
array_impl!(30);
array_impl!(31);
array_impl!(32);

impl<T> SerializeAs<T> for Same
where
    T: Serialize + ?Sized,
{
    fn serialize_as<S>(source: &T, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        source.serialize(serializer)
    }
}

macro_rules! map_as_tuple_seq {
    ($ty:ident < K $(: $kbound1:ident $(+ $kbound2:ident)*)*, V $(, $typaram:ident : $bound:ident)* >) => {
        impl<K, KAs, V, VAs> SerializeAs<$ty<K, V>> for Vec<(KAs, VAs)>
        where
            KAs: SerializeAs<K>,
            VAs: SerializeAs<V>,
        {
            fn serialize_as<S>(source: &$ty<K, V>, serializer: S) -> Result<S::Ok, S::Error>
            where
                S: Serializer,
            {
                serializer.collect_seq(source.iter().map(|(k, v)| {
                    (
                        SerializeAsWrap::<K, KAs>::new(k),
                        SerializeAsWrap::<V, VAs>::new(v),
                    )
                }))
            }
        }
    };
}
map_as_tuple_seq!(BTreeMap<K: Ord, V>);
map_as_tuple_seq!(HashMap<K: Eq + Hash, V, H: BuildHasher>);

impl<AsRefStr> SerializeAs<Option<AsRefStr>> for NoneAsEmptyString
where
    AsRefStr: AsRef<str>,
{
    fn serialize_as<S>(source: &Option<AsRefStr>, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        crate::rust::string_empty_as_none::serialize(source, serializer)
    }
}

macro_rules! tuple_seq_as_map_impl_intern {
    ($tyorig:ty, $ty:ident <K, V>) => {
        #[allow(clippy::implicit_hasher)]
        impl<K, KAs, V, VAs> SerializeAs<$tyorig> for $ty<KAs, VAs>
        where
            KAs: SerializeAs<K>,
            VAs: SerializeAs<V>,
        {
            fn serialize_as<S>(source: &$tyorig, serializer: S) -> Result<S::Ok, S::Error>
            where
                S: Serializer,
            {
                serializer.collect_map(source.iter().map(|(k, v)| {
                    (
                        SerializeAsWrap::<K, KAs>::new(k),
                        SerializeAsWrap::<V, VAs>::new(v),
                    )
                }))
            }
        }
    };
}
macro_rules! tuple_seq_as_map_impl {
    ($($ty:ty $(,)?)+) => {$(
        tuple_seq_as_map_impl_intern!($ty, BTreeMap<K, V>);
        tuple_seq_as_map_impl_intern!($ty, HashMap<K, V>);
    )+}
}

tuple_seq_as_map_impl! {
    BinaryHeap<(K, V)>,
    BTreeSet<(K, V)>,
    HashSet<(K, V)>,
    LinkedList<(K, V)>,
    Option<(K, V)>,
    Vec<(K, V)>,
    VecDeque<(K, V)>,
}
tuple_seq_as_map_impl! {
    [(K, V); 0], [(K, V); 1], [(K, V); 2], [(K, V); 3], [(K, V); 4], [(K, V); 5], [(K, V); 6],
    [(K, V); 7], [(K, V); 8], [(K, V); 9], [(K, V); 10], [(K, V); 11], [(K, V); 12], [(K, V); 13],
    [(K, V); 14], [(K, V); 15], [(K, V); 16], [(K, V); 17], [(K, V); 18], [(K, V); 19], [(K, V); 20],
    [(K, V); 21], [(K, V); 22], [(K, V); 23], [(K, V); 24], [(K, V); 25], [(K, V); 26], [(K, V); 27],
    [(K, V); 28], [(K, V); 29], [(K, V); 30], [(K, V); 31], [(K, V); 32],
}

impl<T, TAs> SerializeAs<T> for DefaultOnError<TAs>
where
    TAs: SerializeAs<T>,
{
    fn serialize_as<S>(source: &T, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        TAs::serialize_as(source, serializer)
    }
}

impl SerializeAs<Vec<u8>> for BytesOrString {
    fn serialize_as<S>(source: &Vec<u8>, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        source.serialize(serializer)
    }
}

impl<SEPARATOR, I, T> SerializeAs<I> for StringWithSeparator<SEPARATOR, T>
where
    SEPARATOR: Separator,
    for<'a> &'a I: IntoIterator<Item = &'a T>,
    T: ToString,
{
    fn serialize_as<S>(source: &I, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        let mut s = String::new();
        for v in source {
            s.push_str(&*v.to_string());
            s.push_str(SEPARATOR::separator());
        }
        serializer.serialize_str(if !s.is_empty() {
            // remove trailing separator if present
            &s[..s.len() - SEPARATOR::separator().len()]
        } else {
            &s[..]
        })
    }
}

macro_rules! use_signed_duration {
    (
        $main_trait:ident $internal_trait:ident =>
        {
            $ty:ty; $converter:ident =>
            $({
                $format:ty, $strictness:ty =>
                $($tbound:ident: $bound:ident)*
            })*
        }
    ) => {
        $(
            impl<$($tbound,)*> SerializeAs<$ty> for $main_trait<$format, $strictness>
            where
                $($tbound: $bound,)*
            {
                fn serialize_as<S>(source: &$ty, serializer: S) -> Result<S::Ok, S::Error>
                where
                    S: Serializer,
                {
                    $internal_trait::<$format, $strictness>::serialize_as(
                        &DurationSigned::from(source),
                        serializer,
                    )
                }
            }
        )*
    };
    (
        $( $main_trait:ident $internal_trait:ident, )+ => $rest:tt
    ) => {
        $( use_signed_duration!($main_trait $internal_trait => $rest); )+
    };
}

use_signed_duration!(
    DurationSeconds DurationSeconds,
    DurationMilliSeconds DurationMilliSeconds,
    DurationMicroSeconds DurationMicroSeconds,
    DurationNanoSeconds DurationNanoSeconds,
    => {
        Duration; to_std_duration =>
        {u64, STRICTNESS => STRICTNESS: Strictness}
        {f64, STRICTNESS => STRICTNESS: Strictness}
        {String, STRICTNESS => STRICTNESS: Strictness}
    }
);
use_signed_duration!(
    DurationSecondsWithFrac DurationSecondsWithFrac,
    DurationMilliSecondsWithFrac DurationMilliSecondsWithFrac,
    DurationMicroSecondsWithFrac DurationMicroSecondsWithFrac,
    DurationNanoSecondsWithFrac DurationNanoSecondsWithFrac,
    => {
        Duration; to_std_duration =>
        {f64, STRICTNESS => STRICTNESS: Strictness}
        {String, STRICTNESS => STRICTNESS: Strictness}
    }
);

use_signed_duration!(
    TimestampSeconds DurationSeconds,
    TimestampMilliSeconds DurationMilliSeconds,
    TimestampMicroSeconds DurationMicroSeconds,
    TimestampNanoSeconds DurationNanoSeconds,
    => {
        SystemTime; to_system_time =>
        {i64, STRICTNESS => STRICTNESS: Strictness}
        {f64, STRICTNESS => STRICTNESS: Strictness}
        {String, STRICTNESS => STRICTNESS: Strictness}
    }
);
use_signed_duration!(
    TimestampSecondsWithFrac DurationSecondsWithFrac,
    TimestampMilliSecondsWithFrac DurationMilliSecondsWithFrac,
    TimestampMicroSecondsWithFrac DurationMicroSecondsWithFrac,
    TimestampNanoSecondsWithFrac DurationNanoSecondsWithFrac,
    => {
        SystemTime; to_system_time =>
        {f64, STRICTNESS => STRICTNESS: Strictness}
        {String, STRICTNESS => STRICTNESS: Strictness}
    }
);

impl<T, U> SerializeAs<T> for DefaultOnNull<U>
where
    U: SerializeAs<T>,
{
    fn serialize_as<S>(source: &T, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        serializer.serialize_some(&SerializeAsWrap::<T, U>::new(source))
    }
}
