//! An implementation of the Congruence Closure algorithm based on the
//! paper "Fast Decision Procedures Based on Congruence Closure" by Nelson
//! and Oppen, JACM 1980.

use graph::{self, Graph, NodeIndex};
use std::collections::HashMap;
use std::fmt::Debug;
use std::hash::Hash;
use std::iter;
use unify::{UnifyKey, UnifyValue, InfallibleUnifyValue, UnificationTable, UnionedKeys};

#[cfg(test)]
mod test;

pub struct CongruenceClosure<K: Key> {
    map: HashMap<K, Token>,
    table: UnificationTable<Token>,
    graph: Graph<K, ()>,
}

pub trait Key: Hash + Eq + Clone + Debug {
    // If this Key has some efficient way of converting itself into a
    // congruence closure `Token`, then it shold return `Some(token)`.
    // Otherwise, return `None`, in which case the CC will internally
    // map the key to a token. Typically, this is used by layers that
    // wrap the CC, where inference variables are mapped directly to
    // particular tokens.
    fn to_token(&self) -> Option<Token> {
        None
    }
    fn key_kind(&self) -> KeyKind;
    fn shallow_eq(&self, key: &Self) -> bool;
    fn successors(&self) -> Vec<Self>;
}

#[derive(Copy, Clone, Debug, PartialEq, Eq)]
pub enum KeyKind {
    Applicative,
    Generative,
}
use self::KeyKind::*;

#[derive(Copy, Clone, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct Token {
    // this is the index both for the graph and the unification table,
    // since for every node there is also a slot in the unification
    // table
    index: u32,
}

impl Token {
    fn new(index: u32) -> Token {
        Token { index: index }
    }

    fn from_node(node: NodeIndex) -> Token {
        Token { index: node.0 as u32 }
    }

    fn node(&self) -> NodeIndex {
        NodeIndex(self.index as usize)
    }
}

impl UnifyKey for Token {
    type Value = KeyKind;
    fn index(&self) -> u32 {
        self.index
    }
    fn from_index(i: u32) -> Token {
        Token::new(i)
    }
    fn tag() -> &'static str {
        "CongruenceClosure"
    }
    fn order_roots(a: Self,
                   &a_value: &KeyKind,
                   b: Self,
                   &b_value: &KeyKind)
                   -> Option<(Self, Self)> {
        if a_value == b_value {
            None
        } else if a_value == Generative {
            Some((a, b))
        } else {
            debug_assert!(b_value == Generative);
            Some((b, a))
        }
    }
}

impl UnifyValue for KeyKind {
    fn unify_values(&kind1: &Self, &kind2: &Self) -> Result<Self, (Self, Self)> {
        match (kind1, kind2) {
            (Generative, _) => Ok(Generative),
            (_, Generative) => Ok(Generative),
            (Applicative, Applicative) => Ok(Applicative),
        }
    }
}

impl InfallibleUnifyValue for KeyKind {}

impl<K: Key> CongruenceClosure<K> {
    pub fn new() -> CongruenceClosure<K> {
        CongruenceClosure {
            map: HashMap::new(),
            table: UnificationTable::new(),
            graph: Graph::new(),
        }
    }

    /// Manually create a new CC token. You don't normally need to do
    /// this, as CC tokens are automatically created for each key when
    /// we first observe it. However, if you wish to have keys that
    /// make use of the `to_token` method to bypass the `key -> token`
    /// map, then you can use this function to make a new-token.  The
    /// callback `key_op` will be invoked to create the key for the
    /// fresh token (typically, it will wrap the token in some kind of
    /// enum indicating an inference variable).
    ///
    /// **WARNING:** The new key **must** be a leaf (no successor
    /// keys) or else things will not work right. This invariant is
    /// not currently checked.
    pub fn new_token<OP>(&mut self, key_kind: KeyKind, key_op: OP) -> Token
        where OP: FnOnce(Token) -> K
    {
        let token = self.table.new_key(key_kind);
        let key = key_op(token);
        let node = self.graph.add_node(key);
        assert_eq!(token.node(), node);
        token
    }

    /// Return the key for a given token
    pub fn key(&self, token: Token) -> &K {
        self.graph.node_data(token.node())
    }

    /// Indicates they `key1` and `key2` are equivalent.
    pub fn merge(&mut self, key1: K, key2: K) {
        let token1 = self.add(key1);
        let token2 = self.add(key2);
        self.algorithm().merge(token1, token2);
    }

