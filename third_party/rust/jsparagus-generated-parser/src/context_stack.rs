use ast::source_atom_set::SourceAtomSetIndex;
use std::iter::{Skip, Take};
use std::slice::Iter;

// The kind of BindingIdentifier found while parsing.
//
// Given we don't yet have the context stack, at the point of finding
// a BindingIdentifier, we don't know what kind of binding it is.
// So it's marked as `Unknown`.
//
// Once the parser reaches the end of a declaration, bindings found in the
// range are marked as corresponding kind.
//
// This is a separate enum than `DeclarationKind` for the following reason:
//   * `DeclarationKind` is determined only when the parser reaches the end of
//     the entire context (also because we don't have context stack), not each
//     declaration
//   * As long as `BindingKind` is known for each binding, we can map it to
//     `DeclarationKind`
//   * `DeclarationKind::CatchParameter` and some others don't to be marked
//     this way, because `AstBuilder` knows where they are in the `bindings`
//     vector.
#[derive(Debug, PartialEq, Clone, Copy)]
pub enum BindingKind {
    // The initial state.
    // BindingIdentifier is found, and haven't yet marked as any kind.
    Unknown,

    // BindingIdentifier is inside VariableDeclaration.
    Var,

    // BindingIdentifier is the name of FunctionDeclaration.
    Function,

    // BindingIdentifier is the name of GeneratorDeclaration,
    // AsyncFunctionDeclaration, or AsyncGeneratorDeclaration.
    AsyncOrGenerator,

    // BindingIdentifier is inside LexicalDeclaration with let.
    Let,

    // BindingIdentifier is inside LexicalDeclaration with const.
    Const,

    // BindingIdentifier is the name of ClassDeclaration.
    Class,
}

#[derive(Debug, PartialEq, Clone, Copy)]
pub struct BindingInfo {
    pub name: SourceAtomSetIndex,
    // The offset of the BindingIdentifier in the source.
    pub offset: usize,
    pub kind: BindingKind,
}

#[derive(Debug, PartialEq, Clone, Copy)]
pub struct BindingsIndex {
    pub index: usize,
}

#[derive(Debug, PartialEq, Clone, Copy)]
pub enum ControlKind {
    Continue,
    Break,
}

#[derive(Debug, PartialEq, Clone, Copy)]
pub struct ControlInfo {
    pub label: Option<SourceAtomSetIndex>,
    // The offset of the nested control in the source.
    pub offset: usize,
    pub kind: ControlKind,
}

impl ControlInfo {
    pub fn new_continue(offset: usize, label: Option<SourceAtomSetIndex>) -> Self {
        Self {
            label,
            kind: ControlKind::Continue,
            offset,
        }
    }

    pub fn new_break(offset: usize, label: Option<SourceAtomSetIndex>) -> Self {
        Self {
            label,
            kind: ControlKind::Break,
            offset,
        }
    }
}

#[derive(Debug, PartialEq, Clone, Copy)]
pub struct BreakOrContinueIndex {
    pub index: usize,
}

impl BreakOrContinueIndex {
    pub fn new(index: usize) -> Self {
        Self { index }
    }
}

#[derive(Debug, PartialEq, Clone, Copy)]
pub enum LabelKind {
    Other,

    Function,

    Loop,

    LabelledLabel,
}

#[derive(Debug, PartialEq, Clone, Copy)]
pub struct LabelInfo {
    pub name: SourceAtomSetIndex,
    // The offset of the BindingIdentifier in the source.
    pub offset: usize,
    pub kind: LabelKind,
}

#[derive(Debug, PartialEq, Clone, Copy)]
pub struct LabelIndex {
    pub index: usize,
}

