use super::errors::{ErrorKind, ParserError};
use super::{core::Parser, core::Result, slice::Slice};
use crate::ast;

impl<'s, S> Parser<S>
where
    S: Slice<'s>,
{
    pub(super) fn get_expression(&mut self) -> Result<ast::Expression<S>> {
        let exp = self.get_inline_expression(false)?;

        self.skip_blank();

        if !self.is_current_byte(b'-') || !self.is_byte_at(b'>', self.ptr + 1) {
            if let ast::InlineExpression::TermReference { ref attribute, .. } = exp {
                if attribute.is_some() {
                    return error!(ErrorKind::TermAttributeAsPlaceable, self.ptr);
                }
            }
            return Ok(ast::Expression::Inline(exp));
        }

        match exp {
            ast::InlineExpression::MessageReference { ref attribute, .. } => {
                if attribute.is_none() {
                    return error!(ErrorKind::MessageReferenceAsSelector, self.ptr);
                } else {
                    return error!(ErrorKind::MessageAttributeAsSelector, self.ptr);
                }
            }
            ast::InlineExpression::TermReference { ref attribute, .. } => {
                if attribute.is_none() {
                    return error!(ErrorKind::TermReferenceAsSelector, self.ptr);
                }
            }
            ast::InlineExpression::StringLiteral { .. }
            | ast::InlineExpression::NumberLiteral { .. }
            | ast::InlineExpression::VariableReference { .. }
            | ast::InlineExpression::FunctionReference { .. } => {}
            _ => {
                return error!(ErrorKind::ExpectedSimpleExpressionAsSelector, self.ptr);
            }
        };

        self.ptr += 2; // ->

        self.skip_blank_inline();
        if !self.skip_eol() {
            return error!(
                ErrorKind::ExpectedCharRange {
                    range: "\n | \r\n".to_string()
                },
                self.ptr
            );
        }
        self.skip_blank();

        let variants = self.get_variants()?;

        Ok(ast::Expression::Select {
            selector: exp,
            variants,
        })
    }

    pub(super) fn get_inline_expression(
        &mut self,
        only_literal: bool,
    ) -> Result<ast::InlineExpression<S>> {
        match get_current_byte!(self) {
            Some(b'"') => {
                self.ptr += 1; // "
                let start = self.ptr;
                while let Some(b) = get_current_byte!(self) {
                    match b {
                        b'\\' => match get_byte!(self, self.ptr + 1) {
                            Some(b'\\') | Some(b'{') | Some(b'"') => self.ptr += 2,
                            Some(b'u') => {
                                self.ptr += 2;
                                self.skip_unicode_escape_sequence(4)?;
                            }
                            Some(b'U') => {
                                self.ptr += 2;
                                self.skip_unicode_escape_sequence(6)?;
                            }
                            b => {
                                let seq = b.unwrap_or(&b' ').to_string();
                                return error!(ErrorKind::UnknownEscapeSequence(seq), self.ptr);
                            }
                        },
                        b'"' => {
                            break;
                        }
                        b'\n' => {
                            return error!(ErrorKind::UnterminatedStringLiteral, self.ptr);
                        }
                        _ => self.ptr += 1,
                    }
                }

                self.expect_byte(b'"')?;
                let slice = self.source.slice(start..self.ptr - 1);
                Ok(ast::InlineExpression::StringLiteral { value: slice })
            }
            Some(b) if b.is_ascii_digit() => {
                let num = self.get_number_literal()?;
                Ok(ast::InlineExpression::NumberLiteral { value: num })
            }
            Some(b'-') if !only_literal => {
                self.ptr += 1; // -
                if self.is_identifier_start() {
                    self.ptr += 1;
                    let id = self.get_identifier_unchecked();
                    let attribute = self.get_attribute_accessor()?;
                    let arguments = self.get_call_arguments()?;
                    Ok(ast::InlineExpression::TermReference {
                        id,
                        attribute,
                        arguments,
                    })
                } else {
                    self.ptr -= 1;
                    let num = self.get_number_literal()?;
                    Ok(ast::InlineExpression::NumberLiteral { value: num })
                }
            }
            Some(b'$') if !only_literal => {
                self.ptr += 1; // $
                let id = self.get_identifier()?;
                Ok(ast::InlineExpression::VariableReference { id })
            }
            Some(b) if b.is_ascii_alphabetic() => {
                self.ptr += 1;
                let id = self.get_identifier_unchecked();
                let arguments = self.get_call_arguments()?;
                if let Some(arguments) = arguments {
                    if !Self::is_callee(&id.name) {
                        return error!(ErrorKind::ForbiddenCallee, self.ptr);
                    }

                    Ok(ast::InlineExpression::FunctionReference { id, arguments })
                } else {
                    let attribute = self.get_attribute_accessor()?;
                    Ok(ast::InlineExpression::MessageReference { id, attribute })
                }
            }
            Some(b'{') if !only_literal => {
                self.ptr += 1; // {
                let exp = self.get_placeable()?;
                Ok(ast::InlineExpression::Placeable {
                    expression: Box::new(exp),
                })
            }
            _ if only_literal => error!(ErrorKind::ExpectedLiteral, self.ptr),
            _ => error!(ErrorKind::ExpectedInlineExpression, self.ptr),
        }
    }

    pub fn get_call_arguments(&mut self) -> Result<Option<ast::CallArguments<S>>> {
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

            let expr = self.get_inline_expression(false)?;

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
                    let val = self.get_inline_expression(true)?;

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
