//! Types for the output of scope analysis.
//!
//! The top-level output of this analysis is the `ScopeDataMap`, which holds
//! the following:
//!   * `LexicalScopeData` for each lexial scope in the AST
//!   * `GlobalScopeData` for the global scope
//!   * `VarScopeData` for extra body var scope in function
//!   * `FunctionScopeData` for the function scope
//!
//! Each scope contains a list of bindings (`BindingName`).

use crate::frame_slot::FrameSlot;
use crate::script::ScriptStencilIndex;
use ast::associated_data::AssociatedData;
use ast::source_atom_set::SourceAtomSetIndex;
use ast::source_location_accessor::SourceLocationAccessor;
use ast::type_id::NodeTypeIdAccessor;

/// Corresponds to js::BindingKind in m-c/js/src/vm/Scope.h.
#[derive(Debug, Clone, Copy, PartialEq)]
pub enum BindingKind {
    Var,
    Let,
    Const,
}

/// Information about a single binding in a JS script.
///
/// Corresponds to `js::BindingName` in m-c/js/src/vm/Scope.h.
#[derive(Debug)]
pub struct BindingName {
    pub name: SourceAtomSetIndex,
    pub is_closed_over: bool,
    pub is_top_level_function: bool,
}

impl BindingName {
    pub fn new(name: SourceAtomSetIndex, is_closed_over: bool) -> Self {
        Self {
            name,
            is_closed_over,
            is_top_level_function: false,
        }
    }

    pub fn new_top_level_function(name: SourceAtomSetIndex, is_closed_over: bool) -> Self {
        Self {
            name,
            is_closed_over,
            is_top_level_function: true,
        }
    }
}

/// Corresponds to the accessor part of `js::BindingIter` in
/// m-c/js/src/vm/Scope.h.
pub struct BindingIterItem<'a> {
    name: &'a BindingName,
    kind: BindingKind,
}

impl<'a> BindingIterItem<'a> {
    fn new(name: &'a BindingName, kind: BindingKind) -> Self {
        Self { name, kind }
    }

    pub fn name(&self) -> SourceAtomSetIndex {
        self.name.name
    }

    pub fn is_top_level_function(&self) -> bool {
        self.name.is_top_level_function
    }

    pub fn is_closed_over(&self) -> bool {
        self.name.is_closed_over
    }

    pub fn kind(&self) -> BindingKind {
        self.kind
    }
}

/// Accessor for both BindingName/Option<BindingName>.
pub trait MaybeBindingName {
    fn is_closed_over(&self) -> bool;
}
impl MaybeBindingName for BindingName {
    fn is_closed_over(&self) -> bool {
        self.is_closed_over
    }
}
impl MaybeBindingName for Option<BindingName> {
    fn is_closed_over(&self) -> bool {
        match self {
            Some(b) => b.is_closed_over,
            None => false,
        }
    }
}

#[derive(Debug)]
pub struct BaseScopeData<BindingItemT>
where
    BindingItemT: MaybeBindingName,
{
    /// Corresponds to `*Scope::Data.{length, trailingNames}.`
    /// The layout is defined by *ScopeData structs below, that has
    /// this struct.
    pub bindings: Vec<BindingItemT>,
    pub has_eval: bool,
    pub has_with: bool,
}

impl<BindingItemT> BaseScopeData<BindingItemT>
where
    BindingItemT: MaybeBindingName,
{
    pub fn new(bindings_count: usize) -> Self {
        Self {
            bindings: Vec::with_capacity(bindings_count),
            has_eval: false,
            has_with: false,
        }
    }

    pub fn is_all_bindings_closed_over(&self) -> bool {
        // `with` and direct `eval` can dynamically access any binding in this
        // scope.
        self.has_eval || self.has_with
    }

    /// Returns true if this scope needs to be allocated on heap as
    /// EnvironmentObject.
    pub fn needs_environment_object(&self) -> bool {
        // `with` and direct `eval` can dynamically access bindings in this
        // scope.
        if self.is_all_bindings_closed_over() {
            return true;
        }

        // If a binding in this scope is closed over by inner function,
        // it can be accessed when this frame isn't on the stack.
        for binding in &self.bindings {
            if binding.is_closed_over() {
                return true;
            }
        }

        false
    }
}

/// Bindings created in global environment.
///
/// Maps to js::GlobalScope::Data in m-c/js/src/vm/Scope.h.
#[derive(Debug)]
pub struct GlobalScopeData {
    /// Bindings are sorted by kind:
    ///
    /// * `base.bindings[0..let_start]` - `var`s
    /// * `base.bindings[let_start..const_start]` - `let`s
    /// * `base.bindings[const_start..]` - `const`s
    pub base: BaseScopeData<BindingName>,

