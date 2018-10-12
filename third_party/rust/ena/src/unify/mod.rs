// Copyright 2012-2014 The Rust Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution and at
// http://rust-lang.org/COPYRIGHT.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use std::marker;
use std::fmt::Debug;
use std::marker::PhantomData;
use snapshot_vec as sv;

#[cfg(test)]
mod tests;

/// This trait is implemented by any type that can serve as a type
/// variable. We call such variables *unification keys*. For example,
/// this trait is implemented by `IntVid`, which represents integral
/// variables.
///
/// Each key type has an associated value type `V`. For example, for
/// `IntVid`, this is `Option<IntVarValue>`, representing some
/// (possibly not yet known) sort of integer.
///
/// Clients are expected to provide implementations of this trait; you
/// can see some examples in the `test` module.
pub trait UnifyKey : Copy + Clone + Debug + PartialEq {
    type Value: UnifyValue;

    fn index(&self) -> u32;

    fn from_index(u: u32) -> Self;

    fn tag() -> &'static str;

    /// If true, then `self` should be preferred as root to `other`.
    /// Note that we assume a consistent partial ordering, so
    /// returning true implies that `other.prefer_as_root_to(self)`
    /// would return false.  If there is no ordering between two keys
    /// (i.e., `a.prefer_as_root_to(b)` and `b.prefer_as_root_to(a)`
    /// both return false) then the rank will be used to determine the
    /// root in an optimal way.
    ///
    /// NB. The only reason to implement this method is if you want to
    /// control what value is returned from `find()`. In general, it
    /// is better to let the unification table determine the root,
    /// since overriding the rank can cause execution time to increase
    /// dramatically.
    #[allow(unused_variables)]
    fn order_roots(a: Self, a_value: &Self::Value,
                   b: Self, b_value: &Self::Value)
                   -> Option<(Self, Self)> {
        None
    }
}

pub trait UnifyValue: Clone + Debug {
    /// Given two values, produce a new value that combines them.
    /// If that is not possible, produce an error.
    fn unify_values(value1: &Self, value2: &Self) -> Result<Self, (Self, Self)>;
}

/// Marker trait which indicates that `UnifyValues::unify_values` will never return `Err`.
pub trait InfallibleUnifyValue: UnifyValue {
}

/// Value of a unification key. We implement Tarjan's union-find
/// algorithm: when two keys are unified, one of them is converted
/// into a "redirect" pointing at the other. These redirects form a
/// DAG: the roots of the DAG (nodes that are not redirected) are each
/// associated with a value of type `V` and a rank. The rank is used
/// to keep the DAG relatively balanced, which helps keep the running
/// time of the algorithm under control. For more information, see
/// <http://en.wikipedia.org/wiki/Disjoint-set_data_structure>.
#[derive(PartialEq,Clone,Debug)]
struct VarValue<K: UnifyKey> {
    parent: K, // if equal to self, this is a root
    value: K::Value, // value assigned (only relevant to root)
    child: K, // if equal to self, no child (relevant to both root/redirect)
    sibling: K, // if equal to self, no sibling (only relevant to redirect)
    rank: u32, // max depth (only relevant to root)
}

/// Table of unification keys and their values.
#[derive(Clone)]
pub struct UnificationTable<K: UnifyKey> {
    /// Indicates the current value of each key.
    values: sv::SnapshotVec<Delegate<K>>,
}

/// At any time, users may snapshot a unification table.  The changes
/// made during the snapshot may either be *committed* or *rolled back*.
pub struct Snapshot<K: UnifyKey> {
    // Link snapshot to the key type `K` of the table.
    marker: marker::PhantomData<K>,
    snapshot: sv::Snapshot,
}

#[derive(Copy, Clone)]
struct Delegate<K>(PhantomData<K>);

impl<K: UnifyKey> VarValue<K> {
    fn new_var(key: K, value: K::Value) -> VarValue<K> {
        VarValue::new(key, value, key, key, 0)
    }

    fn new(parent: K, value: K::Value, child: K, sibling: K, rank: u32) -> VarValue<K> {
        VarValue {
            parent: parent, // this is a root
            value: value,
            child: child,
            sibling: sibling,
            rank: rank,
        }
    }

    fn redirect(&mut self, to: K, sibling: K) {
        assert_eq!(self.parent, self.sibling); // ...since this used to be a root
        self.parent = to;
        self.sibling = sibling;
    }

    fn root(&mut self, rank: u32, child: K, value: K::Value) {
        self.rank = rank;
        self.child = child;
        self.value = value;
    }

    /// Returns the key of this node. Only valid if this is a root
    /// node, which you yourself must ensure.
    fn key(&self) -> K {
        self.parent
    }

    fn parent(&self, self_key: K) -> Option<K> {
        self.if_not_self(self.parent, self_key)
    }

    fn child(&self, self_key: K) -> Option<K> {
        self.if_not_self(self.child, self_key)
    }

    fn sibling(&self, self_key: K) -> Option<K> {
        self.if_not_self(self.sibling, self_key)
    }

