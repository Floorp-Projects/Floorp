import os
from tests import common
from hglib.util import b

class test_status(common.basetest):
    def test_empty(self):
        self.assertEquals(self.client.status(), [])

    def test_one_of_each(self):
        self.append('.hgignore', 'ignored')
        self.append('ignored', 'a')
        self.append('clean', 'a')
        self.append('modified', 'a')
        self.append('removed', 'a')
        self.append('missing', 'a')
        self.client.commit(b('first'), addremove=True)
        self.append('modified', 'a')
        self.append('added', 'a')
        self.client.add([b('added')])
        os.remove('missing')
        self.client.remove([b('removed')])
        self.append('untracked')

        l = [(b('M'), b('modified')),
             (b('A'), b('added')),
             (b('R'), b('removed')),
             (b('C'), b('.hgignore')),
             (b('C'), b('clean')),
             (b('!'), b('missing')),
             (b('?'), b('untracked')),
             (b('I'), b('ignored'))]

        st = self.client.status(all=True)

        for i in l:
            self.assertTrue(i in st)

    def test_copy(self):
        self.append('source', 'a')
        self.client.commit(b('first'), addremove=True)
        self.client.copy(b('source'), b('dest'))
        l = [(b('A'), b('dest')), (b(' '), b('source'))]
        self.assertEquals(self.client.status(copies=True), l)

    def test_copy_origin_space(self):
        self.append('s ource', 'a')
        self.client.commit(b('first'), addremove=True)
        self.client.copy(b('s ource'), b('dest'))
        l = [(b('A'), b('dest')), (b(' '), b('s ource'))]
        self.assertEquals(self.client.status(copies=True), l)
