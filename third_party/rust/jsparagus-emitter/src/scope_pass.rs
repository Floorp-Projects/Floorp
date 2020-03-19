//! Collect bindings and generate scope data from AST.
//! The information is used while emitting bytecode.

use crate::scope::{
    BindingName, GlobalScopeData, LexicalScopeData, ScopeData, ScopeDataList, ScopeDataMap,
    ScopeIndex,
};
use ast::source_atom_set::SourceAtomSetIndex;
use ast::{associated_data::AssociatedData, types::*, visit::Pass};
use indexmap::set::IndexSet;
use std::marker::PhantomData;

/// The kind of items inside the result of VarScopedDeclarations.
///
/// This enum isn't actually used, but just for simplifying comment in
/// ScopeKind.
#[allow(dead_code)]
#[derive(Debug, Clone, PartialEq)]
enum VarScopedDeclarationsItemKind {
    /// Static Semantics: VarScopedDeclarations
    /// https://tc39.es/ecma262/#sec-variable-statement-static-semantics-varscopeddeclarations
    ///
    /// VariableDeclarationList : VariableDeclaration
    ///
    /// 1. Return a new List containing VariableDeclaration.
    ///
    /// VariableDeclarationList : VariableDeclarationList, VariableDeclaration
    ///
    /// 1. Let declarations be VarScopedDeclarations of VariableDeclarationList.
    /// 2. Append VariableDeclaration to declarations.
    /// 3. Return declarations.
    VariableDeclaration,

    /// Static Semantics: VarScopedDeclarations
    /// https://tc39.es/ecma262/#sec-for-in-and-for-of-statements-static-semantics-varscopeddeclarations
    ///
    /// IterationStatement :
    ///   for ( var ForBinding in Expression ) Statement
    ///   for ( var ForBinding of AssignmentExpression ) Statement
    ///   for await ( var ForBinding of AssignmentExpression ) Statement
    ///
    /// 1. Let declarations be a List containing ForBinding.
    /// 2. Append to declarations the elements of the VarScopedDeclarations of
    ///    Statement.
    /// 3. Return declarations.
    ForBinding,

    /// Static Semantics: VarScopedDeclarations
    /// https://tc39.es/ecma262/#sec-function-definitions-static-semantics-varscopeddeclarations
    ///
    /// FunctionStatementList : StatementList
    ///
    /// 1. Return the TopLevelVarScopedDeclarations of StatementList.

    /// Static Semantics: VarScopedDeclarations
    /// https://tc39.es/ecma262/#sec-scripts-static-semantics-varscopeddeclarations
    ///
    /// ScriptBody : StatementList
    ///
    /// 1. Return TopLevelVarScopedDeclarations of StatementList.

    /// Static Semantics: TopLevelVarScopedDeclarations
    /// https://tc39.es/ecma262/#sec-block-static-semantics-toplevelvarscopeddeclarations
    ///
    /// StatementListItem : Declaration
    ///
    /// 1. If Declaration is Declaration : HoistableDeclaration, then
    ///   a. Let declaration be DeclarationPart of HoistableDeclaration.
    ///   b. Return « declaration ».
    /// 2. Return a new empty List.

    /// Static Semantics: DeclarationPart
    /// https://tc39.es/ecma262/#sec-static-semantics-declarationpart
    ///
    /// HoistableDeclaration : FunctionDeclaration
    ///
    /// 1. Return FunctionDeclaration.
    FunctionDeclaration,

    /// HoistableDeclaration : GeneratorDeclaration
    ///
    /// 1. Return GeneratorDeclaration.
    GeneratorDeclaration,

    /// HoistableDeclaration : AsyncFunctionDeclaration
    ///
    /// 1. Return AsyncFunctionDeclaration.
    AsyncFunctionDeclaration,

    /// HoistableDeclaration : AsyncGeneratorDeclaration
    ///
    /// 1. Return AsyncGeneratorDeclaration.
    AsyncGeneratorDeclaration,

