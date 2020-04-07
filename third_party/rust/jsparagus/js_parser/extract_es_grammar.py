"""extract_es_grammar.py - Extract the grammar from the ECMAScript spec

To run this script, you first need to get the source of the version of
the ECMAScript spec you're interested in.

    cd ../..
    mkdir tc39
    cd tc39
    git clone git@github.com:tc39/ecma262.git

Then:

    make js_parser/es.esgrammar

You can also use this script on a random HTTPS URL, like:

    python extract_esgrammar.py https://raw.githubusercontent.com/tc39/proposal-class-fields/master/spec.html

"""

import argparse
import urllib
import html5lib  # type: ignore
import re
from textwrap import dedent


HTML = "{http://www.w3.org/1999/xhtml}"
INS_TAG = HTML + "ins"
DEL_TAG = HTML + "del"

INS = '+'
DEL = '-'
KEEP = ' '


def pre_with_code_filter_factory(e):
    """Checks if the <pre> is used in the following pattern:

    ```
    <pre><code>
    </code></pre>
    ```

    If so, return a filter that formats the content, removing extra spaces.
    line-wrap, and backquote added by <code>.

    """
    if e.text and e.text.strip() != '':
        return False

    if len(e) != 1:
        return False

    if e[0].tag != '{http://www.w3.org/1999/xhtml}code':
        return False

    if e[0].tail and e[0].tail.strip() != '':
        return False

    def children_filter(texts):
        while len(texts) > 0 and texts[0].strip() == '':
            texts.pop(0)
        if len(texts) > 0 and texts[0].strip() == '`':
            texts.pop(0)
        while len(texts) > 0 and texts[0].strip() == '':
            texts.pop(0)

        while len(texts) > 0 and texts[-1].strip() == '':
            texts.pop()
        if len(texts) > 0 and texts[-1].strip() == '`':
            texts.pop()
        while len(texts) > 0 and texts[-1].strip() == '':
            texts.pop()

        is_first = True
        for text in texts:
            for line in text.split('\n'):
                line = line.strip()
                if line == '':
                    continue

                if not is_first:
                    yield '\n'
                is_first = False

                yield line

    return children_filter


# Rules for extracting text, used by extracting Early Errors.
EXTRA_RULES_FOR_EE = {
    'b': {},
    'br': {},
    'code': {
        'prefix': '`',
        'postfix': '`',
    },
    'emu-alg': {},
    'emu-grammar': {},
    'emu-note': {
        'prefix': ['NOTE', '\n'],
        'strip': True,
    },
    'emu-xref': {
        'prefix': lambda e: e.attrib.get('href'),
    },
    'ins': {
        'ignore_highlighted': True,
    },
    'p': {
        'strip': True,
    },
    'pre': {
        'prefix': ['\n', '\n', '```', '\n'],
        'postfix': ['\n', '```', '\n', '\n'],
        'strip': True,
        'children_filter_factroy': pre_with_code_filter_factory,
    },
    'sub': {
        'prefix': '_',
    },
    'sup': {
        'prefix': '^',
    },
}


def apply_prefix_postfix_rule(e, rule, name):
    """If rule is provided, apply prefix/postfix rule to the element `e`.
    """
    if not rule:
        return

    fix = rule.get(name)
    if callable(fix):
        yield fix(e)
    elif isinstance(fix, list):
        for item in fix:
            yield item
    elif fix:
        yield fix


def apply_strip_rule(text, rule):
    """If rule is provided, apply strip rule to the text.
    """
    if not text:
        return

    if not rule:
        yield text
        return

    strip = rule.get('strip')
    if strip:
        yield text.strip()
    else:
        yield text


