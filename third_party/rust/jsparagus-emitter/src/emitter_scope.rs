//! Code for tracking scopes and looking up names as the emitter traverses
//! the program.
//!
//! EmitterScopes exist only while the bytecode emitter is working.
//! Longer-lived scope information is stored in `ScopeDataMap`.

use crate::emitter::InstructionWriter;
use ast::source_atom_set::SourceAtomSetIndex;
use std::collections::HashMap;
use std::iter::Iterator;
use stencil::env_coord::{EnvironmentHops, EnvironmentSlot};
use stencil::frame_slot::FrameSlot;
use stencil::scope::{BindingKind, GlobalScopeData, LexicalScopeData, ScopeDataMap, ScopeIndex};
use stencil::scope_notes::ScopeNoteIndex;

/// Result of looking up a name.
///
/// Corresponds to js::frontend::NameLocation in
/// m-c/js/src/frontend/NameAnalysisTypes.h
#[derive(Debug, Clone)]
pub enum NameLocation {
    Dynamic,
    Global(BindingKind),
    FrameSlot(FrameSlot, BindingKind),
    EnvironmentCoord(EnvironmentHops, EnvironmentSlot, BindingKind),
}

#[derive(Debug, Copy, Clone, PartialEq)]
pub struct EmitterScopeDepth {
    index: usize,
}

impl EmitterScopeDepth {
    fn has_parent(&self) -> bool {
        self.index > 0
    }

    fn parent(&self) -> Self {
        debug_assert!(self.has_parent());

        Self {
            index: self.index - 1,
        }
    }
}

// --- EmitterScope types
//
// These types are the variants of enum EmitterScope.

#[derive(Debug)]
pub struct GlobalEmitterScope {
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

    fn has_environment_object(&self) -> bool {
        false
    }
}

struct LexicalEnvironmentObject {}
impl LexicalEnvironmentObject {
    fn first_free_slot() -> u32 {
        // FIXME: This is the value of
        //   `JSSLOT_FREE(&LexicalEnvironmentObject::class_)`
        // in SpiderMonkey
        2
    }
}

#[derive(Debug)]
pub struct LexicalEmitterScope {
    cache: HashMap<SourceAtomSetIndex, NameLocation>,
    next_frame_slot: FrameSlot,
    needs_environment_object: bool,
    scope_note_index: Option<ScopeNoteIndex>,
}

