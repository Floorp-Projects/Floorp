use crate::parser_tables_generated::TerminalId;
use crate::DeclarationKind;
use crate::ParseError;
use crate::Token;
use ast::arena;
use ast::source_atom_set::{CommonSourceAtomSetIndices, SourceAtomSet, SourceAtomSetIndex};
use std::collections::HashMap;

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

pub struct BreakOrContinueIndex {
    pub index: usize,
}

impl BreakOrContinueIndex {
    pub fn new(index: usize) -> Self {
        Self { index }
    }
}

#[derive(Clone, Copy, Debug, PartialEq)]
struct DeclarationInfo {
    kind: DeclarationKind,
    offset: usize,
}

impl DeclarationInfo {
    fn new(kind: DeclarationKind, offset: usize) -> Self {
        Self { kind, offset }
    }
}

pub type EarlyErrorsResult<'alloc> = Result<(), ParseError<'alloc>>;

pub trait LexicalEarlyErrorsContext {
    fn declare_lex<'alloc>(
        &mut self,
        name: SourceAtomSetIndex,
        kind: DeclarationKind,
        offset: usize,
        atoms: &SourceAtomSet<'alloc>,
    ) -> EarlyErrorsResult<'alloc>;
}

pub trait VarEarlyErrorsContext {
    fn declare_var<'alloc>(
        &mut self,
        name: SourceAtomSetIndex,
        kind: DeclarationKind,
        offset: usize,
        atoms: &SourceAtomSet<'alloc>,
    ) -> EarlyErrorsResult<'alloc>;
}

pub trait ParameterEarlyErrorsContext {
    fn declare<'alloc>(
        &mut self,
        name: SourceAtomSetIndex,
        offset: usize,
        atoms: &SourceAtomSet<'alloc>,
    ) -> EarlyErrorsResult<'alloc>;
}

pub trait ControlEarlyErrorsContext {
    fn on_unhandled_break_or_continue<'alloc>(
        &self,
        info: &ControlInfo,
    ) -> EarlyErrorsResult<'alloc>;
}

// ===========================================================================
// Identifiers
// https://tc39.es/ecma262/#sec-identifiers
// ===========================================================================

#[derive(Debug, PartialEq)]
pub struct IdentifierEarlyErrorsContext {}

impl IdentifierEarlyErrorsContext {
    pub fn new() -> Self {
        Self {}
    }

    fn is_strict<'alloc>(&self) -> Result<bool, ParseError<'alloc>> {
        Err(ParseError::NotImplemented(
            "strict-mode-only early error is not yet supported",
        ))
    }

    // Not used due to NotImplemented before the callsite.
    /*
    fn is_module(&self) -> Result<bool, ParseError<'alloc>> {
        Err(ParseError::NotImplemented(
            "module-only early error is not yet supported",
        ))
    }
     */

    fn is_arguments_identifier<'alloc>(token: &arena::Box<'alloc, Token>) -> bool {
        return (token.terminal_id == TerminalId::Name
            || token.terminal_id == TerminalId::NameWithEscape)
            && token.value.as_atom() == CommonSourceAtomSetIndices::arguments();
    }

    fn is_eval_identifier<'alloc>(token: &arena::Box<'alloc, Token>) -> bool {
        return (token.terminal_id == TerminalId::Name
            || token.terminal_id == TerminalId::NameWithEscape)
            && token.value.as_atom() == CommonSourceAtomSetIndices::eval();
    }

    fn is_yield_identifier<'alloc>(token: &arena::Box<'alloc, Token>) -> bool {
        return token.terminal_id == TerminalId::Yield
            || (token.terminal_id == TerminalId::NameWithEscape
                && token.value.as_atom() == CommonSourceAtomSetIndices::yield_());
    }

    fn is_await_identifier<'alloc>(token: &arena::Box<'alloc, Token>) -> bool {
        return token.terminal_id == TerminalId::Await
            || (token.terminal_id == TerminalId::NameWithEscape
                && token.value.as_atom() == CommonSourceAtomSetIndices::await_());
    }

    pub fn check_binding_identifier<'alloc>(
        &self,
        token: &arena::Box<'alloc, Token>,
        atoms: &SourceAtomSet<'alloc>,
    ) -> EarlyErrorsResult<'alloc> {
        if Self::is_arguments_identifier(token) || Self::is_eval_identifier(token) {
            // Static Semantics: Early Errors
            // https://tc39.es/ecma262/#sec-identifiers-static-semantics-early-errors
            //
            // BindingIdentifier : Identifier
            //
            // * It is a Syntax Error if the code matched by this
            //   production is contained in strict mode code and the
            //   StringValue of Identifier is "arguments" or "eval".
            if self.is_strict()? {
                let name = atoms.get(token.value.as_atom());
                let offset = token.loc.start;
                return Err(ParseError::InvalidIdentifier(name.clone(), offset));
            }

            return Ok(());
        }

        if Self::is_yield_identifier(token) {
            // BindingIdentifier : yield
            //
            // * It is a Syntax Error if this production has a [Yield]
            //   parameter.
            return Err(ParseError::NotImplemented("[Yield] parameter"));

            // return self.check_yield_common();
        }

        if Self::is_await_identifier(token) {
            // BindingIdentifier : await
            //
            // * It is a Syntax Error if this production has an [Await]
            //   parameter.
            return Err(ParseError::NotImplemented("[Await] parameter"));

            // return self.check_await_common();
        }

        self.check_identifier(token, atoms)
    }

    pub fn check_label_identifier<'alloc>(
        &self,
        token: &arena::Box<'alloc, Token>,
        atoms: &SourceAtomSet<'alloc>,
    ) -> EarlyErrorsResult<'alloc> {
        if Self::is_yield_identifier(token) {
            return self.check_yield_common(token, atoms);
        }

        if Self::is_await_identifier(token) {
            return self.check_await_common(token, atoms);
        }

        self.check_identifier(token, atoms)
    }

    pub fn check_identifier_reference<'alloc>(
        &self,
        token: &arena::Box<'alloc, Token>,
        atoms: &SourceAtomSet<'alloc>,
    ) -> EarlyErrorsResult<'alloc> {
        if Self::is_yield_identifier(token) {
            return self.check_yield_common(token, atoms);
        }

        if Self::is_await_identifier(token) {
            return self.check_await_common(token, atoms);
        }

        self.check_identifier(token, atoms)
    }

    fn check_yield_common<'alloc>(
        &self,
        _token: &arena::Box<'alloc, Token>,
        _atoms: &SourceAtomSet<'alloc>,
    ) -> EarlyErrorsResult<'alloc> {
        // Static Semantics: Early Errors
        // https://tc39.es/ecma262/#sec-identifiers-static-semantics-early-errors
        //
        // IdentifierReference[Yield, Await] : Identifier
        //
        // BindingIdentifier[Yield, Await] : Identifier
        //
        // LabelIdentifier[Yield, Await] : Identifier
        //
        // * It is a Syntax Error if this production has a [Yield] parameter
        //   and StringValue of Identifier is "yield".
        return Err(ParseError::NotImplemented("[Yield] parameter"));

        // IdentifierReference : yield
        //
        // BindingIdentifier : yield
        //
        // LabelIdentifier : yield
        //
        // * It is a Syntax Error if the code matched by this production is
        //   contained in strict mode code.
        //
        // and
        //
        // Identifier : IdentifierName but not ReservedWord
        //
        // * It is a Syntax Error if this phrase is contained in strict mode
        //   code and the StringValue of IdentifierName is: "implements",
        //   "interface", "let", "package", "private", "protected", "public",
        //   "static", or "yield".
        //
        // if self.is_strict()? {
        //     return Err(ParseError::InvalidIdentifier(
        //         atoms.get(token.value.as_atom()),
        //         offset,
        //     ));
        // }
        //
        // Ok(())
    }

    fn check_await_common<'alloc>(
        &self,
        _token: &arena::Box<'alloc, Token>,
        _atoms: &SourceAtomSet<'alloc>,
    ) -> EarlyErrorsResult<'alloc> {
        // Static Semantics: Early Errors
        // https://tc39.es/ecma262/#sec-identifiers-static-semantics-early-errors
        //
        // IdentifierReference[Yield, Await] : Identifier
        //
        // BindingIdentifier[Yield, Await] : Identifier
        //
        // LabelIdentifier[Yield, Await] : Identifier
        //
        // * It is a Syntax Error if this production has an [Await] parameter
        //   and StringValue of Identifier is "await".
        return Err(ParseError::NotImplemented("[Await] parameter"));

        // IdentifierReference : await
        //
        // BindingIdentifier : await
        //
        // LabelIdentifier : await
        //
        // * It is a Syntax Error if the goal symbol of the syntactic
        //   grammar is Module.
        //
        // and
        //
        // Identifier : IdentifierName but not ReservedWord
        //
        // * It is a Syntax Error if the goal symbol of the syntactic grammar
        //   is Module and the StringValue of IdentifierName is "await".
        //
        // if self.is_module()? {
        //     return Err(ParseError::InvalidIdentifier(
        //         atoms.get(token.value.as_atom()),
        //         offset,
        //     ));
        // }
        //
        // Ok(())
    }

    fn is_contextual_keyword_excluding_yield(name: SourceAtomSetIndex) -> bool {
        name == CommonSourceAtomSetIndices::implements()
            || name == CommonSourceAtomSetIndices::interface()
            || name == CommonSourceAtomSetIndices::let_()
            || name == CommonSourceAtomSetIndices::package()
            || name == CommonSourceAtomSetIndices::private()
            || name == CommonSourceAtomSetIndices::protected()
            || name == CommonSourceAtomSetIndices::public()
            || name == CommonSourceAtomSetIndices::static_()
    }

    fn is_keyword(name: SourceAtomSetIndex) -> bool {
        name == CommonSourceAtomSetIndices::break_()
            || name == CommonSourceAtomSetIndices::case()
            || name == CommonSourceAtomSetIndices::catch()
            || name == CommonSourceAtomSetIndices::class()
            || name == CommonSourceAtomSetIndices::const_()
            || name == CommonSourceAtomSetIndices::continue_()
            || name == CommonSourceAtomSetIndices::debugger()
            || name == CommonSourceAtomSetIndices::default()
            || name == CommonSourceAtomSetIndices::delete()
            || name == CommonSourceAtomSetIndices::do_()
            || name == CommonSourceAtomSetIndices::else_()
            || name == CommonSourceAtomSetIndices::enum_()
            || name == CommonSourceAtomSetIndices::export()
            || name == CommonSourceAtomSetIndices::extends()
            || name == CommonSourceAtomSetIndices::false_()
            || name == CommonSourceAtomSetIndices::finally()
            || name == CommonSourceAtomSetIndices::for_()
            || name == CommonSourceAtomSetIndices::function()
            || name == CommonSourceAtomSetIndices::if_()
            || name == CommonSourceAtomSetIndices::import()
            || name == CommonSourceAtomSetIndices::in_()
            || name == CommonSourceAtomSetIndices::instanceof()
            || name == CommonSourceAtomSetIndices::new_()
            || name == CommonSourceAtomSetIndices::null()
            || name == CommonSourceAtomSetIndices::return_()
            || name == CommonSourceAtomSetIndices::super_()
            || name == CommonSourceAtomSetIndices::switch()
            || name == CommonSourceAtomSetIndices::this()
            || name == CommonSourceAtomSetIndices::throw()
            || name == CommonSourceAtomSetIndices::true_()
            || name == CommonSourceAtomSetIndices::try_()
            || name == CommonSourceAtomSetIndices::typeof_()
            || name == CommonSourceAtomSetIndices::var()
            || name == CommonSourceAtomSetIndices::void()
            || name == CommonSourceAtomSetIndices::while_()
            || name == CommonSourceAtomSetIndices::with()
    }

    fn check_identifier<'alloc>(
        &self,
        token: &arena::Box<'alloc, Token>,
        atoms: &SourceAtomSet<'alloc>,
    ) -> EarlyErrorsResult<'alloc> {
        match token.terminal_id {
            TerminalId::NameWithEscape => {
                let name = token.value.as_atom();
                if Self::is_contextual_keyword_excluding_yield(name) {
                    // Identifier : IdentifierName but not ReservedWord
                    //
                    // * It is a Syntax Error if this phrase is contained
                    //   in strict mode code and the StringValue of
                    //   IdentifierName is:
                    //   "implements", "interface", "let", "package",
                    //   "private", "protected", "public", "static",
                    //   or "yield".
                    //
                    // NOTE: "yield" case is handled in
                    //       `check_yield_common`.
                    if self.is_strict()? {
                        let name = atoms.get(token.value.as_atom());
                        let offset = token.loc.start;
                        return Err(ParseError::InvalidIdentifier(name, offset));
                    }
                } else if Self::is_keyword(name) {
                    // Identifier : IdentifierName but not ReservedWord
                    //
                    // * It is a Syntax Error if StringValue of
                    //   IdentifierName is the same String value as the
                    //   StringValue of any ReservedWord except for yield
                    //   or await.
                    let name = atoms.get(token.value.as_atom());
                    let offset = token.loc.start;
                    return Err(ParseError::InvalidIdentifier(name, offset));
                }
            }
            TerminalId::Implements
            | TerminalId::Interface
            | TerminalId::Let
            | TerminalId::Package
            | TerminalId::Private
            | TerminalId::Protected
            | TerminalId::Public
            | TerminalId::Static => {
                // Identifier : IdentifierName but not ReservedWord
                //
                // * It is a Syntax Error if this phrase is contained in strict
                //   mode code and the StringValue of IdentifierName is:
                //   "implements", "interface", "let", "package", "private",
                //   "protected", "public", "static", or "yield".
                //
                // NOTE: "yield" case is handled in `check_yield_common`.
                if self.is_strict()? {
                    let name = atoms.get(token.value.as_atom());
                    let offset = token.loc.start;
                    return Err(ParseError::InvalidIdentifier(name, offset));
                }
            }
            _ => {}
        }

        Ok(())
    }
}

