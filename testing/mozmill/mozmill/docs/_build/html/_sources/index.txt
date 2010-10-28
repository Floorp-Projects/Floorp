:mod:`mozmill` --- Full automation of XULRunner applications.
=============================================================

.. module:: mozmill
   :synopsis: Full automation of XULRunner applications.
.. moduleauthor:: Mikeal Rogers <mikeal.rogers@gmail.com>
.. sectionauthor:: Mikeal Rogers <mikeal.rogers@gmail.com>

Command Line Usage
------------------

The mozmill command line is versatile and includes a fair amount of debugging options. Even though all these options are available mozmill should run by default without any arguments and find your locally installed Firefox and run with mozmill.

In most modes, ctrl-c will shut down Firefox and exit out of the mozmill Python side as well.

.. code-block:: none
      
      $ mozmill

.. cmdoption:: -h, --help

   Show help message.

.. cmdoption:: -b <binary>, --binary <binary>

   Specify application binary location.
   
   Default :class:`mozrunner.Profile` and :class:`mozrunner.Runner` are still 
   :class:`mozrunner.FirefoxProfile` and :class:`mozrunner.FirefoxRunner`. You can
   change this by creating your own command line utility by subclassing :class:`CLI`
   
.. cmdoption:: -d <defaultprofile>

   Specify the path to the default **clean** profile used to create new profiles.

.. cmdoption:: -n, --no-new-profile

   Do not create a new fresh profile.
   
.. cmdoption:: -p <profile>, --profile <profile>

   Specifies a profile to use. Must be used with --no-new-profile.

.. cmdoption:: -w <plugins>, --plugins <plugins>

   Comma seperated list of additional paths to plugins to install.

   Plugins can be either .xpi zip compressed extensions or deflated extension directories.

.. cmdoption:: -l <logfile>, --logfile <logfile>

   Log all events to *logfile*.

.. cmdoption:: --report <uri>

   *Currently in development.*

   POST results to given brasstacks results server at *uri*. 

.. cmdoption:: -t <test>, --test <test>

   Run *test*. Can be either single test file or directory of tests.

.. cmdoption::  --showall

   Show all test output.

.. cmdoption:: -D, --debug

   Install debugging extensions and run with -jsconole

.. cmdoption:: --show-errors

   Print all logger errors to the console. When running tests only test failures and skipped 
   tests are printed, this option print all other errors.

.. cmdoption:: -s, --shell

   Starts a Python shell for debugging.

.. cmdoption:: -u, --usecode

   By default --shell mode will use iPython if install and fall back to using the code module.
   This option forces the use of the code module instead of iPython even when installed.

.. cmdoption:: -P <port>, --port <port>

   Specify port for jsbridge.

Command Line Class
------------------

.. class:: CLI

   Inherits from :class:`jsbridge.CLI` which inherits from :class:`mozrunner.CLI`.
   
   All the heavy lifting is handled by jsbridge and mozrunner. If you are subclassing
   this in order to creat a new command line interface be sure to call :func:`super` on all
   related methods.
   
   .. attribute:: runner_class
   
      Default runner class. Should be subclass of :class:`mozrunner.Runner`.
      Defaults to :class:`mozrunner.FirefoxRunner`. 

   .. attribute:: profile_class
   
      Default profile class. Should be subclass of :class:`mozruner.Profile`.
      Defaults to :class:`mozrunner.FirefoxProfile`.

Running MozMill from Python
---------------------------

.. class:: MozMill([runner_class[, profile_class[, jsbridge_port]]])

   Manages an instance of Firefox w/ jsbridge and provides facilities for running tests and
   keeping track of results with callback methods.
   
   Default *runner_class* is :class:`mozrunner.FirefoxRunner`. Value should be a subclass of 
   :class:`mozrunner.Runner`.
   
   Default *profile_class* is :class:`mozrunner.FirefoxProfile`. Value should be a subclass of 
   :class:`mozrunner.Profile`.
   
   Default *jsbridge_port* is `24242`.
   
   .. attribute:: runner_class
   
      Set during initialization to subclass of :class:`mozrunner.Runner`.
      
   .. attribute:: profile_class
   
      Set during initialization to subclass of :class:`mozrunner.Profile`.
   
   .. attribute:: jsbridge_port
   
      Set during initialization to :class:`numbers.Integral`.
   
   .. method:: start([profile[, runner]])
   
      Start mozrunner and jsbridge pre-requisites.
   
      *profile* should be an instance of a `mozrunner.Profile` subclass. If one is not passed 
      an instance of `self.profile_class` is created. `self.profile` will be set to this 
      value.
      
      *runner* should be an instance of a `mozrunner.Runner` subclass. If one is not passed an 
      instance of :attr:`runner_class` will be created. :attr:`runner` will be set to this value.
      
      This method will also run `runner.start()` and :func:`mozrunner.wait_and_create_network`
      and sets :attr:`back_channel` and :attr:`bridge` to instances of 
      :class:`jsbridge.BackChannel` and :class:`jsbridge.Bridge` respectively.
      
   .. attribute:: profile
   
      Set during :meth:`start` to subclass of :class:`mozrunner.Profile`.
      
   .. attribute:: runner
   
      Set during :meth:`start` to subclass of :class:`mozrunner.Runner`.
      
   .. attribute:: back_channel
   
      Set during :meth:`start` to subclass of :class:`jsbridge.BackChannel`.
      
   .. attribute:: bridge
   
      Set during :meth:`start` to subclass of :class:`jsbridge.Bridge`

   .. method:: run_tests(test[, report])
   
      Run *test* in live Firefox using :attr:`bridge`.
      
      Adds local listeners :meth:`endTest_listener` and :meth:`endRunner_listener` to 
      `"endTest"` and `"endRunner"` events using :meth:`jsbridge.BackChannel.add_listener` of
      :attr:`back_channel`.
      
      When tests are done the results are posted to a results server at *report* if passed.
      
   .. method:: endTest_listener(test)
   
      When a test is finished the test object will be passed to this callback.
      
   .. method:: endRunner_listener(obj)
   
      When all the tests are done running this callback will be fired.
   
   