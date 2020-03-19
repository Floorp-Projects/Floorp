use ast::source_atom_set::SourceAtomSetIndex;
use indexmap::set::IndexSet;

/// Index into ScriptAtomSet.atoms.
#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub struct ScriptAtomSetIndex {
    index: u32,
}
impl ScriptAtomSetIndex {
    fn new(index: u32) -> Self {
        Self { index }
    }
}

impl From<ScriptAtomSetIndex> for u32 {
    fn from(index: ScriptAtomSetIndex) -> u32 {
        index.index
    }
}

/// List of atoms referred from bytecode.
///
/// Maps to JSScript::atoms() in js/src/vm/JSScript.h.
pub struct ScriptAtomSet {
    // This keeps the insertion order.
    atoms: IndexSet<SourceAtomSetIndex>,
}

impl ScriptAtomSet {
    pub fn new() -> Self {
        Self {
            atoms: IndexSet::new(),
        }
    }

    pub fn insert(&mut self, value: SourceAtomSetIndex) -> ScriptAtomSetIndex {
        let (index, _) = self.atoms.insert_full(value);
        ScriptAtomSetIndex::new(index as u32)
    }
}

impl From<ScriptAtomSet> for Vec<SourceAtomSetIndex> {
    fn from(set: ScriptAtomSet) -> Vec<SourceAtomSetIndex> {
        set.atoms.into_iter().collect()
    }
}
