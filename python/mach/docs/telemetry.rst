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

#. Parameterize your ``@Command`` annotation with ``metrics_path``.
#. Use the ``command_context.metrics`` handle provided by ``MachCommandBase``

For example::

    METRICS_PATH = os.path.abspath(os.path.join(__file__, '..', '..', 'metrics.yaml'))

    @Command('custom-command', metrics_path=METRICS_PATH)
    def custom_command(command_context):
        command_context.metrics.custom.foo.set('bar')

Updating Generated Metrics Docs
===============================

When a ``metrics.yaml`` is added/changed/removed, :ref:`the metrics document<metrics>` will need to be updated::

    ./mach doc mach-telemetry
