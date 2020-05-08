//! Code for tracking scopes and looking up names as the emitter traverses
//! the program.
//!
//! EmitterScopes exist only while the bytecode emitter is working.
//! Longer-lived scope information is stored in `scope::ScopeDataMap`.

use crate::emitter::InstructionWriter;
use crate::scope_notes::ScopeNoteIndex;
use ast::source_atom_set::SourceAtomSetIndex;
use scope::data::{BindingKind, GlobalScopeData, LexicalScopeData, ScopeDataMap, ScopeIndex};
use scope::frame_slot::FrameSlot;
use std::collections::HashMap;

/// Result of looking up a name.
///
/// Corresponds to js::frontend::NameLocation in
/// m-c/js/src/frontend/NameAnalysisTypes.h
#[derive(Debug, Clone)]
pub enum NameLocation {
    Dynamic,
    Global(BindingKind),
    FrameSlot(FrameSlot, BindingKind),
}

// --- EmitterScope types
//
// These types are the variants of enum EmitterScope.

#[derive(Debug)]
struct GlobalEmitterScope {
    cache: HashMap<SourceAtomSetIndex, NameLocation>,
}

impl GlobalEmitterScope {
    fn new(data: &GlobalScopeData) -> Self {
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

/// The information about a scope needed for emitting bytecode.
#[derive(Debug)]
enum EmitterScope {
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

/// Stack that tracks the current scope chain while emitting bytecode.
///
/// Bytecode is emitted by traversing the structure of the program. During this
/// process there's always a "current position". This stack represents the
/// scope chain at the current position. It is updated when we enter or leave
/// any node that has its own scope.
///
/// Unlike the C++ impl, this uses an explicit stack struct, since Rust cannot
/// store a reference to a stack-allocated object in another object that has a
/// longer lifetime.
pub struct EmitterScopeStack {
    scope_stack: Vec<EmitterScope>,
}

impl EmitterScopeStack {
    /// Create a new, empty scope stack.
    pub fn new() -> Self {
        Self {
            scope_stack: Vec::new(),
        }
    }

    /// The current innermost scope.
    fn innermost(&self) -> &EmitterScope {
        self.scope_stack
            .last()
            .expect("There should be at least one scope")
    }

    /// Enter the global scope. Call this once at the beginning of a top-level
    /// script.
    ///
    /// This emits bytecode that implements parts of [ScriptEvaluation][1] and
    /// [GlobalDeclarationInstantiation][2].
    ///
    /// [1]: https://tc39.es/ecma262/#sec-runtime-semantics-scriptevaluation
    /// [2]: https://tc39.es/ecma262/#sec-globaldeclarationinstantiation
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

    /// Leave the global scope. Call this once at the end of a top-level
    /// script.
    pub fn leave_global(&mut self, emit: &InstructionWriter) {
        match self.scope_stack.pop() {
            Some(EmitterScope::Global(_)) => {}
            _ => panic!("unmatching scope"),
        }
        emit.leave_global_scope();
    }

    /// Emit bytecode to mark some local lexical variables as uninitialized.
    fn dead_zone_frame_slot_range(
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

    /// Enter a lexical scope.
    ///
    /// A new LexicalEmitterScope based on scope_data_map is pushed to the
    /// scope stack. Bytecode is emitted to mark the new lexical bindings as
    /// uninitialized.
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

    /// Leave a lexical scope.
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

    /// Resolve a name by searching the current scope chain.
    ///
    /// Implements the parts of [ResolveBinding][1] that can be done at
    /// emit time.
    ///
    /// [1]: https://tc39.es/ecma262/#sec-resolvebinding
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
