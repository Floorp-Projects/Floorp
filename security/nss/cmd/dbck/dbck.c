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
** dbck.c
**
** utility for fixing corrupt cert databases
**
*/
#include <stdio.h>
#include <string.h>

#include "secutil.h"
#include "cdbhdl.h"
#include "certdb.h"
#include "cert.h"
#include "nspr.h"
#include "prtypes.h"
#include "prtime.h"
#include "prlong.h"

static char *progName;

/* placeholders for pointer error types */
static void *WrongEntry;
static void *NoNickname;
static void *NoSMime;

enum {
    GOBOTH = 0,
    GORIGHT,
    GOLEFT
};

typedef struct
{
    PRBool verbose;
    PRBool dograph;
    PRFileDesc *out;
    PRFileDesc *graphfile;
    int dbErrors[10];
} dbDebugInfo;

/*
 * A list node for a cert db entry.  The index is a unique identifier
 * to use for creating generic maps of a db.  This struct handles
 * the cert, nickname, and smime db entry types, as all three have a
 * single handle to a subject entry.
 * This structure is pointed to by certDBEntryListNode->appData.
 */
typedef struct 
{
    PRArenaPool *arena;
    int index;
    certDBEntryListNode *pSubject;
} certDBEntryMap;

/*
 * Subject entry is special case, it has bidirectional handles.  One
 * subject entry can point to several certs (using the same DN), and
 * a nickname and/or smime entry.
 * This structure is pointed to by certDBEntryListNode->appData.
 */
typedef struct
{
    PRArenaPool *arena;
    int index;
    int numCerts;
    certDBEntryListNode **pCerts;
    certDBEntryListNode *pNickname;
    certDBEntryListNode *pSMime;
} certDBSubjectEntryMap;

/*
 * A map of a certdb.
 */
typedef struct
{
    int numCerts;
    int numSubjects;
    int numNicknames;
    int numSMime;
    certDBEntryListNode certs;      /* pointer to head of cert list */
    certDBEntryListNode subjects;   /* pointer to head of subject list */
    certDBEntryListNode nicknames;  /* pointer to head of nickname list */
    certDBEntryListNode smime;      /* pointer to head of smime list */
} certDBArray;

/* Cast list to the base element, a certDBEntryListNode. */
#define LISTNODE_CAST(node) \
    ((certDBEntryListNode *)(node))

static void 
Usage(char *progName)
{
#define FPS fprintf(stderr, 
    FPS "Type %s -H for more detailed descriptions\n", progName);
    FPS "Usage:  %s -D [-d certdir] [-i dbname] [-m] [-v  [-f dumpfile]]\n", 
	progName);
    FPS "        %s -R -o newdbname [-d certdir] [-i dbname] [-aprsx] [-v [-f dumpfile]]\n", 
	progName);
    exit(-1);
}

static void
LongUsage(char *progName)
{
    FPS "%-15s Display this help message.\n",
	"-H");
    FPS "%-15s Dump analysis.  No changes will be made to the database.\n",
	"-D");
    FPS "%-15s Cert database directory (default is ~/.netscape)\n",
	"   -d certdir");
    FPS "%-15s Input cert database name (default is cert7.db)\n",
	"   -i dbname");
    FPS "%-15s Mail a graph of the database to certdb@netscape.com.\n",
	"   -m");
    FPS "%-15s This will produce an index graph of your cert db and send\n",
	"");
    FPS "%-15s it to Netscape for analysis.  Personal info will be removed.\n",
	"");
    FPS "%-15s Verbose mode.  Dumps the entire contents of your cert7.db.\n",
	"   -v");
    FPS "%-15s File to dump verbose output into.\n",
	"   -f dumpfile");
    FPS "%-15s Repair the database.  The program will look for broken\n",
	"-R");
    FPS "%-15s dependencies between subject entries and certificates,\n",
        "");
    FPS "%-15s between nickname entries and subjects, and between SMIME\n",
        "");
    FPS "%-15s profiles and subjects.  Any duplicate entries will be\n",
        "");
    FPS "%-15s removed, any missing entries will be created.\n",
        "");
    FPS "%-15s File to store new database in (default is new_cert7.db)\n",
	"   -o newdbname");
    FPS "%-15s Cert database directory (default is ~/.netscape)\n",
	"   -d certdir");
    FPS "%-15s Input cert database name (default is cert7.db)\n",
	"   -i dbname");
    FPS "%-15s Prompt before removing any certificates.\n",
        "   -p");
    FPS "%-15s Keep all possible certificates.  Only remove certificates\n",
	"   -a");
    FPS "%-15s which prevent creation of a consistent database.  Thus any\n",
	"");
    FPS "%-15s expired or redundant entries will be kept.\n",
	"");
    FPS "%-15s Keep redundant nickname/email entries.  It is possible\n",
	"   -r");
    FPS "%-15s only one such entry will be usable.\n",
	"");
    FPS "%-15s Don't require an S/MIME profile in order to keep an S/MIME\n",
	"   -s");
    FPS "%-15s cert.  An empty profile will be created.\n",
	"");
    FPS "%-15s Keep expired certificates.\n",
	"   -x");
    FPS "%-15s Verbose mode - report all activity while recovering db.\n",
	"   -v");
    FPS "%-15s File to dump verbose output into.\n",
	"   -f dumpfile");
    FPS "\n");
    exit(-1);
#undef FPS
}

/*******************************************************************
 *
 *  Functions for dbck.
 *
 ******************************************************************/

void
printHexString(PRFileDesc *out, SECItem *hexval)
{
    int i;
    for (i = 0; i < hexval->len; i++) {
	if (i != hexval->len - 1) {
	    PR_fprintf(out, "%02x:", hexval->data[i]);
	} else {
	    PR_fprintf(out, "%02x", hexval->data[i]);
	}
    }
    PR_fprintf(out, "\n");
}

typedef enum {
/* 0*/ NoSubjectForCert = 0,
/* 1*/ SubjectHasNoKeyForCert,
/* 2*/ NoNicknameOrSMimeForSubject,
/* 3*/ WrongNicknameForSubject,
/* 4*/ NoNicknameEntry,
/* 5*/ WrongSMimeForSubject,
/* 6*/ NoSMimeEntry,
/* 7*/ NoSubjectForNickname,
/* 8*/ NoSubjectForSMime,
/* 9*/ NicknameAndSMimeEntry
} dbErrorType;

static char *dbErrorString[] = {
/* 0*/ "<CERT ENTRY>\nDid not find a subject entry for this certificate.",
/* 1*/ "<SUBJECT ENTRY>\nSubject has certKey which is not in db.",
/* 2*/ "<SUBJECT ENTRY>\nSubject does not have a nickname or email address.",
/* 3*/ "<SUBJECT ENTRY>\nUsing this subject's nickname, found a nickname entry for a different subject.",
/* 4*/ "<SUBJECT ENTRY>\nDid not find a nickname entry for this subject.",
/* 5*/ "<SUBJECT ENTRY>\nUsing this subject's email, found an S/MIME entry for a different subject.",
/* 6*/ "<SUBJECT ENTRY>\nDid not find an S/MIME entry for this subject.",
/* 7*/ "<NICKNAME ENTRY>\nDid not find a subject entry for this nickname.",
/* 8*/ "<S/MIME ENTRY>\nDid not find a subject entry for this S/MIME profile.",
};

SECStatus
dumpCertificate(CERTCertificate *cert, int num, PRFileDesc *outfile)
{
    int userCert = 0;
    CERTCertTrust *trust = cert->trust;
    userCert = (SEC_GET_TRUST_FLAGS(trust, trustSSL) & CERTDB_USER) ||
               (SEC_GET_TRUST_FLAGS(trust, trustEmail) & CERTDB_USER) ||
               (SEC_GET_TRUST_FLAGS(trust, trustObjectSigning) & CERTDB_USER);
    if (num >= 0) {
	PR_fprintf(outfile, "Certificate: %3d\n", num);
    } else {
	PR_fprintf(outfile, "Certificate:\n");
    }
    PR_fprintf(outfile, "----------------\n");
    if (userCert)
	PR_fprintf(outfile, "(User Cert)\n");
    PR_fprintf(outfile, "## SUBJECT:  %s\n", cert->subjectName);
    PR_fprintf(outfile, "## ISSUER:  %s\n", cert->issuerName);
    PR_fprintf(outfile, "## SERIAL NUMBER:  ");
    printHexString(outfile, &cert->serialNumber);
    {  /*  XXX should be separate function.  */
	int64 timeBefore, timeAfter;
	PRExplodedTime beforePrintable, afterPrintable;
	char *beforestr, *afterstr;
	DER_DecodeTimeChoice(&timeBefore, &cert->validity.notBefore);
	DER_DecodeTimeChoice(&timeAfter, &cert->validity.notAfter);
	PR_ExplodeTime(timeBefore, PR_GMTParameters, &beforePrintable);
	PR_ExplodeTime(timeAfter, PR_GMTParameters, &afterPrintable);
	beforestr = PORT_Alloc(100);
	afterstr = PORT_Alloc(100);
	PR_FormatTime(beforestr, 100, "%a %b %d %H:%M:%S %Y", &beforePrintable);
	PR_FormatTime(afterstr, 100, "%a %b %d %H:%M:%S %Y", &afterPrintable);
	PR_fprintf(outfile, "## VALIDITY:  %s to %s\n", beforestr, afterstr);
    }
    PR_fprintf(outfile, "\n");
    return SECSuccess;
}

SECStatus
dumpCertEntry(certDBEntryCert *entry, int num, PRFileDesc *outfile)
{
    CERTCertificate *cert;
    cert = CERT_DecodeDERCertificate(&entry->derCert, PR_FALSE, NULL);
    if (!cert) {
	fprintf(stderr, "Failed to decode certificate.\n");
	return SECFailure;
    }
    cert->trust = &entry->trust;
    dumpCertificate(cert, num, outfile);
    CERT_DestroyCertificate(cert);
    return SECSuccess;
}

SECStatus
dumpSubjectEntry(certDBEntrySubject *entry, int num, PRFileDesc *outfile)
{
    char *subjectName;
    subjectName = CERT_DerNameToAscii(&entry->derSubject);
    PR_fprintf(outfile, "Subject: %3d\n", num);
    PR_fprintf(outfile, "------------\n");
    PR_fprintf(outfile, "## %s\n", subjectName);
    if (entry->nickname)
	PR_fprintf(outfile, "## Subject nickname:  %s\n", entry->nickname);
    if (entry->emailAddr && entry->emailAddr[0])
	PR_fprintf(outfile, "## Subject email address:  %s\n", 
	           entry->emailAddr);
    PR_fprintf(outfile, "## This subject has %d cert(s).\n", entry->ncerts);
    PR_fprintf(outfile, "\n");
    PORT_Free(subjectName);
    return SECSuccess;
}

SECStatus
dumpNicknameEntry(certDBEntryNickname *entry, int num, PRFileDesc *outfile)
{
    PR_fprintf(outfile, "Nickname: %3d\n", num);
    PR_fprintf(outfile, "-------------\n");
    PR_fprintf(outfile, "##  \"%s\"\n\n", entry->nickname);
    return SECSuccess;
}

SECStatus
dumpSMimeEntry(certDBEntrySMime *entry, int num, PRFileDesc *outfile)
{
    PR_fprintf(outfile, "S/MIME Profile: %3d\n", num);
    PR_fprintf(outfile, "-------------------\n");
    PR_fprintf(outfile, "##  \"%s\"\n", entry->emailAddr);
    PR_fprintf(outfile, "##  OPTIONS:  ");
    printHexString(outfile, &entry->smimeOptions);
    PR_fprintf(outfile, "##  TIMESTAMP:  ");
    printHexString(outfile, &entry->optionsDate);
    PR_fprintf(outfile, "\n");
    return SECSuccess;
}

SECStatus
mapCertEntries(certDBArray *dbArray)
{
    certDBEntryCert *certEntry;
    certDBEntrySubject *subjectEntry;
    certDBEntryListNode *certNode, *subjNode;
    certDBSubjectEntryMap *smap;
    certDBEntryMap *map;
    PRArenaPool *tmparena;
    SECItem derSubject;
    SECItem certKey;
    PRCList *cElem, *sElem;
    int i;

    /* Arena for decoded entries */
    tmparena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (tmparena == NULL) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	return SECFailure;
    }

    /* Iterate over cert entries and map them to subject entries. 
     * NOTE: mapSubjectEntries must be called first to alloc memory
     * for array of subject->cert map.
     */
    for (cElem = PR_LIST_HEAD(&dbArray->certs.link); 
         cElem != &dbArray->certs.link; cElem = PR_NEXT_LINK(cElem)) {
	certNode = LISTNODE_CAST(cElem);
	certEntry = (certDBEntryCert *)&certNode->entry;
	map = (certDBEntryMap *)certNode->appData;
	CERT_NameFromDERCert(&certEntry->derCert, &derSubject);
	CERT_KeyFromDERCert(tmparena, &certEntry->derCert, &certKey);
	/*  Loop over found subjects for cert's DN.  */
	for (sElem = PR_LIST_HEAD(&dbArray->subjects.link);
	     sElem != &dbArray->subjects.link; sElem = PR_NEXT_LINK(sElem)) {
	    subjNode = LISTNODE_CAST(sElem);
	    subjectEntry = (certDBEntrySubject *)&subjNode->entry;
	    if (SECITEM_ItemsAreEqual(&derSubject, &subjectEntry->derSubject)) {
		/*  Found matching subject name, create link.  */
		map->pSubject = subjNode;
		/*  Make sure subject entry has cert's key.  */
		for (i=0; i<subjectEntry->ncerts; i++) {
		    if (SECITEM_ItemsAreEqual(&certKey,
		                              &subjectEntry->certKeys[i])) {
			/*  Found matching cert key.  */
			smap = (certDBSubjectEntryMap *)subjNode->appData;
			smap->pCerts[i] = certNode;
			break;
		    }
		}
	    }
	}
    }
    PORT_FreeArena(tmparena, PR_FALSE);
    return SECSuccess;
}

SECStatus
mapSubjectEntries(certDBArray *dbArray)
{
    certDBEntrySubject *subjectEntry;
    certDBEntryNickname *nicknameEntry;
    certDBEntrySMime *smimeEntry;
    certDBEntryListNode *subjNode, *nickNode, *smimeNode;
    certDBSubjectEntryMap *subjMap;
    certDBEntryMap *nickMap, *smimeMap;
    PRCList *sElem, *nElem, *mElem;

    for (sElem = PR_LIST_HEAD(&dbArray->subjects.link);
         sElem != &dbArray->subjects.link; sElem = PR_NEXT_LINK(sElem)) {
	/* Iterate over subject entries and map subjects to nickname
	 * and smime entries.  The cert<->subject map will be handled
	 * by a subsequent call to mapCertEntries.
	 */
	subjNode = LISTNODE_CAST(sElem);
	subjectEntry = (certDBEntrySubject *)&subjNode->entry;
	subjMap = (certDBSubjectEntryMap *)subjNode->appData;
	/* need to alloc memory here for array of matching certs. */
	subjMap->pCerts = PORT_ArenaAlloc(subjMap->arena, 
	                                  subjectEntry->ncerts*sizeof(int));
	subjMap->numCerts = subjectEntry->ncerts;
	if (subjectEntry->nickname) {
	    /* Subject should have a nickname entry, so create a link. */
	    for (nElem = PR_LIST_HEAD(&dbArray->nicknames.link);
	         nElem != &dbArray->nicknames.link; 
	         nElem = PR_NEXT_LINK(nElem)) {
		/*  Look for subject's nickname in nickname entries.  */
		nickNode = LISTNODE_CAST(nElem);
		nicknameEntry = (certDBEntryNickname *)&nickNode->entry;
		nickMap = (certDBEntryMap *)nickNode->appData;
		if (PL_strcmp(subjectEntry->nickname, 
		              nicknameEntry->nickname) == 0) {
		    /*  Found a nickname entry for subject's nickname.  */
		    if (SECITEM_ItemsAreEqual(&subjectEntry->derSubject,
		                              &nicknameEntry->subjectName)) {
			/*  Nickname and subject match.  */
			subjMap->pNickname = nickNode;
			nickMap->pSubject = subjNode;
		    } else {
			/*  Nickname entry found is for diff. subject.  */
			subjMap->pNickname = WrongEntry;
		    }
		}
	    }
	} else {
	    subjMap->pNickname = NoNickname;
	}
	if (subjectEntry->emailAddr && subjectEntry->emailAddr[0]) {
	    /* Subject should have an smime entry, so create a link. */
	    for (mElem = PR_LIST_HEAD(&dbArray->smime.link);
	         mElem != &dbArray->smime.link; mElem = PR_NEXT_LINK(mElem)) {
		/*  Look for subject's email in S/MIME entries.  */
		smimeNode = LISTNODE_CAST(mElem);
		smimeEntry = (certDBEntrySMime *)&smimeNode->entry;
		smimeMap = (certDBEntryMap *)smimeNode->appData;
		if (PL_strcmp(subjectEntry->emailAddr, 
		              smimeEntry->emailAddr) == 0) {
		    /*  Found a S/MIME entry for subject's email.  */
		    if (SECITEM_ItemsAreEqual(&subjectEntry->derSubject,
		                              &smimeEntry->subjectName)) {
			/*  S/MIME entry and subject match.  */
			subjMap->pSMime = smimeNode;
			smimeMap->pSubject = subjNode;
		    } else {
			/*  S/MIME entry found is for diff. subject.  */
			subjMap->pSMime = WrongEntry;
		    }
		}
	    }
	} else {
	    subjMap->pSMime = NoSMime;
	}
    }
    return SECSuccess;
}

void
printnode(dbDebugInfo *info, const char *str, int num)
{
    if (!info->dograph)
	return;
    if (num < 0) {
	PR_fprintf(info->graphfile, str);
    } else {
	PR_fprintf(info->graphfile, str, num);
    }
}

PRBool
map_handle_is_ok(dbDebugInfo *info, void *mapPtr, int indent)
{
    if (mapPtr == NULL) {
	if (indent > 0)
	    printnode(info, "                ", -1);
	if (indent >= 0)
	    printnode(info, "******************* ", -1);
	return PR_FALSE;
    } else if (mapPtr == WrongEntry) {
	if (indent > 0)
	    printnode(info, "                  ", -1);
	if (indent >= 0)
	    printnode(info, "??????????????????? ", -1);
	return PR_FALSE;
    } else {
	return PR_TRUE;
    }
}

