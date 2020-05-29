use crate::function::FunctionStencil;
use crate::scope::ScopeData;
use crate::script::ScriptStencil;

/// The result of emitter.
#[derive(Debug)]
pub struct EmitResult<'alloc> {
    pub atoms: Vec<&'alloc str>,
    pub slices: Vec<&'alloc str>,
    pub scopes: Vec<ScopeData>,

    /// Emitted scripts.
    /// The first item corresponds to the global script, and the remaining
    /// items correspond to inner functions.
    pub script: ScriptStencil,
    pub functions: Vec<FunctionStencil>,
}

impl<'alloc> EmitResult<'alloc> {
    pub fn new(
        atoms: Vec<&'alloc str>,
        slices: Vec<&'alloc str>,
        scopes: Vec<ScopeData>,
        script: ScriptStencil,
        functions: Vec<FunctionStencil>,
    ) -> Self {
        Self {
            atoms,
            slices,
            scopes,
            script,
            functions,
        }
    }
}
