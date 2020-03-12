use crate::ast;
use serde::ser::SerializeMap;
use serde::ser::SerializeSeq;
use serde::{Serialize, Serializer};
use std::error::Error;

pub fn serialize<'s>(res: &'s ast::Resource) -> Result<String, Box<dyn Error>> {
    #[derive(Serialize)]
    struct Helper<'ast>(#[serde(with = "ResourceDef")] &'ast ast::Resource<'ast>);
    Ok(serde_json::to_string(&Helper(res)).unwrap())
}

pub fn serialize_to_pretty_json<'s>(res: &'s ast::Resource) -> Result<String, Box<dyn Error>> {
    #[derive(Serialize)]
    struct Helper<'ast>(#[serde(with = "ResourceDef")] &'ast ast::Resource<'ast>);

    let buf = Vec::new();
    let formatter = serde_json::ser::PrettyFormatter::with_indent(b"    ");
    let mut ser = serde_json::Serializer::with_formatter(buf, formatter);
    Helper(res).serialize(&mut ser).unwrap();
    Ok(String::from_utf8(ser.into_inner()).unwrap())
}

#[derive(Serialize)]
#[serde(remote = "ast::Resource")]
#[serde(tag = "type")]
#[serde(rename = "Resource")]
pub struct ResourceDef<'ast> {
    #[serde(serialize_with = "serialize_resource_entry_vec")]
    pub body: Vec<ast::ResourceEntry<'ast>>,
}

fn serialize_resource_entry_vec<'se, S>(
    v: &Vec<ast::ResourceEntry<'se>>,
    serializer: S,
) -> Result<S::Ok, S::Error>
where
    S: Serializer,
{
    #[derive(Serialize)]
    #[serde(tag = "type")]
    enum EntryHelper<'ast> {
        Junk {
            annotations: Vec<&'ast str>,
            content: &'ast str,
        },
        #[serde(with = "MessageDef")]
        Message(&'ast ast::Message<'ast>),
        #[serde(with = "TermDef")]
        Term(&'ast ast::Term<'ast>),
        Comment {
            content: String,
        },
        GroupComment {
            content: String,
        },
        ResourceComment {
            content: String,
        },
    }

    let mut seq = serializer.serialize_seq(Some(v.len()))?;
    for e in v {
        let entry = match *e {
            ast::ResourceEntry::Entry(ref entry) => match entry {
                ast::Entry::Message(ref msg) => EntryHelper::Message(msg),
                ast::Entry::Term(ref term) => EntryHelper::Term(term),
                ast::Entry::Comment(ast::Comment::Comment { ref content }) => {
                    EntryHelper::Comment {
                        content: content.join("\n"),
                    }
                }
                ast::Entry::Comment(ast::Comment::GroupComment { ref content }) => {
                    EntryHelper::GroupComment {
                        content: content.join("\n"),
                    }
                }
                ast::Entry::Comment(ast::Comment::ResourceComment { ref content }) => {
                    EntryHelper::ResourceComment {
                        content: content.join("\n"),
                    }
                }
            },
            ast::ResourceEntry::Junk(ref junk) => EntryHelper::Junk {
                content: junk,
                annotations: vec![],
            },
        };
        seq.serialize_element(&entry)?;
    }
    seq.end()
}

#[derive(Serialize)]
#[serde(remote = "ast::Message")]
pub struct MessageDef<'ast> {
    #[serde(with = "IdentifierDef")]
    pub id: ast::Identifier<'ast>,
    #[serde(serialize_with = "serialize_pattern_option")]
    pub value: Option<ast::Pattern<'ast>>,
    #[serde(serialize_with = "serialize_attribute_vec")]
    pub attributes: Vec<ast::Attribute<'ast>>,
    #[serde(serialize_with = "serialize_comment_option")]
    pub comment: Option<ast::Comment<'ast>>,
}