impl LexicalEmitterScope {
    pub fn new(data: &LexicalScopeData, first_frame_slot: FrameSlot) -> Self {
        let is_all_bindings_closed_over = data.base.is_all_bindings_closed_over();
        let mut needs_environment_object = false;

        let mut cache = HashMap::new();
        let mut frame_slot = first_frame_slot;
        let mut env_slot = EnvironmentSlot::new(LexicalEnvironmentObject::first_free_slot());
        for item in data.iter() {
            if is_all_bindings_closed_over || item.is_closed_over() {
                cache.insert(
                    item.name(),
                    NameLocation::EnvironmentCoord(EnvironmentHops::new(0), env_slot, item.kind()),
                );
                env_slot.next();
                needs_environment_object = true;
            } else {
                cache.insert(
                    item.name(),
                    NameLocation::FrameSlot(frame_slot, item.kind()),
                );
                frame_slot.next();
            }
        }

        Self {
            cache,
            next_frame_slot: frame_slot,
            needs_environment_object,
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

    pub fn has_environment_object(&self) -> bool {
        self.needs_environment_object
    }
}

/// The information about a scope needed for emitting bytecode.
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

    pub fn scope_note_index(&self) -> Option<ScopeNoteIndex> {
        match self {
            EmitterScope::Global(scope) => scope.scope_note_index(),
            EmitterScope::Lexical(scope) => scope.scope_note_index(),
        }
    }

    fn has_environment_object(&self) -> bool {
        match self {
            EmitterScope::Global(scope) => scope.has_environment_object(),
            EmitterScope::Lexical(scope) => scope.has_environment_object(),
        }
    }

    fn is_var_scope(&self) -> bool {
        match self {
            EmitterScope::Global(_) => true,
            EmitterScope::Lexical(_) => false,
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

        // The outermost scope should be the first item in the GC things list.
        // Enter global scope here, before emitting any name ops below.
        emit.enter_global_scope(scope_index);

        if scope_data.base.bindings.len() > 0 {
            emit.check_global_or_eval_decl();
        }

        for item in scope_data.iter() {
            let name_index = emit.get_atom_gcthing_index(item.name());

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

        let parent_scope_note_index = self.innermost().scope_note_index();

        let first_frame_slot = self.innermost().next_frame_slot();
        scope_data.first_frame_slot = first_frame_slot;
        let mut lexical_scope = LexicalEmitterScope::new(scope_data, first_frame_slot);
        let next_frame_slot = lexical_scope.next_frame_slot;
        let index = emit.enter_lexical_scope(
            scope_index,
            parent_scope_note_index,
            next_frame_slot,
            lexical_scope.needs_environment_object,
        );
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
            lexical_scope.needs_environment_object,
        );
    }

    /// Resolve a name by searching the current scope chain.
    ///
    /// Implements the parts of [ResolveBinding][1] that can be done at
    /// emit time.
    ///
    /// [1]: https://tc39.es/ecma262/#sec-resolvebinding
    pub fn lookup_name(&mut self, name: SourceAtomSetIndex) -> NameLocation {
        let mut hops = EnvironmentHops::new(0);

        for scope in self.scope_stack.iter().rev() {
            if let Some(loc) = scope.lookup_name(name) {
                return match loc {
                    NameLocation::EnvironmentCoord(orig_hops, slot, kind) => {
                        debug_assert!(u8::from(orig_hops) == 0u8);
                        NameLocation::EnvironmentCoord(hops, slot, kind)
                    }
                    _ => loc,
                };
            }
            if scope.has_environment_object() {
                hops.next();
            }
        }

        NameLocation::Dynamic
    }

    /// Just like lookup_name, but only in var scope.
    pub fn lookup_name_in_var(&mut self, name: SourceAtomSetIndex) -> NameLocation {
        let mut hops = EnvironmentHops::new(0);

        for scope in self.scope_stack.iter().rev() {
            if scope.is_var_scope() {
                if let Some(loc) = scope.lookup_name(name) {
                    return match loc {
                        NameLocation::EnvironmentCoord(orig_hops, slot, kind) => {
                            debug_assert!(u8::from(orig_hops) == 0u8);
                            NameLocation::EnvironmentCoord(hops, slot, kind)
                        }
                        _ => loc,
                    };
                }
            }

            if scope.has_environment_object() {
                hops.next();
            }
        }

        NameLocation::Dynamic
    }

    pub fn current_depth(&self) -> EmitterScopeDepth {
        EmitterScopeDepth {
            index: self.scope_stack.len() - 1,
        }
    }

    /// Walk the scope stack up to and including `to` depth.
    /// See EmitterScopeWalker for the details.
    pub fn walk_up_to_including<'a>(&'a self, to: EmitterScopeDepth) -> EmitterScopeWalker<'a> {
        EmitterScopeWalker::new(self, to)
    }

    pub fn get_current_scope_note_index(&self) -> Option<ScopeNoteIndex> {
        self.innermost().scope_note_index()
    }

    fn get<'a>(&'a self, index: EmitterScopeDepth) -> &'a EmitterScope {
        self.scope_stack
            .get(index.index)
            .expect("scope should exist")
    }
}

/// Walk the scope stack up to `to`, and yields EmitterScopeWalkItem for
/// each scope.
///
/// The first item is `{ outer: parent-of-innermost, inner: innermost }`, and
/// the last item is `{ outer: to, inner: child-of-to }`.
pub struct EmitterScopeWalker<'a> {
    stack: &'a EmitterScopeStack,
    to: EmitterScopeDepth,
    current: EmitterScopeDepth,
}

impl<'a> EmitterScopeWalker<'a> {
    fn new(stack: &'a EmitterScopeStack, to: EmitterScopeDepth) -> Self {
        let current = stack.current_depth();

        Self { stack, to, current }
    }
}

pub struct EmitterScopeWalkItem<'a> {
    pub outer: &'a EmitterScope,
    pub inner: &'a EmitterScope,
}

impl<'a> Iterator for EmitterScopeWalker<'a> {
    type Item = EmitterScopeWalkItem<'a>;

    fn next(&mut self) -> Option<EmitterScopeWalkItem<'a>> {
        if self.current == self.to {
            return None;
        }

        let outer_index = self.current.parent();
        let inner_index = self.current;
        self.current = outer_index;

        Some(EmitterScopeWalkItem {
            inner: self.stack.get(inner_index),
            outer: self.stack.get(outer_index),
        })
    }
}
