use crate::scope::ScopeIndex;

/// Corresponds to js::frontend::GCThingList::ListType
/// in m-c/js/src/frontend/BytecodeSection.h.
#[derive(Debug)]
pub enum GCThing {
    Scope(ScopeIndex),
}

/// Index into GCThingList.atoms.
#[derive(Debug, Clone, Copy)]
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

    pub fn append_scope(&mut self, scope_index: ScopeIndex) -> GCThingIndex {
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
