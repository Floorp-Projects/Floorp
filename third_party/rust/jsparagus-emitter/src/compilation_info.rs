use ast::associated_data::AssociatedData;
use ast::source_atom_set::SourceAtomSet;
use ast::source_slice_list::SourceSliceList;
use ast::types::Function;
use stencil::function::FunctionStencilList;
use stencil::scope::ScopeDataMap;

pub struct CompilationInfo<'alloc> {
    pub atoms: SourceAtomSet<'alloc>,
    pub slices: SourceSliceList<'alloc>,
    pub scope_data_map: ScopeDataMap,
    pub function_map: AssociatedData<&'alloc Function<'alloc>>,
    pub functions: FunctionStencilList,
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
            // FIXME: This should be created by scope pass, that is the
            //        list of all functions, with position information and
            //        inner functions/closed over bindings populated.
            functions: FunctionStencilList::new(),
        }
    }
}
