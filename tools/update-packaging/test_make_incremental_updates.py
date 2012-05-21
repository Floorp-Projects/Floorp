#!/usr/bin/python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import unittest
import make_incremental_updates as mkup
from make_incremental_updates import PatchInfo, MarFileEntry

class TestPatchInfo(unittest.TestCase):
    def setUp(self):
        self.work_dir = 'work_dir'
        self.file_exclusion_list = ['channel-prefs.js','update.manifest','updatev2.manifest','removed-files']
        self.path_exclusion_list = ['/readme.txt']
        self.patch_info = PatchInfo(self.work_dir, self.file_exclusion_list, self.path_exclusion_list)

    def testPatchInfo(self):
        self.assertEquals(self.work_dir, self.patch_info.work_dir)
        self.assertEquals([], self.patch_info.archive_files)
        self.assertEquals([], self.patch_info.manifestv1)
        self.assertEquals([], self.patch_info.manifestv2)
        self.assertEquals(self.file_exclusion_list, self.patch_info.file_exclusion_list)
        self.assertEquals(self.path_exclusion_list, self.patch_info.path_exclusion_list)

    def test_append_add_instruction(self):
        self.patch_info.append_add_instruction('file.test')
        self.assertEquals(['add "file.test"'], self.patch_info.manifestv1)

    def test_append_patch_instruction(self):
        self.patch_info.append_patch_instruction('file.test', 'patchname')
        self.assertEquals(['patch "patchname" "file.test"'], self.patch_info.manifestv1)

    """ FIXME touches the filesystem, need refactoring
    def test_append_remove_instruction(self):
        self.patch_info.append_remove_instruction('file.test')
        self.assertEquals(['remove "file.test"'], self.patch_info.manifestv1)

    def test_create_manifest_file(self):
        self.patch_info.create_manifest_file()
    """

    def test_build_marfile_entry_hash(self):
        self.assertEquals(({}, set([]), set([])), self.patch_info.build_marfile_entry_hash('root_path'))

""" FIXME touches the filesystem, need refactoring
class TestMarFileEntry(unittest.TestCase):
    def setUp(self):
        root_path = '.'
        self.filename = 'file.test'
        f = open(self.filename, 'w')
        f.write('Test data\n')
        f.close()
        self.mar_file_entry = MarFileEntry(root_path, self.filename)

    def test_calc_file_sha_digest(self):
        f = open('test.sha', 'r')
        goodSha = f.read()
        f.close()
        sha = self.mar_file_entry.calc_file_sha_digest(self.filename)
        self.assertEquals(goodSha, sha)

    def test_sha(self):
        f = open('test.sha', 'r')
        goodSha = f.read()
        f.close()
        sha = self.mar_file_entry.sha()
        self.assertEquals(goodSha, sha)
"""

class TestMakeIncrementalUpdates(unittest.TestCase):
    def setUp(self):
        work_dir = '.'
        self.patch_info = PatchInfo(work_dir, ['channel-prefs.js','update.manifest','updatev2.manifest','removed-files'],['/readme.txt'])
        root_path = '/'
        filename = 'test.file'
        self.mar_file_entry = MarFileEntry(root_path, filename)

    """ FIXME makes direct shell calls, need refactoring
    def test_exec_shell_cmd(self):
        mkup.exec_shell_cmd('echo test')

    def test_copy_file(self):
        mkup.copy_file('src_file_abs_path', 'dst_file_abs_path')

    def test_bzip_file(self):
        mkup.bzip_file('filename')

    def test_bunzip_file(self):
        mkup.bunzip_file('filename')

    def test_extract_mar(self): 
        mkup.extract_mar('filename', 'work_dir')

    def test_create_partial_patch_for_file(self):
        mkup.create_partial_patch_for_file('from_marfile_entry', 'to_marfile_entry', 'shas', self.patch_info)

    def test_create_add_patch_for_file(self):           
        mkup.create_add_patch_for_file('to_marfile_entry', self.patch_info)
        
    def test_process_explicit_remove_files(self): 
        mkup.process_explicit_remove_files('dir_path', self.patch_info)
    
    def test_create_partial_patch(self):
        mkup.create_partial_patch('from_dir_path', 'to_dir_path', 'patch_filename', 'shas', self.patch_info, 'forced_updates')

    def test_create_partial_patches(patches):
        mkup.create_partial_patches('patches')

    """

    """ FIXME touches the filesystem, need refactoring
    def test_get_buildid(self):
        mkup.get_buildid('work_dir', 'platform')
    """

    def test_decode_filename(self):
        expected = {'locale': 'lang', 'platform': 'platform', 'product': 'product', 'version': '1.0', 'type': 'complete'}
        self.assertEquals(expected, mkup.decode_filename('product-1.0.lang.platform.complete.mar'))
        self.assertRaises(Exception, mkup.decode_filename, 'fail')
       
if __name__ == '__main__':
    unittest.main()
