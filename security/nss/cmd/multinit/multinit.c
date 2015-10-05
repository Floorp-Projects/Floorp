/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "nss.h"
#include "secutil.h"
#include "pk11pub.h"
#include "cert.h"

typedef struct commandDescriptStr {
    int required;
    char *arg;
    char *des;
} commandDescript;

enum optionNames {
    opt_liborder = 0, 
    opt_mainDB, 
    opt_lib1DB,
    opt_lib2DB,
    opt_mainRO,
    opt_lib1RO,
    opt_lib2RO,
    opt_mainCMD,
    opt_lib1CMD,
    opt_lib2CMD,
    opt_mainTokNam,
    opt_lib1TokNam,
    opt_lib2TokNam,
    opt_oldStyle,
    opt_verbose,
    opt_summary,
    opt_help,
    opt_last
};


static const
secuCommandFlag options_init[] =
{
   { /* opt_liborder */  'o', PR_TRUE, "1M2zmi", PR_TRUE,  "order" },
   { /* opt_mainDB */    'd', PR_TRUE,     0,    PR_FALSE, "main_db" },
   { /* opt_lib1DB */    '1', PR_TRUE,     0,    PR_FALSE, "lib1_db" },
   { /* opt_lib2DB */    '2', PR_TRUE,     0,    PR_FALSE, "lib2_db" },
   { /* opt_mainRO */    'r', PR_FALSE,    0,    PR_FALSE, "main_readonly" },
   { /* opt_lib1RO */     0,  PR_FALSE,    0,    PR_FALSE, "lib1_readonly" },
   { /* opt_lib2RO */     0,  PR_FALSE,    0,    PR_FALSE, "lib2_readonly" },
   { /* opt_mainCMD */   'c', PR_TRUE,     0,    PR_FALSE, "main_command" },
   { /* opt_lib1CMD */    0,  PR_TRUE,     0,    PR_FALSE, "lib1_command" },
   { /* opt_lib2CMD */    0,  PR_TRUE,     0,    PR_FALSE, "lib2_command" },
   { /* opt_mainTokNam */'t', PR_TRUE,     0,    PR_FALSE, "main_token_name" },
   { /* opt_lib1TokNam */ 0,  PR_TRUE,     0,    PR_FALSE, "lib1_token_name" },
   { /* opt_lib2TokNam */ 0,  PR_TRUE,     0,    PR_FALSE, "lib2_token_name" },
   { /* opt_oldStype */  's', PR_FALSE,    0,    PR_FALSE, "oldStype" },
   { /* opt_verbose */   'v', PR_FALSE,    0,    PR_FALSE, "verbose" },
   { /* opt_summary */   'z', PR_FALSE,    0,    PR_FALSE, "summary" },
   { /* opt_help */      'h', PR_FALSE,    0,    PR_FALSE, "help" }
};

