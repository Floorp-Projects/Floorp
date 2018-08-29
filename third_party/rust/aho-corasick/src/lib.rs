/*!
An implementation of the
[Aho-Corasick string search algorithm](https://en.wikipedia.org/wiki/Aho%E2%80%93Corasick_string_matching_algorithm).

The Aho-Corasick algorithm is principally useful when you need to search many
large texts for a fixed (possibly large) set of keywords. In particular, the
Aho-Corasick algorithm preprocesses the set of keywords by constructing a
finite state machine. The search phase is then a quick linear scan through the
text. Each character in the search text causes a state transition in the
automaton. Matches are reported when the automaton enters a match state.

# Examples

The main type exposed by this crate is `AcAutomaton`, which can be constructed
from an iterator of pattern strings:

```rust
use aho_corasick::{Automaton, AcAutomaton};

let aut = AcAutomaton::new(vec!["apple", "maple"]);

// AcAutomaton also implements `FromIterator`:
let aut: AcAutomaton<&str> = ["apple", "maple"].iter().cloned().collect();
```

Finding matches can be done with `find`:

```rust
use aho_corasick::{Automaton, AcAutomaton, Match};

let aut = AcAutomaton::new(vec!["apple", "maple"]);
let mut it = aut.find("I like maple apples.");
assert_eq!(it.next(), Some(Match {
    pati: 1,
    start: 7,
    end: 12,
}));
assert_eq!(it.next(), Some(Match {
    pati: 0,
    start: 13,
    end: 18,
}));
assert_eq!(it.next(), None);
```

Use `find_overlapping` if you want to report all matches, even if they
overlap with each other.

```rust
use aho_corasick::{Automaton, AcAutomaton, Match};

let aut = AcAutomaton::new(vec!["abc", "a"]);
let matches: Vec<_> = aut.find_overlapping("abc").collect();
assert_eq!(matches, vec![
    Match { pati: 1, start: 0, end: 1}, Match { pati: 0, start: 0, end: 3 },
]);

// Regular `find` will report only one match:
let matches: Vec<_> = aut.find("abc").collect();
assert_eq!(matches, vec![Match { pati: 1, start: 0, end: 1}]);
```

Finally, there are also methods for finding matches on *streams*. Namely, the
search text does not have to live in memory. It's useful to run this on files
that can't fit into memory:

```no_run
use std::fs::File;

use aho_corasick::{Automaton, AcAutomaton};

let aut = AcAutomaton::new(vec!["foo", "bar", "baz"]);
let rdr = File::open("search.txt").unwrap();
for m in aut.stream_find(rdr) {
    let m = m.unwrap(); // could be an IO error
    println!("Pattern '{}' matched at: ({}, {})",
             aut.pattern(m.pati), m.start, m.end);
}
```

There is also `stream_find_overlapping`, which is just like `find_overlapping`,
but it operates on streams.

Please see `dict-search.rs` in this crate's `examples` directory for a more
complete example. It creates a large automaton from a dictionary and can do a
streaming match over arbitrarily large data.

# Memory usage

A key aspect of an Aho-Corasick implementation is how the state transitions
are represented. The easiest way to make the automaton fast is to store a
sparse 256-slot map in each state. It maps an input byte to a state index.
This makes the matching loop extremely fast, since it translates to a simple
pointer read.

The problem is that as the automaton accumulates more states, you end up paying
a `256 * 4` (`4` is for the `u32` state index) byte penalty for every state
regardless of how many transitions it has.

To solve this, only states near the root of the automaton have this sparse
map representation. States near the leaves of the automaton use a dense mapping
that requires a linear scan.

(The specific limit currently set is `3`, so that states with a depth less than
or equal to `3` are less memory efficient. The result is that the memory usage
of the automaton stops growing rapidly past ~60MB, even for automatons with
thousands of patterns.)

If you'd like to opt for the less-memory-efficient-but-faster version, then
you can construct an `AcAutomaton` with a `Sparse` transition strategy:

```rust
use aho_corasick::{Automaton, AcAutomaton, Match, Sparse};

let aut = AcAutomaton::<&str, Sparse>::with_transitions(vec!["abc", "a"]);
let matches: Vec<_> = aut.find("abc").collect();
assert_eq!(matches, vec![Match { pati: 1, start: 0, end: 1}]);
```
*/

#![deny(missing_docs)]

extern crate memchr;
#[cfg(test)]
extern crate quickcheck;
#[cfg(test)]
extern crate rand;

use std::collections::VecDeque;
use std::fmt;
use std::iter::FromIterator;
use std::mem;

