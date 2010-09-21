from ctypes import c_void_p, POINTER, sizeof, Structure, windll, WinError, WINFUNCTYPE, addressof
from ctypes.wintypes import BOOL, BYTE, DWORD, HANDLE, LARGE_INTEGER

LPVOID = c_void_p
LPDWORD = POINTER(DWORD)

# A ULONGLONG is a 64-bit unsigned integer.
# Thus there are 8 bytes in a ULONGLONG.
ULONGLONG = BYTE * 8

class IO_COUNTERS(Structure):
    # The IO_COUNTERS struct is 6 ULONGLONGs.
    # TODO: Replace with non-dummy fields.
    _fields_ = [('dummy', ULONGLONG * 6)]

class JOBOBJECT_BASIC_ACCOUNTING_INFORMATION(Structure):
    _fields_ = [('TotalUserTime', LARGE_INTEGER),
                ('TotalKernelTime', LARGE_INTEGER),
                ('ThisPeriodTotalUserTime', LARGE_INTEGER),
                ('ThisPeriodTotalKernelTime', LARGE_INTEGER),
                ('TotalPageFaultCount', DWORD),
                ('TotalProcesses', DWORD),
                ('ActiveProcesses', DWORD),
                ('TotalTerminatedProcesses', DWORD)]

class JOBOBJECT_BASIC_AND_IO_ACCOUNTING_INFORMATION(Structure):
    _fields_ = [('BasicInfo', JOBOBJECT_BASIC_ACCOUNTING_INFORMATION),
                ('IoInfo', IO_COUNTERS)]

JobObjectBasicAndIoAccountingInformation = 8

QueryInformationJobObjectProto = WINFUNCTYPE(
    BOOL,        # Return type
    HANDLE,      # hJob
    DWORD,       # JobObjectInfoClass
    LPVOID,      # lpJobObjectInfo
    DWORD,       # cbJobObjectInfoLength
    LPDWORD      # lpReturnLength
    )

QueryInformationJobObjectFlags = (
    (1, 'hJob'),
    (1, 'JobObjectInfoClass'),
    (1, 'lpJobObjectInfo'),
    (1, 'cbJobObjectInfoLength'),
    (1, 'lpReturnLength', None)
    )

_QueryInformationJobObject = QueryInformationJobObjectProto(
    ('QueryInformationJobObject', windll.kernel32),
    QueryInformationJobObjectFlags
    )

class SubscriptableReadOnlyStruct(object):
    def __init__(self, struct):
        self._struct = struct

    def _delegate(self, name):
        result = getattr(self._struct, name)
        if isinstance(result, Structure):
            return SubscriptableReadOnlyStruct(result)
        return result

    def __getitem__(self, name):
        match = [fname for fname, ftype in self._struct._fields_
                 if fname == name]
        if match:
            return self._delegate(name)
        raise KeyError(name)

    def __getattr__(self, name):
        return self._delegate(name)

def QueryInformationJobObject(hJob, JobObjectInfoClass):
    if JobObjectInfoClass != 8:
        raise NotImplementedError()
    info = JOBOBJECT_BASIC_AND_IO_ACCOUNTING_INFORMATION()
    result = _QueryInformationJobObject(
        hJob=hJob,
        JobObjectInfoClass=JobObjectInfoClass,
        lpJobObjectInfo=addressof(info),
        cbJobObjectInfoLength=sizeof(info)
        )
    if result == 0:
        raise WinError()
    return SubscriptableReadOnlyStruct(info)

def test_qijo():
    from killableprocess import Popen

    popen = Popen('c:\\windows\\notepad.exe')

    try:
        result = QueryInformationJobObject(0, 8)
        raise AssertionError('throw should occur')
    except WindowsError, e:
        pass

    try:
        result = QueryInformationJobObject(0, 1)
        raise AssertionError('throw should occur')
    except NotImplementedError, e:
        pass

    result = QueryInformationJobObject(popen._job, 8)
    if result['BasicInfo']['ActiveProcesses'] != 1:
        raise AssertionError('expected ActiveProcesses to be 1')
    popen.kill()

    result = QueryInformationJobObject(popen._job, 8)
    if result.BasicInfo.ActiveProcesses != 0:
        raise AssertionError('expected ActiveProcesses to be 0')

if __name__ == '__main__':
    print "testing."
    test_qijo()
    print "success!"