static const
commandDescript options_des[] =
{
   { /* opt_liborder */  PR_FALSE, "initOrder", 
	" Specifies the order of NSS initialization and shutdown. Order is\n"
	" given as a string where each character represents either an init or\n"
	" a shutdown of the main program or one of the 2 test libraries\n"
	" (library 1 and library 2). The valid characters are as follows:\n"
	"   M Init the main program\n   1 Init library 1\n"
	"   2 Init library 2\n"
	"   m Shutdown the main program\n   i Shutdown library 1\n"
	"   z Shutdown library 2\n" },
   { /* opt_mainDB */   PR_TRUE, "nss_db",
	" Specified the directory to open the nss database for the main\n" 
	" program. Must be specified if \"M\" is given in the order string\n"},
   { /* opt_lib1DB */   PR_FALSE, "nss_db",
	" Specified the directory to open the nss database for library 1.\n" 
	" Must be specified if \"1\" is given in the order string\n"},
   { /* opt_lib2DB */   PR_FALSE, "nss_db",
	" Specified the directory to open the nss database for library 2.\n" 
	" Must be specified if \"2\" is given in the order string\n"},
   { /* opt_mainRO */   PR_FALSE,    NULL,
	" Open the main program's database read only.\n" },
   { /* opt_lib1RO */   PR_FALSE,    NULL,
	" Open library 1's database read only.\n" },
   { /* opt_lib2RO */   PR_FALSE,    NULL,
	" Open library 2's database read only.\n" },
   { /* opt_mainCMD */  PR_FALSE,  "nss_command",
	" Specifies the NSS command to execute in the main program.\n"
	" Valid commands are: \n"
	"   key_slot, list_slots, list_certs, add_cert, none.\n"
	" Default is \"none\".\n" },
   { /* opt_lib1CMD */  PR_FALSE,  "nss_command",
	" Specifies the NSS command to execute in library 1.\n" },
   { /* opt_lib2CMD */  PR_FALSE,  "nss_command",
	" Specifies the NSS command to execute in library 2.\n" },
   { /* opt_mainTokNam */PR_FALSE,  "token_name",
	" Specifies the name of PKCS11 token for the main program's "
	"database.\n" },
   { /* opt_lib1TokNam */PR_FALSE,  "token_name",
	" Specifies the name of PKCS11 token for library 1's database.\n" },
   { /* opt_lib2TokNam */PR_FALSE,  "token_name",
	" Specifies the name of PKCS11 token for library 2's database.\n" },
   { /* opt_oldStype */ PR_FALSE,   NULL,
	" Use NSS_Shutdown rather than NSS_ShutdownContext in the main\n"
	" program.\n" },
   { /* opt_verbose */  PR_FALSE,   NULL,
	" Noisily output status to standard error\n" },
   { /* opt_summarize */ PR_FALSE,  NULL, 
	"report a summary of the test results\n" },
   { /* opt_help */ PR_FALSE,   NULL, " give this message\n" }
}; 

/*
 * output our short help (table driven). (does not exit).
 */
static void
short_help(const char *prog)
{
    int count = opt_last;
    int i,words_found;

    /* make sure all the tables are up to date before we allow compiles to
     * succeed */
    PR_STATIC_ASSERT(sizeof(options_init)/sizeof(secuCommandFlag) == opt_last);
    PR_STATIC_ASSERT(sizeof(options_init)/sizeof(secuCommandFlag) == 
		     sizeof(options_des)/sizeof(commandDescript));

    /* print the base usage */
    fprintf(stderr,"usage: %s ",prog);
    for (i=0, words_found=0; i < count; i++) {
	if (!options_des[i].required) {
	    fprintf(stderr,"[");
	}
	if (options_init[i].longform) {
	    fprintf(stderr, "--%s", options_init[i].longform);
	    words_found++;
	} else {
	    fprintf(stderr, "-%c", options_init[i].flag);
	}
	if (options_init[i].needsArg) {
	    if (options_des[i].arg) {
		fprintf(stderr," %s",options_des[i].arg);
	    } else {
		fprintf(stderr," arg");
	    }
	    words_found++;
	}
	if (!options_des[i].required) {
	    fprintf(stderr,"]");
	}
	if (i < count-1 ) {
	    if (words_found >= 5) {
 		fprintf(stderr,"\n      ");
		words_found=0;
	    } else {
		fprintf(stderr," ");
	    }
	}
    }
    fprintf(stderr,"\n");
}

/*
 * print out long help. like short_help, this does not exit
 */
static void
long_help(const char *prog)
{
    int i;
    int count = opt_last;

    short_help(prog);
    /* print the option descriptions */
    fprintf(stderr,"\n");
    for (i=0; i < count; i++) {
	fprintf(stderr,"        ");
	if (options_init[i].flag) {
	    fprintf(stderr, "-%c", options_init[i].flag);
	    if (options_init[i].longform) {
		fprintf(stderr,",");
	    }
	}
	if (options_init[i].longform) {
	    fprintf(stderr,"--%s", options_init[i].longform);
	}
	if (options_init[i].needsArg) {
	    if (options_des[i].arg) {
		fprintf(stderr," %s",options_des[i].arg);
	    } else {
		fprintf(stderr," arg");
	    }
	    if (options_init[i].arg) {
		fprintf(stderr," (default = \"%s\")",options_init[i].arg);
	    }
	}
	fprintf(stderr,"\n%s",options_des[i].des);
    }
}

