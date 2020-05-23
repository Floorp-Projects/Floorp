/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
** secutil.c - various functions used by security stuff
**
*/

#include "prtypes.h"
#include "prtime.h"
#include "prlong.h"
#include "prerror.h"
#include "prprf.h"
#include "plgetopt.h"
#include "prenv.h"
#include "prnetdb.h"

#include "basicutil.h"
#include <stdarg.h>
#include <stddef.h>
#include <sys/stat.h>
#include <errno.h>

#ifdef XP_UNIX
#include <unistd.h>
#endif

#include "secoid.h"

extern long DER_GetInteger(const SECItem *src);

static PRBool wrapEnabled = PR_TRUE;

void
SECU_EnableWrap(PRBool enable)
{
    wrapEnabled = enable;
}

PRBool
SECU_GetWrapEnabled(void)
{
    return wrapEnabled;
}

void
SECU_PrintErrMsg(FILE *out, int level, const char *progName, const char *msg,
                 ...)
{
    va_list args;
    PRErrorCode err = PORT_GetError();
    const char *errString = PORT_ErrorToString(err);

    va_start(args, msg);

    SECU_Indent(out, level);
    fprintf(out, "%s: ", progName);
    vfprintf(out, msg, args);
    if (errString != NULL && PORT_Strlen(errString) > 0)
        fprintf(out, ": %s\n", errString);
    else
        fprintf(out, ": error %d\n", (int)err);

    va_end(args);
}

void
SECU_PrintError(const char *progName, const char *msg, ...)
{
    va_list args;
    PRErrorCode err = PORT_GetError();
    const char *errName = PR_ErrorToName(err);
    const char *errString = PR_ErrorToString(err, 0);

    va_start(args, msg);

    fprintf(stderr, "%s: ", progName);
    vfprintf(stderr, msg, args);

    if (errName != NULL) {
        fprintf(stderr, ": %s", errName);
    } else {
        fprintf(stderr, ": error %d", (int)err);
    }

    if (errString != NULL && PORT_Strlen(errString) > 0)
        fprintf(stderr, ": %s\n", errString);

    va_end(args);
}

void
SECU_PrintSystemError(const char *progName, const char *msg, ...)
{
    va_list args;

    va_start(args, msg);
    fprintf(stderr, "%s: ", progName);
    vfprintf(stderr, msg, args);
    fprintf(stderr, ": %s\n", strerror(errno));
    va_end(args);
}

SECStatus
secu_StdinToItem(SECItem *dst)
{
    unsigned char buf[1000];
    PRInt32 numBytes;
    PRBool notDone = PR_TRUE;

    dst->len = 0;
    dst->data = NULL;

    while (notDone) {
        numBytes = PR_Read(PR_STDIN, buf, sizeof(buf));

        if (numBytes < 0) {
            return SECFailure;
        }

        if (numBytes == 0)
            break;

        if (dst->data) {
            unsigned char *p = dst->data;
            dst->data = (unsigned char *)PORT_Realloc(p, dst->len + numBytes);
            if (!dst->data) {
                PORT_Free(p);
            }
        } else {
            dst->data = (unsigned char *)PORT_Alloc(numBytes);
        }
        if (!dst->data) {
            return SECFailure;
        }
        PORT_Memcpy(dst->data + dst->len, buf, numBytes);
        dst->len += numBytes;
    }

    return SECSuccess;
}

SECStatus
SECU_FileToItem(SECItem *dst, PRFileDesc *src)
{
    PRFileInfo info;
    PRInt32 numBytes;
    PRStatus prStatus;

    if (src == PR_STDIN)
        return secu_StdinToItem(dst);

    prStatus = PR_GetOpenFileInfo(src, &info);

    if (prStatus != PR_SUCCESS) {
        PORT_SetError(SEC_ERROR_IO);
        return SECFailure;
    }

    /* XXX workaround for 3.1, not all utils zero dst before sending */
    dst->data = 0;
    if (!SECITEM_AllocItem(NULL, dst, info.size))
        goto loser;

    numBytes = PR_Read(src, dst->data, info.size);
    if (numBytes != info.size) {
        PORT_SetError(SEC_ERROR_IO);
        goto loser;
    }

    return SECSuccess;
loser:
    SECITEM_FreeItem(dst, PR_FALSE);
    dst->data = NULL;
    return SECFailure;
}

