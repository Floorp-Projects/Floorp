/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
#include "pcert.h"
#include "nss.h"

static char *progName;

/* placeholders for pointer error types */
static void *WrongEntry;
static void *NoNickname;
static void *NoSMime;

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
    /* 9*/ NicknameAndSMimeEntries,
    NUM_ERROR_TYPES
} dbErrorType;

static char *dbErrorString[NUM_ERROR_TYPES] = {
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

static char *errResult[NUM_ERROR_TYPES] = {
    "Certificate entries that had no subject entry.",
    "Subject entries with no corresponding Certificate entries.",
    "Subject entries that had no nickname or S/MIME entries.",
    "Redundant nicknames (subjects with the same nickname).",
    "Subject entries that had no nickname entry.",
    "Redundant email addresses (subjects with the same email address).",
    "Subject entries that had no S/MIME entry.",
    "Nickname entries that had no subject entry.",
    "S/MIME entries that had no subject entry.",
    "Subject entries with BOTH nickname and S/MIME entries."
};

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
    int dbErrors[NUM_ERROR_TYPES];
} dbDebugInfo;

struct certDBEntryListNodeStr {
    PRCList link;
    certDBEntry entry;
    void *appData;
};
typedef struct certDBEntryListNodeStr certDBEntryListNode;

/*
 * A list node for a cert db entry.  The index is a unique identifier
 * to use for creating generic maps of a db.  This struct handles
 * the cert, nickname, and smime db entry types, as all three have a
 * single handle to a subject entry.
 * This structure is pointed to by certDBEntryListNode->appData.
 */
typedef struct
{
    PLArenaPool *arena;
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
    PLArenaPool *arena;
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
    int numRevocation;
    certDBEntryListNode certs;      /* pointer to head of cert list */
    certDBEntryListNode subjects;   /* pointer to head of subject list */
    certDBEntryListNode nicknames;  /* pointer to head of nickname list */
    certDBEntryListNode smime;      /* pointer to head of smime list */
    certDBEntryListNode revocation; /* pointer to head of revocation list */
} certDBArray;

/* Cast list to the base element, a certDBEntryListNode. */
#define LISTNODE_CAST(node) \
    ((certDBEntryListNode *)(node))

static void
Usage(char *progName)
{
#define FPS fprintf(stderr,
    FPS "Type %s -H for more detailed descriptions\n", progName);
    FPS "Usage:  %s -D [-d certdir] [-m] [-v [-f dumpfile]]\n",
    progName);
#ifdef DORECOVER
    FPS "        %s -R -o newdbname [-d certdir] [-aprsx] [-v [-f dumpfile]]\n",
    progName);
#endif
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
    FPS "%-15s Put database graph in ./mailfile (default is stdout).\n",
    "   -m");
    FPS "%-15s Verbose mode.  Dumps the entire contents of your cert8.db.\n",
    "   -v");
    FPS "%-15s File to dump verbose output into. (default is stdout)\n",
    "   -f dumpfile");
#ifdef DORECOVER
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
    FPS "%-15s File to store new database in (default is new_cert8.db)\n",
    "   -o newdbname");
    FPS "%-15s Cert database directory (default is ~/.netscape)\n",
    "   -d certdir");
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
#endif
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
    unsigned int i;
    for (i = 0; i < hexval->len; i++) {
        if (i != hexval->len - 1) {
            PR_fprintf(out, "%02x:", hexval->data[i]);
        } else {
            PR_fprintf(out, "%02x", hexval->data[i]);
        }
    }
    PR_fprintf(out, "\n");
}

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
    { /*  XXX should be separate function.  */
        PRTime timeBefore, timeAfter;
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
#if 0
    NSSLOWCERTCertificate *cert;
    /* should we check for existing duplicates? */
    cert = nsslowcert_DecodeDERCertificate(&entry->cert.derCert,
                        entry->cert.nickname);
#else
    CERTCertificate *cert;
    cert = CERT_DecodeDERCertificate(&entry->derCert, PR_FALSE, NULL);
#endif
    if (!cert) {
        fprintf(stderr, "Failed to decode certificate.\n");
        return SECFailure;
    }
    cert->trust = (CERTCertTrust *)&entry->trust;
    dumpCertificate(cert, num, outfile);
    CERT_DestroyCertificate(cert);
    return SECSuccess;
}

