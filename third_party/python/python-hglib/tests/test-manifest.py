from tests import common
import hglib, os, stat
from hglib.util import b

class test_manifest(common.basetest):
    def test_basic(self):
        self.append('a', 'a')
        files = [b('a')]
        manifest = [(b('047b75c6d7a3ef6a2243bd0e99f94f6ea6683597'), b('644'),
                     False, False, b('a'))]

        if os.name == 'posix':
            self.append('b', 'b')
            os.chmod('b', os.stat('b')[0] | stat.S_IEXEC)
            os.symlink('b', 'c')

            files.extend([b('b'), b('c')])
            manifest.extend([(b('62452855512f5b81522aa3895892760bb8da9f3f'),
                              b('755'), True, False, b('b')),
                             (b('62452855512f5b81522aa3895892760bb8da9f3f'),
                              b('644'), False, True, b('c'))])

        self.client.commit(b('first'), addremove=True)

        self.assertEquals(list(self.client.manifest(all=True)), files)

        self.assertEquals(list(self.client.manifest()), manifest)
