use super::errors::{ErrorKind, ParserError};
use super::{Parser, Result, Slice};
use crate::ast;

impl<'s, S> Parser<S>
where
    S: Slice<'s>,
{
    pub(super) fn get_expression(&mut self) -> Result<ast::Expression<S>> {
        let exp = self.get_inline_expression()?;

        self.skip_blank();

        if !self.is_current_byte(b'-') || !self.is_byte_at(b'>', self.ptr + 1) {
            if let ast::InlineExpression::TermReference { ref attribute, .. } = exp {
                if attribute.is_some() {
                    return error!(ErrorKind::TermAttributeAsPlaceable, self.ptr);
                }
            }
            return Ok(ast::Expression::InlineExpression(exp));
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

        Ok(ast::Expression::SelectExpression {
            selector: exp,
            variants,
        })
    }

    pub(super) fn get_inline_expression(&mut self) -> Result<ast::InlineExpression<S>> {
        match self.source.as_ref().as_bytes().get(self.ptr) {
            Some(b'"') => {
                self.ptr += 1; // "
                let start = self.ptr;
                while let Some(b) = self.source.as_ref().as_bytes().get(self.ptr) {
                    match b {
                        b'\\' => match self.source.as_ref().as_bytes().get(self.ptr + 1) {
                            Some(b'\\') | Some(b'{') | Some(b'"') => self.ptr += 2,
                            Some(b'u') => {
                                self.ptr += 2;
                                self.skip_unicode_escape_sequence(4)?;
                            }
                            Some(b'U') => {
                                self.ptr += 2;
                                self.skip_unicode_escape_sequence(6)?;
                            }
                            _ => return error!(ErrorKind::Generic, self.ptr),
                        },
                        b'"' => {
                            break;
                        }
                        b'\n' => {
                            return error!(ErrorKind::Generic, self.ptr);
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
            Some(b'-') => {
                self.ptr += 1; // -
                if self.is_identifier_start() {
                    let id = self.get_identifier()?;
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
            Some(b'$') => {
                self.ptr += 1; // -
                let id = self.get_identifier()?;
                Ok(ast::InlineExpression::VariableReference { id })
            }
            Some(b) if b.is_ascii_alphabetic() => {
                let id = self.get_identifier()?;
                let arguments = self.get_call_arguments()?;
                if arguments.is_some() {
                    if !Self::is_callee(id.name.as_ref().as_bytes()) {
                        return error!(ErrorKind::ForbiddenCallee, self.ptr);
                    }

                    Ok(ast::InlineExpression::FunctionReference { id, arguments })
                } else {
                    let attribute = self.get_attribute_accessor()?;
                    Ok(ast::InlineExpression::MessageReference { id, attribute })
                }
            }
            Some(b'{') => {
                let exp = self.get_placeable()?;
                Ok(ast::InlineExpression::Placeable {
                    expression: Box::new(exp),
                })
            }
            _ => error!(ErrorKind::ExpectedInlineExpression, self.ptr),
        }
    }
}
