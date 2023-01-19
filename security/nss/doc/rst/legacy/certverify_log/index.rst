.. _mozilla_projects_nss_certverify_log:

NSS CERTVerify Log
==================

`CERTVerifyLog <#certverifylog>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   All the NSS verify functions except, the \*VerifyNow() functions, take a parameter called
   'CERTVerifyLog'. If you supply the log parameter, NSS will continue chain validation after each
   error . The log tells you what the problem was with the chain and what certificate in the chain
   failed.

   To create a log:

   .. code::

      #include "secport.h"
      #include "certt.h"

      CERTVerifyLog *log;

      arena = PORT_NewArena(512);
      log =  PORT_ArenaZNew(arena,log);
      log->arena = arena;

   You can then pass this log into your favorite cert verify function. On return:

   -  log->count is the number of entries.
   -  log->head is the first entry;
   -  log->tail is the last entry.

   Each entry is a CERTVerifyLogNode. Defined in certt.h:

   .. code::

      /*
       * This structure is used to keep a log of errors when verifying
       * a cert chain.  This allows multiple errors to be reported all at
       * once.
       */
      struct CERTVerifyLogNodeStr {
        CERTCertificate *cert;      /* what cert had the error */
        long error;                 /* what error was it? */
        unsigned int depth;         /* how far up the chain are we */
        void *arg;                  /* error specific argument */
        struct CERTVerifyLogNodeStr *next; /* next in the list */
        struct CERTVerifyLogNodeStr *prev; /* next in the list */
      };

   The list is a doubly linked NULL terminated list sorted from low to high based on depth into the
   cert chain. When you are through, you will need to walk the list and free all the cert entries,
   then free the arena.