pub use self::autiter::{
    Automaton, Match,
    Matches, MatchesOverlapping, StreamMatches, StreamMatchesOverlapping,
};
pub use self::full::FullAcAutomaton;

// We're specifying paths explicitly so that we can use
// these modules simultaneously from `main.rs`.
// Should probably make just make `main.rs` a separate crate.
#[path = "autiter.rs"]
mod autiter;
#[path = "full.rs"]
mod full;

/// The integer type used for the state index.
///
/// Limiting this to 32 bit integers can have a big impact on memory usage
/// when using the `Sparse` transition representation.
pub type StateIdx = u32;

// Constants for special state indexes.
const FAIL_STATE: u32 = 0;
const ROOT_STATE: u32 = 1;

// Limit the depth at which we use a sparse alphabet map. Once the limit is
// reached, a dense set is used (and lookup becomes O(n)).
//
// This does have a performance hit, but the (straight forward) alternative
// is to have a `256 * 4` byte overhead for every state.
// Given that Aho-Corasick is typically used for dictionary searching, this
// can lead to dramatic memory bloat.
//
// This limit should only be increased at your peril. Namely, in the worst
// case, `256^DENSE_DEPTH_THRESHOLD * 4` corresponds to the memory usage in
// bytes. A value of `1` gives us a good balance. This is also a happy point
// in the benchmarks. A value of `0` gives considerably worse times on certain
// benchmarks (e.g., `ac_ten_one_prefix_byte_every_match`) than even a value
// of `1`. A value of `2` is slightly better than `1` and it looks like gains
// level off at that point with not much observable difference when set to
// `3`.
//
// Why not make this user configurable? Well, it doesn't make much sense
// because we pay for it with case analysis in the matching loop. Increasing it
// doesn't have much impact on performance (outside of pathological cases?).
//
// N.B. Someone else seems to have discovered an alternative, but I haven't
// grokked it yet: https://github.com/mischasan/aho-corasick
const DENSE_DEPTH_THRESHOLD: u32 = 1;

/// An Aho-Corasick finite automaton.
///
/// The type parameter `P` is the type of the pattern that was used to
/// construct this AcAutomaton.
#[derive(Clone)]
pub struct AcAutomaton<P, T=Dense> {
    pats: Vec<P>,
    states: Vec<State<T>>,
    start_bytes: Vec<u8>,
}

#[derive(Clone)]
struct State<T> {
    out: Vec<usize>,
    fail: StateIdx,
    goto: T,
    depth: u32,
}

impl<P: AsRef<[u8]>> AcAutomaton<P> {
    /// Create a new automaton from an iterator of patterns.
    ///
    /// The patterns must be convertible to bytes (`&[u8]`) via the `AsRef`
    /// trait.
    pub fn new<I>(pats: I) -> AcAutomaton<P, Dense>
            where I: IntoIterator<Item=P> {
        AcAutomaton::with_transitions(pats)
    }
}

impl<P: AsRef<[u8]>, T: Transitions> AcAutomaton<P, T> {
    /// Create a new automaton from an iterator of patterns.
    ///
    /// This constructor allows one to choose the transition representation.
    ///
    /// The patterns must be convertible to bytes (`&[u8]`) via the `AsRef`
    /// trait.
    pub fn with_transitions<I>(pats: I) -> AcAutomaton<P, T>
            where I: IntoIterator<Item=P> {
        AcAutomaton {
            pats: vec![], // filled in later, avoid wrath of borrow checker
            states: vec![State::new(0), State::new(0)], // empty and root
            start_bytes: vec![], // also filled in later
        }.build(pats.into_iter().collect())
    }

    /// Build out the entire automaton into a single matrix.
    ///
    /// This will make searching as fast as possible at the expense of using
    /// at least `4 * 256 * #states` bytes of memory.
    pub fn into_full(self) -> FullAcAutomaton<P> {
        FullAcAutomaton::new(self)
    }

    #[doc(hidden)]
    pub fn num_states(&self) -> usize {
        self.states.len()
    }

    #[doc(hidden)]
    pub fn heap_bytes(&self) -> usize {
        self.pats.iter()
            .map(|p| mem::size_of::<P>() + p.as_ref().len())
            .sum::<usize>()
        + self.states.iter()
              .map(|s| mem::size_of::<State<T>>() + s.heap_bytes())
              .sum::<usize>()
        + self.start_bytes.len()
    }

    // The states of `full_automaton` should be set for all states < si
    fn memoized_next_state(
        &self,
        full_automaton: &FullAcAutomaton<P>,
        current_si: StateIdx,
        mut si: StateIdx,
        b: u8,
    ) -> StateIdx {
        loop {
            if si < current_si {
                return full_automaton.next_state(si, b);
            }
            let state = &self.states[si as usize];
            let maybe_si = state.goto(b);
            if maybe_si != FAIL_STATE {
                return maybe_si;
            } else {
                si = state.fail;
            }
        }
    }
}