#[derive(Debug, PartialEq, Clone)]
pub struct ContextMetadata {
    // The stack of information about BindingIdentifier.
    //
    // When the parser found BindingIdentifier, the information (`name` and
    // `offset`) are noted to this vector, and when the parser determined what
    // kind of binding it is, `kind` is updated.
    //
    // The bindings are sorted by offset.
    //
    // When the parser reaches the end of a scope, all bindings declared within
    // that scope (not including nested scopes) are fed to an
    // EarlyErrorsContext to detect Early Errors.
    //
    // When leaving a context that is not one of script/module/function,
    // lexical items (`kind != BindingKind::Var) in the corresponding range
    // are removed, while non-lexical items (`kind == BindingKind::Var) are
    // left there, so that VariableDeclarations and labels are propagated to the
    // enclosing context.
    //
    // FIXME: Once the context stack gets implemented, this structure and
    //        related methods should be removed and each declaration should be
    //        fed directly to EarlyErrorsContext.
    bindings: Vec<BindingInfo>,

    // Track continues and breaks without the use of a context stack.
    //
    // FIXME: Once th context stack geets implmentd, this structure and
    //        related metehods should be removed, and each break/continue should be
    //        fed directly to EarlyErrorsContext.
    breaks_and_continues: Vec<ControlInfo>,

    labels: Vec<LabelInfo>,
}

impl ContextMetadata {
    pub fn new() -> Self {
        Self {
            bindings: Vec::new(),
            breaks_and_continues: Vec::new(),
            labels: Vec::new(),
        }
    }

    pub fn push_binding(&mut self, binding: BindingInfo) {
        self.bindings.push(binding);
    }

    pub fn last_binding(&mut self) -> Option<&BindingInfo> {
        self.bindings.last()
    }

    pub fn push_break_or_continue(&mut self, control: ControlInfo) {
        self.breaks_and_continues.push(control);
    }

    pub fn push_label(&mut self, label: LabelInfo) {
        self.labels.push(label);
    }

    // Update the binding kind of all names declared in a specific range of the
    // source (and not in any nested scope). This is used e.g. when the parser
    // reaches the end of a VariableStatement to mark all the variables as Var
    // bindings.
    //
    // It's necessary because the current parser only calls AstBuilder methods
    // at the end of each production, not at the beginning.
    //
    // Bindings inside `StatementList` must be marked using this method before
    // we reach the end of its scope.
    pub fn mark_binding_kind(&mut self, from: usize, to: Option<usize>, kind: BindingKind) {
        for info in self.bindings.iter_mut().rev() {
            if info.offset < from {
                break;
            }

            if to.is_none() || info.offset < to.unwrap() {
                info.kind = kind;
            }
        }
    }

