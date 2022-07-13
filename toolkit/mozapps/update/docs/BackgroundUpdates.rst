==================
Background Updates
==================

The purpose of the background update system is to perform application updates
during times when Firefox is not running. It was originally implemented in `bug
1689520 <https://bugzilla.mozilla.org/show_bug.cgi?id=1689520>`__.

The system has three main tasks it needs to handle:

1. :ref:`Determining whether background updates are possible <background-updates-determining>`

2. :ref:`Scheduling background tasks <background-updates-scheduling>`

3. :ref:`Checking for updates <background-updates-checking>`

Architecturally, the background task is an instance of Firefox running in a
special background mode, not a separate tool. This allows it to leverage
existing functionality in Firefox, including the existing update code, but also
keep acceptable performance characteristics for a background task by controlling
and limiting the parts of Firefox that are loaded.

Everything in this document applies only to Microsoft Windows systems. In the
future, we would like to extend background update support to macOS (see `bug
1653435 <https://bugzilla.mozilla.org/show_bug.cgi?id=1653435>`__), however
support for Linux and other Unix variants is not planned due to the variation in
OS-level scheduling affordances across distributions/configurations.

Lifecycle
=========

When background updates are possible, the background update task will be invoked
every 7 hours (by default).  The first invocation initiates an update download
which proceeds after the task exits using Windows BITS.  The second invocation
prepares and stages the update.  The third invocation installs the update as it
starts up, and then checks for a newer update, possibly initiating another
update download.  The cycle then continues.  If the user launches Firefox at any
point in this process, it will take over.  If the background update task is
invoked while Firefox proper is running, the task exits without doing any work.
In the future, the second invocation will stage and then restart to finish
installing the update, rather than waiting for the third invocation (see `bug
1704855 <https://bugzilla.mozilla.org/show_bug.cgi?id=1704855>`__).

.. _background-updates-determining:

Determining whether background updates are possible
===================================================

Configuration
-------------

Updating Firefox, by definition, is an operation that applies to a Firefox
installation. However, Firefox configuration is generally done via preference
values and other files which are stored in a Firefox profile, and in general
profiles do not correspond 1:1 with installations. This raises the question of
how the configuration for something like the background updater should be
managed. We deal with this question in two different ways.

There are two main preferences specifically relevant to updates. Those
are ``app.update.auto``, which controls whether updates should be
downloaded automatically at all, even if Firefox is running, and
``app.update.background.enabled``, to specifically control whether to
use the background update system. We store these preferences in the
update root directory, which is located in a per-installation location
outside of any profile. Any profile loaded in that installation can
observe and control these settings.

But there are some other pieces of state which absolutely must come from a
profile, such as the telemetry client ID and logging level settings (see
`BackgroundTasksUtils.jsm <https://searchfox.org/mozilla-central/source/toolkit/components/backgroundtasks/BackgroundTasksUtils.jsm>`__).

This means that, in addition to our per-installation prefs, we also need
to be able to identify and load a profile. To do that, we leverage `the profile
service <https://searchfox.org/mozilla-central/source/toolkit/profile/nsIToolkitProfileService.idl>`__
to determine what the default profile for the installation would be if we were
running a normal browser session, and the background updater always uses it.

Criteria
--------

The default profile must satisfy several conditions in order for background
updates to be scheduled. None of these confounding factors are present in fully
default configurations, but some are relatively common. See
`BackgroundUpdate.REASON <https://searchfox.org/mozilla-central/search?q=symbol:BackgroundUpdate%23REASON>`__
for all the details.

In order for the background task to be scheduled:

-  The per-installation ``app.update.background.enabled`` pref must be
   true

-  The per-installation ``app.update.auto`` pref must be true (the
   default)

-  The installation must have been created by an installer executable and not by
   manually extracting an archive file

-  The current OS user must be capable of updating the installation based on its
   file system permissions, either by having permission to write to application
   files directly or by using the Mozilla Maintenance Service (which also
   requires that it be installed and enabled, as it is by default)

-  BITS must be enabled via ``app.update.BITS.enabled`` (the default)

-  Firefox proxy server settings must not be configured (the default)

-  ``app.update.langpack.enabled`` must be false, or otherwise there must be no
   langpacks installed. Background tasks cannot update addons such as langpacks,
   because they are installed into a profile, and langpacks that are not
   precisely matched with the version of Firefox that is installed can cause
   YSOD failures (see `bug 1647443 <https://bugzilla.mozilla.org/show_bug.cgi?id=1647443>`__),
   so background updating in the presence of langpacks is too risky.

