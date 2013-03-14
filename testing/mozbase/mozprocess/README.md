[mozprocess](https://github.com/mozilla/mozbase/tree/master/mozprocess)
provides python process management via an operating system
and platform transparent interface to Mozilla platforms of interest.
Mozprocess aims to provide the ability
to robustly terminate a process (by timeout or otherwise), along with
any child processes, on Windows, OS X, and Linux. Mozprocess utilizes
and extends `subprocess.Popen` to these ends.


# API

[mozprocess.processhandler:ProcessHandler](https://github.com/mozilla/mozbase/blob/master/mozprocess/mozprocess/processhandler.py)
is the central exposed API for mozprocess.  `ProcessHandler` utilizes
a contained subclass of [subprocess.Popen](http://docs.python.org/library/subprocess.html),
`Process`, which does the brunt of the process management.

## Basic usage

    process = ProcessHandler(['command', '-line', 'arguments'],
                             cwd=None, # working directory for cmd; defaults to None
                             env={},   # environment to use for the process; defaults to os.environ
                             )
    process.run(timeout=60) # seconds
    process.wait()

`ProcessHandler` offers several other properties and methods as part of its API:

    def __init__(self,
                 cmd,
                 args=None,
                 cwd=None,
                 env=None,
                 ignore_children = False,
                 processOutputLine=(),
                 onTimeout=(),
                 onFinish=(),
                 **kwargs):
        """
        cmd = Command to run
        args = array of arguments (defaults to None)
        cwd = working directory for cmd (defaults to None)
        env = environment to use for the process (defaults to os.environ)
        ignore_children = when True, causes system to ignore child processes,
        defaults to False (which tracks child processes)
        processOutputLine = handlers to process the output line
        onTimeout = handlers for timeout event
        kwargs = keyword args to pass directly into Popen

        NOTE: Child processes will be tracked by default. If for any reason
        we are unable to track child processes and ignore_children is set to False,
        then we will fall back to only tracking the root process. The fallback
        will be logged.
        """

    @property
    def timedOut(self):
        """True if the process has timed out."""


    def run(self, timeout=None, outputTimeout=None):
        """
        Starts the process.

        If timeout is not None, the process will be allowed to continue for
        that number of seconds before being killed.

        If outputTimeout is not None, the process will be allowed to continue
        for that number of seconds without producing any output before
        being killed.
        """

    def kill(self):
        """
        Kills the managed process and if you created the process with
        'ignore_children=False' (the default) then it will also
        also kill all child processes spawned by it.
        If you specified 'ignore_children=True' when creating the process,
        only the root process will be killed.

        Note that this does not manage any state, save any output etc,
        it immediately kills the process.
        """

    def readWithTimeout(self, f, timeout):
        """
        Try to read a line of output from the file object |f|.
        |f| must be a pipe, like the |stdout| member of a subprocess.Popen
        object created with stdout=PIPE. If no output
        is received within |timeout| seconds, return a blank line.
        Returns a tuple (line, did_timeout), where |did_timeout| is True
        if the read timed out, and False otherwise.

        Calls a private member because this is a different function based on
        the OS
        """

    def processOutputLine(self, line):
        """Called for each line of output that a process sends to stdout/stderr."""
        for handler in self.processOutputLineHandlers:
            handler(line)

    def onTimeout(self):
        """Called when a process times out."""
        for handler in self.onTimeoutHandlers:
            handler()

    def onFinish(self):
        """Called when a process finishes without a timeout."""
        for handler in self.onFinishHandlers:
            handler()

    def wait(self, timeout=None):
        """
        Waits until all output has been read and the process is 
        terminated.

        If timeout is not None, will return after timeout seconds.
        This timeout only causes the wait function to return and
        does not kill the process.
        """

See https://github.com/mozilla/mozbase/blob/master/mozprocess/mozprocess/processhandler.py
for the python implementation.

`ProcessHandler` extends `ProcessHandlerMixin` which by default prints the
output, logs to a file (if specified), and stores the output (if specified, by
default `True`).  `ProcessHandlerMixin`, by default, does none of these things
and has no handlers for `onTimeout`, `processOutput`, or `onFinish`.

`ProcessHandler` may be subclassed to handle process timeouts (by overriding
the `onTimeout()` method), process completion (by overriding
`onFinish()`), and to process the command output (by overriding
`processOutputLine()`).

## Examples

In the most common case, a process_handler is created, then run followed by wait are called:

    proc_handler = ProcessHandler([cmd, args])
    proc_handler.run(outputTimeout=60) # will time out after 60 seconds without output
    proc_handler.wait()

Often, the main thread will do other things:

    proc_handler = ProcessHandler([cmd, args])
    proc_handler.run(timeout=60) # will time out after 60 seconds regardless of output
    do_other_work()

    if proc_handler.proc.poll() is None:
        proc_handler.wait()

By default output is printed to stdout, but anything is possible:

    # this example writes output to both stderr and a file called 'output.log'
    def some_func(line):
        print >> sys.stderr, line

        with open('output.log', 'a') as log:
            log.write('%s\n' % line)

    proc_handler = ProcessHandler([cmd, args], processOutputLine=some_func)
    proc_handler.run()
    proc_handler.wait()

# TODO

- Document improvements over `subprocess.Popen.kill`
- Introduce test the show improvements over `subprocess.Popen.kill`
