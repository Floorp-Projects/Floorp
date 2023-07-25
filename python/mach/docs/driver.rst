.. _mach_driver:

=======
Drivers
=======

Entry Points
============

It is possible to use setuptools' entry points to load commands
directly from python packages. A mach entry point is a function which
returns a list of files or directories containing mach command
providers. e.g.:

.. code-block:: python

   def list_providers():
       providers = []
       here = os.path.abspath(os.path.dirname(__file__))
       for p in os.listdir(here):
           if p.endswith('.py'):
               providers.append(os.path.join(here, p))
       return providers

See http://pythonhosted.org/setuptools/setuptools.html#dynamic-discovery-of-services-and-plugins
for more information on creating an entry point. To search for entry
point plugins, you can call
:py:meth:`mach.command_util.load_commands_from_entry_point`. e.g.:

.. code-block:: python

   load_commands_from_entry_point("mach.external.providers")