    /// Static Semantics: TopLevelVarScopedDeclarations
    /// https://tc39.es/ecma262/#sec-labelled-statements-static-semantics-toplevelvarscopeddeclarations
    ///
    /// LabelledItem : FunctionDeclaration
    ///
    /// 1. Return a new List containing FunctionDeclaration.
    /* FunctionDeclaration */

    /// Annex B Initializers in ForIn Statement Heads
    /// https://tc39.es/ecma262/#sec-initializers-in-forin-statement-heads
    ///
    /// IterationStatement :
    ///   for ( var BindingIdentifier Initializer in Expression ) Statement
    ///
    /// 1. Let declarations be a List containing BindingIdentifier.
    /// 2. Append to declarations the elements of the VarScopedDeclarations of
    ///    Statement.
    /// 3. Return declarations.
    BindingIdentifier,
}

/// The kind of items inside the result of LexicallyScopedDeclarations.
///
/// This enum isn't actually used, but just for simplifying comment in
/// ScopeKind.
#[allow(dead_code)]
#[derive(Debug, Clone, PartialEq)]
enum LexicallyScopedDeclarations {
    /// Static Semantics: LexicallyScopedDeclarations
    /// https://tc39.es/ecma262/#sec-block-static-semantics-lexicallyscopeddeclarations
    ///
    /// StatementListItem : Declaration
    ///
    /// 1. Return a new List containing DeclarationPart of Declaration.

    /// Static Semantics: DeclarationPart
    /// https://tc39.es/ecma262/#sec-static-semantics-declarationpart
    ///
    /// HoistableDeclaration : FunctionDeclaration
    ///
    /// 1. Return FunctionDeclaration.
    FunctionDeclaration,

    /// HoistableDeclaration : GeneratorDeclaration
    ///
    /// 1. Return GeneratorDeclaration.
    GeneratorDeclaration,

    /// HoistableDeclaration : AsyncFunctionDeclaration
    ///
    /// 1. Return AsyncFunctionDeclaration.
    AsyncFunctionDeclaration,

    /// HoistableDeclaration : AsyncGeneratorDeclaration
    ///
    /// 1. Return AsyncGeneratorDeclaration.
    AsyncGeneratorDeclaration,

    /// Declaration : ClassDeclaration
    ///
    /// 1. Return ClassDeclaration.
    ClassDeclaration,

    /// Declaration : LexicalDeclaration
    ///
    /// 1. Return LexicalDeclaration.
    LexicalDeclarationWithLet,
    LexicalDeclarationWithConst,

    /// Static Semantics: LexicallyScopedDeclarations
    /// https://tc39.es/ecma262/#sec-labelled-statements-static-semantics-lexicallyscopeddeclarations
    ///
    /// LabelledItem : FunctionDeclaration
    ///
    /// 1. Return a new List containing FunctionDeclaration.
    /* FunctionDeclaration */

    /// Static Semantics: LexicallyScopedDeclarations
    /// https://tc39.es/ecma262/#sec-function-definitions-static-semantics-lexicallyscopeddeclarations
    ///
    /// FunctionStatementList : StatementList
    ///
    /// 1. Return the TopLevelLexicallyScopedDeclarations of StatementList.

    /// Static Semantics: LexicallyScopedDeclarations
    /// https://tc39.es/ecma262/#sec-scripts-static-semantics-lexicallyscopeddeclarations
    ///
    /// ScriptBody : StatementList
    ///
    /// 1. Return TopLevelLexicallyScopedDeclarations of StatementList.

    /// Static Semantics: TopLevelLexicallyScopedDeclarations
    /// https://tc39.es/ecma262/#sec-block-static-semantics-toplevellexicallyscopeddeclarations
    ///
    /// StatementListItem : Declaration
    ///
    /// 1. If Declaration is Declaration : HoistableDeclaration, then
    ///   a. Return « ».
    /// 2. Return a new List containing Declaration.

    /// Static Semantics: LexicallyScopedDeclarations
    /// https://tc39.es/ecma262/#sec-exports-static-semantics-lexicallyscopeddeclarations
    ///
    /// ExportDeclaration : export Declaration
    ///
    /// 1. Return a new List containing DeclarationPart of Declaration.

