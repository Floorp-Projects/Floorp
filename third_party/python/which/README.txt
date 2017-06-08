which.py -- a portable GNU which replacement
============================================

Download the latest which.py packages from here:
    (source) http://trentm.com/downloads/which/1.1.0/which-1.1.0.zip


Home            : http://trentm.com/projects/which/
License         : MIT (see LICENSE.txt)
Platforms       : Windows, Linux, Mac OS X, Unix
Current Version : 1.1
Dev Status      : mature, has been heavily used in a commercial product for
                  over 2 years
Requirements    : Python >= 2.3 (http://www.activestate.com/ActivePython/)


What's new?
-----------

I have moved hosting of `which.py` from my old [Starship
pages](http://starship.python.net/~tmick/) to this site. These starter
docs have been improved a little bit. See the [Change Log](#changelog)
below for more.

**WARNING**: If you are upgrading your `which.py` and you also use my
[process.py](../process/) module, you must upgrade `process.py` as well
because of the `_version_/__version__` change in v1.1.0.


Why which.py?
-------------

`which.py` is a small GNU-which replacement. It has the following
features:

- it is portable (Windows, Linux, Mac OS X, Un*x); 
- it understands PATHEXT and "App Paths" registration on Windows 
  (i.e. it will find everything that `start` does from the command shell); 
- it can print all matches on the PATH; 
- it can note "near misses" on the PATH (e.g. files that match but may
  not, say, have execute permissions); and 
- it can be used as a Python module. 

I also would be happy to have this be a replacement for the `which.py` in the
Python CVS tree at `dist/src/Tools/scripts/which.py` which is
Unix-specific and not usable as a module; and perhaps for inclusion in
the stdlib.

Please send any feedback to [Trent Mick](mailto:TrentM@ActiveState.com).


Install Notes
-------------

Download the latest `which.py` source package, unzip it, and run
`python setup.py install`:

    unzip which-1.1.0.zip
    cd which-1.1.0
    python setup.py install

If your install fails then please visit [the Troubleshooting
FAQ](http://trentm.com/faq.html#troubleshooting-python-package-installation).

`which.py` can be used both as a module and as a script. By default,
`which.py` will be installed into your Python's `site-packages`
directory so it can be used as a module. On *Windows only*, `which.py`
(and the launcher stub `which.exe`) will be installed in the Python
install dir to (hopefully) put `which` on your PATH.

On Un*x platforms (including Linux and Mac OS X) there is often a
`which` executable already on your PATH. To use this `which` instead of
your system's on those platforms you can manually do one of the
following:

- Copy `which.py` to `which` somewhere on your PATH ahead of the system
  `which`. This can be a symlink, as well:

        ln -s /PATH/TO/site-packages/which.py /usr/local/bin/which

- Python 2.4 users might want to use Python's new '-m' switch and setup
  and alias:
  
        alias which='python -m which'
  
  or stub script like this:

        #!/bin/sh
        python -m which $@


Getting Started
---------------

Currently the best intro to using `which.py` as a module is its module
documentation.  Either install `which.py` and run:

    pydoc which
    
take a look at `which.py` in your editor or [here](which.py), or read
on. Most commonly you'll use the `which()` method to find an
executable:

    >>> import which
    >>> which.which("perl")
    '/usr/local/bin/perl'

Or you might want to know if you have multiple versions on your path:

    >>> which.whichall("perl")
    ['/usr/local/bin/perl', '/usr/bin/perl']

Use `verbose` to see where your executable is being found. (On Windows
this might not always be so obvious as your PATH environment variable.
There is an "App Paths" area of the registry where the `start` command
will find "registered" executables -- `which.py` mimics this.)

    >>> which.whichall("perl", verbose=True)
    [('/usr/local/bin/perl', 'from PATH element 10'), 
     ('/usr/bin/perl', 'from PATH element 15')]

You can restrict the searched path:

    >>> which.whichall("perl", path=["/usr/bin"])
    ['/usr/bin/perl']

There is a generator interface:

    >>> for perl in which.whichgen("perl"):
    ...     print "found a perl here:", perl
    ... 
    found a perl here: /usr/local/bin/perl
    found a perl here: /usr/bin/perl

An exception is raised if your executable is not found:

    >>> which.which("fuzzywuzzy")
    Traceback (most recent call last):
      ...
    which.WhichError: Could not find 'fuzzywuzzy' on the path.
    >>> 

There are some other options too:
    
    >>> help(which.which)
    ...

Run `which --help` to see command-line usage:

    $ which --help
    Show the full path of commands.

    Usage:
        which [<options>...] [<command-name>...]

    Options:
        -h, --help      Print this help and exit.
        -V, --version   Print the version info and exit.

        -a, --all       Print *all* matching paths.
        -v, --verbose   Print out how matches were located and
                        show near misses on stderr.
        -q, --quiet     Just print out matches. I.e., do not print out
                        near misses.

        -p <altpath>, --path=<altpath>
                        An alternative path (list of directories) may
                        be specified for searching.
        -e <exts>, --exts=<exts>
                        Specify a list of extensions to consider instead
                        of the usual list (';'-separate list, Windows
                        only).

    Show the full path to the program that would be run for each given
    command name, if any. Which, like GNU's which, returns the number of
    failed arguments, or -1 when no <command-name> was given.

    Near misses include duplicates, non-regular files and (on Un*x)
    files without executable access.


Change Log
----------

### v1.1.0
- Change version attributes and semantics. Before: had a _version_
  tuple. After: __version__ is a string, __version_info__ is a tuple.

### v1.0.3
- Move hosting of which.py to trentm.com. Tweaks to associated bits
  (README.txt, etc.)

### v1.0.2:
- Rename mainline handler function from _main() to main(). I can
  conceive of it being called from externally.

### v1.0.1:
- Add an optimization for Windows to allow the optional
  specification of a list of exts to consider when searching the
  path.

### v1.0.0:
- Simpler interface: What was which() is now called whichgen() -- it
  is a generator of matches. The simpler which() and whichall()
  non-generator interfaces were added.

### v0.8.1:
- API change: 0.8.0's API change making "verbose" output the default
  was a mistake -- it breaks backward compatibility for existing
  uses of which in scripts. This makes verbose, once again, optional
  but NOT the default.

### v0.8.0:
- bug fix: "App Paths" lookup had been crippled in 0.7.0. Restore that.
- feature/module API change: Now print out (and return for the module
  interface) from where a match was found, e.g. "(from PATH element 3)".
  The module interfaces now returns (match, from-where) tuples.
- bug fix: --path argument was broken (-p shortform was fine)

### v0.7.0:
- bug fix: Handle "App Paths" registered executable that does not
  exist.
- feature: Allow an alternate PATH to be specified via 'path'
  optional argument to which.which() and via -p|--path command line
  option.

### v0.6.1:
- first public release