impl<P: AsRef<[u8]>, T: Transitions> Automaton<P> for AcAutomaton<P, T> {
    #[inline]
    fn next_state(&self, mut si: StateIdx, b: u8) -> StateIdx {
        loop {
            let state = &self.states[si as usize];
            let maybe_si = state.goto(b);
            if maybe_si != FAIL_STATE {
                si = maybe_si;
                break;
            } else {
                si = state.fail;
            }
        }
        si
    }

    #[inline]
    fn get_match(&self, si: StateIdx, outi: usize, texti: usize) -> Match {
        let pati = self.states[si as usize].out[outi];
        let patlen = self.pats[pati].as_ref().len();
        let start = texti + 1 - patlen;
        Match {
            pati: pati,
            start: start,
            end: start + patlen,
        }
    }

    #[inline]
    fn has_match(&self, si: StateIdx, outi: usize) -> bool {
        outi < self.states[si as usize].out.len()
    }

    #[inline]
    fn start_bytes(&self) -> &[u8] {
        &self.start_bytes
    }

    #[inline]
    fn patterns(&self) -> &[P] {
        &self.pats
    }

    #[inline]
    fn pattern(&self, i: usize) -> &P {
        &self.pats[i]
    }
}

// `(0..256).map(|b| b as u8)` optimizes poorly in debug builds so
// we use this small explicit iterator instead
struct AllBytesIter(i32);
impl Iterator for AllBytesIter {
    type Item = u8;
    #[inline]
    fn next(&mut self) -> Option<Self::Item> {
        if self.0 < 256 {
            let b = self.0 as u8;
            self.0 += 1;
            Some(b)
        } else {
            None
        }
    }
}

impl AllBytesIter {
    fn new() -> AllBytesIter {
        AllBytesIter(0)
    }
}

// Below contains code for *building* the automaton. It's a reasonably faithful
// translation of the description/psuedo-code from:
// http://www.cs.uku.fi/~kilpelai/BSA05/lectures/slides04.pdf

impl<P: AsRef<[u8]>, T: Transitions> AcAutomaton<P, T> {
    // This is the first phase and builds the initial keyword tree.
    fn build(mut self, pats: Vec<P>) -> AcAutomaton<P, T> {
        for (pati, pat) in pats.iter().enumerate() {
            if pat.as_ref().is_empty() {
                continue;
            }
            let mut previ = ROOT_STATE;
            for &b in pat.as_ref() {
                if self.states[previ as usize].goto(b) != FAIL_STATE {
                    previ = self.states[previ as usize].goto(b);
                } else {
                    let depth = self.states[previ as usize].depth + 1;
                    let nexti = self.add_state(State::new(depth));
                    self.states[previ as usize].set_goto(b, nexti);
                    previ = nexti;
                }
            }
            self.states[previ as usize].out.push(pati);
        }
        {
            let root_state = &mut self.states[ROOT_STATE as usize];
            for c in AllBytesIter::new() {
                if root_state.goto(c) == FAIL_STATE {
                    root_state.set_goto(c, ROOT_STATE);
                } else {
                    self.start_bytes.push(c);
                }
            }
        }
        // If any of the start bytes are non-ASCII, then remove them all,
        // because we don't want to be calling memchr on non-ASCII bytes.
        // (Well, we could, but it requires being more clever. Simply using
        // the prefix byte isn't good enough.)
        if self.start_bytes.iter().any(|&b| b > 0x7F) {
            self.start_bytes.clear();
        }
        self.pats = pats;
        self.fill()
    }

    // The second phase that fills in the back links.
    fn fill(mut self) -> AcAutomaton<P, T> {
        // Fill up the queue with all non-root transitions out of the root
        // node. Then proceed by breadth first traversal.
        let mut q = VecDeque::new();
        self.states[ROOT_STATE as usize].for_each_transition(|_, si| {
            if si != ROOT_STATE {
                q.push_front(si);
            }
        });

        let mut transitions = Vec::new();

        while let Some(si) = q.pop_back() {
            self.states[si as usize].for_each_ok_transition(|c, u| {
                transitions.push((c, u));
                q.push_front(u);
            });

            for (c, u) in transitions.drain(..) {
                let mut v = self.states[si as usize].fail;
                loop {
                    let state = &self.states[v as usize];
                    if state.goto(c) == FAIL_STATE {
                        v = state.fail;
                    } else {
                        break;
                    }
                }
                let ufail = self.states[v as usize].goto(c);
                self.states[u as usize].fail = ufail;

                fn get_two<T>(xs: &mut [T], i: usize, j: usize) -> (&mut T, &mut T) {
                    if i < j {
                        let (before, after) = xs.split_at_mut(j);
                        (&mut before[i], &mut after[0])
                    } else {
                        let (before, after) = xs.split_at_mut(i);
                        (&mut after[0], &mut before[j])
                    }
                }

                let (ufail_out, out) = get_two(&mut self.states, ufail as usize, u as usize);
                out.out.extend_from_slice(&ufail_out.out);
            }
        }
        self
    }