    pub let_start: usize,
    pub const_start: usize,

    /// The global functions in this script.
    pub functions: Vec<ScriptStencilIndex>,
}

impl GlobalScopeData {
    pub fn new(
        var_count: usize,
        let_count: usize,
        const_count: usize,
        functions: Vec<ScriptStencilIndex>,
    ) -> Self {
        let capacity = var_count + let_count + const_count;

        Self {
            base: BaseScopeData::new(capacity),
            let_start: var_count,
            const_start: var_count + let_count,
            functions,
        }
    }

    pub fn iter<'a>(&'a self) -> GlobalBindingIter<'a> {
        GlobalBindingIter::new(self)
    }
}

/// Corresponds to the iteration part of js::BindingIter
/// in m-c/js/src/vm/Scope.h.
pub struct GlobalBindingIter<'a> {
    data: &'a GlobalScopeData,
    i: usize,
}

impl<'a> GlobalBindingIter<'a> {
    fn new(data: &'a GlobalScopeData) -> Self {
        Self { data, i: 0 }
    }
}

impl<'a> Iterator for GlobalBindingIter<'a> {
    type Item = BindingIterItem<'a>;

    fn next(&mut self) -> Option<BindingIterItem<'a>> {
        if self.i == self.data.base.bindings.len() {
            return None;
        }

        let kind = if self.i < self.data.let_start {
            BindingKind::Var
        } else if self.i < self.data.const_start {
            BindingKind::Let
        } else {
            BindingKind::Const
        };

        let name = &self.data.base.bindings[self.i];

        self.i += 1;

        Some(BindingIterItem::new(name, kind))
    }
}

/// Bindings created in var environment.
///
/// Maps to js::VarScope::Data in m-c/js/src/vm/Scope.h,
/// and the parameter for js::frontend::ScopeCreationData::create
/// in m-c/js/src/frontend/Stencil.h
#[derive(Debug)]
pub struct VarScopeData {
    /// All bindings are `var`.
    pub base: BaseScopeData<BindingName>,

    /// The first frame slot of this scope.
    ///
    /// Unlike VarScope::Data, this struct holds the first frame slot,
    /// instead of the next frame slot.
    ///
    /// This is because ScopeCreationData::create receives the first frame slot
    /// and VarScope::Data.nextFrameSlot is calculated there.
    pub first_frame_slot: FrameSlot,

    pub function_has_extensible_scope: bool,

    /// ScopeIndex of the enclosing scope.
    ///
    /// A parameter for ScopeCreationData::create.
    pub enclosing: ScopeIndex,
}

impl VarScopeData {
    pub fn new(
        var_count: usize,
        function_has_extensible_scope: bool,
        enclosing: ScopeIndex,
    ) -> Self {
        let capacity = var_count;

        Self {
            base: BaseScopeData::new(capacity),
            // Set to the correct value in EmitterScopeStack::enter_lexical.
            first_frame_slot: FrameSlot::new(0),
            function_has_extensible_scope,
            enclosing,
        }
    }
}

/// Bindings created in lexical environment.
///
/// Maps to js::LexicalScope::Data in m-c/js/src/vm/Scope.h,
/// and the parameter for js::frontend::ScopeCreationData::create
/// in m-c/js/src/frontend/Stencil.h
#[derive(Debug)]
pub struct LexicalScopeData {
    /// Bindings are sorted by kind:
    ///
    /// * `base.bindings[0..const_start]` - `let`s
    /// * `base.bindings[const_start..]` - `const`s
    pub base: BaseScopeData<BindingName>,

    pub const_start: usize,

    /// The first frame slot of this scope.
    ///
    /// Unlike LexicalScope::Data, this struct holds the first frame slot,
    /// instead of the next frame slot.
    ///
    /// This is because ScopeCreationData::create receives the first frame slot
    /// and LexicalScope::Data.nextFrameSlot is calculated there.
    pub first_frame_slot: FrameSlot,

    /// ScopeIndex of the enclosing scope.
    ///
    /// A parameter for ScopeCreationData::create.
    pub enclosing: ScopeIndex,

    /// Functions in this scope.
    pub functions: Vec<ScriptStencilIndex>,
}

impl LexicalScopeData {
    fn new(
        let_count: usize,
        const_count: usize,
        enclosing: ScopeIndex,
        functions: Vec<ScriptStencilIndex>,
    ) -> Self {
        let capacity = let_count + const_count;

        Self {
            base: BaseScopeData::new(capacity),
            const_start: let_count,
            // Set to the correct value in EmitterScopeStack::enter_lexical.
            first_frame_slot: FrameSlot::new(0),
            enclosing,
            functions,
        }
    }

