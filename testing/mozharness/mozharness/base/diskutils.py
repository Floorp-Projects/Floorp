"""Disk utility module, no mixins here!

    examples:
    1) get disk size
    from mozharness.base.diskutils import DiskInfo, DiskutilsError
    ...
    try:
        DiskSize().get_size(path='/', unit='Mb')
    except DiskutilsError as e:
        # manage the exception e.g: log.error(e)
        pass
    log.info("%s" % di)


    2) convert disk size:
    from mozharness.base.diskutils import DiskutilsError, convert_to
    ...
    file_size = <function that gets file size in bytes>
    # convert file_size to GB
    try:
        file_size = convert_to(file_size, from_unit='bytes', to_unit='GB')
    except DiskutilsError as e:
        # manage the exception e.g: log.error(e)
        pass

"""
import ctypes
import os
import sys
import logging
from mozharness.base.log import INFO, numeric_log_level

# use mozharness log
log = logging.getLogger(__name__)


class DiskutilsError(Exception):
    """Exception thrown by Diskutils module"""
    pass


def convert_to(size, from_unit, to_unit):
    """Helper method to convert filesystem sizes to kB/ MB/ GB/ TB/
       valid values for source_format and destination format are:
           * bytes
           * kB
           * MB
           * GB
           * TB
        returns: size converted from source_format to destination_format.
    """
    sizes = {'bytes': 1,
             'kB': 1024,
             'MB': 1024 * 1024,
             'GB': 1024 * 1024 * 1024,
             'TB': 1024 * 1024 * 1024 * 1024}
    try:
        df = sizes[to_unit]
        sf = sizes[from_unit]
        return size * sf / df
    except KeyError:
        raise DiskutilsError('conversion error: Invalid source or destination format')
    except TypeError:
        raise DiskutilsError('conversion error: size (%s) is not a number' % size)


class DiskInfo(object):
    """Stores basic information about the disk"""
    def __init__(self):
        self.unit = 'bytes'
        self.free = 0
        self.used = 0
        self.total = 0

    def __str__(self):
        string = ['Disk space info (in %s)' % self.unit]
        string += ['total: %s' % self.total]
        string += ['used: %s' % self.used]
        string += ['free: %s' % self.free]
        return " ".join(string)

    def _to(self, unit):
        from_unit = self.unit
        to_unit = unit
        self.free = convert_to(self.free, from_unit=from_unit, to_unit=to_unit)
        self.used = convert_to(self.used, from_unit=from_unit, to_unit=to_unit)
        self.total = convert_to(self.total, from_unit=from_unit, to_unit=to_unit)
        self.unit = unit


class DiskSize(object):
    """DiskSize object
    """
    @staticmethod
    def _posix_size(path):
        """returns the disk size in bytes
           disk size is relative to path
        """
        # we are on a POSIX system
        st = os.statvfs(path)
        disk_info = DiskInfo()
        disk_info.free = st.f_bavail * st.f_frsize
        disk_info.used = (st.f_blocks - st.f_bfree) * st.f_frsize
        disk_info.total = st.f_blocks * st.f_frsize
        return disk_info

    @staticmethod
    def _windows_size(path):
        """returns size in bytes, works only on windows platforms"""
        # we're on a non POSIX system (windows)
        # DLL call
        disk_info = DiskInfo()
        dummy = ctypes.c_ulonglong()  # needed by the dll call but not used
        total = ctypes.c_ulonglong()  # stores the total space value
        free = ctypes.c_ulonglong()   # stores the free space value
        # depending on path format (unicode or not) and python version (2 or 3)
        # we need to call GetDiskFreeSpaceExW or GetDiskFreeSpaceExA
        called_function = ctypes.windll.kernel32.GetDiskFreeSpaceExA
        if isinstance(path, unicode) or sys.version_info >= (3,):
            called_function = ctypes.windll.kernel32.GetDiskFreeSpaceExW
        # we're ready for the dll call. On error it returns 0
        if called_function(path,
                           ctypes.byref(dummy),
                           ctypes.byref(total),
                           ctypes.byref(free)) != 0:
            # success, we can use the values returned by the dll call
            disk_info.free = free.value
            disk_info.total = total.value
            disk_info.used = total.value - free.value
        return disk_info

    @staticmethod
    def get_size(path, unit, log_level=INFO):
        """Disk info stats:
                total => size of the disk
                used  => space used
                free  => free space
          In case of error raises a DiskutilError Exception
        """
        try:
            # let's try to get the disk size using os module
            disk_info = DiskSize()._posix_size(path)
        except AttributeError:
            try:
                # os module failed. let's try to get the size using
                # ctypes.windll...
                disk_info = DiskSize()._windows_size(path)
            except AttributeError:
                # No luck! This is not a posix nor window platform
                # raise an exception
                raise DiskutilsError('Unsupported platform')

        disk_info._to(unit)
        lvl = numeric_log_level(log_level)
        log.log(lvl, msg="%s" % disk_info)
        return disk_info
