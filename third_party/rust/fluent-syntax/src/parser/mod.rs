#[macro_use]
pub mod errors;
mod ftlstream;

use std::cmp;
use std::result;
use std::str;

use self::errors::ErrorKind;
pub use self::errors::ParserError;
use self::ftlstream::ParserStream;
use super::ast;

pub type Result<T> = result::Result<T, ParserError>;

pub fn parse(source: &str) -> result::Result<ast::Resource, (ast::Resource, Vec<ParserError>)> {
    let mut errors = vec![];

    let mut ps = ParserStream::new(source);

    let mut body = vec![];

    ps.skip_blank_block();
    let mut last_comment = None;
    let mut last_blank_count = 0;

    while ps.ptr < ps.length {
        let entry_start = ps.ptr;
        let mut entry = get_entry(&mut ps, entry_start);

        if let Some(comment) = last_comment.take() {
            match entry {
                Ok(ast::Entry::Message(ref mut msg)) if last_blank_count < 2 => {
                    msg.comment = Some(comment);
                }
                Ok(ast::Entry::Term(ref mut term)) if last_blank_count < 2 => {
                    term.comment = Some(comment);
                }
                _ => {
                    body.push(ast::ResourceEntry::Entry(ast::Entry::Comment(comment)));
                }
            }
        }

        match entry {
            Ok(ast::Entry::Comment(comment @ ast::Comment::Comment { .. })) => {
                last_comment = Some(comment);
            }
            Ok(entry) => {
                body.push(ast::ResourceEntry::Entry(entry));
            }
            Err(mut err) => {
                ps.skip_to_next_entry_start();
                err.slice = Some((entry_start, ps.ptr));
                errors.push(err);
                let slice = ps.get_slice(entry_start, ps.ptr);
                body.push(ast::ResourceEntry::Junk(slice));
            }
        }
        last_blank_count = ps.skip_blank_block();
    }

    if let Some(last_comment) = last_comment.take() {
        body.push(ast::ResourceEntry::Entry(ast::Entry::Comment(last_comment)));
    }
    if errors.is_empty() {
        Ok(ast::Resource { body })
    } else {
        Err((ast::Resource { body }, errors))
    }
}

fn get_entry<'p>(ps: &mut ParserStream<'p>, entry_start: usize) -> Result<ast::Entry<'p>> {
    let entry = match ps.source[ps.ptr] {
        b'#' => ast::Entry::Comment(get_comment(ps)?),
        b'-' => ast::Entry::Term(get_term(ps, entry_start)?),
        _ => ast::Entry::Message(get_message(ps, entry_start)?),
    };
    Ok(entry)
}

fn get_message<'p>(ps: &mut ParserStream<'p>, entry_start: usize) -> Result<ast::Message<'p>> {
    let id = get_identifier(ps)?;
    ps.skip_blank_inline();
    ps.expect_byte(b'=')?;

    let pattern = get_pattern(ps)?;

    ps.skip_blank_block();

    let attributes = get_attributes(ps);

    if pattern.is_none() && attributes.is_empty() {
        return error!(
            ErrorKind::ExpectedMessageField {
                entry_id: id.name.to_string()
            },
            entry_start, ps.ptr
        );
    }

    Ok(ast::Message {
        id,
        value: pattern,
        attributes,
        comment: None,
    })
}

fn get_term<'p>(ps: &mut ParserStream<'p>, entry_start: usize) -> Result<ast::Term<'p>> {
    ps.expect_byte(b'-')?;
    let id = get_identifier(ps)?;
    ps.skip_blank_inline();
    ps.expect_byte(b'=')?;
    ps.skip_blank_inline();

    let value = get_pattern(ps)?;

    ps.skip_blank_block();

    let attributes = get_attributes(ps);

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
                entry_id: id.name.to_string()
            },
            entry_start, ps.ptr
        )
    }
}

fn get_attributes<'p>(ps: &mut ParserStream<'p>) -> Vec<ast::Attribute<'p>> {
    let mut attributes = vec![];

    loop {
        let line_start = ps.ptr;
        ps.skip_blank_inline();
        if !ps.is_current_byte(b'.') {
            ps.ptr = line_start;
            break;
        }

        match get_attribute(ps) {
            Ok(attr) => attributes.push(attr),
            Err(_) => {
                ps.ptr = line_start;
                break;
            }
        }
    }
    attributes
}

