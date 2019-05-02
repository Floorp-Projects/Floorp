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
    'ace': CATEGORIES_BY_INDEX[0],
    'ach': CATEGORIES_BY_INDEX[1],
    'af': CATEGORIES_BY_INDEX[1],
    'ak': CATEGORIES_BY_INDEX[2],
    'an': CATEGORIES_BY_INDEX[1],
    'ar': CATEGORIES_BY_INDEX[12],
    'arn': CATEGORIES_BY_INDEX[1],
    'as': CATEGORIES_BY_INDEX[1],
    'ast': CATEGORIES_BY_INDEX[1],
    'az': CATEGORIES_BY_INDEX[1],
    'be': CATEGORIES_BY_INDEX[7],
    'bg': CATEGORIES_BY_INDEX[1],
    'bn': CATEGORIES_BY_INDEX[2],
    'bn-BD': CATEGORIES_BY_INDEX[2],
    'bn-IN': CATEGORIES_BY_INDEX[2],
    'br': CATEGORIES_BY_INDEX[16],
    'brx': CATEGORIES_BY_INDEX[1],
    'bs': CATEGORIES_BY_INDEX[19],
    'ca': CATEGORIES_BY_INDEX[1],
    'cak': CATEGORIES_BY_INDEX[1],
    'crh': CATEGORIES_BY_INDEX[1],
    'cs': CATEGORIES_BY_INDEX[8],
    'csb': CATEGORIES_BY_INDEX[9],
    'cv': CATEGORIES_BY_INDEX[1],
    'cy': CATEGORIES_BY_INDEX[18],
    'da': CATEGORIES_BY_INDEX[1],
    'de': CATEGORIES_BY_INDEX[1],
    'dsb': CATEGORIES_BY_INDEX[10],
    'el': CATEGORIES_BY_INDEX[1],
    'en-CA': CATEGORIES_BY_INDEX[1],
    'en-GB': CATEGORIES_BY_INDEX[1],
    'en-US': CATEGORIES_BY_INDEX[1],
    'en-ZA': CATEGORIES_BY_INDEX[1],
    'en-x-moz-reference': CATEGORIES_BY_INDEX[1],  # for reference validation
    'eo': CATEGORIES_BY_INDEX[1],
    'es-AR': CATEGORIES_BY_INDEX[1],
    'es-CL': CATEGORIES_BY_INDEX[1],
    'es-ES': CATEGORIES_BY_INDEX[1],
    'es-MX': CATEGORIES_BY_INDEX[1],
    'et': CATEGORIES_BY_INDEX[1],
    'eu': CATEGORIES_BY_INDEX[1],
    'fa': CATEGORIES_BY_INDEX[2],
    'ff': CATEGORIES_BY_INDEX[1],
    'fi': CATEGORIES_BY_INDEX[1],
    'fr': CATEGORIES_BY_INDEX[2],
    'frp': CATEGORIES_BY_INDEX[2],
    'fur': CATEGORIES_BY_INDEX[1],
    'fy-NL': CATEGORIES_BY_INDEX[1],
    'ga-IE': CATEGORIES_BY_INDEX[11],
    'gd': CATEGORIES_BY_INDEX[4],
    'gl': CATEGORIES_BY_INDEX[1],
    'gn': CATEGORIES_BY_INDEX[1],
    'gu-IN': CATEGORIES_BY_INDEX[2],
    'he': CATEGORIES_BY_INDEX[1],
    'hi-IN': CATEGORIES_BY_INDEX[2],
    'hr': CATEGORIES_BY_INDEX[19],
    'hsb': CATEGORIES_BY_INDEX[10],
    'hto': CATEGORIES_BY_INDEX[1],
    'hu': CATEGORIES_BY_INDEX[1],
    'hy-AM': CATEGORIES_BY_INDEX[1],
    'ia': CATEGORIES_BY_INDEX[1],
    'id': CATEGORIES_BY_INDEX[0],
    'ilo': CATEGORIES_BY_INDEX[0],
    'is': CATEGORIES_BY_INDEX[15],
    'it': CATEGORIES_BY_INDEX[1],
    'ja': CATEGORIES_BY_INDEX[0],
    'ja-JP-mac': CATEGORIES_BY_INDEX[0],
    'jiv': CATEGORIES_BY_INDEX[17],
    'ka': CATEGORIES_BY_INDEX[1],
    'kab': CATEGORIES_BY_INDEX[1],
    'kk': CATEGORIES_BY_INDEX[1],
    'km': CATEGORIES_BY_INDEX[0],
    'kn': CATEGORIES_BY_INDEX[1],
    'ko': CATEGORIES_BY_INDEX[0],
    'ks': CATEGORIES_BY_INDEX[1],
    'ku': CATEGORIES_BY_INDEX[1],
    'lb': CATEGORIES_BY_INDEX[1],
    'lg': CATEGORIES_BY_INDEX[1],
    'lij': CATEGORIES_BY_INDEX[1],
    'lo': CATEGORIES_BY_INDEX[0],
    'lt': CATEGORIES_BY_INDEX[6],
    'ltg': CATEGORIES_BY_INDEX[3],
    'lv': CATEGORIES_BY_INDEX[3],
    'lus': CATEGORIES_BY_INDEX[0],
    'mai': CATEGORIES_BY_INDEX[1],
    'meh': CATEGORIES_BY_INDEX[0],
    'mix': CATEGORIES_BY_INDEX[0],
    'mk': CATEGORIES_BY_INDEX[15],
    'ml': CATEGORIES_BY_INDEX[1],
    'mn': CATEGORIES_BY_INDEX[1],
    'mr': CATEGORIES_BY_INDEX[1],
    'ms': CATEGORIES_BY_INDEX[0],
    'my': CATEGORIES_BY_INDEX[0],
    'nb-NO': CATEGORIES_BY_INDEX[1],
    'ne-NP': CATEGORIES_BY_INDEX[1],
    'nl': CATEGORIES_BY_INDEX[1],
    'nn-NO': CATEGORIES_BY_INDEX[1],
    'nr': CATEGORIES_BY_INDEX[1],
    'nso': CATEGORIES_BY_INDEX[2],
    'ny': CATEGORIES_BY_INDEX[1],
    'oc': CATEGORIES_BY_INDEX[2],
    'or': CATEGORIES_BY_INDEX[1],
    'pa-IN': CATEGORIES_BY_INDEX[2],
    'pai': CATEGORIES_BY_INDEX[0],
    'pl': CATEGORIES_BY_INDEX[9],
    'pt-BR': CATEGORIES_BY_INDEX[1],
    'pt-PT': CATEGORIES_BY_INDEX[1],
    'quy': CATEGORIES_BY_INDEX[1],
    'qvi': CATEGORIES_BY_INDEX[1],
    'rm': CATEGORIES_BY_INDEX[1],
    'ro': CATEGORIES_BY_INDEX[5],
    'ru': CATEGORIES_BY_INDEX[7],
    'rw': CATEGORIES_BY_INDEX[1],
    'sah': CATEGORIES_BY_INDEX[0],
    'sat': CATEGORIES_BY_INDEX[1],
    'sc': CATEGORIES_BY_INDEX[1],
    'scn': CATEGORIES_BY_INDEX[1],
    'si': CATEGORIES_BY_INDEX[1],
    'sk': CATEGORIES_BY_INDEX[8],
    'sl': CATEGORIES_BY_INDEX[10],
    'son': CATEGORIES_BY_INDEX[1],
    'sq': CATEGORIES_BY_INDEX[1],
    'sr': CATEGORIES_BY_INDEX[19],
    'ss': CATEGORIES_BY_INDEX[1],
    'st': CATEGORIES_BY_INDEX[1],
    'sv-SE': CATEGORIES_BY_INDEX[1],
    'sw': CATEGORIES_BY_INDEX[1],
    'ta-LK': CATEGORIES_BY_INDEX[1],
    'ta': CATEGORIES_BY_INDEX[1],
    'te': CATEGORIES_BY_INDEX[1],
    'th': CATEGORIES_BY_INDEX[0],
    'tl': CATEGORIES_BY_INDEX[1],
    'tn': CATEGORIES_BY_INDEX[1],
    'tr': CATEGORIES_BY_INDEX[1],
    'trs': CATEGORIES_BY_INDEX[1],
    'ts': CATEGORIES_BY_INDEX[1],
    'tsz': CATEGORIES_BY_INDEX[1],
    'uk': CATEGORIES_BY_INDEX[7],
    'ur': CATEGORIES_BY_INDEX[1],
    'uz': CATEGORIES_BY_INDEX[1],
    've': CATEGORIES_BY_INDEX[1],
    'vi': CATEGORIES_BY_INDEX[0],
    'wo': CATEGORIES_BY_INDEX[0],
    'xh': CATEGORIES_BY_INDEX[1],
    'zam': CATEGORIES_BY_INDEX[1],
    'zh-CN': CATEGORIES_BY_INDEX[0],
    'zh-TW': CATEGORIES_BY_INDEX[0],
    'zu': CATEGORIES_BY_INDEX[2],
}