/* these call each other */
void print_smime_graph(dbDebugInfo *info, certDBEntryMap *smimeMap, 
                       int direction);
void print_nickname_graph(dbDebugInfo *info, certDBEntryMap *nickMap, 
                          int direction);
void print_subject_graph(dbDebugInfo *info, certDBSubjectEntryMap *subjMap, 
                         int direction, int optindex, int opttype);
void print_cert_graph(dbDebugInfo *info, certDBEntryMap *certMap, 
                      int direction);

/* Given an smime entry, print its unique identifier.  If GOLEFT is 
 * specified, print the cert<-subject<-smime map, else just print
 * the smime entry.
 */
void
print_smime_graph(dbDebugInfo *info, certDBEntryMap *smimeMap, int direction)
{
    certDBSubjectEntryMap *subjMap;
    certDBEntryListNode *subjNode;
    if (direction == GOLEFT) {
	/* Need to output subject and cert first, see print_subject_graph */
	subjNode = smimeMap->pSubject;
	if (map_handle_is_ok(info, (void *)subjNode, 1)) {
	    subjMap = (certDBSubjectEntryMap *)subjNode->appData; 
	    print_subject_graph(info, subjMap, GOLEFT,
	                        smimeMap->index, certDBEntryTypeSMimeProfile);
	} else {
	    printnode(info, "<---- S/MIME   %5d   ", smimeMap->index);
	}
    } else {
	printnode(info, "S/MIME   %5d   ", smimeMap->index);
    }
}

/* Given a nickname entry, print its unique identifier.  If GOLEFT is 
 * specified, print the cert<-subject<-nickname map, else just print
 * the nickname entry.
 */
void
print_nickname_graph(dbDebugInfo *info, certDBEntryMap *nickMap, int direction)
{
    certDBSubjectEntryMap *subjMap;
    certDBEntryListNode *subjNode;
    if (direction == GOLEFT) {
	/* Need to output subject and cert first, see print_subject_graph */
	subjNode = nickMap->pSubject;
	if (map_handle_is_ok(info, (void *)subjNode, 1)) {
	    subjMap = (certDBSubjectEntryMap *)subjNode->appData;
	    print_subject_graph(info, subjMap, GOLEFT,
	                        nickMap->index, certDBEntryTypeNickname);
	} else {
	    printnode(info, "<---- Nickname %5d   ", nickMap->index);
	}
    } else {
	printnode(info, "Nickname %5d   ", nickMap->index);
    }
}

/* Given a subject entry, if going right print the graph of the nickname|smime
 * that it maps to (by its unique identifier); and if going left
 * print the list of certs that it points to.
 */
void
print_subject_graph(dbDebugInfo *info, certDBSubjectEntryMap *subjMap, 
                    int direction, int optindex, int opttype)
{
    certDBEntryMap *map;
    certDBEntryListNode *node;
    int i;
    /* The first line of output always contains the cert id, subject id,
     * and nickname|smime id.  Subsequent lines may contain additional
     * cert id's for the subject if going left or both directions.
     * Ex. of printing the graph for a subject entry:
     * Cert 3 <- Subject 5 -> Nickname 32
     * Cert 8 /
     * Cert 9 /
     * means subject 5 has 3 certs, 3, 8, and 9, and corresponds
     * to nickname entry 32.
     * To accomplish the above, it is required to dump the entire first
     * line left-to-right, regardless of the input direction, and then
     * finish up any remaining cert entries.  Hence the code is uglier
     * than one may expect.
     */
    if (direction == GOLEFT || direction == GOBOTH) {
	/* In this case, nothing should be output until the first cert is
	 * located and output (cert 3 in the above example).
	 */
	if (subjMap->numCerts == 0 || subjMap->pCerts == NULL)
	    /* XXX uh-oh */
	    return;
	/* get the first cert and dump it. */
	node = subjMap->pCerts[0];
	if (map_handle_is_ok(info, (void *)node, 0)) {
	    map = (certDBEntryMap *)node->appData;
	    /* going left here stops. */
	    print_cert_graph(info, map, GOLEFT); 
	}
	/* Now it is safe to output the subject id. */
	if (direction == GOLEFT)
	    printnode(info, "Subject  %5d <---- ", subjMap->index);
	else /* direction == GOBOTH */
	    printnode(info, "Subject  %5d ----> ", subjMap->index);
    }
    if (direction == GORIGHT || direction == GOBOTH) { 
	/* Okay, now output the nickname|smime for this subject. */
	if (direction != GOBOTH) /* handled above */
	   printnode(info, "Subject  %5d ----> ", subjMap->index);
	if (subjMap->pNickname) {
	    node = subjMap->pNickname;
	    if (map_handle_is_ok(info, (void *)node, 0)) {
		map = (certDBEntryMap *)node->appData;
		/* going right here stops. */
		print_nickname_graph(info, map, GORIGHT);
	    }
	}
	if (subjMap->pSMime) {
	    node = subjMap->pSMime;
	    if (map_handle_is_ok(info, (void *)node, 0)) {
		map = (certDBEntryMap *)node->appData;
		/* going right here stops. */
		print_smime_graph(info, map, GORIGHT); 
	    }
	}
	if (!subjMap->pNickname && !subjMap->pSMime) {
	    printnode(info, "******************* ", -1);
	}
    }
    if (direction != GORIGHT) { /* going right has only one cert */
	if (opttype == certDBEntryTypeNickname)
	    printnode(info, "Nickname %5d   ", optindex);
	else if (opttype == certDBEntryTypeSMimeProfile)
	    printnode(info, "S/MIME   %5d   ", optindex);
	for (i=1 /* 1st one already done */; i<subjMap->numCerts; i++) {
	    printnode(info, "\n", -1); /* start a new line */
	    node = subjMap->pCerts[i];
	    if (map_handle_is_ok(info, (void *)node, 0)) {
		map = (certDBEntryMap *)node->appData;
		/* going left here stops. */
		print_cert_graph(info, map, GOLEFT); 
		printnode(info, "/", -1);
	    }
	}
    }
}

/* Given a cert entry, print its unique identifer.  If GORIGHT is specified,
 * print the cert->subject->nickname|smime map, else just print
 * the cert entry.
 */
void
print_cert_graph(dbDebugInfo *info, certDBEntryMap *certMap, int direction)
{
    certDBSubjectEntryMap *subjMap;
    certDBEntryListNode *subjNode;
    if (direction == GOLEFT) {
	printnode(info, "Cert     %5d <---- ", certMap->index);
	/* only want cert entry, terminate here. */
	return;
    }
    /* Keep going right then. */
    printnode(info, "Cert     %5d ----> ", certMap->index);
    subjNode = certMap->pSubject;
    if (map_handle_is_ok(info, (void *)subjNode, 0)) {
	subjMap = (certDBSubjectEntryMap *)subjNode->appData;
	print_subject_graph(info, subjMap, GORIGHT, -1, -1);
    }
}

SECStatus
computeDBGraph(certDBArray *dbArray, dbDebugInfo *info)
{
    PRCList *cElem, *sElem, *nElem, *mElem;
    certDBEntryListNode *node;
    certDBEntryMap *map;
    certDBSubjectEntryMap *subjMap;

    /* Graph is of this form:
     *
     * certs:
     * cert ---> subject ---> (nickname|smime)
     *
     * subjects:
     * cert <--- subject ---> (nickname|smime)
     *
     * nicknames and smime:
     * cert <--- subject <--- (nickname|smime)
     */

    /* Print cert graph. */
    for (cElem = PR_LIST_HEAD(&dbArray->certs.link);
         cElem != &dbArray->certs.link; cElem = PR_NEXT_LINK(cElem)) {
	/* Print graph of everything to right of cert entry. */
	node = LISTNODE_CAST(cElem);
	map = (certDBEntryMap *)node->appData;
	print_cert_graph(info, map, GORIGHT);
	printnode(info, "\n", -1);
    }
    printnode(info, "\n", -1);

    /* Print subject graph. */
    for (sElem = PR_LIST_HEAD(&dbArray->subjects.link);
         sElem != &dbArray->subjects.link; sElem = PR_NEXT_LINK(sElem)) {
	/* Print graph of everything to both sides of subject entry. */
	node = LISTNODE_CAST(sElem);
	subjMap = (certDBSubjectEntryMap *)node->appData;
	print_subject_graph(info, subjMap, GOBOTH, -1, -1);
	printnode(info, "\n", -1);
    }
    printnode(info, "\n", -1);

    /* Print nickname graph. */
    for (nElem = PR_LIST_HEAD(&dbArray->nicknames.link);
         nElem != &dbArray->nicknames.link; nElem = PR_NEXT_LINK(nElem)) {
	/* Print graph of everything to left of nickname entry. */
	node = LISTNODE_CAST(nElem);
	map = (certDBEntryMap *)node->appData;
	print_nickname_graph(info, map, GOLEFT);
	printnode(info, "\n", -1);
    }
    printnode(info, "\n", -1);

    /* Print smime graph. */
    for (mElem = PR_LIST_HEAD(&dbArray->smime.link);
         mElem != &dbArray->smime.link; mElem = PR_NEXT_LINK(mElem)) {
	/* Print graph of everything to left of smime entry. */
	node = LISTNODE_CAST(mElem);
	if (node == NULL) break;
	map = (certDBEntryMap *)node->appData;
	print_smime_graph(info, map, GOLEFT);
	printnode(info, "\n", -1);
    }
    printnode(info, "\n", -1);

    return SECSuccess;
}

