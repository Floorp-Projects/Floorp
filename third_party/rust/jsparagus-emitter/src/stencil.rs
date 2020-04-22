//! The result of emitter

use crate::compilation_info::CompilationInfo;
use crate::gcthings::GCThing;
use crate::regexp::RegExpItem;
use crate::scope_notes::ScopeNote;

use ast::source_atom_set::SourceAtomSetIndex;
use scope::data::ScopeData;
use scope::frame_slot::FrameSlot;

/// The result of emitter.
pub struct EmitResult<'alloc> {
    pub atoms: Vec<&'alloc str>,
    pub slices: Vec<&'alloc str>,
    pub scopes: Vec<ScopeData>,

    /// Emitted scripts.
    /// The first item corresponds to the global script, and the remaining
    /// items correspond to inner functions.
    pub scripts: Vec<ScriptStencil>,
}

impl<'alloc> EmitResult<'alloc> {
    pub fn new(compilation_info: CompilationInfo<'alloc>, scripts: Vec<ScriptStencil>) -> Self {
        Self {
            atoms: compilation_info.atoms.into(),
            slices: compilation_info.slices.into(),
            scopes: compilation_info.scope_data_map.into(),
            scripts,
        }
    }
}

/// Data used to instantiate the non-lazy script.
/// Maps to js::frontend::ScriptStencil in m-c/js/src/frontend/Stencil.h.
#[derive(Debug)]
pub struct ScriptStencil {
    pub bytecode: Vec<u8>,
    pub atoms: Vec<SourceAtomSetIndex>,
    pub regexps: Vec<RegExpItem>,
    pub gcthings: Vec<GCThing>,
    pub scope_notes: Vec<ScopeNote>,
    // Line and column numbers for the first character of source.
    pub lineno: usize,
    pub column: usize,

    pub main_offset: usize,
    pub max_fixed_slots: FrameSlot,
    pub maximum_stack_depth: u32,
    pub body_scope_index: u32,
    pub num_ic_entries: u32,
    pub num_type_sets: u32,

    pub strict: bool,
    pub bindings_accessed_dynamically: bool,
    pub has_call_site_obj: bool,
    pub is_for_eval: bool,
    pub is_module: bool,
    pub is_function: bool,
    pub has_non_syntactic_scope: bool,
    pub needs_function_environment_objects: bool,
    pub has_module_goal: bool,
}
