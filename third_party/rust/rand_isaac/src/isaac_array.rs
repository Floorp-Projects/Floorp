// Copyright 2018 Developers of the Rand project.
// Copyright 2017-2018 The Rust Project Developers.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! ISAAC helper functions for 256-element arrays.

// Terrible workaround because arrays with more than 32 elements do not
// implement `AsRef`, `Default`, `Serialize`, `Deserialize`, or any other
// traits for that matter.

#[cfg(feature="serde1")] use serde::{Serialize, Deserialize};

const RAND_SIZE_LEN: usize = 8;
const RAND_SIZE: usize = 1 << RAND_SIZE_LEN;


#[derive(Copy, Clone)]
#[allow(missing_debug_implementations)]
#[cfg_attr(feature="serde1", derive(Serialize, Deserialize))]
pub struct IsaacArray<T> {
    #[cfg_attr(feature="serde1",serde(with="isaac_array_serde"))]
    #[cfg_attr(feature="serde1", serde(bound(
        serialize = "T: Serialize",
        deserialize = "T: Deserialize<'de> + Copy + Default")))]
    inner: [T; RAND_SIZE]
}

impl<T> ::core::convert::AsRef<[T]> for IsaacArray<T> {
    #[inline(always)]
    fn as_ref(&self) -> &[T] {
        &self.inner[..]
    }
}

impl<T> ::core::convert::AsMut<[T]> for IsaacArray<T> {
    #[inline(always)]
    fn as_mut(&mut self) -> &mut [T] {
        &mut self.inner[..]
    }
}

impl<T> ::core::ops::Deref for IsaacArray<T> {
    type Target = [T; RAND_SIZE];
    #[inline(always)]
    fn deref(&self) -> &Self::Target {
        &self.inner
    }
}

impl<T> ::core::ops::DerefMut for IsaacArray<T> {
    #[inline(always)]
    fn deref_mut(&mut self) -> &mut [T; RAND_SIZE] {
        &mut self.inner
    }
}

impl<T> ::core::default::Default for IsaacArray<T> where T: Copy + Default {
    fn default() -> IsaacArray<T> {
        IsaacArray { inner: [T::default(); RAND_SIZE] }
    }
}


#[cfg(feature="serde1")]
pub(super) mod isaac_array_serde {
    const RAND_SIZE_LEN: usize = 8;
    const RAND_SIZE: usize = 1 << RAND_SIZE_LEN;

    use serde::{Deserialize, Deserializer, Serialize, Serializer};
    use serde::de::{Visitor,SeqAccess};
    use serde::de;

    use core::fmt;

    pub fn serialize<T, S>(arr: &[T;RAND_SIZE], ser: S) -> Result<S::Ok, S::Error>
    where
        T: Serialize,
        S: Serializer
    {
        use serde::ser::SerializeTuple;

        let mut seq = ser.serialize_tuple(RAND_SIZE)?;

        for e in arr.iter() {
            seq.serialize_element(&e)?;
        }

        seq.end()
    }

    #[inline]
    pub fn deserialize<'de, T, D>(de: D) -> Result<[T;RAND_SIZE], D::Error>
    where
        T: Deserialize<'de>+Default+Copy,
        D: Deserializer<'de>,
    {
        use core::marker::PhantomData;
        struct ArrayVisitor<T> {
            _pd: PhantomData<T>,
        };
        impl<'de,T> Visitor<'de> for ArrayVisitor<T>
        where
            T: Deserialize<'de>+Default+Copy
        {
            type Value = [T; RAND_SIZE];

            fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
                formatter.write_str("Isaac state array")
            }

            #[inline]
            fn visit_seq<A>(self, mut seq: A) -> Result<[T; RAND_SIZE], A::Error>
            where
                A: SeqAccess<'de>,
            {
                let mut out = [Default::default();RAND_SIZE];

                for i in 0..RAND_SIZE {
                    match seq.next_element()? {
                        Some(val) => out[i] = val,
                        None => return Err(de::Error::invalid_length(i, &self)),
                    };
                }

                Ok(out)
            }
        }

        de.deserialize_tuple(RAND_SIZE, ArrayVisitor{_pd: PhantomData})
    }
}
