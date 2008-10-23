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
 * $Id: shlibsign.c,v 1.17 2008/09/30 04:32:43 nelson%bolyard.com Exp $
 */

#ifdef XP_UNIX
#define USES_LINKS 1
#endif

#include "nspr.h"
#include <stdio.h>
#include "nss.h"
#include "secutil.h"
#include "cert.h"
#include "pk11func.h"

#include "plgetopt.h"
#include "pk11sdr.h"
#include "shsign.h"
#include "pk11pqg.h"

#ifdef USES_LINKS
#include <unistd.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#endif

static void
usage (char *program_name)
{
    PRFileDesc *pr_stderr;

    pr_stderr = PR_STDERR;
    PR_fprintf (pr_stderr,
      "Usage:%s [-v] [-o outfile] [-d dbdir] [-f pwfile] [-p pwd]\n"
      "      -i shared_library_name\n", program_name);
}

static char *
mkoutput(const char *input)
{
    int in_len = PORT_Strlen(input);
    char *output = PORT_Alloc(in_len+sizeof(SGN_SUFFIX));
    int index = in_len + 1 - sizeof("."SHLIB_SUFFIX);

    if ((index > 0) && 
	(PORT_Strncmp(&input[index],
			"."SHLIB_SUFFIX,sizeof("."SHLIB_SUFFIX)) == 0)) {
	in_len = index;
    }
    PORT_Memcpy(output,input,in_len);
    PORT_Memcpy(&output[in_len],SGN_SUFFIX,sizeof(SGN_SUFFIX));
    return output;
}


static void
lperror(const char *string)
{
     int errNum = PORT_GetError();
     const char *error = SECU_Strerror(errNum);
     fprintf(stderr,"%s: %s\n",string, error);
}

static void
encodeInt(unsigned char *buf, int val)
{
    buf[3] = (val >> 0) & 0xff;
    buf[2] = (val >>  8) & 0xff;
    buf[1] = (val >> 16) & 0xff;
    buf[0] = (val >> 24) & 0xff;
    return;
}

static SECStatus 
writeItem(PRFileDesc *fd, SECItem *item, char *file)
{
    unsigned char buf[4];
    int bytesWritten;

    encodeInt(buf,item->len);
    bytesWritten = PR_Write(fd,buf, 4);
    if (bytesWritten != 4) {
	lperror(file);
	return SECFailure;
    }
    bytesWritten = PR_Write(fd, item->data, item->len);
    if (bytesWritten != item->len) {
	lperror(file);
	return SECFailure;
    }
    return SECSuccess;
}

static const unsigned char prime[] = { 0x00,
   0x97, 0x44, 0x1d, 0xcc, 0x0d, 0x39, 0x0d, 0x8d, 
   0xcb, 0x75, 0xdc, 0x24, 0x25, 0x6f, 0x01, 0x92, 
   0xa1, 0x11, 0x07, 0x6b, 0x70, 0xac, 0x73, 0xd7, 
   0x82, 0x28, 0xdf, 0xab, 0x82, 0x0c, 0x41, 0x0c, 
   0x95, 0xb3, 0x3c, 0x3d, 0xea, 0x8a, 0xe6, 0x44, 
   0x0a, 0xb8, 0xab, 0x90, 0x15, 0x41, 0x11, 0xe8, 
   0x48, 0x7b, 0x8d, 0xb0, 0x9c, 0xd3, 0xf2, 0x69, 
   0x66, 0xff, 0x66, 0x4b, 0x70, 0x2b, 0xbf, 0xfb, 
   0xd6, 0x68, 0x85, 0x76, 0x1e, 0x34, 0xaa, 0xc5, 
   0x57, 0x6e, 0x23, 0x02, 0x08, 0x60, 0x6e, 0xfd, 
   0x67, 0x76, 0xe1, 0x7c, 0xc8, 0xcb, 0x51, 0x77, 
   0xcf, 0xb1, 0x3b, 0x00, 0x2e, 0xfa, 0x21, 0xcd, 
   0x34, 0x76, 0x75, 0x01, 0x19, 0xfe, 0xf8, 0x5d, 
   0x43, 0xc5, 0x34, 0xf3, 0x7a, 0x95, 0xdc, 0xc2, 
   0x58, 0x07, 0x19, 0x2f, 0x1d, 0x6f, 0x9a, 0x77, 
   0x7e, 0x55, 0xaa, 0xe7, 0x5a, 0x50, 0x43, 0xd3 };

