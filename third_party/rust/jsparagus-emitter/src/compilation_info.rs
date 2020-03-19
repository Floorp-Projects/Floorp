use crate::scope::ScopeDataMap;
use ast::source_atom_set::SourceAtomSet;

pub struct CompilationInfo<'alloc> {
    pub atoms: SourceAtomSet<'alloc>,
    pub scope_data_map: ScopeDataMap,
}

impl<'alloc> CompilationInfo<'alloc> {
    pub fn new(atoms: SourceAtomSet<'alloc>, scope_data_map: ScopeDataMap) -> Self {
        Self {
            atoms,
            scope_data_map,
        }
    }
}