    pub fn new_block(
        let_count: usize,
        const_count: usize,
        enclosing: ScopeIndex,
        functions: Vec<ScriptStencilIndex>,
    ) -> Self {
        Self::new(let_count, const_count, enclosing, functions)
    }

    pub fn new_named_lambda(enclosing: ScopeIndex) -> Self {
        Self::new(0, 1, enclosing, Vec::new())
    }

    pub fn new_function_lexical(
        let_count: usize,
        const_count: usize,
        enclosing: ScopeIndex,
    ) -> Self {
        Self::new(let_count, const_count, enclosing, Vec::new())
    }

    pub fn iter<'a>(&'a self) -> LexicalBindingIter<'a> {
        LexicalBindingIter::new(self)
    }
}

/// Corresponds to the iteration part of js::BindingIter
/// in m-c/js/src/vm/Scope.h.
pub struct LexicalBindingIter<'a> {
    data: &'a LexicalScopeData,
    i: usize,
}

impl<'a> LexicalBindingIter<'a> {
    fn new(data: &'a LexicalScopeData) -> Self {
        Self { data, i: 0 }
    }
}

impl<'a> Iterator for LexicalBindingIter<'a> {
    type Item = BindingIterItem<'a>;

    fn next(&mut self) -> Option<BindingIterItem<'a>> {
        if self.i == self.data.base.bindings.len() {
            return None;
        }

        let kind = if self.i < self.data.const_start {
            BindingKind::Let
        } else {
            BindingKind::Const
        };

        let name = &self.data.base.bindings[self.i];

        self.i += 1;

        Some(BindingIterItem::new(name, kind))
    }
}

/// Bindings created in function environment.
///
/// Maps to js::FunctionScope::Data in m-c/js/src/vm/Scope.h,
/// and the parameter for js::frontend::ScopeCreationData::create
/// in m-c/js/src/frontend/Stencil.h
#[derive(Debug)]
pub struct FunctionScopeData {
    /// Bindings are sorted by kind:
    ///
    /// * `base.bindings[0..non_positional_formal_start]` -
    ///    positional foparameters:
    ///      - single binding parameter with/without default
    ///      - single binding rest parameter
    /// * `base.bindings[non_positional_formal_start..var_start]` -
    ///    non positional parameters:
    ///      - destructuring parameter
    ///      - destructuring rest parameter
    /// * `base.bindings[var_start..]` - `var`s
    ///
    /// Given positional parameters range can have null slot for destructuring,
    /// use Vec of Option<BindingName>, instead of BindingName like others.
    pub base: BaseScopeData<Option<BindingName>>,

    pub has_parameter_exprs: bool,

    pub non_positional_formal_start: usize,
    pub var_start: usize,

    /// The first frame slot of this scope.
    ///
    /// Unlike FunctionScope::Data, this struct holds the first frame slot,
    /// instead of the next frame slot.
    ///
    /// This is because ScopeCreationData::create receives the first frame slot
    /// and FunctionScope::Data.nextFrameSlot is calculated there.
    pub first_frame_slot: FrameSlot,

    /// ScopeIndex of the enclosing scope.
    ///
    /// A parameter for ScopeCreationData::create.
    pub enclosing: ScopeIndex,

    pub function_index: ScriptStencilIndex,

    /// True if the function is an arrow function.
    pub is_arrow: bool,
}

impl FunctionScopeData {
    pub fn new(
        has_parameter_exprs: bool,
        positional_parameter_count: usize,
        non_positional_formal_start: usize,
        max_var_count: usize,
        enclosing: ScopeIndex,
        function_index: ScriptStencilIndex,
        is_arrow: bool,
    ) -> Self {
        let capacity = positional_parameter_count + non_positional_formal_start + max_var_count;

        Self {
            base: BaseScopeData::new(capacity),
            has_parameter_exprs,
            non_positional_formal_start: positional_parameter_count,
            var_start: positional_parameter_count + non_positional_formal_start,
            // Set to the correct value in EmitterScopeStack::enter_function.
            first_frame_slot: FrameSlot::new(0),
            enclosing,
            function_index,
            is_arrow,
        }
    }
}

#[derive(Debug)]
pub enum ScopeData {
    /// No scope should be generated, but this scope becomes an alias to
    /// enclosing scope. This is used, for example, when we see a function,
    /// and set aside a ScopeData for its lexical bindings, but upon
    /// reaching the end of the function body, we find that there were no
    /// lexical bindings and the spec actually says not to generate a Lexical
    /// Environment when this function is called.
    ///
    /// In other places, the spec does say to create a Lexical Environment, but
    /// it turns out it doesn't have any bindings in it and we can optimize it
    /// away.
    ///
    /// NOTE: Alias can be chained.
    Alias(ScopeIndex),

