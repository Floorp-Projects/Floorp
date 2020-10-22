import mock
import unittest
from mozharness.base.diskutils import convert_to, DiskutilsError, DiskSize, DiskInfo


class TestDiskutils(unittest.TestCase):
    def test_convert_to(self):
        # 0 is 0 regardless from_unit/to_unit
        self.assertTrue(convert_to(size=0, from_unit='GB', to_unit='MB') == 0)
        size = 524288  # 512 * 1024
        # converting from/to same unit
        self.assertTrue(convert_to(size=size, from_unit='MB', to_unit='MB') == size)

        self.assertTrue(convert_to(size=size, from_unit='MB', to_unit='GB') == 512)

        self.assertRaises(DiskutilsError,
                          lambda: convert_to(size='a string', from_unit='MB', to_unit='MB'))
        self.assertRaises(DiskutilsError,
                          lambda: convert_to(size=0, from_unit='foo', to_unit='MB'))
        self.assertRaises(DiskutilsError,
                          lambda: convert_to(size=0, from_unit='MB', to_unit='foo'))


class TestDiskInfo(unittest.TestCase):

    def testDiskinfo_to(self):
        di = DiskInfo()
        self.assertTrue(di.unit == 'bytes')
        self.assertTrue(di.free == 0)
        self.assertTrue(di.used == 0)
        self.assertTrue(di.total == 0)
        # convert to GB
        di._to('GB')
        self.assertTrue(di.unit == 'GB')
        self.assertTrue(di.free == 0)
        self.assertTrue(di.used == 0)
        self.assertTrue(di.total == 0)


class MockStatvfs(object):
    def __init__(self):
        self.f_bsize = 0
        self.f_frsize = 0
        self.f_blocks = 0
        self.f_bfree = 0
        self.f_bavail = 0
        self.f_files = 0
        self.f_ffree = 0
        self.f_favail = 0
        self.f_flag = 0
        self.f_namemax = 0


class TestDiskSpace(unittest.TestCase):

    @mock.patch('mozharness.base.diskutils.os')
    def testDiskSpacePosix(self, mock_os):
        ds = MockStatvfs()
        mock_os.statvfs.return_value = ds
        di = DiskSize()._posix_size('/')
        self.assertTrue(di.unit == 'bytes')
        self.assertTrue(di.free == 0)
        self.assertTrue(di.used == 0)
        self.assertTrue(di.total == 0)

    @mock.patch('mozharness.base.diskutils.ctypes')
    def testDiskSpaceWindows(self, mock_ctypes):
        mock_ctypes.windll.kernel32.GetDiskFreeSpaceExA.return_value = 0
        mock_ctypes.windll.kernel32.GetDiskFreeSpaceExW.return_value = 0
        di = DiskSize()._windows_size('/c/')
        self.assertTrue(di.unit == 'bytes')
        self.assertTrue(di.free == 0)
        self.assertTrue(di.used == 0)
        self.assertTrue(di.total == 0)

    @mock.patch('mozharness.base.diskutils.os')
    @mock.patch('mozharness.base.diskutils.ctypes')
    def testUnspportedPlafrom(self, mock_ctypes, mock_os):
        mock_os.statvfs.side_effect = AttributeError('')
        self.assertRaises(AttributeError, lambda: DiskSize()._posix_size('/'))
        mock_ctypes.windll.kernel32.GetDiskFreeSpaceExW.side_effect = AttributeError('')
        mock_ctypes.windll.kernel32.GetDiskFreeSpaceExA.side_effect = AttributeError('')
        self.assertRaises(AttributeError, lambda: DiskSize()._windows_size('/'))
        self.assertRaises(DiskutilsError, lambda: DiskSize().get_size(path='/', unit='GB'))