// ===========================================================================
// Block
// https://tc39.es/ecma262/#sec-block
// ===========================================================================

#[derive(Debug, PartialEq)]
pub struct BlockEarlyErrorsContext {
    lex_names_of_stmt_list: HashMap<SourceAtomSetIndex, DeclarationInfo>,
    var_names_of_stmt_list: HashMap<SourceAtomSetIndex, DeclarationInfo>,
}

impl BlockEarlyErrorsContext {
    pub fn new() -> Self {
        Self {
            lex_names_of_stmt_list: HashMap::new(),
            var_names_of_stmt_list: HashMap::new(),
        }
    }

    fn is_supported_lexical(kind: DeclarationKind) -> bool {
        match kind {
            // LexicallyDeclaredNames of StatementList
            //
            // Static Semantics: LexicallyDeclaredNames
            // https://tc39.es/ecma262/#sec-block-static-semantics-lexicallydeclarednames
            //
            // StatementList => StatementListItem => Declaration
            //   1. Return the BoundNames of Declaration.
            // Declaration => HoistableDeclaration => FunctionDeclaration
            //   1. Return the BoundNames of BindingIdentifier.
            //   1. Return « "*default*" ».
            //
            // and
            //
            // StatementList => StatementListItem => Statement
            //   1. If Statement is Statement: LabelledStatement, return
            //      LexicallyDeclaredNames of LabelledStatement.
            //   2. Return a new empty List.
            // LabelledStatement => LabelledItem => FunctionDeclaration
            //   1. Return BoundNames of FunctionDeclaration.
            // FunctionDeclaration
            //   1. Return the BoundNames of BindingIdentifier.
            //   1. Return « "*default*" ».
            //
            // NOTE: This is separated from LexicalAsyncOrGenerator to support
            //       https://tc39.es/ecma262/#sec-block-duplicates-allowed-static-semantics
            DeclarationKind::LexicalFunction |

            // StatementList => StatementListItem => Declaration
            //   1. Return the BoundNames of Declaration.
            // Declaration => HoistableDeclaration => FunctionDeclaration
            // Declaration => HoistableDeclaration => GeneratorDeclaration
            // Declaration => HoistableDeclaration => AsyncFunctionDeclaration
            // Declaration => HoistableDeclaration => AsyncGeneratorDeclaration
            //   1. Return the BoundNames of BindingIdentifier.
            //   1. Return « "*default*" ».
            DeclarationKind::LexicalAsyncOrGenerator |

            // StatementList => StatementListItem => Declaration
            //   1. Return the BoundNames of Declaration.
            // Declaration => ClassDeclaration
            //   1. Return the BoundNames of BindingIdentifier.
            //   1. Return « "*default*" ».
            DeclarationKind::Class |

            // StatementList => StatementListItem => Declaration
            //   1. Return the BoundNames of Declaration.
            // Declaration => LexicalDeclaration => BindingList
            // => LexicalBinding
            //   1. Return the BoundNames of BindingIdentifier.
            //   1. Return the BoundNames of BindingPattern.
            DeclarationKind::Let |
            DeclarationKind::Const => true,
            _ => false,
        }
    }

    fn is_supported_var(kind: DeclarationKind) -> bool {
        match kind {
            // VarDeclaredNames of StatementList
            //
            // Static Semantics: VarDeclaredNames
            // https://tc39.es/ecma262/#sec-block-static-semantics-vardeclarednames
            //
            // StatementList => StatementListItem => Statement
            // => VariableStatement
            //   1. Return BoundNames of VariableDeclarationList.
            // VariableDeclarationList => VariableDeclaration
            //   1. Return the BoundNames of BindingIdentifier.
            //   1. Return the BoundNames of BindingPattern.
            //
            // and
            //
            // StatementList => StatementListItem => Statement
            // => BreakableStatement => IterationStatement => c-style-for-var
            //   1. Let names be BoundNames of VariableDeclarationList.
            //   2. Append to names the elements of the VarDeclaredNames of
            //      Statement.
            //   3. Return names.
            // VariableDeclarationList => VariableDeclaration
            //   1. Return the BoundNames of BindingIdentifier.
            //   1. Return the BoundNames of BindingPattern.
            //
            // and
            //
            // StatementList => StatementListItem => Statement
            // => BreakableStatement => IterationStatement => for-in-var
            //   1. Let names be the BoundNames of ForBinding.
            //   2. Append to names the elements of the VarDeclaredNames of
            //      Statement.
            //   3. Return names.
            // ForBinding => BindingIdentifier
            // ForBinding => BindingPattern
            DeclarationKind::Var |

            // StatementList => StatementListItem => Statement => BlockStatement
            // => Block

            // StatementList => StatementListItem => Statement => IfStatement
            // => Statement

            // StatementList => StatementListItem => Statement
            // => BreakableStatement => IterationStatement => do-while
            // => Statement

            // StatementList => StatementListItem => Statement
            // => BreakableStatement => IterationStatement => while => Statement

            // StatementList => StatementListItem => Statement
            // => BreakableStatement => IterationStatement => c-style-for
            // => Statement

            // StatementList => StatementListItem => Statement
            // => BreakableStatement => IterationStatement => for-in
            // => Statement

            // StatementList => StatementListItem => Statement
            // => BreakableStatement => SwitchStatement => CaseBlock
            // => CaseClauses => StatementList

            // StatementList => StatementListItem => Statement
            // => BreakableStatement => SwitchStatement => CaseBlock
            // => DefaultClause => StatementList

            // StatementList => StatementListItem => Statement => WithStatement
            // => Statement

            // StatementList => StatementListItem => Statement
            // => LabelledStatement => LabelledItem => Statement

            // StatementList => StatementListItem => Statement
            // => LabelledStatement => LabelledItem => FunctionDeclaration
            //   1. Return a new empty List.

            // StatementList => StatementListItem => Statement => TryStatement
            // => Block
            // StatementList => StatementListItem => Statement => TryStatement
            // => Catch => Block
            // StatementList => StatementListItem => Statement => TryStatement
            // => Finally => Block

            // StatementList => StatementListItem => Declaration
            //   1. Return a new empty List.

            // StatementList => StatementListItem => Declaration
            // => HoistableDeclaration => FunctionDeclaration
            //
            // Changes to FunctionDeclarationInstantiation
            // https://tc39.es/ecma262/#sec-web-compat-functiondeclarationinstantiation
            //
            // During FunctionDeclarationInstantiation the following steps are
            // performed in place of step 29:
            //
            // 1. If strict is false, then
            //   a. For each FunctionDeclaration f that is directly contained
            //      in the StatementList of a Block, CaseClause, or
            //      DefaultClause, do
            //     i. Let F be StringValue of the BindingIdentifier of f.
            //     ii. If replacing the FunctionDeclaration f with a
            //         VariableStatement that has F as a BindingIdentifier
            //         would not produce any Early Errors for func and F is not
            //         an element of parameterNames, then
            //
            // and
            //
            // Changes to GlobalDeclarationInstantiation
            // https://tc39.es/ecma262/#sec-web-compat-globaldeclarationinstantiation
            //
            // During GlobalDeclarationInstantiation the following steps are
            // performed in place of step 14:
            //
            // 1. Let strict be IsStrict of script.
            // 2. If strict is false, then
            //   d. For each FunctionDeclaration f that is directly contained
            //      in the StatementList of a Block , CaseClause , or
            //      DefaultClause Contained within script, do
            //     i. Let F be StringValue of the BindingIdentifier of f.
            //     ii. If replacing the FunctionDeclaration f with a
            //         VariableStatement that has F as a BindingIdentifier
            //         would not produce any Early Errors for script, then
            //
            // This isn't used while checking actual Early Errors.
            DeclarationKind::VarForAnnexBLexicalFunction => true,
            _ => false,
        }
    }

