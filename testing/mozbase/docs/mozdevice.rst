:mod:`mozdevice` --- Interact with remote devices
=================================================

Mozdevice provides an interface to interact with a remote device such
as an Android- or FirefoxOS-based phone connected to a
host machine. Currently there are two implementations of the interface: one
uses a custom TCP-based protocol to communicate with a server running
on the device, another uses Android's adb utility.

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
