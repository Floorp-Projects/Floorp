.. _mozilla_projects_nss_key_log_format:

NSS Key Log Format
==================

.. container::

   Key logs can be written by NSS so that external programs can decrypt TLS connections. Wireshark
   1.6.0 and above can use these log files to decrypt packets. You can tell Wireshark where to find
   the key file via *Edit→Preferences→Protocols→TLS→(Pre)-Master-Secret log filename*.

   Key logging is enabled by setting the environment variable ``SSLKEYLOGFILE`` to point to a file.
   Note: starting with :ref:`mozilla_projects_nss_nss_3_24_release_notes` (used by Firefox 48 and 49
   only), the ``SSLKEYLOGFILE`` approach is disabled by default for optimized builds using the
   Makefile (those using gyp via ``build.sh`` are *not* affected). Distributors can re-enable it at
   compile time though (using the ``NSS_ALLOW_SSLKEYLOGFILE=1`` make variable) which is done for the
   official Firefox binaries. (See `bug
   1188657 <https://bugzilla.mozilla.org/show_bug.cgi?id=1188657>`__.) Notably, Debian does not have
   this option enabled, see `Debian bug
   842292 <https://bugs.debian.org/cgi-bin/bugreport.cgi?bug=842292>`__.

   This key log file is a series of lines. Comment lines begin with a sharp character ('#') and are
   ignored. Secrets follow the format ``<Label> <space> <ClientRandom> <space> <Secret>`` where:

   -  ``<Label>`` describes the following secret.
   -  ``<ClientRandom>`` is 32 bytes Random value from the Client Hello message, encoded as 64
      hexadecimal characters.
   -  ``<Secret>`` depends on the Label (see below).

   The following labels are defined, followed by a description of the secret:

   -  ``RSA``: 48 bytes for the premaster secret, encoded as 96 hexadecimal characters (removed in
      NSS 3.34)
   -  ``CLIENT_RANDOM``: 48 bytes for the master secret, encoded as 96 hexadecimal characters (for
      SSL 3.0, TLS 1.0, 1.1 and 1.2)
   -  ``CLIENT_EARLY_TRAFFIC_SECRET``: the hex-encoded early traffic secret for the client side (for
      TLS 1.3)
   -  ``CLIENT_HANDSHAKE_TRAFFIC_SECRET``: the hex-encoded handshake traffic secret for the client
      side (for TLS 1.3)
   -  ``SERVER_HANDSHAKE_TRAFFIC_SECRET``: the hex-encoded handshake traffic secret for the server
      side (for TLS 1.3)
   -  ``CLIENT_TRAFFIC_SECRET_0``: the first hex-encoded application traffic secret for the client
      side (for TLS 1.3)
   -  ``SERVER_TRAFFIC_SECRET_0``: the first hex-encoded application traffic secret for the server
      side (for TLS 1.3)
   -  ``EARLY_EXPORTER_SECRET``: the hex-encoded early exporter secret (for TLS 1.3).
   -  ``EXPORTER_SECRET``: the hex-encoded exporter secret (for TLS 1.3)

   The ``RSA`` form allows ciphersuites using RSA key-agreement to be logged and was the first form
   supported by Wireshark 1.6.0. It has been superseded by ``CLIENT_RANDOM`` which also works with
   other key-agreement algorithms (such as those based on Diffie-Hellman) and is supported since
   Wireshark 1.8.0.

   The TLS 1.3 lines are supported since NSS 3.34 (`bug
   1287711 <https://bugzilla.mozilla.org/show_bug.cgi?id=1287711>`__) and Wireshark 2.4
   (``EARLY_EXPORTER_SECRET`` exists since NSS 3.35, `bug
   1417331 <https://bugzilla.mozilla.org/show_bug.cgi?id=1417331>`__). The size of the hex-encoded
   secret depends on the selected cipher suite. It is 64, 96 or 128 characters for SHA256, SHA384 or
   SHA512 respectively.

   For Wireshark usage, see `TLS - Wireshark Wiki <https://wiki.wireshark.org/TLS>`__.