    fn is_strict<'alloc>(&self) -> Result<bool, ParseError<'alloc>> {
        Err(ParseError::NotImplemented(
            "strict-mode-only early error is not yet supported",
        ))
    }
}

impl LexicalEarlyErrorsContext for BlockEarlyErrorsContext {
    fn declare_lex<'alloc>(
        &mut self,
        name: SourceAtomSetIndex,
        kind: DeclarationKind,
        offset: usize,
        atoms: &SourceAtomSet<'alloc>,
    ) -> EarlyErrorsResult<'alloc> {
        debug_assert!(Self::is_supported_lexical(kind));

        // Static Semantics: Early Errors
        // https://tc39.es/ecma262/#sec-block-static-semantics-early-errors
        //
        // Block : { StatementList }
        //
        // * It is a Syntax Error if the LexicallyDeclaredNames of StatementList
        //   contains any duplicate entries.
        //
        if let Some(info) = self.lex_names_of_stmt_list.get(&name) {
            // Changes to Block Static Semantics: Early Errors
            // https://tc39.es/ecma262/#sec-block-duplicates-allowed-static-semantics
            //
            // Block : { StatementList }
            //
            // * It is a Syntax Error if the LexicallyDeclaredNames of
            //   StatementList contains any duplicate entries, ** unless the
            //   source code matching this production is not strict mode
            //   code and the duplicate entries are only bound by
            //   FunctionDeclarations **.
            if !(!self.is_strict()?
                && info.kind == DeclarationKind::LexicalFunction
                && kind == DeclarationKind::LexicalFunction)
            {
                let name = atoms.get(name);
                return Err(ParseError::DuplicateBinding(
                    name,
                    info.kind,
                    info.offset,
                    kind,
                    offset,
                ));
            }
        }

        // Static Semantics: Early Errors
        // https://tc39.es/ecma262/#sec-block-static-semantics-early-errors
        //
        // Block : { StatementList }
        //
        // * It is a Syntax Error if any element of the LexicallyDeclaredNames
        //   of StatementList also occurs in the VarDeclaredNames of
        //   StatementList.
        if let Some(info) = self.var_names_of_stmt_list.get(&name) {
            let name = atoms.get(name);
            return Err(ParseError::DuplicateBinding(
                name,
                info.kind,
                info.offset,
                kind,
                offset,
            ));
        }

        self.lex_names_of_stmt_list
            .insert(name, DeclarationInfo::new(kind, offset));

        Ok(())
    }
}

impl VarEarlyErrorsContext for BlockEarlyErrorsContext {
    fn declare_var<'alloc>(
        &mut self,
        name: SourceAtomSetIndex,
        kind: DeclarationKind,
        offset: usize,
        atoms: &SourceAtomSet<'alloc>,
    ) -> EarlyErrorsResult<'alloc> {
        debug_assert!(Self::is_supported_var(kind));

        // Static Semantics: Early Errors
        // https://tc39.es/ecma262/#sec-block-static-semantics-early-errors
        //
        // Block : { StatementList }
        //
        // * It is a Syntax Error if any element of the LexicallyDeclaredNames
        //   of StatementList also occurs in the VarDeclaredNames of
        //   StatementList.
        if let Some(info) = self.lex_names_of_stmt_list.get(&name) {
            let name = atoms.get(name);
            return Err(ParseError::DuplicateBinding(
                name,
                info.kind,
                info.offset,
                kind,
                offset,
            ));
        }

        self.var_names_of_stmt_list
            .insert(name, DeclarationInfo::new(kind, offset));

        Ok(())
    }
}

// ===========================================================================
// The for Statement
// https://tc39.es/ecma262/#sec-for-statement
//
// The for-in, for-of, and for-await-of Statements
// https://tc39.es/ecma262/#sec-for-in-and-for-of-statements
// ===========================================================================

#[derive(Debug, PartialEq)]
pub struct LexicalForHeadEarlyErrorsContext {
    bound_names_of_decl: HashMap<SourceAtomSetIndex, DeclarationInfo>,
}

impl LexicalForHeadEarlyErrorsContext {
    pub fn new() -> Self {
        Self {
            bound_names_of_decl: HashMap::new(),
        }
    }

    fn is_supported_lexical(kind: DeclarationKind) -> bool {
        match kind {
            // BoundNames of BindingList
            //
            // Static Semantics: BoundNames
            // https://tc39.es/ecma262/#sec-let-and-const-declarations-static-semantics-boundnames
            //
            // BindingList => LexicalBinding
            //   1. Return the BoundNames of BindingIdentifier.
            //   1. Return the BoundNames of BindingPattern.
            //
            // and
            //
            // BoundNames of ForDeclaration
            //
            // Static Semantics: BoundNames
            // https://tc39.es/ecma262/#sec-let-and-const-declarations-static-semantics-boundnames
            //
            // ForDeclaration => BindingList => LexicalBinding
            //   1. Return the BoundNames of BindingIdentifier.
            //   1. Return the BoundNames of BindingPattern.
            DeclarationKind::Let | DeclarationKind::Const => true,
            _ => false,
        }
    }
}

impl LexicalEarlyErrorsContext for LexicalForHeadEarlyErrorsContext {
    fn declare_lex<'alloc>(
        &mut self,
        name: SourceAtomSetIndex,
        kind: DeclarationKind,
        offset: usize,
        atoms: &SourceAtomSet<'alloc>,
    ) -> EarlyErrorsResult<'alloc> {
        debug_assert!(Self::is_supported_lexical(kind));

        // Static Semantics: Early Errors
        // https://tc39.es/ecma262/#sec-semantics-static-semantics-early-errors
        //
        // IterationStatement :
        //   for ( LexicalDeclaration Expression_opt ; Expression_opt )
        //   Statement
        //
        // Static Semantics: Early Errors
        // https://tc39.es/ecma262/#sec-let-and-const-declarations-static-semantics-early-errors
        //
        // LexicalDeclaration : LetOrConst BindingList ;
        //
        // * It is a Syntax Error if the BoundNames of BindingList contains any
        //  duplicate entries.
        //
        // and
        //
        // Static Semantics: Early Errors
        // https://tc39.es/ecma262/#sec-for-in-and-for-of-statements-static-semantics-early-errors
        //
        // IterationStatement :
        //   for ( ForDeclaration in Expression ) Statement
        //   for ( ForDeclaration of AssignmentExpression ) Statement
        //   for await ( ForDeclaration of AssignmentExpression ) Statement
        //
        // * It is a Syntax Error if the BoundNames of ForDeclaration contains
        //   any duplicate entries.
        if let Some(info) = self.bound_names_of_decl.get(&name) {
            let name = atoms.get(name);
            return Err(ParseError::DuplicateBinding(
                name,
                info.kind,
                info.offset,
                kind,
                offset,
            ));
        }

        self.bound_names_of_decl
            .insert(name, DeclarationInfo::new(kind, offset));

        Ok(())
    }
}

#[derive(Debug, PartialEq)]
struct InternalForBodyEarlyErrorsContext {
    var_names_of_stmt: HashMap<SourceAtomSetIndex, DeclarationInfo>,
}

impl InternalForBodyEarlyErrorsContext {
    fn new() -> Self {
        Self {
            var_names_of_stmt: HashMap::new(),
        }
    }

    fn is_supported_var(kind: DeclarationKind) -> bool {
        match kind {
            // VarDeclaredNames of Statement
            //
            // See Block::is_supported_var for the details.
            DeclarationKind::Var => true,

            _ => false,
        }
    }
}

impl VarEarlyErrorsContext for InternalForBodyEarlyErrorsContext {
    fn declare_var<'alloc>(
        &mut self,
        name: SourceAtomSetIndex,
        kind: DeclarationKind,
        offset: usize,
        _atoms: &SourceAtomSet<'alloc>,
    ) -> EarlyErrorsResult<'alloc> {
        debug_assert!(Self::is_supported_var(kind));

        self.var_names_of_stmt
            .insert(name, DeclarationInfo::new(kind, offset));

        Ok(())
    }
}

#[derive(Debug, PartialEq)]
pub struct LexicalForBodyEarlyErrorsContext {
    head: LexicalForHeadEarlyErrorsContext,
    body: InternalForBodyEarlyErrorsContext,
}

impl LexicalForBodyEarlyErrorsContext {
    pub fn new(head: LexicalForHeadEarlyErrorsContext) -> Self {
        Self {
            head,
            body: InternalForBodyEarlyErrorsContext::new(),
        }
    }
}

impl VarEarlyErrorsContext for LexicalForBodyEarlyErrorsContext {
    fn declare_var<'alloc>(
        &mut self,
        name: SourceAtomSetIndex,
        kind: DeclarationKind,
        offset: usize,
        atoms: &SourceAtomSet<'alloc>,
    ) -> EarlyErrorsResult<'alloc> {
        // Static Semantics: Early Errors
        // https://tc39.es/ecma262/#sec-for-statement-static-semantics-early-errors
        //
        // IterationStatement :
        //   for ( LexicalDeclaration Expression_opt ; Expression_opt )
        //   Statement
        //
        // * It is a Syntax Error if any element of the BoundNames of
        //   LexicalDeclaration also occurs in the VarDeclaredNames of
        //   Statement.
        //
        // and
        //
        // Static Semantics: Early Errors
        // https://tc39.es/ecma262/#sec-for-in-and-for-of-statements-static-semantics-early-errors
        //
        // IterationStatement :
        //   for ( ForDeclaration in Expression ) Statement
        //   for ( ForDeclaration of AssignmentExpression ) Statement
        //   for await ( ForDeclaration of AssignmentExpression ) Statement
        //
        // * It is a Syntax Error if any element of the BoundNames of
        //   ForDeclaration also occurs in the VarDeclaredNames of Statement.
        if let Some(info) = self.head.bound_names_of_decl.get(&name) {
            let name = atoms.get(name);
            return Err(ParseError::DuplicateBinding(
                name,
                info.kind,
                info.offset,
                kind,
                offset,
            ));
        }

        self.body.declare_var(name, kind, offset, atoms)
    }
}

