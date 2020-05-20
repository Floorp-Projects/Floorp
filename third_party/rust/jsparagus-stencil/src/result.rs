use crate::scope::ScopeData;
use crate::script::ScriptStencil;

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
    pub fn new(
        atoms: Vec<&'alloc str>,
        slices: Vec<&'alloc str>,
        scopes: Vec<ScopeData>,
        scripts: Vec<ScriptStencil>,
    ) -> Self {
        Self {
            atoms,
            slices,
            scopes,
            scripts,
        }
    }
}
