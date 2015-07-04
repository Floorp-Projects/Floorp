import unittest
import mock

from mozharness.base.transfer import TransferMixin


class GoodMockMixin(object):
    def query_abs_dirs(self):
        return {'abs_work_dir': ''}

    def query_exe(self, exe):
        return exe

    def info(self, msg):
        pass

    def log(self, msg, level):
        pass

    def run_command(*args, **kwargs):
        return 0


class BadMockMixin(GoodMockMixin):
    def run_command(*args, **kwargs):
        return 1


class GoodTransferMixin(TransferMixin, GoodMockMixin):
    pass


class BadTransferMixin(TransferMixin, BadMockMixin):
    pass


class TestTranferMixin(unittest.TestCase):
    @mock.patch('mozharness.base.transfer.os')
    def test_rsync_upload_dir_not_a_dir(self, os_mock):
        # simulates upload dir but dir is a file
        os_mock.path.isdir.return_value = False
        tm = GoodTransferMixin()
        self.assertEqual(tm.rsync_upload_directory(
                         local_path='',
                         ssh_key='my ssh key',
                         ssh_user='my ssh user',
                         remote_host='remote host',
                         remote_path='remote path',), -1)

    @mock.patch('mozharness.base.transfer.os')
    def test_rsync_upload_fails_create_remote_dir(self, os_mock):
        # we cannot create the remote directory
        os_mock.path.isdir.return_value = True
        tm = BadTransferMixin()
        self.assertEqual(tm.rsync_upload_directory(
                         local_path='',
                         ssh_key='my ssh key',
                         ssh_user='my ssh user',
                         remote_host='remote host',
                         remote_path='remote path',
                         create_remote_directory=True), -2)

    @mock.patch('mozharness.base.transfer.os')
    def test_rsync_upload_fails_do_not_create_remote_dir(self, os_mock):
        # upload fails, remote directory is not created
        os_mock.path.isdir.return_value = True
        tm = BadTransferMixin()
        self.assertEqual(tm.rsync_upload_directory(
                         local_path='',
                         ssh_key='my ssh key',
                         ssh_user='my ssh user',
                         remote_host='remote host',
                         remote_path='remote path',
                         create_remote_directory=False), -3)

    @mock.patch('mozharness.base.transfer.os')
    def test_rsync_upload(self, os_mock):
        # simulates an upload with no errors
        os_mock.path.isdir.return_value = True
        tm = GoodTransferMixin()
        self.assertEqual(tm.rsync_upload_directory(
                         local_path='',
                         ssh_key='my ssh key',
                         ssh_user='my ssh user',
                         remote_host='remote host',
                         remote_path='remote path',
                         create_remote_directory=False), None)

    @mock.patch('mozharness.base.transfer.os')
    def test_rsync_download_in_not_a_dir(self, os_mock):
        # local path is not a directory
        os_mock.path.isdir.return_value = False
        tm = GoodTransferMixin()
        self.assertEqual(tm.rsync_download_directory(
                         local_path='',
                         ssh_key='my ssh key',
                         ssh_user='my ssh user',
                         remote_host='remote host',
                         remote_path='remote path',), -1)

    @mock.patch('mozharness.base.transfer.os')
    def test_rsync_download(self, os_mock):
        # successful rsync
        os_mock.path.isdir.return_value = True
        tm = GoodTransferMixin()
        self.assertEqual(tm.rsync_download_directory(
                         local_path='',
                         ssh_key='my ssh key',
                         ssh_user='my ssh user',
                         remote_host='remote host',
                         remote_path='remote path',), None)

    @mock.patch('mozharness.base.transfer.os')
    def test_rsync_download_fail(self, os_mock):
        # ops download has failed
        os_mock.path.isdir.return_value = True
        tm = BadTransferMixin()
        self.assertEqual(tm.rsync_download_directory(
                         local_path='',
                         ssh_key='my ssh key',
                         ssh_user='my ssh user',
                         remote_host='remote host',
                         remote_path='remote path',), -3)


if __name__ == '__main__':
    unittest.main()