// ===========================================================================
// LabelledStatements
// https://tc39.es/ecma262/#sec-labelled-statements
// ===========================================================================

pub struct LabelledStatementEarlyErrorsContext {
    name: SourceAtomSetIndex,
    is_loop: bool,
}

impl LabelledStatementEarlyErrorsContext {
    pub fn new(name: SourceAtomSetIndex, is_loop: bool) -> Self {
        Self { name, is_loop }
    }

    pub fn check_duplicate_label<'alloc>(
        &self,
        inner_label_name: SourceAtomSetIndex,
    ) -> EarlyErrorsResult<'alloc> {
        // Static Semantics: ContainsDuplicateLabels
        // https://tc39.es/ecma262/#sec-labelled-statements-static-semantics-containsduplicatelabels
        //
        //  LabelledStatement : LabelIdentifier : LabelledItem
        //
        // Static Semantics: Early Errors
        // https://tc39.es/ecma262/#sec-scripts-static-semantics-early-errors
        //
        // * It is a Syntax Error if ContainsDuplicateLabels of StatementList with argument « » is
        //   true.
        //
        // and
        //
        // https://tc39.es/ecma262/#sec-module-semantics-static-semantics-early-errors
        //
        // * It is a Syntax Error if ContainsDuplicateLabels of ModuleItemList with argument « » is
        //   true.
        //
        // and
        //
        // https://tc39.es/ecma262/#sec-function-definitions-static-semantics-early-errors
        // * It is a Syntax Error if ContainsDuplicateLabels of FunctionStatementList with argument
        //   « » is true.
        if inner_label_name == self.name {
            return Err(ParseError::DuplicateLabel);
        }
        Ok(())
    }

    pub fn check_labelled_continue_to_non_loop<'alloc>(
        &self,
        info: &ControlInfo,
    ) -> EarlyErrorsResult<'alloc> {
        //  Continues outside of iterators and Unlabelled breaks can not be detected at the
        //  function / script level easily, because we only have binding information there, not
        //  whether the label in question is associated with a loop or not. The nesting errors
        //  also cannot be detected at the {Break,Continue}Statement level, as we don't have that
        //  information there either. So we are handling these errors here. This handles labelled
        //  continues that would otherwise pass.
        //
        //  Static Semantics: Early Errors
        //  https://tc39.es/ecma262/#sec-continue-statement-static-semantics-early-e
        //
        //  ContinueStatement : continue LabelIdentifier ;
        //
        //  * It is a Syntax Error if this ContinueStatement is not nested, directly
        //    indirectly (but not crossing function boundaries), within an
        //    IterationStatement.
        if let Some(name) = info.label {
            if !self.is_loop && info.kind == ControlKind::Continue && name == self.name {
                return Err(ParseError::BadContinue);
            }
        }

        Ok(())
    }
}

// ===========================================================================
// The switch Statement
// https://tc39.es/ecma262/#sec-switch-statement
// ===========================================================================

#[derive(Debug, PartialEq)]
pub struct CaseBlockEarlyErrorsContext {
    lex_names_of_case_block: HashMap<SourceAtomSetIndex, DeclarationInfo>,
    var_names_of_case_block: HashMap<SourceAtomSetIndex, DeclarationInfo>,
}

impl CaseBlockEarlyErrorsContext {
    pub fn new() -> Self {
        Self {
            lex_names_of_case_block: HashMap::new(),
            var_names_of_case_block: HashMap::new(),
        }
    }

    fn is_supported_lexical(kind: DeclarationKind) -> bool {
        // CaseBlock => CaseClauses => CaseClause => StatementList
        // CaseBlock => DefaultClause => StatementList
        BlockEarlyErrorsContext::is_supported_lexical(kind)
    }

    fn is_supported_var(kind: DeclarationKind) -> bool {
        BlockEarlyErrorsContext::is_supported_var(kind)
    }

    fn is_strict<'alloc>(&self) -> Result<bool, ParseError<'alloc>> {
        Err(ParseError::NotImplemented(
            "strict-mode-only early error is not yet supported",
        ))
    }
}

impl LexicalEarlyErrorsContext for CaseBlockEarlyErrorsContext {
    fn declare_lex<'alloc>(
        &mut self,
        name: SourceAtomSetIndex,
        kind: DeclarationKind,
        offset: usize,
        atoms: &SourceAtomSet<'alloc>,
    ) -> EarlyErrorsResult<'alloc> {
        debug_assert!(Self::is_supported_lexical(kind));

        // Static Semantics: Early Errors
        // https://tc39.es/ecma262/#sec-switch-statement-static-semantics-early-errors
        //
        // SwitchStatement : switch ( Expression ) CaseBlock
        //
        // * It is a Syntax Error if the LexicallyDeclaredNames of CaseBlock
        //   contains any duplicate entries.
        if let Some(info) = self.lex_names_of_case_block.get(&name) {
            // Changes to switch Statement Static Semantics: Early Errors
            // https://tc39.es/ecma262/#sec-switch-duplicates-allowed-static-semantics
            //
            // SwitchStatement : switch ( Expression ) CaseBlock
            //
            // * It is a Syntax Error if the LexicallyDeclaredNames of
            //   CaseBlock contains any duplicate entries, ** unless the source
            //   code matching this production is not strict mode code and the
            //   duplicate entries are only bound by FunctionDeclarations **.
            if !(!self.is_strict()?
                && info.kind == DeclarationKind::LexicalFunction
                && kind == DeclarationKind::LexicalFunction)
            {
                let name = atoms.get(name);
                return Err(ParseError::DuplicateBinding(
                    name,
                    info.kind,
                    info.offset,
                    kind,
                    offset,
                ));
            }
        }

        // Static Semantics: Early Errors
        // https://tc39.es/ecma262/#sec-switch-statement-static-semantics-early-errors
        //
        // SwitchStatement : switch ( Expression ) CaseBlock
        //
        // * It is a Syntax Error if any element of the LexicallyDeclaredNames
        //   of CaseBlock also occurs in the VarDeclaredNames of CaseBlock.
        if let Some(info) = self.var_names_of_case_block.get(&name) {
            let name = atoms.get(name);
            return Err(ParseError::DuplicateBinding(
                name,
                info.kind,
                info.offset,
                kind,
                offset,
            ));
        }

        self.lex_names_of_case_block
            .insert(name, DeclarationInfo::new(kind, offset));

        Ok(())
    }
}

impl VarEarlyErrorsContext for CaseBlockEarlyErrorsContext {
    fn declare_var<'alloc>(
        &mut self,
        name: SourceAtomSetIndex,
        kind: DeclarationKind,
        offset: usize,
        atoms: &SourceAtomSet<'alloc>,
    ) -> EarlyErrorsResult<'alloc> {
        debug_assert!(Self::is_supported_var(kind));

        // Static Semantics: Early Errors
        // https://tc39.es/ecma262/#sec-switch-statement-static-semantics-early-errors
        //
        // SwitchStatement : switch ( Expression ) CaseBlock
        //
        // * It is a Syntax Error if any element of the LexicallyDeclaredNames
        //   of CaseBlock also occurs in the VarDeclaredNames of CaseBlock.
        if let Some(info) = self.lex_names_of_case_block.get(&name) {
            let name = atoms.get(name);
            return Err(ParseError::DuplicateBinding(
                name,
                info.kind,
                info.offset,
                kind,
                offset,
            ));
        }

        self.var_names_of_case_block
            .insert(name, DeclarationInfo::new(kind, offset));

        Ok(())
    }
}

// ===========================================================================
// The try Statement
// https://tc39.es/ecma262/#sec-try-statement
// ===========================================================================

#[derive(Debug, PartialEq)]
pub struct CatchParameterEarlyErrorsContext {
    bound_names_of_catch_param: HashMap<SourceAtomSetIndex, DeclarationInfo>,
    is_simple: bool,
}

impl CatchParameterEarlyErrorsContext {
    pub fn new_with_binding_identifier() -> Self {
        Self {
            bound_names_of_catch_param: HashMap::new(),
            is_simple: true,
        }
    }

    pub fn new_with_binding_pattern() -> Self {
        Self {
            bound_names_of_catch_param: HashMap::new(),
            is_simple: false,
        }
    }
}

impl ParameterEarlyErrorsContext for CatchParameterEarlyErrorsContext {
    fn declare<'alloc>(
        &mut self,
        name: SourceAtomSetIndex,
        offset: usize,
        atoms: &SourceAtomSet<'alloc>,
    ) -> EarlyErrorsResult<'alloc> {
        // BoundNames of CatchParameter
        //
        // CatchParameter => BindingIdentifier
        // CatchParameter => BindingPattern
        let kind = DeclarationKind::CatchParameter;

        // Static Semantics: Early Errors
        // https://tc39.es/ecma262/#sec-try-statement-static-semantics-early-errors
        //
        // Catch : catch ( CatchParameter ) Block
        //
        // * It is a Syntax Error if BoundNames of CatchParameter contains any
        //   duplicate elements.
        if let Some(info) = self.bound_names_of_catch_param.get(&name) {
            let name = atoms.get(name);
            return Err(ParseError::DuplicateBinding(
                name, info.kind, offset, kind, offset,
            ));
        }

        self.bound_names_of_catch_param
            .insert(name, DeclarationInfo::new(kind, offset));

        Ok(())
    }
}

#[derive(Debug, PartialEq)]
pub struct CatchBlockEarlyErrorsContext {
    param: CatchParameterEarlyErrorsContext,
    block: BlockEarlyErrorsContext,
}

impl CatchBlockEarlyErrorsContext {
    pub fn new(param: CatchParameterEarlyErrorsContext) -> Self {
        Self {
            param,
            block: BlockEarlyErrorsContext::new(),
        }
    }
}

