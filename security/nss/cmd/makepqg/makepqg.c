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

#include "prtypes.h"
#include "prtime.h"
#include "prlong.h"

#include "nss.h"
#include "secutil.h"
#include "secitem.h"
#include "pk11func.h"
#include "pk11pqg.h"

#if defined(XP_UNIX)
#include <unistd.h>
#endif

#include "plgetopt.h"

#define BPB 8 /* bits per byte. */

char               *progName;


const SEC_ASN1Template seckey_PQGParamsTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(SECKEYPQGParams) },
    { SEC_ASN1_INTEGER, offsetof(SECKEYPQGParams,prime) },
    { SEC_ASN1_INTEGER, offsetof(SECKEYPQGParams,subPrime) },
    { SEC_ASN1_INTEGER, offsetof(SECKEYPQGParams,base) },
    { 0, }
};



void
Usage(void)
{
    fprintf(stderr, "Usage:  %s\n", progName);
    fprintf(stderr, 
"-a   Output DER-encoded PQG params, BTOA encoded.\n"
"     -l prime-length       Length of prime in bits (1024 is default)\n"
"     -o file               Output to this file (default is stdout)\n"
"-b   Output DER-encoded PQG params in binary\n"
"     -l prime-length       Length of prime in bits (1024 is default)\n"
"     -o file               Output to this file (default is stdout)\n"
"-r   Output P, Q and G in ASCII hexadecimal. \n"
"     -l prime-length       Length of prime in bits (1024 is default)\n"
"     -o file               Output to this file (default is stdout)\n"   
"-g bits       Generate SEED this many bits long.\n"
);
    exit(-1);

}

int
outputPQGParams(PQGParams * pqgParams, PRBool output_binary, PRBool output_raw,
                FILE * outFile)
{
    PRArenaPool   * arena 		= NULL;
    char          * PQG;
    SECItem         encodedParams;

    if (output_raw) {
    	SECItem item;

	PK11_PQG_GetPrimeFromParams(pqgParams, &item);
	SECU_PrintInteger(outFile, &item,    "Prime",    1);
	SECITEM_FreeItem(&item, PR_FALSE);

	PK11_PQG_GetSubPrimeFromParams(pqgParams, &item);
	SECU_PrintInteger(outFile, &item, "Subprime", 1);
	SECITEM_FreeItem(&item, PR_FALSE);

	PK11_PQG_GetBaseFromParams(pqgParams, &item);
	SECU_PrintInteger(outFile, &item,     "Base",     1);
	SECITEM_FreeItem(&item, PR_FALSE);

	fprintf(outFile, "\n");
	return 0;
    }

    encodedParams.data = NULL;
    encodedParams.len  = 0;
    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    SEC_ASN1EncodeItem(arena, &encodedParams, pqgParams,
		       seckey_PQGParamsTemplate);
    if (output_binary) {
	fwrite(encodedParams.data, encodedParams.len, sizeof(char), outFile);
	printf("\n");
	return 0;
    }

    /* must be output ASCII */
    PQG = BTOA_DataToAscii(encodedParams.data, encodedParams.len);    

    fprintf(outFile,"%s",PQG);
    printf("\n");
    return 0;
}

int
outputPQGVerify(PQGVerify * pqgVerify, PRBool output_binary, PRBool output_raw,
                FILE * outFile)
{
    if (output_raw) {
    	SECItem item;
	unsigned int counter;

	PK11_PQG_GetHFromVerify(pqgVerify, &item);
	SECU_PrintInteger(outFile, &item,        "h",        1);
	SECITEM_FreeItem(&item, PR_FALSE);

	PK11_PQG_GetSeedFromVerify(pqgVerify, &item);
	SECU_PrintInteger(outFile, &item,     "SEED",     1);
	fprintf(outFile, "    g:       %d\n", item.len * BPB);
	SECITEM_FreeItem(&item, PR_FALSE);

	counter = PK11_PQG_GetCounterFromVerify(pqgVerify);
	fprintf(outFile, "    counter: %d\n", counter);
	fprintf(outFile, "\n");
	return 0;
    }
    return 0;
}