/*
 * List the entries in the db, showing handles between entry types.
 */
void
verboseOutput(certDBArray *dbArray, dbDebugInfo *info)
{
    int i, ref;
    PRCList *elem;
    certDBEntryListNode *node;
    certDBEntryMap *map;
    certDBSubjectEntryMap *smap;
    certDBEntrySubject *subjectEntry;

    /* List certs */
    for (elem = PR_LIST_HEAD(&dbArray->certs.link);
         elem != &dbArray->certs.link; elem = PR_NEXT_LINK(elem)) {
	node = LISTNODE_CAST(elem);
	map = (certDBEntryMap *)node->appData;
	dumpCertEntry((certDBEntryCert*)&node->entry, map->index, info->out);
	/* walk the cert handle to it's subject entry */
	if (map_handle_is_ok(info, map->pSubject, -1)) {
	    smap = (certDBSubjectEntryMap *)map->pSubject->appData;
	    ref = smap->index;
	    PR_fprintf(info->out, "-->(subject %d)\n\n\n", ref);
	} else {
	    PR_fprintf(info->out, "-->(MISSING SUBJECT ENTRY)\n\n\n");
	}
    }
    /* List subjects */
    for (elem = PR_LIST_HEAD(&dbArray->subjects.link);
         elem != &dbArray->subjects.link; elem = PR_NEXT_LINK(elem)) {
	node = LISTNODE_CAST(elem);
	subjectEntry = (certDBEntrySubject *)&node->entry;
	smap = (certDBSubjectEntryMap *)node->appData;
	dumpSubjectEntry(subjectEntry, smap->index, info->out);
	/* iterate over subject's certs */
	for (i=0; i<smap->numCerts; i++) {
	    /* walk each subject handle to it's cert entries */
	    if (map_handle_is_ok(info, smap->pCerts[i], -1)) {
		ref = ((certDBEntryMap *)smap->pCerts[i]->appData)->index;
		PR_fprintf(info->out, "-->(%d. certificate %d)\n", i, ref);
	    } else {
		PR_fprintf(info->out, "-->(%d. MISSING CERT ENTRY)\n", i);
	    }
	}
	if (subjectEntry->nickname) {
	    /* walk each subject handle to it's nickname entry */
	    if (map_handle_is_ok(info, smap->pNickname, -1)) {
		ref = ((certDBEntryMap *)smap->pNickname->appData)->index;
		PR_fprintf(info->out, "-->(nickname %d)\n", ref);
	    } else {
		PR_fprintf(info->out, "-->(MISSING NICKNAME ENTRY)\n");
	    }
	}
	if (subjectEntry->emailAddr && subjectEntry->emailAddr[0]) {
	    /* walk each subject handle to it's smime entry */
	    if (map_handle_is_ok(info, smap->pSMime, -1)) {
		ref = ((certDBEntryMap *)smap->pSMime->appData)->index;
		PR_fprintf(info->out, "-->(s/mime %d)\n", ref);
	    } else {
		PR_fprintf(info->out, "-->(MISSING S/MIME ENTRY)\n");
	    }
	}
	PR_fprintf(info->out, "\n\n");
    }
    for (elem = PR_LIST_HEAD(&dbArray->nicknames.link);
         elem != &dbArray->nicknames.link; elem = PR_NEXT_LINK(elem)) {
	node = LISTNODE_CAST(elem);
	map = (certDBEntryMap *)node->appData;
	dumpNicknameEntry((certDBEntryNickname*)&node->entry, map->index, 
	                  info->out);
	if (map_handle_is_ok(info, map->pSubject, -1)) {
	    ref = ((certDBEntryMap *)map->pSubject->appData)->index;
	    PR_fprintf(info->out, "-->(subject %d)\n\n\n", ref);
	} else {
	    PR_fprintf(info->out, "-->(MISSING SUBJECT ENTRY)\n\n\n");
	}
    }
    for (elem = PR_LIST_HEAD(&dbArray->smime.link);
         elem != &dbArray->smime.link; elem = PR_NEXT_LINK(elem)) {
	node = LISTNODE_CAST(elem);
	map = (certDBEntryMap *)node->appData;
	dumpSMimeEntry((certDBEntrySMime*)&node->entry, map->index, info->out);
	if (map_handle_is_ok(info, map->pSubject, -1)) {
	    ref = ((certDBEntryMap *)map->pSubject->appData)->index;
	    PR_fprintf(info->out, "-->(subject %d)\n\n\n", ref);
	} else {
	    PR_fprintf(info->out, "-->(MISSING SUBJECT ENTRY)\n\n\n");
	}
    }
    PR_fprintf(info->out, "\n\n");
}

char *errResult[] = {
    "Certificate entries that had no subject entry.", 
    "Certificate entries that had no key in their subject entry.", 
    "Subject entries that had no nickname or email address.",
    "Redundant nicknames (subjects with the same nickname).",
    "Subject entries that had no nickname entry.",
    "Redundant email addresses (subjects with the same email address).",
    "Subject entries that had no S/MIME entry.",
    "Nickname entries that had no subject entry.", 
    "S/MIME entries that had no subject entry.",
};

int
fillDBEntryArray(CERTCertDBHandle *handle, certDBEntryType type, 
                 certDBEntryListNode *list)
{
    PRCList *elem;
    certDBEntryListNode *node;
    certDBEntryMap *mnode;
    certDBSubjectEntryMap *smnode;
    PRArenaPool *arena;
    int count = 0;
    /* Initialize a dummy entry in the list.  The list head will be the
     * next element, so this element is skipped by for loops.
     */
    PR_INIT_CLIST((PRCList *)list);
    /* Collect all of the cert db entries for this type into a list. */
    SEC_TraverseDBEntries(handle, type, SEC_GetCertDBEntryList, 
                          (PRCList *)list);
    for (elem = PR_LIST_HEAD(&list->link); 
         elem != &list->link; elem = PR_NEXT_LINK(elem)) {
	/* Iterate over the entries and ... */
	node = (certDBEntryListNode *)elem;
	if (type != certDBEntryTypeSubject) {
	    arena = PORT_NewArena(sizeof(*mnode));
	    mnode = (certDBEntryMap *)PORT_ArenaZAlloc(arena, sizeof(*mnode));
	    mnode->arena = arena;
	    /* ... assign a unique index number to each node, and ... */
	    mnode->index = count;
	    /* ... set the map pointer for the node. */
	    node->appData = (void *)mnode;
	} else {
	    /* allocate some room for the cert pointers also */
	    arena = PORT_NewArena(sizeof(*smnode) + 20*sizeof(void *));
	    smnode = (certDBSubjectEntryMap *)
	              PORT_ArenaZAlloc(arena, sizeof(*smnode));
	    smnode->arena = arena;
	    smnode->index = count;
	    node->appData = (void *)smnode;
	}
	count++;
    }
    return count;
}

void
freeDBEntryList(PRCList *list)
{
    PRCList *next, *elem;
    certDBEntryListNode *node;
    certDBEntryMap *map;

    for (elem = PR_LIST_HEAD(list); elem != list;) { 
	next = PR_NEXT_LINK(elem);
	node = (certDBEntryListNode *)elem;
	map = (certDBEntryMap *)node->appData;
	PR_REMOVE_LINK(&node->link);
	PORT_FreeArena(map->arena, PR_TRUE);
	PORT_FreeArena(node->entry.common.arena, PR_TRUE);
	elem = next;
    }
}

void
DBCK_DebugDB(CERTCertDBHandle *handle, PRFileDesc *out, PRFileDesc *mailfile)
{
    int i, nCertsFound, nSubjFound, nErr;
    int nCerts, nSubjects, nSubjCerts, nNicknames, nSMime;
    PRCList *elem;
    char c;
    dbDebugInfo info;
    certDBArray dbArray;

    PORT_Memset(&dbArray, 0, sizeof(dbArray));
    PORT_Memset(&info, 0, sizeof(info));
    info.verbose = (out == NULL) ? PR_FALSE : PR_TRUE ;
    info.dograph = (mailfile == NULL) ? PR_FALSE : PR_TRUE ;
    info.out = (out) ? out : PR_STDOUT;
    info.graphfile = mailfile;

    /*  Fill the array structure with cert/subject/nickname/smime entries.  */
    dbArray.numCerts = fillDBEntryArray(handle, certDBEntryTypeCert, 
                                        &dbArray.certs);
    dbArray.numSubjects = fillDBEntryArray(handle, certDBEntryTypeSubject, 
                                           &dbArray.subjects);
    dbArray.numNicknames = fillDBEntryArray(handle, certDBEntryTypeNickname, 
                                            &dbArray.nicknames);
    dbArray.numSMime = fillDBEntryArray(handle, certDBEntryTypeSMimeProfile, 
                                        &dbArray.smime);

    /*  Compute the map between the database entries.  */
    mapSubjectEntries(&dbArray);
    mapCertEntries(&dbArray);
    computeDBGraph(&dbArray, &info);

    /*  Store the totals for later reference.  */
    nCerts = dbArray.numCerts;
    nSubjects = dbArray.numSubjects;
    nNicknames = dbArray.numNicknames;
    nSMime = dbArray.numSMime;
    nSubjCerts = 0;
    for (elem = PR_LIST_HEAD(&dbArray.subjects.link);
         elem != &dbArray.subjects.link; elem = PR_NEXT_LINK(elem)) {
	certDBSubjectEntryMap *smap;
	smap = (certDBSubjectEntryMap *)LISTNODE_CAST(elem)->appData;
	nSubjCerts += smap->numCerts;
    }

    if (info.verbose) {
	/*  Dump the database contents.  */
	verboseOutput(&dbArray, &info);
    }

    freeDBEntryList(&dbArray.certs.link);
    freeDBEntryList(&dbArray.subjects.link);
    freeDBEntryList(&dbArray.nicknames.link);
    freeDBEntryList(&dbArray.smime.link);

    PR_fprintf(info.out, "\n");
    PR_fprintf(info.out, "Database statistics:\n");
    PR_fprintf(info.out, "N0: Found %4d Certificate entries.\n", 
                          nCerts);
    PR_fprintf(info.out, "N1: Found %4d Subject entries (unique DN's).\n", 
                          nSubjects);
    PR_fprintf(info.out, "N2: Found %4d Cert keys within Subject entries.\n", 
                          nSubjCerts);
    PR_fprintf(info.out, "N3: Found %4d Nickname entries.\n", 
                          nNicknames);
    PR_fprintf(info.out, "N4: Found %4d S/MIME entries.\n", 
                          nSMime);
    PR_fprintf(info.out, "\n");

    nErr = 0;
    for (i=0; i<sizeof(errResult)/sizeof(char*); i++) {
	PR_fprintf(info.out, "E%d: Found %4d %s\n", 
	           i, info.dbErrors[i], errResult[i]);
	nErr += info.dbErrors[i];
    }
    PR_fprintf(info.out, "--------------\n    Found %4d errors in database.\n", 
               nErr);

    PR_fprintf(info.out, "\nCertificates:\n");
    PR_fprintf(info.out, "N0 == N2 + E%d + E%d\n", NoSubjectForCert, 
                                                   SubjectHasNoKeyForCert);
    nCertsFound = nSubjCerts +
                  info.dbErrors[NoSubjectForCert] +
                  info.dbErrors[SubjectHasNoKeyForCert];
    c = (nCertsFound == nCerts) ? '=' : '!';
    PR_fprintf(info.out, "%d %c= %d + %d + %d\n", nCerts, c, nSubjCerts, 
                  info.dbErrors[NoSubjectForCert],
                  info.dbErrors[SubjectHasNoKeyForCert]);
    PR_fprintf(info.out, "\nSubjects:\n");
    PR_fprintf(info.out, "N1 == N3 + N4 + E%d + E%d + E%d + E%d + E%d - E%d - E%d\n",
                  NoNicknameOrSMimeForSubject, WrongNicknameForSubject,
		  NoNicknameEntry, WrongSMimeForSubject, NoSMimeEntry,
		  NoSubjectForNickname, NoSubjectForSMime);
    PR_fprintf(info.out, "      - #(subjects with both nickname and S/MIME entries)\n");
    nSubjFound = nNicknames + nSMime + 
                 info.dbErrors[NoNicknameOrSMimeForSubject] +
		 info.dbErrors[WrongNicknameForSubject] +
		 info.dbErrors[NoNicknameEntry] +
		 info.dbErrors[WrongSMimeForSubject] +
                 info.dbErrors[NoSMimeEntry] -
		 info.dbErrors[NoSubjectForNickname] -
		 info.dbErrors[NoSubjectForSMime] -
		 info.dbErrors[NicknameAndSMimeEntry];
    c = (nSubjFound == nSubjects) ? '=' : '!';
    PR_fprintf(info.out, "%d %c= %d + %d + %d + %d + %d + %d + %d - %d - %d - %d\n",
                  nSubjects, c, nNicknames, nSMime,
                  info.dbErrors[NoNicknameOrSMimeForSubject],
		  info.dbErrors[WrongNicknameForSubject],
		  info.dbErrors[NoNicknameEntry],
		  info.dbErrors[WrongSMimeForSubject],
                  info.dbErrors[NoSMimeEntry],
		  info.dbErrors[NoSubjectForNickname],
		  info.dbErrors[NoSubjectForSMime],
		  info.dbErrors[NicknameAndSMimeEntry]);
    PR_fprintf(info.out, "\n");
}

#ifdef DORECOVER
enum {
    dbInvalidCert = 0,
    dbNoSMimeProfile,
    dbOlderCert,
    dbBadCertificate,
    dbCertNotWrittenToDB
};

typedef struct dbRestoreInfoStr
{
    CERTCertDBHandle *handle;
    PRBool verbose;
    PRFileDesc *out;
    int nCerts;
    int nOldCerts;
    int dbErrors[5];
    PRBool removeType[3];
    PRBool promptUser[3];
} dbRestoreInfo;

char *
IsEmailCert(CERTCertificate *cert)
{
    char *email, *tmp1, *tmp2;
    PRBool isCA;
    int len;

    if (!cert->subjectName) {
	return NULL;
    }

    tmp1 = PORT_Strstr(cert->subjectName, "E=");
    tmp2 = PORT_Strstr(cert->subjectName, "MAIL=");
    /* XXX Nelson has cert for KTrilli which does not have either
     * of above but is email cert (has cert->emailAddr). 
     */
    if (!tmp1 && !tmp2 && !(cert->emailAddr && cert->emailAddr[0])) {
	return NULL;
    }

    /*  Server or CA cert, not personal email.  */
    isCA = CERT_IsCACert(cert, NULL);
    if (isCA)
	return NULL;

    /*  XXX CERT_IsCACert advertises checking the key usage ext.,
	but doesn't appear to. */
    /*  Check the key usage extension.  */
    if (cert->keyUsagePresent) {
	/*  Must at least be able to sign or encrypt (not neccesarily
	 *  both if it is one of a dual cert).  
	 */
	if (!((cert->rawKeyUsage & KU_DIGITAL_SIGNATURE) || 
              (cert->rawKeyUsage & KU_KEY_ENCIPHERMENT)))
	    return NULL;

	/*  CA cert, not personal email.  */
	if (cert->rawKeyUsage & (KU_KEY_CERT_SIGN | KU_CRL_SIGN))
	    return NULL;
    }

    if (cert->emailAddr && cert->emailAddr[0]) {
	email = PORT_Strdup(cert->emailAddr);
    } else {
	if (tmp1)
	    tmp1 += 2; /* "E="  */
	else
	    tmp1 = tmp2 + 5; /* "MAIL=" */
	len = strcspn(tmp1, ", ");
	email = (char*)PORT_Alloc(len+1);
	PORT_Strncpy(email, tmp1, len);
	email[len] = '\0';
    }

    return email;
}

SECStatus
deleteit(CERTCertificate *cert, void *arg)
{
    return SEC_DeletePermCertificate(cert);
}

/*  Different than DeleteCertificate - has the added bonus of removing
 *  all certs with the same DN.  
 */
SECStatus
deleteAllEntriesForCert(CERTCertDBHandle *handle, CERTCertificate *cert,
                        PRFileDesc *outfile)
{
#if 0
    certDBEntrySubject *subjectEntry;
    certDBEntryNickname *nicknameEntry;
    certDBEntrySMime *smimeEntry;
    int i;
#endif

    if (outfile) {
	PR_fprintf(outfile, "$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$\n\n");
	PR_fprintf(outfile, "Deleting redundant certificate:\n");
	dumpCertificate(cert, -1, outfile);
    }

    CERT_TraverseCertsForSubject(handle, cert->subjectList, deleteit, NULL);
#if 0
    CERT_LockDB(handle);
    subjectEntry = ReadDBSubjectEntry(handle, &cert->derSubject);
    /*  It had better be there, or created a bad db.  */
    PORT_Assert(subjectEntry);
    for (i=0; i<subjectEntry->ncerts; i++) {
	DeleteDBCertEntry(handle, &subjectEntry->certKeys[i]);
    }
    DeleteDBSubjectEntry(handle, &cert->derSubject);
    if (subjectEntry->emailAddr && subjectEntry->emailAddr[0]) {
	smimeEntry = ReadDBSMimeEntry(handle, subjectEntry->emailAddr);
	if (smimeEntry) {
	    if (SECITEM_ItemsAreEqual(&subjectEntry->derSubject,
	                              &smimeEntry->subjectName))
		/*  Only delete it if it's for this subject!  */
		DeleteDBSMimeEntry(handle, subjectEntry->emailAddr);
	    SEC_DestroyDBEntry((certDBEntry*)smimeEntry);
	}
    }
    if (subjectEntry->nickname) {
	nicknameEntry = ReadDBNicknameEntry(handle, subjectEntry->nickname);
	if (nicknameEntry) {
	    if (SECITEM_ItemsAreEqual(&subjectEntry->derSubject,
	                              &nicknameEntry->subjectName))
		/*  Only delete it if it's for this subject!  */
		DeleteDBNicknameEntry(handle, subjectEntry->nickname);
	    SEC_DestroyDBEntry((certDBEntry*)nicknameEntry);
	}
    }
    SEC_DestroyDBEntry((certDBEntry*)subjectEntry);
    CERT_UnlockDB(handle);
#endif
    return SECSuccess;
}