impl LexicalEarlyErrorsContext for CatchBlockEarlyErrorsContext {
    fn declare_lex<'alloc>(
        &mut self,
        name: SourceAtomSetIndex,
        kind: DeclarationKind,
        offset: usize,
        atoms: &SourceAtomSet<'alloc>,
    ) -> EarlyErrorsResult<'alloc> {
        // Static Semantics: Early Errors
        // https://tc39.es/ecma262/#sec-try-statement-static-semantics-early-errors
        //
        // Catch : catch ( CatchParameter ) Block
        //
        // * It is a Syntax Error if any element of the BoundNames of
        //   CatchParameter also occurs in the LexicallyDeclaredNames of Block.
        if let Some(info) = self.param.bound_names_of_catch_param.get(&name) {
            let name = atoms.get(name);
            return Err(ParseError::DuplicateBinding(
                name, info.kind, offset, kind, offset,
            ));
        }

        self.block.declare_lex(name, kind, offset, atoms)
    }
}

impl VarEarlyErrorsContext for CatchBlockEarlyErrorsContext {
    fn declare_var<'alloc>(
        &mut self,
        name: SourceAtomSetIndex,
        kind: DeclarationKind,
        offset: usize,
        atoms: &SourceAtomSet<'alloc>,
    ) -> EarlyErrorsResult<'alloc> {
        // Static Semantics: Early Errors
        // https://tc39.es/ecma262/#sec-try-statement-static-semantics-early-errors
        //
        // Catch : catch ( CatchParameter ) Block
        //
        // * It is a Syntax Error if any element of the BoundNames of
        //   CatchParameter also occurs in the VarDeclaredNames of Block.
        //
        if let Some(info) = self.param.bound_names_of_catch_param.get(&name) {
            // VariableStatements in Catch Blocks
            // https://tc39.es/ecma262/#sec-variablestatements-in-catch-blocks
            //
            // Catch : catch ( CatchParameter ) Block
            //
            // * It is a Syntax Error if any element of the BoundNames of
            //   CatchParameter also occurs in the VarDeclaredNames of Block **
            //   unless CatchParameter is CatchParameter : BindingIdentifier **.
            if !self.param.is_simple {
                let name = atoms.get(name);
                return Err(ParseError::DuplicateBinding(
                    name,
                    info.kind,
                    info.offset,
                    kind,
                    offset,
                ));
            }
        }

        self.block.declare_var(name, kind, offset, atoms)
    }
}

// ===========================================================================
// Function Definitions
// https://tc39.es/ecma262/#sec-function-definitions
//
// Arrow Function Definitions
// https://tc39.es/ecma262/#sec-arrow-function-definitions
//
// Method Definitions
// https://tc39.es/ecma262/#sec-method-definitions
//
// Generator Function Definitions
// https://tc39.es/ecma262/#sec-generator-function-definitions
//
// Async Generator Function Definitions
// https://tc39.es/ecma262/#sec-async-generator-function-definitions
//
// Async Function Definitions
// https://tc39.es/ecma262/#sec-async-function-definitions
//
// Async Arrow Function Definitions
// https://tc39.es/ecma262/#sec-async-arrow-function-definitions
// ===========================================================================

#[derive(Debug, PartialEq)]
pub struct FormalParametersEarlyErrorsContext {
    bound_names_of_params: HashMap<SourceAtomSetIndex, DeclarationInfo>,
    is_simple: bool,
}

impl FormalParametersEarlyErrorsContext {
    pub fn new_simple() -> Self {
        Self {
            bound_names_of_params: HashMap::new(),
            is_simple: true,
        }
    }

    pub fn new_non_simple() -> Self {
        Self {
            bound_names_of_params: HashMap::new(),
            is_simple: false,
        }
    }
}

impl ParameterEarlyErrorsContext for FormalParametersEarlyErrorsContext {
    fn declare<'alloc>(
        &mut self,
        name: SourceAtomSetIndex,
        offset: usize,
        atoms: &SourceAtomSet<'alloc>,
    ) -> EarlyErrorsResult<'alloc> {
        // BoundNames of FormalParameterList
        //
        // Static Semantics: BoundNames
        // https://tc39.es/ecma262/#sec-function-definitions-static-semantics-boundnames
        //
        // FormalParameters => FunctionParameterList => FormalParameter
        // => BindingElement => SingleNameBinding => BindingIdentifier
        //
        // and
        //
        // FormalParameters => FunctionParameterList => FormalParameter
        // => BindingElement => BindingPattern
        //
        // and
        //
        // FormalParameters => FunctionRestParameter => BindingRestElement
        // => BindingIdentifier
        //
        // and
        //
        // FormalParameters => FunctionRestParameter => BindingRestElement
        // => BindingPattern
        let kind = DeclarationKind::FormalParameter;

        // Static Semantics: Early Errors
        // https://tc39.es/ecma262/#sec-function-definitions-static-semantics-early-errors
        //
        // FormalParameters : FormalParameterList
        //
        // * It is a Syntax Error if IsSimpleParameterList of
        //   FormalParameterList is false and BoundNames of FormalParameterList
        //   contains any duplicate elements.
        if let Some(info) = self.bound_names_of_params.get(&name) {
            if !self.is_simple {
                let name = atoms.get(name);
                return Err(ParseError::DuplicateBinding(
                    name,
                    info.kind,
                    info.offset,
                    kind,
                    offset,
                ));
            }
        }

        self.bound_names_of_params
            .insert(name, DeclarationInfo::new(kind, offset));

        Ok(())
    }
}

#[derive(Debug, PartialEq)]
pub struct UniqueFormalParametersEarlyErrorsContext {
    bound_names_of_params: HashMap<SourceAtomSetIndex, DeclarationInfo>,
}

impl UniqueFormalParametersEarlyErrorsContext {
    pub fn new() -> Self {
        Self {
            bound_names_of_params: HashMap::new(),
        }
    }
}

impl ParameterEarlyErrorsContext for UniqueFormalParametersEarlyErrorsContext {
    fn declare<'alloc>(
        &mut self,
        name: SourceAtomSetIndex,
        offset: usize,
        atoms: &SourceAtomSet<'alloc>,
    ) -> EarlyErrorsResult<'alloc> {
        let kind = DeclarationKind::FormalParameter;

        // Static Semantics: Early Errors
        // https://tc39.es/ecma262/#sec-function-definitions-static-semantics-early-errors
        //
        // UniqueFormalParameters : FormalParameters
        //
        // * It is a Syntax Error if BoundNames of FormalParameters contains any
        //   duplicate elements.
        //
        // and
        //
        // Static Semantics: Early Errors
        // https://tc39.es/ecma262/#sec-method-definitions-static-semantics-early-errors
        //
        // MethodDefinition :
        //   set PropertyName ( PropertySetParameterList ) { FunctionBody }
        //
        // * It is a Syntax Error if BoundNames of PropertySetParameterList
        //   contains any duplicate elements.
        if let Some(info) = self.bound_names_of_params.get(&name) {
            let name = atoms.get(name);
            return Err(ParseError::DuplicateBinding(
                name,
                info.kind,
                info.offset,
                kind,
                offset,
            ));
        }

        self.bound_names_of_params
            .insert(name, DeclarationInfo::new(kind, offset));

        Ok(())
    }
}

#[derive(Debug, PartialEq)]
struct InternalFunctionBodyEarlyErrorsContext {
    lex_names_of_body: HashMap<SourceAtomSetIndex, DeclarationInfo>,
    var_names_of_body: HashMap<SourceAtomSetIndex, DeclarationInfo>,
}

impl InternalFunctionBodyEarlyErrorsContext {
    fn new() -> Self {
        Self {
            lex_names_of_body: HashMap::new(),
            var_names_of_body: HashMap::new(),
        }
    }

    fn is_supported_lexical(kind: DeclarationKind) -> bool {
        match kind {
            // LexicallyDeclaredNames of FunctionStatementList
            //
            // Static Semantics: LexicallyDeclaredNames
            // https://tc39.es/ecma262/#sec-function-definitions-static-semantics-lexicallydeclarednames
            //
            // FunctionStatementList
            //   1. Return TopLevelLexicallyDeclaredNames of StatementList.
            //
            // Static Semantics: TopLevelLexicallyDeclaredNames
            // https://tc39.es/ecma262/#sec-block-static-semantics-toplevellexicallydeclarednames
            //
            // StatementList => StatementListItem => Statement
            //   1. Return a new empty List.
            //
            // StatementList => StatementListItem => Declaration
            //   1. If Declaration is Declaration : HoistableDeclaration, then
            //     a. Return « ».
            //   2. Return the BoundNames of Declaration.
            //
            // See Block::is_supported_lexical for the details.
            DeclarationKind::Class | DeclarationKind::Let | DeclarationKind::Const => true,
            _ => false,
        }
    }

    fn is_supported_var(kind: DeclarationKind) -> bool {
        match kind {
            // VarDeclaredNames of FunctionStatementList
            //
            // Static Semantics: VarDeclaredNames
            // https://tc39.es/ecma262/#sec-function-definitions-static-semantics-vardeclarednames
            //
            // FunctionStatementList
            //   1. Return TopLevelVarDeclaredNames of StatementList.
            //
            // Static Semantics: TopLevelVarDeclaredNames
            // https://tc39.es/ecma262/#sec-block-static-semantics-toplevelvardeclarednames
            //
            // StatementList => StatementListItem => Declaration
            //   1. If Declaration is Declaration : HoistableDeclaration, then
            //     a. Return the BoundNames of HoistableDeclaration.
            //   2. Return a new empty List.
            //
            // HoistableDeclaration => FunctionDeclaration
            //   1. Return the BoundNames of BindingIdentifier.
            //   1. Return « "*default*" ».
            //
            // and
            //
            // Static Semantics: TopLevelVarDeclaredNames
            // https://tc39.es/ecma262/#sec-block-static-semantics-toplevelvardeclarednames
            //
            // StatementList => StatementListItem => Statement
            //   1. If Statement is Statement : LabelledStatement, return
            //      TopLevelVarDeclaredNames of Statement.
            //
            // Static Semantics: TopLevelVarDeclaredNames
            // https://tc39.es/ecma262/#sec-labelled-statements-static-semantics-toplevelvardeclarednames
            //
            // LabelledStatement => LabelledItem => Statement
            //   1. If Statement is Statement : LabelledStatement, return
            //      TopLevelVarDeclaredNames of Statement.
            //   2. Return VarDeclaredNames of Statement.
            //
            // LabelledStatement => LabelledItem => FunctionDeclaration
            //   1. Return BoundNames of FunctionDeclaration.
            DeclarationKind::BodyLevelFunction |

            // Static Semantics: TopLevelVarDeclaredNames
            // https://tc39.es/ecma262/#sec-block-static-semantics-toplevelvardeclarednames
            //
            // StatementList => StatementListItem => Statement
            //   2. Return VarDeclaredNames of Statement.
            //
            // and
            //
            // Static Semantics: TopLevelVarDeclaredNames
            // https://tc39.es/ecma262/#sec-block-static-semantics-toplevelvardeclarednames
            //
            // StatementList => StatementListItem => Statement
            //   1. If Statement is Statement : LabelledStatement, return
            //      TopLevelVarDeclaredNames of Statement.
            //
            // Static Semantics: TopLevelVarDeclaredNames
            // https://tc39.es/ecma262/#sec-labelled-statements-static-semantics-toplevelvardeclarednames
            //
            // LabelledStatement => LabelledItem => Statement
            //   2. Return VarDeclaredNames of Statement.
            //
            // See Block::is_supported_var for the details.
            DeclarationKind::Var |
            DeclarationKind::VarForAnnexBLexicalFunction => true,
            _ => false,
        }
    }
}

