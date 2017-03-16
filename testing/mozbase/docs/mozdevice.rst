:mod:`mozdevice` --- Interact with remote devices
=================================================

Mozdevice provides several interfaces to interact with a remote device
such as an Android- or FirefoxOS-based phone. It allows you to push
files to these types of devices, launch processes, and more. There are
currently two available interfaces:

* :ref:`DeviceManager`: Works either via ADB or a custom TCP protocol
  (the latter requires an agent application running on the device).
* :ref:`ADB`: Uses the Android Debugger Protocol explicitly

In general, new code should use the ADB abstraction where possible as
it is simpler and more reliable.

.. automodule:: mozdevice

.. _DeviceManager:

DeviceManager interface
-----------------------
.. autoclass:: DeviceManager

Here's an example script which lists the files in '/mnt/sdcard' and sees if a
process called 'org.mozilla.fennec' is running. In this example, we're
instantiating the DeviceManagerADB implementation, but we could just
as easily have used DeviceManagerSUT (assuming the device had an agent
running speaking the SUT protocol).

::

  import mozdevice

  dm = mozdevice.DeviceManagerADB()
  print dm.listFiles("/mnt/sdcard")
  if dm.processExist("org.mozilla.fennec"):
      print "Fennec is running"

Informational methods
`````````````````````
.. automethod:: DeviceManager.getInfo(self, directive=None)
.. automethod:: DeviceManager.getCurrentTime(self)
.. automethod:: DeviceManager.getIP
.. automethod:: DeviceManager.saveScreenshot
.. automethod:: DeviceManager.recordLogcat
.. automethod:: DeviceManager.getLogcat

File management methods
```````````````````````
.. autoattribute:: DeviceManager.deviceRoot
.. automethod:: DeviceManager.getDeviceRoot(self)
.. automethod:: DeviceManager.pushFile(self, localFilename, remoteFilename, retryLimit=1)
.. automethod:: DeviceManager.pushDir(self, localDirname, remoteDirname, retryLimit=1)
.. automethod:: DeviceManager.pullFile(self, remoteFilename)
.. automethod:: DeviceManager.getFile(self, remoteFilename, localFilename)
.. automethod:: DeviceManager.getDirectory(self, remoteDirname, localDirname, checkDir=True)
.. automethod:: DeviceManager.validateFile(self, remoteFilename, localFilename)
.. automethod:: DeviceManager.mkDir(self, remoteDirname)
.. automethod:: DeviceManager.mkDirs(self, filename)
.. automethod:: DeviceManager.dirExists(self, dirpath)
.. automethod:: DeviceManager.fileExists(self, filepath)
.. automethod:: DeviceManager.listFiles(self, rootdir)
.. automethod:: DeviceManager.removeFile(self, filename)
.. automethod:: DeviceManager.removeDir(self, remoteDirname)
.. automethod:: DeviceManager.chmodDir(self, remoteDirname, mask="777")
.. automethod:: DeviceManager.getTempDir(self)

Process management methods
``````````````````````````
.. automethod:: DeviceManager.shell(self, cmd, outputfile, env=None, cwd=None, timeout=None, root=False)
.. automethod:: DeviceManager.shellCheckOutput(self, cmd, env=None, cwd=None, timeout=None, root=False)
.. automethod:: DeviceManager.getProcessList(self)
.. automethod:: DeviceManager.processExist(self, processName)
.. automethod:: DeviceManager.killProcess(self, processName)

System control methods
``````````````````````
.. automethod:: DeviceManager.reboot(self, ipAddr=None, port=30000)

Application management methods
``````````````````````````````
.. automethod:: DeviceManager.uninstallAppAndReboot(self, appName, installPath=None)
.. automethod:: DeviceManager.installApp(self, appBundlePath, destPath=None)
.. automethod:: DeviceManager.uninstallApp(self, appName, installPath=None)
.. automethod:: DeviceManager.updateApp(self, appBundlePath, processName=None, destPath=None, ipAddr=None, port=30000)

DeviceManagerADB implementation
```````````````````````````````

.. autoclass:: mozdevice.DeviceManagerADB

DeviceManagerADB has several methods that are not present in all
DeviceManager implementations. Please do not use them in code that
is meant to be interoperable.

.. automethod:: DeviceManagerADB.forward
.. automethod:: DeviceManagerADB.remount
.. automethod:: DeviceManagerADB.devices

DeviceManagerSUT implementation
```````````````````````````````

.. autoclass:: mozdevice.DeviceManagerSUT

DeviceManagerSUT has several methods that are only used in specific
tests and are not present in all DeviceManager implementations. Please
do not use them in code that is meant to be interoperable.

.. automethod:: DeviceManagerSUT.unpackFile
.. automethod:: DeviceManagerSUT.adjustResolution