void
getCertsToDelete(char *numlist, int len, int *certNums, int nCerts)
{
    int j, num;
    char *numstr, *numend, *end;

    numstr = numlist;
    end = numstr + len - 1;
    while (numstr != end) {
	numend = strpbrk(numstr, ", \n");
	*numend = '\0';
	if (PORT_Strlen(numstr) == 0)
	    return;
	num = PORT_Atoi(numstr);
	if (numstr == numlist)
	    certNums[0] = num;
	for (j=1; j<nCerts+1; j++) {
	    if (num == certNums[j]) {
		certNums[j] = -1;
		break;
	    }
	}
	if (numend == end)
	    break;
	numstr = strpbrk(numend+1, "0123456789");
    }
}

PRBool
userSaysDeleteCert(CERTCertificate **certs, int nCerts,
                   int errtype, dbRestoreInfo *info, int *certNums)
{
    char response[32];
    int32 nb;
    int i;
    /*  User wants to remove cert without prompting.  */
    if (info->promptUser[errtype] == PR_FALSE)
	return (info->removeType[errtype]);
    switch (errtype) {
    case dbInvalidCert:
	PR_fprintf(PR_STDOUT, "********  Expired ********\n");
	PR_fprintf(PR_STDOUT, "Cert has expired.\n\n");
	dumpCertificate(certs[0], -1, PR_STDOUT);
	PR_fprintf(PR_STDOUT,
	           "Keep it? (y/n - this one, Y/N - all expired certs) [n] ");
	break;
    case dbNoSMimeProfile:
	PR_fprintf(PR_STDOUT, "********  No Profile ********\n");
	PR_fprintf(PR_STDOUT, "S/MIME cert has no profile.\n\n");
	dumpCertificate(certs[0], -1, PR_STDOUT);
	PR_fprintf(PR_STDOUT,
	      "Keep it? (y/n - this one, Y/N - all S/MIME w/o profile) [n] ");
	break;
    case dbOlderCert:
	PR_fprintf(PR_STDOUT, "*******  Redundant nickname/email *******\n\n");
	PR_fprintf(PR_STDOUT, "These certs have the same nickname/email:\n");
	for (i=0; i<nCerts; i++)
	    dumpCertificate(certs[i], i, PR_STDOUT);
	PR_fprintf(PR_STDOUT, 
	"Enter the certs you would like to keep from those listed above.\n");
	PR_fprintf(PR_STDOUT, 
	"Use a comma-separated list of the cert numbers (ex. 0, 8, 12).\n");
	PR_fprintf(PR_STDOUT, 
	"The first cert in the list will be the primary cert\n");
	PR_fprintf(PR_STDOUT, 
	" accessed by the nickname/email handle.\n");
	PR_fprintf(PR_STDOUT, 
	"List cert numbers to keep here, or hit enter\n");
	PR_fprintf(PR_STDOUT, 
	" to always keep only the newest cert:  ");
	break;
    default:
    }
    nb = PR_Read(PR_STDIN, response, sizeof(response));
    PR_fprintf(PR_STDOUT, "\n\n");
    if (errtype == dbOlderCert) {
	if (!isdigit(response[0])) {
	    info->promptUser[errtype] = PR_FALSE;
	    info->removeType[errtype] = PR_TRUE;
	    return PR_TRUE;
	}
	getCertsToDelete(response, nb, certNums, nCerts);
	return PR_TRUE;
    }
    /*  User doesn't want to be prompted for this type anymore.  */
    if (response[0] == 'Y') {
	info->promptUser[errtype] = PR_FALSE;
	info->removeType[errtype] = PR_FALSE;
	return PR_FALSE;
    } else if (response[0] == 'N') {
	info->promptUser[errtype] = PR_FALSE;
	info->removeType[errtype] = PR_TRUE;
	return PR_TRUE;
    }
    return (response[0] != 'y') ? PR_TRUE : PR_FALSE;
}

SECStatus
addCertToDB(certDBEntryCert *certEntry, dbRestoreInfo *info, 
            CERTCertDBHandle *oldhandle)
{
    SECStatus rv = SECSuccess;
    PRBool allowOverride;
    PRBool userCert;
    SECCertTimeValidity validity;
    CERTCertificate *oldCert = NULL;
    CERTCertificate *dbCert = NULL;
    CERTCertificate *newCert = NULL;
    CERTCertTrust *trust;
    certDBEntrySMime *smimeEntry = NULL;
    char *email = NULL;
    char *nickname = NULL;
    int nCertsForSubject = 1;

    oldCert = CERT_DecodeDERCertificate(&certEntry->derCert, PR_FALSE,
                                        certEntry->nickname);
    if (!oldCert) {
	info->dbErrors[dbBadCertificate]++;
	SEC_DestroyDBEntry((certDBEntry*)certEntry);
	return SECSuccess;
    }

    oldCert->dbEntry = certEntry;
    oldCert->trust = &certEntry->trust;
    oldCert->dbhandle = oldhandle;

    trust = oldCert->trust;

    info->nOldCerts++;

    if (info->verbose)
	PR_fprintf(info->out, "%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%\n\n");

    if (oldCert->nickname)
	nickname = PORT_Strdup(oldCert->nickname);

    /*  Always keep user certs.  Skip ahead.  */
    /*  XXX if someone sends themselves a signed message, it is possible
	for their cert to be imported as an "other" cert, not a user cert.
	this mucks with smime entries...  */
    userCert = (SEC_GET_TRUST_FLAGS(trust, trustSSL) & CERTDB_USER) ||
               (SEC_GET_TRUST_FLAGS(trust, trustEmail) & CERTDB_USER) ||
               (SEC_GET_TRUST_FLAGS(trust, trustObjectSigning) & CERTDB_USER);
    if (userCert)
	goto createcert;

    /*  If user chooses so, ignore expired certificates.  */
    allowOverride = (PRBool)((oldCert->keyUsage == certUsageSSLServer) ||
                         (oldCert->keyUsage == certUsageSSLServerWithStepUp));
    validity = CERT_CheckCertValidTimes(oldCert, PR_Now(), allowOverride);
    /*  If cert expired and user wants to delete it, ignore it. */
    if ((validity != secCertTimeValid) && 
	 userSaysDeleteCert(&oldCert, 1, dbInvalidCert, info, 0)) {
	info->dbErrors[dbInvalidCert]++;
	if (info->verbose) {
	    PR_fprintf(info->out, "Deleting expired certificate:\n");
	    dumpCertificate(oldCert, -1, info->out);
	}
	goto cleanup;
    }

    /*  New database will already have default certs, don't attempt
	to overwrite them.  */
    dbCert = CERT_FindCertByDERCert(info->handle, &oldCert->derCert);
    if (dbCert) {
	info->nCerts++;
	if (info->verbose) {
	    PR_fprintf(info->out, "Added certificate to database:\n");
	    dumpCertificate(oldCert, -1, info->out);
	}
	goto cleanup;
    }
    
    /*  Determine if cert is S/MIME and get its email if so.  */
    email = IsEmailCert(oldCert);

    /*
	XXX  Just create empty profiles?
    if (email) {
	SECItem *profile = CERT_FindSMimeProfile(oldCert);
	if (!profile &&
	    userSaysDeleteCert(&oldCert, 1, dbNoSMimeProfile, info, 0)) {
	    info->dbErrors[dbNoSMimeProfile]++;
	    if (info->verbose) {
		PR_fprintf(info->out, 
		           "Deleted cert missing S/MIME profile.\n");
		dumpCertificate(oldCert, -1, info->out);
	    }
	    goto cleanup;
	} else {
	    SECITEM_FreeItem(profile);
	}
    }
    */

createcert:

    /*  Sometimes happens... */
    if (!nickname && userCert)
	nickname = PORT_Strdup(oldCert->subjectName);

    /*  Create a new certificate, copy of the old one.  */
    newCert = CERT_NewTempCertificate(info->handle, &oldCert->derCert, 
                                      nickname, PR_FALSE, PR_TRUE);
    if (!newCert) {
	PR_fprintf(PR_STDERR, "Unable to create new certificate.\n");
	dumpCertificate(oldCert, -1, PR_STDERR);
	info->dbErrors[dbBadCertificate]++;
	goto cleanup;
    }

    /*  Add the cert to the new database.  */
    rv = CERT_AddTempCertToPerm(newCert, nickname, oldCert->trust);
    if (rv) {
	PR_fprintf(PR_STDERR, "Failed to write temp cert to perm database.\n");
	dumpCertificate(oldCert, -1, PR_STDERR);
	info->dbErrors[dbCertNotWrittenToDB]++;
	goto cleanup;
    }

    if (info->verbose) {
	PR_fprintf(info->out, "Added certificate to database:\n");
	dumpCertificate(oldCert, -1, info->out);
    }

    /*  If the cert is an S/MIME cert, and the first with it's subject,
     *  modify the subject entry to include the email address,
     *  CERT_AddTempCertToPerm does not do email addresses and S/MIME entries.
     */
    if (smimeEntry) { /*&& !userCert && nCertsForSubject == 1) { */
#if 0
	UpdateSubjectWithEmailAddr(newCert, email);
#endif
	SECItem emailProfile, profileTime;
	rv = CERT_FindFullSMimeProfile(oldCert, &emailProfile, &profileTime);
	/*  calls UpdateSubjectWithEmailAddr  */
	if (rv == SECSuccess)
	    rv = CERT_SaveSMimeProfile(newCert, &emailProfile, &profileTime);
    }

    info->nCerts++;

cleanup:

    if (nickname)
	PORT_Free(nickname);
    if (email)
	PORT_Free(email);
    if (oldCert)
	CERT_DestroyCertificate(oldCert);
    if (dbCert)
	CERT_DestroyCertificate(dbCert);
    if (newCert)
	CERT_DestroyCertificate(newCert);
    if (smimeEntry)
	SEC_DestroyDBEntry((certDBEntry*)smimeEntry);
    return SECSuccess;
}

