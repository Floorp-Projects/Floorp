/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

%{

#include "crlgen.h"

static SECStatus parserStatus = SECSuccess;
static CRLGENGeneratorData *parserData;
static PRFileDesc *src;

#define YY_INPUT(buf,result,max_size) \
    if ( parserStatus != SECFailure) { \
	 if (((result = PR_Read(src, buf, max_size)) == 0) && \
	     ferror( yyin )) \
	   return SECFailure; \
    } else  { return SECFailure; }
              

%}

%a 5000
DIGIT          [0-9]+
DIGIT_RANGE    [0-9]+-[0-9]+
ID             [a-zA-Z][a-zA-Z0-9]*
OID            [0-9]+\.[\.0-9]+
DATE           [0-9]{4}[01][0-9][0-3][0-9][0-2][0-9][0-6][0-9][0-6][0-9]
ZDATE          [0-9]{4}[01][0-9][0-3][0-9][0-2][0-9][0-6][0-9][0-6][0-9]Z
N_SP_STRING    [a-zA-Z0-9\:\|\.]+

%%

{ZDATE}   {
parserStatus = crlgen_setNextData(parserData, yytext, CRLGEN_TYPE_ZDATE);
if (parserStatus != SECSuccess)
    return parserStatus;
}

{DIGIT}   {
parserStatus = crlgen_setNextData(parserData, yytext, CRLGEN_TYPE_DIGIT);
if (parserStatus != SECSuccess)
    return parserStatus;
}

{DIGIT_RANGE}  {
parserStatus = crlgen_setNextData(parserData, yytext, CRLGEN_TYPE_DIGIT_RANGE);
if (parserStatus != SECSuccess)
    return parserStatus;
}

{OID}     {
parserStatus = crlgen_setNextData(parserData, yytext, CRLGEN_TYPE_OID);
if (parserStatus != SECSuccess)
    return parserStatus;
}

issuer     {
parserStatus = crlgen_createNewLangStruct(parserData, CRLGEN_ISSUER_CONTEXT);
if (parserStatus != SECSuccess)
    return parserStatus;
}

update     {
parserStatus = crlgen_createNewLangStruct(parserData, CRLGEN_UPDATE_CONTEXT);
if (parserStatus != SECSuccess)
    return parserStatus;
}
nextupdate {
parserStatus = crlgen_createNewLangStruct(parserData, CRLGEN_NEXT_UPDATE_CONTEXT);
if (parserStatus != SECSuccess)
    return parserStatus;
}

range      {
parserStatus = crlgen_createNewLangStruct(parserData, CRLGEN_CHANGE_RANGE_CONTEXT);
if (parserStatus != SECSuccess)
    return parserStatus;
}

{ID}      {
if (strcmp(yytext, "addcert") == 0) {
    parserStatus = crlgen_createNewLangStruct(parserData,
                                    CRLGEN_ADD_CERT_CONTEXT);
    if (parserStatus != SECSuccess)
        return parserStatus;
} else if (strcmp(yytext, "rmcert") == 0) {
    parserStatus = crlgen_createNewLangStruct(parserData,
                                    CRLGEN_RM_CERT_CONTEXT);
    if (parserStatus != SECSuccess)
        return parserStatus;
} else if (strcmp(yytext, "addext") == 0) {
    parserStatus = crlgen_createNewLangStruct(parserData,
                                    CRLGEN_ADD_EXTENSION_CONTEXT);
    if (parserStatus != SECSuccess)
        return parserStatus;
} else {
    parserStatus = crlgen_setNextData(parserData, yytext, CRLGEN_TYPE_ID);
    if (parserStatus != SECSuccess)
        return parserStatus;
}
}

"="

\"[^\"]* {
if (yytext[yyleng-1] == '\\') {
    yymore();
} else {
    register int c;
    c = input();
    if (c != '\"') {
        printf( "Error: Line ending \" is missing:  %c\n", c);
        unput(c);
    } else {
        parserStatus = crlgen_setNextData(parserData, yytext + 1,
                                          CRLGEN_TYPE_STRING);
        if (parserStatus != SECSuccess)
            return parserStatus;
    }
}
}

{N_SP_STRING} {
parserStatus = crlgen_setNextData(parserData, yytext, CRLGEN_TYPE_STRING);
if (parserStatus != SECSuccess)
    return parserStatus;
}



^#[^\n]*     /* eat up one-line comments */ {}

[ \t]+      {}

(\n|\r\n)  {
parserStatus = crlgen_updateCrl(parserData);
if (parserStatus != SECSuccess)
    return parserStatus;
}

.           {
    fprintf(stderr, "Syntax error at line %d: unknown token %s\n",
            parserData->parsedLineNum, yytext);
    return SECFailure;
}

%%
#include "prlock.h"

static PRLock *parserInvocationLock;

void CRLGEN_InitCrlGenParserLock()
{
    parserInvocationLock = PR_NewLock();
}

void CRLGEN_DestroyCrlGenParserLock()
{
    PR_DestroyLock(parserInvocationLock);
}


SECStatus CRLGEN_StartCrlGen(CRLGENGeneratorData *parserCtlData)
{
    SECStatus rv;

    PR_Lock(parserInvocationLock);

    parserStatus = SECSuccess;
    parserData = parserCtlData;
    src = parserCtlData->src;

    rv = yylex();

    PR_Unlock(parserInvocationLock);

    return rv;
}

int yywrap() {return 1;}
