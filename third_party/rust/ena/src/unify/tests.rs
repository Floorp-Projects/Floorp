// Copyright 2015 The Rust Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution and at
// http://rust-lang.org/COPYRIGHT.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#[cfg(feature = "unstable")]
extern crate test;
#[cfg(feature = "unstable")]
use self::test::Bencher;
use std::collections::HashSet;
use std::cmp;
use unify::{UnifyKey, UnifyValue, UnificationTable, InfallibleUnifyValue};

#[derive(Copy, Clone, Debug, Hash, PartialEq, Eq)]
struct UnitKey(u32);

impl UnifyKey for UnitKey {
    type Value = ();
    fn index(&self) -> u32 {
        self.0
    }
    fn from_index(u: u32) -> UnitKey {
        UnitKey(u)
    }
    fn tag() -> &'static str {
        "UnitKey"
    }
}

#[test]
fn basic() {
    let mut ut: UnificationTable<UnitKey> = UnificationTable::new();
    let k1 = ut.new_key(());
    let k2 = ut.new_key(());
    assert_eq!(ut.unioned(k1, k2), false);
    ut.union(k1, k2);
    assert_eq!(ut.unioned(k1, k2), true);
}

#[test]
fn big_array() {
    let mut ut: UnificationTable<UnitKey> = UnificationTable::new();
    let mut keys = Vec::new();
    const MAX: usize = 1 << 15;

    for _ in 0..MAX {
        keys.push(ut.new_key(()));
    }

    for i in 1..MAX {
        let l = keys[i - 1];
        let r = keys[i];
        ut.union(l, r);
    }

    for i in 0..MAX {
        assert!(ut.unioned(keys[0], keys[i]));
    }
}

#[cfg(feature = "unstable")]
#[bench]
fn big_array_bench(b: &mut Bencher) {
    let mut ut: UnificationTable<UnitKey> = UnificationTable::new();
    let mut keys = Vec::new();
    const MAX: usize = 1 << 15;

    for _ in 0..MAX {
        keys.push(ut.new_key(()));
    }

    b.iter(|| {
        for i in 1..MAX {
            let l = keys[i - 1];
            let r = keys[i];
            ut.union(l, r);
        }

        for i in 0..MAX {
            assert!(ut.unioned(keys[0], keys[i]));
        }
    })
}

#[test]
fn even_odd() {
    let mut ut: UnificationTable<UnitKey> = UnificationTable::new();
    let mut keys = Vec::new();
    const MAX: usize = 1 << 10;

    for i in 0..MAX {
        let key = ut.new_key(());
        keys.push(key);

        if i >= 2 {
            ut.union(key, keys[i - 2]);
        }
    }

    for i in 1..MAX {
        assert!(!ut.unioned(keys[i - 1], keys[i]));
    }

    for i in 2..MAX {
        assert!(ut.unioned(keys[i - 2], keys[i]));
    }
}

#[test]
fn even_odd_iter() {
    let mut ut: UnificationTable<UnitKey> = UnificationTable::new();
    let mut keys = Vec::new();
    const MAX: usize = 1 << 10;

    for i in 0..MAX {
        let key = ut.new_key(());
        keys.push(key);

        if i >= 2 {
            ut.union(key, keys[i - 2]);
        }
    }

    let even_keys: HashSet<UnitKey> = ut.unioned_keys(keys[22]).collect();

    assert_eq!(even_keys.len(), MAX / 2);

    for key in even_keys {
        assert!((key.0 & 1) == 0);
    }
}

#[derive(Copy, Clone, Debug, Hash, PartialEq, Eq)]
struct IntKey(u32);

impl UnifyKey for IntKey {
    type Value = Option<i32>;
    fn index(&self) -> u32 {
        self.0
    }
    fn from_index(u: u32) -> IntKey {
        IntKey(u)
    }
    fn tag() -> &'static str {
        "IntKey"
    }
}

impl UnifyValue for i32 {
    fn unify_values(&a: &i32, &b: &i32) -> Result<Self, (Self, Self)> {
        if a == b {
            Ok(a)
        } else {
            Err((a, b))
        }
    }
}

#[test]
fn unify_same_int_twice() {
    let mut ut: UnificationTable<IntKey> = UnificationTable::new();
    let k1 = ut.new_key(None);
    let k2 = ut.new_key(None);
    assert!(ut.unify_var_value(k1, Some(22)).is_ok());
    assert!(ut.unify_var_value(k2, Some(22)).is_ok());
    assert!(ut.unify_var_var(k1, k2).is_ok());
    assert_eq!(ut.probe_value(k1), Some(22));
}

#[test]
fn unify_vars_then_int_indirect() {
    let mut ut: UnificationTable<IntKey> = UnificationTable::new();
    let k1 = ut.new_key(None);
    let k2 = ut.new_key(None);
    assert!(ut.unify_var_var(k1, k2).is_ok());
    assert!(ut.unify_var_value(k1, Some(22)).is_ok());
    assert_eq!(ut.probe_value(k2), Some(22));
}

#[test]
fn unify_vars_different_ints_1() {
    let mut ut: UnificationTable<IntKey> = UnificationTable::new();
    let k1 = ut.new_key(None);
    let k2 = ut.new_key(None);
    assert!(ut.unify_var_var(k1, k2).is_ok());
    assert!(ut.unify_var_value(k1, Some(22)).is_ok());
    assert!(ut.unify_var_value(k2, Some(23)).is_err());
}