SECStatus
dumpSubjectEntry(certDBEntrySubject *entry, int num, PRFileDesc *outfile)
{
    char *subjectName = CERT_DerNameToAscii(&entry->derSubject);

    PR_fprintf(outfile, "Subject: %3d\n", num);
    PR_fprintf(outfile, "------------\n");
    PR_fprintf(outfile, "## %s\n", subjectName);
    if (entry->nickname)
        PR_fprintf(outfile, "## Subject nickname:  %s\n", entry->nickname);
    if (entry->emailAddrs) {
        unsigned int n;
        for (n = 0; n < entry->nemailAddrs && entry->emailAddrs[n]; ++n) {
            char *emailAddr = entry->emailAddrs[n];
            if (emailAddr[0]) {
                PR_fprintf(outfile, "## Subject email address:  %s\n",
                           emailAddr);
            }
        }
    }
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
#ifdef OLDWAY
    PR_fprintf(outfile, "##  OPTIONS:  ");
    printHexString(outfile, &entry->smimeOptions);
    PR_fprintf(outfile, "##  TIMESTAMP:  ");
    printHexString(outfile, &entry->optionsDate);
#else
    SECU_PrintAny(stdout, &entry->smimeOptions, "##  OPTIONS  ", 0);
    fflush(stdout);
    if (entry->optionsDate.len && entry->optionsDate.data)
        PR_fprintf(outfile, "##  TIMESTAMP: %.*s\n",
                   entry->optionsDate.len, entry->optionsDate.data);
#endif
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
    PLArenaPool *tmparena;
    SECItem derSubject;
    SECItem certKey;
    PRCList *cElem, *sElem;

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
                unsigned int i;
                /*  Found matching subject name, create link.  */
                map->pSubject = subjNode;
                /*  Make sure subject entry has cert's key.  */
                for (i = 0; i < subjectEntry->ncerts; i++) {
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
    certDBEntryListNode *subjNode;
    certDBSubjectEntryMap *subjMap;
    PRCList *sElem;

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
                                          subjectEntry->ncerts * sizeof(int));
        subjMap->numCerts = subjectEntry->ncerts;
        subjMap->pNickname = NoNickname;
        subjMap->pSMime = NoSMime;

        if (subjectEntry->nickname) {
            /* Subject should have a nickname entry, so create a link. */
            PRCList *nElem;
            for (nElem = PR_LIST_HEAD(&dbArray->nicknames.link);
                 nElem != &dbArray->nicknames.link;
                 nElem = PR_NEXT_LINK(nElem)) {
                certDBEntryListNode *nickNode;
                certDBEntryNickname *nicknameEntry;
                /*  Look for subject's nickname in nickname entries.  */
                nickNode = LISTNODE_CAST(nElem);
                nicknameEntry = (certDBEntryNickname *)&nickNode->entry;
                if (PL_strcmp(subjectEntry->nickname,
                              nicknameEntry->nickname) == 0) {
                    /*  Found a nickname entry for subject's nickname.  */
                    if (SECITEM_ItemsAreEqual(&subjectEntry->derSubject,
                                              &nicknameEntry->subjectName)) {
                        certDBEntryMap *nickMap;
                        nickMap = (certDBEntryMap *)nickNode->appData;
                        /*  Nickname and subject match.  */
                        subjMap->pNickname = nickNode;
                        nickMap->pSubject = subjNode;
                    } else if (subjMap->pNickname == NoNickname) {
                        /*  Nickname entry found is for diff. subject.  */
                        subjMap->pNickname = WrongEntry;
                    }
                }
            }
        }
        if (subjectEntry->emailAddrs) {
            unsigned int n;
            for (n = 0; n < subjectEntry->nemailAddrs &&
                        subjectEntry->emailAddrs[n];
                 ++n) {
                char *emailAddr = subjectEntry->emailAddrs[n];
                if (emailAddr[0]) {
                    PRCList *mElem;
                    /* Subject should have an smime entry, so create a link. */
                    for (mElem = PR_LIST_HEAD(&dbArray->smime.link);
                         mElem != &dbArray->smime.link;
                         mElem = PR_NEXT_LINK(mElem)) {
                        certDBEntryListNode *smimeNode;
                        certDBEntrySMime *smimeEntry;
                        /*  Look for subject's email in S/MIME entries.  */
                        smimeNode = LISTNODE_CAST(mElem);
                        smimeEntry = (certDBEntrySMime *)&smimeNode->entry;
                        if (PL_strcmp(emailAddr,
                                      smimeEntry->emailAddr) == 0) {
                            /*  Found a S/MIME entry for subject's email.  */
                            if (SECITEM_ItemsAreEqual(
                                    &subjectEntry->derSubject,
                                    &smimeEntry->subjectName)) {
                                certDBEntryMap *smimeMap;
                                /*  S/MIME entry and subject match.  */
                                subjMap->pSMime = smimeNode;
                                smimeMap = (certDBEntryMap *)smimeNode->appData;
                                smimeMap->pSubject = subjNode;
                            } else if (subjMap->pSMime == NoSMime) {
                                /*  S/MIME entry found is for diff. subject.  */
                                subjMap->pSMime = WrongEntry;
                            }
                        }
                    } /* end for */
                }     /* endif (emailAddr[0]) */
            }         /* end for */
        }             /* endif (subjectEntry->emailAddrs) */
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
            info->dbErrors[NoSubjectForSMime]++;
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
            info->dbErrors[NoSubjectForNickname]++;
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
        } else {
            info->dbErrors[SubjectHasNoKeyForCert]++;
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
            info->dbErrors[NoNicknameOrSMimeForSubject]++;
        }
        if (subjMap->pNickname && subjMap->pSMime) {
            info->dbErrors[NicknameAndSMimeEntries]++;
        }
    }
    if (direction != GORIGHT) { /* going right has only one cert */
        if (opttype == certDBEntryTypeNickname)
            printnode(info, "Nickname %5d   ", optindex);
        else if (opttype == certDBEntryTypeSMimeProfile)
            printnode(info, "S/MIME   %5d   ", optindex);
        for (i = 1 /* 1st one already done */; i < subjMap->numCerts; i++) {
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
    } else {
        info->dbErrors[NoSubjectForCert]++;
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
        if (node == NULL)
            break;
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
        dumpCertEntry((certDBEntryCert *)&node->entry, map->index, info->out);
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
        int refs = 0;
        node = LISTNODE_CAST(elem);
        subjectEntry = (certDBEntrySubject *)&node->entry;
        smap = (certDBSubjectEntryMap *)node->appData;
        dumpSubjectEntry(subjectEntry, smap->index, info->out);
        /* iterate over subject's certs */
        for (i = 0; i < smap->numCerts; i++) {
            /* walk each subject handle to it's cert entries */
            if (map_handle_is_ok(info, smap->pCerts[i], -1)) {
                ref = ((certDBEntryMap *)smap->pCerts[i]->appData)->index;
                PR_fprintf(info->out, "-->(%d. certificate %d)\n", i, ref);
            } else {
                PR_fprintf(info->out, "-->(%d. MISSING CERT ENTRY)\n", i);
            }
        }
        if (subjectEntry->nickname) {
            ++refs;
            /* walk each subject handle to it's nickname entry */
            if (map_handle_is_ok(info, smap->pNickname, -1)) {
                ref = ((certDBEntryMap *)smap->pNickname->appData)->index;
                PR_fprintf(info->out, "-->(nickname %d)\n", ref);
            } else {
                PR_fprintf(info->out, "-->(MISSING NICKNAME ENTRY)\n");
            }
        }
        if (subjectEntry->nemailAddrs &&
            subjectEntry->emailAddrs &&
            subjectEntry->emailAddrs[0] &&
            subjectEntry->emailAddrs[0][0]) {
            ++refs;
            /* walk each subject handle to it's smime entry */
            if (map_handle_is_ok(info, smap->pSMime, -1)) {
                ref = ((certDBEntryMap *)smap->pSMime->appData)->index;
                PR_fprintf(info->out, "-->(s/mime %d)\n", ref);
            } else {
                PR_fprintf(info->out, "-->(MISSING S/MIME ENTRY)\n");
            }
        }
        if (!refs) {
            PR_fprintf(info->out, "-->(NO NICKNAME+S/MIME ENTRY)\n");
        }
        PR_fprintf(info->out, "\n\n");
    }
    for (elem = PR_LIST_HEAD(&dbArray->nicknames.link);
         elem != &dbArray->nicknames.link; elem = PR_NEXT_LINK(elem)) {
        node = LISTNODE_CAST(elem);
        map = (certDBEntryMap *)node->appData;
        dumpNicknameEntry((certDBEntryNickname *)&node->entry, map->index,
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
        dumpSMimeEntry((certDBEntrySMime *)&node->entry, map->index, info->out);
        if (map_handle_is_ok(info, map->pSubject, -1)) {
            ref = ((certDBEntryMap *)map->pSubject->appData)->index;
            PR_fprintf(info->out, "-->(subject %d)\n\n\n", ref);
        } else {
            PR_fprintf(info->out, "-->(MISSING SUBJECT ENTRY)\n\n\n");
        }
    }
    PR_fprintf(info->out, "\n\n");
}

/* A callback function, intended to be called from nsslowcert_TraverseDBEntries
 * Builds a PRCList of DB entries of the specified type.
 */
SECStatus
SEC_GetCertDBEntryList(SECItem *dbdata, SECItem *dbkey,
                       certDBEntryType entryType, void *pdata)
{
    certDBEntry *entry;
    certDBEntryListNode *node;
    PRCList *list = (PRCList *)pdata;

    if (!dbdata || !dbkey || !pdata || !dbdata->data || !dbkey->data) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }
    entry = nsslowcert_DecodeAnyDBEntry(dbdata, dbkey, entryType, NULL);
    if (!entry) {
        return SECSuccess; /* skip it */
    }
    node = PORT_ArenaZNew(entry->common.arena, certDBEntryListNode);
    if (!node) {
        /* DestroyDBEntry(entry); */
        PLArenaPool *arena = entry->common.arena;
        PORT_Memset(&entry->common, 0, sizeof entry->common);
        PORT_FreeArena(arena, PR_FALSE);
        return SECFailure;
    }
    node->entry = *entry; /* crude but effective. */
    PR_INIT_CLIST(&node->link);
    PR_INSERT_BEFORE(&node->link, list);
    return SECSuccess;
}