/*
 * record summary data
 */
struct bufferData {
   char * data;		/* lowest address of the buffer */
   char * next;		/* pointer to the next element on the buffer */
   int  len;		/* length of the buffer */
};

/* our actual buffer. If data is NULL, then all append ops 
 * except are noops */
static struct bufferData buffer= { NULL, NULL, 0 };

#define CHUNK_SIZE 1000

/*
 * get our initial data. and set the buffer variables up. on failure,
 * just don't initialize the buffer.
 */
static void
initBuffer(void)
{
   buffer.data = PORT_Alloc(CHUNK_SIZE);
   if (!buffer.data) {
	return;
   }
   buffer.next = buffer.data;
   buffer.len = CHUNK_SIZE;
}

/*
 * grow the buffer. If we can't get more data, record a 'D' in the second
 * to last record and allow the rest of the data to overwrite the last
 * element.
 */
static void
growBuffer(void)
{
   char *new = PORT_Realloc(buffer.data, buffer.len + CHUNK_SIZE);
   if (!new) {
	buffer.data[buffer.len-2] = 'D'; /* signal malloc failure in summary */
	/* buffer must always point to good memory if it exists */
	buffer.next = buffer.data + (buffer.len -1);
	return;
   }
   buffer.next = new + (buffer.next-buffer.data);
   buffer.data = new;
   buffer.len += CHUNK_SIZE;
}

/*
 * append a label, doubles as appending a single character.
 */
static void
appendLabel(char label)
{
    if (!buffer.data) {
	return;
    }

    *buffer.next++ = label;
    if (buffer.data+buffer.len >= buffer.next) {
	growBuffer();
    }
}

/*
 * append a string onto the buffer. The result will be <string>
 */
static void
appendString(char *string)
{
    if (!buffer.data) {
	return;
    }

    appendLabel('<');
    while (*string) {
	appendLabel(*string++);
    }
    appendLabel('>');
}

/*
 * append a bool, T= true, F=false
 */
static void
appendBool(PRBool bool)
{
    if (!buffer.data) {
	return;
    }

    if (bool) {
	appendLabel('t');
    } else {
	appendLabel('f');
    }
}

/*
 * append a single hex nibble.
 */
static void
appendHex(unsigned char nibble)
{
    if (nibble <= 9) {
	appendLabel('0'+nibble);
    } else {
	appendLabel('a'+nibble-10);
    }
}

/*
 * append a secitem as colon separated hex bytes.
 */
static void
appendItem(SECItem *item)
{
    int i;

    if (!buffer.data) {
	return;
    }

    appendLabel(':');
    for (i=0; i < item->len; i++) {
	unsigned char byte=item->data[i];
	appendHex(byte >> 4);
	appendHex(byte & 0xf);
	appendLabel(':');
    }
}

/*
 * append a 32 bit integer (even on a 64 bit platform).
 * for simplicity append it as a hex value, full extension with 0x prefix.
 */
static void
appendInt(unsigned int value)
{
    int i;

    if (!buffer.data) {
	return;
    }

    appendLabel('0');
    appendLabel('x');
    value = value & 0xffffffff; /* only look at the buttom 8 bytes */
    for (i=0; i < 8; i++) {
	appendHex(value >> 28 );
	value = value << 4;
    }
}

/* append a trust flag */
static void
appendFlags(unsigned int flag)
{
  char trust[10];
  char *cp=trust;

  trust[0] = 0;
  printflags(trust, flag);
  while (*cp) {
    appendLabel(*cp++);
  }
}

/*
 * dump our buffer out with a result= flag so we can find it easily.
 * free the buffer as a side effect.
 */
