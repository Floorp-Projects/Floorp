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

#include "nspr.h"
#include "prtypes.h"
#include "prtime.h"
#include "prlong.h"
#include "nss.h"
#include "cmdutil.h"
#include "nsspki.h"
/* hmmm...*/
#include "pki.h"

#define PKIUTIL_VERSION_STRING "pkiutil version 0.1"

char *progName = NULL;

typedef struct {
    PRBool      raw;
    PRBool      ascii;
    char       *name;
    PRFileDesc *file;
} objOutputMode;

typedef enum {
    PKIUnknown = -1,
    PKICertificate,
    PKIPublicKey,
    PKIPrivateKey,
    PKIAny
} PKIObjectType;

static PKIObjectType
get_object_class(char *type)
{
    if (strcmp(type, "certificate") == 0 || strcmp(type, "cert") == 0 ||
        strcmp(type, "Certificate") == 0 || strcmp(type, "Cert") == 0) {
	return PKICertificate;
    } else if (strcmp(type, "public_key") == 0 || 
               strcmp(type, "PublicKey") == 0) {
	return PKIPublicKey;
    } else if (strcmp(type, "private_key") == 0 || 
               strcmp(type, "PrivateKey") == 0) {
	return PKIPrivateKey;
    } else if (strcmp(type, "all") == 0 || strcmp(type, "any") == 0) {
	return PKIAny;
    }
    fprintf(stderr, "%s: \"%s\" is not a valid PKCS#11 object type.\n",
                     progName, type);
    return PKIUnknown;
}

static PRStatus
print_cert_callback(NSSCertificate *c, void *arg)
{
    int i;
    NSSUTF8 *label;
    NSSItem *id;
    label = NSSCertificate_GetLabel(c);
    printf("%s\n", label);
    nss_ZFreeIf((void*)label);
#if 0
    id = NSSCertificate_GetID(c);
    for (i=0; i<id->size; i++) {
	printf("%c", ((char *)id->data)[i]);
    }
    printf("\n");
#endif
    return PR_SUCCESS;
}

/*  pkiutil commands  */
enum {
    cmd_Add = 0,
    cmd_Dump,
    cmd_List,
    cmd_Version,
    pkiutil_num_commands
};

/*  pkiutil options */
enum {
    opt_Help = 0,
    opt_Ascii,
    opt_ProfileDir,
    opt_TokenName,
    opt_InputFile,
    opt_Nickname,
    opt_OutputFile,
    opt_Binary,
    opt_Trust,
    opt_Type,
    pkiutil_num_options
};

static cmdCommandLineArg pkiutil_commands[] =
{
 { /* cmd_Add         */  'A', "add", CMDNoArg, 0, PR_FALSE, 
                           CMDBIT(opt_Nickname) | CMDBIT(opt_Trust), 
                           CMDBIT(opt_Ascii) | CMDBIT(opt_ProfileDir) 
                           | CMDBIT(opt_TokenName) | CMDBIT(opt_InputFile) 
                           | CMDBIT(opt_Binary) | CMDBIT(opt_Type) },
 { /* cmd_Dump        */   0 , "dump", CMDNoArg, 0, PR_FALSE,
                           CMDBIT(opt_Nickname),
                           CMDBIT(opt_Ascii) | CMDBIT(opt_ProfileDir) 
                           | CMDBIT(opt_TokenName) | CMDBIT(opt_Binary)
                           | CMDBIT(opt_Type) },
 { /* cmd_List        */  'L', "list", CMDNoArg, 0, PR_FALSE, 0, 
                           CMDBIT(opt_Ascii) | CMDBIT(opt_ProfileDir) 
                           | CMDBIT(opt_TokenName) | CMDBIT(opt_Binary)
                           | CMDBIT(opt_Nickname) | CMDBIT(opt_Type) },
 { /* cmd_Version     */  'Y', "version", CMDNoArg, 0, PR_FALSE, 0, 0 }
};

