/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape security libraries.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1994-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * Test program for SDR (Secret Decoder Ring) functions.
 *
 * $Id: pwdecrypt.c,v 1.3 2004/07/28 21:10:07 nelsonb%netscape.com Exp $
 */

#include "nspr.h"
#include "string.h"
#include "nss.h"
#include "secutil.h"
#include "cert.h"
#include "pk11func.h"
#include "nssb64.h"

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
	"\t%s [-i <input-file>] [-o <output-file>] [-d <dir>] [-l logfile]\n",
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
    PR_fprintf (pr_stderr, "\nDecode encrypted passwords (and other data).\n");
    PR_fprintf (pr_stderr, 
	"This program reads in standard configuration files looking\n"
	"for base 64 encoded data. Data that looks like it's base 64 encode\n"
	"is decoded an passed to the NSS SDR code. If the decode and decrypt\n"
	"is successful, then decrypted data is outputted in place of the\n"
	"original base 64 data. If the decode or decrypt fails, the original\n"
	"data is written and the reason for failure is logged to the \n"
	"optional logfile.\n");
    PR_fprintf (pr_stderr,
		"  %-13s Read stream including encrypted data from "
	        "\"read_file\"\n",
		"-i read_file");
    PR_fprintf (pr_stderr,
		"  %-13s Write results to \"write_file\"\n",
		"-o write_file");
    PR_fprintf (pr_stderr,
		"  %-13s Find security databases in \"dbdir\"\n",
		"-d dbdir");
    PR_fprintf (pr_stderr,
		"  %-13s Log failed decrypt/decode attempts to \"log_file\"\n",
		"-l log_file");
}

/*
 * base64 table only used to identify the end of a base64 string 
 */
static unsigned char b64[256] = {
/*   0: */        0,      0,      0,      0,      0,      0,      0,      0,
/*   8: */        0,      0,      0,      0,      0,      0,      0,      0,
/*  16: */        0,      0,      0,      0,      0,      0,      0,      0,
/*  24: */        0,      0,      0,      0,      0,      0,      0,      0,
/*  32: */        0,      0,      0,      0,      0,      0,      0,      0,
/*  40: */        0,      0,      0,      1,      0,      0,      0,      1,
/*  48: */        1,      1,      1,      1,      1,      1,      1,      1,
/*  56: */        1,      1,      0,      0,      0,      0,      0,      0,
/*  64: */        0,      1,      1,      1,      1,      1,      1,      1,
/*  72: */        1,      1,      1,      1,      1,      1,      1,      1,
/*  80: */        1,      1,      1,      1,      1,      1,      1,      1,
/*  88: */        1,      1,      1,      0,      0,      0,      0,      0,
/*  96: */        0,      1,      1,      1,      1,      1,      1,      1,
/* 104: */        1,      1,      1,      1,      1,      1,      1,      1,
/* 112: */        1,      1,      1,      1,      1,      1,      1,      1,
/* 120: */        1,      1,      1,      0,      0,      0,      0,      0,
/* 128: */        0,      0,      0,      0,      0,      0,      0,      0
};

enum {
   false = 0,
   true = 1
} bool;

int
isatobchar(int c) { return b64[c] != 0; }


#define MAX_STRING 256
int
getData(FILE *inFile,char **inString) {
    int len = 0;
    int space = MAX_STRING;
    int oneequal = false;
    int c;
    char *string = (char *) malloc(space);

    string[len++]='M';

    while ((c = getc(inFile)) != EOF) {
	if (len >= space) {
	    char *newString;

	    space *= 2;
	    newString = (char *)realloc(string,space);
	    if (newString == NULL) {
		ungetc(c,inFile);
		break;
	    }
	    string = newString;
	}
	string[len++] = c;
	if (!isatobchar(c)) {
	   if (c == '=') {
		if (oneequal) {
		    break;
		}
		oneequal = true;
		continue;
	   } else {
	       ungetc(c,inFile);
	       len--;
	       break;
	   }
	}
	if (oneequal) {
	   ungetc(c,inFile);
	   len--;
	   break;
	}
    }
    if (len >= space) {
	space += 2;
	string = (char *)realloc(string,space);
    }
    string[len++] = 0;
    *inString = string;
    return true;
}

