use crate::emitter::BytecodeOffset;
use crate::scope::ScopeIndex;

/// Maps to js::ScopeNote in m-c/js/src/vm//JSScript.h.
#[derive(Debug)]
pub struct ScopeNote {
    pub scope_index: ScopeIndex,
    pub start: BytecodeOffset,
    pub end: BytecodeOffset,
    pub parent: Option<ScopeNoteIndex>,
}

impl ScopeNote {
    fn new(scope_index: ScopeIndex, start: BytecodeOffset, parent: Option<ScopeNoteIndex>) -> Self {
        Self {
            scope_index,
            start: start.clone(),
            end: start,
            parent,
        }
    }
}

/// Index into ScopeNoteList.notes.
#[derive(Debug, Clone, Copy)]
pub struct ScopeNoteIndex {
    index: usize,
}

impl ScopeNoteIndex {
    fn new(index: usize) -> Self {
        Self { index }
    }
}

impl From<ScopeNoteIndex> for usize {
    fn from(index: ScopeNoteIndex) -> usize {
        index.index
    }
}

/// List of scope notes.
///
/// Maps to JSScript::scopeNotes() in js/src/vm/JSScript.h.
pub struct ScopeNoteList {
    notes: Vec<ScopeNote>,
}

impl ScopeNoteList {
    pub fn new() -> Self {
        Self { notes: Vec::new() }
    }

    pub fn enter_scope(
        &mut self,
        scope_index: ScopeIndex,
        offset: BytecodeOffset,
        parent: Option<ScopeNoteIndex>,
    ) -> ScopeNoteIndex {
        let index = self.notes.len();
        self.notes.push(ScopeNote::new(scope_index, offset, parent));
        ScopeNoteIndex::new(index)
    }

    pub fn leave_scope(&mut self, index: ScopeNoteIndex, offset: BytecodeOffset) {
        self.notes[usize::from(index)].end = offset;
    }
}

impl From<ScopeNoteList> for Vec<ScopeNote> {
    fn from(list: ScopeNoteList) -> Vec<ScopeNote> {
        list.notes
    }
}