Android extensions
``````````````````

For Android, we provide two variants of the `DeviceManager` interface
with extensions useful for that platform. These classes are called
DroidADB and DroidSUT. They inherit all methods from DeviceManagerADB
and DeviceManagerSUT. Here is the interface for DroidADB:

.. automethod:: mozdevice.DroidADB.launchApplication
.. automethod:: mozdevice.DroidADB.launchFennec
.. automethod:: mozdevice.DroidADB.getInstalledApps
.. automethod:: mozdevice.DroidADB.getAppRoot

These methods are also found in the DroidSUT class.

.. _ADB:

ADB Interface
-------------

The following classes provide a basic interface to interact with the
Android Debug Tool (adb) and Android-based devices.  It is intended to
provide the basis for a replacement for DeviceManager and
DeviceManagerADB.

ADBCommand
``````````

.. autoclass:: mozdevice.ADBCommand

.. automethod:: ADBCommand.command(self, cmds, timeout=None)
.. automethod:: ADBCommand.command_output(self, cmds, timeout=None)

ADBHost
```````
.. autoclass:: mozdevice.ADBHost

.. automethod:: ADBHost.command(self, cmds, timeout=None)
.. automethod:: ADBHost.command_output(self, cmds, timeout=None)
.. automethod:: ADBHost.start_server(self, timeout=None)
.. automethod:: ADBHost.kill_server(self, timeout=None)
.. automethod:: ADBHost.devices(self, timeout=None)

ADBDevice
`````````
.. autoclass:: mozdevice.ADBDevice

Host Command methods
++++++++++++++++++++
.. automethod:: ADBDevice.command(self, cmds, timeout=None)
.. automethod:: ADBDevice.command_output(self, cmds, timeout=None)

Device Shell methods
++++++++++++++++++++
.. automethod:: ADBDevice.shell(self, cmd, env=None, cwd=None, timeout=None, root=False)
.. automethod:: ADBDevice.shell_bool(self, cmd, env=None, cwd=None, timeout=None, root=False)
.. automethod:: ADBDevice.shell_output(self, cmd, env=None, cwd=None, timeout=None, root=False)

Informational methods
+++++++++++++++++++++
.. automethod:: ADBDevice.clear_logcat
.. automethod:: ADBDevice.get_battery_percentage
.. automethod:: ADBDevice.get_info
.. automethod:: ADBDevice.get_logcat
.. automethod:: ADBDevice.get_prop
.. automethod:: ADBDevice.get_state

System control methods
++++++++++++++++++++++
.. automethod:: ADBDevice.is_device_ready
.. automethod:: ADBDevice.reboot

File management methods
+++++++++++++++++++++++
.. automethod:: ADBDevice.chmod
.. automethod:: ADBDevice.cp
.. automethod:: ADBDevice.exists
.. automethod:: ADBDevice.is_dir
.. automethod:: ADBDevice.is_file
.. automethod:: ADBDevice.list_files
.. automethod:: ADBDevice.mkdir
.. automethod:: ADBDevice.mv
.. automethod:: ADBDevice.push
.. automethod:: ADBDevice.rm
.. automethod:: ADBDevice.rmdir
.. autoattribute:: ADBDevice.test_root

Process management methods
++++++++++++++++++++++++++
.. automethod:: ADBDevice.get_process_list
.. automethod:: ADBDevice.kill
.. automethod:: ADBDevice.pkill
.. automethod:: ADBDevice.process_exist

ADBAndroid
``````````
.. autoclass:: ADBAndroid

Informational methods
+++++++++++++++++++++
.. automethod:: ADBAndroid.get_battery_percentage

System control methods
++++++++++++++++++++++
.. automethod:: ADBAndroid.is_device_ready
.. automethod:: ADBAndroid.power_on

Application management methods
++++++++++++++++++++++++++++++
.. automethod:: ADBAndroid.install_app
.. automethod:: ADBAndroid.is_app_installed
.. automethod:: ADBAndroid.launch_application
.. automethod:: ADBAndroid.launch_fennec
.. automethod:: ADBAndroid.stop_application
.. automethod:: ADBAndroid.uninstall_app
.. automethod:: ADBAndroid.update_app

ADBB2G
``````
.. autoclass:: ADBB2G

Informational methods
+++++++++++++++++++++
.. automethod:: ADBB2G.get_battery_percentage
.. automethod:: ADBB2G.get_info
.. automethod:: ADBB2G.get_memory_total

ADBProcess
``````````
.. autoclass:: mozdevice.ADBProcess

ADBError
````````
.. autoexception:: mozdevice.ADBError

ADBRootError
````````````
.. autoexception:: mozdevice.ADBRootError

ADBTimeoutError
```````````````
.. autoexception:: mozdevice.ADBTimeoutError

