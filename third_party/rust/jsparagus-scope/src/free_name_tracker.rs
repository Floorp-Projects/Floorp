//! Code to track free names in scopes, to compute which bindings are closed
//! over.
//!
//! A binding is closed-over if it is defined in a scope in one `JSScript` and
//! can be used in another `JSScript` (a nested function or direct eval).
//!
//! In our single pass over the AST, we compute which bindings are closed-over
//! using a `FreeNameTracker` for each scope.

use ast::source_atom_set::SourceAtomSetIndex;
use std::collections::hash_map::RandomState;
use std::collections::hash_set::Difference;
use std::collections::HashSet;

/// Tracks free names in a scope.
///
/// Usage:
///
/// *   Create this when entering a scope.
///
/// *   Call `.note_use(name)` when a name is used and `.note_def(name)` when
///     one is defined.
///
/// *   Call `.propagate_from_inner_script(inner)` or
///     `.propagate_from_inner_non_script(inner)` when leaving an inner
///     scope. These methods do the real work of marking bindings as
///     closed-over.
///
/// The gist of the algorithm is tracking name *uses* and propagating them
/// outward to the scope where the name is *defined*. When we propagate a use
/// out of a function, we put it in `closed_over_names`; when that use is
/// matched with a binding, the binding is closed-over.
///
/// **Why we do all the work when exiting a scope** - It would save a lot of
/// work if we could mark a def as closed-over as soon as we see a matching
/// use.  We could deal with each use as we encounter it. We wouldn't have to
/// track them. The problem is hoisting. Bindings can be used anywhere in a
/// scope, even before their definition. We can't mark a def that we haven't
/// seen yet. The end of scope is the first point where we have definitely seen
/// all uses and defs in that scope.
///
#[derive(Debug)]
pub struct FreeNameTracker {
    /// Names closed over in inner script-scopes, that's not yet
    /// defined up to (excluding) the current scope.
    /// The name might be defined in the current scope.
    ///
    /// The name defined in this or outer scope is marked "closed over", and
    /// no more propagated to outer scope.
    closed_over_names: HashSet<SourceAtomSetIndex>,

    /// Names used in the current scope, and nested non-script scopes.
    /// The name might be defined in the current scope.
    used_names: HashSet<SourceAtomSetIndex>,

    /// Names defined in the current scope.
    def_names: HashSet<SourceAtomSetIndex>,
}

impl FreeNameTracker {
    pub fn new() -> Self {
        Self {
            closed_over_names: HashSet::new(),
            used_names: HashSet::new(),
            def_names: HashSet::new(),
        }
    }

    fn note_closed_over(&mut self, atom: SourceAtomSetIndex) {
        self.closed_over_names.insert(atom);
    }

    /// Note that the given name is used in this scope.
    pub fn note_use(&mut self, atom: SourceAtomSetIndex) {
        self.used_names.insert(atom);
    }

    pub fn note_def(&mut self, atom: SourceAtomSetIndex) {
        self.def_names.insert(atom);
    }

    /// Names closed over in inner script-scopes, that's not yet defined.
    pub fn closed_over_freevars(&self) -> Difference<'_, SourceAtomSetIndex, RandomState> {
        self.closed_over_names.difference(&self.def_names)
    }

    /// Names used in this and inner non-script scopes, that's not yet defined.
    pub fn used_freevars(&self) -> Difference<'_, SourceAtomSetIndex, RandomState> {
        self.used_names.difference(&self.def_names)
    }

    pub fn is_closed_over_def(&self, atom: &SourceAtomSetIndex) -> bool {
        return self.def_names.contains(atom) && self.closed_over_names.contains(atom);
    }

    /// Propagate free closed over names and used names from the inner
    /// non-script scope.
    pub fn propagate_from_inner_non_script(&mut self, inner: &FreeNameTracker) {
        for atom in inner.closed_over_freevars() {
            self.note_closed_over(*atom);
        }
        for atom in inner.used_freevars() {
            self.note_use(*atom);
        }
    }

    #[allow(dead_code)]
    /// Propagate free closed over names and used names from the inner
    /// script scope, converting everything into free closed over names
    pub fn propagate_from_inner_script(&mut self, inner: &FreeNameTracker) {
        for atom in inner.closed_over_freevars() {
            self.note_closed_over(*atom);
        }
        for atom in inner.used_freevars() {
            self.note_closed_over(*atom);
        }
    }
}
