:mod:`mozprocess` --- Launch and manage processes
=================================================

Mozprocess is a process-handling module that provides some additional
features beyond those available with python's subprocess:

* better handling of child processes, especially on Windows
* the ability to timeout the process after some absolute period, or some
  period without any data written to stdout/stderr
* the ability to specify output handlers that will be called
  for each line of output produced by the process
* the ability to specify handlers that will be called on process timeout
  and normal process termination


.. module:: mozprocess
.. autoclass:: ProcessHandlerMixin
   :members: __init__, timedOut, commandline, run, kill, readWithTimeout, processOutputLine, onTimeout, onFinish, processOutput, wait
.. autoclass:: ProcessHandler
   :members:
