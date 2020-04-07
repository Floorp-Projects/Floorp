use crate::emitter::InstructionWriter;
use crate::scope_notes::ScopeNoteIndex;
use ast::source_atom_set::SourceAtomSetIndex;
use scope::data::{BindingKind, GlobalScopeData, LexicalScopeData, ScopeDataMap, ScopeIndex};
use scope::frame_slot::FrameSlot;
use std::collections::HashMap;

/// Corresponds to js::frontend::NameLocation in
/// m-c/js/src/frontend/NameAnalysisTypes.h
#[derive(Debug, Clone)]
pub enum NameLocation {
    Dynamic,
    Global(BindingKind),
    FrameSlot(FrameSlot, BindingKind),
}

#[derive(Debug)]
pub struct GlobalEmitterScope {
    cache: HashMap<SourceAtomSetIndex, NameLocation>,
}

impl GlobalEmitterScope {
    pub fn new(data: &GlobalScopeData) -> Self {
        let mut cache = HashMap::new();
        for item in data.iter() {
            cache.insert(item.name(), NameLocation::Global(item.kind()));
        }
        Self { cache }
    }

    fn lookup_name(&self, name: SourceAtomSetIndex) -> Option<NameLocation> {
        match self.cache.get(&name) {
            Some(loc) => Some(loc.clone()),
            None => Some(NameLocation::Global(BindingKind::Var)),
        }
    }

    fn next_frame_slot(&self) -> FrameSlot {
        FrameSlot::new(0)
    }

    fn scope_note_index(&self) -> Option<ScopeNoteIndex> {
        None
    }
}

#[derive(Debug)]
pub struct LexicalEmitterScope {
    cache: HashMap<SourceAtomSetIndex, NameLocation>,
    next_frame_slot: FrameSlot,
    scope_note_index: Option<ScopeNoteIndex>,
}

impl LexicalEmitterScope {
    pub fn new(data: &LexicalScopeData, first_frame_slot: FrameSlot) -> Self {
        let mut cache = HashMap::new();
        let mut slot = first_frame_slot;
        for item in data.iter() {
            // FIXME: support environment (item.is_closed_over()).
            cache.insert(item.name(), NameLocation::FrameSlot(slot, item.kind()));
            slot.next();
        }

        Self {
            cache,
            next_frame_slot: slot,
            scope_note_index: None,
        }
    }

    fn lookup_name(&self, name: SourceAtomSetIndex) -> Option<NameLocation> {
        match self.cache.get(&name) {
            Some(loc) => Some(loc.clone()),
            None => None,
        }
    }

    fn next_frame_slot(&self) -> FrameSlot {
        self.next_frame_slot
    }

    fn scope_note_index(&self) -> Option<ScopeNoteIndex> {
        self.scope_note_index
    }
}

#[derive(Debug)]
pub enum EmitterScope {
    Global(GlobalEmitterScope),
    Lexical(LexicalEmitterScope),
}

impl EmitterScope {
    fn lookup_name(&self, name: SourceAtomSetIndex) -> Option<NameLocation> {
        match self {
            EmitterScope::Global(scope) => scope.lookup_name(name),
            EmitterScope::Lexical(scope) => scope.lookup_name(name),
        }
    }

    fn next_frame_slot(&self) -> FrameSlot {
        match self {
            EmitterScope::Global(scope) => scope.next_frame_slot(),
            EmitterScope::Lexical(scope) => scope.next_frame_slot(),
        }
    }

    fn scope_note_index(&self) -> Option<ScopeNoteIndex> {
        match self {
            EmitterScope::Global(scope) => scope.scope_note_index(),
            EmitterScope::Lexical(scope) => scope.scope_note_index(),
        }
    }
}

/// Compared to C++ impl, this uses explicit stack struct,
/// given Rust cannot store a reference of stack-allocated object into
/// another object that has longer-lifetime.
///
/// Some methods are implemented here, instead of each EmitterScope.
pub struct EmitterScopeStack {
    scope_stack: Vec<EmitterScope>,
}

impl EmitterScopeStack {
    pub fn new() -> Self {
        Self {
            scope_stack: Vec::new(),
        }
    }

    pub fn innermost(&self) -> &EmitterScope {
        self.scope_stack
            .last()
            .expect("There should be at least one scope")
    }

    pub fn enter_global(&mut self, emit: &mut InstructionWriter, scope_data_map: &ScopeDataMap) {
        let scope_index = scope_data_map.get_global_index();
        let scope_data = scope_data_map.get_global_at(scope_index);

        if scope_data.bindings.len() > 0 {
            emit.check_global_or_eval_decl();
        }

        for item in scope_data.iter() {
            let name_index = emit.get_atom_index(item.name());

            match item.kind() {
                BindingKind::Var => {
                    if !item.is_top_level_function() {
                        emit.def_var(name_index);
                    }
                }
                BindingKind::Let => {
                    emit.def_let(name_index);
                }
                BindingKind::Const => {
                    emit.def_const(name_index);
                }
            }
        }

        emit.switch_to_main();

        let scope = EmitterScope::Global(GlobalEmitterScope::new(scope_data));
        self.scope_stack.push(scope);

        emit.enter_global_scope(scope_index);
    }

    pub fn leave_global(&mut self, emit: &InstructionWriter) {
        match self.scope_stack.pop() {
            Some(EmitterScope::Global(_)) => {}
            _ => panic!("unmatching scope"),
        }
        emit.leave_global_scope();
    }

    pub fn dead_zone_frame_slot_range(
        &self,
        emit: &mut InstructionWriter,
        slot_start: FrameSlot,
        slot_end: FrameSlot,
    ) {
        if slot_start == slot_end {
            return;
        }

        emit.uninitialized();
        let mut slot = slot_start;
        while slot < slot_end {
            emit.init_lexical(slot.into());
            slot.next();
        }
        emit.pop();
    }

    pub fn enter_lexical(
        &mut self,
        emit: &mut InstructionWriter,
        scope_data_map: &mut ScopeDataMap,
        scope_index: ScopeIndex,
    ) {
        let mut scope_data = scope_data_map.get_lexical_at_mut(scope_index);

        let first_frame_slot = self.innermost().next_frame_slot();
        let parent_scope_note_index = self.innermost().scope_note_index();

        scope_data.first_frame_slot = first_frame_slot;

        let mut lexical_scope = LexicalEmitterScope::new(scope_data, first_frame_slot);
        let next_frame_slot = lexical_scope.next_frame_slot;
        let index = emit.enter_lexical_scope(scope_index, parent_scope_note_index, next_frame_slot);
        lexical_scope.scope_note_index = Some(index);

        let scope = EmitterScope::Lexical(lexical_scope);
        self.scope_stack.push(scope);

        self.dead_zone_frame_slot_range(emit, first_frame_slot, next_frame_slot);
    }

    pub fn leave_lexical(&mut self, emit: &mut InstructionWriter) {
        let lexical_scope = match self.scope_stack.pop() {
            Some(EmitterScope::Lexical(scope)) => scope,
            _ => panic!("unmatching scope"),
        };
        emit.leave_lexical_scope(
            lexical_scope
                .scope_note_index
                .expect("scope note index should be populated"),
        );
    }

    pub fn lookup_name(&mut self, name: SourceAtomSetIndex) -> NameLocation {
        for scope in self.scope_stack.iter().rev() {
            if let Some(loc) = scope.lookup_name(name) {
                // FIXME: handle hops in aliased var.
                return loc;
            }
        }

        NameLocation::Dynamic
    }
}
