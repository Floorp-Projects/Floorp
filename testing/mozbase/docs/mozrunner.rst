:mod:`mozrunner` --- Manage remote and local gecko processes
============================================================

Mozrunner provides an API to manage a gecko-based application with an
arbitrary configuration profile. It currently supports local desktop
binaries such as Firefox and Thunderbird, as well as Firefox OS on
mobile devices and emulators.


Basic usage
-----------

The simplest way to use mozrunner, is to instantiate a runner, start it
and then wait for it to finish:

.. code-block:: python

    from mozrunner import FirefoxRunner
    binary = 'path/to/firefox/binary'
    runner = FirefoxRunner(binary=binary)
    runner.start()
    runner.wait()

This automatically creates and uses a default mozprofile object. If you
wish to use a specialized or pre-existing profile, you can create a
:doc:`mozprofile <mozprofile>` object and pass it in:

.. code-block:: python

    from mozprofile import FirefoxProfile
    from mozrunner import FirefoxRunner
    import os

    binary = 'path/to/firefox/binary'
    profile_path = 'path/to/profile'
    if os.path.exists(profile_path):
        profile = FirefoxProfile.clone(path_from=profile_path)
    else:
        profile = FirefoxProfile(profile=profile_path)
    runner = FirefoxRunner(binary=binary, profile=profile)
    runner.start()
    runner.wait()


Handling output
---------------

By default, mozrunner dumps the output of the gecko process to standard output.
It is possible to add arbitrary output handlers by passing them in via the
`process_args` argument. Be careful, passing in a handler overrides the default
behaviour. So if you want to use a handler in addition to dumping to stdout, you
need to specify that explicitly. For example:

.. code-block:: python

    from mozrunner import FirefoxRunner

    def handle_output_line(line):
        do_something(line)

    binary = 'path/to/firefox/binary'
    process_args = { 'stream': sys.stdout,
                     'processOutputLine': [handle_output_line] }
    runner = FirefoxRunner(binary=binary, process_args=process_args)

Mozrunner uses :doc:`mozprocess <mozprocess>` to manage the underlying gecko
process and handle output. See the :doc:`mozprocess documentation <mozprocess>`
for all available arguments accepted by `process_args`.


Handling timeouts
-----------------

Sometimes gecko can hang, or maybe it is just taking too long. To handle this case you
may want to set a timeout. Mozrunner has two kinds of timeouts, the
traditional `timeout`, and the `outputTimeout`. These get passed into the
`runner.start()` method. Setting `timeout` will cause gecko to be killed after
the specified number of seconds, no matter what. Setting `outputTimeout` will cause
gecko to be killed after the specified number of seconds with no output. In both
cases the process handler's `onTimeout` callbacks will be triggered.

.. code-block:: python

    from mozrunner import FirefoxRunner

    def on_timeout():
        print('timed out after 10 seconds with no output!')

    binary = 'path/to/firefox/binary'
    process_args = { 'onTimeout': on_timeout }
    runner = FirefoxRunner(binary=binary, process_args=process_args)
    runner.start(outputTimeout=10)
    runner.wait()

The `runner.wait()` method also accepts a timeout argument. But unlike the arguments
to `runner.start()`, this one simply returns from the wait call and does not kill the
gecko process.

.. code-block:: python

    runner.start(timeout=100)

    waiting = 0
    while runner.wait(timeout=1) is None:
        waiting += 1
        print("Been waiting for %d seconds so far.." % waiting)
    assert waiting <= 100


Using a device runner
---------------------

The previous examples used a GeckoRuntimeRunner. If you want to control a
gecko process on a remote device, you need to use a DeviceRunner. The api is
nearly identical except you don't pass in a binary, instead you create a device
object. For example to run Firefox for Android on the emulator, you might do:

.. code-block:: python

    from mozrunner import FennecEmulatorRunner

    avd_home = 'path/to/avd'
    runner = FennecEmulatorRunner(app='org.mozilla.fennec', avd_home=avd_home)
    runner.start()
    runner.wait()

Device runners have a `device` object. Remember that the gecko process runs on
the device. In the case of the emulator, it is possible to start the
device independently of the gecko process.

.. code-block:: python

    runner.device.start() # launches the emulator
    runner.start()        # stops the gecko process (if started), installs the profile, (re)starts the gecko process


Runner API Documentation
------------------------

Application Runners
~~~~~~~~~~~~~~~~~~~
.. automodule:: mozrunner.runners
   :members:

BaseRunner
~~~~~~~~~~
.. autoclass:: mozrunner.base.BaseRunner
   :members:

GeckoRuntimeRunner
~~~~~~~~~~~~~~~~~~
.. autoclass:: mozrunner.base.GeckoRuntimeRunner
   :show-inheritance:
   :members:

BlinkRuntimeRunner
~~~~~~~~~~~~~~~~~~
.. autoclass:: mozrunner.base.BlinkRuntimeRunner
   :show-inheritance:
   :members:

DeviceRunner
~~~~~~~~~~~~
.. autoclass:: mozrunner.base.DeviceRunner
   :show-inheritance:
   :members:

Device API Documentation
------------------------

Generally using the device classes directly shouldn't be required, but in some
cases it may be desirable.

Device
~~~~~~
.. autoclass:: mozrunner.devices.Device
   :members:

EmulatorAVD
~~~~~~~~~~~
.. autoclass:: mozrunner.devices.EmulatorAVD
   :show-inheritance:
   :members:
