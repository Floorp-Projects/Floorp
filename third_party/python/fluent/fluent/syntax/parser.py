from __future__ import unicode_literals
import re
from . import ast
from .stream import EOF, EOL, FluentParserStream
from .errors import ParseError


def with_span(fn):
    def decorated(self, ps, *args, **kwargs):
        if not self.with_spans:
            return fn(self, ps, *args, **kwargs)

        start = ps.index
        node = fn(self, ps, *args, **kwargs)

        # Don't re-add the span if the node already has it. This may happen
        # when one decorated function calls another decorated function.
        if node.span is not None:
            return node

        end = ps.index
        node.add_span(start, end)
        return node

    return decorated


class FluentParser(object):
    def __init__(self, with_spans=True):
        self.with_spans = with_spans

    def parse(self, source):
        ps = FluentParserStream(source)
        ps.skip_blank_block()

        entries = []
        last_comment = None

        while ps.current_char:
            entry = self.get_entry_or_junk(ps)
            blank_lines = ps.skip_blank_block()

            # Regular Comments require special logic. Comments may be attached
            # to Messages or Terms if they are followed immediately by them.
            # However they should parse as standalone when they're followed by
            # Junk. Consequently, we only attach Comments once we know that the
            # Message or the Term parsed successfully.
            if isinstance(entry, ast.Comment) and len(blank_lines) == 0 \
                    and ps.current_char:
                # Stash the comment and decide what to do with it
                # in the next pass.
                last_comment = entry
                continue

            if last_comment is not None:
                if isinstance(entry, (ast.Message, ast.Term)):
                    entry.comment = last_comment
                    if self.with_spans:
                        entry.span.start = entry.comment.span.start
                else:
                    entries.append(last_comment)
                # In either case, the stashed comment has been dealt with;
                # clear it.
                last_comment = None

            entries.append(entry)

        res = ast.Resource(entries)

        if self.with_spans:
            res.add_span(0, ps.index)

        return res

    def parse_entry(self, source):
        """Parse the first Message or Term in source.

        Skip all encountered comments and start parsing at the first Mesage
        or Term start. Return Junk if the parsing is not successful.

        Preceding comments are ignored unless they contain syntax errors
        themselves, in which case Junk for the invalid comment is returned.
        """
        ps = FluentParserStream(source)
        ps.skip_blank_block()

        while ps.current_char == '#':
            skipped = self.get_entry_or_junk(ps)
            if isinstance(skipped, ast.Junk):
                # Don't skip Junk comments.
                return skipped
            ps.skip_blank_block()

        return self.get_entry_or_junk(ps)

    def get_entry_or_junk(self, ps):
        entry_start_pos = ps.index

        try:
            entry = self.get_entry(ps)
            ps.expect_line_end()
            return entry
        except ParseError as err:
            error_index = ps.index
            ps.skip_to_next_entry_start(entry_start_pos)
            next_entry_start = ps.index
            if next_entry_start < error_index:
                # The position of the error must be inside of the Junk's span.
                error_index = next_entry_start

            # Create a Junk instance
            slice = ps.string[entry_start_pos:next_entry_start]
            junk = ast.Junk(slice)
            if self.with_spans:
                junk.add_span(entry_start_pos, next_entry_start)
            annot = ast.Annotation(err.code, err.args, err.message)
            annot.add_span(error_index, error_index)
            junk.add_annotation(annot)
            return junk

    def get_entry(self, ps):
        if ps.current_char == '#':
            return self.get_comment(ps)

        if ps.current_char == '-':
            return self.get_term(ps)

        if ps.is_identifier_start():
            return self.get_message(ps)

        raise ParseError('E0002')

    @with_span
    def get_comment(self, ps):
        # 0 - comment
        # 1 - group comment
        # 2 - resource comment
        level = -1
        content = ''

        while True:
            i = -1
            while ps.current_char == '#' \
                    and (i < (2 if level == -1 else level)):
                ps.next()
                i += 1

            if level == -1:
                level = i

            if ps.current_char != EOL:
                ps.expect_char(' ')
                ch = ps.take_char(lambda x: x != EOL)
                while ch:
                    content += ch
                    ch = ps.take_char(lambda x: x != EOL)

            if ps.is_next_line_comment(level=level):
                content += ps.current_char
                ps.next()
            else:
                break

        if level == 0:
            return ast.Comment(content)
        elif level == 1:
            return ast.GroupComment(content)
        elif level == 2:
            return ast.ResourceComment(content)

    @with_span
    def get_message(self, ps):
        id = self.get_identifier(ps)
        ps.skip_blank_inline()
        ps.expect_char('=')

        value = self.maybe_get_pattern(ps)
        attrs = self.get_attributes(ps)

        if value is None and len(attrs) == 0:
            raise ParseError('E0005', id.name)

        return ast.Message(id, value, attrs)

    @with_span
    def get_term(self, ps):
        ps.expect_char('-')
        id = self.get_identifier(ps)

        ps.skip_blank_inline()
        ps.expect_char('=')

        value = self.maybe_get_pattern(ps)
        if value is None:
            raise ParseError('E0006', id.name)

        attrs = self.get_attributes(ps)
        return ast.Term(id, value, attrs)

    @with_span
    def get_attribute(self, ps):
        ps.expect_char('.')

        key = self.get_identifier(ps)

        ps.skip_blank_inline()
        ps.expect_char('=')

        value = self.maybe_get_pattern(ps)
        if value is None:
            raise ParseError('E0012')

        return ast.Attribute(key, value)

    def get_attributes(self, ps):
        attrs = []
        ps.peek_blank()

        while ps.is_attribute_start():
            ps.skip_to_peek()
            attr = self.get_attribute(ps)
            attrs.append(attr)
            ps.peek_blank()

        return attrs

    @with_span
    def get_identifier(self, ps):
        name = ps.take_id_start()
        ch = ps.take_id_char()
        while ch:
            name += ch
            ch = ps.take_id_char()

        return ast.Identifier(name)

    def get_variant_key(self, ps):
        ch = ps.current_char

        if ch is EOF:
            raise ParseError('E0013')

        cc = ord(ch)
        if ((cc >= 48 and cc <= 57) or cc == 45):  # 0-9, -
            return self.get_number(ps)

        return self.get_identifier(ps)

    @with_span
    def get_variant(self, ps, has_default):
        default_index = False

        if ps.current_char == '*':
            if has_default:
                raise ParseError('E0015')
            ps.next()
            default_index = True

        ps.expect_char('[')
        ps.skip_blank()

        key = self.get_variant_key(ps)

        ps.skip_blank()
        ps.expect_char(']')

        value = self.maybe_get_pattern(ps)
        if value is None:
            raise ParseError('E0012')

        return ast.Variant(key, value, default_index)

    def get_variants(self, ps):
        variants = []
        has_default = False

        ps.skip_blank()
        while ps.is_variant_start():
            variant = self.get_variant(ps, has_default)

            if variant.default:
                has_default = True

            variants.append(variant)
            ps.expect_line_end()
            ps.skip_blank()

        if len(variants) == 0:
            raise ParseError('E0011')

        if not has_default:
            raise ParseError('E0010')

        return variants

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

        if ps.current_char == '-':
            num += '-'
            ps.next()

        num += self.get_digits(ps)

        if ps.current_char == '.':
            num += '.'
            ps.next()
            num += self.get_digits(ps)

        return ast.NumberLiteral(num)

    def maybe_get_pattern(self, ps):
        '''Parse an inline or a block Pattern, or None

        maybe_get_pattern distinguishes between patterns which start on the
        same line as the indentifier (aka inline singleline patterns and inline
        multiline patterns), and patterns which start on a new line (aka block
        patterns). The distinction is important for the dedentation logic: the
        indent of the first line of a block pattern must be taken into account
        when calculating the maximum common indent.
        '''
        ps.peek_blank_inline()
        if ps.is_value_start():
            ps.skip_to_peek()
            return self.get_pattern(ps, is_block=False)

        ps.peek_blank_block()
        if ps.is_value_continuation():
            ps.skip_to_peek()
            return self.get_pattern(ps, is_block=True)

        return None

    @with_span
    def get_pattern(self, ps, is_block):
        elements = []
        if is_block:
            # A block pattern is a pattern which starts on a new line. Measure
            # the indent of this first line for the dedentation logic.
            blank_start = ps.index
            first_indent = ps.skip_blank_inline()
            elements.append(self.Indent(first_indent, blank_start, ps.index))
            common_indent_length = len(first_indent)
        else:
            common_indent_length = float('infinity')

        while ps.current_char:
            if ps.current_char == EOL:
                blank_start = ps.index
                blank_lines = ps.peek_blank_block()
                if ps.is_value_continuation():
                    ps.skip_to_peek()
                    indent = ps.skip_blank_inline()
                    common_indent_length = min(common_indent_length, len(indent))
                    elements.append(self.Indent(blank_lines + indent, blank_start, ps.index))
                    continue

                # The end condition for get_pattern's while loop is a newline
                # which is not followed by a valid pattern continuation.
                ps.reset_peek()
                break

            if ps.current_char == '}':
                raise ParseError('E0027')

            if ps.current_char == '{':
                element = self.get_placeable(ps)
            else:
                element = self.get_text_element(ps)

            elements.append(element)

        dedented = self.dedent(elements, common_indent_length)
        return ast.Pattern(dedented)

    class Indent(ast.SyntaxNode):
        def __init__(self, value, start, end):
            super(FluentParser.Indent, self).__init__()
            self.value = value
            self.add_span(start, end)

    def dedent(self, elements, common_indent):
        '''Dedent a list of elements by removing the maximum common indent from
        the beginning of text lines. The common indent is calculated in
        get_pattern.
        '''
        trimmed = []

        for element in elements:
            if isinstance(element, ast.Placeable):
                trimmed.append(element)
                continue

            if isinstance(element, self.Indent):
                # Strip the common indent.
                element.value = element.value[:len(element.value) - common_indent]
                if len(element.value) == 0:
                    continue

            prev = trimmed[-1] if len(trimmed) > 0 else None
            if isinstance(prev, ast.TextElement):
                # Join adjacent TextElements by replacing them with their sum.
                sum = ast.TextElement(prev.value + element.value)
                if self.with_spans:
                    sum.add_span(prev.span.start, element.span.end)
                trimmed[-1] = sum
                continue

            if isinstance(element, self.Indent):
                # If the indent hasn't been merged into a preceding
                # TextElements, convert it into a new TextElement.
                text_element = ast.TextElement(element.value)
                if self.with_spans:
                    text_element.add_span(element.span.start, element.span.end)
                element = text_element

            trimmed.append(element)

        # Trim trailing whitespace from the Pattern.
        last_element = trimmed[-1] if len(trimmed) > 0 else None
        if isinstance(last_element, ast.TextElement):
            last_element.value = last_element.value.rstrip(' \t\n\r')
            if last_element.value == "":
                trimmed.pop()

        return trimmed

    @with_span
    def get_text_element(self, ps):
        buf = ''

        while ps.current_char:
            ch = ps.current_char

            if ch == '{' or ch == '}':
                return ast.TextElement(buf)

            if ch == EOL:
                return ast.TextElement(buf)

            buf += ch
            ps.next()

        return ast.TextElement(buf)

    def get_escape_sequence(self, ps):
        next = ps.current_char

        if next == '\\' or next == '"':
            ps.next()
            return '\\{}'.format(next)

        if next == 'u':
            return self.get_unicode_escape_sequence(ps, next, 4)

        if next == 'U':
            return self.get_unicode_escape_sequence(ps, next, 6)

        raise ParseError('E0025', next)

    def get_unicode_escape_sequence(self, ps, u, digits):
        ps.expect_char(u)
        sequence = ''
        for _ in range(digits):
            ch = ps.take_hex_digit()
            if not ch:
                raise ParseError('E0026', '\\{}{}{}'.format(u, sequence, ps.current_char))
            sequence += ch

        return '\\{}{}'.format(u, sequence)

    @with_span
    def get_placeable(self, ps):
        ps.expect_char('{')
        ps.skip_blank()
        expression = self.get_expression(ps)
        ps.expect_char('}')
        return ast.Placeable(expression)

    @with_span
    def get_expression(self, ps):
        selector = self.get_inline_expression(ps)

        ps.skip_blank()

        if ps.current_char == '-':
            if ps.peek() != '>':
                ps.reset_peek()
                return selector

            if isinstance(selector, ast.MessageReference):
                if selector.attribute is None:
                    raise ParseError('E0016')
                else:
                    raise ParseError('E0018')

            if (
                isinstance(selector, ast.TermReference)
                and selector.attribute is None
            ):
                raise ParseError('E0017')

            ps.next()
            ps.next()

            ps.skip_blank_inline()
            ps.expect_line_end()

            variants = self.get_variants(ps)
            return ast.SelectExpression(selector, variants)

        if (
            isinstance(selector, ast.TermReference)
            and selector.attribute is not None
        ):
            raise ParseError('E0019')

        return selector

    @with_span
    def get_inline_expression(self, ps):
        if ps.current_char == '{':
            return self.get_placeable(ps)

        if ps.is_number_start():
            return self.get_number(ps)

        if ps.current_char == '"':
            return self.get_string(ps)

        if ps.current_char == '$':
            ps.next()
            id = self.get_identifier(ps)
            return ast.VariableReference(id)

        if ps.current_char == '-':
            ps.next()
            id = self.get_identifier(ps)
            attribute = None
            if ps.current_char == '.':
                ps.next()
                attribute = self.get_identifier(ps)
            arguments = None
            if ps.current_char == '(':
                arguments = self.get_call_arguments(ps)
            return ast.TermReference(id, attribute, arguments)

        if ps.is_identifier_start():
            id = self.get_identifier(ps)

            if ps.current_char == '(':
                # It's a Function. Ensure it's all upper-case.
                if not re.match('^[A-Z][A-Z0-9_-]*$', id.name):
                    raise ParseError('E0008')
                args = self.get_call_arguments(ps)
                return ast.FunctionReference(id, args)

            attribute = None
            if ps.current_char == '.':
                ps.next()
                attribute = self.get_identifier(ps)

            return ast.MessageReference(id, attribute)

        raise ParseError('E0028')

    @with_span
    def get_call_argument(self, ps):
        exp = self.get_inline_expression(ps)

        ps.skip_blank()

        if ps.current_char != ':':
            return exp

        if isinstance(exp, ast.MessageReference) and exp.attribute is None:
            ps.next()
            ps.skip_blank()

            value = self.get_literal(ps)
            return ast.NamedArgument(exp.id, value)

        raise ParseError('E0009')

    @with_span
    def get_call_arguments(self, ps):
        positional = []
        named = []
        argument_names = set()

        ps.expect_char('(')
        ps.skip_blank()

        while True:
            if ps.current_char == ')':
                break

            arg = self.get_call_argument(ps)
            if isinstance(arg, ast.NamedArgument):
                if arg.name.name in argument_names:
                    raise ParseError('E0022')
                named.append(arg)
                argument_names.add(arg.name.name)
            elif len(argument_names) > 0:
                raise ParseError('E0021')
            else:
                positional.append(arg)

            ps.skip_blank()

            if ps.current_char == ',':
                ps.next()
                ps.skip_blank()
                continue

            break

        ps.expect_char(')')
        return ast.CallArguments(positional, named)

    @with_span
    def get_string(self, ps):
        value = ''

        ps.expect_char('"')

        while True:
            ch = ps.take_char(lambda x: x != '"' and x != EOL)
            if not ch:
                break
            if ch == '\\':
                value += self.get_escape_sequence(ps)
            else:
                value += ch

        if ps.current_char == EOL:
            raise ParseError('E0020')

        ps.expect_char('"')

        return ast.StringLiteral(value)

    @with_span
    def get_literal(self, ps):
        if ps.is_number_start():
            return self.get_number(ps)
        if ps.current_char == '"':
            return self.get_string(ps)
        raise ParseError('E0014')
