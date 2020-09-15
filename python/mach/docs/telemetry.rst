.. _mach_telemetry:

==============
Mach Telemetry
==============

`Glean <https://mozilla.github.io/glean/>`_ is used to collect telemetry, and uses the metrics
defined in the ``metrics.yaml`` files in-tree.
These files are all documented in a single :ref:`generated file here<metrics>`.

.. toctree::
   :maxdepth: 1

   metrics

Adding Metrics to a new Command
===============================

If you would like to submit telemetry metrics from your mach ``@Command``, you should take two steps:

#. Parameterize your class's ``@CommandProvider`` annotation with ``metrics_path``.
#. Use the ``self.metrics`` handle provided by ``MachCommandBase``

For example::

    METRICS_PATH = os.path.abspath(os.path.join(__file__, '..', '..', 'metrics.yaml'))

    @CommandProvider(metrics_path=METRICS_PATH)
    class CustomCommand(MachCommandBase):
        @Command('custom-command')
        def custom_command(self):
            self.metrics.custom.foo.set('bar')

Updating Generated Metrics Docs
===============================

When a ``metrics.yaml`` is added/changed/removed, :ref:`the metrics document<metrics>` will need to be updated::

    ./mach doc mach-telemetry
