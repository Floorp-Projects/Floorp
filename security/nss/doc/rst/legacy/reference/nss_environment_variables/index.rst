.. _mozilla_projects_nss_reference_nss_environment_variables:

NSS environment variables
=========================

.. container::

   .. note::

      **Note: NSS Environment Variables are subject to be changed and/or removed from NSS.**

.. _run-time_environment_variables:

`Run-Time Environment Variables <#run-time_environment_variables>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   These environment variables affect the RUN TIME behavior of NSS shared libraries. There is a
   separate set of environment variables that affect how NSS is built, documented below.

   +------------------------+------------------------+------------------------+------------------------+
   | Variable               | Type                   | Description            | Introduced in version  |
   +------------------------+------------------------+------------------------+------------------------+
   | ``NSRANDCOUNT``        | Integer                | Sets the maximum       | 3.12.3                 |
   |                        | (byte count)           | number of bytes to     |                        |
   |                        |                        | read from the file     |                        |
   |                        |                        | named in the           |                        |
   |                        |                        | environment variable   |                        |
   |                        |                        | NSRANDFILE (see        |                        |
   |                        |                        | below).  Makes         |                        |
   |                        |                        | NSRANDFILE usable with |                        |
   |                        |                        | /dev/urandom.          |                        |
   +------------------------+------------------------+------------------------+------------------------+
   | ``NSRANDFILE``         | String                 | Uses this file to seed | Before 3.0             |
   |                        | (file name)            | the Pseudo Random      |                        |
   |                        |                        | Number Generator.      |                        |
   +------------------------+------------------------+------------------------+------------------------+
   | ``NSS_ALLO             | Boolean                | Enables the use of MD2 | 3.12.3                 |
   | W_WEAK_SIGNATURE_ALG`` | (any non-empty value   | and MD4 inside         |                        |
   |                        | to enable)             | signatures. This was   |                        |
   |                        |                        | allowed by default     |                        |
   |                        |                        | before NSS 3.12.3.     |                        |
   +------------------------+------------------------+------------------------+------------------------+
   | ``NSS                  | String                 | Name the PKCS#11       | 3.6                    |
   | _DEBUG_PKCS11_MODULE`` | (module name)          | module to be traced.   |                        |
   |                        |                        | :ref:`mozilla          |                        |
   |                        |                        | _projects_nss_nss_tech |                        |
   |                        |                        | _notes_nss_tech_note2` |                        |
   +------------------------+------------------------+------------------------+------------------------+
   | `                      | String                 | Determines the default | 3.12                   |
   | `NSS_DEFAULT_DB_TYPE`` | ("dbm", "sql", or      | Database type to open  |                        |
   |                        | "extern")              | if the app does not    |                        |
   |                        |                        | specify.               |                        |
   |                        |                        | `NSS_Shared_D          |                        |
   |                        |                        | B <http://wiki.mozilla |                        |
   |                        |                        | .org/NSS_Shared_DB>`__ |                        |
   +------------------------+------------------------+------------------------+------------------------+
   | ``NSS_DIS              | String                 | Define this variable   | 3.4                    |
   | ABLE_ARENA_FREE_LIST`` | (any non-empty value)  | to get accurate leak   |                        |
   |                        |                        | allocation stacks when |                        |
   |                        |                        | using leak reporting   |                        |
   |                        |                        | software.              |                        |
   |                        |                        | :                      |                        |
   |                        |                        | ref:`mozilla_projects_ |                        |
   |                        |                        | nss_memory_allocation` |                        |
   +------------------------+------------------------+------------------------+------------------------+
   | ``NSS_DISABLE_UNLOAD`` | String                 | Disable unloading of   | 3.11.8                 |
   |                        | (any non-empty value)  | dynamically loaded NSS |                        |
   |                        |                        | shared libraries       |                        |
   |                        |                        | during shutdown.       |                        |
   |                        |                        | Necessary on some      |                        |
   |                        |                        | platforms to get       |                        |
   |                        |                        | correct function names |                        |
   |                        |                        | when using leak        |                        |
   |                        |                        | reporting software.    |                        |
   +------------------------+------------------------+------------------------+------------------------+
   | ``NSS_ENABLE_AUDIT``   | Boolean                | Enable auditing of     | 3.11.2                 |
   |                        | (1 to enable)          | activities of the NSS  |                        |
   |                        |                        | cryptographic module   |                        |
   |                        |                        | in FIPS mode. `Audit   |                        |
   |                        |                        | Data <http://wiki.     |                        |
   |                        |                        | mozilla.org/FIPS_Opera |                        |
   |                        |                        | tional_Environment>`__ |                        |
   +------------------------+------------------------+------------------------+------------------------+
   | ``NS                   | Boolean                | Use libPKIX, rather    | 3.12                   |
   | S_ENABLE_PKIX_VERIFY`` | (any non-empty value   | than the old cert      |                        |
   |                        | to enable)             | library, to verify     |                        |
   |                        |                        | certificates.          |                        |
   +------------------------+------------------------+------------------------+------------------------+
   | ``NSS_FIPS``           | String                 | Will start NSS in FIPS | 3.12.5                 |
   |                        | ("                     | mode.                  |                        |
   |                        | fips","true","on","1") |                        |                        |
   +------------------------+------------------------+------------------------+------------------------+
   | ``                     | String                 | Specifies agorithms    | 3.12.3                 |
   | NSS_HASH_ALG_SUPPORT`` |                        | allowed to be used in  |                        |
   |                        |                        | certain applications,  |                        |
   |                        |                        | such as in signatures  |                        |
   |                        |                        | on certificates and    |                        |
   |                        |                        | CRLs. See              |                        |
   |                        |                        | documentation at `this |                        |
   |                        |                        | link <https://bugzill  |                        |
   |                        |                        | a.mozilla.org/show_bug |                        |
   |                        |                        | .cgi?id=483113#c0>`__. |                        |
   +------------------------+------------------------+------------------------+------------------------+
   | ``NSS_OUTPUT_FILE``    | String                 | Output file path name  | 3.7                    |
   |                        | (filename)             | for the                |                        |
   |                        |                        | :ref:`mozilla_         |                        |
   |                        |                        | projects_nss_nss_tech_ |                        |
   |                        |                        | notes_nss_tech_note2`. |                        |
   |                        |                        | Default is stdout.     |                        |
   +------------------------+------------------------+------------------------+------------------------+
   | ``NSS_SDB_USE_CACHE``  | String                 | Controls whether NSS   | 3.12                   |
   |                        | ("no","yes","auto")    | uses a local cache of  |                        |
   |                        |                        | SQL database contents. |                        |
   |                        |                        | Default is "auto". See |                        |
   |                        |                        | `the                   |                        |
   |                        |                        | source <http://bonsai  |                        |
   |                        |                        | .mozilla.org/cvsblame. |                        |
   |                        |                        | cgi?file=/mozilla/secu |                        |
   |                        |                        | rity/nss/lib/softoken/ |                        |
   |                        |                        | sdb.c&rev=1.6#1797>`__ |                        |
   |                        |                        | for more information.  |                        |
   +------------------------+------------------------+------------------------+------------------------+
   | `NS                    | String ("0", "1")      | Controls the           |                        |
   | S_SSL_CBC_RANDOM_IV <h |                        | workaround for the     |                        |
   | ttps://dxr.mozilla.org |                        | `BEAST <https          |                        |
   | /security/search?q=NSS |                        | ://en.wikipedia.org/wi |                        |
   | _SSL_CBC_RANDOM_IV>`__ |                        | ki/Transport_Layer_Sec |                        |
   |                        |                        | urity#BEAST_attack>`__ |                        |
   |                        |                        | attack on SSL 3.0 and  |                        |
   |                        |                        | TLS 1.0. "0" disables  |                        |
   |                        |                        | it, "1" enables it. It |                        |
   |                        |                        | is also known as 1/n-1 |                        |
   |                        |                        | record splitting.      |                        |
   |                        |                        | Default is "1".        |                        |
   +------------------------+------------------------+------------------------+------------------------+
   | ``NSS_SSL_             | String                 | (Definition for NSS    | 3.12.5                 |
   | ENABLE_RENEGOTIATION`` | ([0|n|N],              | 3.12.6 and above)      | Modified in 3.12.6     |
   |                        | [1|u|U],               | Sets how TLS           |                        |
   |                        | [2|r|R],               | renegotiation is       |                        |
   |                        | [3|t|T])               | handled                |                        |
   |                        |                        |                        |                        |
   |                        |                        | -  [1|u|U]:            |                        |
   |                        |                        |    SSL_RE              |                        |
   |                        |                        | NEGOTIATE_UNRESTRICTED |                        |
   |                        |                        |                        |                        |
   |                        |                        | | Server and client    |                        |
   |                        |                        |   are allowed to       |                        |
   |                        |                        |   renegotiate without  |                        |
   |                        |                        |   any restrictions.    |                        |
   |                        |                        | | This setting was the |                        |
   |                        |                        |   default prior 3.12.5 |                        |
   |                        |                        |   and makes products   |                        |
   |                        |                        |   vulnerable.          |                        |
   |                        |                        |                        |                        |
   |                        |                        | -  [0|n|N]:            |                        |
   |                        |                        |                        |                        |
   |                        |                        |  SSL_RENEGOTIATE_NEVER |                        |
   |                        |                        |                        |                        |
   |                        |                        | Never allow            |                        |
   |                        |                        | renegotiation - That   |                        |
   |                        |                        | was the default for    |                        |
   |                        |                        | 3.12.5 release.        |                        |
   |                        |                        |                        |                        |
   |                        |                        | -  [3|t|T]:            |                        |
   |                        |                        |    SSL_RE              |                        |
   |                        |                        | NEGOTIATE_TRANSITIONAL |                        |
   |                        |                        |                        |                        |
   |                        |                        | Disallows unsafe       |                        |
   |                        |                        | renegotiation in       |                        |
   |                        |                        | server sockets only,   |                        |
   |                        |                        | but allows clients to  |                        |
   |                        |                        | continue to            |                        |
   |                        |                        | renegotiate with       |                        |
   |                        |                        | vulnerable servers.    |                        |
   |                        |                        | This value should only |                        |
   |                        |                        | be used during the     |                        |
   |                        |                        | transition period when |                        |
   |                        |                        | few servers have been  |                        |
   |                        |                        | upgraded.              |                        |
   |                        |                        |                        |                        |
   |                        |                        | -  [2|r|R]:            |                        |
   |                        |                        |    SSL_RE              |                        |
   |                        |                        | NEGOTIATE_REQUIRES_XTN |                        |
   |                        |                        |    (default)           |                        |
   |                        |                        |                        |                        |
   |                        |                        | | Only allows          |                        |
   |                        |                        |   renegotiation if the |                        |
   |                        |                        |   peer's hello bears   |                        |
   |                        |                        |   the TLS              |                        |
   |                        |                        |   renegotiation_info   |                        |
   |                        |                        |   extension.           |                        |
   |                        |                        | | This is the safe     |                        |
   |                        |                        |   renegotiation.       |                        |
   +------------------------+------------------------+------------------------+------------------------+
   | ``NSS_SSL_REQU         | Boolean                | It controls whether    | 3.12.5                 |
   | IRE_SAFE_NEGOTIATION`` | (1 to enable)          | safe renegotiation     |                        |
   |                        |                        | indication is required |                        |
   |                        |                        | for initial handshake. |                        |
   |                        |                        | In other words a       |                        |
   |                        |                        | connection will be     |                        |
   |                        |                        | dropped at initial     |                        |
   |                        |                        | handshake if a server  |                        |
   |                        |                        | or client do not       |                        |
   |                        |                        | support safe           |                        |
   |                        |                        | renegotiation. The     |                        |
   |                        |                        | default setting for    |                        |
   |                        |                        | this option is FALSE.  |                        |
   +------------------------+------------------------+------------------------+------------------------+
   | ``NSS_SSL_SERVER       | Integer                | Timeout time to detect | 3.4                    |
   | _CACHE_MUTEX_TIMEOUT`` | (seconds)              | dead or hung process   |                        |
   |                        |                        | in multi-process SSL   |                        |
   |                        |                        | server. Default is 30  |                        |
   |                        |                        | seconds.               |                        |
   +------------------------+------------------------+------------------------+------------------------+
   | ``NSS_STRICT_NOFORK``  | String                 | It is an error to try  | 3.12.3                 |
   |                        | ("1",                  | to use a PKCS#11       |                        |
   |                        | "DISABLED",            | crypto module in a     |                        |
   |                        | or any other non-empty | process before it has  |                        |
   |                        | value)                 | been initialized in    |                        |
   |                        |                        | that process, even if  |                        |
   |                        |                        | the module was         |                        |
   |                        |                        | initialized in the     |                        |
   |                        |                        | parent process.        |                        |
   |                        |                        | Beginning in NSS       |                        |
   |                        |                        | 3.12.3, Softoken will  |                        |
   |                        |                        | detect this error.     |                        |
   |                        |                        | This environment       |                        |
   |                        |                        | variable controls      |                        |
   |                        |                        | Softoken's response to |                        |
   |                        |                        | that error.            |                        |
   |                        |                        |                        |                        |
   |                        |                        | -  If set to "1" or    |                        |
   |                        |                        |    unset, Softoken     |                        |
   |                        |                        |    will trigger an     |                        |
   |                        |                        |    assertion failure   |                        |
   |                        |                        |    in debug builds,    |                        |
   |                        |                        |    and will report an  |                        |
   |                        |                        |    error in non-DEBUG  |                        |
   |                        |                        |    builds.             |                        |
   |                        |                        | -  If set  to          |                        |
   |                        |                        |    "DISABLED",         |                        |
   |                        |                        |    Softoken will       |                        |
   |                        |                        |    ignore forks, and   |                        |
   |                        |                        |    behave as it did in |                        |
   |                        |                        |    older versions.     |                        |
   |                        |                        | -  If set to any other |                        |
   |                        |                        |    non-empty value,    |                        |
   |                        |                        |    Softoken will       |                        |
   |                        |                        |    report an error in  |                        |
   |                        |                        |    both DEBUG and      |                        |
   |                        |                        |    non-DEBUG builds.   |                        |
   +------------------------+------------------------+------------------------+------------------------+
   | `                      | String                 | will trigger an        | 3.5                    |
   | `NSS_STRICT_SHUTDOWN`` | (any non-empty value)  | assertion failure in   |                        |
   |                        |                        | debug builds when a    |                        |
   |                        |                        | program tries to       |                        |
   |                        |                        | shutdown NSS before    |                        |
   |                        |                        | freeing all the        |                        |
   |                        |                        | resources it acquired  |                        |
   |                        |                        | from NSS while NSS was |                        |
   |                        |                        | initialized.           |                        |
   +------------------------+------------------------+------------------------+------------------------+
   | ``NSS_TRACE_OCSP``     | Boolean                | Enables OCSP tracing.  | 3.12                   |
   |                        | (any value to enable)  | The trace information  |                        |
   |                        |                        | is written to the file |                        |
   |                        |                        | pointed by             |                        |
   |                        |                        | NSPR_LOG_FILE (default |                        |
   |                        |                        | stderr). See `NSS      |                        |
   |                        |                        | trac                   |                        |
   |                        |                        | ing <http://wiki.mozil |                        |
   |                        |                        | la.org/NSS:Tracing>`__ |                        |
   +------------------------+------------------------+------------------------+------------------------+
   | ``NSS_USE_             | Boolean                | Tells NSS to send EC   | 3.12.3                 |
   | DECODED_CKA_EC_POINT`` | (any value to enable)  | key points across the  |                        |
   |                        |                        | PKCS#11 interface in   |                        |
   |                        |                        | the non-standard       |                        |
   |                        |                        | unencoded format that  |                        |
   |                        |                        | was used by default    |                        |
   |                        |                        | before NSS 3.12.3.     |                        |
   +------------------------+------------------------+------------------------+------------------------+
   | ``NSS_US               | Boolean                | Tells NSS to allow     | 3.12.3                 |
   | E_SHEXP_IN_CERT_NAME`` | (any value to enable)  | shell-style wildcard   |                        |
   |                        |                        | patterns in            |                        |
   |                        |                        | certificates to match  |                        |
   |                        |                        | SSL server host names. |                        |
   |                        |                        | This behavior was the  |                        |
   |                        |                        | default before NSS     |                        |
   |                        |                        | 3.12.3.                |                        |
   +------------------------+------------------------+------------------------+------------------------+
   | ``PKIX_OBJECT_LEA      | String                 | Debug variable for     | 3.12                   |
   | K_TEST_ABORT_ON_LEAK`` | (any non-empty value)  | PKIX leak checking.    |                        |
   |                        |                        | Note: *The code must   |                        |
   |                        |                        | be built with          |                        |
   |                        |                        | PKIX_OBJECT_LEAK_TEST  |                        |
   |                        |                        | defined to use this    |                        |
   |                        |                        | functionality.*        |                        |
   +------------------------+------------------------+------------------------+------------------------+
   | ``SOCKETTRACE``        | Boolean                | Controls tracing of    | 3.12                   |
   |                        | (1 to enable)          | socket activity by     |                        |
   |                        |                        | libPKIX. Messages sent |                        |
   |                        |                        | and received will be   |                        |
   |                        |                        | timestamped and dumped |                        |
   |                        |                        | (to stdout) in         |                        |
   |                        |                        | standard hex-dump      |                        |
   |                        |                        | format.                |                        |
   +------------------------+------------------------+------------------------+------------------------+
   | ``SQLITE               | Boolean                | 1 means force always   | 3.12.6                 |
   | _FORCE_PROXY_LOCKING`` | (1 to enable)          | use proxy, 0 means     |                        |
   |                        |                        | never use proxy, NULL  |                        |
   |                        |                        | means use proxy for    |                        |
   |                        |                        | non-local files only.  |                        |
   +------------------------+------------------------+------------------------+------------------------+
   | ``SSLBYPASS``          | Boolean                | Uses PKCS#11 bypass    | 3.11                   |
   |                        | (1 to enable)          | for performance        |                        |
   |                        |                        | improvement.           |                        |
   |                        |                        | Do not set this        |                        |
   |                        |                        | variable if FIPS is    |                        |
   |                        |                        | enabled.               |                        |
   +------------------------+------------------------+------------------------+------------------------+
   | ``SSLDEBUG``           | Integer                | Debug level            | Before 3.0             |
   |                        |                        | Note: *The code must   |                        |
   |                        |                        | be built with DEBUG    |                        |
   |                        |                        | defined to use this    |                        |
   |                        |                        | functionality.*        |                        |
   +------------------------+------------------------+------------------------+------------------------+
   | ``SSLDEBUGFILE``       | String                 | File where debug or    | 3.12                   |
   |                        | (file name)            | trace information is   |                        |
   |                        |                        | written.               |                        |
   |                        |                        | If not set, the debug  |                        |
   |                        |                        | or trace information   |                        |
   |                        |                        | is written to stderr.  |                        |
   |                        |                        |                        |                        |
   |                        |                        | Note: *SSLDEBUG or     |                        |
   |                        |                        | SSLTRACE have to be    |                        |
   |                        |                        | set to use this        |                        |
   |                        |                        | functionality.*        |                        |
   +------------------------+------------------------+------------------------+------------------------+
   | ``SSLFORCELOCKS``      | Boolean                | Forces NSS to use      | 3.11                   |
   |                        | (1 to enable)          | locks for protection.  |                        |
   |                        |                        | Overrides the effect   |                        |
   |                        |                        | of SSL_NO_LOCKS (see   |                        |
   |                        |                        | ssl.h).                |                        |
   +------------------------+------------------------+------------------------+------------------------+
   | ``SSLKEYLOGFILE``      | String                 | Key log file. If set,  | 3.12.6                 |
   |                        | (file name)            | NSS logs RSA           |                        |
   |                        |                        | pre-master secrets to  |                        |
   |                        |                        | this file. This allows |                        |
   |                        |                        | packet sniffers to     |                        |
   |                        |                        | decrypt TLS            |                        |
   |                        |                        | connections. See       |                        |
   |                        |                        | :ref:`mozilla_project  |                        |
   |                        |                        | s_nss_key_log_format`. |                        |
   +------------------------+------------------------+------------------------+------------------------+
   | ``SSLTRACE``           | Integer                | Tracing level          | Before 3.0             |
   |                        |                        | Note: *The code must   |                        |
   |                        |                        | be built with TRACE    |                        |
   |                        |                        | defined to use this    |                        |
   |                        |                        | functionality.*        |                        |
   +------------------------+------------------------+------------------------+------------------------+