#[derive(Serialize)]
#[serde(remote = "ast::Identifier")]
#[serde(tag = "type")]
#[serde(rename = "Identifier")]
pub struct IdentifierDef<'ast> {
    pub name: &'ast str,
}

#[derive(Serialize)]
#[serde(remote = "ast::Variant")]
#[serde(tag = "type")]
#[serde(rename = "Variant")]
pub struct VariantDef<'ast> {
    #[serde(with = "VariantKeyDef")]
    pub key: ast::VariantKey<'ast>,
    #[serde(with = "PatternDef")]
    pub value: ast::Pattern<'ast>,
    pub default: bool,
}

#[derive(Serialize)]
#[serde(remote = "ast::Term")]
pub struct TermDef<'ast> {
    #[serde(with = "IdentifierDef")]
    pub id: ast::Identifier<'ast>,
    #[serde(with = "PatternDef")]
    pub value: ast::Pattern<'ast>,
    #[serde(serialize_with = "serialize_attribute_vec")]
    pub attributes: Vec<ast::Attribute<'ast>>,
    #[serde(serialize_with = "serialize_comment_option")]
    pub comment: Option<ast::Comment<'ast>>,
}

fn serialize_pattern_option<'se, S>(
    v: &Option<ast::Pattern<'se>>,
    serializer: S,
) -> Result<S::Ok, S::Error>
where
    S: Serializer,
{
    #[derive(Serialize)]
    struct Helper<'ast>(#[serde(with = "PatternDef")] &'ast ast::Pattern<'ast>);
    v.as_ref().map(Helper).serialize(serializer)
}

fn serialize_attribute_vec<'se, S>(
    v: &Vec<ast::Attribute<'se>>,
    serializer: S,
) -> Result<S::Ok, S::Error>
where
    S: Serializer,
{
    #[derive(Serialize)]
    struct Helper<'ast>(#[serde(with = "AttributeDef")] &'ast ast::Attribute<'ast>);
    let mut seq = serializer.serialize_seq(Some(v.len()))?;
    for e in v {
        seq.serialize_element(&Helper(e))?;
    }
    seq.end()
}

fn serialize_comment_option<'se, S>(
    v: &Option<ast::Comment<'se>>,
    serializer: S,
) -> Result<S::Ok, S::Error>
where
    S: Serializer,
{
    #[derive(Serialize)]
    struct Helper<'ast>(#[serde(with = "CommentDef")] &'ast ast::Comment<'ast>);
    v.as_ref().map(Helper).serialize(serializer)
}

#[derive(Serialize)]
#[serde(remote = "ast::Pattern")]
#[serde(tag = "type")]
#[serde(rename = "Pattern")]
pub struct PatternDef<'ast> {
    #[serde(serialize_with = "serialize_pattern_elements")]
    pub elements: Vec<ast::PatternElement<'ast>>,
}

fn serialize_pattern_elements<'se, S>(
    v: &Vec<ast::PatternElement<'se>>,
    serializer: S,
) -> Result<S::Ok, S::Error>
where
    S: Serializer,
{
    #[derive(Serialize)]
    struct Helper<'ast>(#[serde(with = "PatternElementDef")] &'ast ast::PatternElement<'ast>);
    let mut seq = serializer.serialize_seq(Some(v.len()))?;
    let mut buffer = String::new();
    for e in v {
        match e {
            ast::PatternElement::TextElement(e) => {
                buffer.push_str(e);
            }
            _ => {
                if !buffer.is_empty() {
                    seq.serialize_element(&Helper(&ast::PatternElement::TextElement(&buffer)))?;
                    buffer = String::new();
                }

                seq.serialize_element(&Helper(e))?;
            }
        }
    }
    if !buffer.is_empty() {
        seq.serialize_element(&Helper(&ast::PatternElement::TextElement(&buffer)))?;
    }
    seq.end()
}

