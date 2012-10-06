The mach Driver
===============

The *mach* driver is the command line interface (CLI) to the source tree.

The *mach* driver is invoked by running the *mach* script or from
instantiating the *Mach* class from the *mach.main* module.

Implementing mach Commands
--------------------------

The *mach* driver follows the convention of popular tools like Git,
Subversion, and Mercurial and provides a common driver for multiple
subcommands.

Subcommands are implemented by decorating a class inheritting from
mozbuild.base.MozbuildObject and by decorating methods that act as
subcommand handlers.

Relevant decorators are defined in the *mach.base* module. There are
the *Command* and *CommandArgument* decorators, which should be used
on methods to denote that a specific method represents a handler for
a mach subcommand. There is also the *CommandProvider* decorator,
which is applied to a class to denote that it contains mach subcommands.

Here is a complete example:

    from mozbuild.base import MozbuildObject

    from mach.base import CommandArgument
    from mach.base import CommandProvider
    from mach.base import Command

    @CommandProvider
    class MyClass(MozbuildObject):

        @Command('doit', help='Do ALL OF THE THINGS.')
        @CommandArgument('--force', '-f', action='store_true',
            help='Force doing it.')
        def doit(self, force=False):
            # Do stuff here.


When the module is loaded, the decorators tell mach about all handlers.
When mach runs, it takes the assembled metadata from these handlers and
hooks it up to the command line driver. Under the hood, arguments passed
to the decorators are being used as arguments to
*argparse.ArgumentParser.add_parser()* and
*argparse.ArgumentParser.add_argument()*. See the documentation in the
*mach.base* module for more.

The Python modules defining mach commands do not need to live inside the
main mach source tree. If a path on *sys.path* contains a *mach/commands*
directory, modules will be loaded automatically by mach and any classes
containing the decorators described above will be detected and loaded
automatically by mach. So, to add a new subcommand to mach, you just need
to ensure your Python module is present on *sys.path*.

Minimizing Code in Mach
-----------------------

Mach is just a frontend. Therefore, code in this package should pertain to
one of 3 areas:

1. Obtaining user input (parsing arguments, prompting, etc)
2. Calling into some other Python package
3. Formatting output

Mach should not contain core logic pertaining to the desired task. If you
find yourself needing to invent some new functionality, you should implement
it as a generic package outside of mach and then write a mach shim to call
into it. There are many advantages to this approach, including reusability
outside of mach (others may want to write other frontends) and easier testing
(it is easier to test generic libraries than code that interacts with the
command line or terminal).

Keeping Frontend Modules Small
------------------------------

The frontend modules providing mach commands are currently all loaded when
the mach CLI driver starts. Therefore, there is potential for *import bloat*.

We want the CLI driver to load quickly. So, please delay load external modules
until they are actually required. In other words, don't use a global
*import* when you can import from inside a specific command's handler.