#if 0
SECStatus
copyDBEntry(SECItem *data, SECItem *key, certDBEntryType type, void *pdata)
{
    SECStatus rv;
    CERTCertDBHandle *newdb = (CERTCertDBHandle *)pdata;
    certDBEntryCommon common;
    SECItem dbkey;

    common.type = type;
    common.version = CERT_DB_FILE_VERSION;
    common.flags = data->data[2];
    common.arena = NULL;

    dbkey.len = key->len + SEC_DB_KEY_HEADER_LEN;
    dbkey.data = (unsigned char *)PORT_Alloc(dbkey.len*sizeof(unsigned char));
    PORT_Memcpy(&dbkey.data[SEC_DB_KEY_HEADER_LEN], key->data, key->len);
    dbkey.data[0] = type;

    rv = WriteDBEntry(newdb, &common, &dbkey, data);

    PORT_Free(dbkey.data);
    return rv;
}
#endif

int
certIsOlder(CERTCertificate **cert1, CERTCertificate** cert2)
{
    return !CERT_IsNewer(*cert1, *cert2);
}

int
findNewestSubjectForEmail(CERTCertDBHandle *handle, int subjectNum,
                          certDBArray *dbArray, dbRestoreInfo *info,
                          int *subjectWithSMime, int *smimeForSubject)
{
    int newestSubject;
    int subjectsForEmail[50];
    int i, j, ns, sNum;
    certDBEntryListNode *subjects = &dbArray->subjects;
    certDBEntryListNode *smime = &dbArray->smime;
    certDBEntrySubject *subjectEntry1, *subjectEntry2;
    certDBEntrySMime *smimeEntry;
    CERTCertificate **certs;
    CERTCertificate *cert;
    CERTCertTrust *trust;
    PRBool userCert;
    int *certNums;

    ns = 0;
    subjectEntry1 = (certDBEntrySubject*)&subjects.entries[subjectNum];
    subjectsForEmail[ns++] = subjectNum;

    *subjectWithSMime = -1;
    *smimeForSubject = -1;
    newestSubject = subjectNum;

    cert = CERT_FindCertByKey(handle, &subjectEntry1->certKeys[0]);
    if (cert) {
	trust = cert->trust;
	userCert = (SEC_GET_TRUST_FLAGS(trust, trustSSL) & CERTDB_USER) ||
	          (SEC_GET_TRUST_FLAGS(trust, trustEmail) & CERTDB_USER) ||
	         (SEC_GET_TRUST_FLAGS(trust, trustObjectSigning) & CERTDB_USER);
	CERT_DestroyCertificate(cert);
    }

    /*
     * XXX Should we make sure that subjectEntry1->emailAddr is not
     * a null pointer or an empty string before going into the next
     * two for loops, which pass it to PORT_Strcmp?
     */

    /*  Loop over the remaining subjects.  */
    for (i=subjectNum+1; i<subjects.numEntries; i++) {
	subjectEntry2 = (certDBEntrySubject*)&subjects.entries[i];
	if (!subjectEntry2)
	    continue;
	if (subjectEntry2->emailAddr && subjectEntry2->emailAddr[0] &&
	     PORT_Strcmp(subjectEntry1->emailAddr, 
	                 subjectEntry2->emailAddr) == 0) {
	    /*  Found a subject using the same email address.  */
	    subjectsForEmail[ns++] = i;
	}
    }

    /*  Find the S/MIME entry for this email address.  */
    for (i=0; i<smime.numEntries; i++) {
	smimeEntry = (certDBEntrySMime*)&smime.entries[i];
	if (smimeEntry->common.arena == NULL)
	    continue;
	if (smimeEntry->emailAddr && smimeEntry->emailAddr[0] && 
	    PORT_Strcmp(subjectEntry1->emailAddr, smimeEntry->emailAddr) == 0) {
	    /*  Find which of the subjects uses this S/MIME entry.  */
	    for (j=0; j<ns && *subjectWithSMime < 0; j++) {
		sNum = subjectsForEmail[j];
		subjectEntry2 = (certDBEntrySubject*)&subjects.entries[sNum];
		if (SECITEM_ItemsAreEqual(&smimeEntry->subjectName,
		                          &subjectEntry2->derSubject)) {
		    /*  Found the subject corresponding to the S/MIME entry. */
		    *subjectWithSMime = sNum;
		    *smimeForSubject = i;
		}
	    }
	    SEC_DestroyDBEntry((certDBEntry*)smimeEntry);
	    PORT_Memset(smimeEntry, 0, sizeof(certDBEntry));
	    break;
	}
    }

    if (ns <= 1)
	return subjectNum;

    if (userCert)
	return *subjectWithSMime;

    /*  Now find which of the subjects has the newest cert.  */
    certs = (CERTCertificate**)PORT_Alloc(ns*sizeof(CERTCertificate*));
    certNums = (int*)PORT_Alloc((ns+1)*sizeof(int));
    certNums[0] = 0;
    for (i=0; i<ns; i++) {
	sNum = subjectsForEmail[i];
	subjectEntry1 = (certDBEntrySubject*)&subjects.entries[sNum];
	certs[i] = CERT_FindCertByKey(handle, &subjectEntry1->certKeys[0]);
	certNums[i+1] = i;
    }
    /*  Sort the array by validity.  */
    qsort(certs, ns, sizeof(CERTCertificate*), 
          (int (*)(const void *, const void *))certIsOlder);
    newestSubject = -1;
    for (i=0; i<ns; i++) {
	sNum = subjectsForEmail[i];
	subjectEntry1 = (certDBEntrySubject*)&subjects.entries[sNum];
	if (SECITEM_ItemsAreEqual(&subjectEntry1->derSubject,
	                          &certs[0]->derSubject))
	    newestSubject = sNum;
	else
	    SEC_DestroyDBEntry((certDBEntry*)subjectEntry1);
    }
    if (info && userSaysDeleteCert(certs, ns, dbOlderCert, info, certNums)) {
	for (i=1; i<ns+1; i++) {
	    if (certNums[i] >= 0 && certNums[i] != certNums[0]) {
		deleteAllEntriesForCert(handle, certs[certNums[i]], info->out);
		info->dbErrors[dbOlderCert]++;
	    }
	}
    }
    CERT_DestroyCertArray(certs, ns);
    return newestSubject;
}

CERTCertDBHandle *
DBCK_ReconstructDBFromCerts(CERTCertDBHandle *oldhandle, char *newdbname,
                            PRFileDesc *outfile, PRBool removeExpired,
                            PRBool requireProfile, PRBool singleEntry,
                            PRBool promptUser)
{
    SECStatus rv;
    dbRestoreInfo info;
    certDBEntryContentVersion *oldContentVersion;
    certDBArray dbArray;
    int i;

    PORT_Memset(&dbArray, 0, sizeof(dbArray));
    PORT_Memset(&info, 0, sizeof(info));
    info.verbose = (outfile) ? PR_TRUE : PR_FALSE;
    info.out = (outfile) ? outfile : PR_STDOUT;
    info.removeType[dbInvalidCert] = removeExpired;
    info.removeType[dbNoSMimeProfile] = requireProfile;
    info.removeType[dbOlderCert] = singleEntry;
    info.promptUser[dbInvalidCert]  = promptUser;
    info.promptUser[dbNoSMimeProfile]  = promptUser;
    info.promptUser[dbOlderCert]  = promptUser;

    /*  Allocate a handle to fill with CERT_OpenCertDB below.  */
    info.handle = (CERTCertDBHandle *)PORT_ZAlloc(sizeof(CERTCertDBHandle));
    if (!info.handle) {
	fprintf(stderr, "unable to get database handle");
	return NULL;
    }

    /*  Create a certdb with the most recent set of roots.  */
    rv = CERT_OpenCertDBFilename(info.handle, newdbname, PR_FALSE);

    if (rv) {
	fprintf(stderr, "could not open certificate database");
	goto loser;
    }

    /*  Create certificate, subject, nickname, and email records.
     *  mcom_db seems to have a sequential access bug.  Though reads and writes
     *  should be allowed during traversal, they seem to screw up the sequence.
     *  So, stuff all the cert entries into an array, and loop over the array
     *  doing read/writes in the db.
     */
    fillDBEntryArray(oldhandle, certDBEntryTypeCert, &dbArray.certs);
    for (elem = PR_LIST_HEAD(&dbArray->certs.link);
         elem != &dbArray->certs.link; elem = PR_NEXT_LINK(elem)) {
	node = LISTNODE_CAST(elem);
	addCertToDB((certDBEntryCert*)&node->entry, &info, oldhandle);
	/* entries get destroyed in addCertToDB */
    }
#if 0
    rv = SEC_TraverseDBEntries(oldhandle, certDBEntryTypeSMimeProfile, 
                               copyDBEntry, info.handle);
#endif

    /*  Fix up the pointers between (nickname|S/MIME) --> (subject).
     *  Create S/MIME entries for S/MIME certs.
     *  Have the S/MIME entry point to the last-expiring cert using
     *  an email address.
     */
#if 0
    CERT_RedoHandlesForSubjects(info.handle, singleEntry, &info);
#endif

    freeDBEntryList(&dbArray.certs.link);

    /*  Copy over the version record.  */
    /*  XXX Already exists - and _must_ be correct... */
    /*
    versionEntry = ReadDBVersionEntry(oldhandle);
    rv = WriteDBVersionEntry(info.handle, versionEntry);
    */

    /*  Copy over the content version record.  */
    /*  XXX Can probably get useful info from old content version?
     *      Was this db created before/after this tool?  etc.
     */
#if 0
    oldContentVersion = ReadDBContentVersionEntry(oldhandle);
    CERT_SetDBContentVersion(oldContentVersion->contentVersion, info.handle); 
#endif

#if 0
    /*  Copy over the CRL & KRL records.  */
    rv = SEC_TraverseDBEntries(oldhandle, certDBEntryTypeRevocation, 
                               copyDBEntry, info.handle);
    /*  XXX Only one KRL, just do db->get? */
    rv = SEC_TraverseDBEntries(oldhandle, certDBEntryTypeKeyRevocation, 
                               copyDBEntry, info.handle);
#endif

    PR_fprintf(info.out, "Database had %d certificates.\n", info.nOldCerts);

    PR_fprintf(info.out, "Reconstructed %d certificates.\n", info.nCerts);
    PR_fprintf(info.out, "(ax) Rejected %d expired certificates.\n", 
                       info.dbErrors[dbInvalidCert]);
    PR_fprintf(info.out, "(as) Rejected %d S/MIME certificates missing a profile.\n", 
                       info.dbErrors[dbNoSMimeProfile]);
    PR_fprintf(info.out, "(ar) Rejected %d certificates for which a newer certificate was found.\n", 
                       info.dbErrors[dbOlderCert]);
    PR_fprintf(info.out, "     Rejected %d corrupt certificates.\n", 
                       info.dbErrors[dbBadCertificate]);
    PR_fprintf(info.out, "     Rejected %d certificates which did not write to the DB.\n", 
                       info.dbErrors[dbCertNotWrittenToDB]);

    if (rv)
	goto loser;

    return info.handle;

loser:
    if (info.handle) 
	PORT_Free(info.handle);
    return NULL;
}
#endif /* DORECOVER */