static cmdCommandLineOpt pkiutil_options[] =
{
 { /* opt_Help        */  '?', "help",     CMDNoArg,   0, PR_FALSE },
 { /* opt_Ascii       */  'a', "ascii",    CMDNoArg,   0, PR_FALSE },
 { /* opt_ProfileDir  */  'd', "dbdir",    CMDArgReq,  0, PR_FALSE },
 { /* opt_TokenName   */  'h', "token",    CMDArgReq,  0, PR_FALSE },
 { /* opt_InputFile   */  'i', "infile",   CMDArgReq,  0, PR_FALSE },
 { /* opt_Nickname    */  'n', "nickname", CMDArgReq,  0, PR_FALSE },
 { /* opt_OutputFile  */  'o', "outfile",  CMDArgReq,  0, PR_FALSE },
 { /* opt_Binary      */  'r', "raw",      CMDNoArg,   0, PR_FALSE },
 { /* opt_Trust       */  't', "trust",    CMDArgReq,  0, PR_FALSE },
 { /* opt_Type        */   0 , "type",     CMDArgReq,  0, PR_FALSE }
};

void pkiutil_usage(cmdPrintState *ps, 
                   int num, PRBool cmd, PRBool header, PRBool footer)
{
#define pusg CMD_PrintUsageString
    if (header) {
	pusg(ps, "utility for managing PKCS#11 objects (certs and keys)\n");
    } else if (footer) {
	/*
	printf("certificate trust can be:\n");
	printf(" p - valid peer, P - trusted peer (implies p)\n");
	printf(" c - valid CA\n");
	printf("  T - trusted CA to issue client certs (implies c)\n");
	printf("  C - trusted CA to issue server certs (implies c)\n");
	printf(" u - user cert\n");
	printf(" w - send warning\n");
	*/
    } else if (cmd) {
	switch(num) {
	case cmd_Add:     
	    pusg(ps, "Add an object to the token"); break;
	case cmd_Dump:
	    pusg(ps, "Dump a single object"); break;
	case cmd_List:    
	    pusg(ps, "List objects on the token (-n for single object)"); break;
	case cmd_Version: 
	    pusg(ps, "Report version"); break;
	default:
	    pusg(ps, "Unrecognized command"); break;
	}
    } else {
	switch(num) {
	case opt_Ascii:      
	    pusg(ps, "Use ascii (base-64 encoded) mode for I/O"); break;
	case opt_ProfileDir:    
	    pusg(ps, "Directory containing security databases (def: \".\")"); 
	    break;
	case opt_TokenName:  
	    pusg(ps, "Name of PKCS#11 token to use (def: internal)"); break;
	case opt_InputFile:  
	    pusg(ps, "File for input (def: stdin)"); break;
	case opt_Nickname:   
	    pusg(ps, "Nickname of object"); break;
	case opt_OutputFile: 
	    pusg(ps, "File for output (def: stdout)"); break;
	case opt_Binary:    
	    pusg(ps, "Use raw (binary der-encoded) mode for I/O"); break;
	case opt_Trust:      
	    pusg(ps, "Trust level for certificate"); break;
	case opt_Help: break;
	default:
	    pusg(ps, "Unrecognized option");
	}
    }
}