    fn add_state(&mut self, state: State<T>) -> StateIdx {
        let i = self.states.len();
        self.states.push(state);
        i as StateIdx
    }
}

impl<T: Transitions> State<T> {
    fn new(depth: u32) -> State<T> {
        State {
            out: vec![],
            fail: 1,
            goto: Transitions::new(depth),
            depth: depth,
        }
    }

    fn goto(&self, b: u8) -> StateIdx {
        self.goto.goto(b)
    }

    fn set_goto(&mut self, b: u8, si: StateIdx) {
        self.goto.set_goto(b, si);
    }

    fn heap_bytes(&self) -> usize {
        (self.out.len() * usize_bytes())
        + self.goto.heap_bytes()
    }

    fn for_each_transition<F>(&self, f: F)
        where F: FnMut(u8, StateIdx)
    {
        self.goto.for_each_transition(f)
    }

    fn for_each_ok_transition<F>(&self, f: F)
        where F: FnMut(u8, StateIdx)
    {
        self.goto.for_each_ok_transition(f)
    }
}

/// An abstraction over state transition strategies.
///
/// This is an attempt to let the caller choose the space/time trade offs
/// used for state transitions.
///
/// (It's possible that this interface is merely good enough for just the two
/// implementations in this crate.)
pub trait Transitions {
    /// Return a new state at the given depth.
    fn new(depth: u32) -> Self;
    /// Return the next state index given the next character.
    fn goto(&self, alpha: u8) -> StateIdx;
    /// Set the next state index for the character given.
    fn set_goto(&mut self, alpha: u8, si: StateIdx);
    /// The memory use in bytes (on the heap) of this set of transitions.
    fn heap_bytes(&self) -> usize;

    /// Iterates over each state
    fn for_each_transition<F>(&self, mut f: F)
        where F: FnMut(u8, StateIdx)
    {
        for b in AllBytesIter::new() {
            f(b, self.goto(b));
        }
    }

    /// Iterates over each non-fail state
    fn for_each_ok_transition<F>(&self, mut f: F)
    where
        F: FnMut(u8, StateIdx),
    {
        self.for_each_transition(|b, si| {
            if si != FAIL_STATE {
                f(b, si);
            }
        });
    }
}

/// State transitions that can be stored either sparsely or densely.
///
/// This uses less space but at the expense of slower matching.
#[derive(Clone, Debug)]
pub struct Dense(DenseChoice);

#[derive(Clone, Debug)]
enum DenseChoice {
    Sparse(Box<Sparse>),
    Dense(Vec<(u8, StateIdx)>),
}

impl Transitions for Dense {
    fn new(depth: u32) -> Dense {
        if depth <= DENSE_DEPTH_THRESHOLD {
            Dense(DenseChoice::Sparse(Box::new(Sparse::new(depth))))
        } else {
            Dense(DenseChoice::Dense(vec![]))
        }
    }

    fn goto(&self, b1: u8) -> StateIdx {
        match self.0 {
            DenseChoice::Sparse(ref m) => m.goto(b1),
            DenseChoice::Dense(ref m) => {
                for &(b2, si) in m {
                    if b1 == b2 {
                        return si;
                    }
                }
                FAIL_STATE
            }
        }
    }

    fn set_goto(&mut self, b: u8, si: StateIdx) {
        match self.0 {
            DenseChoice::Sparse(ref mut m) => m.set_goto(b, si),
            DenseChoice::Dense(ref mut m) => m.push((b, si)),
        }
    }

    fn heap_bytes(&self) -> usize {
        match self.0 {
            DenseChoice::Sparse(_) => mem::size_of::<Sparse>(),
            DenseChoice::Dense(ref m) => m.len() * (1 + 4),
        }
    }