    /// ExportDeclaration : export default HoistableDeclaration
    ///
    /// 1. Return a new List containing DeclarationPart of HoistableDeclaration.

    /// ExportDeclaration : export default ClassDeclaration
    ///
    /// 1. Return a new List containing ClassDeclaration.
    /* ClassDeclaration */

    /// ExportDeclaration : export default AssignmentExpression ;
    ///
    /// 1. Return a new List containing this ExportDeclaration.
    ExportDeclarationWithAssignmentExpression,
}

/// Items on the Context.scope_stack.
/// Specifies the kind of BindingIdentifier.
///
/// This includes only BindingIdentifier that appears inside list or recursive
/// structure.
///
/// BindingIdentifier that appears only once for a structure
/// (e.g. Function.name) should be handled immediately, without using
/// Context.scope_stack.
#[derive(Debug, Clone, PartialEq)]
enum ScopeKind {
    /// VarScopedDeclarationsItemKind::VariableDeclaration
    /// VarScopedDeclarationsItemKind::ForBinding
    /// VarScopedDeclarationsItemKind::BindingIdentifier
    Var,

    /// LexicallyScopedDeclarations::LexicalDeclarationWithLet
    Let,

    /// LexicallyScopedDeclarations::LexicalDeclarationWithConst
    Const,

    #[allow(dead_code)]
    FormalParameter,

    #[allow(dead_code)]
    CatchParameter,

    /// LexicallyScopedDeclarations::ExportDeclarationWithAssignmentExpression
    #[allow(dead_code)]
    Export,

    /// VarScopedDeclarationsItemKind::FunctionDeclaration
    /// VarScopedDeclarationsItemKind::GeneratorDeclaration
    /// VarScopedDeclarationsItemKind::AsyncFunctionDeclaration
    /// VarScopedDeclarationsItemKind::AsyncGeneratorDeclaration
    #[allow(dead_code)]
    ScriptBodyStatementList,
    #[allow(dead_code)]
    FunctionStatementList,

    /// LexicallyScopedDeclarations::FunctionDeclaration
    /// LexicallyScopedDeclarations::GeneratorDeclaration
    /// LexicallyScopedDeclarations::AsyncFunctionDeclaration
    /// LexicallyScopedDeclarations::AsyncGeneratorDeclaration
    /// LexicallyScopedDeclarations::ClassDeclaration
    #[allow(dead_code)]
    BlockStatementList,
}

/// Variables declared/used in GlobalDeclarationInstantiation.
///
/// Collected while visiting Script's fields, and used when leaving Script.
#[derive(Debug)]
struct GlobalContext {
    /// Runtime Semantics: GlobalDeclarationInstantiation ( script, env )
    /// https://tc39.es/ecma262/#sec-globaldeclarationinstantiation
    ///
    /// Step 9. Let declaredFunctionNames be a new empty List.
    declared_function_names: IndexSet<SourceAtomSetIndex>,

    /// Step 11. Let declaredVarNames be a new empty List.
    /// NOTE: This is slightly different than the spec that this can contain
    ///       names in declaredFunctionNames.
    ///       The duplication should be filtered before the use.
    declared_var_names: IndexSet<SourceAtomSetIndex>,

    /// Step 15. Let lexDeclarations be the LexicallyScopedDeclarations of
    ///          script.
    let_names: Vec<SourceAtomSetIndex>,
    const_names: Vec<SourceAtomSetIndex>,

    scope_index: ScopeIndex,
    // FIXME: Step 8. Let functionsToInitialize be a new empty List.
}

impl GlobalContext {
    fn new(scope_index: ScopeIndex) -> Self {
        Self {
            declared_function_names: IndexSet::new(),
            declared_var_names: IndexSet::new(),
            let_names: Vec::new(),
            const_names: Vec::new(),
            scope_index,
        }
    }

