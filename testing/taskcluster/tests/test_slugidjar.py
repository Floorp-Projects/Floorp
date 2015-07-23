import unittest
import mozunit
from datetime import datetime
from taskcluster_graph.slugidjar import SlugidJar

class SlugidJarTest(unittest.TestCase):

    def test_slugidjar(self):
        subject = SlugidJar()
        self.assertEqual(subject('woot'), subject('woot'))
        self.assertTrue(type(subject('woot')) is str)

        other_jar = SlugidJar()
        self.assertNotEqual(subject('woot'), other_jar('woot'))

if __name__ == '__main__':
    mozunit.main()


