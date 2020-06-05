use ast::associated_data::AssociatedData;
use ast::source_atom_set::SourceAtomSet;
use ast::source_slice_list::SourceSliceList;
use ast::types::Function;
use scope::data::FunctionDeclarationPropertyMap;
use std::collections::HashMap;
use stencil::function::{FunctionStencilIndex, FunctionStencilList};
use stencil::scope::ScopeDataMap;

pub struct CompilationInfo<'alloc> {
    pub atoms: SourceAtomSet<'alloc>,
    pub slices: SourceSliceList<'alloc>,
    pub scope_data_map: ScopeDataMap,
    pub function_declarations: HashMap<FunctionStencilIndex, &'alloc Function<'alloc>>,
    pub function_stencil_indices: AssociatedData<FunctionStencilIndex>,
    pub function_declaration_properties: FunctionDeclarationPropertyMap,
    pub functions: FunctionStencilList,
}

impl<'alloc> CompilationInfo<'alloc> {
    pub fn new(
        atoms: SourceAtomSet<'alloc>,
        slices: SourceSliceList<'alloc>,
        scope_data_map: ScopeDataMap,
        function_declarations: HashMap<FunctionStencilIndex, &'alloc Function<'alloc>>,
        function_stencil_indices: AssociatedData<FunctionStencilIndex>,
        function_declaration_properties: FunctionDeclarationPropertyMap,
        functions: FunctionStencilList,
    ) -> Self {
        Self {
            atoms,
            slices,
            scope_data_map,
            function_declarations,
            function_stencil_indices,
            function_declaration_properties,
            functions,
        }
    }
}
