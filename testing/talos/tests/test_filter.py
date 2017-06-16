#!/usr/bin/env python

"""
test talos' filter module:

http://hg.mozilla.org/build/talos/file/tip/talos/filter.py
"""

import unittest
import talos.filter


class TestFilter(unittest.TestCase):

    data = range(30)  # test data

    def test_ignore(self):
        """test the ignore filter"""
        # a bit of a stub sanity test for a single filter

        filtered = talos.filter.ignore_first(self.data)
        self.assertEquals(filtered, self.data[1:])

        filtered = talos.filter.ignore_first(self.data, 10)
        self.assertEquals(filtered, self.data[10:])

        # short series won't be filtered
        filtered = talos.filter.ignore_first(self.data, 50)
        self.assertEquals(filtered, self.data)

    def test_getting_filters(self):
        """test getting a list of filters from a string"""

        filter_names = ['ignore_max', 'ignore_max', 'max']

        # get the filter functions
        filters = talos.filter.filters(*filter_names)
        self.assertEquals(len(filter_names), len(filters))
        for filter in filters:
            self.assertTrue(self, hasattr(filter, '__call__'))

        # apply them on the data
        filtered = talos.filter.apply(self.data, filters)
        self.assertEquals(filtered, 27)

    def test_parse(self):
        """test the filter name parse function"""

        # an example with no arguments
        parsed = talos.filter.parse('mean')
        self.assertEquals(parsed, ['mean', []])

        # an example with one integer argument
        parsed = talos.filter.parse('ignore_first:10')
        self.assertEquals(parsed, ['ignore_first', [10]])
        self.assertEquals(type(parsed[1][0]), int)
        self.assertNotEqual(type(parsed[1][0]), float)

        # an example with several arguments

        # temporarily add foo
        # value is lambda function to mimic filter_dict key:value pair
        talos.filter.scalar_filters['foo'] = lambda *args: args
        parsed = talos.filter.parse('foo:10.1,2,5.0,6.')
        self.assertEquals(parsed, ['foo', [10.1, 2, 5.0, 6.0]])
        for index in (2, 3):
            self.assertEquals(type(parsed[1][index]), float)
            self.assertNotEqual(type(parsed[1][index]), int)

        # an example that should fail
        self.assertRaises(ValueError, talos.filter.parse, 'foo:bar')
        self.assertRaises(ValueError, talos.filter.parse, 'foo:1,')

        # delete foo again
        del talos.filter.scalar_filters['foo']


if __name__ == '__main__':
    unittest.main()