#[test]
fn unify_vars_different_ints_2() {
    let mut ut: UnificationTable<IntKey> = UnificationTable::new();
    let k1 = ut.new_key(None);
    let k2 = ut.new_key(None);
    assert!(ut.unify_var_var(k2, k1).is_ok());
    assert!(ut.unify_var_value(k1, Some(22)).is_ok());
    assert!(ut.unify_var_value(k2, Some(23)).is_err());
}

#[test]
fn unify_distinct_ints_then_vars() {
    let mut ut: UnificationTable<IntKey> = UnificationTable::new();
    let k1 = ut.new_key(None);
    let k2 = ut.new_key(None);
    assert!(ut.unify_var_value(k1, Some(22)).is_ok());
    assert!(ut.unify_var_value(k2, Some(23)).is_ok());
    assert!(ut.unify_var_var(k2, k1).is_err());
}

#[test]
fn unify_root_value_1() {
    let mut ut: UnificationTable<IntKey> = UnificationTable::new();
    let k1 = ut.new_key(None);
    let k2 = ut.new_key(None);
    let k3 = ut.new_key(None);
    assert!(ut.unify_var_value(k1, Some(22)).is_ok());
    assert!(ut.unify_var_var(k1, k2).is_ok());
    assert!(ut.unify_var_value(k3, Some(23)).is_ok());
    assert!(ut.unify_var_var(k1, k3).is_err());
}

#[test]
fn unify_root_value_2() {
    let mut ut: UnificationTable<IntKey> = UnificationTable::new();
    let k1 = ut.new_key(None);
    let k2 = ut.new_key(None);
    let k3 = ut.new_key(None);
    assert!(ut.unify_var_value(k1, Some(22)).is_ok());
    assert!(ut.unify_var_var(k2, k1).is_ok());
    assert!(ut.unify_var_value(k3, Some(23)).is_ok());
    assert!(ut.unify_var_var(k1, k3).is_err());
}

#[derive(Copy, Clone, Debug, Hash, PartialEq, Eq)]
struct OrderedKey(u32);

#[derive(Copy, Clone, Debug, Hash, PartialEq, Eq, PartialOrd, Ord)]
struct OrderedRank(u32);

impl UnifyKey for OrderedKey {
    type Value = OrderedRank;
    fn index(&self) -> u32 {
        self.0
    }
    fn from_index(u: u32) -> OrderedKey {
        OrderedKey(u)
    }
    fn tag() -> &'static str {
        "OrderedKey"
    }
    fn order_roots(a: OrderedKey, a_rank: &OrderedRank,
                   b: OrderedKey, b_rank: &OrderedRank)
                   -> Option<(OrderedKey, OrderedKey)> {
        println!("{:?} vs {:?}", a_rank, b_rank);
        if a_rank > b_rank {
            Some((a, b))
        } else if b_rank > a_rank {
            Some((b, a))
        } else {
            None
        }
    }
}

impl UnifyValue for OrderedRank {
    fn unify_values(value1: &Self, value2: &Self) -> Result<Self, (Self, Self)> {
        Ok(OrderedRank(cmp::max(value1.0, value2.0)))
    }
}

impl InfallibleUnifyValue for OrderedRank { }

#[test]
fn ordered_key() {
    let mut ut: UnificationTable<OrderedKey> = UnificationTable::new();

    let k0_1 = ut.new_key(OrderedRank(0));
    let k0_2 = ut.new_key(OrderedRank(0));
    let k0_3 = ut.new_key(OrderedRank(0));
    let k0_4 = ut.new_key(OrderedRank(0));

    ut.union(k0_1, k0_2); // rank of one of those will now be 1
    ut.union(k0_3, k0_4); // rank of new root also 1
    ut.union(k0_1, k0_3); // rank of new root now 2

    let k0_5 = ut.new_key(OrderedRank(0));
    let k0_6 = ut.new_key(OrderedRank(0));
    ut.union(k0_5, k0_6); // rank of new root now 1

    ut.union(k0_1, k0_5); // new root rank 2, should not be k0_5 or k0_6
    assert!(vec![k0_1, k0_2, k0_3, k0_4].contains(&ut.find(k0_1)));
}

#[test]
fn ordered_key_k1() {
    let mut ut: UnificationTable<OrderedKey> = UnificationTable::new();

    let k0_1 = ut.new_key(OrderedRank(0));
    let k0_2 = ut.new_key(OrderedRank(0));
    let k0_3 = ut.new_key(OrderedRank(0));
    let k0_4 = ut.new_key(OrderedRank(0));

    ut.union(k0_1, k0_2); // rank of one of those will now be 1
    ut.union(k0_3, k0_4); // rank of new root also 1
    ut.union(k0_1, k0_3); // rank of new root now 2

    let k1_5 = ut.new_key(OrderedRank(1));
    let k1_6 = ut.new_key(OrderedRank(1));
    ut.union(k1_5, k1_6); // rank of new root now 1

    ut.union(k0_1, k1_5); // even though k1 has lower rank, it wins
    assert!(vec![k1_5, k1_6].contains(&ut.find(k0_1)),
            "unexpected choice for root: {:?}", ut.find(k0_1));
}

/// Test that we *can* clone.
#[test]
fn clone_table() {
    let mut ut: UnificationTable<IntKey> = UnificationTable::new();
    let k1 = ut.new_key(None);
    let k2 = ut.new_key(None);
    assert!(ut.unify_var_value(k1, Some(22)).is_ok());
    assert!(ut.unify_var_value(k2, Some(22)).is_ok());
    assert!(ut.unify_var_var(k1, k2).is_ok());

    let mut ut1 = ut.clone();
    assert_eq!(ut1.probe_value(k1), Some(22));
}
