.. _mozilla_projects_nss_nss_tools_sslstrength:

NSS Tools sslstrength
=====================

`sslstrength <#sslstrength>`__
------------------------------

.. container::

`Summary <#summary>`__
~~~~~~~~~~~~~~~~~~~~~~

.. container::

   A simple command-line client which connects to an SSL-server, and reports back the encryption
   cipher and strength used.

`Synopsis <#synopsis>`__
~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   1) sslstrength ciphers
   2) sslstrength hostname[:port] [ciphers=xyz] [debug] [verbose] [policy=export|domestic]

`Description <#description>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   The first form simple lists out the possible ciphers. The letter in the first column of the
   output is used to identify the cipher preferences in the ciphers=  command.
   The second form attempts to connect to the named ssl host. The hostname argument must be present.
   However, the port number is an optional argument, and if not given, will default to the https
   port (443).

   .. rubric:: Restricting Ciphers
      :name: restricting_ciphers

   By default, sslstrength assumes that all the preferences are on, so it will use any preferences
   in your policy. The enabled ciphersuites will always be printed out before the connection is
   made. If you want to test out a particular cipher, there are two ways to affect which ciphers are
   available. Firstly, you can set **policy** to be either domestic or export. This restricts the
   available ciphers to the same set used by Communicator. In addition to this, the **ciphers**
   command can be used to further restrict the ciphers available. The argument to the ciphers
   command is a string of characters, where each single character represents a cipher. You can
   obtain this list of character->cipher mappings by doing 'sslstrength ciphers'. For example,
   **    ciphers=bfi** will turn on these cipher preferences and turn off all others.

   **    policy=export** or **policy=domestic** will set your policies appropriately.

   | **    policy** will default to domestic if not specified.

   .. rubric:: Step-up
      :name: step-up

   Step up is a mode where the connection starts out with 40-bit encryption, but due to a
   'change-cipher-spec' handshake, changes to 128-bit encryption. This is only done in 'export
   mode', with servers with a special certificate. You can tell if you stepped-up, because the
   output will says 'using export policy', and you'll find the secret key size was 128-bits.

`Prerequisites <#prerequisites>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   You should have a cert7.db in the directory in which you run sslstrength.

`Other <#other>`__
~~~~~~~~~~~~~~~~~~

.. container::

   For references, here is a table of well-known SSL port numbers:

   ===== ===
   HTTPS 443
   IMAPS 993
   NNTPS 563
   ===== ===