fn get_attribute<'p>(ps: &mut ParserStream<'p>) -> Result<ast::Attribute<'p>> {
    ps.expect_byte(b'.')?;
    let id = get_identifier(ps)?;
    ps.skip_blank_inline();
    ps.expect_byte(b'=')?;
    let pattern = get_pattern(ps)?;

    match pattern {
        Some(pattern) => Ok(ast::Attribute { id, value: pattern }),
        None => error!(ErrorKind::MissingValue, ps.ptr),
    }
}

fn get_identifier<'p>(ps: &mut ParserStream<'p>) -> Result<ast::Identifier<'p>> {
    let mut ptr = ps.ptr;

    match ps.source.get(ptr) {
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

    while let Some(b) = ps.source.get(ptr) {
        if b.is_ascii_alphabetic() || b.is_ascii_digit() || [b'_', b'-'].contains(b) {
            ptr += 1;
        } else {
            break;
        }
    }

    let name = ps.get_slice(ps.ptr, ptr);
    ps.ptr = ptr;

    Ok(ast::Identifier { name })
}

fn get_attribute_accessor<'p>(ps: &mut ParserStream<'p>) -> Result<Option<ast::Identifier<'p>>> {
    if !ps.take_byte_if(b'.') {
        Ok(None)
    } else {
        let ident = get_identifier(ps)?;
        Ok(Some(ident))
    }
}

fn get_variant_key<'p>(ps: &mut ParserStream<'p>) -> Result<ast::VariantKey<'p>> {
    if !ps.take_byte_if(b'[') {
        return error!(ErrorKind::ExpectedToken('['), ps.ptr);
    }
    ps.skip_blank();

    let key = if ps.is_number_start() {
        ast::VariantKey::NumberLiteral {
            value: get_number_literal(ps)?,
        }
    } else {
        ast::VariantKey::Identifier {
            name: get_identifier(ps)?.name,
        }
    };

    ps.skip_blank();

    ps.expect_byte(b']')?;

    Ok(key)
}

fn get_variants<'p>(ps: &mut ParserStream<'p>) -> Result<Vec<ast::Variant<'p>>> {
    let mut variants = vec![];
    let mut has_default = false;

    while ps.is_current_byte(b'*') || ps.is_current_byte(b'[') {
        let default = ps.take_byte_if(b'*');

        if default {
            if has_default {
                return error!(ErrorKind::MultipleDefaultVariants, ps.ptr);
            } else {
                has_default = true;
            }
        }

        let key = get_variant_key(ps)?;

        let value = get_pattern(ps)?;

        if let Some(value) = value {
            variants.push(ast::Variant {
                key,
                value,
                default,
            });
            ps.skip_blank();
        } else {
            return error!(ErrorKind::MissingValue, ps.ptr);
        }
    }

    if !has_default {
        error!(ErrorKind::MissingDefaultVariant, ps.ptr)
    } else {
        Ok(variants)
    }
}

// This enum tracks the reason for which a text slice ended.
// It is used by the pattern to set the proper state for the next line.
//
// CRLF variant is specific because we want to skip the CR but keep the LF in text elements
// production.
// For example `a\r\n b` will produce (`a`, `\n` and ` b`) TextElements.
#[derive(Debug, PartialEq)]
enum TextElementTermination {
    LineFeed,
    CRLF,
    PlaceableStart,
    EOF,
}

// This enum tracks the placement of the text element in the pattern, which is needed for
// dedentation logic.
#[derive(Debug, PartialEq)]
enum TextElementPosition {
    InitialLineStart,
    LineStart,
    Continuation,
}

// This enum allows us to mark pointers in the source which will later become text elements
// but without slicing them out of the source string. This makes the indentation adjustments
// cheaper since they'll happen on the pointers, rather than extracted slices.
#[derive(Debug)]
enum PatternElementPlaceholders<'a> {
    Placeable(ast::Expression<'a>),
    // (start, end, indent, position)
    TextElement(usize, usize, usize, TextElementPosition),
}

// This enum tracks whether the text element is blank or not.
// This is important to identify text elements which should not be taken into account
// when calculating common indent.
#[derive(Debug, PartialEq)]
enum TextElementType {
    Blank,
    NonBlank,
}

