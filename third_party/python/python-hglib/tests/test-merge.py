from tests import common
import hglib
from hglib.util import b

class test_merge(common.basetest):
    def setUp(self):
        common.basetest.setUp(self)

        self.append('a', 'a')
        rev, self.node0 = self.client.commit(b('first'), addremove=True)

        self.append('a', 'a')
        rev, self.node1 = self.client.commit(b('change'))

    def test_basic(self):
        self.client.update(self.node0)
        self.append('b', 'a')
        rev, node2 = self.client.commit(b('new file'), addremove=True)
        self.client.merge(self.node1)
        rev, node = self.client.commit(b('merge'))
        diff = b("diff -r ") + node2[:12] + b(" -r ") + node[:12] + b(""" a
--- a/a
+++ b/a
@@ -1,1 +1,1 @@
-a
\ No newline at end of file
+aa
\ No newline at end of file
""")

        self.assertEquals(diff, self.client.diff(change=node, nodates=True))

    def test_merge_prompt_abort(self):
        self.client.update(self.node0)
        self.client.remove(b('a'))
        self.client.commit(b('remove'))

        self.assertRaises(hglib.error.CommandError, self.client.merge)

    def test_merge_prompt_noninteractive(self):
        self.client.update(self.node0)
        self.client.remove(b('a'))
        rev, node = self.client.commit(b('remove'))

        if self.client.version >= (3, 7):
            self.assertRaises(hglib.error.CommandError,
                self.client.merge,
                cb=hglib.merge.handlers.noninteractive)
        else:
            self.client.merge(cb=hglib.merge.handlers.noninteractive)

        diff = b("diff -r ") + node[:12] + b(""" a
--- /dev/null
+++ b/a
@@ -0,0 +1,1 @@
+aa
\ No newline at end of file
""")
        self.assertEquals(diff, self.client.diff(nodates=True))

    def test_merge_prompt_cb(self):
        self.client.update(self.node0)
        self.client.remove(b('a'))
        rev, node = self.client.commit(b('remove'))

        def cb(output):
            return b('c')

        self.client.merge(cb=cb)

        diff = b("diff -r ") + node[:12] + b(""" a
--- /dev/null
+++ b/a
@@ -0,0 +1,1 @@
+aa
\ No newline at end of file
""")
        self.assertEquals(diff, self.client.diff(nodates=True))