    /// Indicates whether `key1` and `key2` are equivalent.
    pub fn merged(&mut self, key1: K, key2: K) -> bool {
        // Careful: you cannot naively remove the `add` calls
        // here. The reason is because of patterns like the test
        // `struct_union_no_add`. If we unify X and Y, and then unify
        // F(X) and F(Z), we need to be sure to figure out that F(Y)
        // == F(Z). This requires a non-trivial deduction step, so
        // just checking if the arguments are congruent will fail,
        // because `Y == Z` does not hold.

        debug!("merged: called({:?}, {:?})", key1, key2);

        let token1 = self.add(key1);
        let token2 = self.add(key2);
        self.algorithm().unioned(token1, token2)
    }

    /// Returns an iterator over all keys that are known to have been
    /// merged with `key`. This is a bit dubious, since the set of
    /// merged keys will be dependent on what has been added, and is
    /// not the full set of equivalencies that one might imagine. See the
    /// test `merged_keys` for an example.
    pub fn merged_keys(&mut self, key: K) -> MergedKeys<K> {
        let token = self.add(key);
        MergedKeys {
            graph: &self.graph,
            iterator: self.table.unioned_keys(token),
        }
    }

    /// Add a key into the CC table, returning the corresponding
    /// token. This is not part of the public API, though it could be
    /// if we wanted.
    fn add(&mut self, key: K) -> Token {
        debug!("add(): key={:?}", key);

        let (is_new, token) = self.get_or_add(&key);
        debug!("add: key={:?} is_new={:?} token={:?}", key, is_new, token);

        // if this node is already in the graph, we are done
        if !is_new {
            return token;
        }

        // Otherwise, we want to add the 'successors' also. So, for
        // example, if we are adding `Box<Foo>`, the successor would
        // be `Foo`.  So go ahead and recursively add `Foo` if it
        // doesn't already exist.
        let successors: Vec<_> = key.successors()
            .into_iter()
            .map(|s| self.add(s))
            .collect();

        debug!("add: key={:?} successors={:?}", key, successors);

        // Now we have to be a bit careful. It might be that we are
        // adding `Box<Foo>`, but `Foo` was already present, and in
        // fact equated with `Bar`. That is, maybe we had a graph like:
        //
        //      Box<Bar> -> Bar == Foo
        //
        // Now we just added `Box<Foo>`, but we need to equate
        // `Box<Foo>` and `Box<Bar>`.
        for successor in successors {
            // get set of predecessors for each successor BEFORE we add the new node;
            // this would be `Box<Bar>` in the above example.
            let predecessors: Vec<_> = self.algorithm().all_preds(successor);

            debug!("add: key={:?} successor={:?} predecessors={:?}",
                   key,
                   successor,
                   predecessors);

            // add edge from new node `Box<Foo>` to its successor `Foo`
            self.graph.add_edge(token.node(), successor.node(), ());

            // Now we have to consider merging the old predecessors,
            // like `Box<Bar>`, with this new node `Box<Foo>`.
            //
            // Note that in other cases it might be that no merge will
            // occur. For example, if we were adding `(A1, B1)` to a
            // graph like this:
            //
            //     (A, B) -> A == A1
            //        |
            //        v
            //        B
            //
            // In this case, the predecessor would be `(A, B)`; but we don't
            // know that `B == B1`, so we can't merge that with `(A1, B1)`.
            for predecessor in predecessors {
                self.algorithm().maybe_merge(token, predecessor);
            }
        }

        token
    }

    /// Gets the token for a key, if any.
    fn get(&self, key: &K) -> Option<Token> {
        key.to_token()
            .or_else(|| self.map.get(key).cloned())
    }

    /// Gets the token for a key, adding one if none exists. Returns the token
    /// and a boolean indicating whether it had to be added.
    fn get_or_add(&mut self, key: &K) -> (bool, Token) {
        if let Some(token) = self.get(key) {
            return (false, token);
        }

        let token = self.new_token(key.key_kind(), |_| key.clone());
        self.map.insert(key.clone(), token);
        (true, token)
    }

    fn algorithm(&mut self) -> Algorithm<K> {
        Algorithm {
            graph: &self.graph,
            table: &mut self.table,
        }
    }
}

// # Walking merged keys

pub struct MergedKeys<'cc, K: Key + 'cc> {
    graph: &'cc Graph<K, ()>,
    iterator: UnionedKeys<'cc, Token>,
}

impl<'cc, K: Key> Iterator for MergedKeys<'cc, K> {
    type Item = K;

    fn next(&mut self) -> Option<Self::Item> {
        self.iterator
            .next()
            .map(|token| self.graph.node_data(token.node()).clone())
    }
}

// # The core algorithm

struct Algorithm<'a, K: Key + 'a> {
    graph: &'a Graph<K, ()>,
    table: &'a mut UnificationTable<Token>,
}

impl<'a, K: Key> Algorithm<'a, K> {
    fn merge(&mut self, u: Token, v: Token) {
        debug!("merge(): u={:?} v={:?}", u, v);

        if self.unioned(u, v) {
            return;
        }

        let u_preds = self.all_preds(u);
        let v_preds = self.all_preds(v);

        self.union(u, v);

        for &p_u in &u_preds {
            for &p_v in &v_preds {
                self.maybe_merge(p_u, p_v);
            }
        }
    }