enum {
    cmd_Debug = 0,
    cmd_LongUsage,
    cmd_Recover
};

enum {
    opt_KeepAll = 0,
    opt_CertDir,
    opt_Dumpfile,
    opt_InputDB,
    opt_OutputDB,
    opt_Mailfile,
    opt_Prompt,
    opt_KeepRedundant,
    opt_KeepNoSMimeProfile,
    opt_Verbose,
    opt_KeepExpired
};

static secuCommandFlag dbck_commands[] =
{
    { /* cmd_Debug,    */  'D', PR_FALSE, 0, PR_FALSE },
    { /* cmd_LongUsage,*/  'H', PR_FALSE, 0, PR_FALSE },
    { /* cmd_Recover,  */  'R', PR_FALSE, 0, PR_FALSE }
};

static secuCommandFlag dbck_options[] =
{
    { /* opt_KeepAll,           */  'a', PR_FALSE, 0, PR_FALSE },
    { /* opt_CertDir,           */  'd', PR_TRUE,  0, PR_FALSE },
    { /* opt_Dumpfile,          */  'f', PR_TRUE,  0, PR_FALSE },
    { /* opt_InputDB,           */  'i', PR_TRUE,  0, PR_FALSE },
    { /* opt_OutputDB,          */  'o', PR_TRUE,  0, PR_FALSE },
    { /* opt_Mailfile,          */  'm', PR_FALSE, 0, PR_FALSE },
    { /* opt_Prompt,            */  'p', PR_FALSE, 0, PR_FALSE },
    { /* opt_KeepRedundant,     */  'r', PR_FALSE, 0, PR_FALSE },
    { /* opt_KeepNoSMimeProfile,*/  's', PR_FALSE, 0, PR_FALSE },
    { /* opt_Verbose,           */  'v', PR_FALSE, 0, PR_FALSE },
    { /* opt_KeepExpired,       */  'x', PR_FALSE, 0, PR_FALSE }
};

int 
main(int argc, char **argv)
{
    CERTCertDBHandle *certHandle;

    PRFileInfo fileInfo;
    PRFileDesc *mailfile = NULL;
    PRFileDesc *dumpfile = NULL;

    char * pathname     = 0;
    char * fullname     = 0;
    char * newdbname    = 0;

    PRBool removeExpired, requireProfile, singleEntry;
    
    SECStatus rv;

    secuCommand dbck;
    dbck.numCommands = sizeof(dbck_commands) / sizeof(secuCommandFlag);
    dbck.numOptions = sizeof(dbck_options) / sizeof(secuCommandFlag);
    dbck.commands = dbck_commands;
    dbck.options = dbck_options;

    progName = strrchr(argv[0], '/');
    progName = progName ? progName+1 : argv[0];

    rv = SECU_ParseCommandLine(argc, argv, progName, &dbck);

    if (rv != SECSuccess)
	Usage(progName);

    if (dbck.commands[cmd_LongUsage].activated)
	LongUsage(progName);

    if (!dbck.commands[cmd_Debug].activated &&
        !dbck.commands[cmd_Recover].activated) {
	PR_fprintf(PR_STDERR, "Please specify -D or -R.\n");
	Usage(progName);
    }

    removeExpired = !(dbck.options[opt_KeepAll].activated ||
                      dbck.options[opt_KeepExpired].activated);

    requireProfile = !(dbck.options[opt_KeepAll].activated ||
                    dbck.options[opt_KeepNoSMimeProfile].activated);

    singleEntry = !(dbck.options[opt_KeepAll].activated ||
                    dbck.options[opt_KeepRedundant].activated);

    if (dbck.options[opt_OutputDB].activated) {
	newdbname = PL_strdup(dbck.options[opt_OutputDB].arg);
    } else {
	newdbname = PL_strdup("new_cert7.db");
    }

    /*  Create a generic graph of the database.  */
    if (dbck.options[opt_Mailfile].activated) {
	mailfile = PR_Open("./mailfile", PR_RDWR | PR_CREATE_FILE, 00660);
	if (!mailfile) {
	    fprintf(stderr, "Unable to create mailfile.\n");
	    return -1;
	}
    }

    /*  Dump all debugging info while running.  */
    if (dbck.options[opt_Verbose].activated) {
	if (dbck.options[opt_Dumpfile].activated) {
	    dumpfile = PR_Open(dbck.options[opt_Dumpfile].arg,
	                       PR_RDWR | PR_CREATE_FILE, 00660);
	}
	if (!dumpfile) {
	    fprintf(stderr, "Unable to create dumpfile.\n");
	    return -1;
	}
    }

    /*  Set the cert database directory.  */
    if (dbck.options[opt_CertDir].activated) {
	SECU_ConfigDirectory(dbck.options[opt_CertDir].arg);
    }

    PR_Init(PR_SYSTEM_THREAD, PR_PRIORITY_NORMAL, 1);
    SEC_Init();

    certHandle = (CERTCertDBHandle *)PORT_ZAlloc(sizeof(CERTCertDBHandle));
    if (!certHandle) {
	SECU_PrintError(progName, "unable to get database handle");
	return -1;
    }

    /*  Open the possibly corrupt database.  */
    if (dbck.options[opt_InputDB].activated) {
	pathname = SECU_ConfigDirectory(NULL);
	fullname = PR_smprintf("%s/%s", pathname, 
	                                dbck.options[opt_InputDB].arg);
	if (PR_GetFileInfo(fullname, &fileInfo) != PR_SUCCESS) {
	    fprintf(stderr, "Unable to read file \"%s\".\n", fullname);
	    return -1;
	}
	rv = CERT_OpenCertDBFilename(certHandle, fullname, PR_TRUE);
    } else {
	/*  Use the default.  */
	fullname = SECU_CertDBNameCallback(NULL, CERT_DB_FILE_VERSION);
	if (PR_GetFileInfo(fullname, &fileInfo) != PR_SUCCESS) {
	    fprintf(stderr, "Unable to read file \"%s\".\n", fullname);
	    return -1;
	}
	rv = CERT_OpenCertDB(certHandle, PR_TRUE, 
	                     SECU_CertDBNameCallback, NULL);
    }

    if (rv) {
	SECU_PrintError(progName, "unable to open cert database");
	return -1;
    }

    if (dbck.commands[cmd_Debug].activated) {
	DBCK_DebugDB(certHandle, dumpfile, mailfile);
	return 0;
    }

#ifdef DORECOVER
    if (dbck.commands[cmd_Recover].activated) {
	DBCK_ReconstructDBFromCerts(certHandle, newdbname,
	                            dumpfile, removeExpired, 
	                            requireProfile, singleEntry, 
	                            dbck.options[opt_Prompt].activated);
	return 0;
    }
#endif

    if (mailfile)
	PR_Close(mailfile);
    if (dumpfile)
	PR_Close(dumpfile);
    if (certHandle) {
	CERT_ClosePermCertDB(certHandle);
	PORT_Free(certHandle);
    }
    return -1;
}
