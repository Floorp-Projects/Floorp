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

Subcommands are implemented by decorating a class and by decorating
methods that act as subcommand handlers.

Relevant decorators are defined in the *mach.decorators* module. There are
the *Command* and *CommandArgument* decorators, which should be used
on methods to denote that a specific method represents a handler for
a mach subcommand. There is also the *CommandProvider* decorator,
which is applied to a class to denote that it contains mach subcommands.

Classes with the *@CommandProvider* decorator *must* have an *__init__*
method that accepts 1 or 2 arguments. If it accepts 2 arguments, the
2nd argument will be a *MachCommandContext* instance. This is just a named
tuple containing references to objects provided by the mach driver.

Here is a complete example:

    from mach.decorators import (
        CommandArgument,
        CommandProvider,
        Command,
    )

    @CommandProvider
    class MyClass(object):
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
main mach source tree.

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

Structured Logging
==================

One of the features of mach is structured logging. Instead of conventional
logging where simple strings are logged, the internal logging mechanism logs
all events with the following pieces of information:

* A string *action*
* A dict of log message fields
* A formatting string

Essentially, instead of assembling a human-readable string at
logging-time, you create an object holding all the pieces of data that
will constitute your logged event. For each unique type of logged event,
you assign an *action* name.

Depending on how logging is configured, your logged event could get
written a couple of different ways.

JSON Logging
------------

Where machines are the intended target of the logging data, a JSON
logger is configured. The JSON logger assembles an array consisting of
the following elements:

* Decimal wall clock time in seconds since UNIX epoch
* String *action* of message
* Object with structured message data

The JSON-serialized array is written to a configured file handle.
Consumers of this logging stream can just perform a readline() then feed
that into a JSON deserializer to reconstruct the original logged
message. They can key off the *action* element to determine how to
process individual events. There is no need to invent a parser.
Convenient, isn't it?

Logging for Humans
------------------

Where humans are the intended consumer of a log message, the structured
log message are converted to more human-friendly form. This is done by
utilizing the *formatting* string provided at log time. The logger
simply calls the *format* method of the formatting string, passing the
dict containing the message's fields.

When *mach* is used in a terminal that supports it, the logging facility
also supports terminal features such as colorization. This is done
automatically in the logging layer - there is no need to control this at
logging time.

In addition, messages intended for humans typically prepends every line
with the time passed since the application started.

Logging HOWTO
-------------

Structured logging piggybacks on top of Python's built-in logging
infrastructure provided by the *logging* package. We accomplish this by
taking advantage of *logging.Logger.log()*'s *extra* argument. To this
argument, we pass a dict with the fields *action* and *params*. These
are the string *action* and dict of message fields, respectively. The
formatting string is passed as the *msg* argument, like normal.

If you were logging to a logger directly, you would do something like:

    logger.log(logging.INFO, 'My name is {name}',
        extra={'action': 'my_name', 'params': {'name': 'Gregory'}})

The JSON logging would produce something like:

    [1339985554.306338, "my_name", {"name": "Gregory"}]

Human logging would produce something like:

     0.52 My name is Gregory

Since there is a lot of complexity using logger.log directly, it is
recommended to go through a wrapping layer that hides part of the
complexity for you. The easiest way to do this is by utilizing the
LoggingMixin:

    import logging
    from mach.mixin.logging import LoggingMixin

    class MyClass(LoggingMixin):
        def foo(self):
             self.log(logging.INFO, 'foo_start', {'bar': True},
                 'Foo performed. Bar: {bar}')