    fn declare_var<'alloc>(&mut self, binding: &BindingIdentifier) {
        // Runtime Semantics: GlobalDeclarationInstantiation ( script, env )
        // https://tc39.es/ecma262/#sec-globaldeclarationinstantiation
        //
        // Step 7. Let varDeclarations be the VarScopedDeclarations of script.
        //
        // Step 12. For each d in varDeclarations, do
        // Step 12.a. If d is a VariableDeclaration, a ForBinding, or a
        //            BindingIdentifier, then
        // Step 12.a.i. For each String vn in the BoundNames of d, do
        // (implicit)

        // Step 12.a.i.i If vn is not an element of declaredFunctionNames, then
        // (done in remove_function_names_from_var_names)

        // Step 12.a.i.1.a. Let vnDefinable be ? envRec.CanDeclareGlobalVar(vn).
        // Step 12.a.i.1.b. If vnDefinable is false, throw a TypeError
        //                  exception.
        // (done in runtime)

        // Step 12.a.i.1.c. If vn is not an element of declaredVarNames, then
        // Step 12.a.i.1.a.i. Append vn to declaredVarNames.
        self.declared_var_names.insert(binding.name.value);
    }

    fn declare_let<'alloc>(&mut self, binding: &BindingIdentifier) {
        // Runtime Semantics: GlobalDeclarationInstantiation ( script, env )
        // https://tc39.es/ecma262/#sec-globaldeclarationinstantiation
        //
        // Step 15. Let lexDeclarations be the LexicallyScopedDeclarations of
        //          script.
        self.let_names.push(binding.name.value);
    }

    fn declare_const<'alloc>(&mut self, binding: &BindingIdentifier) {
        // Runtime Semantics: GlobalDeclarationInstantiation ( script, env )
        // https://tc39.es/ecma262/#sec-globaldeclarationinstantiation
        //
        // Step 15. Let lexDeclarations be the LexicallyScopedDeclarations of
        //          script.
        self.const_names.push(binding.name.value);
    }

    fn declare_function<'alloc>(&mut self, fun: &Function) {
        // Step 10. For each d in varDeclarations, in reverse list order, do
        // Step 10.a. If d is neither a VariableDeclaration nor a ForBinding
        //            nor a BindingIdentifier, then
        // (implicit)

        // Step 10.a.i. Assert: d is either a FunctionDeclaration,
        //              a GeneratorDeclaration, an AsyncFunctionDeclaration,
        //              or an AsyncGeneratorDeclaration.

        // Step 10.a.ii. NOTE: If there are multiple function declarations for
        //               the same name, the last declaration is used.

        // Step 10.a.iii. Let fn be the sole element of the BoundNames of d.
        let fn_ = if let Some(ref name) = fun.name {
            name.name.value
        } else {
            panic!("FunctionDeclaration should have name");
        };

        // Step 10.a.iv. If fn is not an element of declaredFunctionNames, then
        //
        // NOTE: Instead of iterating in reverse list oder, we iterate in
        // normal order and overwrite existing item.

        // Steps 10.a.iv.1-2.
        // (done in runtime)

        // Step 10.a.iv.3. Append fn to declaredFunctionNames.
        self.declared_function_names.insert(fn_);

        // Step 10.a.iv.4. Insert d as the first element of
        //                 functionsToInitialize.
        //
        // Cannot do this given lifetime is unknown.
        // (we can modify visitor to pass lifetime)
        //self.functions_to_initialize.insert(fn_, fun);
    }

    fn remove_function_names_from_var_names(&mut self) {
        // Runtime Semantics: GlobalDeclarationInstantiation ( script, env )
        // https://tc39.es/ecma262/#sec-globaldeclarationinstantiation
        //
        // Step 12.a.i.i If vn is not an element of declaredFunctionNames, then
        //
        // To avoid doing 2-pass, we note all var names, and filter function
        // names out after visiting all of them.
        for n in &self.declared_function_names {
            self.declared_var_names.remove(n);
        }
    }

    fn into_scope_data(self) -> ScopeData {
        let mut data = GlobalScopeData::new(
            self.declared_var_names.len() + self.declared_function_names.len(),
            self.let_names.len(),
            self.const_names.len(),
        );

        // Runtime Semantics: GlobalDeclarationInstantiation ( script, env )
        // https://tc39.es/ecma262/#sec-globaldeclarationinstantiation
        //
        // NOTE: Steps are reordered to match the order of binding in runtime.
        //
        // Step 18. For each String vn in declaredVarNames, in list order, do
        for n in &self.declared_var_names {
            // 18.a. Perform ? envRec.CreateGlobalVarBinding(vn, false).
            data.bindings.push(BindingName::new(*n))
        }

        // Step 17. For each Parse Node f in functionsToInitialize, do
        for n in &self.declared_function_names {
            // Step 17.a. Let fn be the sole element of the BoundNames of f.
            // Step 17.b. Let fo be InstantiateFunctionObject of f with
            //            argument env.
            // Step 17.c. Perform
            //            ? envRec.CreateGlobalFunctionBinding(fn, fo, false).
            //
            // FIXME: for Annex B functions, use `new`.
            data.bindings.push(BindingName::new_top_level_function(*n));
        }

        // Step 15. Let lexDeclarations be the LexicallyScopedDeclarations of
        //          script.
        // Step 16. For each element d in lexDeclarations, do
        // Step 16.b. For each element dn of the BoundNames of d, do
        for n in &self.let_names {
            // Step 16.b.ii. Else,
            // Step 16.b.ii.1. Perform ? envRec.CreateMutableBinding(dn, false).
            data.bindings.push(BindingName::new(*n))
        }
        for n in &self.const_names {
            // Step 16.b.i. If IsConstantDeclaration of d is true, then
            // Step 16.b.i.1. Perform ? envRec.CreateImmutableBinding(dn, true).
            data.bindings.push(BindingName::new(*n))
        }

        ScopeData::Global(data)
    }
}