int
main (int argc, char **argv)
{
    int		 retval = 0;  /* 0 - test succeeded.  -1 - test failed */
    SECStatus	 rv;
    PLOptState	*optstate;
    char	*program_name;
    char  *input_file = NULL; 	/* read encrypted data from here (or create) */
    char  *output_file = NULL;	/* write new encrypted data here */
    char  *log_file = NULL;	/* write new encrypted data here */
    FILE	*inFile = stdin;
    FILE	*outFile = stdout;
    FILE	*logFile = NULL;
    PLOptStatus optstatus;
    SECItem	result;
    int		c;

    result.data = 0;

    program_name = PL_strrchr(argv[0], '/');
    program_name = program_name ? (program_name + 1) : argv[0];

    optstate = PL_CreateOptState (argc, argv, "Hd:i:o:l:?");
    if (optstate == NULL) {
	SECU_PrintError (program_name, "PL_CreateOptState failed");
	return 1;
    }

    while ((optstatus = PL_GetNextOpt(optstate)) == PL_OPT_OK) {
	switch (optstate->option) {
	  case '?':
	    short_usage (program_name);
	    return 1;

	  case 'H':
	    long_usage (program_name);
	    return 1;

	  case 'd':
	    SECU_ConfigDirectory(optstate->value);
	    break;

          case 'i':
            input_file = PL_strdup(optstate->value);
            break;

          case 'o':
            output_file = PL_strdup(optstate->value);
            break;

          case 'l':
            log_file = PL_strdup(optstate->value);
            break;

	}
    }
    PL_DestroyOptState(optstate);
    if (optstatus == PL_OPT_BAD) {
	short_usage (program_name);
	return 1;
    }

    if (input_file) {
      inFile = fopen(input_file,"r");
      if (inFile == NULL) {
	perror(input_file);
	return 1;
      }
      PR_Free(input_file);
    }
    if (output_file) {
      outFile = fopen(output_file,"w+");
      if (outFile == NULL) {
	perror(output_file);
	return 1;
      }
      PR_Free(output_file);
    }
    if (log_file) {
      logFile = fopen(log_file,"w+");
      if (logFile == NULL) {
	perror(log_file);
	return 1;
      }
      PR_Free(log_file);
    }

    /*
     * Initialize the Security libraries.
     */
    PK11_SetPasswordFunc(SECU_GetModulePassword);
    rv = NSS_Init(SECU_ConfigDirectory(NULL));
    if (rv != SECSuccess) {
	SECU_PrintError (program_name, "NSS_Init failed");
	retval = 1;
	goto prdone;
    }

    /* Get the encrypted result, either from the input file
     * or from encrypting the plaintext value
     */

    while ((c = getc(inFile)) != EOF) {
	if (c == 'M') {
	   char *dataString = NULL;
	   SECItem *inText;

	   rv = getData(inFile, &dataString);
	   if (!rv) {
		fputs(dataString,outFile);
		free(dataString);
		continue;
	   }
	   inText = NSSBase64_DecodeBuffer(NULL, NULL, dataString,
							strlen(dataString));
	   if ((inText == NULL) || (inText->len == 0)) {
		if (logFile) {
		    fprintf(logFile,"Base 64 decode failed on <%s>\n",
								dataString);
		    fprintf(logFile," Error %x: %s\n",PORT_GetError(),
			SECU_Strerror(PORT_GetError()));
		}
		fputs(dataString,outFile);
		free(dataString);
		continue;
	   }
	   result.data = malloc(inText->len+1);
	   result.len = inText->len+1;
	   rv = PK11SDR_Decrypt(inText, &result, NULL);
	   SECITEM_FreeItem(inText, PR_TRUE);
	   if (rv != SECSuccess) {
		if (logFile) {
		    fprintf(logFile,"SDR decrypt failed on <%s>\n",
								dataString);
		    fprintf(logFile," Error %x: %s\n",PORT_GetError(),
			SECU_Strerror(PORT_GetError()));
		}
		fputs(dataString,outFile);
		free(dataString);
		free(result.data);
		continue;
	   }
	   result.data[result.len] = 0;
	   fputs(result.data,outFile);
	   free(result.data);
         } else {
	   putc(c,outFile);
         }
    }

    fclose(outFile);
    fclose(inFile);
    if (logFile) {
	fclose(logFile);
    }

    if (NSS_Shutdown() != SECSuccess) {
	SECU_PrintError (program_name, "NSS_Shutdown failed");
       exit(1);
    }

prdone:
    PR_Cleanup ();
    return retval;
}















