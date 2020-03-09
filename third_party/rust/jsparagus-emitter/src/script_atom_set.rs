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

    pub fn into_raw(self) -> u32 {
        self.index
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

    pub fn into_vec(mut self) -> Vec<SourceAtomSetIndex> {
        self.atoms.drain(..).collect()
    }
}
