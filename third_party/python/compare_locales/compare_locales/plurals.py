# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

'Mapping of locales to CLDR plural categories as implemented by PluralForm.jsm'

CATEGORIES_BY_INDEX = (
    # 0 (Chinese)
    ('other',),
    # 1 (English)
    ('one', 'other'),
    # 2 (French)
    ('one', 'other'),
    # 3 (Latvian)
    ('zero', 'one', 'other'),
    # 4 (Scottish Gaelic)
    ('one', 'two', 'few', 'other'),
    # 5 (Romanian)
    ('one', 'few', 'other'),
    # 6 (Lithuanian)
    # CLDR: one, few, many (fractions), other
    ('one', 'other', 'few'),
    # 7 (Russian)
    # CLDR: one, few, many, other (fractions)
    ('one', 'few', 'many'),
    # 8 (Slovak)
    # CLDR: one, few, many (fractions), other
    ('one', 'few', 'other'),
    # 9 (Polish)
    # CLDR: one, few, many, other (fractions)
    ('one', 'few', 'many'),
    # 10 (Slovenian)
    ('one', 'two', 'few', 'other'),
    # 11 (Irish Gaelic)
    ('one', 'two', 'few', 'many', 'other'),
    # 12 (Arabic)
    # CLDR: zero, one, two, few, many, other
    ('one', 'two', 'few', 'many', 'other', 'zero'),
    # 13 (Maltese)
    ('one', 'few', 'many', 'other'),
    # 14 (Unused)
    # CLDR: one, other
    ('one', 'two', 'other'),
    # 15 (Icelandic, Macedonian)
    ('one', 'other'),
    # 16 (Breton)
    ('one', 'two', 'few', 'many', 'other'),
    # 17 (Shuar)
    # CLDR: (missing)
    ('zero', 'other'),
    # 18 (Welsh),
    ('zero', 'one', 'two', 'few', 'many', 'other'),
    # 19 (Bosnian, Croatian, Serbian)
    ('one', 'few', 'other'),
)

CATEGORIES_EXCEPTIONS = {
}

CATEGORIES_BY_LOCALE = {
    'ace': 0,
    'ach': 1,
    'af': 1,
    'ak': 2,
    'an': 1,
    'ar': 12,
    'arn': 1,
    'as': 1,
    'ast': 1,
    'az': 1,
    'be': 7,
    'bg': 1,
    'bn': 2,
    'bo': 0,
    'br': 16,
    'brx': 1,
    'bs': 19,
    'ca': 1,
    'cak': 1,
    'ckb': 1,
    'crh': 1,
    'cs': 8,
    'csb': 9,
    'cv': 1,
    'cy': 18,
    'da': 1,
    'de': 1,
    'dsb': 10,
    'el': 1,
    'en': 1,
    'eo': 1,
    'es': 1,
    'et': 1,
    'eu': 1,
    'fa': 2,
    'ff': 1,
    'fi': 1,
    'fr': 2,
    'frp': 2,
    'fur': 1,
    'fy': 1,
    'ga': 11,
    'gd': 4,
    'gl': 1,
    'gn': 1,
    'gu': 2,
    'he': 1,
    'hi': 2,
    'hr': 19,
    'hsb': 10,
    'hto': 1,
    'hu': 1,
    'hy': 1,
    'hye': 1,
    'ia': 1,
    'id': 0,
    'ilo': 0,
    'is': 15,
    'it': 1,
    'ja': 0,
    'jiv': 17,
    'ka': 1,
    'kab': 1,
    'kk': 1,
    'km': 0,
    'kn': 1,
    'ko': 0,
    'ks': 1,
    'ku': 1,
    'lb': 1,
    'lg': 1,
    'lij': 1,
    'lo': 0,
    'lt': 6,
    'ltg': 3,
    'lv': 3,
    'lus': 0,
    'mai': 1,
    'meh': 0,
    'mix': 0,
    'mk': 15,
    'ml': 1,
    'mn': 1,
    'mr': 1,
    'ms': 0,
    'my': 0,
    'nb': 1,
    'ne': 1,
    'nl': 1,
    'nn': 1,
    'nr': 1,
    'nso': 2,
    'ny': 1,
    'oc': 2,
    'or': 1,
    'pa': 2,
    'pai': 0,
    'pl': 9,
    'pt': 1,
    'quy': 1,
    'qvi': 1,
    'rm': 1,
    'ro': 5,
    'ru': 7,
    'rw': 1,
    'sah': 0,
    'sat': 1,
    'sc': 1,
    'scn': 1,
    'sco': 1,
    'si': 1,
    'sk': 8,
    'skr': 1,
    'sl': 10,
    'son': 1,
    'sq': 1,
    'sr': 19,
    'ss': 1,
    'st': 1,
    'sv': 1,
    'sw': 1,
    'szl': 9,
    'ta': 1,
    'ta': 1,
    'te': 1,
    'tg': 1,
    'th': 0,
    'tl': 1,
    'tn': 1,
    'tr': 1,
    'trs': 1,
    'ts': 1,
    'tsz': 1,
    'uk': 7,
    'ur': 1,
    'uz': 1,
    've': 1,
    'vi': 0,
    'wo': 0,
    'xh': 1,
    'zam': 1,
    'zh-CN': 0,
    'zh-TW': 0,
    'zu': 2,
}


def get_plural(locale):
    plural_form = get_plural_rule(locale)
    if plural_form is None:
        return None
    return CATEGORIES_BY_INDEX[plural_form]


def get_plural_rule(locale):
    if locale is None:
        return None
    if locale in CATEGORIES_BY_LOCALE:
        return CATEGORIES_BY_LOCALE[locale]
    locale = locale.split('-', 1)[0]
    return CATEGORIES_BY_LOCALE.get(locale)
