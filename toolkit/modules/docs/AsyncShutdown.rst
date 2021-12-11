.. _AsyncShutdown:

==============
AsyncShutdown
==============

During shutdown of the process, subsystems are closed one after another. ``AsyncShutdown`` is a module dedicated to express shutdown-time dependencies between:

- services and their clients;
- shutdown phases (e.g. profile-before-change) and their clients.

.. _AsyncShutdown_Barriers:

Barriers: Expressing shutdown dependencies towards a service
============================================================

Consider a service FooService. At some point during the shutdown of the process, this service needs to:

- inform its clients that it is about to shut down;
- wait until the clients have completed their final operations based on FooService (often asynchronously);
- only then shut itself down.

This may be expressed as an instance of ``AsyncShutdown.Barrier``. An instance of ``AsyncShutdown.Barrier`` provides:

- a capability ``client`` that may be published to clients, to let them register or unregister blockers;
- methods for the owner of the barrier to let it consult the state of blockers and wait until all client-registered blockers have been resolved.

Shutdown timeouts
-----------------

By design, an instance of ``AsyncShutdown.Barrier`` will cause a crash
if it takes more than 60 seconds `awake` for its clients to lift or
remove their blockers (`awake` meaning that seconds during which the
computer is asleep or too busy to do anything are not counted). This
mechanism helps ensure that we do not leave the process in a state in
which it can neither proceed with shutdown nor be relaunched.

If the CrashReporter is enabled, this crash will report:

- the name of the barrier that failed;
- for each blocker that has not been released yet:

  - the name of the blocker;
  - the state of the blocker, if a state function has been provided (see :ref:`AsyncShutdown.Barrier.state`).

Example 1: Simple Barrier client
--------------------------------

The following snippet presents an example of a client of FooService that has a shutdown dependency upon FooService. In this case, the client wishes to ensure that FooService is not shutdown before some state has been reached. An example is clients that need write data asynchronously and need to ensure that they have fully written their state to disk before shutdown, even if due to some user manipulation shutdown takes place immediately.

.. code-block:: javascript

    // Some client of FooService called FooClient

    Components.utils.import("resource://gre/modules/FooService.jsm", this);

    // FooService.shutdown is the `client` capability of a `Barrier`.
    // See example 2 for the definition of `FooService.shutdown`
    FooService.shutdown.addBlocker(
      "FooClient: Need to make sure that we have reached some state",
      () => promiseReachedSomeState
    );
    // promiseReachedSomeState should be an instance of Promise resolved once
    // we have reached the expected state

Example 2: Simple Barrier owner
-------------------------------

The following snippet presents an example of a service FooService that
wishes to ensure that all clients have had a chance to complete any
outstanding operations before FooService shuts down.

.. code-block:: javascript

    // Module FooService

    Components.utils.import("resource://gre/modules/AsyncShutdown.jsm", this);

    this.exports = ["FooService"];

    let shutdown = new AsyncShutdown.Barrier("FooService: Waiting for clients before shutting down");

    // Export the `client` capability, to let clients register shutdown blockers
    FooService.shutdown = shutdown.client;

    // This function should be triggered at some point during shutdown, generally
    // as a client to another Barrier or Phase. Triggering this function is not covered
    // in this snippet.
    let onshutdown = async function() {
      // Wait for all registered clients to have lifted the barrier
      await shutdown.wait();

      // Now deactivate FooService itself.
      // ...
    });

Frequently, a service that owns a ``AsyncShutdown.Barrier`` is itself a client of another Barrier.

.. _AsyncShutdown.Barrier.state:

Example 3: More sophisticated Barrier client
--------------------------------------------

The following snippet presents FooClient2, a more sophisticated client of FooService that needs to perform a number of operations during shutdown but before the shutdown of FooService. Also, given that this client is more sophisticated, we provide a function returning the state of FooClient2 during shutdown. If for some reason FooClient2's blocker is never lifted, this state can be reported as part of a crash report.

