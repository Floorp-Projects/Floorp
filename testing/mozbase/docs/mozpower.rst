:mod:`mozpower` --- Power-usage testing
=======================================

Mozpower provides an interface through which power usage measurements
can be done on any OS and CPU combination (auto-detected) that has
been implemented within the module. It provides 2 methods to start
and stop the measurement gathering as well as methods to get the
result that can also be formatted into a perfherder data blob.

Basic Usage
-----------

Although multiple classes exist within the mozpower module,
the only one that should be used is MozPower which is accessible
from the top-level of the module. It handles which subclasses
should be used depending on the detected OS and CPU combination.

.. code-block:: python

    from mozpower import MozPower

    mp = MozPower(
        ipg_measure_duration=600,
        sampling_rate=1000,
        output_file_path='tempdir/dataprefix'
    )
    mp.initialize_power_measurements()

    # Run test TEST_NAME

    mp.finalize_power_measurements(
        test_name=TEST_NAME,
        output_dir_path=env['MOZ_UPLOAD_DIR']
    )

    # Get complete PERFHERDER_DATA
    perfherder_data = mp.get_full_perfherder_data('raptor')

All the possible known errors that can occur are also provided
at the top-level of the module.

.. code-block:: python

    from mozpower import MozPower, IPGExecutableMissingError, OsCpuComboMissingError

    try:
        mp = MozPower(ipg_measure_duration=600, sampling_rate=1000)
    except IPGExecutableMissingError as e:
        pass
    except OsCpuComboMissingError as e:
        pass


.. automodule:: mozpower

.. _MozPower:

MozPower Interface
------------------

The following class provides a basic interface to interact with the
power measurement tools that have been implemented. The tool used
to measure power depends on the OS and CPU combination, i.e. Intel-based
MacOS machines would use Intel Power Gadget, while ARM64-based Windows
machines would use the native Windows tool powercfg.

MozPower
````````

.. autoclass:: mozpower.MozPower

Measurement methods
+++++++++++++++++++
.. automethod:: MozPower.initialize_power_measurements(self, **kwargs)
.. automethod:: MozPower.finalize_power_measurements(self, **kwargs)

Informational methods
+++++++++++++++++++++
.. automethod:: MozPower.get_perfherder_data(self)
.. automethod:: MozPower.get_full_perfherder_data(self, framework, lowerisbetter=True, alertthreshold=2.0)

IPGEmptyFileError
`````````````````
.. autoexception:: mozpower.IPGEmptyFileError

IPGExecutableMissingError
`````````````````````````
.. autoexception:: mozpower.IPGExecutableMissingError

IPGMissingOutputFileError
`````````````````````````
.. autoexception:: mozpower.IPGMissingOutputFileError

IPGTimeoutError
```````````````
.. autoexception:: mozpower.IPGTimeoutError

IPGUnknownValueTypeError
````````````````````````
.. autoexception:: mozpower.IPGUnknownValueTypeError

MissingProcessorInfoError
`````````````````````````
.. autoexception:: mozpower.MissingProcessorInfoError

OsCpuComboMissingError
``````````````````````
.. autoexception:: mozpower.OsCpuComboMissingError

PlatformUnsupportedError
````````````````````````
.. autoexception:: mozpower.PlatformUnsupportedError