    pub fn bindings_from(&self, index: BindingsIndex) -> Skip<Iter<'_, BindingInfo>> {
        self.bindings.iter().skip(index.index)
    }

    pub fn bindings_from_to(
        &self,
        from: BindingsIndex,
        to: BindingsIndex,
    ) -> Take<Skip<Iter<'_, BindingInfo>>> {
        self.bindings
            .iter()
            .skip(from.index)
            .take(to.index - from.index)
    }

    // Returns the index of the first label at/after `offset` source position.
    pub fn find_first_binding(&mut self, offset: usize) -> BindingsIndex {
        let mut i = self.bindings.len();
        for info in self.bindings.iter_mut().rev() {
            if info.offset < offset {
                break;
            }
            i -= 1;
        }
        BindingsIndex { index: i }
    }

    // Remove all bindings after `index`-th item.
    //
    // This should be called when leaving function/script/module.
    pub fn pop_bindings_from(&mut self, index: BindingsIndex) {
        self.bindings.truncate(index.index)
    }

    pub fn labels_from(&self, index: LabelIndex) -> Skip<Iter<'_, LabelInfo>> {
        self.labels.iter().skip(index.index)
    }

    // Update the label kind of a label declared in a specific range of the
    // source (and not in any nested scope). There should never be more than one.
    //
    // It's necessary because the current parser only calls AstBuilder methods
    // at the end of each production, not at the beginning.
    //
    // Labels inside `StatementList` must be marked using this method before
    // we reach the end of its scope.
    pub fn mark_label_kind_at_offset(&mut self, from: usize, kind: LabelKind) {
        let maybe_label = self.find_label_at_offset(from);
        if let Some(info) = maybe_label {
            info.kind = kind
        } else {
            panic!("Tried to mark a non-existant label");
        }
    }

    // Remove lexical bindings after `index`-th item,
    // while keeping var bindings.
    //
    // This should be called when leaving block.
    pub fn pop_lexical_bindings_from(&mut self, index: BindingsIndex) {
        let len = self.bindings.len();
        let mut i = index.index;

        while i < len && self.bindings[i].kind == BindingKind::Var {
            i += 1;
        }

        let mut j = i;
        while j < len {
            if self.bindings[j].kind == BindingKind::Var {
                self.bindings[i] = self.bindings[j];
                i += 1;
            }
            j += 1;
        }

        self.bindings.truncate(i)
    }

    // Returns the index of the first binding at/after `offset` source position.
    pub fn find_first_label(&mut self, offset: usize) -> LabelIndex {
        let mut i = self.labels.len();
        for info in self.labels.iter_mut().rev() {
            if info.offset < offset {
                break;
            }
            i -= 1;
        }
        LabelIndex { index: i }
    }

    // Remove all bindings after `index`-th item.
    //
    // This should be called when leaving function/script/module.
    pub fn pop_labels_from(&mut self, index: LabelIndex) {
        self.labels.truncate(index.index)
    }

    pub fn breaks_and_continues_from(
        &self,
        index: BreakOrContinueIndex,
    ) -> Skip<Iter<'_, ControlInfo>> {
        self.breaks_and_continues.iter().skip(index.index)
    }

    // Returns the index of the first break or continue at/after `offset` source position.
    pub fn find_first_break_or_continue(&mut self, offset: usize) -> BreakOrContinueIndex {
        let mut i = self.breaks_and_continues.len();
        for info in self.breaks_and_continues.iter_mut().rev() {
            if info.offset < offset {
                break;
            }
            i -= 1;
        }
        BreakOrContinueIndex::new(i)
    }

    pub fn pop_labelled_breaks_and_continues_from_index(
        &mut self,
        index: BreakOrContinueIndex,
        name: SourceAtomSetIndex,
    ) {
        let len = self.breaks_and_continues.len();
        let mut i = index.index;

        let mut j = i;
        while j < len {
            let label = self.breaks_and_continues[j].label;
            if label.is_none() || label.unwrap() != name {
                self.breaks_and_continues[i] = self.breaks_and_continues[j];
                i += 1;
            }
            j += 1;
        }

        self.breaks_and_continues.truncate(i)
    }

    pub fn pop_unlabelled_breaks_and_continues_from(&mut self, offset: usize) {
        let len = self.breaks_and_continues.len();
        let index = self.find_first_break_or_continue(offset);
        let mut i = index.index;

        while i < len && self.breaks_and_continues[i].label.is_some() {
            i += 1;
        }

        let mut j = i;
        while j < len {
            if self.breaks_and_continues[j].label.is_some() {
                self.breaks_and_continues[i] = self.breaks_and_continues[j];
                i += 1;
            }
            j += 1;
        }

        self.breaks_and_continues.truncate(i)
    }

    pub fn pop_unlabelled_breaks_from(&mut self, offset: usize) {
        let len = self.breaks_and_continues.len();
        let index = self.find_first_break_or_continue(offset);
        let mut i = index.index;

        while i < len && self.breaks_and_continues[i].label.is_some() {
            i += 1;
        }

        let mut j = i;
        while j < len {
            if self.breaks_and_continues[j].label.is_some()
                || self.breaks_and_continues[j].kind == ControlKind::Continue
            {
                self.breaks_and_continues[i] = self.breaks_and_continues[j];
                i += 1;
            }
            j += 1;
        }

        self.breaks_and_continues.truncate(i)
    }

    pub fn find_break_or_continue_at(&self, index: BreakOrContinueIndex) -> Option<&ControlInfo> {
        self.breaks_and_continues.get(index.index)
    }

    pub fn find_label_index_at_offset(&self, offset: usize) -> Option<LabelIndex> {
        let index = self.labels.iter().position(|info| info.offset == offset);
        index.map(|index| LabelIndex { index })
    }

    pub fn find_label_at_offset(&mut self, offset: usize) -> Option<&mut LabelInfo> {
        self.labels.iter_mut().find(|info| info.offset == offset)
    }
}
