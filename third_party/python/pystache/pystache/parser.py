# coding: utf-8

"""
Exposes a parse() function to parse template strings.

"""

import re

from pystache import defaults
from pystache.parsed import ParsedTemplate


END_OF_LINE_CHARACTERS = [u'\r', u'\n']
NON_BLANK_RE = re.compile(ur'^(.)', re.M)


# TODO: add some unit tests for this.
# TODO: add a test case that checks for spurious spaces.
# TODO: add test cases for delimiters.
def parse(template, delimiters=None):
    """
    Parse a unicode template string and return a ParsedTemplate instance.

    Arguments:

      template: a unicode template string.

      delimiters: a 2-tuple of delimiters.  Defaults to the package default.

    Examples:

    >>> parsed = parse(u"Hey {{#who}}{{name}}!{{/who}}")
    >>> print str(parsed).replace('u', '')  # This is a hack to get the test to pass both in Python 2 and 3.
    ['Hey ', _SectionNode(key='who', index_begin=12, index_end=21, parsed=[_EscapeNode(key='name'), '!'])]

    """
    if type(template) is not unicode:
        raise Exception("Template is not unicode: %s" % type(template))
    parser = _Parser(delimiters)
    return parser.parse(template)


def _compile_template_re(delimiters):
    """
    Return a regular expresssion object (re.RegexObject) instance.

    """
    # The possible tag type characters following the opening tag,
    # excluding "=" and "{".
    tag_types = "!>&/#^"

    # TODO: are we following this in the spec?
    #
    #   The tag's content MUST be a non-whitespace character sequence
    #   NOT containing the current closing delimiter.
    #
    tag = r"""
        (?P<whitespace>[\ \t]*)
        %(otag)s \s*
        (?:
          (?P<change>=) \s* (?P<delims>.+?)   \s* = |
          (?P<raw>{)    \s* (?P<raw_name>.+?) \s* } |
          (?P<tag>[%(tag_types)s]?)  \s* (?P<tag_key>[\s\S]+?)
        )
        \s* %(ctag)s
    """ % {'tag_types': tag_types, 'otag': re.escape(delimiters[0]), 'ctag': re.escape(delimiters[1])}

    return re.compile(tag, re.VERBOSE)


class ParsingError(Exception):

    pass


## Node types

def _format(obj, exclude=None):
    if exclude is None:
        exclude = []
    exclude.append('key')
    attrs = obj.__dict__
    names = list(set(attrs.keys()) - set(exclude))
    names.sort()
    names.insert(0, 'key')
    args = ["%s=%s" % (name, repr(attrs[name])) for name in names]
    return "%s(%s)" % (obj.__class__.__name__, ", ".join(args))


class _CommentNode(object):

    def __repr__(self):
        return _format(self)

    def render(self, engine, context):
        return u''


class _ChangeNode(object):

    def __init__(self, delimiters):
        self.delimiters = delimiters

    def __repr__(self):
        return _format(self)

    def render(self, engine, context):
        return u''


class _EscapeNode(object):

    def __init__(self, key):
        self.key = key

    def __repr__(self):
        return _format(self)

    def render(self, engine, context):
        s = engine.fetch_string(context, self.key)
        return engine.escape(s)


class _LiteralNode(object):

    def __init__(self, key):
        self.key = key

    def __repr__(self):
        return _format(self)

    def render(self, engine, context):
        s = engine.fetch_string(context, self.key)
        return engine.literal(s)


class _PartialNode(object):

    def __init__(self, key, indent):
        self.key = key
        self.indent = indent

    def __repr__(self):
        return _format(self)

    def render(self, engine, context):
        template = engine.resolve_partial(self.key)
        # Indent before rendering.
        template = re.sub(NON_BLANK_RE, self.indent + ur'\1', template)

        return engine.render(template, context)


class _InvertedNode(object):

    def __init__(self, key, parsed_section):
        self.key = key
        self.parsed_section = parsed_section

    def __repr__(self):
        return _format(self)

    def render(self, engine, context):
        # TODO: is there a bug because we are not using the same
        #   logic as in fetch_string()?
        data = engine.resolve_context(context, self.key)
        # Note that lambdas are considered truthy for inverted sections
        # per the spec.
        if data:
            return u''
        return self.parsed_section.render(engine, context)


class _SectionNode(object):

    # TODO: the template_ and parsed_template_ arguments don't both seem
    # to be necessary.  Can we remove one of them?  For example, if
    # callable(data) is True, then the initial parsed_template isn't used.
    def __init__(self, key, parsed, delimiters, template, index_begin, index_end):
        self.delimiters = delimiters
        self.key = key
        self.parsed = parsed
        self.template = template
        self.index_begin = index_begin
        self.index_end = index_end

    def __repr__(self):
        return _format(self, exclude=['delimiters', 'template'])

    def render(self, engine, context):
        values = engine.fetch_section_data(context, self.key)

        parts = []
        for val in values:
            if callable(val):
                # Lambdas special case section rendering and bypass pushing
                # the data value onto the context stack.  From the spec--
                #
                #   When used as the data value for a Section tag, the
                #   lambda MUST be treatable as an arity 1 function, and
                #   invoked as such (passing a String containing the
                #   unprocessed section contents).  The returned value
                #   MUST be rendered against the current delimiters, then
                #   interpolated in place of the section.
                #
                #  Also see--
                #
                #   https://github.com/defunkt/pystache/issues/113
                #
                # TODO: should we check the arity?
                val = val(self.template[self.index_begin:self.index_end])
                val = engine._render_value(val, context, delimiters=self.delimiters)
                parts.append(val)
                continue

            context.push(val)
            parts.append(self.parsed.render(engine, context))
            context.pop()

        return unicode(''.join(parts))


