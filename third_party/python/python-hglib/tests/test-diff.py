from tests import common
from hglib.util import b

class test_diff(common.basetest):
    def test_basic(self):
        self.append('a', 'a\n')
        self.client.add(b('a'))
        diff1 = b("""diff -r 000000000000 a
--- /dev/null
+++ b/a
@@ -0,0 +1,1 @@
+a
""")
        self.assertEquals(diff1, self.client.diff(nodates=True))
        self.assertEquals(diff1, self.client.diff([b('a')], nodates=True))
        rev0, node0 = self.client.commit(b('first'))
        diff2 = b("""diff -r 000000000000 -r """) + node0[:12] + b(""" a
--- /dev/null
+++ b/a
@@ -0,0 +1,1 @@
+a
""")
        self.assertEquals(diff2, self.client.diff(change=rev0, nodates=True))
        self.append('a', 'a\n')
        rev1, node1 = self.client.commit(b('second'))
        diff3 = b("""diff -r """) + node0[:12] + b(""" a
--- a/a
+++ b/a
@@ -1,1 +1,2 @@
 a
+a
""")
        self.assertEquals(diff3, self.client.diff(revs=[rev0], nodates=True))
        diff4 = b("""diff -r """) + node0[:12] + b(" -r ") + node1[:12] + b(
            """ a
--- a/a
+++ b/a
@@ -1,1 +1,2 @@
 a
+a
""")
        self.assertEquals(diff4, self.client.diff(revs=[rev0, rev1],
                                                  nodates=True))

    def test_basic_plain(self):
        open('.hg/hgrc', 'a').write('[defaults]\ndiff=--git\n')
        self.test_basic()