/// Variables declared/used in BlockDeclarationInstantiation
///
/// Collected while visiting Block's fields, and used when leaving Block.
#[derive(Debug)]
struct BlockContext {
    /// Step 3. Let declarations be the LexicallyScopedDeclarations of code.
    let_names: Vec<SourceAtomSetIndex>,
    fun_names: Vec<SourceAtomSetIndex>,
    const_names: Vec<SourceAtomSetIndex>,

    /// Scope associated to this context.
    scope_index: ScopeIndex,
}

impl BlockContext {
    fn new(scope_index: ScopeIndex) -> Self {
        Self {
            let_names: Vec::new(),
            fun_names: Vec::new(),
            const_names: Vec::new(),
            scope_index,
        }
    }

    fn declare_let<'alloc>(&mut self, binding: &BindingIdentifier) {
        // https://tc39.es/ecma262/#sec-blockdeclarationinstantiation
        // Runtime Semantics: BlockDeclarationInstantiation ( code, env )
        //
        // Step 3. Let declarations be the LexicallyScopedDeclarations of code.
        self.let_names.push(binding.name.value);
    }

    fn declare_const<'alloc>(&mut self, binding: &BindingIdentifier) {
        // https://tc39.es/ecma262/#sec-blockdeclarationinstantiation
        // Runtime Semantics: BlockDeclarationInstantiation ( code, env )
        //
        // Step 3. Let declarations be the LexicallyScopedDeclarations of code.
        self.const_names.push(binding.name.value);
    }

    fn declare_function<'alloc>(&mut self, fun: &Function) {
        // https://tc39.es/ecma262/#sec-blockdeclarationinstantiation
        // Runtime Semantics: BlockDeclarationInstantiation ( code, env )
        //
        // Step 3. Let declarations be the LexicallyScopedDeclarations of code.

        let fn_ = if let Some(ref name) = fun.name {
            name.name.value
        } else {
            panic!("FunctionDeclaration should have name");
        };

        self.fun_names.push(fn_);
    }

    fn into_scope_data(self, enclosing: ScopeIndex) -> ScopeData {
        // FIXME: Before this, perform Annex B for functions.

        let mut data = LexicalScopeData::new(
            self.let_names.len() + self.fun_names.len(),
            self.const_names.len(),
            enclosing,
        );

        // https://tc39.es/ecma262/#sec-blockdeclarationinstantiation
        // Runtime Semantics: BlockDeclarationInstantiation ( code, env )
        //
        // Step 1. Let envRec be env's EnvironmentRecord.
        // Step 2. Assert: envRec is a declarative Environment Record.
        // (implicit)

        // Step 4. For each element d in declarations, do
        // Step 4.a. For each element dn of the BoundNames of d, do
        for n in &self.let_names {
            // Step 4.a.ii. Else,
            // Step 4.a.ii.1. Perform ! envRec.CreateMutableBinding(dn, false).
            data.bindings.push(BindingName::new(*n))
        }
        for n in &self.fun_names {
            // Step 4.b. If d is a FunctionDeclaration, a GeneratorDeclaration,
            //           an AsyncFunctionDeclaration,
            //           or an AsyncGeneratorDeclaration, then
            // Step 4.b.i. Let fn be the sole element of the BoundNames of d.
            // Step 4.b.ii. Let fo be InstantiateFunctionObject of d with
            //              argument env.
            // Step 4.b.iii. Perform envRec.InitializeBinding(fn, fo).
            data.bindings.push(BindingName::new(*n))
        }
        for n in &self.const_names {
            // Step 4.a.i. If IsConstantDeclaration of d is true, then
            // Step 4.a.i.1. Perform ! envRec.CreateImmutableBinding(dn, true).
            data.bindings.push(BindingName::new(*n))
        }

        ScopeData::Lexical(data)
    }
}