.. code-block:: javascript

    // Some client of FooService called FooClient2

    Components.utils.import("resource://gre/modules/FooService.jsm", this);

    FooService.shutdown.addBlocker(
      "FooClient2: Collecting data, writing it to disk and shutting down",
      () => Blocker.wait(),
      () => Blocker.state
    );

    let Blocker = {
      // This field contains information on the status of the blocker.
      // It can be any JSON serializable object.
      state: "Not started",

      async wait() {
        // This method is called once FooService starts informing its clients that
        // FooService wishes to shut down.

        // Update the state as we go. If the Barrier is used in conjunction with
        // a Phase, this state will be reported as part of a crash report if FooClient fails
        // to shutdown properly.
        this.state = "Starting";

        let data = await collectSomeData();
        this.state = "Data collection complete";

        try {
          await writeSomeDataToDisk(data);
          this.state = "Data successfully written to disk";
        } catch (ex) {
          this.state = "Writing data to disk failed, proceeding with shutdown: " + ex;
        }

        await FooService.oneLastCall();
        this.state = "Ready";
      }
    };


Example 4: A service with both internal and external dependencies
-----------------------------------------------------------------

 .. code-block:: javascript

    // Module FooService2

    let { AsyncShutdown } = Components.utils.import("resource://gre/modules/AsyncShutdown.jsm");
    let { PromiseUtils } = Components.utils.import("resource://gre/modules/PromiseUtils.jsm");

    this.exports = ["FooService2"];

    let shutdown = new AsyncShutdown.Barrier("FooService2: Waiting for clients before shutting down");

    // Export the `client` capability, to let clients register shutdown blockers
    FooService2.shutdown = shutdown.client;

    // A second barrier, used to avoid shutting down while any connections are open.
    let connections = new AsyncShutdown.Barrier("FooService2: Waiting for all FooConnections to be closed before shutting down");

    let isClosed = false;

    FooService2.openFooConnection = function(name) {
      if (isClosed) {
        throw new Error("FooService2 is closed");
      }

      let deferred = PromiseUtils.defer();
      connections.client.addBlocker("FooService2: Waiting for connection " + name + " to close",  deferred.promise);

      // ...


      return {
        // ...
        // Some FooConnection object. Presumably, it will have additional methods.
        // ...
        close: function() {
          // ...
          // Perform any operation necessary for closing
          // ...

          // Don't hoard blockers.
          connections.client.removeBlocker(deferred.promise);

          // The barrier MUST be lifted, even if removeBlocker has been called.
          deferred.resolve();
        }
      };
    };


    // This function should be triggered at some point during shutdown, generally
    // as a client to another Barrier. Triggering this function is not covered
    // in this snippet.
    let onshutdown = async function() {
      // Wait for all registered clients to have lifted the barrier.
      // These clients may open instances of FooConnection if they need to.
      await shutdown.wait();

      // Now stop accepting any other connection request.
      isClosed = true;

      // Wait for all instances of FooConnection to be closed.
      await connections.wait();

      // Now finish shutting down FooService2
      // ...
    });

.. _AsyncShutdown_phases:

Phases: Expressing dependencies towards phases of shutdown
==========================================================

The shutdown of a process takes place by phase, such as:

- ``profileBeforeChange`` (once this phase is complete, there is no guarantee that the process has access to a profile directory);
- ``webWorkersShutdown`` (once this phase is complete, JavaScript does not have access to workers anymore);
- ...

Much as services, phases have clients. For instance, all users of web workers MUST have finished using their web workers before the end of phase ``webWorkersShutdown``.

Module ``AsyncShutdown`` provides pre-defined barriers for a set of
well-known phases. Each of the barriers provided blocks the corresponding shutdown
phase until all clients have lifted their blockers.

List of phases
--------------

``AsyncShutdown.profileChangeTeardown``

  The client capability for clients wishing to block asynchronously
  during observer notification "profile-change-teardown".


``AsyncShutdown.profileBeforeChange``

  The client capability for clients wishing to block asynchronously
  during observer notification "profile-change-teardown". Once the
  barrier is resolved, clients other than Telemetry MUST NOT access
  files in the profile directory and clients MUST NOT use Telemetry
  anymore.

``AsyncShutdown.sendTelemetry``

  The client capability for clients wishing to block asynchronously
  during observer notification "profile-before-change-telemetry".
  Once the barrier is resolved, Telemetry must stop its operations.

``AsyncShutdown.webWorkersShutdown``

  The client capability for clients wishing to block asynchronously
  during observer notification "web-workers-shutdown". Once the phase
  is complete, clients MUST NOT use web workers.