#[derive(Serialize)]
#[serde(remote = "ast::PatternElement")]
#[serde(untagged)]
pub enum PatternElementDef<'ast> {
    #[serde(serialize_with = "serialize_text_element")]
    TextElement(&'ast str),
    #[serde(serialize_with = "serialize_placeable")]
    Placeable(ast::Expression<'ast>),
}

fn serialize_text_element<'se, S>(s: &'se str, serializer: S) -> Result<S::Ok, S::Error>
where
    S: Serializer,
{
    let mut map = serializer.serialize_map(Some(2))?;
    map.serialize_entry("type", "TextElement")?;
    map.serialize_entry("value", s)?;
    map.end()
}

fn serialize_placeable<'se, S>(exp: &ast::Expression<'se>, serializer: S) -> Result<S::Ok, S::Error>
where
    S: Serializer,
{
    #[derive(Serialize)]
    struct Helper<'ast>(#[serde(with = "ExpressionDef")] &'ast ast::Expression<'ast>);
    let mut map = serializer.serialize_map(Some(2))?;
    map.serialize_entry("type", "Placeable")?;
    map.serialize_entry("expression", &Helper(exp))?;
    map.end()
}

#[derive(Serialize, Debug)]
#[serde(remote = "ast::VariantKey")]
#[serde(tag = "type")]
pub enum VariantKeyDef<'ast> {
    Identifier { name: &'ast str },
    NumberLiteral { value: &'ast str },
}

#[derive(Serialize)]
#[serde(remote = "ast::Comment")]
#[serde(tag = "type")]
pub enum CommentDef<'ast> {
    Comment {
        #[serde(serialize_with = "serialize_comment_content")]
        content: Vec<&'ast str>,
    },
    GroupComment {
        #[serde(serialize_with = "serialize_comment_content")]
        content: Vec<&'ast str>,
    },
    ResourceComment {
        #[serde(serialize_with = "serialize_comment_content")]
        content: Vec<&'ast str>,
    },
}

fn serialize_comment_content<'se, S>(v: &Vec<&'se str>, serializer: S) -> Result<S::Ok, S::Error>
where
    S: Serializer,
{
    serializer.serialize_str(&v.join("\n"))
}

#[derive(Serialize)]
#[serde(remote = "ast::InlineExpression")]
#[serde(tag = "type")]
pub enum InlineExpressionDef<'ast> {
    StringLiteral {
        value: &'ast str,
    },
    NumberLiteral {
        value: &'ast str,
    },
    FunctionReference {
        #[serde(with = "IdentifierDef")]
        id: ast::Identifier<'ast>,
        #[serde(serialize_with = "serialize_call_arguments_option")]
        arguments: Option<ast::CallArguments<'ast>>,
    },
    MessageReference {
        #[serde(with = "IdentifierDef")]
        id: ast::Identifier<'ast>,
        #[serde(serialize_with = "serialize_identifier_option")]
        attribute: Option<ast::Identifier<'ast>>,
    },
    TermReference {
        #[serde(with = "IdentifierDef")]
        id: ast::Identifier<'ast>,
        #[serde(serialize_with = "serialize_identifier_option")]
        attribute: Option<ast::Identifier<'ast>>,
        #[serde(serialize_with = "serialize_call_arguments_option")]
        arguments: Option<ast::CallArguments<'ast>>,
    },
    VariableReference {
        #[serde(with = "IdentifierDef")]
        id: ast::Identifier<'ast>,
    },
    Placeable {
        #[serde(with = "ExpressionDef")]
        expression: ast::Expression<'ast>,
    },
}

fn serialize_call_arguments_option<'se, S>(
    v: &Option<ast::CallArguments<'se>>,
    serializer: S,
) -> Result<S::Ok, S::Error>
where
    S: Serializer,
{
    #[derive(Serialize)]
    struct Helper<'ast>(#[serde(with = "CallArgumentsDef")] &'ast ast::CallArguments<'ast>);
    v.as_ref().map(Helper).serialize(serializer)
}

