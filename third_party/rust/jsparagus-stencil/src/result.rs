use crate::regexp::RegExpItem;
use crate::scope::ScopeData;
use crate::script::{ImmutableScriptData, ScriptStencil};

/// The result of emitter.
#[derive(Debug)]
pub struct EmitResult<'alloc> {
    pub atoms: Vec<&'alloc str>,
    pub slices: Vec<&'alloc str>,
    pub scopes: Vec<ScopeData>,
    pub regexps: Vec<RegExpItem>,

    pub scripts: Vec<ScriptStencil>,
    pub script_data_list: Vec<ImmutableScriptData>,
}

impl<'alloc> EmitResult<'alloc> {
    pub fn new(
        atoms: Vec<&'alloc str>,
        slices: Vec<&'alloc str>,
        scopes: Vec<ScopeData>,
        regexps: Vec<RegExpItem>,
        scripts: Vec<ScriptStencil>,
        script_data_list: Vec<ImmutableScriptData>,
    ) -> Self {
        Self {
            atoms,
            slices,
            scopes,
            regexps,
            scripts,
            script_data_list,
        }
    }
}
