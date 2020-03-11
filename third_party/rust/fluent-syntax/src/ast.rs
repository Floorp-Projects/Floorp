#[derive(Debug, PartialEq)]
pub struct Resource<'ast> {
    pub body: Vec<ResourceEntry<'ast>>,
}

#[derive(Debug, PartialEq)]
pub enum ResourceEntry<'ast> {
    Entry(Entry<'ast>),
    Junk(&'ast str),
}

#[derive(Debug, PartialEq)]
pub enum Entry<'ast> {
    Message(Message<'ast>),
    Term(Term<'ast>),
    Comment(Comment<'ast>),
}

#[derive(Debug, PartialEq)]
pub struct Message<'ast> {
    pub id: Identifier<'ast>,
    pub value: Option<Pattern<'ast>>,
    pub attributes: Vec<Attribute<'ast>>,
    pub comment: Option<Comment<'ast>>,
}

#[derive(Debug, PartialEq)]
pub struct Term<'ast> {
    pub id: Identifier<'ast>,
    pub value: Pattern<'ast>,
    pub attributes: Vec<Attribute<'ast>>,
    pub comment: Option<Comment<'ast>>,
}

#[derive(Debug, PartialEq)]
pub struct Pattern<'ast> {
    pub elements: Vec<PatternElement<'ast>>,
}

#[derive(Debug, PartialEq)]
pub enum PatternElement<'ast> {
    TextElement(&'ast str),
    Placeable(Expression<'ast>),
}

#[derive(Debug, PartialEq)]
pub struct Attribute<'ast> {
    pub id: Identifier<'ast>,
    pub value: Pattern<'ast>,
}

#[derive(Debug, PartialEq)]
pub struct Identifier<'ast> {
    pub name: &'ast str,
}

#[derive(Debug, PartialEq)]
pub struct Variant<'ast> {
    pub key: VariantKey<'ast>,
    pub value: Pattern<'ast>,
    pub default: bool,
}

#[derive(Debug, PartialEq)]
pub enum VariantKey<'ast> {
    Identifier { name: &'ast str },
    NumberLiteral { value: &'ast str },
}

#[derive(Debug, PartialEq)]
pub enum Comment<'ast> {
    Comment { content: Vec<&'ast str> },
    GroupComment { content: Vec<&'ast str> },
    ResourceComment { content: Vec<&'ast str> },
}

#[derive(Debug, PartialEq)]
pub enum InlineExpression<'ast> {
    StringLiteral {
        value: &'ast str,
    },
    NumberLiteral {
        value: &'ast str,
    },
    FunctionReference {
        id: Identifier<'ast>,
        arguments: Option<CallArguments<'ast>>,
    },
    MessageReference {
        id: Identifier<'ast>,
        attribute: Option<Identifier<'ast>>,
    },
    TermReference {
        id: Identifier<'ast>,
        attribute: Option<Identifier<'ast>>,
        arguments: Option<CallArguments<'ast>>,
    },
    VariableReference {
        id: Identifier<'ast>,
    },
    Placeable {
        expression: Box<Expression<'ast>>,
    },
}

#[derive(Debug, PartialEq)]
pub struct CallArguments<'ast> {
    pub positional: Vec<InlineExpression<'ast>>,
    pub named: Vec<NamedArgument<'ast>>,
}

#[derive(Debug, PartialEq)]
pub struct NamedArgument<'ast> {
    pub name: Identifier<'ast>,
    pub value: InlineExpression<'ast>,
}

#[derive(Debug, PartialEq)]
pub enum Expression<'ast> {
    InlineExpression(InlineExpression<'ast>),
    SelectExpression {
        selector: InlineExpression<'ast>,
        variants: Vec<Variant<'ast>>,
    },
}