fn get_pattern<'p>(ps: &mut ParserStream<'p>) -> Result<Option<ast::Pattern<'p>>> {
    let mut elements = vec![];
    let mut last_non_blank = None;
    let mut common_indent = None;

    ps.skip_blank_inline();

    let mut text_element_role = if ps.skip_eol() {
        ps.skip_blank_block();
        TextElementPosition::LineStart
    } else {
        TextElementPosition::InitialLineStart
    };

    while ps.ptr < ps.length {
        if ps.is_current_byte(b'{') {
            if text_element_role == TextElementPosition::LineStart {
                common_indent = Some(0);
            }
            let exp = get_placeable(ps)?;
            last_non_blank = Some(elements.len());
            elements.push(PatternElementPlaceholders::Placeable(exp));
            text_element_role = TextElementPosition::Continuation;
        } else {
            let slice_start = ps.ptr;
            let mut indent = 0;
            if text_element_role == TextElementPosition::LineStart {
                indent = ps.skip_blank_inline();
                if ps.ptr >= ps.length {
                    break;
                }
                let b = ps.source[ps.ptr];
                if indent == 0 {
                    if b != b'\n' {
                        break;
                    }
                } else if !ps.is_byte_pattern_continuation(b) {
                    ps.ptr = slice_start;
                    break;
                }
            }
            let (start, end, text_element_type, termination_reason) = get_text_slice(ps)?;
            if start != end {
                if text_element_role == TextElementPosition::LineStart
                    && text_element_type == TextElementType::NonBlank
                {
                    if let Some(common) = common_indent {
                        if indent < common {
                            common_indent = Some(indent);
                        }
                    } else {
                        common_indent = Some(indent);
                    }
                }
                if text_element_role != TextElementPosition::LineStart
                    || text_element_type == TextElementType::NonBlank
                    || termination_reason == TextElementTermination::LineFeed
                {
                    if text_element_type == TextElementType::NonBlank {
                        last_non_blank = Some(elements.len());
                    }
                    elements.push(PatternElementPlaceholders::TextElement(
                        slice_start,
                        end,
                        indent,
                        text_element_role,
                    ));
                }
            }

            text_element_role = match termination_reason {
                TextElementTermination::LineFeed => TextElementPosition::LineStart,
                TextElementTermination::CRLF => TextElementPosition::Continuation,
                TextElementTermination::PlaceableStart => TextElementPosition::Continuation,
                TextElementTermination::EOF => TextElementPosition::Continuation,
            };
        }
    }

    if let Some(last_non_blank) = last_non_blank {
        let elements = elements
            .into_iter()
            .take(last_non_blank + 1)
            .enumerate()
            .map(|(i, elem)| match elem {
                PatternElementPlaceholders::Placeable(exp) => ast::PatternElement::Placeable(exp),
                PatternElementPlaceholders::TextElement(start, end, indent, role) => {
                    let start = if role == TextElementPosition::LineStart {
                        if let Some(common_indent) = common_indent {
                            start + cmp::min(indent, common_indent)
                        } else {
                            start + indent
                        }
                    } else {
                        start
                    };
                    let slice = ps.get_slice(start, end);
                    if last_non_blank == i {
                        ast::PatternElement::TextElement(slice.trim_end())
                    } else {
                        ast::PatternElement::TextElement(slice)
                    }
                }
            })
            .collect();
        return Ok(Some(ast::Pattern { elements }));
    }

    Ok(None)
}

fn get_text_slice<'p>(
    ps: &mut ParserStream<'p>,
) -> Result<(usize, usize, TextElementType, TextElementTermination)> {
    let start_pos = ps.ptr;
    let mut text_element_type = TextElementType::Blank;

    while ps.ptr < ps.length {
        match ps.source[ps.ptr] {
            b' ' => ps.ptr += 1,
            b'\n' => {
                ps.ptr += 1;
                return Ok((
                    start_pos,
                    ps.ptr,
                    text_element_type,
                    TextElementTermination::LineFeed,
                ));
            }
            b'\r' if ps.is_byte_at(b'\n', ps.ptr + 1) => {
                ps.ptr += 1;
                return Ok((
                    start_pos,
                    ps.ptr - 1,
                    text_element_type,
                    TextElementTermination::CRLF,
                ));
            }
            b'{' => {
                return Ok((
                    start_pos,
                    ps.ptr,
                    text_element_type,
                    TextElementTermination::PlaceableStart,
                ));
            }
            b'}' => {
                return error!(ErrorKind::UnbalancedClosingBrace, ps.ptr);
            }
            _ => {
                text_element_type = TextElementType::NonBlank;
                ps.ptr += 1
            }
        }
    }
    Ok((
        start_pos,
        ps.ptr,
        text_element_type,
        TextElementTermination::EOF,
    ))
}

