import os

import unittest
import mozunit
from taskcluster_graph.templates import (
    Templates,
    TemplatesException
)

class TemplatesTest(unittest.TestCase):

    def setUp(self):
        abs_path = os.path.abspath(os.path.dirname(__file__))
        self.subject = Templates(os.path.join(abs_path, 'fixtures'))


    def test_invalid_path(self):
        with self.assertRaisesRegexp(TemplatesException, 'must be a directory'):
            Templates('/zomg/not/a/dir')

    def test_no_templates(self):
        content = self.subject.load('simple.yml', {})
        self.assertEquals(content, {
            'is_simple': True
        })

    def test_with_templates(self):
        content = self.subject.load('templates.yml', {
            'woot': 'bar'
        })

        self.assertEquals(content, {
            'content': 'content',
            'variable': 'bar'
        })

    def test_inheritance(self):
        '''
        The simple single pass inheritance case.
        '''
        content = self.subject.load('inherit.yml', {})
        self.assertEqual(content, {
            'content': 'content',
            'variable': 'inherit'
        })

    def test_inheritance_implicat_pass(self):
        '''
        Implicitly pass parameters from the child to the ancestor.
        '''
        content = self.subject.load('inherit_pass.yml', {
            'a': 'overriden'
        })

        self.assertEqual(content, { 'values': ['overriden', 'b', 'c'] });


    def test_inheritance_circular(self):
        '''
        Circular reference handling.
        '''
        with self.assertRaisesRegexp(TemplatesException, 'circular'):
            self.subject.load('circular.yml', {})

    def test_deep_inheritance(self):
        content = self.subject.load('deep/4.yml', {
            'value': 'myvalue'
        })
        self.assertEqual(content, { 'variable': 'myvalue' })

    def test_inheritance_with_simple_extensions(self):
        content = self.subject.load('extend_parent.yml', {})
        self.assertEquals(content, {
            'list': ['1', '2', '3', '4'],
            'obj': {
                'from_parent': True,
                'deeper': {
                    'woot': 'bar',
                    'list': ['baz', 'bar']
                },
                'level': 2,
            },
            'was_list': { 'replaced': True }
        })


if __name__ == '__main__':
    mozunit.main()