impl LexicalEarlyErrorsContext for InternalFunctionBodyEarlyErrorsContext {
    fn declare_lex<'alloc>(
        &mut self,
        name: SourceAtomSetIndex,
        kind: DeclarationKind,
        offset: usize,
        atoms: &SourceAtomSet<'alloc>,
    ) -> EarlyErrorsResult<'alloc> {
        debug_assert!(Self::is_supported_lexical(kind));

        // Static Semantics: Early Errors
        // https://tc39.es/ecma262/#sec-function-definitions-static-semantics-early-errors
        //
        // FunctionBody : FunctionStatementList
        //
        // * It is a Syntax Error if the LexicallyDeclaredNames of
        //   FunctionStatementList contains any duplicate entries.
        if let Some(info) = self.lex_names_of_body.get(&name) {
            let name = atoms.get(name);
            return Err(ParseError::DuplicateBinding(
                name,
                info.kind,
                info.offset,
                kind,
                offset,
            ));
        }

        // Static Semantics: Early Errors
        // https://tc39.es/ecma262/#sec-function-definitions-static-semantics-early-errors
        //
        // FunctionBody : FunctionStatementList
        //
        // * It is a Syntax Error if any element of the LexicallyDeclaredNames
        //   of FunctionStatementList also occurs in the VarDeclaredNames of
        //   FunctionStatementList.
        if let Some(info) = self.var_names_of_body.get(&name) {
            let name = atoms.get(name);
            return Err(ParseError::DuplicateBinding(
                name,
                info.kind,
                info.offset,
                kind,
                offset,
            ));
        }

        self.lex_names_of_body
            .insert(name, DeclarationInfo::new(kind, offset));

        Ok(())
    }
}

impl VarEarlyErrorsContext for InternalFunctionBodyEarlyErrorsContext {
    fn declare_var<'alloc>(
        &mut self,
        name: SourceAtomSetIndex,
        kind: DeclarationKind,
        offset: usize,
        atoms: &SourceAtomSet<'alloc>,
    ) -> EarlyErrorsResult<'alloc> {
        debug_assert!(Self::is_supported_var(kind));

        // Static Semantics: Early Errors
        // https://tc39.es/ecma262/#sec-function-definitions-static-semantics-early-errors
        //
        // FunctionBody : FunctionStatementList
        //
        // * It is a Syntax Error if any element of the LexicallyDeclaredNames
        //   of FunctionStatementList also occurs in the VarDeclaredNames of
        //   FunctionStatementList.
        if let Some(info) = self.lex_names_of_body.get(&name) {
            let name = atoms.get(name);
            return Err(ParseError::DuplicateBinding(
                name,
                info.kind,
                info.offset,
                kind,
                offset,
            ));
        }

        self.var_names_of_body
            .insert(name, DeclarationInfo::new(kind, offset));

        Ok(())
    }
}

impl ControlEarlyErrorsContext for InternalFunctionBodyEarlyErrorsContext {
    fn on_unhandled_break_or_continue<'alloc>(
        &self,
        info: &ControlInfo,
    ) -> EarlyErrorsResult<'alloc> {
        ModuleScriptOrFunctionEarlyErrorsContext::on_unhandled_break_or_continue(info)
    }
}

// Functions with FormalParameters + FunctionBody.
//
// This is used for the following:
//   * function declaration
//   * function expression
//   * generator declaration
//   * generator expression
//   * async generator declaration
//   * async generator expression
//   * async function declaration
//   * async function expression

#[derive(Debug, PartialEq)]
pub struct FunctionBodyEarlyErrorsContext {
    param: FormalParametersEarlyErrorsContext,
    body: InternalFunctionBodyEarlyErrorsContext,
}

impl FunctionBodyEarlyErrorsContext {
    pub fn new(param: FormalParametersEarlyErrorsContext) -> Self {
        Self {
            param,
            body: InternalFunctionBodyEarlyErrorsContext::new(),
        }
    }
}

impl LexicalEarlyErrorsContext for FunctionBodyEarlyErrorsContext {
    fn declare_lex<'alloc>(
        &mut self,
        name: SourceAtomSetIndex,
        kind: DeclarationKind,
        offset: usize,
        atoms: &SourceAtomSet<'alloc>,
    ) -> EarlyErrorsResult<'alloc> {
        // Static Semantics: Early Errors
        // https://tc39.es/ecma262/#sec-function-definitions-static-semantics-early-errors
        //
        // FunctionDeclaration :
        //   function BindingIdentifier ( FormalParameters ) { FunctionBody }
        // FunctionDeclaration :
        //   function ( FormalParameters ) { FunctionBody }
        // FunctionExpression :
        //   function BindingIdentifier_opt ( FormalParameters )
        //   { FunctionBody }
        //
        // * It is a Syntax Error if any element of the BoundNames of
        //   FormalParameters also occurs in the LexicallyDeclaredNames of
        //   FunctionBody.
        //
        // and
        //
        // Static Semantics: Early Errors
        // https://tc39.es/ecma262/#sec-generator-function-definitions-static-semantics-early-errors
        //
        // GeneratorDeclaration :
        //   function * BindingIdentifier ( FormalParameters ) { GeneratorBody }
        // GeneratorDeclaration :
        //   function * ( FormalParameters ) { GeneratorBody }
        // GeneratorExpression :
        //   function * BindingIdentifier_opt ( FormalParameters )
        //   { GeneratorBody }
        //
        // * It is a Syntax Error if any element of the BoundNames of
        //   FormalParameters also occurs in the LexicallyDeclaredNames of
        //   GeneratorBody.
        //
        // and
        //
        // Static Semantics: Early Errors
        // https://tc39.es/ecma262/#sec-async-generator-function-definitions-static-semantics-early-errors
        //
        // AsyncGeneratorDeclaration :
        //   async function * BindingIdentifier ( FormalParameters )
        //   { AsyncGeneratorBody }
        // AsyncGeneratorDeclaration :
        //   async function * ( FormalParameters ) { AsyncGeneratorBody }
        // AsyncGeneratorExpression :
        //   async function * BindingIdentifier_opt ( FormalParameters )
        //   { AsyncGeneratorBody }
        //
        // * It is a Syntax Error if any element of the BoundNames of
        //   FormalParameters also occurs in the LexicallyDeclaredNames of
        //   AsyncGeneratorBody.
        //
        // and
        //
        // Static Semantics: Early Errors
        // https://tc39.es/ecma262/#sec-async-function-definitions-static-semantics-early-errors
        //
        // AsyncFunctionDeclaration :
        //   async function BindingIdentifier ( FormalParameters )
        //   { AsyncFunctionBody }
        // AsyncFunctionDeclaration :
        //   async function ( FormalParameters ) { AsyncFunctionBody }
        // AsyncFunctionExpression :
        //   async function ( FormalParameters ) { AsyncFunctionBody }
        // AsyncFunctionExpression :
        //   async function BindingIdentifier ( FormalParameters )
        //   { AsyncFunctionBody }
        //
        // * It is a Syntax Error if any element of the BoundNames of
        //   FormalParameters also occurs in the LexicallyDeclaredNames of
        //   AsyncFunctionBody.
        if let Some(info) = self.param.bound_names_of_params.get(&name) {
            let name = atoms.get(name);
            return Err(ParseError::DuplicateBinding(
                name,
                info.kind,
                info.offset,
                kind,
                offset,
            ));
        }

        self.body.declare_lex(name, kind, offset, atoms)
    }
}

impl VarEarlyErrorsContext for FunctionBodyEarlyErrorsContext {
    fn declare_var<'alloc>(
        &mut self,
        name: SourceAtomSetIndex,
        kind: DeclarationKind,
        offset: usize,
        atoms: &SourceAtomSet<'alloc>,
    ) -> EarlyErrorsResult<'alloc> {
        self.body.declare_var(name, kind, offset, atoms)
    }
}

impl ControlEarlyErrorsContext for FunctionBodyEarlyErrorsContext {
    fn on_unhandled_break_or_continue<'alloc>(
        &self,
        info: &ControlInfo,
    ) -> EarlyErrorsResult<'alloc> {
        self.body.on_unhandled_break_or_continue(info)
    }
}

// Functions with UniqueFormalParameters + FunctionBody
//
// This is used for the following:
//   * arrow function
//   * method definition
//   * setter
//   * generator method
//   * async generator method
//   * async method
//   * async arrow function

#[derive(Debug, PartialEq)]
pub struct UniqueFunctionBodyEarlyErrorsContext {
    param: UniqueFormalParametersEarlyErrorsContext,
    body: InternalFunctionBodyEarlyErrorsContext,
}

impl UniqueFunctionBodyEarlyErrorsContext {
    pub fn new(param: UniqueFormalParametersEarlyErrorsContext) -> Self {
        Self {
            param,
            body: InternalFunctionBodyEarlyErrorsContext::new(),
        }
    }
}