int
main(int argc, char **argv)
{
    FILE          * outFile 		= NULL;
    PQGParams     * pqgParams 		= NULL;
    PQGVerify     * pqgVerify           = NULL;
    int             keySizeInBits	= 1024;
    int             j;
    int             o;
    int             g                   = 0;
    SECStatus       rv 			= 0;
    SECStatus       passed 		= 0;
    PRBool          output_ascii	= PR_FALSE;
    PRBool          output_binary	= PR_FALSE;
    PRBool          output_raw		= PR_FALSE;
    PLOptState *optstate;
    PLOptStatus status;


    progName = strrchr(argv[0], '/');
    if (!progName)
	progName = strrchr(argv[0], '\\');
    progName = progName ? progName+1 : argv[0];

    /* Parse command line arguments */
    optstate = PL_CreateOptState(argc, argv, "l:abro:g:" );
    while ((status = PL_GetNextOpt(optstate)) == PL_OPT_OK) {
	switch (optstate->option) {

	  case 'l':
	    keySizeInBits = atoi(optstate->value);
	    break;

	  case 'a':
	    output_ascii = PR_TRUE;  
	    break;

	  case 'b':
	    output_binary = PR_TRUE;
	    break;

	  case 'r':
	    output_raw = PR_TRUE;
	    break;

	  case 'o':
	    outFile = fopen(optstate->value, "wb");
	    if (!outFile) {
		fprintf(stderr, "%s: unable to open \"%s\" for writing\n",
			progName, optstate->value);
		rv = -1;
	    }
	    break;

	  case 'g':
	    g = atoi(optstate->value);
	    break;

	  default:
	  case '?':
	    Usage();
	    break;

	}
    }

    if (rv != 0) {
	return rv;
    }

    /* exactly 1 of these options must be set. */
    if (1 != ((output_ascii  != PR_FALSE) + 
	      (output_binary != PR_FALSE) +
	      (output_raw    != PR_FALSE))) {
	Usage();
    }

    j = PQG_PBITS_TO_INDEX(keySizeInBits);
    if (j < 0) {
	fprintf(stderr, "%s: Illegal prime length, \n"
			"\tacceptable values are between 512 and 1024,\n"
			"\tand divisible by 64\n", progName);
	return -1;
    }
    if (g != 0 && (g < 160 || g >= 2048 || g % 8 != 0)) {
	fprintf(stderr, "%s: Illegal g bits, \n"
			"\tacceptable values are between 160 and 2040,\n"
			"\tand divisible by 8\n", progName);
	return -1;
    }

    if (outFile == NULL) {
	outFile = stdout;
    }


    NSS_NoDB_Init(NULL);

    if (g) 
	rv = PK11_PQG_ParamGenSeedLen((unsigned)j, (unsigned)(g/8), 
	                         &pqgParams, &pqgVerify);
    else 
	rv = PK11_PQG_ParamGen((unsigned)j, &pqgParams, &pqgVerify);

    if (rv != SECSuccess || pqgParams == NULL) {
	fprintf(stderr, "%s: PQG parameter generation failed.\n", progName);
	goto loser;
    } 
    fprintf(stderr, "%s: PQG parameter generation completed.\n", progName);

    o = outputPQGParams(pqgParams, output_binary, output_raw, outFile);
    o = outputPQGVerify(pqgVerify, output_binary, output_raw, outFile);

    rv = PK11_PQG_VerifyParams(pqgParams, pqgVerify, &passed);
    if (rv != SECSuccess) {
	fprintf(stderr, "%s: PQG parameter verification aborted.\n", progName);
	goto loser;
    }
    if (passed != SECSuccess) {
	fprintf(stderr, "%s: PQG parameters failed verification.\n", progName);
	goto loser;
    } 
    fprintf(stderr, "%s: PQG parameters passed verification.\n", progName);

    PK11_PQG_DestroyParams(pqgParams);
    PK11_PQG_DestroyVerify(pqgVerify);
    return 0;

loser:
    PK11_PQG_DestroyParams(pqgParams);
    PK11_PQG_DestroyVerify(pqgVerify);
    return 1;
}