.. _build-time_environment_variables:

`Build-Time Environment Variables <#build-time_environment_variables>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   These environment variables affect the build (compilation) of NSS.

   .. note::

      **Note: This section is a work in progress and is not yet complete.**

   +------------------------+------------------------+------------------------+------------------------+
   | Variable               | Type                   | Description            | Introduced in version  |
   +------------------------+------------------------+------------------------+------------------------+
   | ``BUILD_OPT``          | Boolean                | Do an optimized (not   | Before 3.0             |
   |                        | (1 to enable)          | DEBUG) build. Default  |                        |
   |                        |                        | is to do a DEBUG       |                        |
   |                        |                        | build.                 |                        |
   +------------------------+------------------------+------------------------+------------------------+
   | ``MOZ_DEBUG_SYMBOLS``  | Boolean                | Needed on Windows to   | 3.11                   |
   |                        | (1 to enable)          | build with versions of |                        |
   |                        |                        | MSVC (such as VC8 and  |                        |
   |                        |                        | VC9) that do not       |                        |
   |                        |                        | understand /PDB:NONE   |                        |
   +------------------------+------------------------+------------------------+------------------------+
   | ``MOZ_DEBUG_FLAGS``    | String                 | When                   | 3.12.8                 |
   |                        |                        | ``MOZ_DEBUG_SYMBOLS``  |                        |
   |                        |                        | is set, you may use    |                        |
   |                        |                        | ``MOZ_DEBUG_FLAGS`` to |                        |
   |                        |                        | specify alternative    |                        |
   |                        |                        | compiler flags to      |                        |
   |                        |                        | produce symbolic       |                        |
   |                        |                        | debugging information  |                        |
   |                        |                        | in a particular        |                        |
   |                        |                        | format.                |                        |
   +------------------------+------------------------+------------------------+------------------------+
   | ``NSDISTMODE``         | String                 | On operating systems   | Before 3.0             |
   |                        |                        | other than Windows,    |                        |
   |                        |                        | this controls whether  |                        |
   |                        |                        | copies, absolute       |                        |
   |                        |                        | symlinks, or relative  |                        |
   |                        |                        | symlinks of the output |                        |
   |                        |                        | files should be        |                        |
   |                        |                        | published to           |                        |
   |                        |                        | mozilla/dist. The      |                        |
   |                        |                        | possible values are:   |                        |
   |                        |                        |                        |                        |
   |                        |                        | -  copy: copies of     |                        |
   |                        |                        |    files are published |                        |
   |                        |                        | -  absolute_symlink:   |                        |
   |                        |                        |    symlinks whose      |                        |
   |                        |                        |    targets are         |                        |
   |                        |                        |    absolute pathnames  |                        |
   |                        |                        |    are published       |                        |
   |                        |                        |                        |                        |
   |                        |                        | If not specified,      |                        |
   |                        |                        | default to relative    |                        |
   |                        |                        | symlinks (symlinks     |                        |
   |                        |                        | whose targets are      |                        |
   |                        |                        | relative pathnames).   |                        |
   |                        |                        | On Windows, copies of  |                        |
   |                        |                        | files are always       |                        |
   |                        |                        | published.             |                        |
   +------------------------+------------------------+------------------------+------------------------+
   | ``NS_USE_GCC``         | Boolean                | On systems where GCC   | Before 3.0             |
   |                        | (1 to enable)          | is not the default     |                        |
   |                        |                        | compiler, this tells   |                        |
   |                        |                        | NSS to build with gcc. |                        |
   +------------------------+------------------------+------------------------+------------------------+
   | `N                     | Boolean                | Enable NSS support in  | 3.24                   |
   | SS_ALLOW_SSLKEYLOGFILE | (1 to enable)          | optimized builds for   |                        |
   |  <https://dxr.mozilla. |                        | logging SSL/TLS key    |                        |
   | org/nss/search?q=NSS_A |                        | material to a logfile  |                        |
   | LLOW_SSLKEYLOGFILE>`__ |                        | if the SSLKEYLOGFILE   |                        |
   |                        |                        | environment variable.  |                        |
   |                        |                        | As of NSS 3.24 this is |                        |
   |                        |                        | disabled by default.   |                        |
   +------------------------+------------------------+------------------------+------------------------+
   | ``NSS_BUI              | Boolean                | Continue building NSS  | 3.12.4                 |
   | LD_CONTINUE_ON_ERROR`` | (1 to enable)          | source directories     |                        |
   |                        |                        | when a build error     |                        |
   |                        |                        | occurs.                |                        |
   +------------------------+------------------------+------------------------+------------------------+
   | ``N                    | Boolean                | Use the system         | 3.12.6                 |
   | SS_USE_SYSTEM_SQLITE`` | (1 to enable)          | installed sqlite       |                        |
   |                        |                        | library instead of the |                        |
   |                        |                        | in-tree version.       |                        |
   +------------------------+------------------------+------------------------+------------------------+
   | ``NSS_DISA             | Boolean                | Disable Elliptic Curve | 3.16                   |
   | BLE_ECC (deprecated)`` | (1 to disable)         | Cryptography features. |                        |
   |                        |                        | As of NSS 3.16, ECC    |                        |
   |                        |                        | features are enabled   |                        |
   |                        |                        | by default. As of NSS  |                        |
   |                        |                        | 3.33 this variable has |                        |
   |                        |                        | no effect.             |                        |
   +------------------------+------------------------+------------------------+------------------------+
   | ``NSS_ENA              | Boolean                | Enable building of     | Before 3.16; since     |
   | BLE_ECC (deprecated)`` | (1 to enable)          | code that uses         | 3.11.                  |
   |                        |                        | Elliptic Curve         |                        |
   |                        |                        | Cryptography. Unused   |                        |
   |                        |                        | as of NSS 3.16; see    |                        |
   |                        |                        | NSS_DISABLE_ECC.       |                        |
   +------------------------+------------------------+------------------------+------------------------+
   | ```NSS_FOR             | | Boolean              | Allows enabling FIPS   | 3.24                   |
   | CE_FIPS`` <https://dxr | | (1 to enable)        | mode using             |                        |
   | .mozilla.org/nss/searc |                        | ``NSS_FIPS``           |                        |
   | h?q=NSS_FORCE_FIPS>`__ |                        |                        |                        |
   +------------------------+------------------------+------------------------+------------------------+
   | ``OS_TARGET``          | String                 | For cross-compilation  | Before 3.0             |
   |                        | (target OS)            | environments only,     |                        |
   |                        |                        | when the target OS is  |                        |
   |                        |                        | not the default for    |                        |
   |                        |                        | the system on which    |                        |
   |                        |                        | the build is           |                        |
   |                        |                        | performed.             |                        |
   |                        |                        | Values understood:     |                        |
   |                        |                        | WIN95                  |                        |
   +------------------------+------------------------+------------------------+------------------------+
   | ``USE_64``             | Boolean                | On platforms that has  | Before 3.0             |
   |                        | (1 to enable)          | separate 32-bit and    |                        |
   |                        |                        | 64-bit ABIs, NSS       |                        |
   |                        |                        | builds for the 32-bit  |                        |
   |                        |                        | ABI by default. This   |                        |
   |                        |                        | tells NSS to build for |                        |
   |                        |                        | the 64-bit ABI.        |                        |
   +------------------------+------------------------+------------------------+------------------------+
   | ``USE_DEBUG_RTL``      | Boolean                | On Windows, MSVC has   | Before 3.0             |
   |                        | (1 to enable)          | options to build with  |                        |
   |                        |                        | a normal Run Time      |                        |
   |                        |                        | Library or a debug Run |                        |
   |                        |                        | Time Library. This     |                        |
   |                        |                        | tells NSS to build     |                        |
   |                        |                        | with the Debug Run     |                        |
   |                        |                        | Time Library.          |                        |
   +------------------------+------------------------+------------------------+------------------------+
   | ``USE_PTHREADS``       | Boolean                | On platforms where     | Before 3.0             |
   |                        | (1 to enable)          | POSIX threads are      |                        |
   |                        |                        | available, but are not |                        |
   |                        |                        | the OS'es preferred    |                        |
   |                        |                        | threads library, this  |                        |
   |                        |                        | tells NSS and NSPR to  |                        |
   |                        |                        | build using pthreads.  |                        |
   +------------------------+------------------------+------------------------+------------------------+
   | ``                     | String                 | Disables at            | Before 3.15            |
   | NSS_NO_PKCS11_BYPASS`` | (1 to enable)          | compile-time the NS    |                        |
   |                        |                        | ssl code to bypass the |                        |
   |                        |                        | pkcs11 layer. When set |                        |
   |                        |                        | the SSLBYPASS run-time |                        |
   |                        |                        | variable won't take    |                        |
   |                        |                        | effect                 |                        |
   +------------------------+------------------------+------------------------+------------------------+