impl LexicalEarlyErrorsContext for UniqueFunctionBodyEarlyErrorsContext {
    fn declare_lex<'alloc>(
        &mut self,
        name: SourceAtomSetIndex,
        kind: DeclarationKind,
        offset: usize,
        atoms: &SourceAtomSet<'alloc>,
    ) -> EarlyErrorsResult<'alloc> {
        // Static Semantics: Early Errors
        // https://tc39.es/ecma262/#sec-arrow-function-definitions-static-semantics-early-errors
        //
        // ArrowFunction : ArrowParameters => ConciseBody
        //
        // * It is a Syntax Error if any element of the BoundNames of
        //   ArrowParameters also occurs in the LexicallyDeclaredNames of
        //   ConciseBody.
        //
        // and
        //
        // Static Semantics: Early Errors
        // https://tc39.es/ecma262/#sec-method-definitions-static-semantics-early-errors
        //
        // MethodDefinition :
        //   PropertyName ( UniqueFormalParameters ) { FunctionBody }
        //
        // * It is a Syntax Error if any element of the BoundNames of
        //   UniqueFormalParameters also occurs in the LexicallyDeclaredNames of
        //   FunctionBody.
        //
        // MethodDefinition :
        //   set PropertyName ( PropertySetParameterList ) { FunctionBody }
        //
        // * It is a Syntax Error if any element of the BoundNames of
        //   PropertySetParameterList also occurs in the LexicallyDeclaredNames
        //   of FunctionBody.
        //
        // and
        //
        // Static Semantics: Early Errors
        // https://tc39.es/ecma262/#sec-generator-function-definitions-static-semantics-early-errors
        //
        // GeneratorMethod :
        //   * PropertyName ( UniqueFormalParameters ) { GeneratorBody }
        //
        // * It is a Syntax Error if any element of the BoundNames of
        //   UniqueFormalParameters also occurs in the LexicallyDeclaredNames of
        //   GeneratorBody.
        //
        // and
        //
        // Static Semantics: Early Errors
        // https://tc39.es/ecma262/#sec-async-generator-function-definitions-static-semantics-early-errors
        //
        // AsyncGeneratorMethod :
        //   async * PropertyName ( UniqueFormalParameters )
        //   { AsyncGeneratorBody }
        //
        // * It is a Syntax Error if any element of the BoundNames of
        //   UniqueFormalParameters also occurs in the LexicallyDeclaredNames of
        //   AsyncGeneratorBody.
        //
        // and
        //
        // Static Semantics: Early Errors
        // https://tc39.es/ecma262/#sec-async-function-definitions-static-semantics-early-errors
        //
        // AsyncMethod :
        //   async PropertyName ( UniqueFormalParameters ) { AsyncFunctionBody }
        //
        // * It is a Syntax Error if any element of the BoundNames of
        //  UniqueFormalParameters also occurs in the LexicallyDeclaredNames of
        //   AsyncFunctionBody.
        //
        // and
        //
        // Static Semantics: Early Errors
        // https://tc39.es/ecma262/#sec-async-arrow-function-definitions-static-semantics-early-errors
        //
        // AsyncArrowFunction :
        //   async AsyncArrowBindingIdentifier => AsyncConciseBody
        //
        // * It is a Syntax Error if any element of the BoundNames of
        //   AsyncArrowBindingIdentifier also occurs in the
        //   LexicallyDeclaredNames of AsyncConciseBody.
        //
        // AsyncArrowFunction :
        //   CoverCallExpressionAndAsyncArrowHead => AsyncConciseBody
        //
        // * It is a Syntax Error if any element of the BoundNames of
        //   CoverCallExpressionAndAsyncArrowHead also occurs in the
        //   LexicallyDeclaredNames of AsyncConciseBody.
        if let Some(info) = self.param.bound_names_of_params.get(&name) {
            let name = atoms.get(name);
            return Err(ParseError::DuplicateBinding(
                name,
                info.kind,
                info.offset,
                kind,
                offset,
            ));
        }

        self.body.declare_lex(name, kind, offset, atoms)
    }
}

impl VarEarlyErrorsContext for UniqueFunctionBodyEarlyErrorsContext {
    fn declare_var<'alloc>(
        &mut self,
        name: SourceAtomSetIndex,
        kind: DeclarationKind,
        offset: usize,
        atoms: &SourceAtomSet<'alloc>,
    ) -> EarlyErrorsResult<'alloc> {
        self.body.declare_var(name, kind, offset, atoms)
    }
}

impl ControlEarlyErrorsContext for UniqueFunctionBodyEarlyErrorsContext {
    fn on_unhandled_break_or_continue<'alloc>(
        &self,
        info: &ControlInfo,
    ) -> EarlyErrorsResult<'alloc> {
        self.body.on_unhandled_break_or_continue(info)
    }
}

// ===========================================================================
// Scripts
// https://tc39.es/ecma262/#sec-scripts
// ===========================================================================

#[derive(Debug, PartialEq)]
pub struct ScriptEarlyErrorsContext {
    lex_names_of_body: HashMap<SourceAtomSetIndex, DeclarationInfo>,
    var_names_of_body: HashMap<SourceAtomSetIndex, DeclarationInfo>,
}

impl ScriptEarlyErrorsContext {
    pub fn new() -> Self {
        Self {
            lex_names_of_body: HashMap::new(),
            var_names_of_body: HashMap::new(),
        }
    }

    fn is_supported_lexical(kind: DeclarationKind) -> bool {
        match kind {
            // LexicallyDeclaredNames of ScriptBody
            //
            // Static Semantics: LexicallyDeclaredNames
            // https://tc39.es/ecma262/#sec-scripts-static-semantics-lexicallydeclarednames
            //
            // ScriptBody => StatementList
            //   1. Return TopLevelLexicallyDeclaredNames of StatementList.
            // StatementList => StatementListItem => Declaration
            //   1. If Declaration is Declaration : HoistableDeclaration, then
            //     a. Return « ».
            //   2. Return the BoundNames of Declaration.
            //
            // See Block::is_supported_lexical for the details.
            DeclarationKind::Class | DeclarationKind::Let | DeclarationKind::Const => true,
            _ => false,
        }
    }

    fn is_supported_var(kind: DeclarationKind) -> bool {
        match kind {
            // VarDeclaredNames of ScriptBody
            //
            // Static Semantics: VarDeclaredNames
            // https://tc39.es/ecma262/#sec-scripts-static-semantics-vardeclarednames
            //
            // ScriptBody => StatementList
            //   1. Return TopLevelVarDeclaredNames of StatementList.
            //
            // See Block::is_supported_var for the detail.
            DeclarationKind::Var
            | DeclarationKind::BodyLevelFunction
            | DeclarationKind::VarForAnnexBLexicalFunction => true,
            _ => false,
        }
    }
}

impl LexicalEarlyErrorsContext for ScriptEarlyErrorsContext {
    fn declare_lex<'alloc>(
        &mut self,
        name: SourceAtomSetIndex,
        kind: DeclarationKind,
        offset: usize,
        atoms: &SourceAtomSet<'alloc>,
    ) -> EarlyErrorsResult<'alloc> {
        debug_assert!(Self::is_supported_lexical(kind));

        // Static Semantics: Early Errors
        // https://tc39.es/ecma262/#sec-scripts-static-semantics-early-errors
        //
        // Script : ScriptBody
        //
        // * It is a Syntax Error if the LexicallyDeclaredNames of ScriptBody
        //   contains any duplicate entries.
        if let Some(info) = self.lex_names_of_body.get(&name) {
            let name = atoms.get(name);
            return Err(ParseError::DuplicateBinding(
                name,
                info.kind,
                info.offset,
                kind,
                offset,
            ));
        }

        // Static Semantics: Early Errors
        // https://tc39.es/ecma262/#sec-scripts-static-semantics-early-errors
        //
        // Script : ScriptBody
        //
        // * It is a Syntax Error if any element of the LexicallyDeclaredNames
        //   of ScriptBody also occurs in the VarDeclaredNames of ScriptBody.
        if let Some(info) = self.var_names_of_body.get(&name) {
            let name = atoms.get(name);
            return Err(ParseError::DuplicateBinding(
                name,
                info.kind,
                info.offset,
                kind,
                offset,
            ));
        }

        self.lex_names_of_body
            .insert(name, DeclarationInfo::new(kind, offset));

        Ok(())
    }
}

impl VarEarlyErrorsContext for ScriptEarlyErrorsContext {
    fn declare_var<'alloc>(
        &mut self,
        name: SourceAtomSetIndex,
        kind: DeclarationKind,
        offset: usize,
        atoms: &SourceAtomSet<'alloc>,
    ) -> EarlyErrorsResult<'alloc> {
        debug_assert!(Self::is_supported_var(kind));

        // Static Semantics: Early Errors
        // https://tc39.es/ecma262/#sec-scripts-static-semantics-early-errors
        //
        // Script : ScriptBody
        //
        // * It is a Syntax Error if any element of the LexicallyDeclaredNames
        //   of ScriptBody also occurs in the VarDeclaredNames of ScriptBody.
        if let Some(info) = self.lex_names_of_body.get(&name) {
            let name = atoms.get(name);
            return Err(ParseError::DuplicateBinding(
                name,
                info.kind,
                info.offset,
                kind,
                offset,
            ));
        }

        self.var_names_of_body
            .insert(name, DeclarationInfo::new(kind, offset));

        Ok(())
    }
}

impl ControlEarlyErrorsContext for ScriptEarlyErrorsContext {
    fn on_unhandled_break_or_continue<'alloc>(
        &self,
        info: &ControlInfo,
    ) -> EarlyErrorsResult<'alloc> {
        ModuleScriptOrFunctionEarlyErrorsContext::on_unhandled_break_or_continue(info)
    }
}

// ===========================================================================
// Modules
// https://tc39.es/ecma262/#sec-modules
// ===========================================================================

#[derive(Debug, PartialEq)]
pub struct ModuleEarlyErrorsContext {
    lex_names_of_item_list: HashMap<SourceAtomSetIndex, DeclarationInfo>,
    var_names_of_item_list: HashMap<SourceAtomSetIndex, DeclarationInfo>,
    exported_names_of_item_list: HashMap<SourceAtomSetIndex, usize>,
    exported_bindings_of_item_list: HashMap<SourceAtomSetIndex, usize>,
}

impl ModuleEarlyErrorsContext {
    pub fn new() -> Self {
        Self {
            lex_names_of_item_list: HashMap::new(),
            var_names_of_item_list: HashMap::new(),
            exported_names_of_item_list: HashMap::new(),
            exported_bindings_of_item_list: HashMap::new(),
        }
    }

