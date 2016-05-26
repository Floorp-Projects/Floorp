.. _Subprocess:

=================
Supbrocess Module
=================

The Subprocess module allows a caller to spawn a native host executable, and
communicate with it asynchronously over its standard input and output pipes.

Processes are launched asynchronously ``Subprocess.call`` method, based
on the properties of a single options object. The method returns a promise
which resolves, once the process has successfully launched, to a ``Process``
object, which can be used to communicate with and control the process.

A simple Hello World invocation, which writes a message to a process, reads it
back, logs it, and waits for the process to exit looks something like:

.. code-block:: javascript

    let proc = await Subprocess.call({
      command: "/bin/cat",
    });

    proc.stdin.write("Hello World!");

    let result = await proc.stdout.readString();
    console.log(result);

    proc.stdin.close();
    let {exitCode} = await proc.wait();

Input and Output Redirection
============================

Communication with the child process happens entirely via one-way pipes tied
to its standard input, standard output, and standard error file descriptors.
While standard input and output are always redirected to pipes, standard error
is inherited from the parent process by default. Standard error can, however,
optionally be either redirected to its own pipe or merged into the standard
output pipe.

The module is designed primarily for use with processes following a strict
IO protocol, with predictable message sizes. Its read operations, therefore,
either complete after reading the exact amount of data specified, or do not
complete at all. For cases where this is not desirable, ``read()`` and
``readString`` may be called without any length argument, and will return a
chunk of data of an arbitrary size.


Process and Pipe Lifecycles
===========================

Once the process exits, any buffered data from its output pipes may still be
read until the pipe is explicitly closed. Unless the pipe is explicitly
closed, however, any pending buffered data *must* be read from the pipe, or
the resources associated with the pipe will not be freed.

Beyond this, no explicit cleanup is required for either processes or their
pipes. So long as the caller ensures that the process exits, and there is no
pending input to be read on its ``stdout`` or ``stderr`` pipes, all resources
will be freed automatically.

The preferred way to ensure that a process exits is to close its input pipe
and wait for it to exit gracefully. Processes which haven't exited gracefully
by shutdown time, however, must be forcibly terminated:

.. code-block:: javascript

    let proc = await Subprocess.call({
      command: "/usr/bin/subprocess.py",
    });

    // Kill the process if it hasn't gracefully exited by shutdown time.
    let blocker = () => proc.kill();

    AsyncShutdown.profileBeforeChange.addBlocker(
      "Subprocess: Killing hung process",
      blocker);

    proc.wait().then(() => {
      // Remove the shutdown blocker once we've exited.
      AsyncShutdown.profileBeforeChange.removeBlocker(blocker);

      // Close standard output, in case there's any buffered data we haven't read.
      proc.stdout.close();
    });

    // Send a message to the process, and close stdin, so the process knows to
    // exit.
    proc.stdin.write(message);
    proc.stdin.close();

In the simpler case of a short-running process which takes no input, and exits
immediately after producing output, it's generally enough to simply read its
output stream until EOF:

.. code-block:: javascript

    let proc = await Subprocess.call({
      command: await Subprocess.pathSearch("ifconfig"),
    });

    // Read all of the process output.
    let result = "";
    let string;
    while ((string = await proc.stdout.readString())) {
      result += string;
    }
    console.log(result);

    // The output pipe is closed and no buffered data remains to be read.
    // This means the process has exited, and no further cleanup is necessary.


Bidirectional IO
================

When performing bidirectional IO, special care needs to be taken to avoid
deadlocks. While all IO operations in the Subprocess API are asynchronous,
careless ordering of operations can still lead to a state where both processes
are blocked on a read or write operation at the same time. For example,

.. code-block:: javascript

    let proc = await Subprocess.call({
      command: "/bin/cat",
    });

    let size = 1024 * 1024;
    await proc.stdin.write(new ArrayBuffer(size));

    let result = await proc.stdout.read(size);

The code attempts to write 1MB of data to an input pipe, and then read it back
from the output pipe. Because the data is big enough to fill both the input
and output pipe buffers, though, and because the code waits for the write
operation to complete before attempting any reads, the ``cat`` process will
block trying to write to its output indefinitely, and never finish reading the
data from its standard input.

In order to avoid the deadlock, we need to avoid blocking on the write
operation:

.. code-block:: javascript

    let size = 1024 * 1024;
    proc.stdin.write(new ArrayBuffer(size));

    let result = await proc.stdout.read(size);

There is no silver bullet to avoiding deadlocks in this type of situation,
though. Any input operations that depend on output operations, or vice versa,
have the possibility of triggering deadlocks, and need to be thought out
carefully.

Arguments
=========

Arguments may be passed to the process in the form an array of strings.
Arguments are never split, or subjected to any sort of shell expansion, so the
target process will receive the exact arguments array as passed to
``Subprocess.call``. Argument 0 will always be the full path to the
executable, as passed via the ``command`` argument:

.. code-block:: javascript

    let proc = await Subprocess.call({
      command: "/bin/sh",
      arguments: ["-c", "echo -n $0"],
    });

    let output = await proc.stdout.readString();
    assert(output === "/bin/sh");


Process Environment
===================

By default, the process is launched with the same environment variables and
working directory as the parent process, but either can be changed if
necessary. The working directory may be changed simply by passing a
``workdir`` option:

.. code-block:: javascript

    let proc = await Subprocess.call({
      command: "/bin/pwd",
      workdir: "/tmp",
    });

    let output = await proc.stdout.readString();
    assert(output === "/tmp\n");

The process's environment variables can be changed using the ``environment``
and ``environmentAppend`` options. By default, passing an ``environment``
object replaces the process's entire environment with the properties in that
object:

.. code-block:: javascript

    let proc = await Subprocess.call({
      command: "/bin/pwd",
      environment: {FOO: "BAR"},
    });

    let output = await proc.stdout.readString();
    assert(output === "FOO=BAR\n");

In order to add variables to, or change variables from, the current set of
environment variables, the ``environmentAppend`` object must be passed in
addition:

.. code-block:: javascript

    let proc = await Subprocess.call({
      command: "/bin/pwd",
      environment: {FOO: "BAR"},
      environmentAppend: true,
    });

    let output = "";
    while ((string = await proc.stdout.readString())) {
      output += string;
    }

    assert(output.includes("FOO=BAR\n"));