class _Parser(object):

    _delimiters = None
    _template_re = None

    def __init__(self, delimiters=None):
        if delimiters is None:
            delimiters = defaults.DELIMITERS

        self._delimiters = delimiters

    def _compile_delimiters(self):
        self._template_re = _compile_template_re(self._delimiters)

    def _change_delimiters(self, delimiters):
        self._delimiters = delimiters
        self._compile_delimiters()

    def parse(self, template):
        """
        Parse a template string starting at some index.

        This method uses the current tag delimiter.

        Arguments:

          template: a unicode string that is the template to parse.

          index: the index at which to start parsing.

        Returns:

          a ParsedTemplate instance.

        """
        self._compile_delimiters()

        start_index = 0
        content_end_index, parsed_section, section_key = None, None, None
        parsed_template = ParsedTemplate()

        states = []

        while True:
            match = self._template_re.search(template, start_index)

            if match is None:
                break

            match_index = match.start()
            end_index = match.end()

            matches = match.groupdict()

            # Normalize the matches dictionary.
            if matches['change'] is not None:
                matches.update(tag='=', tag_key=matches['delims'])
            elif matches['raw'] is not None:
                matches.update(tag='&', tag_key=matches['raw_name'])

            tag_type = matches['tag']
            tag_key = matches['tag_key']
            leading_whitespace = matches['whitespace']

            # Standalone (non-interpolation) tags consume the entire line,
            # both leading whitespace and trailing newline.
            did_tag_begin_line = match_index == 0 or template[match_index - 1] in END_OF_LINE_CHARACTERS
            did_tag_end_line = end_index == len(template) or template[end_index] in END_OF_LINE_CHARACTERS
            is_tag_interpolating = tag_type in ['', '&']

            if did_tag_begin_line and did_tag_end_line and not is_tag_interpolating:
                if end_index < len(template):
                    end_index += template[end_index] == '\r' and 1 or 0
                if end_index < len(template):
                    end_index += template[end_index] == '\n' and 1 or 0
            elif leading_whitespace:
                match_index += len(leading_whitespace)
                leading_whitespace = ''

            # Avoid adding spurious empty strings to the parse tree.
            if start_index != match_index:
                parsed_template.add(template[start_index:match_index])

            start_index = end_index

            if tag_type in ('#', '^'):
                # Cache current state.
                state = (tag_type, end_index, section_key, parsed_template)
                states.append(state)

                # Initialize new state
                section_key, parsed_template = tag_key, ParsedTemplate()
                continue

            if tag_type == '/':
                if tag_key != section_key:
                    raise ParsingError("Section end tag mismatch: %s != %s" % (tag_key, section_key))

                # Restore previous state with newly found section data.
                parsed_section = parsed_template

                (tag_type, section_start_index, section_key, parsed_template) = states.pop()
                node = self._make_section_node(template, tag_type, tag_key, parsed_section,
                                               section_start_index, match_index)

            else:
                node = self._make_interpolation_node(tag_type, tag_key, leading_whitespace)

            parsed_template.add(node)

        # Avoid adding spurious empty strings to the parse tree.
        if start_index != len(template):
            parsed_template.add(template[start_index:])

        return parsed_template

    def _make_interpolation_node(self, tag_type, tag_key, leading_whitespace):
        """
        Create and return a non-section node for the parse tree.

        """
        # TODO: switch to using a dictionary instead of a bunch of ifs and elifs.
        if tag_type == '!':
            return _CommentNode()

        if tag_type == '=':
            delimiters = tag_key.split()
            self._change_delimiters(delimiters)
            return _ChangeNode(delimiters)

        if tag_type == '':
            return _EscapeNode(tag_key)

        if tag_type == '&':
            return _LiteralNode(tag_key)

        if tag_type == '>':
            return _PartialNode(tag_key, leading_whitespace)

        raise Exception("Invalid symbol for interpolation tag: %s" % repr(tag_type))

    def _make_section_node(self, template, tag_type, tag_key, parsed_section,
                           section_start_index, section_end_index):
        """
        Create and return a section node for the parse tree.

        """
        if tag_type == '#':
            return _SectionNode(tag_key, parsed_section, self._delimiters,
                               template, section_start_index, section_end_index)

        if tag_type == '^':
            return _InvertedNode(tag_key, parsed_section)

        raise Exception("Invalid symbol for section tag: %s" % repr(tag_type))