static const unsigned char subprime[] = { 0x0,
   0xd8, 0x16, 0x23, 0x34, 0x8a, 0x9e, 0x3a, 0xf5, 
   0xd9, 0x10, 0x13, 0x35, 0xaa, 0xf3, 0xf3, 0x54, 
   0x0b, 0x31, 0x24, 0xf1 };

static const unsigned char base[] = { 
    0x03, 0x3a, 0xad, 0xfa, 0x3a, 0x0c, 0xea, 0x0a, 
    0x4e, 0x43, 0x32, 0x92, 0xbb, 0x87, 0xf1, 0x11, 
    0xc0, 0xad, 0x39, 0x38, 0x56, 0x1a, 0xdb, 0x23, 
    0x66, 0xb1, 0x08, 0xda, 0xb6, 0x19, 0x51, 0x42, 
    0x93, 0x4f, 0xc3, 0x44, 0x43, 0xa8, 0x05, 0xc1, 
    0xf8, 0x71, 0x62, 0x6f, 0x3d, 0xe2, 0xab, 0x6f, 
    0xd7, 0x80, 0x22, 0x6f, 0xca, 0x0d, 0xf6, 0x9f, 
    0x45, 0x27, 0x83, 0xec, 0x86, 0x0c, 0xda, 0xaa, 
    0xd6, 0xe0, 0xd0, 0x84, 0xfd, 0xb1, 0x4f, 0xdc, 
    0x08, 0xcd, 0x68, 0x3a, 0x77, 0xc2, 0xc5, 0xf1, 
    0x99, 0x0f, 0x15, 0x1b, 0x6a, 0x8c, 0x3d, 0x18, 
    0x2b, 0x6f, 0xdc, 0x2b, 0xd8, 0xb5, 0x9b, 0xb8, 
    0x2d, 0x57, 0x92, 0x1c, 0x46, 0x27, 0xaf, 0x6d, 
    0xe1, 0x45, 0xcf, 0x0b, 0x3f, 0xfa, 0x07, 0xcc, 
    0x14, 0x8e, 0xe7, 0xb8, 0xaa, 0xd5, 0xd1, 0x36, 
    0x1d, 0x7e, 0x5e, 0x7d, 0xfa, 0x5b, 0x77, 0x1f };

static const unsigned char h[] = { 
    0x41, 0x87, 0x47, 0x79, 0xd8, 0xba, 0x4e, 0xac, 
    0x44, 0x4f, 0x6b, 0xd2, 0x16, 0x5e, 0x04, 0xc6, 
    0xc2, 0x29, 0x93, 0x5e, 0xbd, 0xc7, 0xa9, 0x8f, 
    0x23, 0xa1, 0xc8, 0xee, 0x80, 0x64, 0xd5, 0x67, 
    0x3c, 0xba, 0x59, 0x9a, 0x06, 0x0c, 0xcc, 0x29, 
    0x56, 0xc0, 0xb2, 0x21, 0xe0, 0x5b, 0x52, 0xcd, 
    0x84, 0x73, 0x57, 0xfd, 0xd8, 0xc3, 0x5b, 0x13, 
    0x54, 0xd7, 0x4a, 0x06, 0x86, 0x63, 0x09, 0xa5, 
    0xb0, 0x59, 0xe2, 0x32, 0x9e, 0x09, 0xa3, 0x9f, 
    0x49, 0x62, 0xcc, 0xa6, 0xf9, 0x54, 0xd5, 0xb2, 
    0xc3, 0x08, 0x71, 0x7e, 0xe3, 0x37, 0x50, 0xd6, 
    0x7b, 0xa7, 0xc2, 0x60, 0xc1, 0xeb, 0x51, 0x32, 
    0xfa, 0xad, 0x35, 0x25, 0x17, 0xf0, 0x7f, 0x23, 
    0xe5, 0xa8, 0x01, 0x52, 0xcf, 0x2f, 0xd9, 0xa9, 
    0xf6, 0x00, 0x21, 0x15, 0xf1, 0xf7, 0x70, 0xb7, 
    0x57, 0x8a, 0xd0, 0x59, 0x6a, 0x82, 0xdc, 0x9c };

