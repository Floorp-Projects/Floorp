:mod:`mozdevice` --- Interact with remote devices
=================================================

Mozdevice provides several interfaces to interact with a remote device
such as an Android- or FirefoxOS-based phone. It allows you to push
files to these types of devices, launch processes, and more. There are
currently two available interfaces:

* DeviceManager: an interface to a device that works either via ADB or
  a custom TCP protocol (the latter requires an agent application
  running on the device).
* ADB: a similar interface that uses Android Debugger Protocol
  explicitly

In general, new code should use the ADB abstraction where possible as
it is simpler and more reliable.

.. automodule:: mozdevice

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
.. automethod:: DeviceManager.getDeviceRoot(self)
.. automethod:: DeviceManager.getAppRoot(self, packageName=None)
.. automethod:: DeviceManager.getTestRoot(self, harnessName)
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
-------------------------------

.. autoclass:: mozdevice.DeviceManagerADB

ADB-specific methods
````````````````````
DeviceManagerADB has several methods that are not present in all
DeviceManager implementations. Please do not use them in code that
is meant to be interoperable.

.. automethod:: DeviceManagerADB.forward
.. automethod:: DeviceManagerADB.remount
.. automethod:: DeviceManagerADB.devices

DeviceManagerSUT implementation
-------------------------------

.. autoclass:: mozdevice.DeviceManagerSUT

SUT-specific methods
````````````````````
DeviceManagerSUT has several methods that are only used in specific
tests and are not present in all DeviceManager implementations. Please
do not use them in code that is meant to be interoperable.

.. automethod:: DeviceManagerSUT.unpackFile
.. automethod:: DeviceManagerSUT.adjustResolution

Android extensions
------------------

For Android, we provide two variants of the `DeviceManager` interface
with extensions useful for that platform. These classes are called
DroidADB and DroidSUT. They inherit all methods from DeviceManagerADB
and DeviceManagerSUT. Here is the interface for DroidADB:

.. automethod:: mozdevice.DroidADB.launchApplication
.. automethod:: mozdevice.DroidADB.launchFennec
.. automethod:: mozdevice.DroidADB.getInstalledApps

These methods are also found in the DroidSUT class.

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
.. automethod:: ADBHost.kill_server(self, cmds, timeout=None)
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
.. automethod:: ADBDevice.clear_logcat(self, timeout=None)
.. automethod:: ADBDevice.get_logcat(self, filterSpecs=["dalvikvm:I", "ConnectivityService:S", "WifiMonitor:S", "WifiStateTracker:S", "wpa_supplicant:S", "NetworkStateTracker:S"], format="time", filter_out_regexps=[], timeout=None)
.. automethod:: ADBDevice.get_prop(self, prop, timeout=None)
.. automethod:: ADBDevice.get_state(self, timeout=None)

File management methods
+++++++++++++++++++++++
.. automethod:: ADBDevice.chmod(self, path, recursive=False, mask="777", timeout=None, root=False)
.. automethod:: ADBDevice.exists(self, path, timeout=None, root=False)
.. automethod:: ADBDevice.is_dir(self, path, timeout=None, root=False)
.. automethod:: ADBDevice.is_file(self, path, timeout=None, root=False)
.. automethod:: ADBDevice.list_files(self, path, timeout=None, root=False)
.. automethod:: ADBDevice.mkdir(self, path, parents=False, timeout=None, root=False)
.. automethod:: ADBDevice.push(self, local, remote, timeout=None)
.. automethod:: ADBDevice.rm(self, path, recursive=False, force=False, timeout=None, root=False)
.. automethod:: ADBDevice.rmdir(self, path, timeout=None, root=False)

Process management methods
++++++++++++++++++++++++++
.. automethod:: ADBDevice.get_process_list(self, timeout=None)
.. automethod:: ADBDevice.kill(self, pids, sig=None,   attempts=3, wait=5, timeout=None, root=False)
.. automethod:: ADBDevice.pkill(self, appname, sig=None,   attempts=3, wait=5, timeout=None, root=False)
.. automethod:: ADBDevice.process_exist(self, process_name, timeout=None)


ADBAndroid
``````````
.. autoclass:: ADBAndroid

System control methods
++++++++++++++++++++++
.. automethod:: ADBAndroid.is_device_ready(self, timeout=None)
.. automethod:: ADBAndroid.power_on(self, timeout=None)
.. automethod:: ADBAndroid.reboot(self, timeout=None)

Application management methods
++++++++++++++++++++++++++++++
.. automethod:: ADBAndroid.install_app(self, apk_path, timeout=None)
.. automethod:: ADBAndroid.is_app_installed(self, app_name, timeout=None)
.. automethod:: ADBAndroid.launch_application(self, app_name, activity_name, intent, url=None, extras=None, wait=True, fail_if_running=True, timeout=None)
.. automethod:: ADBAndroid.launch_fennec(self, app_name, intent="android.intent.action.VIEW", moz_env=None, extra_args=None, url=None, wait=True, fail_if_running=True, timeout=None)
.. automethod:: ADBAndroid.stop_application(self, app_name, timeout=None, root=False)
.. automethod:: ADBAndroid.uninstall_app(self, app_name, reboot=False, timeout=None)
.. automethod:: ADBAndroid.update_app(self, apk_path, timeout=None)


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

