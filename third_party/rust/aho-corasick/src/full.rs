use std::fmt;
use std::mem;

use super::{
    FAIL_STATE,
    StateIdx, AcAutomaton, Transitions, Match,
    usize_bytes, vec_bytes,
};
use super::autiter::Automaton;

/// A complete Aho-Corasick automaton.
///
/// This uses a single transition matrix that permits each input character
/// to move to the next state with a single lookup in the matrix.
///
/// This is as fast as it gets, but it is guaranteed to use a lot of memory.
/// Namely, it will use at least `4 * 256 * #states`, where the number of
/// states is capped at length of all patterns concatenated.
#[derive(Clone)]
pub struct FullAcAutomaton<P> {
    pats: Vec<P>,
    trans: Vec<StateIdx>,  // row-major, where states are rows
    out: Vec<Vec<usize>>, // indexed by StateIdx
    start_bytes: Vec<u8>,
}

impl<P: AsRef<[u8]>> FullAcAutomaton<P> {
    /// Build a new expanded Aho-Corasick automaton from an existing
    /// Aho-Corasick automaton.
    pub fn new<T: Transitions>(ac: AcAutomaton<P, T>) -> FullAcAutomaton<P> {
        let mut fac = FullAcAutomaton {
            pats: vec![],
            trans: vec![FAIL_STATE; 256 * ac.states.len()],
            out: vec![vec![]; ac.states.len()],
            start_bytes: vec![],
        };
        fac.build_matrix(&ac);
        fac.pats = ac.pats;
        fac.start_bytes = ac.start_bytes;
        fac
    }

    #[doc(hidden)]
    pub fn memory_usage(&self) -> usize {
        self.pats.iter()
            .map(|p| vec_bytes() + p.as_ref().len())
            .sum::<usize>()
        + (4 * self.trans.len())
        + self.out.iter()
              .map(|v| vec_bytes() + (usize_bytes() * v.len()))
              .sum::<usize>()
        + self.start_bytes.len()
    }

    #[doc(hidden)]
    pub fn heap_bytes(&self) -> usize {
        self.pats.iter()
            .map(|p| mem::size_of::<P>() + p.as_ref().len())
            .sum::<usize>()
        + (4 * self.trans.len())
        + self.out.iter()
              .map(|v| vec_bytes() + (usize_bytes() * v.len()))
              .sum::<usize>()
        + self.start_bytes.len()
    }

    fn set(&mut self, si: StateIdx, i: u8, goto: StateIdx) {
        let ns = self.num_states();
        self.trans[i as usize * ns + si as usize] = goto;
    }

    #[doc(hidden)]
    #[inline]
    pub fn num_states(&self) -> usize {
        self.out.len()
    }
}

impl<P: AsRef<[u8]>> Automaton<P> for FullAcAutomaton<P> {
    #[inline]
    fn next_state(&self, si: StateIdx, i: u8) -> StateIdx {
        let at = i as usize * self.num_states() + si as usize;
        unsafe { *self.trans.get_unchecked(at) }
    }

    #[inline]
    fn get_match(&self, si: StateIdx, outi: usize, texti: usize) -> Match {
        let pati = self.out[si as usize][outi];
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
        unsafe { outi < self.out.get_unchecked(si as usize).len() }
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

impl<P: AsRef<[u8]>> FullAcAutomaton<P> {
    fn build_matrix<T: Transitions>(&mut self, ac: &AcAutomaton<P, T>) {
        for (si, s) in ac.states.iter().enumerate().skip(1) {
            self.set_states(ac, si as StateIdx);
            self.out[si].extend_from_slice(&s.out);
        }
    }

    fn set_states<T: Transitions>(&mut self, ac: &AcAutomaton<P, T>, si: StateIdx) {
        let current_state = &ac.states[si as usize];
        let first_fail_state = current_state.fail;
        current_state.for_each_transition(move |b, maybe_si| {
            let goto = if maybe_si == FAIL_STATE {
                ac.memoized_next_state(self, si, first_fail_state, b)
            } else {
                maybe_si
            };
            self.set(si, b, goto);
        });
    }
}

impl<P: AsRef<[u8]> + fmt::Debug> fmt::Debug for FullAcAutomaton<P> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "FullAcAutomaton({:?})", self.pats)
    }
}