static void
dumpBuffer(void)
{
    if (!buffer.data) {
	return;
    }

    appendLabel(0); /* terminate */
    printf("\nresult=%s\n",buffer.data);
    PORT_Free(buffer.data);
    buffer.data = buffer.next = NULL;
    buffer.len = 0;
}


/*
 * usage, like traditional usage, automatically exit
 */
static void
usage(const char *prog)
{
    short_help(prog);
    dumpBuffer();
    exit(1);
}

/*
 * like usage, except prints the long version of help
 */
static void
usage_long(const char *prog)
{
    long_help(prog);
    dumpBuffer();
    exit(1);
}

static const char *
bool2String(PRBool bool) 
{ 
    return bool ? "true" : "false";
}

/*
 * print out interesting info about the given slot
 */
void
print_slot(PK11SlotInfo *slot, int log)
{
    if (log) {
	fprintf(stderr, "* Name=%s Token_Name=%s present=%s, ro=%s *\n",
		PK11_GetSlotName(slot), PK11_GetTokenName(slot),
		bool2String(PK11_IsPresent(slot)), 
		bool2String(PK11_IsReadOnly(slot)));
    }
    appendLabel('S');
    appendString(PK11_GetTokenName(slot));
    appendBool(PK11_IsPresent(slot));
    appendBool(PK11_IsReadOnly(slot));
}

/*
 * list all our slots
 */
void
do_list_slots(const char *progName, int log)
{
   PK11SlotList *list;
   PK11SlotListElement *le;

   list= PK11_GetAllTokens(CKM_INVALID_MECHANISM, PR_FALSE, PR_FALSE, NULL);
   if (list == NULL) {
	fprintf(stderr,"ERROR: no tokens found %s\n", 
		SECU_Strerror(PORT_GetError()));
	appendLabel('S');
	appendString("none");
	return;
   }

   for (le= PK11_GetFirstSafe(list); le; 
				le = PK11_GetNextSafe(list,le,PR_TRUE)) {
	print_slot(le->slot, log);
   }
   PK11_FreeSlotList(list);
}

static PRBool
sort_CN(CERTCertificate *certa, CERTCertificate *certb, void *arg)
{
    char *commonNameA, *commonNameB;
    int ret;

    commonNameA = CERT_GetCommonName(&certa->subject);
    commonNameB = CERT_GetCommonName(&certb->subject);

    if (commonNameA == NULL) {
	PORT_Free(commonNameB);
	return PR_TRUE;
    }
    if (commonNameB == NULL) {
	PORT_Free(commonNameA);
	return PR_FALSE;
    }
    ret = PORT_Strcmp(commonNameA,commonNameB);
    PORT_Free(commonNameA);
    PORT_Free(commonNameB);
    return (ret < 0) ? PR_TRUE: PR_FALSE;
}

/*
 * list all the certs
 */