int 
main(int argc, char **argv)
{
    PRFileDesc     *infile        = NULL;
    PRFileDesc     *outfile       = NULL;
    char           *profiledir       = "./";
#if 0
    secuPWData      pwdata        = { PW_NONE, 0 };
#endif
    int             objclass      = 3; /* ANY */
    NSSTrustDomain *root_cert_td  = NULL;
    char           *rootpath      = NULL;
    char            builtin_name[]= "libnssckbi.so"; /* temporary hardcode */
    PRStatus        rv            = PR_SUCCESS;

    int cmdToRun;
    cmdCommand pkiutil;
    pkiutil.ncmd = pkiutil_num_commands;
    pkiutil.nopt = pkiutil_num_options;
    pkiutil.cmd = pkiutil_commands;
    pkiutil.opt = pkiutil_options;

    progName = strrchr(argv[0], '/');
    progName = progName ? progName+1 : argv[0];

    cmdToRun = CMD_ParseCommandLine(argc, argv, progName, &pkiutil);

#if 0
    { int i, nc;
    for (i=0; i<pkiutil.ncmd; i++)
	printf("%s: %s <%s>\n", pkiutil.cmd[i].s, 
	                        (pkiutil.cmd[i].on) ? "on" : "off",
				pkiutil.cmd[i].arg);
    for (i=0; i<pkiutil.nopt; i++)
	printf("%s: %s <%s>\n", pkiutil.opt[i].s, 
	                        (pkiutil.opt[i].on) ? "on" : "off",
				pkiutil.opt[i].arg);
    }
#endif

    if (pkiutil.opt[opt_Help].on)
	CMD_LongUsage(progName, &pkiutil, pkiutil_usage);

    if (cmdToRun < 0)
	CMD_Usage(progName, &pkiutil);

    /* -d */
    if (pkiutil.opt[opt_ProfileDir].on) {
	profiledir = strdup(pkiutil.opt[opt_ProfileDir].arg);
    }

    /* -i */
    if (pkiutil.opt[opt_InputFile].on) {
	char *fn = pkiutil.opt[opt_InputFile].arg;
	infile = PR_Open(fn, PR_RDONLY, 0660);
    } else {
	infile = PR_STDIN;
    }

    /* -o */
    if (pkiutil.opt[opt_OutputFile].on) {
	char *fn = pkiutil.opt[opt_OutputFile].arg;
	outfile = PR_Open(fn, PR_WRONLY | PR_CREATE_FILE, 0660);
    } else {
	outfile = PR_STDOUT;
    }

    /* --type can be found on many options */
    if (pkiutil.opt[opt_Type].on)
	objclass = get_object_class(pkiutil.opt[opt_Type].arg);
    else if (cmdToRun == cmd_Dump && pkiutil.cmd[cmd_Dump].arg)
	objclass = get_object_class(pkiutil.cmd[cmd_Dump].arg);
    else if (cmdToRun == cmd_List && pkiutil.cmd[cmd_List].arg)
	objclass = get_object_class(pkiutil.cmd[cmd_List].arg);
    else if (cmdToRun == cmd_Add && pkiutil.cmd[cmd_Add].arg)
	objclass = get_object_class(pkiutil.cmd[cmd_Add].arg);
    if (objclass < 0)
	goto done;

    /* --print is an alias for --list --nickname */
    if (cmdToRun == cmd_Dump) cmdToRun = cmd_List;

    /* if list has raw | ascii must have -n.  can't have both raw and ascii */
    if (pkiutil.opt[opt_Binary].on || pkiutil.opt[opt_Ascii].on) {
	if (cmdToRun == cmd_List && !pkiutil.opt[opt_Nickname].on) {
	    fprintf(stderr, "%s: specify a object to output with -n\n", 
	                     progName);
	    CMD_LongUsage(progName, &pkiutil, pkiutil_usage);
	}
    }

    /* initialize */
    PR_Init(PR_SYSTEM_THREAD, PR_PRIORITY_NORMAL, 1);
    /* NSS_InitReadWrite(profiledir); */
    NSS_NoDB_Init(NULL);

    /* Display version info and exit */
    if (cmdToRun == cmd_Version) {
	printf("%s\nNSS Version %s\n", PKIUTIL_VERSION_STRING, NSS_VERSION);
	goto done;
    }

    /* XXX okay - bootstrap stan by loading the root cert module for testing */
    root_cert_td = NSSTrustDomain_Create(NULL, NULL, NULL, NULL);
    {
	int rootpathlen = strlen(profiledir) + strlen(builtin_name) + 1;
	rootpath = (char *)malloc(rootpathlen);
	memcpy(rootpath, profiledir, strlen(profiledir));
	memcpy(rootpath + strlen(profiledir), 
               builtin_name, strlen(builtin_name));
	rootpath[rootpathlen - 1] = '\0';
    }
    NSSTrustDomain_LoadModule(root_cert_td, "Builtin Root Module", rootpath, 
                              NULL, NULL);

    printf("\n");
    if (pkiutil.opt[opt_Nickname].on) {
	int i;
	NSSCertificate **certs;
	NSSCertificate *cert;
	certs = NSSTrustDomain_FindCertificatesByNickname(root_cert_td,
			pkiutil.opt[opt_Nickname].arg, NULL, 0, NULL);
	i = 0;
	while ((cert = certs[i++]) != NULL) {
	    printf("Found cert:\n");
	    print_cert_callback(cert, NULL);
	}
    } else {
        NSSTrustDomain_TraverseCertificates(root_cert_td, print_cert_callback, 0);
    }

    NSSTrustDomain_Destroy(root_cert_td);

    /* List token objects */
    if (cmdToRun == cmd_List)  {
#if 0
	rv = list_token_objects(slot, objclass, 
	                        pkiutil.opt[opt_Nickname].arg,
	                        pkiutil.opt[opt_Binary].on,
	                        pkiutil.opt[opt_Ascii].on,
	                        outfile, &pwdata);
#endif
	goto done;
    }

#if 0
    /* Import an object into the token. */
    if (cmdToRun == cmd_Add) {
	rv = add_object_to_token(slot, object);
	goto done;
    }
#endif

done:
    NSS_Shutdown();

    return rv;
}
