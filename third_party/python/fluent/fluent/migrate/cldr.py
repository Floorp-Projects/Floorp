# coding=utf8

import pkgutil
import json


def in_canonical_order(item):
    return canonical_order.index(item)


cldr_plurals = json.loads(
    pkgutil.get_data('fluent.migrate', 'cldr_data/plurals.json').decode('utf-8')
)

rules = cldr_plurals['supplemental']['plurals-type-cardinal']
canonical_order = ('zero', 'one', 'two', 'few', 'many', 'other')

categories = {}
for lang, rules in rules.items():
    categories[lang] = tuple(sorted(map(
        lambda key: key.replace('pluralRule-count-', ''),
        rules.keys()
    ), key=in_canonical_order))


def get_plural_categories(lang):
    """Return a tuple of CLDR plural categories for `lang`.

    If an exact match for `lang` is not available, recursively fall back to
    a language code with the last subtag stripped. That is, if `ja-JP-mac` is
    not defined in CLDR, the code will try `ja-JP` and then `ja`.

    If no matches are found, a `RuntimeError` is raised.

    >>> get_plural_categories('sl')
    ('one', 'two', 'few', 'other')
    >>> get_plural_categories('ga-IE')
    ('one', 'few', 'two', 'few', 'other')
    >>> get_plural_categories('ja-JP-mac')
    ('other')

    """

    langs_categories = categories.get(lang, None)

    if langs_categories is None:
        # Remove the trailing subtag.
        fallback_lang, _, _ = lang.rpartition('-')

        if fallback_lang == '':
            raise RuntimeError('Unknown language: {}'.format(lang))

        return get_plural_categories(fallback_lang)

    return langs_categories