    fn for_each_transition<F>(&self, mut f: F)
        where F: FnMut(u8, StateIdx)
    {
        match self.0 {
            DenseChoice::Sparse(ref m) => m.for_each_transition(f),
            DenseChoice::Dense(ref m) => {
                let mut iter = m.iter();
                let mut b = 0i32;
                while let Some(&(next_b, next_si)) = iter.next() {
                    while (b as u8) < next_b {
                        f(b as u8, FAIL_STATE);
                        b += 1;
                    }
                    f(b as u8, next_si);
                    b += 1;
                }
                while b < 256 {
                    f(b as u8, FAIL_STATE);
                    b += 1;
                }
            }
        }
    }
    fn for_each_ok_transition<F>(&self, mut f: F)
    where
        F: FnMut(u8, StateIdx),
    {
        match self.0 {
            DenseChoice::Sparse(ref m) => m.for_each_ok_transition(f),
            DenseChoice::Dense(ref m) => for &(b, si) in m {
                f(b, si)
            }
        }
    }
}

/// State transitions that are always sparse.
///
/// This can use enormous amounts of memory when there are many patterns,
/// but matching is very fast.
pub struct Sparse([StateIdx; 256]);

impl Clone for Sparse {
    fn clone(&self) -> Sparse {
        Sparse(self.0)
    }
}

impl fmt::Debug for Sparse {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.debug_tuple("Sparse").field(&&self.0[..]).finish()
    }
}

impl Transitions for Sparse {
    fn new(_: u32) -> Sparse {
        Sparse([0; 256])
    }

    #[inline]
    fn goto(&self, b: u8) -> StateIdx {
        self.0[b as usize]
    }

    fn set_goto(&mut self, b: u8, si: StateIdx) {
        self.0[b as usize] = si;
    }

    fn heap_bytes(&self) -> usize {
        0
    }
}

impl<S: AsRef<[u8]>> FromIterator<S> for AcAutomaton<S> {
    /// Create an automaton from an iterator of strings.
    fn from_iter<T>(it: T) -> AcAutomaton<S> where T: IntoIterator<Item=S> {
        AcAutomaton::new(it)
    }
}

// Provide some question debug impls for viewing automatons.
// The custom impls mostly exist for special showing of sparse maps.

impl<P: AsRef<[u8]> + fmt::Debug, T: Transitions>
        fmt::Debug for AcAutomaton<P, T> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        use std::iter::repeat;

        try!(writeln!(f, "{}", repeat('-').take(79).collect::<String>()));
        try!(writeln!(f, "Patterns: {:?}", self.pats));
        for (i, state) in self.states.iter().enumerate().skip(1) {
            try!(writeln!(f, "{:3}: {}", i, state.debug(i == 1)));
        }
        write!(f, "{}", repeat('-').take(79).collect::<String>())
    }
}

impl<T: Transitions> State<T> {
    fn debug(&self, root: bool) -> String {
        format!("State {{ depth: {:?}, out: {:?}, fail: {:?}, goto: {{{}}} }}",
                self.depth, self.out, self.fail, self.goto_string(root))
    }

    fn goto_string(&self, root: bool) -> String {
        let mut goto = vec![];
        for b in AllBytesIter::new() {
            let si = self.goto(b);
            if (!root && si == FAIL_STATE) || (root && si == ROOT_STATE) {
                continue;
            }
            goto.push(format!("{} => {}", b as char, si));
        }
        goto.join(", ")
    }
}

impl<T: Transitions> fmt::Debug for State<T> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "{}", self.debug(false))
    }
}

