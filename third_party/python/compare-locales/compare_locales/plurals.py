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
    # 14 (Macedonian)
    # CLDR: one, other
    ('one', 'two', 'other'),
    # 15 (Icelandic)
    ('one', 'other'),
    # 16 (Breton)
    ('one', 'two', 'few', 'many', 'other'),
    # 17 (Shuar)
    # CLDR: (missing)
    ('zero', 'other')
)

CATEGORIES_BY_LOCALE = {
    'ach': CATEGORIES_BY_INDEX[1],
    'af': CATEGORIES_BY_INDEX[1],
    'an': CATEGORIES_BY_INDEX[1],
    'ar': CATEGORIES_BY_INDEX[12],
    'as': CATEGORIES_BY_INDEX[1],
    'ast': CATEGORIES_BY_INDEX[1],
    'az': CATEGORIES_BY_INDEX[0],
    'be': CATEGORIES_BY_INDEX[7],
    'bg': CATEGORIES_BY_INDEX[1],
    'bn-BD': CATEGORIES_BY_INDEX[1],
    'bn-IN': CATEGORIES_BY_INDEX[1],
    'br': CATEGORIES_BY_INDEX[1],
    'bs': CATEGORIES_BY_INDEX[1],
    'ca': CATEGORIES_BY_INDEX[1],
    'cak': CATEGORIES_BY_INDEX[1],
    'cs': CATEGORIES_BY_INDEX[8],
    'cy': CATEGORIES_BY_INDEX[1],
    'da': CATEGORIES_BY_INDEX[1],
    'de': CATEGORIES_BY_INDEX[1],
    'dsb': CATEGORIES_BY_INDEX[10],
    'el': CATEGORIES_BY_INDEX[1],
    'en-GB': CATEGORIES_BY_INDEX[1],
    'en-US': CATEGORIES_BY_INDEX[1],
    'en-ZA': CATEGORIES_BY_INDEX[1],
    'eo': CATEGORIES_BY_INDEX[1],
    'es-AR': CATEGORIES_BY_INDEX[1],
    'es-CL': CATEGORIES_BY_INDEX[1],
    'es-ES': CATEGORIES_BY_INDEX[1],
    'es-MX': CATEGORIES_BY_INDEX[1],
    'et': CATEGORIES_BY_INDEX[1],
    'eu': CATEGORIES_BY_INDEX[1],
    'fa': CATEGORIES_BY_INDEX[0],
    'ff': CATEGORIES_BY_INDEX[1],
    'fi': CATEGORIES_BY_INDEX[1],
    'fr': CATEGORIES_BY_INDEX[2],
    'fy-NL': CATEGORIES_BY_INDEX[1],
    'ga-IE': CATEGORIES_BY_INDEX[11],
    'gd': CATEGORIES_BY_INDEX[4],
    'gl': CATEGORIES_BY_INDEX[1],
    'gn': CATEGORIES_BY_INDEX[1],
    'gu-IN': CATEGORIES_BY_INDEX[2],
    'he': CATEGORIES_BY_INDEX[1],
    'hi-IN': CATEGORIES_BY_INDEX[1],
    'hr': CATEGORIES_BY_INDEX[7],
    'hsb': CATEGORIES_BY_INDEX[10],
    'hu': CATEGORIES_BY_INDEX[1],
    'hy-AM': CATEGORIES_BY_INDEX[1],
    'ia': CATEGORIES_BY_INDEX[1],
    'id': CATEGORIES_BY_INDEX[0],
    'is': CATEGORIES_BY_INDEX[15],
    'it': CATEGORIES_BY_INDEX[1],
    'ja': CATEGORIES_BY_INDEX[0],
    'ja-JP-mac': CATEGORIES_BY_INDEX[0],
    'jiv': CATEGORIES_BY_INDEX[17],
    'ka': CATEGORIES_BY_INDEX[0],
    'kab': CATEGORIES_BY_INDEX[1],
    'kk': CATEGORIES_BY_INDEX[1],
    'km': CATEGORIES_BY_INDEX[1],
    'kn': CATEGORIES_BY_INDEX[1],
    'ko': CATEGORIES_BY_INDEX[0],
    'lij': CATEGORIES_BY_INDEX[1],
    'lo': CATEGORIES_BY_INDEX[0],
    'lt': CATEGORIES_BY_INDEX[6],
    'ltg': CATEGORIES_BY_INDEX[3],
    'lv': CATEGORIES_BY_INDEX[3],
    'mai': CATEGORIES_BY_INDEX[1],
    'mk': CATEGORIES_BY_INDEX[15],
    'ml': CATEGORIES_BY_INDEX[1],
    'mr': CATEGORIES_BY_INDEX[1],
    'ms': CATEGORIES_BY_INDEX[1],
    'my': CATEGORIES_BY_INDEX[1],
    'nb-NO': CATEGORIES_BY_INDEX[1],
    'ne-NP': CATEGORIES_BY_INDEX[1],
    'nl': CATEGORIES_BY_INDEX[1],
    'nn-NO': CATEGORIES_BY_INDEX[1],
    'oc': CATEGORIES_BY_INDEX[1],
    'or': CATEGORIES_BY_INDEX[1],
    'pa-IN': CATEGORIES_BY_INDEX[1],
    'pl': CATEGORIES_BY_INDEX[9],
    'pt-BR': CATEGORIES_BY_INDEX[1],
    'pt-PT': CATEGORIES_BY_INDEX[1],
    'rm': CATEGORIES_BY_INDEX[1],
    'ro': CATEGORIES_BY_INDEX[1],
    'ru': CATEGORIES_BY_INDEX[7],
    'si': CATEGORIES_BY_INDEX[1],
    'sk': CATEGORIES_BY_INDEX[8],
    'sl': CATEGORIES_BY_INDEX[10],
    'son': CATEGORIES_BY_INDEX[1],
    'sq': CATEGORIES_BY_INDEX[1],
    'sr': CATEGORIES_BY_INDEX[7],
    'sv-SE': CATEGORIES_BY_INDEX[1],
    'ta': CATEGORIES_BY_INDEX[1],
    'te': CATEGORIES_BY_INDEX[1],
    'th': CATEGORIES_BY_INDEX[0],
    'tl': CATEGORIES_BY_INDEX[1],
    'tr': CATEGORIES_BY_INDEX[0],
    'trs': CATEGORIES_BY_INDEX[1],
    'uk': CATEGORIES_BY_INDEX[7],
    'ur': CATEGORIES_BY_INDEX[1],
    'uz': CATEGORIES_BY_INDEX[0],
    'vi': CATEGORIES_BY_INDEX[1],
    'wo': CATEGORIES_BY_INDEX[0],
    'xh': CATEGORIES_BY_INDEX[1],
    'zam': CATEGORIES_BY_INDEX[1],
    'zh-CN': CATEGORIES_BY_INDEX[1],
    'zh-TW': CATEGORIES_BY_INDEX[0]
}