fn get_comment<'p>(ps: &mut ParserStream<'p>) -> Result<ast::Comment<'p>> {
    let mut level = None;
    let mut content = vec![];

    while ps.ptr < ps.length {
        let line_level = get_comment_level(ps);
        if line_level == 0 {
            ps.ptr -= 1;
            break;
        }
        if level.is_some() && Some(line_level) != level {
            ps.ptr -= line_level;
            break;
        }

        level = Some(line_level);

        if ps.ptr == ps.length {
            break;
        } else if ps.is_current_byte(b'\n') {
            content.push(get_comment_line(ps)?);
        } else {
            if let Err(e) = ps.expect_byte(b' ') {
                if content.is_empty() {
                    return Err(e);
                } else {
                    ps.ptr -= line_level;
                    break;
                }
            }
            content.push(get_comment_line(ps)?);
        }
        ps.skip_eol();
    }

    let comment = match level {
        Some(3) => ast::Comment::ResourceComment { content },
        Some(2) => ast::Comment::GroupComment { content },
        _ => ast::Comment::Comment { content },
    };
    Ok(comment)
}

fn get_comment_level<'p>(ps: &mut ParserStream<'p>) -> usize {
    let mut chars = 0;

    while ps.take_byte_if(b'#') {
        chars += 1;
    }

    chars
}

fn get_comment_line<'p>(ps: &mut ParserStream<'p>) -> Result<&'p str> {
    let start_pos = ps.ptr;

    while ps.ptr < ps.length && !ps.is_eol() {
        ps.ptr += 1;
    }

    Ok(ps.get_slice(start_pos, ps.ptr))
}

fn get_placeable<'p>(ps: &mut ParserStream<'p>) -> Result<ast::Expression<'p>> {
    ps.expect_byte(b'{')?;
    ps.skip_blank();
    let exp = get_expression(ps)?;
    ps.skip_blank_inline();
    ps.expect_byte(b'}')?;

    let invalid_expression_found = match &exp {
        ast::Expression::InlineExpression(ast::InlineExpression::TermReference {
            ref attribute,
            ..
        }) => attribute.is_some(),
        _ => false,
    };
    if invalid_expression_found {
        return error!(ErrorKind::TermAttributeAsPlaceable, ps.ptr);
    }

    Ok(exp)
}

fn get_expression<'p>(ps: &mut ParserStream<'p>) -> Result<ast::Expression<'p>> {
    let exp = get_inline_expression(ps)?;

    ps.skip_blank();

    if !ps.is_current_byte(b'-') || !ps.is_byte_at(b'>', ps.ptr + 1) {
        if let ast::InlineExpression::TermReference { ref attribute, .. } = exp {
            if attribute.is_some() {
                return error!(ErrorKind::TermAttributeAsPlaceable, ps.ptr);
            }
        }
        return Ok(ast::Expression::InlineExpression(exp));
    }

    match exp {
        ast::InlineExpression::MessageReference { ref attribute, .. } => {
            if attribute.is_none() {
                return error!(ErrorKind::MessageReferenceAsSelector, ps.ptr);
            } else {
                return error!(ErrorKind::MessageAttributeAsSelector, ps.ptr);
            }
        }
        ast::InlineExpression::TermReference { ref attribute, .. } => {
            if attribute.is_none() {
                return error!(ErrorKind::TermReferenceAsSelector, ps.ptr);
            }
        }
        ast::InlineExpression::StringLiteral { .. }
        | ast::InlineExpression::NumberLiteral { .. }
        | ast::InlineExpression::VariableReference { .. }
        | ast::InlineExpression::FunctionReference { .. } => {}
        _ => {
            return error!(ErrorKind::ExpectedSimpleExpressionAsSelector, ps.ptr);
        }
    };

    ps.ptr += 2; // ->

    ps.skip_blank_inline();
    ps.expect_byte(b'\n')?;
    ps.skip_blank();

    let variants = get_variants(ps)?;

    Ok(ast::Expression::SelectExpression {
        selector: exp,
        variants,
    })
}

