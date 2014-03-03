:mod:`mozlog` --- Easy, configurable and uniform logging
========================================================

Mozlog is a python package intended to simplify and standardize logs
in the Mozilla universe. It wraps around python's logging module and
adds some additional functionality.

.. note::
  For the purposes of logging results and other output from test runs,
  :doc:`mozlog.structured<mozlog_structured>` should now be used
  instead of this module.

.. automodule:: mozlog
    :members: getLogger

.. autoclass:: MozLogger
    :members: testStart, testEnd, testPass, testFail, testKnownFail

Examples
--------

Log to stdout::

    import mozlog
    log = mozlog.getLogger('MODULE_NAME')
    log.setLevel(mozlog.INFO)
    log.info('This message will be printed to stdout')
    log.debug('This won't')
    log.testPass('A test has passed')
    mozlog.shutdown()

Log to a file::

    import mozlog
    log = mozlog.getLogger('MODULE_NAME', handler=mozlog.FileHandler('path/to/log/file'))
    log.warning('Careful!')
    log.testKnownFail('We know the cause for this failure')
    mozlog.shutdown()

Log from an existing object using the LoggingMixin::

    import mozlog
    class Loggable(mozlog.LoggingMixin):
        """Trivial class inheriting from LoggingMixin"""
        def say_hello(self):
            self.info("hello")

    loggable = Loggable()
    loggable.say_hello()


.. _logging: http://docs.python.org/library/logging.html