#[derive(Debug)]
enum Context {
    Global(GlobalContext),
    Block(BlockContext),
}

impl Context {
    fn get_scope_index(&self) -> ScopeIndex {
        match self {
            Context::Global(context) => context.scope_index,
            Context::Block(context) => context.scope_index,
        }
    }
}

/// Tracks what kind of binding the BindingIdentifier node corresponds to.
#[derive(Debug)]
struct ScopeKindStack {
    stack: Vec<ScopeKind>,
}

impl ScopeKindStack {
    fn new() -> Self {
        Self { stack: Vec::new() }
    }

    fn innermost<'a>(&'a self) -> &'a ScopeKind {
        self.stack
            .last()
            .expect("There should be at least one scope on the stack")
    }

    fn push(&mut self, kind: ScopeKind) {
        self.stack.push(kind)
    }

    fn pop(&mut self, kind: ScopeKind) {
        match self.stack.pop() {
            Some(k) if k == kind => {}
            _ => panic!("unmatching scope kind"),
        }
    }

    fn is_empty(&self) -> bool {
        self.stack.len() == 0
    }
}

/// The stack of context for creating binding into.
#[derive(Debug)]
struct ContextStack {
    stack: Vec<Context>,
}

impl ContextStack {
    fn new() -> Self {
        Self { stack: Vec::new() }
    }

    fn innermost_var<'a>(&'a mut self) -> &'a mut Context {
        for context in self.stack.iter_mut().rev() {
            match context {
                Context::Global(_) => return context,
                _ => {}
            }
        }

        panic!("There should be at least one scope on the stack");
    }

    fn innermost_lexical<'a>(&'a mut self) -> &'a mut Context {
        self.stack
            .last_mut()
            .expect("There should be at least one scope on the stack")
    }

    fn current_scope_index(&self) -> ScopeIndex {
        self.stack
            .last()
            .expect("There should be at least one scope on the stack")
            .get_scope_index()
    }

    fn push_global(&mut self, context: GlobalContext) {
        self.stack.push(Context::Global(context))
    }

    fn pop_global(&mut self) -> GlobalContext {
        match self.stack.pop() {
            Some(Context::Global(context)) => context,
            _ => panic!("unmatching context"),
        }
    }

    fn push_block(&mut self, context: BlockContext) {
        self.stack.push(Context::Block(context))
    }

    fn pop_block(&mut self) -> BlockContext {
        match self.stack.pop() {
            Some(Context::Block(context)) => context,
            _ => panic!("unmatching context"),
        }
    }
}

