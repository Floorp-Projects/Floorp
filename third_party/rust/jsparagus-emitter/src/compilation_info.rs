use ast::source_atom_set::SourceAtomSet;
use ast::source_slice_list::SourceSliceList;
use scope::data::ScopeDataMap;

pub struct CompilationInfo<'alloc> {
    pub atoms: SourceAtomSet<'alloc>,
    pub slices: SourceSliceList<'alloc>,
    pub scope_data_map: ScopeDataMap,
}

impl<'alloc> CompilationInfo<'alloc> {
    pub fn new(
        atoms: SourceAtomSet<'alloc>,
        slices: SourceSliceList<'alloc>,
        scope_data_map: ScopeDataMap,
    ) -> Self {
        Self {
            atoms,
            slices,
            scope_data_map,
        }
    }
}