def fragment_child_chunks(e, extra_rules={}):
    """Partly interpret the content of `e`, yielding `text`,
    applying extra_rules.

    Concatenating the yielded `text` values gives the full text of `e`.
    """
    rule = extra_rules[e.tag.replace(HTML, '')]

    children_filter = None
    factroy = rule.get('children_filter_factroy')
    if factroy:
        children_filter = factroy(e)

    yield from apply_prefix_postfix_rule(e, rule, 'prefix')
    yield from apply_strip_rule(e.text, rule)

    for child in e:
        if child.tag.replace(HTML, '') not in extra_rules:
            raise ValueError("unrecognized element: " + child.tag)

        texts = []
        for text in fragment_child_chunks(child, extra_rules):
            if children_filter:
                texts.append(text)
            else:
                yield text

        if children_filter:
            for text in children_filter(texts):
                yield text

    yield from apply_strip_rule(e.tail, rule)
    yield from apply_prefix_postfix_rule(e, rule, 'postfix')


def is_highlighted_ins(e):
    """Returns True if e matches the following pattern:

      <ins>highlighted</ins> text:

    See `fragment_chunks` comment for the details
    """
    if len(e) != 0:
        return False

    if not e.text:
        return False

    if e.text != 'highlighted':
        return False

    if not e.tail:
        return False

    if not e.tail.startswith(' text:'):
        return False

    return True


def is_negligible_ins(e, extra_rules):
    """Returns True if the 'ignore_highlighted' rule is defined for <ins>,
    and it matches to the negligible pattern.

    See `fragment_chunks` comment for the details
    """

    rule = extra_rules.get(e.tag.replace(HTML, ''))
    if not rule:
        return False

    if rule.get('ignore_highlighted'):
        if is_highlighted_ins(e):
            return True

    return False


def fragment_chunks(e, extra_rules={}):
    """Partly interpret the content of `e`, yielding pairs (ty, text).

    If `extra_rules` isn't provided, the content of `e` must be text with 0
    or more <ins>/<del> elements.

    The goal is to turn the tree `e` into a simple series of tagged strings.

    Yields pairs (ty, text) where ty in (INS, DEL, KEEP). Concatenating the
    yielded `text` values gives the full text of `e`.

    `extra_rules` is a dictionary that defines extra elements that is allowed
    as the content of `e`.
    Each item defines a rule for the tag, with the following:
      * prefix
        Put a prefix before the text
        Possible values:
          * string
          * list of string
          * function
            receives `Element` and returns a prefix string
      * postfix
        Put a postfix after the text
        value uses the same format as prefix
      * strip
        True to strip whitespaces before/after element's text
      * children_filter_factroy
        A function that receives `Element`, and returns a filter function or None
        The filter function receives a list of texts for child nodes, and
        returns a list of filtered text
      * ignore_highlighted
        Effective only with <ins>
        Do not treat <ins> as an insertion if it matches the following pattern:

          <ins>highlighted</ins> text:

        This pattern is used in Annex B description.
    """

    rule = extra_rules.get(e.tag.replace(HTML, ''))

    for text in apply_prefix_postfix_rule(e, rule, 'prefix'):
        yield KEEP, text
    for text in apply_strip_rule(e.text, rule):
        yield KEEP, text

    for child in e:
        if child.tag == INS_TAG and not is_negligible_ins(child, extra_rules):
            ty = INS
        elif child.tag == DEL_TAG:
            ty = DEL
        else:
            if child.tag.replace(HTML, '') not in extra_rules:
                raise ValueError("unrecognized element: " + child.tag)

            for text in fragment_child_chunks(child, extra_rules):
                yield KEEP, text
            continue

        if child.text:
            yield ty, child.text
        if len(child) != 0:
            for grandchild in child:
                if grandchild.tag.replace(HTML, '') not in extra_rules:
                    raise ValueError("unsupported nested element {} in {}"
                                     .format(grandchild.tag, child.tag))

                for text in fragment_child_chunks(grandchild, extra_rules):
                    yield ty, text
        if child.tail:
            yield KEEP, child.tail

    for text in apply_strip_rule(e.tail, rule):
        yield KEEP, text
    for text in apply_prefix_postfix_rule(e, rule, 'postfix'):
        yield KEEP, text


