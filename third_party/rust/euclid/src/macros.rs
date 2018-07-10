// Copyright 2013 The Servo Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

macro_rules! define_matrix {
    (
        $(#[$attr:meta])*
        pub struct $name:ident<T, $($phantom:ident),+> {
            $(pub $field:ident: T,)+
        }
    ) => (
        #[repr(C)]
        $(#[$attr])*
        pub struct $name<T, $($phantom),+> {
            $(pub $field: T,)+

            // Keep this (secretly) public for the few cases where we would like to
            // create static constants which currently can't be initialized with a
            // function.
            #[doc(hidden)]
            pub _unit: PhantomData<($($phantom),+)>
        }

        impl<T: Clone, $($phantom),+> Clone for $name<T, $($phantom),+> {
            fn clone(&self) -> Self {
                $name {
                    $($field: self.$field.clone(),)+
                    _unit: PhantomData,
                }
            }
        }

        impl<T: Copy, $($phantom),+> Copy for $name<T, $($phantom),+> {}

        #[cfg(feature = "serde")]
        impl<'de, T, $($phantom),+> ::serde::Deserialize<'de> for $name<T, $($phantom),+>
            where T: ::serde::Deserialize<'de>
        {
            fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
                where D: ::serde::Deserializer<'de>
            {
                let ($($field,)+) =
                    try!(::serde::Deserialize::deserialize(deserializer));
                Ok($name {
                    $($field: $field,)+
                    _unit: PhantomData,
                })
            }
        }

        #[cfg(feature = "serde")]
        impl<T, $($phantom),+> ::serde::Serialize for $name<T, $($phantom),+>
            where T: ::serde::Serialize
        {
            fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
                where S: ::serde::Serializer
            {
                ($(&self.$field,)+).serialize(serializer)
            }
        }

        impl<T, $($phantom),+> ::core::cmp::Eq for $name<T, $($phantom),+>
            where T: ::core::cmp::Eq {}

        impl<T, $($phantom),+> ::core::cmp::PartialEq for $name<T, $($phantom),+>
            where T: ::core::cmp::PartialEq
        {
            fn eq(&self, other: &Self) -> bool {
                true $(&& self.$field == other.$field)+
            }
        }

        impl<T, $($phantom),+> ::core::hash::Hash for $name<T, $($phantom),+>
            where T: ::core::hash::Hash
        {
            fn hash<H: ::core::hash::Hasher>(&self, h: &mut H) {
                $(self.$field.hash(h);)+
            }
        }
    )
}