fn serialize_identifier_option<'se, S>(
    v: &Option<ast::Identifier<'se>>,
    serializer: S,
) -> Result<S::Ok, S::Error>
where
    S: Serializer,
{
    #[derive(Serialize)]
    struct Helper<'ast>(#[serde(with = "IdentifierDef")] &'ast ast::Identifier<'ast>);
    v.as_ref().map(Helper).serialize(serializer)
}

#[derive(Serialize)]
#[serde(remote = "ast::Attribute")]
#[serde(tag = "type")]
#[serde(rename = "Attribute")]
pub struct AttributeDef<'ast> {
    #[serde(with = "IdentifierDef")]
    pub id: ast::Identifier<'ast>,
    #[serde(with = "PatternDef")]
    pub value: ast::Pattern<'ast>,
}

#[derive(Serialize)]
#[serde(remote = "ast::CallArguments")]
#[serde(tag = "type")]
#[serde(rename = "CallArguments")]
pub struct CallArgumentsDef<'ast> {
    #[serde(serialize_with = "serialize_inline_expressions")]
    pub positional: Vec<ast::InlineExpression<'ast>>,
    #[serde(serialize_with = "serialize_named_arguments")]
    pub named: Vec<ast::NamedArgument<'ast>>,
}

#[derive(Serialize)]
#[serde(remote = "ast::NamedArgument")]
#[serde(tag = "type")]
#[serde(rename = "NamedArgument")]
pub struct NamedArgumentDef<'ast> {
    #[serde(with = "IdentifierDef")]
    pub name: ast::Identifier<'ast>,
    #[serde(with = "InlineExpressionDef")]
    pub value: ast::InlineExpression<'ast>,
}

#[derive(Serialize)]
#[serde(remote = "ast::Expression")]
#[serde(tag = "type")]
pub enum ExpressionDef<'ast> {
    #[serde(with = "InlineExpressionDef")]
    InlineExpression(ast::InlineExpression<'ast>),
    SelectExpression {
        #[serde(with = "InlineExpressionDef")]
        selector: ast::InlineExpression<'ast>,
        #[serde(serialize_with = "serialize_variants")]
        variants: Vec<ast::Variant<'ast>>,
    },
}

fn serialize_variants<'se, S>(v: &Vec<ast::Variant<'se>>, serializer: S) -> Result<S::Ok, S::Error>
where
    S: Serializer,
{
    #[derive(Serialize)]
    struct Helper<'ast>(#[serde(with = "VariantDef")] &'ast ast::Variant<'ast>);
    let mut seq = serializer.serialize_seq(Some(v.len()))?;
    for e in v {
        seq.serialize_element(&Helper(e))?;
    }
    seq.end()
}

fn serialize_inline_expressions<'se, S>(
    v: &Vec<ast::InlineExpression<'se>>,
    serializer: S,
) -> Result<S::Ok, S::Error>
where
    S: Serializer,
{
    #[derive(Serialize)]
    struct Helper<'ast>(#[serde(with = "InlineExpressionDef")] &'ast ast::InlineExpression<'ast>);
    let mut seq = serializer.serialize_seq(Some(v.len()))?;
    for e in v {
        seq.serialize_element(&Helper(e))?;
    }
    seq.end()
}

fn serialize_named_arguments<'se, S>(
    v: &Vec<ast::NamedArgument<'se>>,
    serializer: S,
) -> Result<S::Ok, S::Error>
where
    S: Serializer,
{
    #[derive(Serialize)]
    struct Helper<'ast>(#[serde(with = "NamedArgumentDef")] &'ast ast::NamedArgument<'ast>);
    let mut seq = serializer.serialize_seq(Some(v.len()))?;
    for e in v {
        seq.serialize_element(&Helper(e))?;
    }
    seq.end()
}