SECStatus
SECU_TextFileToItem(SECItem *dst, PRFileDesc *src)
{
    PRFileInfo info;
    PRInt32 numBytes;
    PRStatus prStatus;
    unsigned char *buf;

    if (src == PR_STDIN)
        return secu_StdinToItem(dst);

    prStatus = PR_GetOpenFileInfo(src, &info);

    if (prStatus != PR_SUCCESS) {
        PORT_SetError(SEC_ERROR_IO);
        return SECFailure;
    }

    buf = (unsigned char *)PORT_Alloc(info.size);
    if (!buf)
        return SECFailure;

    numBytes = PR_Read(src, buf, info.size);
    if (numBytes != info.size) {
        PORT_SetError(SEC_ERROR_IO);
        goto loser;
    }

    if (buf[numBytes - 1] == '\n')
        numBytes--;
#ifdef _WINDOWS
    if (buf[numBytes - 1] == '\r')
        numBytes--;
#endif

    /* XXX workaround for 3.1, not all utils zero dst before sending */
    dst->data = 0;
    if (!SECITEM_AllocItem(NULL, dst, numBytes))
        goto loser;

    memcpy(dst->data, buf, numBytes);

    PORT_Free(buf);
    return SECSuccess;
loser:
    PORT_Free(buf);
    return SECFailure;
}

#define INDENT_MULT 4
void
SECU_Indent(FILE *out, int level)
{
    int i;

    for (i = 0; i < level; i++) {
        fprintf(out, "    ");
    }
}

void
SECU_Newline(FILE *out)
{
    fprintf(out, "\n");
}

void
SECU_PrintAsHex(FILE *out, const SECItem *data, const char *m, int level)
{
    unsigned i;
    int column = 0;
    PRBool isString = PR_TRUE;
    PRBool isWhiteSpace = PR_TRUE;
    PRBool printedHex = PR_FALSE;
    unsigned int limit = 15;

    if (m) {
        SECU_Indent(out, level);
        fprintf(out, "%s:", m);
        level++;
        if (wrapEnabled)
            fprintf(out, "\n");
    }

    if (wrapEnabled) {
        SECU_Indent(out, level);
        column = level * INDENT_MULT;
    }
    if (!data->len) {
        fprintf(out, "(empty)\n");
        return;
    }
    /* take a pass to see if it's all printable. */
    for (i = 0; i < data->len; i++) {
        unsigned char val = data->data[i];
        if (!val || !isprint(val)) {
            isString = PR_FALSE;
            break;
        }
        if (isWhiteSpace && !isspace(val)) {
            isWhiteSpace = PR_FALSE;
        }
    }

    /* Short values, such as bit strings (which are printed with this
    ** function) often look like strings, but we want to see the bits.
    ** so this test assures that short values will be printed in hex,
    ** perhaps in addition to being printed as strings.
    ** The threshold size (4 bytes) is arbitrary.
    */
    if (!isString || data->len <= 4) {
        for (i = 0; i < data->len; i++) {
            if (i != data->len - 1) {
                fprintf(out, "%02x:", data->data[i]);
                column += 3;
            } else {
                fprintf(out, "%02x", data->data[i]);
                column += 2;
                break;
            }
            if (wrapEnabled &&
                (column > 76 || (i % 16 == limit))) {
                SECU_Newline(out);
                SECU_Indent(out, level);
                column = level * INDENT_MULT;
                limit = i % 16;
            }
        }
        printedHex = PR_TRUE;
    }
    if (isString && !isWhiteSpace) {
        if (printedHex != PR_FALSE) {
            SECU_Newline(out);
            SECU_Indent(out, level);
            column = level * INDENT_MULT;
        }
        for (i = 0; i < data->len; i++) {
            unsigned char val = data->data[i];

            if (val) {
                fprintf(out, "%c", val);
                column++;
            } else {
                column = 77;
            }
            if (wrapEnabled && column > 76) {
                SECU_Newline(out);
                SECU_Indent(out, level);
                column = level * INDENT_MULT;
            }
        }
    }

    if (column != level * INDENT_MULT) {
        SECU_Newline(out);
    }
}

