.. -*- Mode: rst; fill-column: 100; -*-

===================================
 Shutting down Firefox for Android
===================================

Background
==========

Normally, apps on Android don't need to provide any support for explicit quitting, instead they are
just sent into the background where they eventually get killed by the OS's low-memory killer.

Nevertheless, Firefox on Android allows explicit quitting to support the use case of users wanting
to clear part or all of their private data after finishing a browsing session. When this option to
"Clear private data on exit" is activated from the settings, a "Quit" button is provided in the menu.

Because Firefox on Android uses a native UI (written in Java), which also holds some of the user's
browsing data, this creates some additional complications when compared to quitting on desktop.

Technical details
=================

When the "Quit" button is used, the UI sends a ``Browser:Quit`` notification to Gecko's ``BrowserApp``,
which initiates the normal Gecko shutdown procedure. At the same time however, the native UI needs to
shutdown as well, so as to

1) provide an immediate visual feedback to the user that Firefox is indeed quitting

2) avoid a state where the UI is still running "normally" while the rendering engine is already
   shutting down, which could lead to loosing incoming external tabs if they were to arrive within
   that period.

Therefore, shutdown of the native UI was originally started simultaneously with notifying Gecko.
Because the clearing of private data during shutdown is handled by Gecko's ``Sanitizer``, while some
private data, e.g. the browsing history, is held in a database by the native UI, this means that
Gecko needs to message the native UI during shutdown if the user wants the browsing history to be
cleared on quitting.
Shutting down the UI simultaneously with Gecko therefore introduced a race condition where the data
clearing could fail because the native UI thread responsible for receiving Gecko's sanitization
messages had already exited by the time Gecko's ``Sanitizer`` was attempting to e.g. clear the
user's browsing history (for further reading, compare `bug 1266594
<https://bugzilla.mozilla.org/show_bug.cgi?id=1266594>`_).

To fix this issue, the native UI (in ``GeckoApp``) now waits for the ``Sanitizer`` to run and
message all necessary sanitization handlers and only starts its shutdown after receiving a
``Sanitize:Finished`` message with a ``shutdown: true`` parameter set. While this introduces a
certain delay in closing the UI, it is still faster than having to wait for Gecko to exit completely
before starting to close the UI.

Currently, quitting Firefox therefore proceeds roughly as follows:

1) The user presses the "Quit" button in the main menu, which sends a ``Browser:Quit`` notification
   to ``BrowserApp``. This notification also contains additional parameters indicating which types
   of private user data - if any - to clear during shutdown.

2) ``BrowserApp.quit`` runs, which initiates Gecko shutdown by sending out a
   ``quit-application-requested`` notification.

3) If nobody cancelled shutdown in response to the ``quit-application-requested`` notification,
   quitting proceeds and the ``SessionStore`` enters shutdown mode (``STATE_QUITTING``), which
   basically means that no new (asynchronous) writes are started to prevent any interference with
   the final flushing of data.

4) ``BrowserApp`` calls the ``Sanitizer`` to clear up any private user data that might need cleaning.
   After the ``Sanitizer`` has invoked all required sanitization handlers (including any on the
   native Java UI side, e.g. for the browsing history) and finished running, it sends a
   ``Sanitize:Finished`` message back to the native UI.

5) On receiving the ``Sanitize:Finished`` message, ``GeckoApp`` starts the shutdown of the native UI
   as well by calling ``doShutdown()``.

6) After sending the ``Sanitize:Finished`` message, Gecko's ``Sanitizer`` runs the callback provided
   by ``BrowserApp.quit``, which is ``appStartup.quit(Ci.nsIAppStartup.eForceQuit)``, thereby
   starting the actual and final shutting down of Gecko.

7) On receiving the final ``quit-application`` notification, the ``SessionStore`` synchronously
   writes its current state to disk.
