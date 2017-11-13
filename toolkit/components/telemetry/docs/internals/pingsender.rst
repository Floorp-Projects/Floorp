Ping Sender
===========

The ping sender is a minimalistic program whose sole purpose is to deliver a
telemetry ping. It accepts the following parameters:

- the URL the ping will be sent to (as an HTTP POST command);
- the path to an uncompressed file holding the ping contents.

Once the ping has been read from disk the ping sender will try to post it once, exiting
with a non-zero value if it fails. If sending the ping succeeds then the ping file is removed.

The content of the HTTP request is *gzip* encoded. The request comes with a few
additional headers:

- ``User-Agent: pingsender/1.0``
- ``X-PingSender-Version: 1.0``. Even if this data is already included by the user agent, this
  header is needed as the pipeline is not currently storing use agent strings and doing that
  could require storing a fair chunk of redundant extra data. We need to discern between pings
  sent using the ping sender and the ones sent using the normal flow, at the end of the
  ingestion pipeline, for validation purposes.

.. note::

  The ping sender relies on libcurl for Linux and Mac build and on WinInet for
  Windows ones for its HTTP functionality. It currently ignores Firefox or the
  system proxy configuration.

In non-debug mode the ping sender doesn't print anything, not even on error,
this is done deliberately to prevent startling the user on architectures such
as Windows that would open a separate console window just to display the
program output. If you need runtime information to be printed out compile the
ping sender with debugging enabled.

The pingsender is not supported on Firefox for Android
(see `bug 1335917 <https://bugzilla.mozilla.org/show_bug.cgi?id=1335917>`_)