const char *hex = "0123456789abcdef";

const char printable[257] = {
    "................"  /* 0x */
    "................"  /* 1x */
    " !\"#$%&'()*+,-./" /* 2x */
    "0123456789:;<=>?"  /* 3x */
    "@ABCDEFGHIJKLMNO"  /* 4x */
    "PQRSTUVWXYZ[\\]^_" /* 5x */
    "`abcdefghijklmno"  /* 6x */
    "pqrstuvwxyz{|}~."  /* 7x */
    "................"  /* 8x */
    "................"  /* 9x */
    "................"  /* ax */
    "................"  /* bx */
    "................"  /* cx */
    "................"  /* dx */
    "................"  /* ex */
    "................"  /* fx */
};

void
SECU_PrintBuf(FILE *out, const char *msg, const void *vp, int len)
{
    const unsigned char *cp = (const unsigned char *)vp;
    char buf[80];
    char *bp;
    char *ap;

    fprintf(out, "%s [Len: %d]\n", msg, len);
    memset(buf, ' ', sizeof buf);
    bp = buf;
    ap = buf + 50;
    while (--len >= 0) {
        unsigned char ch = *cp++;
        *bp++ = hex[(ch >> 4) & 0xf];
        *bp++ = hex[ch & 0xf];
        *bp++ = ' ';
        *ap++ = printable[ch];
        if (ap - buf >= 66) {
            *ap = 0;
            fprintf(out, "   %s\n", buf);
            memset(buf, ' ', sizeof buf);
            bp = buf;
            ap = buf + 50;
        }
    }
    if (bp > buf) {
        *ap = 0;
        fprintf(out, "   %s\n", buf);
    }
}

/* This expents i->data[0] to be the MSB of the integer.
** if you want to print a DER-encoded integer (with the tag and length)
** call SECU_PrintEncodedInteger();
*/
void
SECU_PrintInteger(FILE *out, const SECItem *i, const char *m, int level)
{
    int iv;

    if (!i || !i->len || !i->data) {
        SECU_Indent(out, level);
        if (m) {
            fprintf(out, "%s: (null)\n", m);
        } else {
            fprintf(out, "(null)\n");
        }
    } else if (i->len > 4) {
        SECU_PrintAsHex(out, i, m, level);
    } else {
        if (i->type == siUnsignedInteger && *i->data & 0x80) {
            /* Make sure i->data has zero in the highest bite
             * if i->data is an unsigned integer */
            SECItem tmpI;
            char data[] = { 0, 0, 0, 0, 0 };

            PORT_Memcpy(data + 1, i->data, i->len);
            tmpI.len = i->len + 1;
            tmpI.data = (void *)data;

            iv = DER_GetInteger(&tmpI);
        } else {
            iv = DER_GetInteger(i);
        }
        SECU_Indent(out, level);
        if (m) {
            fprintf(out, "%s: %d (0x%x)\n", m, iv, iv);
        } else {
            fprintf(out, "%d (0x%x)\n", iv, iv);
        }
    }
}

#if defined(DEBUG) || defined(FORCE_PR_ASSERT)
/* Returns true iff a[i].flag has a duplicate in a[i+1 : count-1]  */
static PRBool
HasShortDuplicate(int i, secuCommandFlag *a, int count)
{
    char target = a[i].flag;
    int j;

    /* duplicate '\0' flags are okay, they are used with long forms */
    for (j = i + 1; j < count; j++) {
        if (a[j].flag && a[j].flag == target) {
            return PR_TRUE;
        }
    }
    return PR_FALSE;
}

