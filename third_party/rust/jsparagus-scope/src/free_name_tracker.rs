use ast::source_atom_set::SourceAtomSetIndex;
use std::collections::hash_map::RandomState;
use std::collections::hash_set::Difference;
use std::collections::HashSet;

/// Tracks free names in a scope, to compute which binding is closed over.
///
/// The scope is either script-scope or non-script scope.
/// Script-scope is a scope that's associated with a global or a function.
/// Non-script-scope is everything else.
/// A non-script-scope belongs to a script-scope.
///
/// If a scope S1 uses names [A, B, C], and defines [A, B],
/// name C is a free name, that's considered to be defined in outer scope
/// (or maybe dynamic name).
///
/// If C is defined in a scope S2 that corresponds to different
/// script-scope than S1's script-scope, C in S2 is marked "closed over".
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
    fn closed_over_freevars(&self) -> Difference<'_, SourceAtomSetIndex, RandomState> {
        self.closed_over_names.difference(&self.def_names)
    }

    /// Names used in this and inner non-script scopes, that's not yet defined.
    fn used_freevars(&self) -> Difference<'_, SourceAtomSetIndex, RandomState> {
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
