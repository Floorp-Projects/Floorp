// SPDX-FileCopyrightText: 2021 Bruno Corrêa Zimmermann <brunoczim@gmail.com>
// SPDX-FileCopyrightText: 2021 Kamila Borowska <kamila@borowska.pw>
//
// SPDX-License-Identifier: MIT OR Apache-2.0

use crate::{enum_map, EnumArray, EnumMap};
use arbitrary::{Arbitrary, Result, Unstructured};

/// Requires crate feature `"arbitrary"`
impl<'a, K: EnumArray<V>, V: Arbitrary<'a>> Arbitrary<'a> for EnumMap<K, V> {
    fn arbitrary(u: &mut Unstructured<'a>) -> Result<EnumMap<K, V>> {
        Ok(enum_map! {
            _ => Arbitrary::arbitrary(u)?,
        })
    }

    fn size_hint(depth: usize) -> (usize, Option<usize>) {
        if K::LENGTH == 0 {
            (0, Some(0))
        } else {
            let (lo, hi) = V::size_hint(depth);
            (
                lo.saturating_mul(K::LENGTH),
                hi.and_then(|hi| hi.checked_mul(K::LENGTH)),
            )
        }
    }
}
