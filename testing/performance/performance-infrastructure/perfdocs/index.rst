==========================
Performance Infrastructure
==========================

.. contents::
    :depth: 3


Performance tests all run on bare matel, or real hardware devices instead of virtual machines. This gives us more realistic performance metrics, and helps with decreasing the variability of our data. See below for information on what hardware is used for each of the platforms we test on, try run wait times, as well as guidelines for requesting new devices.


Platforms, and Hardware Used
----------------------------

At the moment, these are listed in the Mozilla Wiki which is `found here <https://wiki.mozilla.org/Performance/Platforms>`_ (see `bug 1883400 <https://bugzilla.mozilla.org/show_bug.cgi?id=1883400>`__ for progress on this).


Try Runs, and Wait Times
------------------------

Given that our tests run on hardware, there's a limited amount of devices that can be used to run them. This means that it's very likely that a try run (e.g. tests scheduled by :ref:`Mach Try Perf`) will be delayed waiting for capacity to free up. This limited capacity is also why there is a limit of 600 tasks that can be scheduled with ``mach try perf``.

Something to keep in mind is that try runs have a low priority, and our production branches (e.g. autoland/mozilla-central) have a higher priority. On days when there are more pushes to those branches, try runs will hit more delays. The platforms also have different capacities available to them which will change how long you have to wait for tests to start on them. **To find out how many tasks are currently pending, or running across all platforms** `see this redash query <https://sql.telemetry.mozilla.org/queries/98004#241985>`_ **or consult the graph below.**

.. raw:: html

   <iframe src="https://sql.telemetry.mozilla.org/embed/query/98004/visualization/241985?api_key=MNTnV4fwt3oblbx1uXDK9njIvDUa6rp1sla9RENT&" width="720" height="550"></iframe>

`See this dashboard for average wait times across all the platforms <https://sql.telemetry.mozilla.org/dashboard/average-performance-test-wait-times>`_.

In some cases, the tasks may hit a "task timeout" where they expire before they can run. This can be expected if there are a lot of other higher priority tasks being scheduled, so it's good to check the query mentioned above to see what the load looks like on the platform being targeted. However, there are situations where the test pools go offline or other issues occur. In general, for these task timeouts, reach out in `#perftest on Matrix <https://matrix.to/#/#perftest:mozilla.org>`__ to notify us about the issue as we might not be aware of it.


Requesting New Devices for Testing
----------------------------------

At times, it can be useful to test changes on a different device to due a lack of configuration coverage with our existing set of devices. It's simple to request/expense a device for local testing if you need one. However, for testing in continuous integration (CI) and/or in try runs, it can be quite expensive, and time-consuming to get a device ready for it. In general it takes a **few months** to get the device(s) running, and given contractual limitations, it may also reduce the availability of other devices. The work spans multiple teams as well so the time it takes depends on the current/future tasks that those teams have at the moment.

If a device is only required for a single issue, then it's recommended to order the device for local testing. If it's a long term project with multiple developers/teams that needs continuous monitoring, then it could be useful to have the devices in CI so they can be run on mozilla-central/autoland. This can also help ensure that your changes don't regress over time. Note that this should be planned ahead of time so that there's time to setup the devices before work on the project starts.

For long term projects, it's also good to determine how long these devices would be needed for, and if the device setup in CI needs to be adjusted to encompass these additional configurations. If the devices are no longer needed after the long term project completes, it might not be useful to have these devices running in CI, and using local devices would be quicker, and cheaper to get.

If you need to get a device for local testing, reach out to your manager to ask about expensing it. If you believe that you need to start testing on a new device in CI, please reach out to us in `#perftest on Matrix <https://matrix.to/#/#perftest:mozilla.org>`__. Alternatively, you can `file a bug here <https://bugzilla.mozilla.org/enter_bug.cgi?product=Testing&component=Performance&status_whiteboard=[fxp]>`_ for support on this.
