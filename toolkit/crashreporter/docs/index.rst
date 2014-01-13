==============
Crash Reporter
==============

Overview
========

The **crash reporter** is a subsystem to record and manage application
crash data.

While the subsystem is known as *crash reporter*, it helps to think of
it more as a *process dump manager*. This is because the heart of this
subsystem is really managing process dump files and these files are
created not only from process crashes but also from hangs and other
exceptional events.

The crash reporter subsystem is composed of a number of pieces working
together.

Breakpad
   Breakpad is a library and set of tools to make collecting process
   information (notably dumps from crashes) easy. Breakpad is a 3rd
   party project (originaly developed by Google) that is imported into
   the tree.

Dump files
   Breakpad produces files called *dump files* that hold process data
   (stacks, heap data, etc).

Crash Reporter Client
   The crash reporter client is a standalone executable that is launched
   to handle dump files. This application optionally submits crashes to
   Mozilla (or the configured server).

How Main-Process Crash Handling Works
=====================================

The crash handler is hooked up very early in the Gecko process lifetime.
It all starts in ``XREMain::XRE_mainInit()`` from ``nsAppRunner.cpp``.
Assuming crash reporting is enabled, this startup function registers an
exception handler for the process and tells the crash reporter subsystem
about basic metadata such as the application name and version.

The registration of the crash reporter exception handler doubles as
initialization of the crash reporter itself. This happens in
``CrashReporter::SetExceptionHandler()`` from ``nsExceptionHandler.cpp``.
The crash reporter figures out what application to use for reporting
dumped crashes and where to store these dump files on disk. The Breakpad
exception handler (really just a mechanism for dumping process state) is
initialized as part of this function. The Breakpad exception handler is
a ``google_breakpad::ExceptionHandler`` instance and it's stored as
``gExceptionHandler``.

As the application runs, various other systems may write *annotations*
or *notes* to the crash reporter to indicate state of the application,
help with possible reasons for a current or future crash, etc. These are
performed via ``CrashReporter::AnnotateCrashReport()`` and
``CrashReporter::AppendAppNotesToCrashReport()`` from
``nsExceptionHandler.h``.

For well running applications, this is all that happens. However, if a
crash or similar exceptional event occurs (such as a hang), we need to
write a crash report.

When an event worthy of writing a dump occurs, the Breakpad exception
handler is invoked and Breakpad does its thing. When Breakpad has
finished, it calls back into ``CrashReporter::MinidumpCallback()`` from
``nsExceptionHandler.cpp`` to tell the crash reporter about what was
written.

``MinidumpCallback()`` performs a number of actions once a dump has been
written. It writes a file with the time of the crash so other systems can
easily determine the time of the last crash. It supplements the dump
file with an *extra* file containing Mozilla-specific metadata. This data
includes the annotations set via ``CrashReporter::AnnotateCrashReport()``
as well as time since last crash, whether garbage collection was active at
the time of the crash, memory statistics, etc.

If the *crash reporter client* is enabled, ``MinidumpCallback()`` invokes
it. It simply tries to create a new *crash reporter client* process (e.g.
*crashreporter.exe*) with the path to the written minidump file as an
argument.

The *crash reporter client* performs a number of roles. There's a lot going
on, so you may want to look at ``main()`` in ``crashreporter.cpp``. First,
it verifies the dump data is sane. If it isn't (e.g. required metadata is
missing), the dump data is ignored. If dump data looks sane, the dump data
is moved into the *pending* directory for the configured data directory
(defined via the ``MOZ_CRASHREPORTER_DATA_DIRECTORY`` environment variable
or from the UI). Once this is done, the main crash reporter UI is displayed
via ``UIShowCrashUI()``. The crash reporter UI is platform specific: there
are separate versions for Windows, OS X, and various \*NIX presentation
flavors (such as GTK). The basic gist is a dialog is displayed to the user
and the user has the opportunity to submit this dump data to a remote
server.

If a dump is submitted via the crash reporter, the raw dump files are
removed from the *pending* directory and a file containing the
crash ID from the remote server for the submitted dump is created in the
*submitted* directory.

If the user chooses not to submit a dump in the crash reporter UI, the dump
files are deleted.

And that's pretty much what happens when a crash/dump is written!

Plugin and Child Process Crashes
================================

Crashes in plugin and child processes are also managed by the crash
reporting subsystem.

Child process crashes are handled by the ``mozilla::dom::CrashReporterParent``
class defined in ``dom/ipc``. When a child process crashes, the toplevel IPDL
actor should check for it by calling TakeMinidump in its ``ActorDestroy``
Method: see ``mozilla::plugins::PluginModuleParent::ActorDestroy`` and
``mozilla::plugins::PluginModuleParent::ProcessFirstMinidump``. That method
is responsible for calling
``mozilla::dom::CrashReporterParent::GenerateCrashReportForMinidump`` with
appropriate crash annotations specific to the crash. All child-process
crashes are annotated with a ``ProcessType`` annotation, such as "content" or
"plugin".

Submission of child process crashes is handled by application code. This
code prompts the user to submit crashes in context-appropriate UI and then
submits the crashes using ``CrashSubmit.jsm``.

Flash Process Crashes
=====================

On Windows Vista+, the Adobe Flash plugin creates two extra processes in its
Firefox plugin to implement OS-level sandboxing. In order to catch crashes in
these processes, Firefox injects a crash report handler into the process using the code at ``InjectCrashReporter.cpp``. When these crashes occur, the
ProcessType=plugin annotation is present, and an additional annotation
FlashProcessDump has the value "Sandbox" or "Broker".

Plugin Hangs
============

Plugin hangs are handled as crash reports. If a plugin doesn't respond to an
IPC message after 60 seconds, the plugin IPC code will take minidumps of all
of the processes involved and then kill the plugin.

In this case, there will be only one .ini file with the crash report metadata,
but there will be multiple dump files: at least one for the browser process and
one for the plugin process, and perhaps also additional dumps for the Flash
sandbox and broker processes. All of these files are submitted together as a
unit. Before submission, the filenames of the files are linked:

- **uuid.ini** - *annotations, includes an additional_minidumps field*
- **uuid.dmp** - *plugin process dump file*
- **uuid-<other>.dmp** - *other process dump file as listed in additional_minidumps*

Browser Hangs
=============

There is a feature of Firefox that will crash Firefox if it stops processing
messages after a certain period of time. This feature doesn't work well and is
disabled by default. See ``xpcom/threads/HangMonitor.cpp``. Hang crashes
are annotated with ``Hang=1``.

about:crashes
=============

If the crash reporter subsystem is enabled, the *about:crashes*
page will be registered with the application. This page provides
information about previous and submitted crashes.

It is also possible to submit crashes from *about:crashes*.