    fn if_not_self(&self, key: K, self_key: K) -> Option<K> {
        if key == self_key {
            None
        } else {
            Some(key)
        }
    }
}

// We can't use V:LatticeValue, much as I would like to,
// because frequently the pattern is that V=Option<U> for some
// other type parameter U, and we have no way to say
// Option<U>:LatticeValue.

impl<K: UnifyKey> UnificationTable<K> {
    pub fn new() -> UnificationTable<K> {
        UnificationTable { values: sv::SnapshotVec::new() }
    }

    /// Starts a new snapshot. Each snapshot must be either
    /// rolled back or committed in a "LIFO" (stack) order.
    pub fn snapshot(&mut self) -> Snapshot<K> {
        Snapshot {
            marker: marker::PhantomData::<K>,
            snapshot: self.values.start_snapshot(),
        }
    }

    /// Reverses all changes since the last snapshot. Also
    /// removes any keys that have been created since then.
    pub fn rollback_to(&mut self, snapshot: Snapshot<K>) {
        debug!("{}: rollback_to()", K::tag());
        self.values.rollback_to(snapshot.snapshot);
    }

    /// Commits all changes since the last snapshot. Of course, they
    /// can still be undone if there is a snapshot further out.
    pub fn commit(&mut self, snapshot: Snapshot<K>) {
        debug!("{}: commit()", K::tag());
        self.values.commit(snapshot.snapshot);
    }

    pub fn new_key(&mut self, value: K::Value) -> K {
        let len = self.values.len();
        let key: K = UnifyKey::from_index(len as u32);
        self.values.push(VarValue::new_var(key, value));
        debug!("{}: created new key: {:?}", K::tag(), key);
        key
    }

    pub fn unioned_keys(&mut self, key: K) -> UnionedKeys<K> {
        let root_key = self.get_root_key(key);
        UnionedKeys {
            table: self,
            stack: vec![root_key],
        }
    }

    fn value(&self, key: K) -> &VarValue<K> {
        &self.values[key.index() as usize]
    }

    /// Find the root node for `vid`. This uses the standard
    /// union-find algorithm with path compression:
    /// <http://en.wikipedia.org/wiki/Disjoint-set_data_structure>.
    ///
    /// NB. This is a building-block operation and you would probably
    /// prefer to call `probe` below.
    fn get_root_key(&mut self, vid: K) -> K {
        let redirect = {
            match self.value(vid).parent(vid) {
                None => return vid,
                Some(redirect) => redirect,
            }
        };

        let root_key: K = self.get_root_key(redirect);
        if root_key != redirect {
            // Path compression
            self.update_value(vid, |value| value.parent = root_key);
        }

        root_key
    }

    fn is_root(&self, key: K) -> bool {
        let index = key.index() as usize;
        self.values.get(index).parent(key).is_none()
    }

    fn update_value<OP>(&mut self, key: K, op: OP)
        where OP: FnOnce(&mut VarValue<K>)
    {
        self.values.update(key.index() as usize, op);
        debug!("Updated variable {:?} to {:?}", key, self.value(key));
    }

    /// Either redirects `node_a` to `node_b` or vice versa, depending
    /// on the relative rank. The value associated with the new root
    /// will be `new_value`.
    ///
    /// NB: This is the "union" operation of "union-find". It is
    /// really more of a building block. If the values associated with
    /// your key are non-trivial, you would probably prefer to call
    /// `unify_var_var` below.
    fn unify_roots(&mut self, key_a: K, key_b: K, new_value: K::Value) {
        debug!("unify(key_a={:?}, key_b={:?})",
               key_a,
               key_b);

        let rank_a = self.value(key_a).rank;
        let rank_b = self.value(key_b).rank;
        if let Some((new_root, redirected)) = K::order_roots(key_a, &self.value(key_a).value,
                                                             key_b, &self.value(key_b).value) {
            // compute the new rank for the new root that they chose;
            // this may not be the optimal choice.
            let new_rank = if new_root == key_a {
                debug_assert!(redirected == key_b);
                if rank_a > rank_b { rank_a } else { rank_b + 1 }
            } else {
                debug_assert!(new_root == key_b);
                debug_assert!(redirected == key_a);
                if rank_b > rank_a { rank_b } else { rank_a + 1 }
            };
            self.redirect_root(new_rank, redirected, new_root, new_value);
        } else if rank_a > rank_b {
            // a has greater rank, so a should become b's parent,
            // i.e., b should redirect to a.
            self.redirect_root(rank_a, key_b, key_a, new_value);
        } else if rank_a < rank_b {
            // b has greater rank, so a should redirect to b.
            self.redirect_root(rank_b, key_a, key_b, new_value);
        } else {
            // If equal, redirect one to the other and increment the
            // other's rank.
            self.redirect_root(rank_a + 1, key_a, key_b, new_value);
        }
    }

