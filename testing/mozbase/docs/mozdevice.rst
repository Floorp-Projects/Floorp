:mod:`mozdevice` --- Interact with Android devices
==================================================

Mozdevice provides several interfaces to interact with an Android
device such as a phone, tablet, or emulator. It allows you to push
files to these types of devices, launch processes, and more. There are
currently two available interfaces:

* :ref:`ADB`: Uses the Android Debugger Protocol explicitly

.. automodule:: mozdevice

.. _ADB:

ADB Interface
-------------

The following classes provide a basic interface to interact with the
Android Debug Tool (adb) and Android-based devices.  It has replaced
the now defunct DeviceManager and DeviceManagerADB classes.

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
.. automethod:: ADBDevice.get_top_activity

System control methods
++++++++++++++++++++++
.. automethod:: ADBDevice.is_device_ready
.. automethod:: ADBDevice.power_on
.. automethod:: ADBDevice.reboot

File management methods
+++++++++++++++++++++++
.. automethod:: ADBDevice.chmod
.. automethod:: ADBDevice.cp
.. automethod:: ADBDevice.exists
.. automethod:: ADBDevice.get_file
.. automethod:: ADBDevice.is_dir
.. automethod:: ADBDevice.is_file
.. automethod:: ADBDevice.list_files
.. automethod:: ADBDevice.mkdir
.. automethod:: ADBDevice.mv
.. automethod:: ADBDevice.push
.. automethod:: ADBDevice.pull
.. automethod:: ADBDevice.rm
.. automethod:: ADBDevice.rmdir
.. autoattribute:: ADBDevice.test_root

Process management methods
++++++++++++++++++++++++++
.. automethod:: ADBDevice.get_process_list
.. automethod:: ADBDevice.kill
.. automethod:: ADBDevice.pkill
.. automethod:: ADBDevice.process_exist

Application management methods
++++++++++++++++++++++++++++++
.. automethod:: ADBDevice.install_app
.. automethod:: ADBDevice.is_app_installed
.. automethod:: ADBDevice.launch_application
.. automethod:: ADBDevice.launch_fennec
.. automethod:: ADBDevice.launch_geckoview_example
.. automethod:: ADBDevice.stop_application
.. automethod:: ADBDevice.uninstall_app
.. automethod:: ADBDevice.update_app

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

