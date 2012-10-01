/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#ifndef _CRLGEN_H_
#define _CRLGEN_H_

#include "prio.h"
#include "prprf.h"
#include "plhash.h"
#include "seccomon.h"
#include "certt.h"
#include "secoidt.h"


#define CRLGEN_UNKNOWN_CONTEXT                   0
#define CRLGEN_ISSUER_CONTEXT                    1
#define CRLGEN_UPDATE_CONTEXT                    2
#define CRLGEN_NEXT_UPDATE_CONTEXT               3
#define CRLGEN_ADD_EXTENSION_CONTEXT             4
#define CRLGEN_ADD_CERT_CONTEXT                  6
#define CRLGEN_CHANGE_RANGE_CONTEXT              7
#define CRLGEN_RM_CERT_CONTEXT                   8

#define CRLGEN_TYPE_DATE                         0
#define CRLGEN_TYPE_ZDATE                        1
#define CRLGEN_TYPE_DIGIT                        2
#define CRLGEN_TYPE_DIGIT_RANGE                  3
#define CRLGEN_TYPE_OID                          4
#define CRLGEN_TYPE_STRING                       5
#define CRLGEN_TYPE_ID                           6


typedef struct CRLGENGeneratorDataStr          CRLGENGeneratorData;
typedef struct CRLGENEntryDataStr              CRLGENEntryData;
typedef struct CRLGENExtensionEntryStr         CRLGENExtensionEntry;
typedef struct CRLGENCertEntrySrt              CRLGENCertEntry;
typedef struct CRLGENCrlFieldStr               CRLGENCrlField;
typedef struct CRLGENEntriesSortedDataStr      CRLGENEntriesSortedData;

/* Exported functions */

/* Used for initialization of extension handles for crl and certs
 * extensions from existing CRL data then modifying existing CRL.*/
extern SECStatus CRLGEN_ExtHandleInit(CRLGENGeneratorData *crlGenData);

/* Commits all added entries and their's extensions into CRL. */
extern SECStatus CRLGEN_CommitExtensionsAndEntries(CRLGENGeneratorData *crlGenData);

/* Lunches the crl generation script parse */
extern SECStatus CRLGEN_StartCrlGen(CRLGENGeneratorData *crlGenData);

/* Closes crl generation script file and frees crlGenData */
extern void CRLGEN_FinalizeCrlGeneration(CRLGENGeneratorData *crlGenData);

/* Parser initialization function. Creates CRLGENGeneratorData structure
 *  for the current thread */
extern CRLGENGeneratorData* CRLGEN_InitCrlGeneration(CERTSignedCrl *newCrl,
                                                     PRFileDesc *src);


/* This lock is defined in crlgen_lex.c(derived from crlgen_lex.l).
 * It controls access to invocation of yylex, allows to parse one
 * script at a time */
extern void CRLGEN_InitCrlGenParserLock();
extern void CRLGEN_DestroyCrlGenParserLock();


/* The following function types are used to define functions for each of
 * CRLGENExtensionEntryStr, CRLGENCertEntrySrt, CRLGENCrlFieldStr to
 * provide functionality needed for these structures*/
typedef SECStatus updateCrlFn_t(CRLGENGeneratorData *crlGenData, void *str);
typedef SECStatus setNextDataFn_t(CRLGENGeneratorData *crlGenData, void *str,
                                  void *data, unsigned short dtype);
typedef SECStatus createNewLangStructFn_t(CRLGENGeneratorData *crlGenData,
                                          void *str, unsigned i);

/* Sets reports failure to parser if anything goes wrong */
extern void      crlgen_setFailure(CRLGENGeneratorData *str, char *);

/* Collects data in to one of the current data structure that corresponds
 * to the correct context type. This function gets called after each token
 * is found for a particular line */
extern SECStatus crlgen_setNextData(CRLGENGeneratorData *str, void *data,
                             unsigned short dtype);

/* initiates crl update with collected data. This function is called at the
 * end of each line */
extern SECStatus crlgen_updateCrl(CRLGENGeneratorData *str);

/* Creates new context structure depending on token that was parsed
 * at the beginning of a line */
extern SECStatus crlgen_createNewLangStruct(CRLGENGeneratorData *str,
                                            unsigned structType);


/* CRLGENExtensionEntry is used to store addext request data for either 
 * CRL extensions or CRL entry extensions. The differentiation between
 * is based on order and type of extension been added.
 *    - extData : all data in request staring from name of the extension are
 *                in saved here.
 *    - nextUpdatedData: counter of elements added to extData
 */
struct CRLGENExtensionEntryStr {
    char **extData;
    int    nextUpdatedData;
    updateCrlFn_t    *updateCrlFn;
    setNextDataFn_t  *setNextDataFn;
};

/* CRLGENCeryestEntry is used to store addcert request data
 *   - certId : certificate id or range of certificate with dash as a delimiter
 *              All certs from range will be inclusively added to crl
 *   - revocationTime: revocation time of cert(s)
 */
struct CRLGENCertEntrySrt {
    char *certId;
    char *revocationTime;
    updateCrlFn_t   *updateCrlFn;
    setNextDataFn_t *setNextDataFn;
};


/* CRLGENCrlField is used to store crl fields record like update time, next
 * update time, etc.
 *  - value: value of the parsed field data*/
struct CRLGENCrlFieldStr {
    char *value;
    updateCrlFn_t   *updateCrlFn;
    setNextDataFn_t *setNextDataFn;
};

/* Can not create entries extension until completely done with parsing.
 * Therefore need to keep joined data
 *   - certId : serial number of certificate
 *   - extHandle: head pointer to a list of extensions that belong to
 *                 entry
 *   - entry : CERTCrlEntry structure pointer*/
struct CRLGENEntryDataStr {
    SECItem *certId;
    void *extHandle;
    CERTCrlEntry *entry;
};

/* Crl generator/parser main structure. Keeps info regarding current state of
 * parser(context, status), parser helper functions pointers, parsed data and
 * generated data.
 *  - contextId : current parsing context. Context in this parser environment
 *                defines what type of crl operations parser is going through
 *                in the current line of crl generation script.
 *                setting or new cert or an extension addition, etc.
 *  - createNewLangStructFn: pointer to top level function which creates
 *                             data structures according contextId
 *  - setNextDataFn : pointer to top level function which sets new parsed data
 *                    in temporary structure
 *  - updateCrlFn   : pointer to top level function which triggers actual
 *                    crl update functions with gathered data
 *  - union         : data union create according to contextId
 *  - rangeFrom, rangeTo : holds last range in which certs was added
 *  - newCrl        : pointer to CERTSignedCrl newly created crl
 *  - crlExtHandle : pointer to crl extension handle
 *  - entryDataHashTable: hash of CRLGENEntryData.
 *                     key: cert serial number
 *                     data: CRLGENEntryData pointer
 *  - parserStatus  : current status of parser. Triggers parser to abort when
 *                    set to SECFailure
 *  - src : PRFileDesc structure pointer of crl generator config file
 *  - parsedLineNum : currently parsing line. Keeping it to report errors */ 
struct CRLGENGeneratorDataStr {
    unsigned short contextId;
    CRLGENCrlField       *crlField;
    CRLGENCertEntry      *certEntry;
    CRLGENExtensionEntry *extensionEntry;	
    PRUint64 rangeFrom;
    PRUint64 rangeTo;
    CERTSignedCrl *signCrl;
    void *crlExtHandle;
    PLHashTable *entryDataHashTable;
    
    PRFileDesc *src;
    int parsedLineNum;
};


#endif /* _CRLGEN_H_ */