static const unsigned char seed[] = { 0x00,
    0xcc, 0x4c, 0x69, 0x74, 0xf6, 0x72, 0x24, 0x68, 
    0x24, 0x4f, 0xd7, 0x50, 0x11, 0x40, 0x81, 0xed, 
    0x19, 0x3c, 0x8a, 0x25, 0xbc, 0x78, 0x0a, 0x85, 
    0x82, 0x53, 0x70, 0x20, 0xf6, 0x54, 0xa5, 0x1b, 
    0xf4, 0x15, 0xcd, 0xff, 0xc4, 0x88, 0xa7, 0x9d, 
    0xf3, 0x47, 0x1c, 0x0a, 0xbe, 0x10, 0x29, 0x83, 
    0xb9, 0x0f, 0x4c, 0xdf, 0x90, 0x16, 0x83, 0xa2, 
    0xb3, 0xe3, 0x2e, 0xc1, 0xc2, 0x24, 0x6a, 0xc4, 
    0x9d, 0x57, 0xba, 0xcb, 0x0f, 0x18, 0x75, 0x00, 
    0x33, 0x46, 0x82, 0xec, 0xd6, 0x94, 0x77, 0xc3, 
    0x4f, 0x4c, 0x58, 0x1c, 0x7f, 0x61, 0x3c, 0x36, 
    0xd5, 0x2f, 0xa5, 0x66, 0xd8, 0x2f, 0xce, 0x6e, 
    0x8e, 0x20, 0x48, 0x4a, 0xbb, 0xe3, 0xe0, 0xb2, 
    0x50, 0x33, 0x63, 0x8a, 0x5b, 0x2d, 0x6a, 0xbe, 
    0x4c, 0x28, 0x81, 0x53, 0x5b, 0xe4, 0xf6, 0xfc, 
    0x64, 0x06, 0x13, 0x51, 0xeb, 0x4a, 0x91, 0x9c };

#define MK_SECITEM(bb) { siBuffer, (unsigned char *)(bb), sizeof(bb) }

static PQGParams pqgParams = {
    NULL,                   /* arena */
    MK_SECITEM(prime),      /* P     */
    MK_SECITEM(subprime),   /* Q     */
    MK_SECITEM(base)        /* G     */
};

static PQGVerify pqgVerify = {
    NULL,              /* arena   */
    1496,              /* counter */
    MK_SECITEM(seed),  /* seed    */
    MK_SECITEM(h)      /* h       */
};