/// Visit all nodes in the AST, and create a scope data.
#[derive(Debug)]
struct ScopePass<'alloc> {
    scope_kind_stack: ScopeKindStack,
    context_stack: ContextStack,
    scopes: ScopeDataList,

    /// The global scope information.
    /// Using `Option` to make this field populated later.
    global: Option<ScopeIndex>,

    /// The map from non-global AST node to scope information.
    non_global: AssociatedData<ScopeIndex>,

    phantom: PhantomData<&'alloc ()>,
}

impl<'alloc> ScopePass<'alloc> {
    fn new() -> Self {
        Self {
            scope_kind_stack: ScopeKindStack::new(),
            context_stack: ContextStack::new(),
            scopes: ScopeDataList::new(),
            global: None,
            non_global: AssociatedData::new(),
            phantom: PhantomData,
        }
    }
}

impl<'alloc> From<ScopePass<'alloc>> for ScopeDataMap {
    fn from(pass: ScopePass<'alloc>) -> ScopeDataMap {
        ScopeDataMap::new(
            pass.scopes,
            pass.global.expect("There should be global scope data"),
            pass.non_global,
        )
    }
}

impl<'alloc> Pass<'alloc> for ScopePass<'alloc> {
    fn enter_script(&mut self, _ast: &mut Script<'alloc>) {
        // SetRealmGlobalObject ( realmRec, globalObj, thisValue )
        // https://tc39.es/ecma262/#sec-setrealmglobalobject
        //
        // Steps 1-4, 7.
        // (done in runtime)

        // Step 5. Let newGlobalEnv be
        //         NewGlobalEnvironment(globalObj, thisValue).
        let index = self.scopes.allocate();
        let context = GlobalContext::new(index);
        self.global = Some(index);

        // Step 6. Set realmRec.[[GlobalEnv]] to newGlobalEnv.
        // (implicit)

        // ScriptEvaluation ( scriptRecord )
        // https://tc39.es/ecma262/#sec-runtime-semantics-scriptevaluation
        //
        // Step 1. Let globalEnv be scriptRecord.[[Realm]].[[GlobalEnv]].
        // (implicit)

        // Step 2. Let scriptContext be a new ECMAScript code execution context.
        // (implicit)

        // Steps 3-5.
        // (done in runtime)

        // Step 6. Set the VariableEnvironment of scriptContext to globalEnv.
        // Step 7. Set the LexicalEnvironment of scriptContext to globalEnv.
        self.context_stack.push_global(context);

        // Steps 8-17.
        // (done in runtime)
    }

    fn leave_script(&mut self, _ast: &mut Script<'alloc>) {
        let mut context = self.context_stack.pop_global();

        // Runtime Semantics: GlobalDeclarationInstantiation ( script, env )
        // https://tc39.es/ecma262/#sec-globaldeclarationinstantiation
        //
        // Steps 3-6.
        // (done in runtime)

        // Step 12.a.i.i If vn is not an element of declaredFunctionNames, then
        context.remove_function_names_from_var_names();

        // Step 14. NOTE: Annex B.3.3.2 adds additional steps at this point.
        //
        // FIXME: Implement

        // Steps 15-18.
        self.scopes
            .populate(context.scope_index, context.into_scope_data());
    }

    fn enter_enum_statement_variant_block_statement(&mut self, block: &mut Block<'alloc>) {
        // https://tc39.es/ecma262/#sec-block-runtime-semantics-evaluation
        // Runtime Semantics: Evaluation
        //
        // Block : { StatementList }
        //
        // Step 1. Let oldEnv be the running execution context's
        //         LexicalEnvironment.
        // (implicit)

        // Step 2. Let blockEnv be NewDeclarativeEnvironment(oldEnv).
        let index = self.scopes.allocate();
        let context = BlockContext::new(index);
        self.non_global.insert(block, index);

        // Step 3. Perform
        //         BlockDeclarationInstantiation(StatementList, blockEnv).
        // (done in leave_enum_statement_variant_block_statement)

        // Step 4. Set the running execution context's LexicalEnvironment to
        //         blockEnv.
        self.context_stack.push_block(context);

        // Step 5. Let blockValue be the result of evaluating StatementList.
        // (done in runtime)
    }

    fn leave_enum_statement_variant_block_statement(&mut self, _block: &mut Block<'alloc>) {
        let context = self.context_stack.pop_block();
        let enclosing = self.context_stack.current_scope_index();

        // https://tc39.es/ecma262/#sec-block-runtime-semantics-evaluation
        // Runtime Semantics: Evaluation
        //
        // Block : { StatementList }
        //
        // Step 3. Perform
        //         BlockDeclarationInstantiation(StatementList, blockEnv).
        self.scopes
            .populate(context.scope_index, context.into_scope_data(enclosing));

        // Step 6. Set the running execution context's LexicalEnvironment to
        //         oldEnv.

        // Step 7. Return blockValue.
        // (implicit)
    }

    fn enter_variable_declaration(&mut self, ast: &mut VariableDeclaration<'alloc>) {
        match ast.kind {
            VariableDeclarationKind::Var { .. } => {
                self.scope_kind_stack.push(ScopeKind::Var);
            }
            VariableDeclarationKind::Let { .. } => {
                self.scope_kind_stack.push(ScopeKind::Let);
            }
            VariableDeclarationKind::Const { .. } => {
                self.scope_kind_stack.push(ScopeKind::Const);
            }
        }
    }

    fn leave_variable_declaration(&mut self, ast: &mut VariableDeclaration<'alloc>) {
        match ast.kind {
            VariableDeclarationKind::Var { .. } => {
                self.scope_kind_stack.pop(ScopeKind::Var);
            }
            VariableDeclarationKind::Let { .. } => {
                self.scope_kind_stack.pop(ScopeKind::Let);
            }
            VariableDeclarationKind::Const { .. } => {
                self.scope_kind_stack.pop(ScopeKind::Const);
            }
        }
    }

    fn visit_binding_identifier(&mut self, ast: &mut BindingIdentifier) {
        // NOTE: The following should be handled at the parent node,
        // without visiting BindingIdentifier field:
        //   * Function.name
        //   * ClassExpression.name
        //   * ClassDeclaration.name
        //   * Import.default_binding
        //   * ImportNamespace.default_binding
        //   * ImportNamespace.namespace_binding
        //   * ImportSpecifier.binding

        if self.scope_kind_stack.is_empty() {
            // FIXME
            // Do nothing for unsupported case.
            // Emitter will return NotImplemented anyway.
            return;
        }

        match self.scope_kind_stack.innermost() {
            ScopeKind::Var => match self.context_stack.innermost_var() {
                Context::Global(ref mut context) => context.declare_var(ast),
                _ => panic!("unexpected var context"),
            },
            ScopeKind::Let => match self.context_stack.innermost_lexical() {
                Context::Global(ref mut context) => context.declare_let(ast),
                Context::Block(ref mut context) => context.declare_let(ast),
            },
            ScopeKind::Const => match self.context_stack.innermost_lexical() {
                Context::Global(ref mut context) => context.declare_const(ast),
                Context::Block(ref mut context) => context.declare_const(ast),
            },
            _ => panic!("Not implemeneted"),
        }
    }

    fn enter_enum_statement_variant_function_declaration(&mut self, ast: &mut Function<'alloc>) {
        match self.context_stack.innermost_lexical() {
            Context::Global(ref mut context) => context.declare_function(ast),
            Context::Block(ref mut context) => context.declare_function(ast),
        }
    }
}

/// Visit all nodes in the AST, and create a scope data.
pub fn generate_scope_data<'alloc>(ast: &mut Program<'alloc>) -> ScopeDataMap {
    let mut scope_pass = ScopePass::new();
    scope_pass.visit_program(ast);
    scope_pass.into()
}