impl<T: Transitions> AcAutomaton<String, T> {
    #[doc(hidden)]
    pub fn dot(&self) -> String {
        use std::fmt::Write;
        let mut out = String::new();
        macro_rules! w {
            ($w:expr, $($tt:tt)*) => { {write!($w, $($tt)*)}.unwrap() }
        }

        w!(out, r#"
digraph automaton {{
    label=<<FONT POINT-SIZE="20">{}</FONT>>;
    labelloc="l";
    labeljust="l";
    rankdir="LR";
"#, self.pats.join(", "));
        for (i, s) in self.states.iter().enumerate().skip(1) {
            let i = i as u32;
            if s.out.is_empty() {
                w!(out, "    {};\n", i);
            } else {
                w!(out, "    {} [peripheries=2];\n", i);
            }
            w!(out, "    {} -> {} [style=dashed];\n", i, s.fail);
            for b in AllBytesIter::new() {
                let si = s.goto(b);
                if si == FAIL_STATE || (i == ROOT_STATE && si == ROOT_STATE) {
                    continue;
                }
                w!(out, "    {} -> {} [label={}];\n", i, si, b as char);
            }
        }
        w!(out, "}}");
        out
    }
}

fn vec_bytes() -> usize {
    usize_bytes() * 3
}

fn usize_bytes() -> usize {
    let bits = usize::max_value().count_ones() as usize;
    bits / 8
}

#[cfg(test)]
mod tests {
    use std::collections::HashSet;
    use std::io;

    use quickcheck::{Arbitrary, Gen, quickcheck};
    use rand::Rng;

    use super::{AcAutomaton, Automaton, Match, AllBytesIter};

    fn aut_find<S>(xs: &[S], haystack: &str) -> Vec<Match>
            where S: Clone + AsRef<[u8]> {
        AcAutomaton::new(xs.to_vec()).find(&haystack).collect()
    }

    fn aut_finds<S>(xs: &[S], haystack: &str) -> Vec<Match>
            where S: Clone + AsRef<[u8]> {
        let cur = io::Cursor::new(haystack.as_bytes());
        AcAutomaton::new(xs.to_vec())
            .stream_find(cur).map(|r| r.unwrap()).collect()
    }

    fn aut_findf<S>(xs: &[S], haystack: &str) -> Vec<Match>
            where S: Clone + AsRef<[u8]> {
        AcAutomaton::new(xs.to_vec()).into_full().find(haystack).collect()
    }

    fn aut_findfs<S>(xs: &[S], haystack: &str) -> Vec<Match>
            where S: Clone + AsRef<[u8]> {
        let cur = io::Cursor::new(haystack.as_bytes());
        AcAutomaton::new(xs.to_vec())
            .into_full()
            .stream_find(cur).map(|r| r.unwrap()).collect()
    }

    fn aut_findo<S>(xs: &[S], haystack: &str) -> Vec<Match>
            where S: Clone + AsRef<[u8]> {
        AcAutomaton::new(xs.to_vec()).find_overlapping(haystack).collect()
    }

    fn aut_findos<S>(xs: &[S], haystack: &str) -> Vec<Match>
            where S: Clone + AsRef<[u8]> {
        let cur = io::Cursor::new(haystack.as_bytes());
        AcAutomaton::new(xs.to_vec())
            .stream_find_overlapping(cur).map(|r| r.unwrap()).collect()
    }

    fn aut_findfo<S>(xs: &[S], haystack: &str) -> Vec<Match>
            where S: Clone + AsRef<[u8]> {
        AcAutomaton::new(xs.to_vec())
            .into_full().find_overlapping(haystack).collect()
    }

    fn aut_findfos<S>(xs: &[S], haystack: &str) -> Vec<Match>
            where S: Clone + AsRef<[u8]> {
        let cur = io::Cursor::new(haystack.as_bytes());
        AcAutomaton::new(xs.to_vec())
            .into_full()
            .stream_find_overlapping(cur).map(|r| r.unwrap()).collect()
    }

    #[test]
    fn one_pattern_one_match() {
        let ns = vec!["a"];
        let hay = "za";
        let matches = vec![
            Match { pati: 0, start: 1, end: 2 },
        ];
        assert_eq!(&aut_find(&ns, hay), &matches);
        assert_eq!(&aut_finds(&ns, hay), &matches);
        assert_eq!(&aut_findf(&ns, hay), &matches);
        assert_eq!(&aut_findfs(&ns, hay), &matches);
    }

    #[test]
    fn one_pattern_many_match() {
        let ns = vec!["a"];
        let hay = "zazazzzza";
        let matches = vec![
            Match { pati: 0, start: 1, end: 2 },
            Match { pati: 0, start: 3, end: 4 },
            Match { pati: 0, start: 8, end: 9 },
        ];
        assert_eq!(&aut_find(&ns, hay), &matches);
        assert_eq!(&aut_finds(&ns, hay), &matches);
        assert_eq!(&aut_findf(&ns, hay), &matches);
        assert_eq!(&aut_findfs(&ns, hay), &matches);
    }

    #[test]
    fn one_longer_pattern_one_match() {
        let ns = vec!["abc"];
        let hay = "zazabcz";
        let matches = vec![ Match { pati: 0, start: 3, end: 6 } ];
        assert_eq!(&aut_find(&ns, hay), &matches);
        assert_eq!(&aut_finds(&ns, hay), &matches);
        assert_eq!(&aut_findf(&ns, hay), &matches);
        assert_eq!(&aut_findfs(&ns, hay), &matches);
    }

    #[test]
    fn one_longer_pattern_many_match() {
        let ns = vec!["abc"];
        let hay = "zazabczzzzazzzabc";
        let matches = vec![
            Match { pati: 0, start: 3, end: 6 },
            Match { pati: 0, start: 14, end: 17 },
        ];
        assert_eq!(&aut_find(&ns, hay), &matches);
        assert_eq!(&aut_finds(&ns, hay), &matches);
        assert_eq!(&aut_findf(&ns, hay), &matches);
        assert_eq!(&aut_findfs(&ns, hay), &matches);
    }

    #[test]
    fn many_pattern_one_match() {
        let ns = vec!["a", "b"];
        let hay = "zb";
        let matches = vec![ Match { pati: 1, start: 1, end: 2 } ];
        assert_eq!(&aut_find(&ns, hay), &matches);
        assert_eq!(&aut_finds(&ns, hay), &matches);
        assert_eq!(&aut_findf(&ns, hay), &matches);
        assert_eq!(&aut_findfs(&ns, hay), &matches);
    }

    #[test]
    fn many_pattern_many_match() {
        let ns = vec!["a", "b"];
        let hay = "zbzazzzzb";
        let matches = vec![
            Match { pati: 1, start: 1, end: 2 },
            Match { pati: 0, start: 3, end: 4 },
            Match { pati: 1, start: 8, end: 9 },
        ];
        assert_eq!(&aut_find(&ns, hay), &matches);
        assert_eq!(&aut_finds(&ns, hay), &matches);
        assert_eq!(&aut_findf(&ns, hay), &matches);
        assert_eq!(&aut_findfs(&ns, hay), &matches);
    }

    #[test]
    fn many_longer_pattern_one_match() {
        let ns = vec!["abc", "xyz"];
        let hay = "zazxyzz";
        let matches = vec![ Match { pati: 1, start: 3, end: 6 } ];
        assert_eq!(&aut_find(&ns, hay), &matches);
        assert_eq!(&aut_finds(&ns, hay), &matches);
        assert_eq!(&aut_findf(&ns, hay), &matches);
        assert_eq!(&aut_findfs(&ns, hay), &matches);
    }

    #[test]
    fn many_longer_pattern_many_match() {
        let ns = vec!["abc", "xyz"];
        let hay = "zazxyzzzzzazzzabcxyz";
        let matches = vec![
            Match { pati: 1, start: 3, end: 6 },
            Match { pati: 0, start: 14, end: 17 },
            Match { pati: 1, start: 17, end: 20 },
        ];
        assert_eq!(&aut_find(&ns, hay), &matches);
        assert_eq!(&aut_finds(&ns, hay), &matches);
        assert_eq!(&aut_findf(&ns, hay), &matches);
        assert_eq!(&aut_findfs(&ns, hay), &matches);
    }

    #[test]
    fn many_longer_pattern_overlap_one_match() {
        let ns = vec!["abc", "bc"];
        let hay = "zazabcz";
        let matches = vec![
            Match { pati: 0, start: 3, end: 6 },
            Match { pati: 1, start: 4, end: 6 },
        ];
        assert_eq!(&aut_findo(&ns, hay), &matches);
        assert_eq!(&aut_findos(&ns, hay), &matches);
        assert_eq!(&aut_findfo(&ns, hay), &matches);
        assert_eq!(&aut_findfos(&ns, hay), &matches);
    }

    #[test]
    fn many_longer_pattern_overlap_one_match_reverse() {
        let ns = vec!["abc", "bc"];
        let hay = "xbc";
        let matches = vec![ Match { pati: 1, start: 1, end: 3 } ];
        assert_eq!(&aut_findo(&ns, hay), &matches);
        assert_eq!(&aut_findos(&ns, hay), &matches);
        assert_eq!(&aut_findfo(&ns, hay), &matches);
        assert_eq!(&aut_findfos(&ns, hay), &matches);
    }

    #[test]
    fn many_longer_pattern_overlap_many_match() {
        let ns = vec!["abc", "bc", "c"];
        let hay = "zzzabczzzbczzzc";
        let matches = vec![
            Match { pati: 0, start: 3, end: 6 },
            Match { pati: 1, start: 4, end: 6 },
            Match { pati: 2, start: 5, end: 6 },
            Match { pati: 1, start: 9, end: 11 },
            Match { pati: 2, start: 10, end: 11 },
            Match { pati: 2, start: 14, end: 15 },
        ];
        assert_eq!(&aut_findo(&ns, hay), &matches);
        assert_eq!(&aut_findos(&ns, hay), &matches);
        assert_eq!(&aut_findfo(&ns, hay), &matches);
        assert_eq!(&aut_findfos(&ns, hay), &matches);
    }

    #[test]
    fn many_longer_pattern_overlap_many_match_reverse() {
        let ns = vec!["abc", "bc", "c"];
        let hay = "zzzczzzbczzzabc";
        let matches = vec![
            Match { pati: 2, start: 3, end: 4 },
            Match { pati: 1, start: 7, end: 9 },
            Match { pati: 2, start: 8, end: 9 },
            Match { pati: 0, start: 12, end: 15 },
            Match { pati: 1, start: 13, end: 15 },
            Match { pati: 2, start: 14, end: 15 },
        ];
        assert_eq!(&aut_findo(&ns, hay), &matches);
        assert_eq!(&aut_findos(&ns, hay), &matches);
        assert_eq!(&aut_findfo(&ns, hay), &matches);
        assert_eq!(&aut_findfos(&ns, hay), &matches);
    }

    #[test]
    fn pattern_returns_original_type() {
        let aut = AcAutomaton::new(vec!["apple", "maple"]);

        // Explicitly given this type to assert that the thing returned
        // from the function is our original type.
        let pat: &str = aut.pattern(0);
        assert_eq!(pat, "apple");

        // Also check the return type of the `patterns` function.
        let pats: &[&str] = aut.patterns();
        assert_eq!(pats, &["apple", "maple"]);
    }

    // Quickcheck time.

    // This generates very small ascii strings, which makes them more likely
    // to interact in interesting ways with larger haystack strings.
    #[derive(Clone, Debug, PartialEq, Eq, PartialOrd, Ord)]
    pub struct SmallAscii(String);

    impl Arbitrary for SmallAscii {
        fn arbitrary<G: Gen>(g: &mut G) -> SmallAscii {
            use std::char::from_u32;
            SmallAscii((0..2)
                       .map(|_| from_u32(g.gen_range(97, 123)).unwrap())
                       .collect())
        }

        fn shrink(&self) -> Box<Iterator<Item=SmallAscii>> {
            Box::new(self.0.shrink().map(SmallAscii))
        }
    }

    impl From<SmallAscii> for String {
        fn from(s: SmallAscii) -> String { s.0 }
    }

    impl AsRef<[u8]> for SmallAscii {
        fn as_ref(&self) -> &[u8] { self.0.as_ref() }
    }

    // This is the same arbitrary impl as `String`, except it has a bias toward
    // ASCII characters.
    #[derive(Clone, Debug, PartialEq, Eq, PartialOrd, Ord)]
    pub struct BiasAscii(String);

    impl Arbitrary for BiasAscii {
        fn arbitrary<G: Gen>(g: &mut G) -> BiasAscii {
            use std::char::from_u32;
            let size = { let s = g.size(); g.gen_range(0, s) };
            let mut s = String::with_capacity(size);
            for _ in 0..size {
                if g.gen_bool(0.3) {
                    s.push(char::arbitrary(g));
                } else {
                    for _ in 0..5 {
                        s.push(from_u32(g.gen_range(97, 123)).unwrap());
                    }
                }
            }
            BiasAscii(s)
        }

        fn shrink(&self) -> Box<Iterator<Item=BiasAscii>> {
            Box::new(self.0.shrink().map(BiasAscii))
        }
    }

    fn naive_find<S>(xs: &[S], haystack: &str) -> Vec<Match>
            where S: Clone + Into<String> {
        let needles: Vec<String> =
            xs.to_vec().into_iter().map(Into::into).collect();
        let mut matches = vec![];
        for hi in 0..haystack.len() {
            for (pati, needle) in needles.iter().enumerate() {
                let needle = needle.as_bytes();
                if needle.len() == 0 || needle.len() > haystack.len() - hi {
                    continue;
                }
                if needle == &haystack.as_bytes()[hi..hi+needle.len()] {
                    matches.push(Match {
                        pati: pati,
                        start: hi,
                        end: hi + needle.len(),
                    });
                }
            }
        }
        matches
    }

    #[test]
    fn qc_ac_equals_naive() {
        fn prop(needles: Vec<SmallAscii>, haystack: BiasAscii) -> bool {
            let aut_matches = aut_findo(&needles, &haystack.0);
            let naive_matches = naive_find(&needles, &haystack.0);
            // Ordering isn't always the same. I don't think we care, so do
            // an unordered comparison.
            let aset: HashSet<Match> = aut_matches.iter().cloned().collect();
            let nset: HashSet<Match> = naive_matches.iter().cloned().collect();
            aset == nset
        }
        quickcheck(prop as fn(Vec<SmallAscii>, BiasAscii) -> bool);
    }


    #[test]
    fn all_bytes_iter() {
        let all_bytes = AllBytesIter::new().collect::<Vec<_>>();
        assert_eq!(all_bytes[0], 0);
        assert_eq!(all_bytes[255], 255);
        assert!(AllBytesIter::new().enumerate().all(|(i, b)| b as usize == i));
    }
}
