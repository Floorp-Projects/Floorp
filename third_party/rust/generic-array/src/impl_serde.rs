//! Serde serialization/deserialization implementation

use {ArrayLength, GenericArray};
use core::fmt;
use core::marker::PhantomData;
use serde::{Deserialize, Deserializer, Serialize, Serializer};
use serde::de::{self, SeqAccess, Visitor};

impl<T, N> Serialize for GenericArray<T, N>
where
    T: Serialize,
    N: ArrayLength<T>,
{
    #[inline]
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        serializer.collect_seq(self.iter())
    }
}

struct GAVisitor<T, N> {
    _t: PhantomData<T>,
    _n: PhantomData<N>,
}

impl<'de, T, N> Visitor<'de> for GAVisitor<T, N>
where
    T: Deserialize<'de> + Default,
    N: ArrayLength<T>,
{
    type Value = GenericArray<T, N>;

    fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
        formatter.write_str("struct GenericArray")
    }

    fn visit_seq<A>(self, mut seq: A) -> Result<GenericArray<T, N>, A::Error>
    where
        A: SeqAccess<'de>,
    {
        let mut result = GenericArray::default();
        for i in 0..N::to_usize() {
            result[i] = seq.next_element()?.ok_or_else(
                || de::Error::invalid_length(i, &self),
            )?;
        }
        Ok(result)
    }
}

impl<'de, T, N> Deserialize<'de> for GenericArray<T, N>
where
    T: Deserialize<'de> + Default,
    N: ArrayLength<T>,
{
    fn deserialize<D>(deserializer: D) -> Result<GenericArray<T, N>, D::Error>
    where
        D: Deserializer<'de>,
    {
        let visitor = GAVisitor {
            _t: PhantomData,
            _n: PhantomData,
        };
        deserializer.deserialize_seq(visitor)
    }
}
