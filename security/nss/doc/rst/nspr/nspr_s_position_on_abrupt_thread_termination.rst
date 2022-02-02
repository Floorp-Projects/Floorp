NSPR's position on abrupt thread termination
============================================

This memo describes my position on a facility that is currently under
discussion for inclusion in the NetScape Portable Runtime (NSPR); the
ability of a thread to abruptly exit. I resist including this function
in NSPR because it results in bad programming practice and unsupportable
programs.

   *Threads are not processes.*

Abrupt termination has been available in the UNIX/C environment for some
time (``exit()``), and I assume that the basic semantics defined there
are applicable here. In that environment, ``exit()`` may be called and
any time, and results in the calling thread's immediate termination. In
the situation where it was defined (UNIX), which has only a single
thread of execution, that is equivalent to terminating the process. The
process abstraction is then responsible for closing all open files and
reclaiming all storage that may have been allocated during the process'
lifetime.

This practice does not extend to threads. Threads run within the
confines of a process (or similar abstractions in other environments).
Threads are lightweight because they do not maintain the full protection
domain provided by a process. So in a threaded environment, what is the
parallel to UNIX' ``exit()``?

NSPR has defined a function, callable by any thread within a process at
any time, called ``PR_ProcessExit()``. This is identical to UNIX
``exit()`` and was so named in an effort to make the obvious even more
so. When called, the process exits, closing files and reclaiming the
process' storage.

Certain people have been disappointed when NSPR did not provide a
functional equivalent to exit just a particular thread. Apparently they
have failed to consider the ramifications. If a thread was to abruptly
terminate, there is no recording of what resources it owns and should
therefore be reclaimed. Those resources are in fact, owned by the
process and shared by all the threads within the process.

In the general course of events when programming with threads, it is
very advantageous for a thread to have resources that it and only it
knows about. In the natural course of events, these resources will be
allocated by a thread, used for some period of time, and then freed as
the stack unwinds. In these cases, the presence of the data is recorded
only on the stack, known only to the single thread (normally referred to
as *encapsulated*).

The problem with abrupt termination is that it can happen at any time,
to a thread that is coded correctly to handle both normal and
exceptional situations, but will be unable to do so since it will be
denied the opportunity to complete execution. It can happen because it
called out of its own scope into some lazily implemented library.

NSPR's answer to this is that there is no abrupt thread termination. All
threads must unwind and return from their root function. If they cannot,
because of some state corruption, then they must assume that the
corruption, like the state, is shared, and their only resource is for
the process to terminate.

To make this solution work requires that a function that encounters an
error be designed such that it first repairs its immediate state, and
then reports that error to its caller. If the caller cannot deal with
the failure, it must do the same. This process continues until the
thread either recovers from the malady or returns from the root
function. This is not all that difficult (though having done it a number
of times to already existing code, I will admit it isn't much fun
either).

The implementation of either strategy within the NSPR runtime is not
difficult. That is not what this memo is about. This is about providing
an API that coaxes people to do the right thing in as many ways as
possible. The existence of ``exit()`` in the UNIX/C environment is a
perfect example of how programmers will employ the most expediant
solution available. The definition of the language C is such that
returning from ``main()`` is a perfectly fine thing to do. But what
percentage of C programs actually bother? In UNIX, with its complex
definition of a protection domain, it happens to work (one might even
say it's more efficient) to exit from anywhere. But threads are not
processes. If threads have to maintain the same type of resource
knowledge as a process, they loose all of their benefit.

Threads are an implementation strategy to provide the illusion of
concurrency within a process. They are alternatives to large state
machines with mostly non-blocking library functions. When the latter is
used to provide concurrency, calling ``exit()`` will terminate the
entire process. Why would anyone expect a thread to behave differently?
Threads are not processes.
