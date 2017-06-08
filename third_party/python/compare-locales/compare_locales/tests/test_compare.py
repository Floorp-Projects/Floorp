# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import unittest

from compare_locales import compare


class TestTree(unittest.TestCase):
    '''Test the Tree utility class

    Tree value classes need to be in-place editable
    '''

    def test_empty_dict(self):
        tree = compare.Tree(dict)
        self.assertEqual(list(tree.getContent()), [])
        self.assertDictEqual(
            tree.toJSON(),
            {}
        )

    def test_disjoint_dict(self):
        tree = compare.Tree(dict)
        tree['one/entry']['leaf'] = 1
        tree['two/other']['leaf'] = 2
        self.assertEqual(
            list(tree.getContent()),
            [
                (0, 'key', ('one', 'entry')),
                (1, 'value', {'leaf': 1}),
                (0, 'key', ('two', 'other')),
                (1, 'value', {'leaf': 2})
            ]
        )
        self.assertDictEqual(
            tree.toJSON(),
            {
                'children': [
                    ('one/entry',
                     {'value': {'leaf': 1}}
                     ),
                    ('two/other',
                     {'value': {'leaf': 2}}
                     )
                ]
            }
        )
        self.assertMultiLineEqual(
            str(tree),
            '''\
one/entry
    {'leaf': 1}
two/other
    {'leaf': 2}\
'''
        )

    def test_overlapping_dict(self):
        tree = compare.Tree(dict)
        tree['one/entry']['leaf'] = 1
        tree['one/other']['leaf'] = 2
        self.assertEqual(
            list(tree.getContent()),
            [
                (0, 'key', ('one',)),
                (1, 'key', ('entry',)),
                (2, 'value', {'leaf': 1}),
                (1, 'key', ('other',)),
                (2, 'value', {'leaf': 2})
            ]
        )
        self.assertDictEqual(
            tree.toJSON(),
            {
                'children': [
                    ('one', {
                        'children': [
                            ('entry',
                             {'value': {'leaf': 1}}
                             ),
                            ('other',
                             {'value': {'leaf': 2}}
                             )
                        ]
                    })
                ]
            }
        )
