use ast::associated_data::AssociatedData;
use ast::source_atom_set::SourceAtomSet;
use ast::source_slice_list::SourceSliceList;
use ast::types::Function;
use scope::data::ScopeDataMap;

pub struct CompilationInfo<'alloc> {
    pub atoms: SourceAtomSet<'alloc>,
    pub slices: SourceSliceList<'alloc>,
    pub scope_data_map: ScopeDataMap,
    pub function_map: AssociatedData<&'alloc Function<'alloc>>,
}

impl<'alloc> CompilationInfo<'alloc> {
    pub fn new(
        atoms: SourceAtomSet<'alloc>,
        slices: SourceSliceList<'alloc>,
        scope_data_map: ScopeDataMap,
        function_map: AssociatedData<&'alloc Function<'alloc>>,
    ) -> Self {
        Self {
            atoms,
            slices,
            scope_data_map,
            function_map,
        }
    }
}