void
do_list_certs(const char *progName, int log)
{
   CERTCertList *list;
   CERTCertList *sorted;
   CERTCertListNode *node;
   CERTCertTrust trust;
   int i;

   list = PK11_ListCerts(PK11CertListUnique, NULL);
   if (list == NULL) {
	fprintf(stderr,"ERROR: no certs found %s\n", 
		SECU_Strerror(PORT_GetError()));
	appendLabel('C');
	appendString("none");
	return;
   }

   sorted = CERT_NewCertList();
   if (sorted == NULL) {
	fprintf(stderr,"ERROR: no certs found %s\n", 
		SECU_Strerror(PORT_GetError()));
	appendLabel('C');
	appendLabel('E');
	appendInt(PORT_GetError());
	return;
   }

   /* sort the list */
   for (node = CERT_LIST_HEAD(list); !CERT_LIST_END(node,list); 
				node = CERT_LIST_NEXT(node)) {
	CERT_AddCertToListSorted(sorted, node->cert, sort_CN, NULL);
   }
    

   for (node = CERT_LIST_HEAD(sorted); !CERT_LIST_END(node,sorted); 
				node = CERT_LIST_NEXT(node)) {
	CERTCertificate *cert = node->cert;
	char *commonName;

	SECU_PrintCertNickname(node, stderr);
	if (log) {
	    fprintf(stderr, "*	Slot=%s*\n", cert->slot ?
		 PK11_GetTokenName(cert->slot) : "none");
	    fprintf(stderr, "*	Nickname=%s*\n", cert->nickname);
	    fprintf(stderr, "*	Subject=<%s>*\n", cert->subjectName);
	    fprintf(stderr, "*	Issuer=<%s>*\n", cert->issuerName);
	    fprintf(stderr, "*	SN=");
	    for (i=0; i < cert->serialNumber.len; i++) {
		if (i!=0) fprintf(stderr,":");
		fprintf(stderr, "%02x",cert->serialNumber.data[0]);
	    }
	    fprintf(stderr," *\n");
	}
	appendLabel('C');
	commonName = CERT_GetCommonName(&cert->subject);
	appendString(commonName?commonName:"*NoName*");
	PORT_Free(commonName);
	if (CERT_GetCertTrust(cert, &trust) == SECSuccess) {
	    appendFlags(trust.sslFlags);
	    appendFlags(trust.emailFlags);
	    appendFlags(trust.objectSigningFlags);
	}
   }
   CERT_DestroyCertList(list);

}

/*
 * need to implement yet... try to add a new certificate
 */
void
do_add_cert(const char *progName, int log)
{
  PORT_Assert(/* do_add_cert not implemented */ 0);
}

/*
 * display the current key slot
 */
void
do_key_slot(const char *progName, int log)
{
   PK11SlotInfo *slot = PK11_GetInternalKeySlot();
   if (!slot) {
	fprintf(stderr,"ERROR: no internal key slot found %s\n", 
		SECU_Strerror(PORT_GetError()));
	appendLabel('K');
	appendLabel('S');
	appendString("none");
   }
   print_slot(slot, log);
   PK11_FreeSlot(slot);
}

/*
 * execute some NSS command.
 */
void
do_command(const char *label, int initialized, secuCommandFlag *command, 
	   const char *progName, int log)
{
   char * command_string;
   if (!initialized) {
	return;
   }

   if (command->activated) {
	command_string = command->arg;
   } else {
	command_string = "none";
   }

   if (log) {
	fprintf(stderr, "*Executing nss command \"%s\" for %s*\n", 
						command_string,label);
   }

   /* do something */
   if (PORT_Strcasecmp(command_string, "list_slots") == 0) {
	do_list_slots(progName, log);
   } else if (PORT_Strcasecmp(command_string, "list_certs") == 0) {
	do_list_certs(progName, log);
   } else if (PORT_Strcasecmp(command_string, "add_cert") == 0) {
	do_add_cert(progName, log);
   } else if (PORT_Strcasecmp(command_string, "key_slot") == 0) {
	do_key_slot(progName, log);
   } else if (PORT_Strcasecmp(command_string, "none") != 0) {
	fprintf(stderr, ">> Unknown command (%s)\n", command_string);
	appendLabel('E');
	appendString("bc");
	usage_long(progName);
   }

}


/*
 * functions do handle
 * different library initializations.
 */
static int main_initialized;
static int lib1_initialized;
static int lib2_initialized;

void
main_Init(secuCommandFlag *db, secuCommandFlag *tokNam,
	  int readOnly, const char *progName, int log)
{
    SECStatus rv;
    if (log) {
	fprintf(stderr,"*NSS_Init for the main program*\n");
    }
    appendLabel('M');
    if (!db->activated) { 
	fprintf(stderr, ">> No main_db has been specified\n");
	usage(progName);
    }
    if (main_initialized) {
	fprintf(stderr,"Warning: Second initialization of Main\n");
	appendLabel('E');
	appendString("2M");
    }
    if (tokNam->activated) {
	PK11_ConfigurePKCS11(NULL, NULL, NULL, tokNam->arg,
			 NULL, NULL, NULL, NULL, 0, 0);
    }
    rv = NSS_Initialize(db->arg, "", "", "", 
		NSS_INIT_NOROOTINIT|(readOnly?NSS_INIT_READONLY:0));
    if (rv != SECSuccess) {
	appendLabel('E');
	appendInt(PORT_GetError());
	fprintf(stderr,">> %s\n", SECU_Strerror(PORT_GetError()));
	dumpBuffer();
	exit(1);
    }
    main_initialized = 1;
}

