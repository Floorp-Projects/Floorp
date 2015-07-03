import os
import gc
import unittest
import copy
import mock

import mozharness.base.log as log
from mozharness.base.log import ERROR
import mozharness.base.script as script
from mozharness.mozilla.blob_upload import BlobUploadMixin, \
    blobupload_config_options

class CleanupObj(script.ScriptMixin, log.LogMixin):
    def __init__(self):
        super(CleanupObj, self).__init__()
        self.log_obj = None
        self.config = {'log_level': ERROR}


def cleanup():
    gc.collect()
    c = CleanupObj()
    for f in ('test_logs', 'test_dir', 'tmpfile_stdout', 'tmpfile_stderr'):
        c.rmtree(f)


class BlobUploadScript(BlobUploadMixin, script.BaseScript):
    config_options = copy.deepcopy(blobupload_config_options)
    def __init__(self, **kwargs):
        self.abs_dirs = None
        self.set_buildbot_property = mock.Mock()
        super(BlobUploadScript, self).__init__(
                config_options=self.config_options,
                **kwargs
        )

    def query_python_path(self, binary="python"):
        if binary == "blobberc.py":
            return mock.Mock(return_value='/path/to/blobberc').return_value
        elif binary == "python":
            return mock.Mock(return_value='/path/to/python').return_value

    def query_abs_dirs(self):
        if self.abs_dirs:
            return self.abs_dirs
        abs_dirs = super(BlobUploadScript, self).query_abs_dirs()
        dirs = {}
        dirs['abs_blob_upload_dir'] = os.path.join(abs_dirs['abs_work_dir'],
                                                   'blobber_upload_dir')
        abs_dirs.update(dirs)
        self.abs_dirs = abs_dirs

        return self.abs_dirs

    def run_command(self, command):
        self.command = command

# TestBlobUploadMechanism {{{1
class TestBlobUploadMechanism(unittest.TestCase):
    # I need a log watcher helper function, here and in test_log.
    def setUp(self):
        cleanup()
        self.s = None

    def tearDown(self):
        # Close the logfile handles, or windows can't remove the logs
        if hasattr(self, 's') and isinstance(self.s, object):
            del(self.s)
        cleanup()

    def test_blob_upload_mechanism(self):
        self.s = BlobUploadScript(config={'log_type': 'multi',
                                          'blob_upload_branch': 'test-branch',
                                          'default_blob_upload_servers':
                                            ['http://blob_server.me'],
                                          'blob_uploader_auth_file':
                                            os.path.abspath(__file__)},
                                  initial_config_file='test/test.json')

        content = "Hello world!"
        parent_dir = self.s.query_abs_dirs()['abs_blob_upload_dir']
        if not os.path.isdir(parent_dir):
            self.s.mkdir_p(parent_dir)

        file_name = os.path.join(parent_dir, 'test_mock_blob_file')
        self.s.write_to_file(file_name, content)
        self.s.upload_blobber_files()
        self.assertTrue(self.s.set_buildbot_property.called)

        expected_result = ['/path/to/python', '/path/to/blobberc', '-u',
                           'http://blob_server.me', '-a',
                           os.path.abspath(__file__), '-b', 'test-branch', '-d']
        expected_result.append(self.s.query_abs_dirs()['abs_blob_upload_dir'])
        expected_result += [
                '--output-manifest',
                os.path.join(self.s.query_abs_dirs()['abs_work_dir'], "uploaded_files.json")
        ]
        self.assertEqual(expected_result, self.s.command)


# main {{{1
if __name__ == '__main__':
    unittest.main()
