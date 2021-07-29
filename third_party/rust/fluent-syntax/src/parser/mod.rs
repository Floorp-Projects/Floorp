#[macro_use]
mod errors;
mod comment;
mod expression;
mod helper;
mod pattern;
mod slice;

use crate::ast;
use slice::Slice;
use std::result;

pub use errors::{ErrorKind, ParserError};

pub type Result<T> = result::Result<T, ParserError>;

pub struct Parser<S> {
    source: S,
    ptr: usize,
    length: usize,
}

impl<'s, S> Parser<S>
where
    S: Slice<'s>,
{
    pub fn new(source: S) -> Self {
        let length = source.as_ref().len();
        Self {
            source,
            ptr: 0,
            length,
        }
    }

    pub fn parse(
        &mut self,
    ) -> std::result::Result<ast::Resource<S>, (ast::Resource<S>, Vec<ParserError>)> {
        let mut errors = vec![];

        let mut body = vec![];

        self.skip_blank_block();
        let mut last_comment = None;
        let mut last_blank_count = 0;

        while self.ptr < self.length {
            let entry_start = self.ptr;
            let mut entry = self.get_entry(entry_start);

            if let Some(comment) = last_comment.take() {
                match entry {
                    Ok(ast::Entry::Message(ref mut msg)) if last_blank_count < 2 => {
                        msg.comment = Some(comment);
                    }
                    Ok(ast::Entry::Term(ref mut term)) if last_blank_count < 2 => {
                        term.comment = Some(comment);
                    }
                    _ => {
                        body.push(ast::Entry::Comment(comment));
                    }
                }
            }

            match entry {
                Ok(ast::Entry::Comment(comment)) => {
                    last_comment = Some(comment);
                }
                Ok(entry) => {
                    body.push(entry);
                }
                Err(mut err) => {
                    self.skip_to_next_entry_start();
                    err.slice = Some((entry_start, self.ptr));
                    errors.push(err);
                    let content = self.source.slice(entry_start..self.ptr);
                    body.push(ast::Entry::Junk { content });
                }
            }
            last_blank_count = self.skip_blank_block();
        }

        if let Some(last_comment) = last_comment.take() {
            body.push(ast::Entry::Comment(last_comment));
        }
        if errors.is_empty() {
            Ok(ast::Resource { body })
        } else {
            Err((ast::Resource { body }, errors))
        }
    }

    fn get_entry(&mut self, entry_start: usize) -> Result<ast::Entry<S>> {
        let entry = match self.source.as_ref().as_bytes().get(self.ptr) {
            Some(b'#') => {
                let (comment, level) = self.get_comment()?;
                match level {
                    comment::Level::Regular => ast::Entry::Comment(comment),
                    comment::Level::Group => ast::Entry::GroupComment(comment),
                    comment::Level::Resource => ast::Entry::ResourceComment(comment),
                    comment::Level::None => unreachable!(),
                }
            }
            Some(b'-') => ast::Entry::Term(self.get_term(entry_start)?),
            _ => ast::Entry::Message(self.get_message(entry_start)?),
        };
        Ok(entry)
    }

    fn get_message(&mut self, entry_start: usize) -> Result<ast::Message<S>> {
        let id = self.get_identifier()?;
        self.skip_blank_inline();
        self.expect_byte(b'=')?;
        let pattern = self.get_pattern()?;

        self.skip_blank_block();

        let attributes = self.get_attributes();

        if pattern.is_none() && attributes.is_empty() {
            let entry_id = id.name.as_ref().to_owned();
            return error!(
                ErrorKind::ExpectedMessageField { entry_id },
                entry_start, self.ptr
            );
        }

        Ok(ast::Message {
            id,
            value: pattern,
            attributes,
            comment: None,
        })
    }

    fn get_term(&mut self, entry_start: usize) -> Result<ast::Term<S>> {
        self.expect_byte(b'-')?;
        let id = self.get_identifier()?;
        self.skip_blank_inline();
        self.expect_byte(b'=')?;
        self.skip_blank_inline();

        let value = self.get_pattern()?;

        self.skip_blank_block();

        let attributes = self.get_attributes();

        if let Some(value) = value {
            Ok(ast::Term {
                id,
                value,
                attributes,
                comment: None,
            })
        } else {
            error!(
                ErrorKind::ExpectedTermField {
                    entry_id: id.name.as_ref().to_owned()
                },
                entry_start, self.ptr
            )
        }
    }

    fn get_attributes(&mut self) -> Vec<ast::Attribute<S>> {
        let mut attributes = vec![];

        loop {
            let line_start = self.ptr;
            self.skip_blank_inline();
            if !self.is_current_byte(b'.') {
                self.ptr = line_start;
                break;
            }

            if let Ok(attr) = self.get_attribute() {
                attributes.push(attr);
            } else {
                self.ptr = line_start;
                break;
            }
        }
        attributes
    }

    fn get_attribute(&mut self) -> Result<ast::Attribute<S>> {
        self.expect_byte(b'.')?;
        let id = self.get_identifier()?;
        self.skip_blank_inline();
        self.expect_byte(b'=')?;
        let pattern = self.get_pattern()?;

        match pattern {
            Some(pattern) => Ok(ast::Attribute { id, value: pattern }),
            None => error!(ErrorKind::MissingValue, self.ptr),
        }
    }

    fn get_identifier(&mut self) -> Result<ast::Identifier<S>> {
        let mut ptr = self.ptr;

        match self.source.as_ref().as_bytes().get(ptr) {
            Some(b) if b.is_ascii_alphabetic() => {
                ptr += 1;
            }
            _ => {
                return error!(
                    ErrorKind::ExpectedCharRange {
                        range: "a-zA-Z".to_string()
                    },
                    ptr
                );
            }
        }

        while let Some(b) = self.source.as_ref().as_bytes().get(ptr) {
            if b.is_ascii_alphabetic() || b.is_ascii_digit() || [b'_', b'-'].contains(b) {
                ptr += 1;
            } else {
                break;
            }
        }

        let name = self.source.slice(self.ptr..ptr);
        self.ptr = ptr;

        Ok(ast::Identifier { name })
    }

    fn get_attribute_accessor(&mut self) -> Result<Option<ast::Identifier<S>>> {
        if self.take_byte_if(b'.') {
            let ident = self.get_identifier()?;
            Ok(Some(ident))
        } else {
            Ok(None)
        }
    }

    fn get_variant_key(&mut self) -> Result<ast::VariantKey<S>> {
        if !self.take_byte_if(b'[') {
            return error!(ErrorKind::ExpectedToken('['), self.ptr);
        }
        self.skip_blank();

        let key = if self.is_number_start() {
            ast::VariantKey::NumberLiteral {
                value: self.get_number_literal()?,
            }
        } else {
            ast::VariantKey::Identifier {
                name: self.get_identifier()?.name,
            }
        };

        self.skip_blank();

        self.expect_byte(b']')?;

        Ok(key)
    }

    fn get_variants(&mut self) -> Result<Vec<ast::Variant<S>>> {
        let mut variants = vec![];
        let mut has_default = false;

        while self.is_current_byte(b'*') || self.is_current_byte(b'[') {
            let default = self.take_byte_if(b'*');

            if default {
                if has_default {
                    return error!(ErrorKind::MultipleDefaultVariants, self.ptr);
                } else {
                    has_default = true;
                }
            }

            let key = self.get_variant_key()?;

            let value = self.get_pattern()?;

            if let Some(value) = value {
                variants.push(ast::Variant {
                    key,
                    value,
                    default,
                });
                self.skip_blank();
            } else {
                return error!(ErrorKind::MissingValue, self.ptr);
            }
        }

        if has_default {
            Ok(variants)
        } else {
            error!(ErrorKind::MissingDefaultVariant, self.ptr)
        }
    }

    fn get_placeable(&mut self) -> Result<ast::Expression<S>> {
        self.expect_byte(b'{')?;
        self.skip_blank();
        let exp = self.get_expression()?;
        self.skip_blank_inline();
        self.expect_byte(b'}')?;

        let invalid_expression_found = match &exp {
            ast::Expression::InlineExpression(ast::InlineExpression::TermReference {
                ref attribute,
                ..
            }) => attribute.is_some(),
            _ => false,
        };
        if invalid_expression_found {
            return error!(ErrorKind::TermAttributeAsPlaceable, self.ptr);
        }

        Ok(exp)
    }

    fn get_call_arguments(&mut self) -> Result<Option<ast::CallArguments<S>>> {
        self.skip_blank();
        if !self.take_byte_if(b'(') {
            return Ok(None);
        }

        let mut positional = vec![];
        let mut named = vec![];
        let mut argument_names = vec![];

        self.skip_blank();

        while self.ptr < self.length {
            if self.is_current_byte(b')') {
                break;
            }

            let expr = self.get_inline_expression()?;

            if let ast::InlineExpression::MessageReference {
                ref id,
                attribute: None,
            } = expr
            {
                self.skip_blank();
                if self.is_current_byte(b':') {
                    if argument_names.contains(&id.name) {
                        return error!(
                            ErrorKind::DuplicatedNamedArgument(id.name.as_ref().to_owned()),
                            self.ptr
                        );
                    }
                    self.ptr += 1;
                    self.skip_blank();
                    let val = self.get_inline_expression()?;

                    argument_names.push(id.name.clone());
                    named.push(ast::NamedArgument {
                        name: ast::Identifier {
                            name: id.name.clone(),
                        },
                        value: val,
                    });
                } else {
                    if !argument_names.is_empty() {
                        return error!(ErrorKind::PositionalArgumentFollowsNamed, self.ptr);
                    }
                    positional.push(expr);
                }
            } else {
                if !argument_names.is_empty() {
                    return error!(ErrorKind::PositionalArgumentFollowsNamed, self.ptr);
                }
                positional.push(expr);
            }

            self.skip_blank();
            self.take_byte_if(b',');
            self.skip_blank();
        }

        self.expect_byte(b')')?;

        Ok(Some(ast::CallArguments { positional, named }))
    }
}