void
main_Do(secuCommandFlag *command, const char *progName, int log) 
{
    do_command("main", main_initialized, command, progName, log);
}

void
main_Shutdown(int old_style, const char *progName, int log)
{
    SECStatus rv;
    appendLabel('N');
    if (log) {
	fprintf(stderr,"*NSS_Shutdown for the main program*\n");
    }
    if (!main_initialized) {
	fprintf(stderr,"Warning: Main shutdown without corresponding init\n");
    }
    if (old_style) {
	rv = NSS_Shutdown();
    } else {
	rv = NSS_ShutdownContext(NULL);
    }
    fprintf(stderr, "Shutdown main state = %d\n", rv);
    if (rv != SECSuccess) {
	appendLabel('E');
	appendInt(PORT_GetError());
	fprintf(stderr,"ERROR: %s\n", SECU_Strerror(PORT_GetError()));
    }
    main_initialized = 0;
}

/* common library init */
NSSInitContext *
lib_Init(const char *lableString, char label, int initialized, 
	 secuCommandFlag *db, secuCommandFlag *tokNam, int readonly, 
	 const char *progName, int log) 
{
    NSSInitContext *ctxt;
    NSSInitParameters initStrings;
    NSSInitParameters *initStringPtr = NULL;

    appendLabel(label);
    if (log) {
	fprintf(stderr,"*NSS_Init for %s*\n", lableString);
    }

    if (!db->activated) { 
	fprintf(stderr, ">> No %s_db has been specified\n", lableString);
	usage(progName);
    }
    if (initialized) {
	fprintf(stderr,"Warning: Second initialization of %s\n", lableString);
    }
    if (tokNam->activated) {
	PORT_Memset(&initStrings, 0, sizeof(initStrings));
	initStrings.length = sizeof(initStrings);
	initStrings.dbTokenDescription = tokNam->arg;
	initStringPtr = &initStrings;
    }
    ctxt = NSS_InitContext(db->arg, "", "", "", initStringPtr,
		NSS_INIT_NOROOTINIT|(readonly?NSS_INIT_READONLY:0));
    if (ctxt == NULL) {
	appendLabel('E');
	appendInt(PORT_GetError());
	fprintf(stderr,">> %s\n",SECU_Strerror(PORT_GetError()));
	dumpBuffer();
	exit(1);
    }
    return ctxt;
}

/* common library shutdown */
void
lib_Shutdown(const char *labelString, char label, NSSInitContext *ctx, 
	     int initialize, const char *progName, int log)
{
    SECStatus rv;
    appendLabel(label);
    if (log) {
	fprintf(stderr,"*NSS_Shutdown for %s\n*", labelString);
    }
    if (!initialize) {
	fprintf(stderr,"Warning: %s shutdown without corresponding init\n",
		 labelString);
    }
    rv = NSS_ShutdownContext(ctx);
    fprintf(stderr, "Shutdown %s state = %d\n", labelString, rv);
    if (rv != SECSuccess) {
	appendLabel('E');
	appendInt(PORT_GetError());
	fprintf(stderr,"ERROR: %s\n", SECU_Strerror(PORT_GetError()));
    }
}


static NSSInitContext *lib1_context;
static NSSInitContext *lib2_context;
void
lib1_Init(secuCommandFlag *db, secuCommandFlag *tokNam,
	  int readOnly, const char *progName, int log)
{
    lib1_context = lib_Init("lib1", '1', lib1_initialized, db, tokNam,
			     readOnly, progName, log);
    lib1_initialized = 1;
}