int
main (int argc, char **argv)
{
    int		 retval = 1;  /* 0 - test succeeded.  1 - test failed */
    SECStatus	 rv;
    PLOptState	*optstate;
    char	*program_name;
    const char  *input_file = NULL; 	/* read encrypted data from here (or create) */
    char  *output_file = NULL;	/* write new encrypted data here */
    PRBool      verbose = PR_FALSE;
    SECKEYPrivateKey *privk = NULL;
    SECKEYPublicKey *pubk = NULL;
    PK11SlotInfo *slot = NULL;
    PRFileDesc *fd;
    int bytesRead;
    int bytesWritten;
    unsigned char file_buf[512];
    unsigned char hash_buf[SHA1_LENGTH];
    unsigned char sign_buf[40]; /* DSA_LENGTH */
    SECItem hash,sign;
    PK11Context *hashcx = NULL;
    int count=0;
    int keySize = 1024;
    const char *nssDir = NULL;
    secuPWData  pwdata = { PW_NONE, 0 };
#ifdef USES_LINKS
    int ret;
    struct stat stat_buf;
    char link_buf[MAXPATHLEN+1];
    char *link_file = NULL;
#endif

    hash.len = sizeof(hash_buf); hash.data = hash_buf;
    sign.len = sizeof(sign_buf); sign.data = sign_buf;

    program_name = PL_strrchr(argv[0], '/');
    program_name = program_name ? (program_name + 1) : argv[0];

    optstate = PL_CreateOptState (argc, argv, "d:f:i:o:p:v");
    if (optstate == NULL) {
	SECU_PrintError (program_name, "PL_CreateOptState failed");
	return 1;
    }

    while (PL_GetNextOpt (optstate) == PL_OPT_OK) {
	switch (optstate->option) {
#ifdef notdef
	  case '?':
	    short_usage (program_name);
	    return 0;

	  case 'H':
	    long_usage (program_name);
	    return 0;
#endif

	  case 'd':
	    nssDir = optstate->value;
	    break;

          case 'i':
            input_file = optstate->value;
            break;

          case 'o':
            output_file = PORT_Strdup(optstate->value);
            break;

          case 'f':
            pwdata.source = PW_FROMFILE;
            pwdata.data = PORT_Strdup(optstate->value);
            break;

          case 'p':
            pwdata.source = PW_PLAINTEXT;
            pwdata.data = PORT_Strdup(optstate->value);
            break;

          case 'v':
            verbose = PR_TRUE;
            break;
	}
    }

    if (input_file == NULL) {
	usage(program_name);
	return 1;
    }

    /*
     * Initialize the Security libraries.
     */
    PK11_SetPasswordFunc(SECU_GetModulePassword);

    if (nssDir) {
        rv = NSS_Init(nssDir);
        if (rv != SECSuccess) {
            rv = NSS_NoDB_Init("");
        }
    } else {
        rv = NSS_NoDB_Init("");
    }
    
    if (rv != SECSuccess) {
	lperror("NSS_Init failed");
	goto prdone;
    }
    
    /* Generate a DSA Key pair */
    slot = PK11_GetBestSlot(CKM_DSA,&pwdata);
    if (slot == NULL) {
	lperror("CKM_DSA");
	goto loser;
	
    }
    printf("Generating DSA Key Pair...."); fflush(stdout);
    privk = PK11_GenerateKeyPair(slot, CKM_DSA_KEY_PAIR_GEN, &pqgParams, &pubk, 
						PR_FALSE, PR_TRUE, &pwdata);
    if (privk == NULL) {
	lperror("Generating DSA Key");
	goto loser;
    }

    printf("done\n");

    /* open the shared library */
    fd = PR_OpenFile(input_file,PR_RDONLY,0);
    if (fd == NULL ) {
	lperror(input_file);
	goto loser;
    }
#ifdef USES_LINKS
    ret = lstat(input_file, &stat_buf);
    if (ret < 0) {
	perror(input_file);
	goto loser;
    }
    if (S_ISLNK(stat_buf.st_mode)) {
	char *dirpath,*dirend;
	ret = readlink(input_file, link_buf, sizeof(link_buf) - 1);
	if (ret < 0) {
	   perror(input_file);
	   goto loser;
	}
	link_buf[ret] = 0;
	link_file = mkoutput(input_file);
	/* get the dirname of input_file */
	dirpath = PORT_Strdup(input_file);
	dirend = PORT_Strrchr(dirpath, '/');
	if (dirend) {
	    *dirend = '\0';
	    ret = chdir(dirpath);
	    if (ret < 0) {
		perror(dirpath);
		goto loser;
	    }
	}
	PORT_Free(dirpath);
	input_file = link_buf;
	/* get the basename of link_file */
	dirend = PORT_Strrchr(link_file, '/');
	if (dirend) {
	    link_file = dirend + 1;
	}
    }
#endif
    if (output_file == NULL) {
	output_file = mkoutput(input_file);
    }

    hashcx = PK11_CreateDigestContext(SEC_OID_SHA1);
    if (hashcx == NULL) {
	lperror("SHA1 Digest Create");
	goto loser;
    }

    /* hash the file */
    while ((bytesRead = PR_Read(fd,file_buf,sizeof(file_buf))) > 0) {
	PK11_DigestOp(hashcx,file_buf,bytesRead);
	count += bytesRead;
    }

    PR_Close(fd);
    fd = NULL;
    if (bytesRead < 0) {
	lperror(input_file);
	goto loser;
    }


    PK11_DigestFinal(hashcx, hash.data, &hash.len, hash.len);

    if (hash.len != SHA1_LENGTH) {
	fprintf(stderr, "Digest length was not correct\n");
	goto loser;
    }

    /* signe the hash */
    rv = PK11_Sign(privk,&sign,&hash);
    if (rv != SECSuccess) {
	lperror("Signing");
	goto loser;
    }

    if (verbose) {
	int i,j;
	fprintf(stderr,"Library File: %s %d bytes\n",input_file, count);
	fprintf(stderr,"Check File: %s\n",output_file);
#ifdef USES_LINKS
	if (link_file) {
	    fprintf(stderr,"Link: %s\n",link_file);
	}
#endif
	fprintf(stderr,"  hash: %d bytes\n", hash.len);
#define STEP 10
	for (i=0; i < hash.len; i += STEP) {
	   fprintf(stderr,"   ");
	   for (j=0; j < STEP && (i+j) < hash.len; j++) {
		fprintf(stderr," %02x", hash.data[i+j]);
	   }
	   fprintf(stderr,"\n");
	}
	fprintf(stderr,"  signature: %d bytes\n", sign.len);
	for (i=0; i < sign.len; i += STEP) {
	   fprintf(stderr,"   ");
	   for (j=0; j < STEP && (i+j) < sign.len; j++) {
		fprintf(stderr," %02x", sign.data[i+j]);
	   }
	   fprintf(stderr,"\n");
	}
    }

    /* open the target signature file */
    fd = PR_OpenFile(output_file,PR_WRONLY|PR_CREATE_FILE|PR_TRUNCATE,0666);
    if (fd == NULL ) {
	lperror(output_file);
	goto loser;
    }

    /*
     * we write the key out in a straight binary format because very
     * low level libraries need to read an parse this file. Ideally we should
     * just derEncode the public key (which would be pretty simple, and be
     * more general), but then we'd need to link the ASN.1 decoder with the
     * freebl libraries.
     */

    file_buf[0] = NSS_SIGN_CHK_MAGIC1;
    file_buf[1] = NSS_SIGN_CHK_MAGIC2;
    file_buf[2] = NSS_SIGN_CHK_MAJOR_VERSION;
    file_buf[3] = NSS_SIGN_CHK_MINOR_VERSION;
    encodeInt(&file_buf[4],12);			/* offset to data start */
    encodeInt(&file_buf[8],CKK_DSA);
    bytesWritten = PR_Write(fd,file_buf, 12);
    if (bytesWritten != 12) {
	lperror(output_file);
	goto loser;
    }

    rv = writeItem(fd,&pubk->u.dsa.params.prime,output_file);
    if (rv != SECSuccess) goto loser;
    rv = writeItem(fd,&pubk->u.dsa.params.subPrime,output_file);
    if (rv != SECSuccess) goto loser;
    rv = writeItem(fd,&pubk->u.dsa.params.base,output_file);
    if (rv != SECSuccess) goto loser;
    rv = writeItem(fd,&pubk->u.dsa.publicValue,output_file);
    if (rv != SECSuccess) goto loser;
    rv = writeItem(fd,&sign,output_file);
    if (rv != SECSuccess) goto loser;

    PR_Close(fd);

#ifdef USES_LINKS
    if (link_file) {
	(void)unlink(link_file);
	ret = symlink(output_file, link_file);
	if (ret < 0) {
	   perror(link_file);
	   goto loser;
	}
    }
#endif

    retval = 0;

loser:
    if (hashcx) {
        PK11_DestroyContext(hashcx, PR_TRUE);
    }
    if (privk) {
        SECKEY_DestroyPrivateKey(privk);
    }
    if (pubk) {
        SECKEY_DestroyPublicKey(pubk);
    }
    if (slot) {
        PK11_FreeSlot(slot);
    }
    if (pwdata.data) {
        PORT_Free(pwdata.data);
    }
    if (NSS_Shutdown() != SECSuccess) {
	exit(1);
    }

prdone:
    PR_Cleanup ();
    return retval;
}