def fragment_parts(e, **kwargs):
    """Like fragment_chunks, but with two fixups.

    1.  Break up pairs that include both a newline and any other text.

    2.  Move newlines inside of a preceding INS or DEL element that spans its
        whole line.
    """
    line_has_ins = False
    line_has_del = False
    for chunk_ty, text in fragment_chunks(e, **kwargs):
        for piece in re.split(r'(\n)', text):
            ty = chunk_ty
            if piece != '':
                if piece == '\n':
                    # Possibly move newline inside preceding INS or DEL.
                    if line_has_ins and not line_has_del:
                        ty = INS
                    elif line_has_del and not line_has_ins:
                        ty = DEL
                    else:
                        ty = KEEP
                    line_has_del = False
                    line_has_ins = False
                elif piece.strip() != '':
                    if ty in (INS, KEEP):
                        line_has_ins = True
                    if ty in (DEL, KEEP):
                        line_has_del = True
                yield ty, piece


def generate_fragment_patch(e, **kwargs):
    line_before = ''
    line_after = ''

    def end_line(ty):
        nonlocal line_before, line_after
        if line_before.rstrip() == line_after.rstrip():
            yield " ", line_after
        else:
            if line_before.strip() != '' or ty != INS:
                yield "-", line_before
            if line_after.strip() != '' or ty != DEL:
                yield "+", line_after
        line_before = ''
        line_after = ''

    for ty, text in fragment_parts(e, **kwargs):
        if text == '\n':
            yield from end_line(ty)
        else:
            if ty in (KEEP, DEL):
                line_before += text
            if ty in (KEEP, INS):
                line_after += text
    if line_before or line_after:
        yield from end_line(KEEP)


def dedent_pairs(pairs):
    """Dedent the `pairs`'s `text` part
    """
    pairs = list(pairs)

    # Using textwrap.dedent on this requires a few lines of hackery.
    types = [ty for ty, _line in pairs]
    dedented_lines = dedent(''.join(line + '\n' for ty, line in pairs)).splitlines()
    assert len(dedented_lines) == len(pairs)

    return zip(types, dedented_lines)


def print_pairs(pairs):
    last_line_was_empty = False

    for ty, line in pairs:
        if ty == KEEP and line == '':
            if last_line_was_empty:
                continue
            last_line_was_empty = True
        else:
            last_line_was_empty = False

        print(ty + line)


def print_fragment_patch(e):
    print_pairs(dedent_pairs(generate_fragment_patch(e)))


def is_annex_early_errors(e):
    """Returns True if the <emu-annex> element contains Early Errors.
    """
    h1 = e.find('{http://www.w3.org/1999/xhtml}h1')
    if 'Early Errors' in h1.text:
        return True

    p = e.find('{http://www.w3.org/1999/xhtml}p')
    if p:
        if 'Early Error' in html5lib.serializer.serialize(p):
            return True

    return False


def get_parent_map(document):
    """Returns a map from a element to parent element.
    This is necessary because `xml.etree.ElementTree.Element` doesn't have
    a reference to parent element.
    """
    parent_map = dict()
    for parent in document.iter():
        for child in parent:
            parent_map[child] = parent
    return parent_map


def get_titles(parent_map, e):
    """Returns a list of section titles for a section.
    """
    titles = []
    while e.tag != '{http://www.w3.org/1999/xhtml}body':
        h1 = e.find('{http://www.w3.org/1999/xhtml}h1')
        titles.insert(0, h1.text)
        e = parent_map[e]

    return titles


