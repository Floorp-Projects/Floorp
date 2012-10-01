/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Test program for SDR (Secret Decoder Ring) functions.
 *
 * $Id: pwdecrypt.c,v 1.8 2012/03/20 14:47:16 gerv%gerv.net Exp $
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
    PR_fprintf (pr_stderr,
	"Usage:\t%s [-i <input-file>] [-o <output-file>] [-d <dir>]\n"
        "      \t[-l logfile] [-p pwd] [-f pwfile]\n", program_name);
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
    PR_fprintf (pr_stderr,
		"  %-13s Token password\n",
		"-p pwd");
    PR_fprintf (pr_stderr,
		"  %-13s Password file\n",
		"-f pwfile");
}

/*
 * base64 table only used to identify the end of a base64 string 
 */
static unsigned char b64[256] = {
/*  00: */	0,	0,	0,	0,	0,	0,	0,	0,
/*  08: */	0,	0,	0,	0,	0,	0,	0,	0,
/*  10: */	0,	0,	0,	0,	0,	0,	0,	0,
/*  18: */	0,	0,	0,	0,	0,	0,	0,	0,
/*  20: */	0,	0,	0,	0,	0,	0,	0,	0,
/*  28: */	0,	0,	0,	1,	0,	0,	0,	1,
/*  30: */	1,	1,	1,	1,	1,	1,	1,	1,
/*  38: */	1,	1,	0,	0,	0,	0,	0,	0,
/*  40: */	0,	1,	1,	1,	1,	1,	1,	1,
/*  48: */	1,	1,	1,	1,	1,	1,	1,	1,
/*  50: */	1,	1,	1,	1,	1,	1,	1,	1,
/*  58: */	1,	1,	1,	0,	0,	0,	0,	0,
/*  60: */	0,	1,	1,	1,	1,	1,	1,	1,
/*  68: */	1,	1,	1,	1,	1,	1,	1,	1,
/*  70: */	1,	1,	1,	1,	1,	1,	1,	1,
/*  78: */	1,	1,	1,	0,	0,	0,	0,	0,
};

enum {
   false = 0,
   true = 1
} bool;

#define isatobchar(c) (b64[c])

#define MAX_STRING 8192

int
isBase64(char *inString) 
{
    unsigned int i;
    unsigned char c;

    for (i = 0; (c = inString[i]) != 0 && isatobchar(c); ++i) 
	;
    if (c == '=') {
	while ((c = inString[++i]) == '=')
	    ; /* skip trailing '=' characters */
    }
    if (c && c != '\n' && c != '\r')
	return false;
    if (i == 0 || i % 4)
    	return false;
    return true;
}

void
doDecrypt(char * dataString, FILE *outFile, FILE *logFile, secuPWData *pwdata)
{
    int        strLen = strlen(dataString);
    SECItem   *decoded = NSSBase64_DecodeBuffer(NULL, NULL, dataString, strLen);
    SECStatus  rv;
    int        err;
    unsigned int i;
    SECItem    result = { siBuffer, NULL, 0 };

    if ((decoded == NULL) || (decoded->len == 0)) {
	if (logFile) {
	    err = PORT_GetError();
	    fprintf(logFile,"Base 64 decode failed on <%s>\n", dataString);
	    fprintf(logFile," Error %d: %s\n", err, SECU_Strerror(err));
	}
	fputs(dataString, outFile);
	if (decoded)
	    SECITEM_FreeItem(decoded, PR_TRUE);
	return;
    }

    rv = PK11SDR_Decrypt(decoded, &result, pwdata);
    SECITEM_ZfreeItem(decoded, PR_TRUE);
    if (rv == SECSuccess) {
	/* result buffer has no extra space for a NULL */
	fprintf(outFile, "Decrypted: \"%.*s\"\n", result.len, result.data);
	SECITEM_ZfreeItem(&result, PR_FALSE);
	return;
    }
    /* Encryption failed. output raw input. */
    if (logFile) {
	err = PORT_GetError();
	fprintf(logFile,"SDR decrypt failed on <%s>\n", dataString);
	fprintf(logFile," Error %d: %s\n", err, SECU_Strerror(err));
    }
    fputs(dataString,outFile);
}

void
doDecode(char * dataString, FILE *outFile, FILE *logFile)
{
    int        strLen = strlen(dataString + 1);
    SECItem   *decoded;

    decoded = NSSBase64_DecodeBuffer(NULL, NULL, dataString + 1, strLen);
    if ((decoded == NULL) || (decoded->len == 0)) {
	if (logFile) {
	    int err = PORT_GetError();
	    fprintf(logFile,"Base 64 decode failed on <%s>\n", dataString + 1);
	    fprintf(logFile," Error %d: %s\n", err, SECU_Strerror(err));
	}
	fputs(dataString, outFile);
	if (decoded)
	    SECITEM_FreeItem(decoded, PR_TRUE);
	return;
    }
    fprintf(outFile, "Decoded: \"%.*s\"\n", decoded->len, decoded->data);
    SECITEM_ZfreeItem(decoded, PR_TRUE);
}

char dataString[MAX_STRING + 1];

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
    secuPWData  pwdata = { PW_NONE, NULL };


    program_name = PL_strrchr(argv[0], '/');
    program_name = program_name ? (program_name + 1) : argv[0];

    optstate = PL_CreateOptState (argc, argv, "Hd:f:i:o:l:p:?");
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

          case 'f':
            pwdata.source = PW_FROMFILE;
            pwdata.data = PL_strdup(optstate->value);
            break;

          case 'p':
            pwdata.source = PW_PLAINTEXT;
            pwdata.data = PL_strdup(optstate->value);
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
	if (log_file[0] == '-')
	    logFile = stderr;
	else
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
    while (fgets(dataString, sizeof dataString, inFile)) {
	unsigned char c = dataString[0];

	if (c == 'M' && isBase64(dataString)) {
	    doDecrypt(dataString, outFile, logFile, &pwdata);
        } else if (c == '~' && isBase64(dataString + 1)) {
	    doDecode(dataString, outFile, logFile);
	} else {
	    fputs(dataString, outFile);
	}
    }
    if (pwdata.data)
    	PR_Free(pwdata.data);

    fclose(outFile);
    fclose(inFile);
    if (logFile && logFile != stderr) {
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
