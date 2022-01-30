.. _mozilla_projects_nss_nss_sample_code_nss_sample_code_sample_1_hashing:

NSS Sample Code Sample_1_Hashing
================================

.. _nss_sample_code_1_hashing_a_file.:

`NSS Sample Code 1: Hashing a file. <#nss_sample_code_1_hashing_a_file.>`__
---------------------------------------------------------------------------

.. container::

   This is an example program that demonstrates how to compute the hash of a file and save it to
   another file.  This program illustrates the use of NSS message APIs.

.. _sample_code_1:

`Sample Code 1 <#sample_code_1>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   .. code:: brush:

      /* NSPR Headers */
      #include <prprf.h>
      #include <prtypes.h>
      #include <plgetopt.h>
      #include <prio.h>

      /* NSS headers */
      #include <secoid.h>
      #include <secmodt.h>
      #include <sechash.h>

      typedef struct {
          const char *hashName;
          SECOidTag oid;
      } NameTagPair;

      /* The hash algorithms supported */
      static const NameTagPair HASH_NAMES[] = {
          { "MD2", SEC_OID_MD2 },
          { "MD5", SEC_OID_MD5 },
          { "SHA1", SEC_OID_SHA1 },
          { "SHA256", SEC_OID_SHA256 },
          { "SHA384", SEC_OID_SHA384 },
          { "SHA512", SEC_OID_SHA512 }
      };

      /*
       * Maps a hash name to a SECOidTag.
       * Returns NULL if the name is not a supported algorithm
       */
      static SECOidTag HashNameToOIDTag(const char *hashName)
      {
          int i, nhashes = sizeof(HASH_NAMES);
          SECOidTag hashtag = SEC_OID_UNKNOWN;

          for (i = 0; i < nhashes; i++) {
              if (PORT_Strcasecmp(hashName, HASH_NAMES[i].hashName) == 0) {
                  hashtag = HASH_NAMES[i].oid;
                  break;
              }
          }
          return hashtag;
      }

      /*
       * Newline
       */
      static void Newline(PRFileDesc* out) { PR_fprintf(out, "\n"); }

      /*
       * PrintAsHex
       */
      void PrintAsHex(PRFileDesc* out, unsigned char *data, unsigned int len)
      {
          unsigned i;
          int column;
          unsigned int limit = 15;
          unsigned int level  = 1;

          column = level;
          if (!len) {
              PR_fprintf(out, "(empty)\n");
              return;
          }

          for (i = 0; i < len; i++) {
              if (i != len - 1) {
                  PR_fprintf(out, "%02x:", data[i]);
                  column += 3;
              } else {
                  PR_fprintf(out, "%02x", data[i]);
                  column += 2;
                  break;
              }
              if (column > 76 || (i % 16 == limit)) {
                  Newline(out);
                  column = level;
                  limit = i % 16;
              }
          }
          if (column != level) {
              Newline(out);
          }
      }

      /*
       * Prints a usage message and exits
       */
      static void Usage(const char *progName)
      {
          int htype;
          int HASH_AlgTOTAL = sizeof(HASH_NAMES) / sizeof(HASH_NAMES[0]);

          fprintf(stderr, "Usage:  %s -t type [ < input ] [ > output ]\n", progName);
          fprintf(stderr, "%-20s Specify the digest method (must be one of\n",
                  "-t type");
          fprintf(stderr, "%-20s ", "");
          for (htype = 0; htype < HASH_AlgTOTAL; htype++) {
              fprintf(stderr, HASH_NAMES[htype].hashName);
              if (htype == (HASH_AlgTOTAL - 2))
                  fprintf(stderr, " or ");
              else if (htype != (HASH_AlgTOTAL - 1))
                  fprintf(stderr, ", ");
          }
          fprintf(stderr, " (case ignored))\n");
          fprintf(stderr, "%-20s Define an input file to use (default is stdin)\n",
                  "< input");
          fprintf(stderr, "%-20s Define an output file to use (default is stdout)\n",
                  "> output");
          exit(-1);
      }

      /*
       * Check for the missing arguments
       */
      static void PrintMsgAndExit(const char *progName, char opt)
      {
          fprintf(stderr, "%s: option -%c requires an argument\n", progName, opt);
          Usage(progName);
      }

      #define REQUIRE_ARG(opt,value) if (!(value)) PrintMsgAndExit(progName, opt)

      /*
       * Digests a file according to the specified algorithm.
       * It writes out the digest as a hexadecimal string.
       */
      static int DigestFile(PRFileDesc *outFile, PRFileDesc *inFile, SECOidTag hashOIDTag)
      {
          unsigned int  nb;
          unsigned char ibuf[4096];
          unsigned char digest[64];
          unsigned int  len;
          unsigned int  digestLen;
          HASH_HashType hashType;
          HASHContext   *hashContext = NULL;

          hashType    = HASH_GetHashTypeByOidTag(hashOIDTag);
          hashContext = HASH_Create(hashType);
          if (hashContext == NULL) {
              return SECFailure;
          }

          do {

              HASH_Begin(hashContext);

              /* Incrementally hash the file contents */
              while ((nb = PR_Read(inFile, ibuf, sizeof(ibuf))) > 0) {
                  HASH_Update(hashContext, ibuf, nb);
              }

              HASH_End(hashContext, digest, &len, 64);

              /*  Normally we would write it out in binary with
               *  nb = PR_Write(outFile, digest, len);
               *  but for illustration let's print it in hex.
               */
              PrintAsHex(outFile, digest, len);

          } while (0);

          /* cleanup */
          if (hashContext != NULL)
              HASH_Destroy(hashContext);

          return SECSuccess;
      }

      /*
       * This sample computes the hash of a file and saves it
       * to another file. It illustrates the use of NSS message
       * APIs.
       */
      int main(int argc, char **argv)
      {
          SECOidTag     hashOIDTag;
          PLOptState    *optstate;
          PLOptStatus   status;
          SECStatus     rv;
          char          *hashName  = NULL;
          char          *progName = strrchr(argv[0], '/');

          progName = progName ? progName + 1 : argv[0];

          rv = NSS_NoDB_Init("/tmp");
          if (rv != SECSuccess) {
              fprintf(stderr, "%s: NSS_Init failed in directory %s\n",
                      progName, "/tmp");
              return -1;
          }

          /* Parse command line arguments */
          optstate = PL_CreateOptState(argc, argv, "t:");
          while ((status = PL_GetNextOpt(optstate)) == PL_OPT_OK) {
              switch (optstate->option) {
              case 't':
                  REQUIRE_ARG(optstate->option, optstate->value);
                  hashName = strdup(optstate->value);
                  break;
              }
          }

          if (!hashName)
              Usage(progName);

          /* convert and validate */
          hashOIDTag = HashNameToOIDTag(hashName);
          if (hashOIDTag == SEC_OID_UNKNOWN) {
              fprintf(stderr, "%s: invalid digest type - %s\n", progName, hashName);
              Usage(progName);
          }

          /* Digest it and print the result */
          rv = DigestFile(PR_STDOUT, PR_STDIN, hashOIDTag);
          if (rv != SECSuccess) {
              fprintf(stderr, "%s: problem digesting data (%d)\n",
                      progName, PORT_GetError());
          }

          rv = NSS_Shutdown();
          if (rv != SECSuccess) {
              exit(-1);
          }

          return 0;
      }

      </sechash.h></secmodt.h></secoid.h></prio.h></plgetopt.h></prtypes.h></prprf.h>