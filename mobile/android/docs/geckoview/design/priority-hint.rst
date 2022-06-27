GeckoView Priority Hint API
===========================

Cathy Lu <calu@mozilla.com>, `Bug 1764998 <https://bugzilla.mozilla.org/show_bug.cgi?id=1764998>`_

May 2nd, 2022

Summary
-------

This document describes the API for setting a process to high priority by
applying a high priority hint. Instead of deducing the priority based on the
extension’s active priority, this will add an API to set it explicitly. 

Motivation
----------

This API will allow Glean metrics to be measured in order to compare
performance and stability metrics for process prioritization on vs off.
Previously, prioritization depended on whether or not a ``GeckoSession`` had a
surface associated with it, which lowered the priority of background tabs and
needed to be reloaded more often.

Goals
-----

Apps can set ``priorityHint`` on a ``GeckoSession``.

Existing Work
-------------

In `bug 1753700 <https://bugzilla.mozilla.org/show_bug.cgi?id=1753700>`_, we
added an API in dom/ipc to allow ``GeckoViewWebExtension`` to set a specific
``remoteTab``’s boolean ``priorityHint``. This allows tabs that do not have a
surface but are active according to web extension to have high priority.

Implementation
--------------

In ``GeckoSession``, add an API ``setPriorityHint`` that takes an integer as a
parameter. The priority int can be ``PRIORITY_DEFAULT`` or ``PRIORITY_HIGH``.
Specified and active tabs would be ``PRIORITY_HIGH``. The default would be
``PRIORITY_DEFAULT``. The API will dispatch an event
``GeckoView:SetPriorityHint``.

.. code:: java

  public void setPriorityHint(final @Priority int priorityHint)

Listeners in ``GeckoViewContent.jsm`` will set
``this.browser.frameLoader.remoteTab.priorityHint`` to the boolean passed in. 

.. code:: java

  case "GeckoView:setPriorityHint":
    if (this.browser.isRemoteBrowser) {
      let remoteTab = this.browser.frameLoader?.remoteTab;
      if (remoteTab) {
        remoteTab.renderLayers.priorityHint = val;
      }
    }
  break;

Additional Complexities
-----------------------

Apps that use this API will need to manually use the API to set the
priorityHint when the tab goes to foreground or background.