If any per-installation prefs are changed while the default profile is not
running, the background update task will witness the changed prefs during its
next scheduled run, and exit if appropriate. The background task will not be
unscheduled at that point; that is delayed until a browser session is run with
the default profile (it should be possible for the background update task to
unschedule itself, but currently we prefer the simplicity of handling all
scheduling tasks from a single location).

In the extremely unusual case when prefs belonging to the default profile are
modified outside of Firefox (with a text editor, say), then the
background task will generally pick up those changes with no action needed,
because it will fish the changed settings directly from the profile.

.. _background-updates-scheduling:

Scheduling background tasks
===========================

We use OS-level scheduling mechanisms to schedule the command ``firefox
--backgroundtask backgroundupdate`` to run on a particular cadence. This cadence
is controlled by the ``app.update.background.interval`` preference, which
defaults to 7 hours.

On Windows, we use the `Task Scheduler
API <https://docs.microsoft.com/en-us/windows/win32/taskschd/task-scheduler-start-page>`__;
on macOS this will use
`launchd <https://developer.apple.com/library/archive/documentation/MacOSX/Conceptual/BPSystemStartup/Chapters/CreatingLaunchdJobs.html>`__.
For platform-specific scheduling details, see the
`TaskScheduler.jsm <https://searchfox.org/mozilla-central/source/toolkit/components/taskscheduler/TaskScheduler.jsm>`__
module.

These background tasks are scheduled per OS user and run with that user’s
permissions. No additional privileges are requested or needed, regardless of the
user account's status, because we have already verified that either the user has
all the permissions they need or that the Maintenance Service can be used.

Scheduling is done from within Firefox (or a background task) itself. To
reduce shared state, only the *default* Firefox profile will interact
with the OS-level task scheduling mechanism.

.. _background-updates-checking:

Checking for updates
====================

After verifying all the preconditions and exiting immediately if any do not
hold, the ``backgroundupdate`` task then verifies that it is the only Firefox
instance running (as determined by a multi-instance lock, see `bug
1553982 <https://bugzilla.mozilla.org/show_bug.cgi?id=1553982>`__), since
otherwise it would be unsafe to continue performing any update work.

The task then fishes configuration settings from the default profile, namely:

-  A subset of update specific preferences, such as ``app.update.log``

-  Data reporting preferences, to ensure the task respects the user’s choices

-  The (legacy) Telemetry client ID, so that background update Telemetry
   can be correlated with other Firefox Telemetry

The background task creates a distinct profile for itself to load, because a
profile must be present in order for most of the Firefox code that it relies on
to function.  This distinct profile is non-ephemeral, i.e., persistent, but not
visible to users: see `bug 1775132
<https://bugzilla.mozilla.org/show_bug.cgi?id=1775132>`__

After setting up this profile and reading all the configuration we need
into it, the regular
`UpdateService.jsm <https://searchfox.org/mozilla-central/source/toolkit/mozapps/update/UpdateService.jsm>`__
check process is initiated. To the greatest extent possible, this process is
identical to what happens during any regular browsing session.

Specific topics
===============

User interface
--------------

The background update task must not produce any user-visible interface. If it
did, whatever appeared would be \*disembodied\*, unconnected to any usage of
Firefox itself and appearing to a user as a weird, scary popup that came out of
nowhere. To this end, we disable all UI within the updater when invoking
from a background task. See `bug
1696276 <https://bugzilla.mozilla.org/show_bug.cgi?id=1696276>`__.

This point also means that we cannot prompt for user elevation (on Windows this
would mean a UAC prompt) from within the task, so we have to make very sure that
we will be able to perform an update without needing to elevate. By default on
Windows we are able to do this because of the presence of the Maintenance
Service, but it may be disabled or not installed, so we still have to check.

Staging
-------

The background update task will follow the update staging setting in the user’s
default profile. The default setting is to enable staging, so most users will
have it. In the future, background update tasks will recognize when an update
has been staged and try to restart to finalize the staged update (see `bug
1704855 <https://bugzilla.mozilla.org/show_bug.cgi?id=1704855>`__). Background
tasks cannot finalize a staged update in all cases however; for one example, see
`bug 1695797 <https://bugzilla.mozilla.org/show_bug.cgi?id=1695797>`__, where we
ensure that background tasks do not finalize a staged update while other
instances of the application are running.

Staging is enabled by default because it provides a marked improvement in
startup time for a browsing session. Without staging, browser startup following
retrieving an update would be blocked on extracting the update archive and
patching each individual application file. Staging does all of that in advance,
so that all that needs to be done to complete an update (and therefore all that
needs to be done during the startup path), is to move the already patched (that
is, staged) files into place, a much faster and less resource intensive job.
