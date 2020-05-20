//! The result of emitter

use crate::frame_slot::FrameSlot;
use crate::function::FunctionCreationData;
use crate::gcthings::GCThing;
use crate::regexp::RegExpItem;
use crate::scope_notes::ScopeNote;

/// Data used to instantiate the non-lazy script.
/// Maps to js::frontend::ScriptStencil in m-c/js/src/frontend/Stencil.h.
#[derive(Debug)]
pub struct ScriptStencil {
    pub bytecode: Vec<u8>,
    pub regexps: Vec<RegExpItem>,
    pub gcthings: Vec<GCThing>,
    pub scope_notes: Vec<ScopeNote>,
    pub inner_functions: Vec<FunctionCreationData>,

    /// Line and column numbers for the first character of source.
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

/// Index into ScriptStencilList.scripts.
#[derive(Debug, Clone, Copy)]
pub struct ScriptStencilIndex {
    index: usize,
}

impl ScriptStencilIndex {
    fn new(index: usize) -> Self {
        Self { index }
    }
}

impl From<ScriptStencilIndex> for usize {
    fn from(index: ScriptStencilIndex) -> usize {
        index.index
    }
}

/// List of stencil scripts.
#[derive(Debug)]
pub struct ScriptStencilList {
    /// Uses Option to allow `allocate()` and `populate()` to be called
    /// separately.
    scripts: Vec<Option<ScriptStencil>>,
}

impl ScriptStencilList {
    pub fn new() -> Self {
        Self {
            scripts: Vec::new(),
        }
    }

    pub fn push(&mut self, script: ScriptStencil) -> ScriptStencilIndex {
        let index = self.scripts.len();
        self.scripts.push(Some(script));
        ScriptStencilIndex::new(index)
    }

    pub fn allocate(&mut self) -> ScriptStencilIndex {
        let index = self.scripts.len();
        self.scripts.push(None);
        ScriptStencilIndex::new(index)
    }

    pub fn populate(&mut self, index: ScriptStencilIndex, script: ScriptStencil) {
        self.scripts[usize::from(index)].replace(script);
    }
}

impl From<ScriptStencilList> for Vec<ScriptStencil> {
    fn from(list: ScriptStencilList) -> Vec<ScriptStencil> {
        list.scripts
            .into_iter()
            .map(|g| g.expect("Should be populated"))
            .collect()
    }
}