/* Returns true iff a[i].longform has a duplicate in a[i+1 : count-1] */
static PRBool
HasLongDuplicate(int i, secuCommandFlag *a, int count)
{
    int j;
    char *target = a[i].longform;

    if (!target)
        return PR_FALSE;

    for (j = i + 1; j < count; j++) {
        if (a[j].longform && strcmp(a[j].longform, target) == 0) {
            return PR_TRUE;
        }
    }
    return PR_FALSE;
}

/* Returns true iff a has no short or long form duplicates
 */
PRBool
HasNoDuplicates(secuCommandFlag *a, int count)
{
    int i;

    for (i = 0; i < count; i++) {
        if (a[i].flag && HasShortDuplicate(i, a, count)) {
            return PR_FALSE;
        }
        if (a[i].longform && HasLongDuplicate(i, a, count)) {
            return PR_FALSE;
        }
    }
    return PR_TRUE;
}
#endif

SECStatus
SECU_ParseCommandLine(int argc, char **argv, char *progName,
                      const secuCommand *cmd)
{
    PRBool found;
    PLOptState *optstate;
    PLOptStatus status;
    char *optstring;
    PLLongOpt *longopts = NULL;
    int i, j;
    int lcmd = 0, lopt = 0;

    PR_ASSERT(HasNoDuplicates(cmd->commands, cmd->numCommands));
    PR_ASSERT(HasNoDuplicates(cmd->options, cmd->numOptions));

    optstring = (char *)PORT_Alloc(cmd->numCommands + 2 * cmd->numOptions + 1);
    if (optstring == NULL)
        return SECFailure;

    j = 0;
    for (i = 0; i < cmd->numCommands; i++) {
        if (cmd->commands[i].flag) /* single character option ? */
            optstring[j++] = cmd->commands[i].flag;
        if (cmd->commands[i].longform)
            lcmd++;
    }
    for (i = 0; i < cmd->numOptions; i++) {
        if (cmd->options[i].flag) {
            optstring[j++] = cmd->options[i].flag;
            if (cmd->options[i].needsArg)
                optstring[j++] = ':';
        }
        if (cmd->options[i].longform)
            lopt++;
    }

    optstring[j] = '\0';

    if (lcmd + lopt > 0) {
        longopts = PORT_NewArray(PLLongOpt, lcmd + lopt + 1);
        if (!longopts) {
            PORT_Free(optstring);
            return SECFailure;
        }

        j = 0;
        for (i = 0; j < lcmd && i < cmd->numCommands; i++) {
            if (cmd->commands[i].longform) {
                longopts[j].longOptName = cmd->commands[i].longform;
                longopts[j].longOption = 0;
                longopts[j++].valueRequired = cmd->commands[i].needsArg;
            }
        }
        lopt += lcmd;
        for (i = 0; j < lopt && i < cmd->numOptions; i++) {
            if (cmd->options[i].longform) {
                longopts[j].longOptName = cmd->options[i].longform;
                longopts[j].longOption = 0;
                longopts[j++].valueRequired = cmd->options[i].needsArg;
            }
        }
        longopts[j].longOptName = NULL;
    }

    optstate = PL_CreateLongOptState(argc, argv, optstring, longopts);
    if (!optstate) {
        PORT_Free(optstring);
        PORT_Free(longopts);
        return SECFailure;
    }
    /* Parse command line arguments */
    while ((status = PL_GetNextOpt(optstate)) == PL_OPT_OK) {
        const char *optstatelong;
        char option = optstate->option;

        /*  positional parameter, single-char option or long opt? */
        if (optstate->longOptIndex == -1) {
            /* not a long opt */
            if (option == '\0')
                continue; /* it's a positional parameter */
            optstatelong = "";
        } else {
            /* long opt */
            if (option == '\0')
                option = '\377'; /* force unequal with all flags */
            optstatelong = longopts[optstate->longOptIndex].longOptName;
        }

        found = PR_FALSE;

        for (i = 0; i < cmd->numCommands; i++) {
            if (cmd->commands[i].flag == option ||
                cmd->commands[i].longform == optstatelong) {
                cmd->commands[i].activated = PR_TRUE;
                if (optstate->value) {
                    cmd->commands[i].arg = (char *)optstate->value;
                }
                found = PR_TRUE;
                break;
            }
        }

        if (found)
            continue;

        for (i = 0; i < cmd->numOptions; i++) {
            if (cmd->options[i].flag == option ||
                cmd->options[i].longform == optstatelong) {
                cmd->options[i].activated = PR_TRUE;
                if (optstate->value) {
                    cmd->options[i].arg = (char *)optstate->value;
                } else if (cmd->options[i].needsArg) {
                    status = PL_OPT_BAD;
                    goto loser;
                }
                found = PR_TRUE;
                break;
            }
        }

        if (!found) {
            status = PL_OPT_BAD;
            break;
        }
    }

loser:
    PL_DestroyOptState(optstate);
    PORT_Free(optstring);
    if (longopts)
        PORT_Free(longopts);
    if (status == PL_OPT_BAD)
        return SECFailure;
    return SECSuccess;
}

