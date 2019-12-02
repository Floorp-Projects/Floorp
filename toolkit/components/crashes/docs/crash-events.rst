============
Crash Events
============

**Crash Events** refers to a special subsystem of Gecko that aims to capture
events of interest related to process crashing and hanging.

When an event worthy of recording occurs, a file containing that event's
information is written to a well-defined location on the filesystem. The Gecko
process periodically scans for produced files and consolidates information
into a more unified and efficient backend store.

Crash Event Files
=================

When a crash-related event occurs, a file describing that event is written
to a well-defined directory. That directory is likely in the directory of
the currently-active profile. However, if a profile is not yet active in
the Gecko process, that directory likely resides in the user's *app data*
directory (*UAppData* from the directory service).

The filename of the event file is not relevant. However, producers need
to choose a filename intelligently to avoid name collisions and race
conditions. Since file locking is potentially dangerous at crash time,
the convention of generating a UUID and using it as a filename has been
adopted.

File Format
-----------

All crash event files share the same high-level file format. The format
consists of the following fields delimited by a UNIX newline (*\n*)
character:

* String event name (valid UTF-8, but likely ASCII)
* String representation of integer seconds since UNIX epoch
* Payload

The payload is event specific and may contain UNIX newline characters.
The recommended method for parsing is to split at most 3 times on UNIX
newline and then dispatch to an event-specific parsed based on the
event name.

If an unknown event type is encountered, the event can safely be ignored
until later. This helps ensure that application downgrades (potentially
due to elevated crash rate) don't result in data loss.

The format and semantics of each event type are meant to be constant once
that event type is committed to the main Firefox repository. If new metadata
needs to be captured or the meaning of data captured in an event changes,
that change should be expressed through the invention of a new event type.
For this reason, event names are highly recommended to contain a version.
e.g. instead of a *Gecko process crashed* event, we prefer a *Gecko process
crashed v1* event.

Event Types
-----------

Each subsection documents the different types of crash events that may be
produced. Each section name corresponds to the first line of the crash
event file.

Currently only main process crashes produce event files. Because crashes and
hangs in child processes can be easily recorded by the main process, we do not
foresee the need for writing event files for child processes, design
considerations below notwithstanding.

crash.main.3
^^^^^^^^^^^^

This event is produced when the main process crashes.

The payload of this event is delimited by UNIX newlines (*\n*) and contains the
following fields:

* The crash ID string, very likely a UUID
* One line holding the crash metadata serialized as a JSON string

crash.main.2
^^^^^^^^^^^^

This event is produced when the main process crashes.

The payload of this event is delimited by UNIX newlines (*\n*) and contains the
following fields:

* The crash ID string, very likely a UUID
* 0 or more lines of metadata, each containing one key=value pair of text

This event is obsolete.

crash.main.1
^^^^^^^^^^^^

This event is produced when the main process crashes.

The payload of this event is the string crash ID, very likely a UUID.
There should be ``UUID.dmp`` and ``UUID.extra`` files on disk, saved by
Breakpad.

This event is obsolete.

crash.submission.1
^^^^^^^^^^^^^^^^^^

This event is produced when a crash is submitted.

The payload of this event is delimited by UNIX newlines (*\n*) and contains the
following fields:

* The crash ID string
* "true" if the submission succeeded or "false" otherwise
* The remote crash ID string if the submission succeeded

Aggregated Event Log
====================

Crash events are aggregated together into a unified event *log*. Currently,
this *log* is really a JSON file. However, this is an implementation detail
and it could change at any time. The interface to crash data provided by
the JavaScript API is the only supported interface.

Design Considerations
=====================

There are many considerations influencing the design of this subsystem.
We attempt to document them in this section.

Decoupling of Event Files from Final Data Structure
---------------------------------------------------

While it is certainly possible for the Gecko process to write directly to
the final data structure on disk, there is an intentional decoupling between
the production of events and their transition into final storage. Along the
same vein, the choice to have events written to multiple files by producers
is deliberate.

Some recorded events are written immediately after a process crash. This is
a very uncertain time for the host system. There is a high liklihood the
system is in an exceptional state, such as memory exhaustion. Therefore, any
action taken after crashing needs to be very deliberate about what it does.
Excessive memory allocation and certain system calls may cause the system
to crash again or the machine's condition to worsen. This means that the act
of recording a crash event must be very light weight. Writing a new file from
nothing is very light weight. This is one reason we write separate files.

Another reason we write separate files is because if the main Gecko process
itself crashes (as opposed to say a plugin process), the crash reporter (not
Gecko) is running and the crash reporter needs to handle the writing of the
event info. If this writing is involved (say loading, parsing, updating, and
reserializing back to disk), this logic would need to be implemented in both
Gecko and the crash reporter or would need to be implemented in such a way
that both could use. Neither of these is very practical from a software
lifecycle management perspective. It's much easier to have separate processes
write a simple file and to let a single implementation do all the complex
work.

Idempotent Event Processing
===========================

Processing of event files has been designed such that the result is
idempotent regardless of what order those files are processed in. This is
not only a good design decision, but it is arguably necessary. While event
files are processed in order by file mtime, filesystem times may not have
the resolution required for proper sorting. Therefore, processing order is
merely an optimistic assumption.

Aggregated Storage Format
=========================

Crash events are aggregated into a unified data structure on disk. That data
structure is currently LZ4-compressed JSON and is represented by a single file.

The choice of a single JSON file was initially driven by time and complexity
concerns. Before changing the format or adding significant amounts of new
data, some considerations must be taken into account.

First, in well-behaving installs, crash data should be minimal. Crashes and
hangs will be rare and thus the size of the crash data should remain small
over time.

The choice of a single JSON file has larger implications as the amount of
crash data grows. As new data is accumulated, we need to read and write
an entire file to make small updates. LZ4 compression helps reduce I/O.
But, there is a potential for unbounded file growth. We establish a
limit for the max age of records. Anything older than that limit is
pruned. We also establish a daily limit on the number of crashes we will
store. All crashes beyond the first N in a day have no payload and are
only recorded by the presence of a count. This count ensures we can
distinguish between ``N`` and ``100 * N``, which are very different
values!