    fn all_preds(&mut self, u: Token) -> Vec<Token> {
        let graph = self.graph;
        self.table
            .unioned_keys(u)
            .flat_map(|k| graph.predecessor_nodes(k.node()))
            .map(|i| Token::from_node(i))
            .collect()
    }

    fn maybe_merge(&mut self, p_u: Token, p_v: Token) {
        debug!("maybe_merge(): p_u={:?} p_v={:?}",
               self.key(p_u),
               self.key(p_v));

        if !self.unioned(p_u, p_v) && self.shallow_eq(p_u, p_v) && self.congruent(p_u, p_v) {
            self.merge(p_u, p_v);
        }
    }

    // Check whether each of the successors are unioned. So if you
    // have `Box<X1>` and `Box<X2>`, this is true if `X1 == X2`. (The
    // result of this fn is not really meaningful unless the two nodes
    // are shallow equal here.)
    fn congruent(&mut self, p_u: Token, p_v: Token) -> bool {
        debug_assert!(self.shallow_eq(p_u, p_v));
        debug!("congruent({:?}, {:?})", self.key(p_u), self.key(p_v));
        let succs_u = self.successors(p_u);
        let succs_v = self.successors(p_v);
        let r = succs_u.zip(succs_v).all(|(s_u, s_v)| {
            debug!("congruent: s_u={:?} s_v={:?}", s_u, s_v);
            self.unioned(s_u, s_v)
        });
        debug!("congruent({:?}, {:?}) = {:?}",
               self.key(p_u),
               self.key(p_v),
               r);
        r
    }

    fn key(&self, u: Token) -> &'a K {
        self.graph.node_data(u.node())
    }

    // Compare the local data, not considering successor nodes. So e.g
    // `Box<X>` and `Box<Y>` are shallow equal for any `X` and `Y`.
    fn shallow_eq(&self, u: Token, v: Token) -> bool {
        let r = self.key(u).shallow_eq(self.key(v));
        debug!("shallow_eq({:?}, {:?}) = {:?}", self.key(u), self.key(v), r);
        r
    }

    fn token_kind(&self, u: Token) -> KeyKind {
        self.graph.node_data(u.node()).key_kind()
    }

    fn unioned(&mut self, u: Token, v: Token) -> bool {
        let r = self.table.unioned(u, v);
        debug!("unioned(u={:?}, v={:?}) = {:?}",
               self.key(u),
               self.key(v),
               r);
        r
    }

    fn union(&mut self, u: Token, v: Token) {
        debug!("union(u={:?}, v={:?})", self.key(u), self.key(v));

        // find the roots of `u` and `v`; if `u` and `v` have been unioned
        // with anything generative, these will be generative.
        let u = self.table.find(u);
        let v = self.table.find(v);

        // u and v are now union'd
        self.table.union(u, v);

        // if both `u` and `v` were generative, we can now propagate
        // the constraint that their successors must also be the same
        if self.token_kind(u) == Generative && self.token_kind(v) == Generative {
            if self.shallow_eq(u, v) {
                let mut succs_u = self.successors(u);
                let mut succs_v = self.successors(v);
                for (succ_u, succ_v) in succs_u.by_ref().zip(succs_v.by_ref()) {
                    // assume # of succ is equal because types are WF (asserted below)
                    self.merge(succ_u, succ_v);
                }
                debug_assert!(succs_u.next().is_none());
                debug_assert!(succs_v.next().is_none());
            } else {
                // error: user asked us to union i32/u32 or Vec<T>/Vec<U>;
                // for now just panic.
                panic!("inconsistent conclusion: {:?} vs {:?}",
                       self.key(u),
                       self.key(v));
            }
        }
    }

    fn successors(&self, token: Token) -> iter::Map<graph::AdjacentTargets<'a, K, ()>,
                                                    fn(NodeIndex) -> Token> {
        self.graph
            .successor_nodes(token.node())
            .map(Token::from_node)
    }

    fn predecessors(&self, token: Token) -> iter::Map<graph::AdjacentSources<'a, K, ()>,
                                                      fn(NodeIndex) -> Token> {
        self.graph
            .predecessor_nodes(token.node())
            .map(Token::from_node)
    }

    /// If `token` has been unioned with something generative, returns
    /// `Ok(u)` where `u` is the generative token. Otherwise, returns
    /// `Err(v)` where `v` is the root of `token`.
    fn normalize_to_generative(&mut self, token: Token) -> Result<Token, Token> {
        let token = self.table.find(token);
        match self.token_kind(token) {
            Generative => Ok(token),
            Applicative => Err(token),
        }
    }
}