char *
SECU_GetOptionArg(const secuCommand *cmd, int optionNum)
{
    if (optionNum < 0 || optionNum >= cmd->numOptions)
        return NULL;
    if (cmd->options[optionNum].activated)
        return PL_strdup(cmd->options[optionNum].arg);
    else
        return NULL;
}

void
SECU_PrintPRandOSError(const char *progName)
{
    char buffer[513];
    PRInt32 errLenInt = PR_GetErrorTextLength();
    size_t errLen = errLenInt < 0 ? 0 : (size_t)errLenInt;
    if (errLen > 0 && errLen < sizeof buffer) {
        PR_GetErrorText(buffer);
    }
    SECU_PrintError(progName, "function failed");
    if (errLen > 0 && errLen < sizeof buffer) {
        PR_fprintf(PR_STDERR, "\t%s\n", buffer);
    }
}

SECOidTag
SECU_StringToSignatureAlgTag(const char *alg)
{
    SECOidTag hashAlgTag = SEC_OID_UNKNOWN;

    if (alg) {
        if (!PL_strcmp(alg, "MD2")) {
            hashAlgTag = SEC_OID_MD2;
        } else if (!PL_strcmp(alg, "MD4")) {
            hashAlgTag = SEC_OID_MD4;
        } else if (!PL_strcmp(alg, "MD5")) {
            hashAlgTag = SEC_OID_MD5;
        } else if (!PL_strcmp(alg, "SHA1")) {
            hashAlgTag = SEC_OID_SHA1;
        } else if (!PL_strcmp(alg, "SHA224")) {
            hashAlgTag = SEC_OID_SHA224;
        } else if (!PL_strcmp(alg, "SHA256")) {
            hashAlgTag = SEC_OID_SHA256;
        } else if (!PL_strcmp(alg, "SHA384")) {
            hashAlgTag = SEC_OID_SHA384;
        } else if (!PL_strcmp(alg, "SHA512")) {
            hashAlgTag = SEC_OID_SHA512;
        }
    }
    return hashAlgTag;
}

/* Caller ensures that dst is at least item->len*2+1 bytes long */
void
SECU_SECItemToHex(const SECItem *item, char *dst)
{
    if (dst && item && item->data) {
        unsigned char *src = item->data;
        unsigned int len = item->len;
        for (; len > 0; --len, dst += 2) {
            sprintf(dst, "%02x", *src++);
        }
        *dst = '\0';
    }
}

static unsigned char
nibble(char c)
{
    c = PORT_Tolower(c);
    return (c >= '0' && c <= '9') ? c - '0' : (c >= 'a' && c <= 'f') ? c - 'a' + 10 : -1;
}

