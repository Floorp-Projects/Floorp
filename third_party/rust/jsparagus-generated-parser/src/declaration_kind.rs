// The kind of declaration.
//
// This is used for error reporting and also for handling early error check.
#[derive(Clone, Copy, Debug, PartialEq)]
pub enum DeclarationKind {
    FormalParameter,

    Var,
    Let,
    Const,

    Class,

    Import,

    BodyLevelFunction,

    LexicalFunction,
    LexicalAsyncOrGenerator,

    // This is used after parsing the entire function/script body.
    VarForAnnexBLexicalFunction,

    CatchParameter,
}

impl DeclarationKind {
    pub fn to_str(&self) -> &'static str {
        match self {
            Self::FormalParameter => "formal parameter",
            Self::Var => "var",
            Self::Let => "let",
            Self::Const => "const",
            Self::Class => "class",
            Self::Import => "import",
            Self::BodyLevelFunction => "function",
            Self::LexicalFunction => "function",
            Self::LexicalAsyncOrGenerator => "function",
            Self::VarForAnnexBLexicalFunction => "function",
            Self::CatchParameter => "catch parameter",
        }
    }
}
