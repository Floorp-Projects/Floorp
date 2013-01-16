=================
mozbuild.frontend
=================

The mozbuild.frontend package is of sufficient importance and complexity
to warrant its own README file. If you are looking for documentation on
how the build system gets started, you've come to the right place.

Overview
========

The build system is defined by a bunch of files in the source tree called
*mozbuild* files. Each *mozbuild* file defines a unique part of the overall
build system. This includes information like "compile file X," "copy this
file here," "link these files together to form a library." Together,
all the *mozbuild* files define how the entire build system works.

*mozbuild* files are actually Python scripts. However, their execution
is governed by special rules. This will be explained later.

Once a *mozbuild* file has executed, it is converted into a set of static
data structures.

The set of all data structures from all relevant *mozbuild* files
constitutes the current build configuration.

How *mozbuild* Files Work
=========================

As stated above, *mozbuild* files are actually Python scripts. However,
their behavior is very different from what you would expect if you executed
the file using the standard Python interpreter from the command line.

There are two properties that make execution of *mozbuild* files special:

1. They are evaluated in a sandbox which exposes a limited subset of Python
2. There is a special set of global variables which hold the output from
   execution.

The limited subset of Python is actually an extremely limited subset.
Only a few built-ins are exposed. These include *True*, *False*, and
*None*. Global functions like *import*, *print*, and *open* aren't defined.
Without these, *mozbuild* files can do very little. This is by design.

The side-effects of the execution of a *mozbuild* file are used to define
the build configuration. Specifically, variables set during the execution
of a *mozbuild* file are examined and their values are used to populate
data structures.

The enforced convention is that all UPPERCASE names inside a sandbox are
reserved and it is the value of these variables post-execution that is
examined. Furthermore, the set of allowed UPPERCASE variable names and
their types is statically defined. If you attempt to reference or assign
to an UPPERCASE variable name that isn't known to the build system or
attempt to assign a value of the wrong type (e.g. a string when it wants a
list), an error will be raised during execution of the *mozbuild* file.
This strictness is to ensure that assignment to all UPPERCASE variables
actually does something. If things weren't this way, *mozbuild* files
might think they were doing something but in reality wouldn't be. We don't
want to create false promises, so we validate behavior strictly.

If a variable is not UPPERCASE, you can do anything you want with it,
provided it isn't a function or other built-in. In other words, normal
Python rules apply.

All of the logic for loading and evaluating *mozbuild* files is in the
*reader* module. Of specific interest is the *MozbuildSandbox* class. The
*BuildReader* class is also important, as it is in charge of
instantiating *MozbuildSandbox* instances and traversing a tree of linked
*mozbuild* files. Unless you are a core component of the build system,
*BuildReader* is probably the only class you care about in this module.

The set of variables and functions *exported* to the sandbox is defined by
the *sandbox_symbols* module. These data structures are actually used to
populate MozbuildSandbox instances. And, there are tests to ensure that the
sandbox doesn't add new symbols without those symbols being added to the
module. And, since the module contains documentation, this ensures the
documentation is up to date (at least in terms of symbol membership).

How Sandboxes are Converted into Data Structures
================================================

The output of a *mozbuild* file execution is essentially a dict of all
the special UPPERCASE variables populated during its execution. While these
dicts are data structures, they aren't the final data structures that
represent the build configuration.

We feed the *mozbuild* execution output (actually *reader.MozbuildSandbox*
instances) into a *BuildDefinitionEmitter* class instance. This class is
defined in the *emitter* module. *BuildDefinitionEmitter* converts the
*MozbuildSandbox* instances into instances of the *BuildDefinition*-derived
classes from the *data* module.

All the classes in the *data* module define a domain-specific
component of the build configuration. File compilation and IDL generation
are separate classes, for example. The only thing these classes have in
common is that they inherit from *BuildDefinition*, which is merely an
abstract base class.

The set of all emitted *BuildDefinition* instances (converted from executed
*mozbuild* files) constitutes the aggregate build configuration. This is
the authoritative definition of the build system and is what's used by
all downstream consumers, such as backends. There is no monolithic build
system configuration class. Instead, the build system configuration is
modeled as a collection/iterable of *BuildDefinition*.

There is no defined mapping between the number of
*MozbuildSandbox*/*moz.build* instances and *BuildDefinition* instances.
Some *mozbuild* files will emit only 1 *BuildDefinition* instance. Some
will emit 7. Some may even emit 0!

The purpose of this *emitter* layer between the raw *mozbuild* execution
result and *BuildDefinition* is to facilitate additional normalization and
verification of the output. The downstream consumer of the build
configuration are build backends. And, there are several of these. There
are common functions shared by backends related to examining the build
configuration. It makes sense to move this functionality upstream as part
of a shared pipe. Thus, *BuildDefinitionEmitter* exists.

Other Notes
===========

*reader.BuildReader* and *emitter.BuildDefinitionEmitter* have a nice
stream-based API courtesy of generators. When you hook them up properly,
*BuildDefinition* instances can be consumed before all *mozbuild* files have
been read. This means that errors down the pipe can trigger before all
upstream tasks (such as executing and converting) are complete. This should
reduce the turnaround time in the event of errors. This likely translates to
a more rapid pace for implementing backends, which require lots of iterative
runs through the entire system.

In theory, the frontend to the build system is generic and could be used
by any project. In practice, parts are specifically tailored towards
Mozilla's needs. With a little work, the core build system bits could be
separated into its own package, independent of the Mozilla bits. Or, one
could simply replace the Mozilla-specific pieces in the *variables*, *data*,
and *emitter* modules to reuse the core logic.
