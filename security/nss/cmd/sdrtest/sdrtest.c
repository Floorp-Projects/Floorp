/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

/*
 * Test program for SDR (Secret Decoder Ring) functions.
 *
 * $Id: sdrtest.c,v 1.11 2003/05/30 23:31:05 wtc%netscape.com Exp $
 */

#include "nspr.h"
#include "string.h"
#include "nss.h"
#include "secutil.h"
#include "cert.h"
#include "pk11func.h"

#include "plgetopt.h"
#include "pk11sdr.h"

#define DEFAULT_VALUE "Test"

static void
synopsis (char *program_name)
{
    PRFileDesc *pr_stderr;

    pr_stderr = PR_STDERR;
    PR_fprintf (pr_stderr, "Usage:");
    PR_fprintf (pr_stderr,
		"\t%s [-i <input-file>] [-o <output-file>] [-r <text>] [-d <dir>]\n",
		program_name);
}


static void
short_usage (char *program_name)
{
    PR_fprintf (PR_STDERR,
		"Type %s -H for more detailed descriptions\n",
		program_name);
    synopsis (program_name);
}


static void
long_usage (char *program_name)
{
    PRFileDesc *pr_stderr;

    pr_stderr = PR_STDERR;
    synopsis (program_name);
    PR_fprintf (pr_stderr, "\nSecret Decoder Test:\n");
    PR_fprintf (pr_stderr,
		"  %-13s Read encrypted data from \"file\"\n",
		"-i file");
    PR_fprintf (pr_stderr,
		"  %-13s Write newly generated encrypted data to \"file\"\n",
		"-o file");
    PR_fprintf (pr_stderr,
		"  %-13s Use \"text\" as the plaintext for encryption and verification\n",
		"-t text");
    PR_fprintf (pr_stderr,
		"  %-13s Find security databases in \"dbdir\"\n",
		"-d dbdir");
}

int
main (int argc, char **argv)
{
    int		 retval = 0;  /* 0 - test succeeded.  -1 - test failed */
    SECStatus	 rv;
    PLOptState	*optstate;
    char	*program_name;
    const char  *input_file = NULL; 	/* read encrypted data from here (or create) */
    const char  *output_file = NULL;	/* write new encrypted data here */
    const char  *value = DEFAULT_VALUE;	/* Use this for plaintext */
    SECItem     data;
    SECItem     result;
    SECItem     text;
    PRBool      verbose = PR_FALSE;

    result.data = 0;
    text.data = 0; text.len = 0;

    program_name = PL_strrchr(argv[0], '/');
    program_name = program_name ? (program_name + 1) : argv[0];

    optstate = PL_CreateOptState (argc, argv, "Hd:i:o:t:v");
    if (optstate == NULL) {
	SECU_PrintError (program_name, "PL_CreateOptState failed");
	return -1;
    }

    while (PL_GetNextOpt (optstate) == PL_OPT_OK) {
	switch (optstate->option) {
	  case '?':
	    short_usage (program_name);
	    return retval;

	  case 'H':
	    long_usage (program_name);
	    return retval;

	  case 'd':
	    SECU_ConfigDirectory(optstate->value);
	    break;

          case 'i':
            input_file = optstate->value;
            break;

          case 'o':
            output_file = optstate->value;
            break;

          case 't':
            value = optstate->value;
            break;

          case 'v':
            verbose = PR_TRUE;
            break;
	}
    }

    /*
     * Initialize the Security libraries.
     */
    PK11_SetPasswordFunc(SECU_GetModulePassword);

    if (output_file) {
	rv = NSS_InitReadWrite(SECU_ConfigDirectory(NULL));
    } else {
	rv = NSS_Init(SECU_ConfigDirectory(NULL));
    }
    if (rv != SECSuccess) {
	retval = -1;
	goto prdone;
    }

    /* Convert value into an item */
    data.data = (unsigned char *)value;
    data.len = strlen(value);

    /* Get the encrypted result, either from the input file
     * or from encrypting the plaintext value
     */
    if (input_file)
    {
      PRFileDesc *file /* = PR_OpenFile(input_file, 0) */;
      PRFileInfo info;
      PRStatus s;
      PRInt32 count;

      if (verbose) printf("Reading data from %s\n", input_file);

      file = PR_Open(input_file, PR_RDONLY, 0);
      if (!file) {
        if (verbose) printf("Open of file failed\n");
        retval = -1;
        goto loser;
      }

      s = PR_GetOpenFileInfo(file, &info);
      if (s != PR_SUCCESS) {
        if (verbose) printf("File info operation failed\n");
        retval = -1;
        goto file_loser;
      }

      result.len = info.size;
      result.data = (unsigned char *)malloc(result.len);
      if (!result.data) {
        if (verbose) printf("Allocation of buffer failed\n");
        retval = -1;
        goto file_loser;
      }

      count = PR_Read(file, result.data, result.len);
      if (count != result.len) {
        if (verbose) printf("Read failed\n");
        retval = -1;
        goto file_loser;
      }

file_loser:
      PR_Close(file);
      if (retval != 0) goto loser;
    }
    else
    {
      SECItem keyid = { 0, 0, 0 };
      PK11SlotInfo *slot = NULL;

      /* sigh, initialize the key database */
      slot = PK11_GetInternalKeySlot();
      if (slot && PK11_NeedUserInit(slot)) {
        rv = SECU_ChangePW(slot, "", 0);
        if (rv != SECSuccess) {
            SECU_PrintError(program_name, "Failed to initialize slot \"%s\"",
                                    PK11_GetSlotName(slot));
            return SECFailure;
        }
      }
      if (slot) {
	PK11_FreeSlot(slot);
      }

      rv = PK11SDR_Encrypt(&keyid, &data, &result, 0);
      if (rv != SECSuccess) {
        if (verbose) printf("Encrypt operation failed\n");
        retval = -1;
        goto loser;
      }

      if (verbose) printf("Encrypted result is %d bytes long\n", result.len);

      /* -v printf("Result is %.*s\n", text.len, text.data); */
      if (output_file)
      {
         PRFileDesc *file;
         PRInt32 count;

         if (verbose) printf("Writing result to %s\n", output_file);

         /* Write to file */
         file = PR_Open(output_file, PR_CREATE_FILE|PR_WRONLY, 0666);
         if (!file) {
            if (verbose) printf("Open of output file failed\n");
            retval = -1;
            goto loser;
         }

         count = PR_Write(file, result.data, result.len);

         PR_Close(file);

         if (count != result.len) {
           if (verbose) printf("Write failed\n");
           retval = -1;
           goto loser;
         }
      }
    }

    /* Decrypt the value */
    rv = PK11SDR_Decrypt(&result, &text, 0);
    if (rv != SECSuccess) {
      if (verbose) printf("Decrypt operation failed\n");
      retval = -1; 
      goto loser;
    }

    if (verbose) printf("Decrypted result is %.*s\n", text.len, text.data);

    /* Compare to required value */
    if (text.len != data.len || memcmp(data.data, text.data, text.len) != 0)
    {
      if (verbose) printf("Comparison failed\n");
      retval = -1;
      goto loser;
    }

loser:
    if (text.data) free(text.data);
    if (result.data) free(result.data);
    if (NSS_Shutdown() != SECSuccess) {
       exit(1);
    }

prdone:
    PR_Cleanup ();
    return retval;
}
