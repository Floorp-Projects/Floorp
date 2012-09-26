The mach Driver
===============

The *mach* driver is the command line interface (CLI) to the source tree.

The *mach* driver is invoked by running the *mach* script or from
instantiating the *Mach* class from the *mach.main* module.

Implementing mach Commands
--------------------------

The *mach* driver follows the convention of popular tools like Git,
Subversion, and Mercurial and provides a common driver for multiple
sub-commands.

Modules inside *mach* typically contain 1 or more classes which
inherit from *mach.base.ArgumentProvider*. Modules that inherit from
this class are hooked up to the *mach* CLI driver. So, to add a new
sub-command/action to *mach*, one simply needs to create a new class in
the *mach* package which inherits from *ArgumentProvider*.

Currently, you also need to hook up some plumbing in
*mach.main.Mach*. In the future, we hope to have automatic detection
of submodules.

Your command class performs the role of configuring the *mach* frontend
argument parser as well as providing the methods invoked if a command is
requested. These methods will take the user-supplied input, do something
(likely by calling a backend function in a separate module), then format
output to the terminal.

The plumbing to hook up the arguments to the *mach* driver involves
light magic. At *mach* invocation time, the driver creates a new
*argparse* instance. For each registered class that provides commands,
it calls the *populate_argparse* static method, passing it the parser
instance.

Your class's *populate_argparse* function should register sub-commands
with the parser.

For example, say you want to provide the *doitall* command. e.g. *mach
doitall*. You would create the module *mach.doitall* and this
module would contain the following class:

    from mach.base import ArgumentProvider

    class DoItAll(ArgumentProvider):
        def run(self, more=False):
            print 'I did it!'

        @staticmethod
        def populate_argparse(parser):
            # Create the parser to handle the sub-command.
            p = parser.add_parser('doitall', help='Do it all!')

            p.add_argument('more', action='store_true', default=False,
                help='Do more!')

            # Tell driver that the handler for this sub-command is the
            # method *run* on the class *DoItAll*.
            p.set_defaults(cls=DoItAll, method='run')

The most important line here is the call to *set_defaults*.
Specifically, the *cls* and *method* parameters, which tell the driver
which class to instantiate and which method to execute if this command
is requested.

The specified method will receive all arguments parsed from the command.
It is important that you use named - not positional - arguments for your
handler functions or things will blow up. This is because the mach driver
is using the ``**kwargs`` notation to call the defined method.

In the future, we may provide additional syntactical sugar to make all
this easier. For example, we may provide decorators on methods to hook
up commands and handlers.

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