fn get_inline_expression<'p>(ps: &mut ParserStream<'p>) -> Result<ast::InlineExpression<'p>> {
    match ps.source.get(ps.ptr) {
        Some(b'"') => {
            ps.ptr += 1; // "
            let start = ps.ptr;
            while ps.ptr < ps.length {
                match ps.source[ps.ptr] {
                    b'\\' => match ps.source.get(ps.ptr + 1) {
                        Some(b'\\') => ps.ptr += 2,
                        Some(b'{') => ps.ptr += 2,
                        Some(b'"') => ps.ptr += 2,
                        Some(b'u') => {
                            ps.ptr += 2;
                            ps.skip_unicode_escape_sequence(4)?;
                        }
                        Some(b'U') => {
                            ps.ptr += 2;
                            ps.skip_unicode_escape_sequence(6)?;
                        }
                        _ => return error!(ErrorKind::Generic, ps.ptr),
                    },
                    b'"' => {
                        break;
                    }
                    b'\n' => {
                        return error!(ErrorKind::Generic, ps.ptr);
                    }
                    _ => ps.ptr += 1,
                }
            }

            ps.expect_byte(b'"')?;
            let slice = ps.get_slice(start, ps.ptr - 1);
            Ok(ast::InlineExpression::StringLiteral { value: slice })
        }
        Some(b) if b.is_ascii_digit() => {
            let num = get_number_literal(ps)?;
            Ok(ast::InlineExpression::NumberLiteral { value: num })
        }
        Some(b'-') => {
            ps.ptr += 1; // -
            if ps.is_identifier_start() {
                let id = get_identifier(ps)?;
                let attribute = get_attribute_accessor(ps)?;
                let arguments = get_call_arguments(ps)?;
                Ok(ast::InlineExpression::TermReference {
                    id,
                    attribute,
                    arguments,
                })
            } else {
                ps.ptr -= 1;
                let num = get_number_literal(ps)?;
                Ok(ast::InlineExpression::NumberLiteral { value: num })
            }
        }
        Some(b'$') => {
            ps.ptr += 1; // -
            let id = get_identifier(ps)?;
            Ok(ast::InlineExpression::VariableReference { id })
        }
        Some(b) if b.is_ascii_alphabetic() => {
            let id = get_identifier(ps)?;
            let arguments = get_call_arguments(ps)?;
            if arguments.is_some() {
                if !id
                    .name
                    .bytes()
                    .all(|c| c.is_ascii_uppercase() || c.is_ascii_digit() || c == b'_' || c == b'-')
                {
                    return error!(ErrorKind::ForbiddenCallee, ps.ptr);
                }

                Ok(ast::InlineExpression::FunctionReference { id, arguments })
            } else {
                let attribute = get_attribute_accessor(ps)?;
                Ok(ast::InlineExpression::MessageReference { id, attribute })
            }
        }
        Some(b'{') => {
            let exp = get_placeable(ps)?;
            Ok(ast::InlineExpression::Placeable {
                expression: Box::new(exp),
            })
        }
        _ => error!(ErrorKind::ExpectedInlineExpression, ps.ptr),
    }
}

fn get_call_arguments<'p>(ps: &mut ParserStream<'p>) -> Result<Option<ast::CallArguments<'p>>> {
    ps.skip_blank();
    if !ps.take_byte_if(b'(') {
        return Ok(None);
    }

    let mut positional = vec![];
    let mut named = vec![];
    let mut argument_names = vec![];

    ps.skip_blank();

    while ps.ptr < ps.length {
        if ps.is_current_byte(b')') {
            break;
        }

        let expr = get_inline_expression(ps)?;

        match expr {
            ast::InlineExpression::MessageReference {
                ref id,
                attribute: None,
            } => {
                ps.skip_blank();
                if ps.is_current_byte(b':') {
                    if argument_names.contains(&id.name.to_owned()) {
                        return error!(
                            ErrorKind::DuplicatedNamedArgument(id.name.to_owned()),
                            ps.ptr
                        );
                    }
                    ps.ptr += 1;
                    ps.skip_blank();
                    let val = get_inline_expression(ps)?;
                    argument_names.push(id.name.to_owned());
                    named.push(ast::NamedArgument {
                        name: ast::Identifier { name: id.name },
                        value: val,
                    });
                } else {
                    if !argument_names.is_empty() {
                        return error!(ErrorKind::PositionalArgumentFollowsNamed, ps.ptr);
                    }
                    positional.push(expr);
                }
            }
            _ => {
                if !argument_names.is_empty() {
                    return error!(ErrorKind::PositionalArgumentFollowsNamed, ps.ptr);
                }
                positional.push(expr);
            }
        }

        ps.skip_blank();
        ps.take_byte_if(b',');
        ps.skip_blank();
    }

    ps.expect_byte(b')')?;

    Ok(Some(ast::CallArguments { positional, named }))
}

fn get_number_literal<'p>(ps: &mut ParserStream<'p>) -> Result<&'p str> {
    let start = ps.ptr;
    ps.take_byte_if(b'-');
    ps.skip_digits()?;
    if ps.take_byte_if(b'.') {
        ps.skip_digits()?;
    }

    Ok(ps.get_slice(start, ps.ptr))
}