    Global(GlobalScopeData),
    Var(VarScopeData),
    Lexical(LexicalScopeData),
    Function(FunctionScopeData),
}

/// Index into ScopeDataList.scopes.
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct ScopeIndex {
    index: usize,
}
impl ScopeIndex {
    fn new(index: usize) -> Self {
        Self { index }
    }

    pub fn next(&self) -> Self {
        Self {
            index: self.index + 1,
        }
    }
}

impl From<ScopeIndex> for usize {
    fn from(index: ScopeIndex) -> usize {
        index.index
    }
}

/// A vector of scopes, incrementally populated during analysis.
/// The goal is to build a `Vec<ScopeData>`.
#[derive(Debug)]
pub struct ScopeDataList {
    /// Uses Option to allow `allocate()` and `populate()` to be called
    /// separately.
    scopes: Vec<Option<ScopeData>>,
}

impl ScopeDataList {
    pub fn new() -> Self {
        Self { scopes: Vec::new() }
    }

    pub fn push(&mut self, scope: ScopeData) -> ScopeIndex {
        let index = self.scopes.len();
        self.scopes.push(Some(scope));
        ScopeIndex::new(index)
    }

    pub fn allocate(&mut self) -> ScopeIndex {
        let index = self.scopes.len();
        self.scopes.push(None);
        ScopeIndex::new(index)
    }

    pub fn populate(&mut self, index: ScopeIndex, scope: ScopeData) {
        self.scopes[usize::from(index)].replace(scope);
    }

    fn get(&self, index: ScopeIndex) -> &ScopeData {
        self.scopes[usize::from(index)]
            .as_ref()
            .expect("Should be populated")
    }

    pub fn get_mut(&mut self, index: ScopeIndex) -> &mut ScopeData {
        self.scopes[usize::from(index)]
            .as_mut()
            .expect("Should be populated")
    }
}

impl From<ScopeDataList> for Vec<ScopeData> {
    fn from(list: ScopeDataList) -> Vec<ScopeData> {
        list.scopes
            .into_iter()
            .map(|g| g.expect("Should be populated"))
            .collect()
    }
}

/// The collection of all scope data associated with bindings and scopes in the
/// AST.
#[derive(Debug)]
pub struct ScopeDataMap {
    scopes: ScopeDataList,
    global: ScopeIndex,

    /// Associates every AST node that's a scope with an index into `scopes`.
    non_global: AssociatedData<ScopeIndex>,
}

impl ScopeDataMap {
    pub fn new(
        scopes: ScopeDataList,
        global: ScopeIndex,
        non_global: AssociatedData<ScopeIndex>,
    ) -> Self {
        Self {
            scopes,
            global,
            non_global,
        }
    }

    pub fn get_global_index(&self) -> ScopeIndex {
        self.global
    }

    pub fn get_global_at(&self, index: ScopeIndex) -> &GlobalScopeData {
        match self.scopes.get(index) {
            ScopeData::Global(scope) => scope,
            _ => panic!("Unexpected scope data for global"),
        }
    }

    pub fn get_index<NodeT>(&self, node: &NodeT) -> ScopeIndex
    where
        NodeT: SourceLocationAccessor + NodeTypeIdAccessor,
    {
        self.non_global
            .get(node)
            .expect("There should be a scope data associated")
            .clone()
    }

    pub fn get_lexical_at(&self, index: ScopeIndex) -> &LexicalScopeData {
        match self.scopes.get(index) {
            ScopeData::Lexical(scope) => scope,
            _ => panic!("Unexpected scope data for lexical"),
        }
    }

    pub fn get_lexical_at_mut(&mut self, index: ScopeIndex) -> &mut LexicalScopeData {
        match self.scopes.get_mut(index) {
            ScopeData::Lexical(scope) => scope,
            _ => panic!("Unexpected scope data for lexical"),
        }
    }

    pub fn get_function_at_mut(&mut self, index: ScopeIndex) -> &mut FunctionScopeData {
        match self.scopes.get_mut(index) {
            ScopeData::Function(scope) => scope,
            _ => panic!("Unexpected scope data for function"),
        }
    }

    pub fn is_alias(&mut self, index: ScopeIndex) -> bool {
        match self.scopes.get(index) {
            ScopeData::Alias(_) => true,
            _ => false,
        }
    }
}

impl From<ScopeDataMap> for Vec<ScopeData> {
    fn from(map: ScopeDataMap) -> Vec<ScopeData> {
        map.scopes.into()
    }
}
