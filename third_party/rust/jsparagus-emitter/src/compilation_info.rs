use ast::associated_data::AssociatedData;
use ast::source_atom_set::SourceAtomSet;
use ast::source_slice_list::SourceSliceList;
use ast::types::Function;
use scope::data::FunctionDeclarationPropertyMap;
use std::collections::HashMap;
use stencil::regexp::RegExpList;
use stencil::scope::ScopeDataMap;
use stencil::script::{ImmutableScriptDataList, ScriptStencilIndex, ScriptStencilList};

pub struct CompilationInfo<'alloc> {
    pub atoms: SourceAtomSet<'alloc>,
    pub slices: SourceSliceList<'alloc>,
    pub regexps: RegExpList,
    pub scope_data_map: ScopeDataMap,
    pub function_declarations: HashMap<ScriptStencilIndex, &'alloc Function<'alloc>>,
    pub function_stencil_indices: AssociatedData<ScriptStencilIndex>,
    pub function_declaration_properties: FunctionDeclarationPropertyMap,
    pub functions: ScriptStencilList,
    pub script_data_list: ImmutableScriptDataList,
}

impl<'alloc> CompilationInfo<'alloc> {
    pub fn new(
        atoms: SourceAtomSet<'alloc>,
        slices: SourceSliceList<'alloc>,
        scope_data_map: ScopeDataMap,
        function_declarations: HashMap<ScriptStencilIndex, &'alloc Function<'alloc>>,
        function_stencil_indices: AssociatedData<ScriptStencilIndex>,
        function_declaration_properties: FunctionDeclarationPropertyMap,
        functions: ScriptStencilList,
    ) -> Self {
        Self {
            atoms,
            slices,
            regexps: RegExpList::new(),
            scope_data_map,
            function_declarations,
            function_stencil_indices,
            function_declaration_properties,
            functions,
            script_data_list: ImmutableScriptDataList::new(),
        }
    }
}
