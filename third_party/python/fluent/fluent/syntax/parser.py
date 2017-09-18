from __future__ import unicode_literals
import re
from .ftlstream import FTLParserStream
from . import ast
from .errors import ParseError


def with_span(fn):
    def decorated(self, ps, *args):
        if not self.with_spans:
            return fn(self, ps, *args)

        start = ps.get_index()
        node = fn(self, ps, *args)

        # Don't re-add the span if the node already has it.  This may happen
        # when one decorated function calls another decorated function.
        if node.span is not None:
            return node

        # Spans of Messages and Sections should include the attached Comment.
        if isinstance(node, ast.Message) or isinstance(node, ast.Section):
            if node.comment is not None:
                start = node.comment.span.start

        end = ps.get_index()
        node.add_span(start, end)
        return node

    return decorated


class FluentParser(object):
    def __init__(self, with_spans=True):
        self.with_spans = with_spans

    def parse(self, source):
        comment = None

        ps = FTLParserStream(source)
        ps.skip_blank_lines()

        entries = []

        while ps.current():
            entry = self.get_entry_or_junk(ps)

            if isinstance(entry, ast.Comment) and len(entries) == 0:
                comment = entry
            else:
                entries.append(entry)

            ps.skip_blank_lines()

        res = ast.Resource(entries, comment)

        if self.with_spans:
            res.add_span(0, ps.get_index())

        return res

    def parse_entry(self, source):
        ps = FTLParserStream(source)
        ps.skip_blank_lines()
        return self.get_entry_or_junk(ps)

    def get_entry_or_junk(self, ps):
        entry_start_pos = ps.get_index()

        try:
            return self.get_entry(ps)
        except ParseError as err:
            error_index = ps.get_index()
            ps.skip_to_next_entry_start()
            next_entry_start = ps.get_index()

            # Create a Junk instance
            slice = ps.get_slice(entry_start_pos, next_entry_start)
            junk = ast.Junk(slice)
            if self.with_spans:
                junk.add_span(entry_start_pos, next_entry_start)
            annot = ast.Annotation(err.code, err.args, err.message)
            annot.add_span(error_index, error_index)
            junk.add_annotation(annot)
            return junk

    def get_entry(self, ps):
        comment = None

        if ps.current_is('/'):
            comment = self.get_comment(ps)

        if ps.current_is('['):
            return self.get_section(ps, comment)

        if ps.is_id_start():
            return self.get_message(ps, comment)

        if comment:
            return comment

        raise ParseError('E0002')

    @with_span
    def get_comment(self, ps):
        ps.expect_char('/')
        ps.expect_char('/')
        ps.take_char_if(' ')

        content = ''

        def until_eol(x):
            return x != '\n'

        while True:
            ch = ps.take_char(until_eol)
            while ch:
                content += ch
                ch = ps.take_char(until_eol)

            ps.next()

            if ps.current_is('/'):
                content += '\n'
                ps.next()
                ps.expect_char('/')
                ps.take_char_if(' ')
            else:
                break

        return ast.Comment(content)

    @with_span
    def get_section(self, ps, comment):
        ps.expect_char('[')
        ps.expect_char('[')

        ps.skip_inline_ws()

        symb = self.get_symbol(ps)

        ps.skip_inline_ws()

        ps.expect_char(']')
        ps.expect_char(']')

        ps.skip_inline_ws()

        ps.expect_char('\n')

        return ast.Section(symb, comment)

    @with_span
    def get_message(self, ps, comment):
        id = self.get_identifier(ps)

        ps.skip_inline_ws()

        pattern = None
        attrs = None
        tags = None

        if ps.current_is('='):
            ps.next()
            ps.skip_inline_ws()

            pattern = self.get_pattern(ps)

        if ps.is_peek_next_line_attribute_start():
            attrs = self.get_attributes(ps)

        if ps.is_peek_next_line_tag_start():
            if attrs is not None:
                raise ParseError('E0012')
            tags = self.get_tags(ps)

        if pattern is None and attrs is None:
            raise ParseError('E0005', id.name)

        return ast.Message(id, pattern, attrs, tags, comment)

    @with_span
    def get_attribute(self, ps):
        ps.expect_char('.')

        key = self.get_identifier(ps)

        ps.skip_inline_ws()
        ps.expect_char('=')
        ps.skip_inline_ws()

        value = self.get_pattern(ps)

        if value is None:
            raise ParseError('E0006', 'value')

        return ast.Attribute(key, value)

    def get_attributes(self, ps):
        attrs = []

        while True:
            ps.expect_char('\n')
            ps.skip_inline_ws()

            attr = self.get_attribute(ps)
            attrs.append(attr)

            if not ps.is_peek_next_line_attribute_start():
                break
        return attrs

    @with_span
    def get_tag(self, ps):
        ps.expect_char('#')
        symb = self.get_symbol(ps)
        return ast.Tag(symb)

    def get_tags(self, ps):
        tags = []

        while True:
            ps.expect_char('\n')
            ps.skip_inline_ws()

            tag = self.get_tag(ps)
            tags.append(tag)

            if not ps.is_peek_next_line_tag_start():
                break
        return tags

    @with_span
    def get_identifier(self, ps):
        name = ''

        name += ps.take_id_start()

        ch = ps.take_id_char()
        while ch:
            name += ch
            ch = ps.take_id_char()

        return ast.Identifier(name)

    def get_variant_key(self, ps):
        ch = ps.current()

        if ch is None:
            raise ParseError('E0013')

        if ps.is_number_start():
            return self.get_number(ps)

        return self.get_symbol(ps)

    @with_span
    def get_variant(self, ps, has_default):
        default_index = False

        if ps.current_is('*'):
            if has_default:
                raise ParseError('E0015')
            ps.next()
            default_index = True

        ps.expect_char('[')

        key = self.get_variant_key(ps)

        ps.expect_char(']')

        ps.skip_inline_ws()

        value = self.get_pattern(ps)

        if value is None:
            raise ParseError('E0006', 'value')

        return ast.Variant(key, value, default_index)

    def get_variants(self, ps):
        variants = []
        has_default = False

        while True:
            ps.expect_char('\n')
            ps.skip_inline_ws()

            variant = self.get_variant(ps, has_default)

            if variant.default:
                has_default = True

            variants.append(variant)

            if not ps.is_peek_next_line_variant_start():
                break

        if not has_default:
            raise ParseError('E0010')

        return variants

    @with_span
    def get_symbol(self, ps):
        name = ''

        name += ps.take_id_start()

        while True:
            ch = ps.take_symb_char()
            if ch:
                name += ch
            else:
                break

        return ast.Symbol(name.rstrip())

    def get_digits(self, ps):
        num = ''

        ch = ps.take_digit()
        while ch:
            num += ch
            ch = ps.take_digit()

        if len(num) == 0:
            raise ParseError('E0004', '0-9')

        return num

    @with_span
    def get_number(self, ps):
        num = ''

        if ps.current_is('-'):
            num += '-'
            ps.next()

        num += self.get_digits(ps)

        if ps.current_is('.'):
            num += '.'
            ps.next()
            num += self.get_digits(ps)

        return ast.NumberExpression(num)

    @with_span
    def get_pattern(self, ps):
        elements = []
        ps.skip_inline_ws()

        # Special-case: trim leading whitespace and newlines.
        if ps.is_peek_next_line_pattern():
            ps.skip_blank_lines()
            ps.skip_inline_ws()

        while ps.current():
            ch = ps.current()

            # The end condition for get_pattern's while loop is a newline
            # which is not followed by a valid pattern continuation.
            if ch == '\n' and not ps.is_peek_next_line_pattern():
                break

            if ch == '{':
                element = self.get_placeable(ps)
            else:
                element = self.get_text_element(ps)

            elements.append(element)

        return ast.Pattern(elements)

    @with_span
    def get_text_element(self, ps):
        buf = ''

        while ps.current():
            ch = ps.current()

            if ch == '{':
                return ast.TextElement(buf)

            if ch == '\n':
                if not ps.is_peek_next_line_pattern():
                    return ast.TextElement(buf)

                ps.next()
                ps.skip_inline_ws()

                # Add the new line to the buffer
                buf += ch
                continue

            if ch == '\\':
                ch2 = ps.next()

                if ch2 == '{' or ch2 == '"':
                    buf += ch2
                else:
                    buf += ch + ch2

            else:
                buf += ch

            ps.next()

        return ast.TextElement(buf)

    @with_span
    def get_placeable(self, ps):
        ps.expect_char('{')
        expression = self.get_expression(ps)
        ps.expect_char('}')
        return ast.Placeable(expression)

    @with_span
    def get_expression(self, ps):

        if ps.is_peek_next_line_variant_start():
            variants = self.get_variants(ps)

            ps.expect_char('\n')
            ps.expect_char(' ')
            ps.skip_inline_ws()

            return ast.SelectExpression(None, variants)

        ps.skip_inline_ws()

        selector = self.get_selector_expression(ps)

        ps.skip_inline_ws()

        if ps.current_is('-'):
            ps.peek()
            if not ps.current_peek_is('>'):
                ps.reset_peek()
            else:
                ps.next()
                ps.next()

                ps.skip_inline_ws()

                variants = self.get_variants(ps)

                if len(variants) == 0:
                    raise ParseError('E0011')

                ps.expect_char('\n')
                ps.expect_char(' ')
                ps.skip_inline_ws()

                return ast.SelectExpression(selector, variants)

        return selector

    @with_span
    def get_selector_expression(self, ps):
        literal = self.get_literal(ps)

        if not isinstance(literal, ast.MessageReference):
            return literal

        ch = ps.current()

        if (ch == '.'):
            ps.next()
            attr = self.get_identifier(ps)
            return ast.AttributeExpression(literal.id, attr)

        if (ch == '['):
            ps.next()
            key = self.get_variant_key(ps)
            ps.expect_char(']')
            return ast.VariantExpression(literal.id, key)

        if (ch == '('):
            ps.next()

            args = self.get_call_args(ps)

            ps.expect_char(')')

            if not re.match('^[A-Z_-]+$', literal.id.name):
                raise ParseError('E0008')

            return ast.CallExpression(
                ast.Function(literal.id.name),
                args
            )

        return literal

    @with_span
    def get_call_arg(self, ps):
        exp = self.get_selector_expression(ps)

        ps.skip_inline_ws()

        if not ps.current_is(':'):
            return exp

        if not isinstance(exp, ast.MessageReference):
            raise ParseError('E0009')

        ps.next()
        ps.skip_inline_ws()

        val = self.get_arg_val(ps)

        return ast.NamedArgument(exp.id, val)

    def get_call_args(self, ps):
        args = []

        ps.skip_inline_ws()

        while True:
            if ps.current_is(')'):
                break

            arg = self.get_call_arg(ps)
            args.append(arg)

            ps.skip_inline_ws()

            if ps.current_is(','):
                ps.next()
                ps.skip_inline_ws()
                continue
            else:
                break

        return args

    def get_arg_val(self, ps):
        if ps.is_number_start():
            return self.get_number(ps)
        elif ps.current_is('"'):
            return self.get_string(ps)
        raise ParseError('E0006', 'value')

    @with_span
    def get_string(self, ps):
        val = ''

        ps.expect_char('"')

        ch = ps.take_char(lambda x: x != '"')
        while ch:
            val += ch
            ch = ps.take_char(lambda x: x != '"')

        ps.next()

        return ast.StringExpression(val)

    @with_span
    def get_literal(self, ps):
        ch = ps.current()

        if ch is None:
            raise ParseError('E0014')

        if ps.is_number_start():
            return self.get_number(ps)
        elif ch == '"':
            return self.get_string(ps)
        elif ch == '$':
            ps.next()
            name = self.get_identifier(ps)
            return ast.ExternalArgument(name)

        name = self.get_identifier(ps)
        return ast.MessageReference(name)