SECStatus
SECU_SECItemHexStringToBinary(SECItem *srcdest)
{
    unsigned int i;

    if (!srcdest) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }
    if (srcdest->len < 4 || (srcdest->len % 2)) {
        /* too short to convert, or even number of characters */
        PORT_SetError(SEC_ERROR_BAD_DATA);
        return SECFailure;
    }
    if (PORT_Strncasecmp((const char *)srcdest->data, "0x", 2)) {
        /* wrong prefix */
        PORT_SetError(SEC_ERROR_BAD_DATA);
        return SECFailure;
    }

    /* 1st pass to check for hex characters */
    for (i = 2; i < srcdest->len; i++) {
        char c = PORT_Tolower(srcdest->data[i]);
        if (!((c >= '0' && c <= '9') ||
              (c >= 'a' && c <= 'f'))) {
            PORT_SetError(SEC_ERROR_BAD_DATA);
            return SECFailure;
        }
    }

    /* 2nd pass to convert */
    for (i = 2; i < srcdest->len; i += 2) {
        srcdest->data[(i - 2) / 2] = (nibble(srcdest->data[i]) << 4) +
                                     nibble(srcdest->data[i + 1]);
    }

    /* adjust length */
    srcdest->len -= 2;
    srcdest->len /= 2;
    return SECSuccess;
}

SECItem *
SECU_HexString2SECItem(PLArenaPool *arena, SECItem *item, const char *str)
{
    int i = 0;
    int byteval = 0;
    int tmp = PORT_Strlen(str);

    PORT_Assert(arena);
    PORT_Assert(item);

    if ((tmp % 2) != 0) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return NULL;
    }

    item = SECITEM_AllocItem(arena, item, tmp / 2);
    if (item == NULL) {
        return NULL;
    }

    while (str[i]) {
        if ((str[i] >= '0') && (str[i] <= '9')) {
            tmp = str[i] - '0';
        } else if ((str[i] >= 'a') && (str[i] <= 'f')) {
            tmp = str[i] - 'a' + 10;
        } else if ((str[i] >= 'A') && (str[i] <= 'F')) {
            tmp = str[i] - 'A' + 10;
        } else {
            /* item is in arena and gets freed by the caller */
            return NULL;
        }

        byteval = byteval * 16 + tmp;
        if ((i % 2) != 0) {
            item->data[i / 2] = byteval;
            byteval = 0;
        }
        i++;
    }

    return item;
}

