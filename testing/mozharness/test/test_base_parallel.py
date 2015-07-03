import unittest

from mozharness.base.parallel import ChunkingMixin


class TestChunkingMixin(unittest.TestCase):
    def setUp(self):
        self.c = ChunkingMixin()

    def test_one_chunk(self):
        self.assertEquals(self.c.query_chunked_list([1, 3, 2], 1, 1), [1, 3, 2])

    def test_sorted(self):
        self.assertEquals(self.c.query_chunked_list([1, 3, 2], 1, 1, sort=True), [1, 2, 3])

    def test_first_chunk(self):
        self.assertEquals(self.c.query_chunked_list([4, 5, 4, 3], 1, 2), [4, 5])

    def test_last_chunk(self):
        self.assertEquals(self.c.query_chunked_list([1, 4, 5, 7, 5, 6], 3, 3), [5, 6])

    def test_not_evenly_divisble(self):
        thing = [1, 3, 6, 4, 3, 2, 6]
        self.assertEquals(self.c.query_chunked_list(thing, 1, 3), [1, 3, 6])
        self.assertEquals(self.c.query_chunked_list(thing, 2, 3), [4, 3])
        self.assertEquals(self.c.query_chunked_list(thing, 3, 3), [2, 6])