    fn redirect_root(&mut self,
                     new_rank: u32,
                     old_root_key: K,
                     new_root_key: K,
                     new_value: K::Value) {
        let sibling = self.value(new_root_key).child(new_root_key)
                                              .unwrap_or(old_root_key);
        self.update_value(old_root_key, |old_root_value| {
            old_root_value.redirect(new_root_key, sibling);
        });
        self.update_value(new_root_key, |new_root_value| {
            new_root_value.root(new_rank, old_root_key, new_value);
        });
    }
}

impl<K: UnifyKey> sv::SnapshotVecDelegate for Delegate<K> {
    type Value = VarValue<K>;
    type Undo = ();

    fn reverse(_: &mut Vec<VarValue<K>>, _: ()) {}
}

/// ////////////////////////////////////////////////////////////////////////
/// Iterator over keys that have been unioned together

pub struct UnionedKeys<'a, K>
    where K: UnifyKey + 'a,
          K::Value: 'a
{
    table: &'a mut UnificationTable<K>,
    stack: Vec<K>,
}

impl<'a, K> UnionedKeys<'a, K>
    where K: UnifyKey,
          K::Value: 'a
{
    fn var_value(&self, key: K) -> VarValue<K> {
        self.table.value(key).clone()
    }
}

impl<'a, K: 'a> Iterator for UnionedKeys<'a, K>
    where K: UnifyKey,
          K::Value: 'a
{
    type Item = K;

    fn next(&mut self) -> Option<K> {
        let key = match self.stack.last() {
            Some(k) => *k,
            None => {
                return None;
            }
        };

        let vv = self.var_value(key);

        match vv.child(key) {
            Some(child_key) => {
                self.stack.push(child_key);
            }

            None => {
                // No child, push a sibling for the current node.  If
                // current node has no siblings, start popping
                // ancestors until we find an aunt or uncle or
                // something to push. Note that we have the invariant
                // that for every node N that we reach by popping
                // items off of the stack, we have already visited all
                // children of N.
                while let Some(ancestor_key) = self.stack.pop() {
                    let ancestor_vv = self.var_value(ancestor_key);
                    match ancestor_vv.sibling(ancestor_key) {
                        Some(sibling) => {
                            self.stack.push(sibling);
                            break;
                        }
                        None => {}
                    }
                }
            }
        }

        Some(key)
    }
}

/// ////////////////////////////////////////////////////////////////////////
/// Public API

impl<'tcx, K, V> UnificationTable<K>
    where K: UnifyKey<Value = V>,
          V: UnifyValue,
{
    /// Unions two keys without the possibility of failure; only
    /// applicable to InfallibleUnifyValue.
    pub fn union(&mut self, a_id: K, b_id: K)
        where V: InfallibleUnifyValue
    {
        self.unify_var_var(a_id, b_id).unwrap();
    }

    /// Given two keys, indicates whether they have been unioned together.
    pub fn unioned(&mut self, a_id: K, b_id: K) -> bool {
        self.find(a_id) == self.find(b_id)
    }

    /// Given a key, returns the (current) root key.
    pub fn find(&mut self, id: K) -> K {
        self.get_root_key(id)
    }

    pub fn unify_var_var(&mut self, a_id: K, b_id: K) -> Result<(), (V, V)> {
        let root_a = self.get_root_key(a_id);
        let root_b = self.get_root_key(b_id);

        if root_a == root_b {
            return Ok(());
        }

        let combined = try!(V::unify_values(&self.value(root_a).value, &self.value(root_b).value));

        Ok(self.unify_roots(root_a, root_b, combined))
    }

    /// Sets the value of the key `a_id` to `b`, attempting to merge
    /// with the previous value.
    pub fn unify_var_value(&mut self, a_id: K, b: V) -> Result<(), (V, V)> {
        let root_a = self.get_root_key(a_id);
        let value = try!(V::unify_values(&self.value(root_a).value, &b));
        self.update_value(root_a, |node| node.value = value);
        Ok(())
    }

    pub fn probe_value(&mut self, id: K) -> V {
        let id = self.get_root_key(id);
        self.value(id).value.clone()
    }
}


///////////////////////////////////////////////////////////////////////////

impl UnifyValue for () {
    fn unify_values(_: &(), _: &()) -> Result<(), ((), ())> {
        Ok(())
    }
}

impl InfallibleUnifyValue for () {
}

impl<V: UnifyValue> UnifyValue for Option<V> {
    fn unify_values(a: &Option<V>, b: &Option<V>) -> Result<Self, (Self, Self)> {
        match (a, b) {
            (&None, &None) => Ok(None),
            (&Some(ref v), &None) | (&None, &Some(ref v)) => Ok(Some(v.clone())),
            (&Some(ref a), &Some(ref b)) => {
                match V::unify_values(a, b) {
                    Ok(v) => Ok(Some(v)),
                    Err((a, b)) => Err((Some(a), Some(b))),
                }
            }
        }
    }
}

impl<V: InfallibleUnifyValue> InfallibleUnifyValue for Option<V> {
}
