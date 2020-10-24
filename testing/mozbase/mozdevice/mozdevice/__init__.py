# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

"""mozdevice provides a Python interface to the Android Debug Bridge (adb) for Android Devices.

mozdevice exports the following classes:

ADBProcess is a class which is used by ADBCommand to execute commands
via subprocess.Popen.

ADBCommand is an internal only class which provides the basics of
the interfaces for connecting to a device, and executing commands
either on the host or device using ADBProcess.

ADBHost is a Python class used to execute commands which are not
necessarily associated with a specific device. It is intended to be
used directly.

ADBDevice is a Python class used to execute commands which will
interact with a specific connected Android device.

ADBAndroid inherits directly from ADBDevice and is essentially a
synonym for ADBDevice. It is included for backwards compatibility only
and should not be used in new code.

ADBDeviceFactory is a Python function used to create instances of
ADBDevice. ADBDeviceFactory is preferred over using ADBDevice to
create new instances of ADBDevice since it will only create one
instance of ADBDevice for each connected device.

mozdevice exports the following exceptions:

::

    Exception -
    |- ADBTimeoutError
    |- ADBDeviceFactoryError
    |- ADBError
       |- ADBProcessError
       |- ADBListDevicesError

ADBTimeoutError is a special exception that is not part of the
ADBError class hierarchy. It is raised when a command has failed to
complete within the specified timeout period. Since this typically is
due to a failure in the usb connection to the device and is not
recoverable, it is implemented separately from ADBError so that it
will not be caught by normal except clause handling of expected error
conditions and is considered to be treated as a *fatal* error.

ADBDeviceFactoryError is also a special exception that is not part
of the ADBError class hierarchy. It is raised by ADBDeviceFactory
when the state of the internal ADBDevices object is in an
inconsistent state and is considered to be a *fatal* error.

ADBListDevicesError is an instance of ADBError which is
raised only by the ADBHost.devices() method to signify that
``adb devices`` reports that the device state has no permissions and can
not be contacted via adb.

ADBProcessError is an instance of ADBError which is raised when a
process executed via ADBProcess has exited with a non-zero exit
code. It is raised by the ADBCommand.command method and the methods
that call it.

ADBError is a generic exception class to signify that some error
condition has occured which may be handled depending on the semantics
of the executing code.

Example:

::

    from mozdevice import ADBHost, ADBDeviceFactory, ADBError

    adbhost = ADBHost()
    try:
        adbhost.kill_server()
        adbhost.start_server()
    except ADBError as e:
        print('Unable to restart the adb server: {}'.format(str(e)))

    device = ADBDeviceFactory()
    try:
        sdcard_contents = device.ls('/sdcard/')  # List the contents of the sdcard on the device.
        print('sdcard contains {}'.format(' '.join(sdcard_contents))
    except ADBError as e:
        print('Unable to list the sdcard: {}'.format(str(e)))

Android devices use a security model based upon user permissions much
like that used in Linux upon which it is based. The adb shell executes
commands on the device as the shell user whose access to the files and
directories on the device are limited by the directory and file
permissions set in the device's file system.

Android apps run under their own user accounts and are restricted by
the app's requested permissions in terms of what commands and files
and directories they may access.

Like Linux, Android supports a root user who has unrestricted access
to the command and content stored on the device.

Most commercially released Android devices do not allow adb to run
commands as the root user. Typically, only Android emulators running
certain system images, devices which have AOSP debug or engineering
Android builds or devices which have been *rooted* can run commands as
the root user.

ADBDevice supports using both unrooted and rooted devices by laddering
its capabilities depending on the specific circumstances where it is
used.

ADBDevice uses a special location on the device, called the
*test_root*, where it places content to be tested. This can include
binary executables and libraries, configuration files and log
files. Since the special location /data/local/tmp is usually
accessible by the shell user, the test_root is located at
/data/local/tmp/test_root by default. /data/local/tmp is used instead
of the sdcard due to recent Scoped Storage restrictions on access to
the sdcard in Android 10 and later.

If the device supports running adbd as root, or if the device has been
rooted and supports the use of the su command to run commands as root,
ADBDevice will default to running all shell commands under the root
user and the test_root will remain set to /data/local/tmp/test_root
unless changed.

If the device does not support running shell commands under the root
user, and a *debuggable* app is set in ADBDevice property
run_as_package, then ADBDevice will set the test_root to
/data/data/<app-package-name>/test_root and will run shell commands as
the app user when accessing content located in the app's data
directory. Content can be pushed to the app's data directory or pulled
from the app's data directory by using the command run-as to access
the app's data.

If a device does not support running commands as root and a
*debuggable* app is not being used, command line programs can still be
executed by pushing them to the /data/local/tmp directory which is
accessible to the shell user.

If for some reason, the device is not rooted and /data/local/tmp is
not acccessible to the shell user, then ADBDevice will fail to
initialize and will not be useable for that device.

NOTE: ADBFactory will clear the contents of the test_root when it
first creates an instance of ADBDevice.

When the run_as_package property is set in an ADBDevice instance, it
will clear the contents of the current test_root before changing the
test_root to point to the new location
/data/data/<app-package-name>/test_root which will then be cleared of
any existing content.

"""

from __future__ import absolute_import

from .adb import ADBError, ADBProcessError, ADBTimeoutError
from .adb import ADBProcess, ADBCommand, ADBHost, ADBDevice, ADBDeviceFactory
from .adb_android import ADBAndroid

__all__ = ['ADBError', 'ADBProcessError', 'ADBTimeoutError',
           'ADBProcess', 'ADBCommand', 'ADBHost', 'ADBDevice', 'ADBAndroid', 'ADBDeviceFactory']
