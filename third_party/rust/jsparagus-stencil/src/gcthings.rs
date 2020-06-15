use crate::regexp::RegExpIndex;
use crate::scope::ScopeIndex;
use crate::script::ScriptStencilIndex;
use ast::source_atom_set::SourceAtomSetIndex;

/// Corresponds to js::frontend::GCThingList::ListType
/// in m-c/js/src/frontend/BytecodeSection.h.
#[derive(Debug)]
pub enum GCThing {
    Atom(SourceAtomSetIndex),
    Function(ScriptStencilIndex),
    RegExp(RegExpIndex),
    Scope(ScopeIndex),
}

/// Index into GCThingList.things.
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct GCThingIndex {
    index: usize,
}

impl GCThingIndex {
    fn new(index: usize) -> Self {
        Self { index }
    }
}

impl From<GCThingIndex> for usize {
    fn from(index: GCThingIndex) -> usize {
        index.index
    }
}

/// List of GC things.
///
/// Maps to JSScript::gcthings() in js/src/vm/JSScript.h.
#[derive(Debug)]
pub struct GCThingList {
    things: Vec<GCThing>,
}

impl GCThingList {
    pub fn new() -> Self {
        Self { things: Vec::new() }
    }

    pub fn push_atom(&mut self, atom_index: SourceAtomSetIndex) -> GCThingIndex {
        let index = self.things.len();
        self.things.push(GCThing::Atom(atom_index));
        GCThingIndex::new(index)
    }

    pub fn push_function(&mut self, fun_index: ScriptStencilIndex) -> GCThingIndex {
        let index = self.things.len();
        self.things.push(GCThing::Function(fun_index));
        GCThingIndex::new(index)
    }

    pub fn push_regexp(&mut self, regexp_index: RegExpIndex) -> GCThingIndex {
        let index = self.things.len();
        self.things.push(GCThing::RegExp(regexp_index));
        GCThingIndex::new(index)
    }

    pub fn push_scope(&mut self, scope_index: ScopeIndex) -> GCThingIndex {
        let index = self.things.len();
        self.things.push(GCThing::Scope(scope_index));
        GCThingIndex::new(index)
    }
}

impl From<GCThingList> for Vec<GCThing> {
    fn from(list: GCThingList) -> Vec<GCThing> {
        list.things
    }
}