void
lib2_Init(secuCommandFlag *db, secuCommandFlag *tokNam,
	  int readOnly, const char *progName, int log) 
{
    lib2_context = lib_Init("lib2", '2', lib2_initialized,
			    db, tokNam, readOnly, progName, log);
    lib2_initialized = 1;
}

void    
lib1_Do(secuCommandFlag *command, const char *progName, int log) 
{
    do_command("lib1", lib1_initialized, command, progName, log);
}

void
lib2_Do(secuCommandFlag *command, const char *progName, int log) 
{
    do_command("lib2", lib2_initialized, command, progName, log);
}

void
lib1_Shutdown(const char *progName, int log) 
{
     lib_Shutdown("lib1", 'I', lib1_context, lib1_initialized, progName, log);
     lib1_initialized = 0;
     /* don't clear lib1_Context, so we can test multiple attempts to close
      * the same context produces correct errors*/
}

void
lib2_Shutdown(const char *progName, int log) 
{
    lib_Shutdown("lib2", 'Z', lib2_context, lib2_initialized, progName, log);
    lib2_initialized = 0;
    /* don't clear lib2_Context, so we can test multiple attempts to close
     * the same context produces correct errors*/
}

int
main(int argc, char **argv)
{
   SECStatus rv;
   secuCommand libinit;
   char *progName;
   char *order;
   secuCommandFlag *options;
   int log = 0;

   progName = strrchr(argv[0], '/');
   progName = progName ? progName+1 : argv[0];

   libinit.numCommands = 0;
   libinit.commands = 0; 
   libinit.numOptions = opt_last;
   options = (secuCommandFlag *)PORT_Alloc(sizeof(options_init));
   if (options == NULL) {
	fprintf(stderr, ">> %s:Not enough free memory to run command\n",
		progName);
	exit(1);
   }
   PORT_Memcpy(options, options_init, sizeof(options_init));
   libinit.options = options;

   rv = SECU_ParseCommandLine(argc, argv, progName, & libinit);
   if (rv != SECSuccess) {
	usage(progName);
   }

   if (libinit.options[opt_help].activated) {
	long_help(progName);
	exit (0);
   }

   log = libinit.options[opt_verbose].activated;
   if (libinit.options[opt_summary].activated) {
	initBuffer();
   }

   order = libinit.options[opt_liborder].arg;
   if (!order) {
	usage(progName);
   }

   if (log) {
	fprintf(stderr,"* initializing with order \"%s\"*\n", order);
   }

   for (;*order; order++) {
	switch (*order) {
	case 'M':
	    main_Init(&libinit.options[opt_mainDB],
		      &libinit.options[opt_mainTokNam],
		       libinit.options[opt_mainRO].activated,
		       progName, log);
	    break;
	case '1':
	    lib1_Init(&libinit.options[opt_lib1DB],
		      &libinit.options[opt_lib1TokNam],
		       libinit.options[opt_lib1RO].activated,
		        progName,log);
	    break;
	case '2':
	    lib2_Init(&libinit.options[opt_lib2DB],
		      &libinit.options[opt_lib2TokNam],
		       libinit.options[opt_lib2RO].activated,
		       progName,log);
	    break;
	case 'm':
	    main_Shutdown(libinit.options[opt_oldStyle].activated, 
			  progName, log);
	    break;
	case 'i':
	    lib1_Shutdown(progName, log);
	    break;
	case 'z':
	    lib2_Shutdown(progName, log);
	    break;
	default:
	    fprintf(stderr,">> Unknown init/shutdown command \"%c\"", *order);
	    usage_long(progName);
	}
	main_Do(&libinit.options[opt_mainCMD], progName, log);
	lib1_Do(&libinit.options[opt_lib1CMD], progName, log);
	lib2_Do(&libinit.options[opt_lib2CMD], progName, log);
   }

   if (NSS_IsInitialized()) {
	appendLabel('X');
	fprintf(stderr, "Warning: NSS is initialized\n");
   }
   dumpBuffer();

   exit(0);
}