int
fillDBEntryArray(NSSLOWCERTCertDBHandle *handle, certDBEntryType type,
                 certDBEntryListNode *list)
{
    PRCList *elem;
    certDBEntryListNode *node;
    certDBEntryMap *mnode;
    certDBSubjectEntryMap *smnode;
    PLArenaPool *arena;
    int count = 0;

    /* Initialize a dummy entry in the list.  The list head will be the
     * next element, so this element is skipped by for loops.
     */
    PR_INIT_CLIST((PRCList *)list);
    /* Collect all of the cert db entries for this type into a list. */
    nsslowcert_TraverseDBEntries(handle, type, SEC_GetCertDBEntryList, list);

    for (elem = PR_LIST_HEAD(&list->link);
         elem != &list->link; elem = PR_NEXT_LINK(elem)) {
        /* Iterate over the entries and ... */
        node = (certDBEntryListNode *)elem;
        if (type != certDBEntryTypeSubject) {
            arena = PORT_NewArena(sizeof(*mnode));
            mnode = PORT_ArenaZNew(arena, certDBEntryMap);
            mnode->arena = arena;
            /* ... assign a unique index number to each node, and ... */
            mnode->index = count;
            /* ... set the map pointer for the node. */
            node->appData = (void *)mnode;
        } else {
            /* allocate some room for the cert pointers also */
            arena = PORT_NewArena(sizeof(*smnode) + 20 * sizeof(void *));
            smnode = PORT_ArenaZNew(arena, certDBSubjectEntryMap);
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
DBCK_DebugDB(NSSLOWCERTCertDBHandle *handle, PRFileDesc *out,
             PRFileDesc *mailfile)
{
    int i, nCertsFound, nSubjFound, nErr;
    int nCerts, nSubjects, nSubjCerts, nNicknames, nSMime, nRevocation;
    PRCList *elem;
    char c;
    dbDebugInfo info;
    certDBArray dbArray;

    PORT_Memset(&dbArray, 0, sizeof(dbArray));
    PORT_Memset(&info, 0, sizeof(info));
    info.verbose = (PRBool)(out != NULL);
    info.dograph = info.verbose;
    info.out = (out) ? out : PR_STDOUT;
    info.graphfile = mailfile ? mailfile : PR_STDOUT;

    /*  Fill the array structure with cert/subject/nickname/smime entries.  */
    dbArray.numCerts = fillDBEntryArray(handle, certDBEntryTypeCert,
                                        &dbArray.certs);
    dbArray.numSubjects = fillDBEntryArray(handle, certDBEntryTypeSubject,
                                           &dbArray.subjects);
    dbArray.numNicknames = fillDBEntryArray(handle, certDBEntryTypeNickname,
                                            &dbArray.nicknames);
    dbArray.numSMime = fillDBEntryArray(handle, certDBEntryTypeSMimeProfile,
                                        &dbArray.smime);
    dbArray.numRevocation = fillDBEntryArray(handle, certDBEntryTypeRevocation,
                                             &dbArray.revocation);

    /*  Compute the map between the database entries.  */
    mapSubjectEntries(&dbArray);
    mapCertEntries(&dbArray);
    computeDBGraph(&dbArray, &info);

    /*  Store the totals for later reference.  */
    nCerts = dbArray.numCerts;
    nSubjects = dbArray.numSubjects;
    nNicknames = dbArray.numNicknames;
    nSMime = dbArray.numSMime;
    nRevocation = dbArray.numRevocation;
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
    freeDBEntryList(&dbArray.revocation.link);

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
    PR_fprintf(info.out, "N5: Found %4d CRL entries.\n",
               nRevocation);
    PR_fprintf(info.out, "\n");

    nErr = 0;
    for (i = 0; i < NUM_ERROR_TYPES; i++) {
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
    PR_fprintf(info.out,
               "N1 == N3 + N4 + E%d + E%d + E%d + E%d + E%d - E%d - E%d - E%d\n",
               NoNicknameOrSMimeForSubject,
               WrongNicknameForSubject,
               NoNicknameEntry,
               WrongSMimeForSubject,
               NoSMimeEntry,
               NoSubjectForNickname,
               NoSubjectForSMime,
               NicknameAndSMimeEntries);
    nSubjFound = nNicknames + nSMime +
                 info.dbErrors[NoNicknameOrSMimeForSubject] +
                 info.dbErrors[WrongNicknameForSubject] +
                 info.dbErrors[NoNicknameEntry] +
                 info.dbErrors[WrongSMimeForSubject] +
                 info.dbErrors[NoSMimeEntry] -
                 info.dbErrors[NoSubjectForNickname] -
                 info.dbErrors[NoSubjectForSMime] -
                 info.dbErrors[NicknameAndSMimeEntries];
    c = (nSubjFound == nSubjects) ? '=' : '!';
    PR_fprintf(info.out,
               "%2d %c= %2d + %2d + %2d + %2d + %2d + %2d + %2d - %2d - %2d - %2d\n",
               nSubjects, c, nNicknames, nSMime,
               info.dbErrors[NoNicknameOrSMimeForSubject],
               info.dbErrors[WrongNicknameForSubject],
               info.dbErrors[NoNicknameEntry],
               info.dbErrors[WrongSMimeForSubject],
               info.dbErrors[NoSMimeEntry],
               info.dbErrors[NoSubjectForNickname],
               info.dbErrors[NoSubjectForSMime],
               info.dbErrors[NicknameAndSMimeEntries]);
    PR_fprintf(info.out, "\n");
}

#ifdef DORECOVER
#include "dbrecover.c"
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

static secuCommandFlag dbck_commands[] = {
    { /* cmd_Debug,    */ 'D', PR_FALSE, 0, PR_FALSE },
    { /* cmd_LongUsage,*/ 'H', PR_FALSE, 0, PR_FALSE },
    { /* cmd_Recover,  */ 'R', PR_FALSE, 0, PR_FALSE }
};

static secuCommandFlag dbck_options[] = {
    { /* opt_KeepAll,           */ 'a', PR_FALSE, 0, PR_FALSE },
    { /* opt_CertDir,           */ 'd', PR_TRUE, 0, PR_FALSE },
    { /* opt_Dumpfile,          */ 'f', PR_TRUE, 0, PR_FALSE },
    { /* opt_InputDB,           */ 'i', PR_TRUE, 0, PR_FALSE },
    { /* opt_OutputDB,          */ 'o', PR_TRUE, 0, PR_FALSE },
    { /* opt_Mailfile,          */ 'm', PR_FALSE, 0, PR_FALSE },
    { /* opt_Prompt,            */ 'p', PR_FALSE, 0, PR_FALSE },
    { /* opt_KeepRedundant,     */ 'r', PR_FALSE, 0, PR_FALSE },
    { /* opt_KeepNoSMimeProfile,*/ 's', PR_FALSE, 0, PR_FALSE },
    { /* opt_Verbose,           */ 'v', PR_FALSE, 0, PR_FALSE },
    { /* opt_KeepExpired,       */ 'x', PR_FALSE, 0, PR_FALSE }
};

#define CERT_DB_FMT "%s/cert%s.db"

static char *
dbck_certdb_name_cb(void *arg, int dbVersion)
{
    const char *configdir = (const char *)arg;
    const char *dbver;
    char *smpname = NULL;
    char *dbname = NULL;

    switch (dbVersion) {
        case 8:
            dbver = "8";
            break;
        case 7:
            dbver = "7";
            break;
        case 6:
            dbver = "6";
            break;
        case 5:
            dbver = "5";
            break;
        case 4:
        default:
            dbver = "";
            break;
    }

    /* make sure we return something allocated with PORT_ so we have properly
     * matched frees at the end */
    smpname = PR_smprintf(CERT_DB_FMT, configdir, dbver);
    if (smpname) {
        dbname = PORT_Strdup(smpname);
        PR_smprintf_free(smpname);
    }
    return dbname;
}

int
main(int argc, char **argv)
{
    NSSLOWCERTCertDBHandle *certHandle;

    PRFileDesc *mailfile = NULL;
    PRFileDesc *dumpfile = NULL;

    char *pathname = 0;
    char *fullname = 0;
    char *newdbname = 0;

    PRBool removeExpired, requireProfile, singleEntry;
    SECStatus rv;
    secuCommand dbck;

    dbck.numCommands = sizeof(dbck_commands) / sizeof(secuCommandFlag);
    dbck.numOptions = sizeof(dbck_options) / sizeof(secuCommandFlag);
    dbck.commands = dbck_commands;
    dbck.options = dbck_options;

    progName = strrchr(argv[0], '/');
    progName = progName ? progName + 1 : argv[0];

    rv = SECU_ParseCommandLine(argc, argv, progName, &dbck);

    if (rv != SECSuccess)
        Usage(progName);

    if (dbck.commands[cmd_LongUsage].activated)
        LongUsage(progName);

    if (!dbck.commands[cmd_Debug].activated &&
        !dbck.commands[cmd_Recover].activated) {
        PR_fprintf(PR_STDERR, "Please specify -H, -D or -R.\n");
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
        newdbname = PL_strdup("new_cert8.db");
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
            if (!dumpfile) {
                fprintf(stderr, "Unable to create dumpfile.\n");
                return -1;
            }
        } else {
            dumpfile = PR_STDOUT;
        }
    }

    /*  Set the cert database directory.  */
    if (dbck.options[opt_CertDir].activated) {
        SECU_ConfigDirectory(dbck.options[opt_CertDir].arg);
    }

    pathname = SECU_ConfigDirectory(NULL);

    PR_Init(PR_SYSTEM_THREAD, PR_PRIORITY_NORMAL, 1);
    rv = NSS_NoDB_Init(pathname);
    if (rv != SECSuccess) {
        fprintf(stderr, "NSS_NoDB_Init failed\n");
        return -1;
    }

    certHandle = PORT_ZNew(NSSLOWCERTCertDBHandle);
    if (!certHandle) {
        SECU_PrintError(progName, "unable to get database handle");
        return -1;
    }
    certHandle->ref = 1;

#ifdef NOTYET
    /*  Open the possibly corrupt database.  */
    if (dbck.options[opt_InputDB].activated) {
        PRFileInfo fileInfo;
        fullname = PR_smprintf("%s/%s", pathname,
                               dbck.options[opt_InputDB].arg);
        if (PR_GetFileInfo(fullname, &fileInfo) != PR_SUCCESS) {
            fprintf(stderr, "Unable to read file \"%s\".\n", fullname);
            return -1;
        }
        rv = CERT_OpenCertDBFilename(certHandle, fullname, PR_TRUE);
    } else
#endif
    {
/*  Use the default.  */
#ifdef NOTYET
        fullname = SECU_CertDBNameCallback(NULL, CERT_DB_FILE_VERSION);
        if (PR_GetFileInfo(fullname, &fileInfo) != PR_SUCCESS) {
            fprintf(stderr, "Unable to read file \"%s\".\n", fullname);
            return -1;
        }
#endif
        rv = nsslowcert_OpenCertDB(certHandle,
                                   PR_TRUE,             /* readOnly */
                                   NULL,                /* rdb appName */
                                   "",                  /* rdb prefix */
                                   dbck_certdb_name_cb, /* namecb */
                                   pathname,            /* configDir */
                                   PR_FALSE);           /* volatile */
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
        nsslowcert_ClosePermCertDB(certHandle);
        PORT_Free(certHandle);
    }
    return -1;
}