/* mapping between ECCurveName enum and SECOidTags */
static SECOidTag ecCurve_oid_map[] = {
    SEC_OID_UNKNOWN,                /* ECCurve_noName */
    SEC_OID_ANSIX962_EC_PRIME192V1, /* ECCurve_NIST_P192 */
    SEC_OID_SECG_EC_SECP224R1,      /* ECCurve_NIST_P224 */
    SEC_OID_ANSIX962_EC_PRIME256V1, /* ECCurve_NIST_P256 */
    SEC_OID_SECG_EC_SECP384R1,      /* ECCurve_NIST_P384 */
    SEC_OID_SECG_EC_SECP521R1,      /* ECCurve_NIST_P521 */
    SEC_OID_SECG_EC_SECT163K1,      /* ECCurve_NIST_K163 */
    SEC_OID_SECG_EC_SECT163R1,      /* ECCurve_NIST_B163 */
    SEC_OID_SECG_EC_SECT233K1,      /* ECCurve_NIST_K233 */
    SEC_OID_SECG_EC_SECT233R1,      /* ECCurve_NIST_B233 */
    SEC_OID_SECG_EC_SECT283K1,      /* ECCurve_NIST_K283 */
    SEC_OID_SECG_EC_SECT283R1,      /* ECCurve_NIST_B283 */
    SEC_OID_SECG_EC_SECT409K1,      /* ECCurve_NIST_K409 */
    SEC_OID_SECG_EC_SECT409R1,      /* ECCurve_NIST_B409 */
    SEC_OID_SECG_EC_SECT571K1,      /* ECCurve_NIST_K571 */
    SEC_OID_SECG_EC_SECT571R1,      /* ECCurve_NIST_B571 */
    SEC_OID_ANSIX962_EC_PRIME192V2,
    SEC_OID_ANSIX962_EC_PRIME192V3,
    SEC_OID_ANSIX962_EC_PRIME239V1,
    SEC_OID_ANSIX962_EC_PRIME239V2,
    SEC_OID_ANSIX962_EC_PRIME239V3,
    SEC_OID_ANSIX962_EC_C2PNB163V1,
    SEC_OID_ANSIX962_EC_C2PNB163V2,
    SEC_OID_ANSIX962_EC_C2PNB163V3,
    SEC_OID_ANSIX962_EC_C2PNB176V1,
    SEC_OID_ANSIX962_EC_C2TNB191V1,
    SEC_OID_ANSIX962_EC_C2TNB191V2,
    SEC_OID_ANSIX962_EC_C2TNB191V3,
    SEC_OID_ANSIX962_EC_C2PNB208W1,
    SEC_OID_ANSIX962_EC_C2TNB239V1,
    SEC_OID_ANSIX962_EC_C2TNB239V2,
    SEC_OID_ANSIX962_EC_C2TNB239V3,
    SEC_OID_ANSIX962_EC_C2PNB272W1,
    SEC_OID_ANSIX962_EC_C2PNB304W1,
    SEC_OID_ANSIX962_EC_C2TNB359V1,
    SEC_OID_ANSIX962_EC_C2PNB368W1,
    SEC_OID_ANSIX962_EC_C2TNB431R1,
    SEC_OID_SECG_EC_SECP112R1,
    SEC_OID_SECG_EC_SECP112R2,
    SEC_OID_SECG_EC_SECP128R1,
    SEC_OID_SECG_EC_SECP128R2,
    SEC_OID_SECG_EC_SECP160K1,
    SEC_OID_SECG_EC_SECP160R1,
    SEC_OID_SECG_EC_SECP160R2,
    SEC_OID_SECG_EC_SECP192K1,
    SEC_OID_SECG_EC_SECP224K1,
    SEC_OID_SECG_EC_SECP256K1,
    SEC_OID_SECG_EC_SECT113R1,
    SEC_OID_SECG_EC_SECT113R2,
    SEC_OID_SECG_EC_SECT131R1,
    SEC_OID_SECG_EC_SECT131R2,
    SEC_OID_SECG_EC_SECT163R1,
    SEC_OID_SECG_EC_SECT193R1,
    SEC_OID_SECG_EC_SECT193R2,
    SEC_OID_SECG_EC_SECT239K1,
    SEC_OID_UNKNOWN, /* ECCurve_WTLS_1 */
    SEC_OID_UNKNOWN, /* ECCurve_WTLS_8 */
    SEC_OID_UNKNOWN, /* ECCurve_WTLS_9 */
    SEC_OID_CURVE25519,
    SEC_OID_UNKNOWN /* ECCurve_pastLastCurve */
};

SECStatus
SECU_ecName2params(ECCurveName curve, SECItem *params)
{
    SECOidData *oidData = NULL;

    if ((curve < ECCurve_noName) || (curve > ECCurve_pastLastCurve) ||
        ((oidData = SECOID_FindOIDByTag(ecCurve_oid_map[curve])) == NULL)) {
        PORT_SetError(SEC_ERROR_UNSUPPORTED_ELLIPTIC_CURVE);
        return SECFailure;
    }

    if (SECITEM_AllocItem(NULL, params, (2 + oidData->oid.len)) == NULL) {
        return SECFailure;
    }
    /*
     * params->data needs to contain the ASN encoding of an object ID (OID)
     * representing the named curve. The actual OID is in
     * oidData->oid.data so we simply prepend 0x06 and OID length
     */
    params->data[0] = SEC_ASN1_OBJECT_ID;
    params->data[1] = oidData->oid.len;
    memcpy(params->data + 2, oidData->oid.data, oidData->oid.len);

    return SECSuccess;
}