    fn is_supported_lexical(kind: DeclarationKind) -> bool {
        match kind {
            // LexicallyDeclaredNames of ModuleItemList
            //
            // Static Semantics: LexicallyDeclaredNames
            // https://tc39.es/ecma262/#sec-module-semantics-static-semantics-lexicallydeclarednames
            //
            // ModuleItemList => ModuleItem => ImportDeclaration
            //   1. Return the BoundNames of ImportDeclaration.
            //
            // ImportDeclaration ... => ImportedBinding => BindingIdentifier
            DeclarationKind::Import |

            // ModuleItemList => ModuleItem => ExportDeclaration
            //   1. If ExportDeclaration is export VariableStatement, return a
            //      new empty List.
            //   2. Return the BoundNames of ExportDeclaration.
            //
            // ExportDeclaration => Declaration
            //   1. Return the BoundNames of Declaration.

            // ExportDeclaration => HoistableDeclaration
            //   1. Let declarationNames be the BoundNames of
            //      HoistableDeclaration.
            //   2. If declarationNames does not include the element
            //      "*default*", append "*default*" to declarationNames.
            //   3. Return declarationNames.

            // ExportDeclaration => ClassDeclaration
            //   1. Let declarationNames be the BoundNames of ClassDeclaration.
            //   2. If declarationNames does not include the element
            //      "*default*", append "*default*" to declarationNames.
            //   3. Return declarationNames.

            // ExportDeclaration => AssignmentExpression
            //   1. Return « "*default*" ».

            // ModuleItemList => ModuleItem => StatementListItem
            //
            // See Block::is_supported_lexical for the details.
            //
            // Function declaration in the top level of module script is
            // lexical, but here isn't LexicalFunction/LexicalAsyncOrGenerator
            // distinction because B.3.3.4 doesn't apply to Module.
            DeclarationKind::BodyLevelFunction |
            DeclarationKind::Class |
            DeclarationKind::Let |
            DeclarationKind::Const => true,
            _ => false,
        }
    }

    fn is_supported_var(kind: DeclarationKind) -> bool {
        match kind {
            // VarDeclaredNames of ModuleItemList
            //
            // Static Semantics: VarDeclaredNames
            // https://tc39.es/ecma262/#sec-module-semantics-static-semantics-vardeclarednames
            //
            // ModuleItemList => ModuleItem => ImportDeclaration
            //   1. Return a new empty List.

            // ModuleItemList => ModuleItem => ExportDeclaration
            //   1. If ExportDeclaration is export VariableStatement, return
            //      BoundNames of ExportDeclaration.
            //   2. Return a new empty List.
            //
            // and
            //
            // ModuleItemList => ModuleItem => StatementList
            DeclarationKind::Var => true,
            _ => false,
        }
    }

    #[allow(dead_code)]
    pub fn add_exported_name<'alloc>(
        &mut self,
        name: SourceAtomSetIndex,
        offset: usize,
        atoms: &SourceAtomSet<'alloc>,
    ) -> EarlyErrorsResult<'alloc> {
        // Static Semantics: Early Errors
        // https://tc39.es/ecma262/#sec-module-semantics-static-semantics-early-errors
        //
        // ModuleBody : ModuleItemList
        //
        // * It is a Syntax Error if the ExportedNames of ModuleItemList
        //   contains any duplicate entries.
        if let Some(prev_offset) = self.exported_names_of_item_list.get(&name) {
            let name = atoms.get(name);
            return Err(ParseError::DuplicateExport(
                name,
                prev_offset.clone(),
                offset,
            ));
        }

        self.exported_names_of_item_list.insert(name, offset);

        Ok(())
    }

    #[allow(dead_code)]
    pub fn add_exported_binding(&mut self, name: SourceAtomSetIndex, offset: usize) {
        self.exported_bindings_of_item_list.insert(name, offset);
    }

    #[allow(dead_code)]
    pub fn check_exported_name<'alloc>(
        &self,
        atoms: &SourceAtomSet<'alloc>,
    ) -> EarlyErrorsResult<'alloc> {
        // Static Semantics: Early Errors
        // https://tc39.es/ecma262/#sec-module-semantics-static-semantics-early-errors
        //
        // ModuleBody : ModuleItemList
        //
        // * It is a Syntax Error if any element of the ExportedBindings of
        //   ModuleItemList does not also occur in either the VarDeclaredNames
        //   of ModuleItemList, or the LexicallyDeclaredNames of ModuleItemList.
        for (name, offset) in &self.exported_bindings_of_item_list {
            if !self.var_names_of_item_list.contains_key(name)
                && !self.lex_names_of_item_list.contains_key(name)
            {
                let name = atoms.get(*name);
                return Err(ParseError::MissingExport(name, offset.clone()));
            }
        }

        Ok(())
    }
}

impl LexicalEarlyErrorsContext for ModuleEarlyErrorsContext {
    fn declare_lex<'alloc>(
        &mut self,
        name: SourceAtomSetIndex,
        kind: DeclarationKind,
        offset: usize,
        atoms: &SourceAtomSet<'alloc>,
    ) -> EarlyErrorsResult<'alloc> {
        debug_assert!(Self::is_supported_lexical(kind));

        // Static Semantics: Early Errors
        // https://tc39.es/ecma262/#sec-module-semantics-static-semantics-early-errors
        //
        // ModuleBody : ModuleItemList
        //
        // * It is a Syntax Error if the LexicallyDeclaredNames of
        //   ModuleItemList contains any duplicate entries.
        //
        // and
        //
        // Static Semantics: Early Errors
        // https://tc39.es/ecma262/#sec-imports-static-semantics-early-errors
        //
        // ModuleItem : ImportDeclaration
        //
        // * It is a Syntax Error if the BoundNames of ImportDeclaration
        //   contains any duplicate entries.
        if let Some(info) = self.lex_names_of_item_list.get(&name) {
            let name = atoms.get(name);
            return Err(ParseError::DuplicateBinding(
                name,
                info.kind,
                info.offset,
                kind,
                offset,
            ));
        }

        // Static Semantics: Early Errors
        // https://tc39.es/ecma262/#sec-module-semantics-static-semantics-early-errors

        //
        // ModuleBody : ModuleItemList
        //
        // * It is a Syntax Error if any element of the LexicallyDeclaredNames
        //   of ModuleItemList also occurs in the VarDeclaredNames of
        //   ModuleItemList.
        if let Some(info) = self.var_names_of_item_list.get(&name) {
            let name = atoms.get(name);
            return Err(ParseError::DuplicateBinding(
                name,
                info.kind,
                info.offset,
                kind,
                offset,
            ));
        }

        self.lex_names_of_item_list
            .insert(name, DeclarationInfo::new(kind, offset));

        Ok(())
    }
}

impl VarEarlyErrorsContext for ModuleEarlyErrorsContext {
    fn declare_var<'alloc>(
        &mut self,
        name: SourceAtomSetIndex,
        kind: DeclarationKind,
        offset: usize,
        atoms: &SourceAtomSet<'alloc>,
    ) -> EarlyErrorsResult<'alloc> {
        debug_assert!(Self::is_supported_var(kind));

        // Static Semantics: Early Errors
        // https://tc39.es/ecma262/#sec-module-semantics-static-semantics-early-errors
        //
        // ModuleBody : ModuleItemList
        //
        // * It is a Syntax Error if any element of the LexicallyDeclaredNames
        //   of ModuleItemList also occurs in the VarDeclaredNames of
        //   ModuleItemList.
        if let Some(info) = self.lex_names_of_item_list.get(&name) {
            let name = atoms.get(name);
            return Err(ParseError::DuplicateBinding(
                name,
                info.kind,
                info.offset,
                kind,
                offset,
            ));
        }

        self.var_names_of_item_list
            .insert(name, DeclarationInfo::new(kind, offset));

        Ok(())
    }
}

impl ControlEarlyErrorsContext for ModuleEarlyErrorsContext {
    fn on_unhandled_break_or_continue<'alloc>(
        &self,
        info: &ControlInfo,
    ) -> EarlyErrorsResult<'alloc> {
        ModuleScriptOrFunctionEarlyErrorsContext::on_unhandled_break_or_continue(info)
    }
}

struct ModuleScriptOrFunctionEarlyErrorsContext {}

impl ModuleScriptOrFunctionEarlyErrorsContext {
    fn on_unhandled_break_or_continue<'alloc>(info: &ControlInfo) -> EarlyErrorsResult<'alloc> {
        if let Some(_) = info.label {
            match info.kind {
                // Static Semantics: Early Errors
                // https://tc39.es/ecma262/#sec-scripts-static-semantics-early-errors
                //
                // Script : ScriptBody
                //
                // * It is a Syntax Error if ContainsUndefinedContinueTarget of StatementList
                //   with arguments « » and « » is true.
                //
                // https://tc39.es/ecma262/#sec-module-semantics-static-semantics-early-errors
                //
                // ModuleBody : ModuleItemList
                //
                // * It is a Syntax Error if ContainsUndefinedContinueTarget of ModuleItemList
                //   with arguments « » and « » is true.
                ControlKind::Continue => {
                    return Err(ParseError::BadContinue);
                }

                // Static Semantics: Early Errors
                // https://tc39.es/ecma262/#sec-scripts-static-semantics-early-errors
                //
                // Script : ScriptBody
                //
                // * It is a Syntax Error if ContainsUndefinedBreakTarget of StatementList
                //   with argument « » is true.
                //
                // https://tc39.es/ecma262/#sec-module-semantics-static-semantics-early-errors
                //
                // ModuleBody : ModuleItemList
                //
                // * It is a Syntax Error if ContainsUndefinedBreakTarget of ModuleItemList
                //   with argument « » is true.
                ControlKind::Break => {
                    return Err(ParseError::LabelNotFound);
                }
            }
        } else {
            match info.kind {
                // Static Semantics: Early Errors
                // https://tc39.es/ecma262/#sec-continue-statement-static-semantics-early-errors
                //
                // ContinueStatement : continue ;
                // ContinueStatement : continueLabelIdentifier ;
                //
                // * It is a Syntax Error if this ContinueStatement is not nested, directly or
                //   indirectly (but not crossing function boundaries), within an
                //   IterationStatement.
                ControlKind::Continue => {
                    return Err(ParseError::BadContinue);
                }
                // Static Semantics: Early Errors
                // https://tc39.es/ecma262/#sec-break-statement-static-semantics-early-errors
                //
                //  BreakStatement : break ;
                //
                // * It is a Syntax Error if this BreakStatement is not nested, directly or
                //   indirectly (but not crossing function boundaries), within an
                //   IterationStatement or a SwitchStatement.
                ControlKind::Break => {
                    return Err(ParseError::ToughBreak);
                }
            }
        }
    }
}