def generate_ul_fragment_patch(e, depth):
    """Similar to generate_fragment_patch, but for <ul>
    """
    first_line_prefix = '{}* '.format('  ' * depth)
    other_line_prefix = '{}  '.format('  ' * depth)

    for item in e:
        if item.tag != '{http://www.w3.org/1999/xhtml}li':
            raise ValueError("unrecognized element: " + item.tag)

        pairs = generate_fragment_patch(item,
                                        extra_rules=EXTRA_RULES_FOR_EE)

        is_first_line = True

        for ty, line in dedent_pairs(pairs):
            if is_first_line and line.strip() == '':
                continue

            if is_first_line:
                is_first_line = False
                yield ty, '{}{}'.format(first_line_prefix, line.strip())
            else:
                yield ty, '{}{}'.format(other_line_prefix, line.strip())


def generate_early_errors_fragment_patch(parent_map, e):
    for t in get_titles(parent_map, e):
        yield KEEP, '# {}'.format(t)
    yield KEEP, '# #{}'.format(e.attrib.get('id'))
    yield KEEP, ''

    for child in e:
        if child.tag == '{http://www.w3.org/1999/xhtml}h1':
            continue

        if child.tag == '{http://www.w3.org/1999/xhtml}emu-grammar':
            pairs = generate_fragment_patch(child)
            yield from dedent_pairs(pairs)
            yield KEEP, ''
        elif child.tag == '{http://www.w3.org/1999/xhtml}ul':
            yield from generate_ul_fragment_patch(child, 0)
        elif child.tag == '{http://www.w3.org/1999/xhtml}emu-note':
            pairs = generate_fragment_patch(child,
                                            extra_rules=EXTRA_RULES_FOR_EE)
            yield from dedent_pairs(pairs)
            yield KEEP, ''
        elif child.tag == '{http://www.w3.org/1999/xhtml}p':
            pairs = generate_fragment_patch(child,
                                            extra_rules=EXTRA_RULES_FOR_EE)
            yield from dedent_pairs(pairs)
            yield KEEP, ''
        elif (child.tag == '{http://www.w3.org/1999/xhtml}emu-alg'
              and e.attrib.get('id') == 'sec-__proto__-property-names-in-object-initializers'):
            # "__proto__ Property Names in Object Initializers" section
            # contains changes both for early errors and algorithm.
            # Ignore algorithm part.
            pass
        else:
            raise ValueError('unsupported element in early errors section: {}'
                             .format(child.tag))


def print_early_errors(parent_map, e):
    pairs = generate_early_errors_fragment_patch(parent_map, e)
    print_pairs(dedent_pairs(pairs))


def extract(filename, unfiltered, target):
    if filename.startswith("https:"):
        file_obj = urllib.request.urlopen(filename)
    else:
        file_obj = open(filename, "rb")

    with file_obj:
        document = html5lib.parse(file_obj)

    if target == 'grammar':
        for e in document.iter("{http://www.w3.org/1999/xhtml}emu-grammar"):
            if unfiltered or e.attrib.get("type") == "definition":
                print_fragment_patch(e)
    elif target == 'ee':
        parent_map = get_parent_map(document)
        for e in document.iter("{http://www.w3.org/1999/xhtml}emu-clause"):
            if e.attrib.get("id").endswith("-early-errors"):
                print_early_errors(parent_map, e)
    elif target == 'ee-annex':
        parent_map = get_parent_map(document)
        for e in document.iter("{http://www.w3.org/1999/xhtml}emu-annex"):
            if is_annex_early_errors(e):
                print_early_errors(parent_map, e)
    else:
        raise ValueError('Unknown target: {}'.format(target))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        description="Extract esgrammar from ECMAScript specifications.")
    parser.add_argument(
        'url',
        nargs=1,
        help="the https: url or local filename of an HTML file containing <emu-grammar> tags")
    parser.add_argument(
        '--unfiltered',
        action='store_true',
        help="Include even <emu-grammar> elements that don't have `type=definition`")
    parser.add_argument(
        '--target',
        default='grammar',
        choices=['grammar', 'ee', 'ee-annex'],
        help="What to extract (\
        grammar = esgrammar, \
        ee = early errors, \
        ee-annex = early errors in Annex\
        )")

    args = parser.parse_args()
    extract(args.url[0], args.unfiltered, args.target)
