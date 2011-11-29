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

Basic usage:

    process = ProcessHandler(['command', '-line', 'arguments'],
                             cwd=None, # working directory for cmd; defaults to None
                             env={},   # environment to use for the process; defaults to os.environ
                             )         
    exit_code = process.waitForFinish(timeout=60) # seconds

See an example in https://github.com/mozilla/mozbase/blob/master/mutt/mutt/tests/python/testprofilepath.py

`ProcessHandler` may be subclassed to handle process timeouts (by overriding
the `onTimeout()` method), process completion (by overriding 
`onFinish()`), and to process the command output (by overriding 
`processOutputLine()`).

# TODO

- Document improvements over `subprocess.Popen.kill`
