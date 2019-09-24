/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
** symkeyutil.c
**
** utility for managing symetric keys in the database or the token
**
*/

/*
 * Wish List for this utility:
 *  1) Display and Set the CKA_ operation flags for the key.
 *  2) Modify existing keys
 *  3) Copy keys
 *  4) Read CKA_ID and display for keys.
 *  5) Option to store CKA_ID in a file on key creation.
 *  6) Encrypt, Decrypt, Hash, and Mac with generated keys.
 *  7) Use asymetric keys to wrap and unwrap keys.
 *  8) Derive.
 *  9) PBE keys.
 */

#include <stdio.h>
#include <string.h>

#include "secutil.h"

#include "nspr.h"

#include "pk11func.h"
#include "secasn1.h"
#include "cert.h"
#include "cryptohi.h"
#include "secoid.h"
#include "certdb.h"
#include "nss.h"

typedef struct _KeyTypes {
    CK_KEY_TYPE keyType;
    CK_MECHANISM_TYPE mechType;
    CK_MECHANISM_TYPE wrapMech;
    char *label;
} KeyTypes;

static KeyTypes keyArray[] = {
#ifdef RECOGNIZE_ASYMETRIC_TYPES
    { CKK_RSA, CKM_RSA_PKCS, CKM_RSA_PKCS, "rsa" },
    { CKK_DSA, CKM_DSA, CKM_INVALID_MECHANISM, "dsa" },
    { CKK_DH, CKM_DH_PKCS_DERIVE, CKM_INVALID_MECHANISM, "dh" },
    { CKK_EC, CKM_ECDSA, CKM_INVALID_MECHANISM, "ec" },
    { CKK_X9_42_DH, CKM_X9_42_DH_DERIVE, CKM_INVALID_MECHANISM, "x9.42dh" },
    { CKK_KEA, CKM_KEA_KEY_DERIVE, CKM_INVALID_MECHANISM, "kea" },
#endif
    { CKK_GENERIC_SECRET, CKM_SHA_1_HMAC, CKM_INVALID_MECHANISM, "generic" },
    { CKK_RC2, CKM_RC2_CBC, CKM_RC2_ECB, "rc2" },
    /* don't define a wrap mech for RC-4 since it's note really safe */
    { CKK_RC4, CKM_RC4, CKM_INVALID_MECHANISM, "rc4" },
    { CKK_DES, CKM_DES_CBC, CKM_DES_ECB, "des" },
    { CKK_DES2, CKM_DES2_KEY_GEN, CKM_DES3_ECB, "des2" },
    { CKK_DES3, CKM_DES3_KEY_GEN, CKM_DES3_ECB, "des3" },
    { CKK_CAST, CKM_CAST_CBC, CKM_CAST_ECB, "cast" },
    { CKK_CAST3, CKM_CAST3_CBC, CKM_CAST3_ECB, "cast3" },
    { CKK_CAST5, CKM_CAST5_CBC, CKM_CAST5_ECB, "cast5" },
    { CKK_CAST128, CKM_CAST128_CBC, CKM_CAST128_ECB, "cast128" },
    { CKK_RC5, CKM_RC5_CBC, CKM_RC5_ECB, "rc5" },
    { CKK_IDEA, CKM_IDEA_CBC, CKM_IDEA_ECB, "idea" },
    { CKK_SKIPJACK, CKM_SKIPJACK_CBC64, CKM_SKIPJACK_WRAP, "skipjack" },
    { CKK_BATON, CKM_BATON_CBC128, CKM_BATON_WRAP, "baton" },
    { CKK_JUNIPER, CKM_JUNIPER_CBC128, CKM_JUNIPER_WRAP, "juniper" },
    { CKK_CDMF, CKM_CDMF_CBC, CKM_CDMF_ECB, "cdmf" },
    { CKK_AES, CKM_AES_CBC, CKM_AES_ECB, "aes" },
    { CKK_CAMELLIA, CKM_CAMELLIA_CBC, CKM_CAMELLIA_ECB, "camellia" },
};

static int keyArraySize = sizeof(keyArray) / sizeof(keyArray[0]);

int
GetLen(PRFileDesc *fd)
{
    PRFileInfo info;

    if (PR_SUCCESS != PR_GetOpenFileInfo(fd, &info)) {
        return -1;
    }

    return info.size;
}

int
ReadBuf(char *inFile, SECItem *item)
{
    int len;
    int ret;
    PRFileDesc *fd = PR_Open(inFile, PR_RDONLY, 0);
    if (NULL == fd) {
        SECU_PrintError("symkeyutil", "PR_Open failed");
        return -1;
    }

    len = GetLen(fd);
    if (len < 0) {
        SECU_PrintError("symkeyutil", "PR_GetOpenFileInfo failed");
        return -1;
    }
    item->data = (unsigned char *)PORT_Alloc(len);
    if (item->data == NULL) {
        fprintf(stderr, "Failed to allocate %d to read file %s\n", len, inFile);
        return -1;
    }

    ret = PR_Read(fd, item->data, item->len);
    if (ret < 0) {
        SECU_PrintError("symkeyutil", "PR_Read failed");
        PORT_Free(item->data);
        item->data = NULL;
        return -1;
    }
    PR_Close(fd);
    item->len = len;
    return 0;
}

int
WriteBuf(char *inFile, SECItem *item)
{
    int ret;
    PRFileDesc *fd = PR_Open(inFile, PR_WRONLY | PR_CREATE_FILE, 0x200);
    if (NULL == fd) {
        SECU_PrintError("symkeyutil", "PR_Open failed");
        return -1;
    }

    ret = PR_Write(fd, item->data, item->len);
    if (ret < 0) {
        SECU_PrintError("symkeyutil", "PR_Write failed");
        return -1;
    }
    PR_Close(fd);
    return 0;
}

CK_KEY_TYPE
GetKeyTypeFromString(const char *keyString)
{
    int i;
    for (i = 0; i < keyArraySize; i++) {
        if (PL_strcasecmp(keyString, keyArray[i].label) == 0) {
            return keyArray[i].keyType;
        }
    }
    return (CK_KEY_TYPE)-1;
}

CK_MECHANISM_TYPE
GetKeyMechFromString(const char *keyString)
{
    int i;
    for (i = 0; i < keyArraySize; i++) {
        if (PL_strcasecmp(keyString, keyArray[i].label) == 0) {
            return keyArray[i].mechType;
        }
    }
    return (CK_MECHANISM_TYPE)-1;
}

const char *
GetStringFromKeyType(CK_KEY_TYPE type)
{
    int i;
    for (i = 0; i < keyArraySize; i++) {
        if (keyArray[i].keyType == type) {
            return keyArray[i].label;
        }
    }
    return "unmatched";
}

CK_MECHANISM_TYPE
GetWrapFromKeyType(CK_KEY_TYPE type)
{
    int i;
    for (i = 0; i < keyArraySize; i++) {
        if (keyArray[i].keyType == type) {
            return keyArray[i].wrapMech;
        }
    }
    return CKM_INVALID_MECHANISM;
}

CK_MECHANISM_TYPE
GetWrapMechanism(PK11SymKey *symKey)
{
    CK_KEY_TYPE type = PK11_GetSymKeyType(symKey);

    return GetWrapFromKeyType(type);
}

int
GetDigit(char c)
{
    if (c == 0) {
        return -1;
    }
    if (c <= '9' && c >= '0') {
        return c - '0';
    }
    if (c <= 'f' && c >= 'a') {
        return c - 'a' + 0xa;
    }
    if (c <= 'F' && c >= 'A') {
        return c - 'A' + 0xa;
    }
    return -1;
}

char
ToDigit(unsigned char c)
{
    c = c & 0xf;
    if (c <= 9) {
        return (char)(c + '0');
    }
    return (char)(c + 'a' - 0xa);
}

char *
BufToHex(SECItem *outbuf)
{
    int len = outbuf->len * 2 + 1;
    char *string, *ptr;
    unsigned int i;

    string = PORT_Alloc(len);
    if (!string) {
        return NULL;
    }

    ptr = string;
    for (i = 0; i < outbuf->len; i++) {
        *ptr++ = ToDigit(outbuf->data[i] >> 4);
        *ptr++ = ToDigit(outbuf->data[i] & 0xf);
    }
    *ptr = 0;
    return string;
}

int
HexToBuf(char *inString, SECItem *outbuf)
{
    int len = strlen(inString);
    int outlen = len + 1 / 2;
    int trueLen = 0;

    outbuf->data = PORT_Alloc(outlen);
    if (!outbuf->data) {
        return -1;
    }

    while (*inString) {
        int digit1, digit2;
        digit1 = GetDigit(*inString++);
        digit2 = GetDigit(*inString++);
        if ((digit1 == -1) || (digit2 == -1)) {
            PORT_Free(outbuf->data);
            outbuf->data = NULL;
            return -1;
        }
        outbuf->data[trueLen++] = digit1 << 4 | digit2;
    }
    outbuf->len = trueLen;
    return 0;
}

void
printBuf(unsigned char *data, int len)
{
    int i;

    for (i = 0; i < len; i++) {
        printf("%02x", data[i]);
    }
}

void
PrintKey(PK11SymKey *symKey)
{
    char *name = PK11_GetSymKeyNickname(symKey);
    int len = PK11_GetKeyLength(symKey);
    int strength = PK11_GetKeyStrength(symKey, NULL);
    SECItem *value = NULL;
    CK_KEY_TYPE type = PK11_GetSymKeyType(symKey);
    (void)PK11_ExtractKeyValue(symKey);

    value = PK11_GetKeyData(symKey);

    printf("%-20s %3d   %4d   %10s  ", name ? name : " ", len, strength,
           GetStringFromKeyType(type));
    if (value && value->data) {
        printBuf(value->data, value->len);
    } else {
        printf("<restricted>");
    }
    printf("\n");
}

SECStatus
ListKeys(PK11SlotInfo *slot, int *printLabel, void *pwd)
{
    PK11SymKey *keyList;
    SECStatus rv = PK11_Authenticate(slot, PR_FALSE, pwd);
    if (rv != SECSuccess) {
        return rv;
        ;
    }

    keyList = PK11_ListFixedKeysInSlot(slot, NULL, pwd);
    if (keyList) {
        if (*printLabel) {
            printf("     Name            Len Strength     Type    Data\n");
            *printLabel = 0;
        }
        printf("%s:\n", PK11_GetTokenName(slot));
    }
    while (keyList) {
        PK11SymKey *freeKey = keyList;
        PrintKey(keyList);
        keyList = PK11_GetNextSymKey(keyList);
        PK11_FreeSymKey(freeKey);
    }
    return SECSuccess;
}

PK11SymKey *
FindKey(PK11SlotInfo *slot, char *name, SECItem *id, void *pwd)
{
    PK11SymKey *key = NULL;
    SECStatus rv = PK11_Authenticate(slot, PR_FALSE, pwd);

    if (rv != SECSuccess) {
        return NULL;
    }

    if (id->data) {
        key = PK11_FindFixedKey(slot, CKM_INVALID_MECHANISM, id, pwd);
    }
    if (name && !key) {
        key = PK11_ListFixedKeysInSlot(slot, name, pwd);
    }

    if (key) {
        printf("Found a key\n");
        PrintKey(key);
    }
    return key;
}

PRBool
IsKeyList(PK11SymKey *symKey)
{
    return (PRBool)(PK11_GetNextSymKey(symKey) != NULL);
}

void
FreeKeyList(PK11SymKey *symKey)
{
    PK11SymKey *next, *current;

    for (current = symKey; current; current = next) {
        next = PK11_GetNextSymKey(current);
        PK11_FreeSymKey(current);
    }
    return;
}

static void
Usage(char *progName)
{
#define FPS fprintf(stderr,
    FPS "Type %s -H for more detailed descriptions\n", progName);
    FPS "Usage:");
    FPS "\t%s -L [std_opts] [-r]\n", progName);
    FPS "\t%s -K [-n name] -t type [-s size] [-i id |-j id_file] [std_opts]\n", progName);
    FPS "\t%s -D <[-n name | -i id | -j id_file> [std_opts]\n", progName);
    FPS "\t%s -I [-n name] [-t type] [-i id | -j id_file] -k data_file [std_opts]\n", progName);
    FPS "\t%s -E  <-nname | -i id | -j id_file> [-t type] -k data_file [-r] [std_opts]\n", progName);
    FPS "\t%s -U [-n name] [-t type] [-i id | -j id_file] -k data_file <wrap_opts> [std_opts]\n", progName);
    FPS "\t%s -W <-n name | -i id | -j id_file> [-t type] -k data_file [-r] <wrap_opts> [std_opts]\n", progName);
    FPS "\t%s -M <-n name | -i id | -j id_file> -g target_token [std_opts]\n", progName);
    FPS "\t\t std_opts -> [-d certdir] [-P dbprefix] [-p password] [-f passwordFile] [-h token]\n");
    FPS "\t\t wrap_opts -> <-w wrap_name | -x wrap_id | -y id_file>\n");
    exit(1);
}

static void
LongUsage(char *progName)
{
    int i;
    FPS "%-15s List all the keys.\n", "-L");
    FPS "%-15s Generate a new key.\n", "-K");
    FPS "%-20s Specify the nickname of the new key\n",
    "   -n name");
    FPS "%-20s Specify the id in hex of the new key\n",
    "   -i key id");
    FPS "%-20s Specify a file to read the id of the new key\n",
    "   -j key id file");
    FPS "%-20s Specify the keyType of the new key\n",
    "   -t type");
    FPS "%-20s", "  valid types: ");
    for (i = 0; i < keyArraySize; i++) {
        FPS "%s%c", keyArray[i].label, i == keyArraySize-1? '\n':',');
    }
    FPS "%-20s Specify the size of the new key in bytes (required by some types)\n",
    "   -s size");
    FPS "%-15s Delete a key.\n", "-D");
    FPS "%-20s Specify the nickname of the key to delete\n",
    "   -n name");
    FPS "%-20s Specify the id in hex of the key to delete\n",
    "   -i key id");
    FPS "%-20s Specify a file to read the id of the key to delete\n",
    "   -j key id file");
    FPS "%-15s Import a new key from a data file.\n", "-I");
    FPS "%-20s Specify the data file to read the key from.\n",
    "   -k key file");
    FPS "%-20s Specify the nickname of the new key\n",
    "   -n name");
    FPS "%-20s Specify the id in hex of the new key\n",
    "   -i key id");
    FPS "%-20s Specify a file to read the id of the new key\n",
    "   -j key id file");
    FPS "%-20s Specify the keyType of the new key\n",
    "   -t type");
    FPS "%-20s", "  valid types: ");
    for (i = 0; i < keyArraySize; i++) {
        FPS "%s%c", keyArray[i].label, i == keyArraySize-1? '\n':',');
    }
    FPS "%-15s Export a key to a data file.\n", "-E");
    FPS "%-20s Specify the data file to write the key to.\n",
    "   -k key file");
    FPS "%-20s Specify the nickname of the key to export\n",
    "   -n name");
    FPS "%-20s Specify the id in hex of the key to export\n",
    "   -i key id");
    FPS "%-20s Specify a file to read the id of the key to export\n",
    "   -j key id file");
    FPS "%-15s Move a key to a new token.\n", "-M");
    FPS "%-20s Specify the nickname of the key to move\n",
    "   -n name");
    FPS "%-20s Specify the id in hex of the key to move\n",
    "   -i key id");
    FPS "%-20s Specify a file to read the id of the key to move\n",
    "   -j key id file");
    FPS "%-20s Specify the token to move the key to\n",
    "   -g target token");
    FPS "%-15s Unwrap a new key from a data file.\n", "-U");
    FPS "%-20s Specify the data file to read the encrypted key from.\n",
    "   -k key file");
    FPS "%-20s Specify the nickname of the new key\n",
    "   -n name");
    FPS "%-20s Specify the id in hex of the new key\n",
    "   -i key id");
    FPS "%-20s Specify a file to read the id of the new key\n",
    "   -j key id file");
    FPS "%-20s Specify the keyType of the new key\n",
    "   -t type");
    FPS "%-20s", "  valid types: ");
    for (i = 0; i < keyArraySize; i++) {
        FPS "%s%c", keyArray[i].label, i == keyArraySize-1? '\n':',');
    }
    FPS "%-20s Specify the nickname of the wrapping key\n",
    "   -w wrap name");
    FPS "%-20s Specify the id in hex of the wrapping key\n",
    "   -x wrap key id");
    FPS "%-20s Specify a file to read the id of the wrapping key\n",
    "   -y wrap key id file");
    FPS "%-15s Wrap a new key to a data file. [not yet implemented]\n", "-W");
    FPS "%-20s Specify the data file to write the encrypted key to.\n",
    "   -k key file");
    FPS "%-20s Specify the nickname of the key to wrap\n",
    "   -n name");
    FPS "%-20s Specify the id in hex of the key to wrap\n",
    "   -i key id");
    FPS "%-20s Specify a file to read the id of the key to wrap\n",
    "   -j key id file");
    FPS "%-20s Specify the nickname of the wrapping key\n",
    "   -w wrap name");
    FPS "%-20s Specify the id in hex of the wrapping key\n",
    "   -x wrap key id");
    FPS "%-20s Specify a file to read the id of the wrapping key\n",
    "   -y wrap key id file");
    FPS "%-15s Options valid for all commands\n", "std_opts");
    FPS "%-20s The directory where the NSS db's reside\n",
    "   -d certdir");
    FPS "%-20s Prefix for the NSS db's\n",
    "   -P db prefix");
    FPS "%-20s Specify password on the command line\n",
    "   -p password");
    FPS "%-20s Specify password file on the command line\n",
    "   -f password file");
    FPS "%-20s Specify token to act on\n",
    "   -h token");
    exit(1);
#undef FPS
}

/*  Certutil commands  */
enum {
    cmd_CreateNewKey = 0,
    cmd_DeleteKey,
    cmd_ImportKey,
    cmd_ExportKey,
    cmd_WrapKey,
    cmd_UnwrapKey,
    cmd_MoveKey,
    cmd_ListKeys,
    cmd_PrintHelp
};

/*  Certutil options */
enum {
    opt_CertDir = 0,
    opt_PasswordFile,
    opt_TargetToken,
    opt_TokenName,
    opt_KeyID,
    opt_KeyIDFile,
    opt_KeyType,
    opt_Nickname,
    opt_KeyFile,
    opt_Password,
    opt_dbPrefix,
    opt_RW,
    opt_KeySize,
    opt_WrapKeyName,
    opt_WrapKeyID,
    opt_WrapKeyIDFile,
    opt_NoiseFile
};

static secuCommandFlag symKeyUtil_commands[] =
    {
      { /* cmd_CreateNewKey        */ 'K', PR_FALSE, 0, PR_FALSE },
      { /* cmd_DeleteKey           */ 'D', PR_FALSE, 0, PR_FALSE },
      { /* cmd_ImportKey           */ 'I', PR_FALSE, 0, PR_FALSE },
      { /* cmd_ExportKey           */ 'E', PR_FALSE, 0, PR_FALSE },
      { /* cmd_WrapKey             */ 'W', PR_FALSE, 0, PR_FALSE },
      { /* cmd_UnwrapKey           */ 'U', PR_FALSE, 0, PR_FALSE },
      { /* cmd_MoveKey             */ 'M', PR_FALSE, 0, PR_FALSE },
      { /* cmd_ListKeys            */ 'L', PR_FALSE, 0, PR_FALSE },
      { /* cmd_PrintHelp           */ 'H', PR_FALSE, 0, PR_FALSE },
    };

static secuCommandFlag symKeyUtil_options[] =
    {
      { /* opt_CertDir             */ 'd', PR_TRUE, 0, PR_FALSE },
      { /* opt_PasswordFile        */ 'f', PR_TRUE, 0, PR_FALSE },
      { /* opt_TargetToken         */ 'g', PR_TRUE, 0, PR_FALSE },
      { /* opt_TokenName           */ 'h', PR_TRUE, 0, PR_FALSE },
      { /* opt_KeyID               */ 'i', PR_TRUE, 0, PR_FALSE },
      { /* opt_KeyIDFile           */ 'j', PR_TRUE, 0, PR_FALSE },
      { /* opt_KeyType             */ 't', PR_TRUE, 0, PR_FALSE },
      { /* opt_Nickname            */ 'n', PR_TRUE, 0, PR_FALSE },
      { /* opt_KeyFile             */ 'k', PR_TRUE, 0, PR_FALSE },
      { /* opt_Password            */ 'p', PR_TRUE, 0, PR_FALSE },
      { /* opt_dbPrefix            */ 'P', PR_TRUE, 0, PR_FALSE },
      { /* opt_RW                  */ 'r', PR_FALSE, 0, PR_FALSE },
      { /* opt_KeySize             */ 's', PR_TRUE, 0, PR_FALSE },
      { /* opt_WrapKeyName         */ 'w', PR_TRUE, 0, PR_FALSE },
      { /* opt_WrapKeyID           */ 'x', PR_TRUE, 0, PR_FALSE },
      { /* opt_WrapKeyIDFile       */ 'y', PR_TRUE, 0, PR_FALSE },
      { /* opt_NoiseFile           */ 'z', PR_TRUE, 0, PR_FALSE },
    };

int
main(int argc, char **argv)
{
    PK11SlotInfo *slot = NULL;
    char *slotname = "internal";
    char *certPrefix = "";
    CK_MECHANISM_TYPE keyType = CKM_SHA_1_HMAC;
    int keySize = 0;
    char *name = NULL;
    char *wrapName = NULL;
    secuPWData pwdata = { PW_NONE, 0 };
    PRBool readOnly = PR_FALSE;
    SECItem key;
    SECItem keyID;
    SECItem wrapKeyID;
    int commandsEntered = 0;
    int commandToRun = 0;
    char *progName;
    int i;
    SECStatus rv = SECFailure;

    secuCommand symKeyUtil;
    symKeyUtil.numCommands = sizeof(symKeyUtil_commands) / sizeof(secuCommandFlag);
    symKeyUtil.numOptions = sizeof(symKeyUtil_options) / sizeof(secuCommandFlag);
    symKeyUtil.commands = symKeyUtil_commands;
    symKeyUtil.options = symKeyUtil_options;

    key.data = NULL;
    key.len = 0;
    keyID.data = NULL;
    keyID.len = 0;
    wrapKeyID.data = NULL;
    wrapKeyID.len = 0;

    progName = strrchr(argv[0], '/');
    progName = progName ? progName + 1 : argv[0];

    rv = SECU_ParseCommandLine(argc, argv, progName, &symKeyUtil);

    if (rv != SECSuccess)
        Usage(progName);

    rv = SECFailure;

    /* -H print help */
    if (symKeyUtil.commands[cmd_PrintHelp].activated)
        LongUsage(progName);

    /* -f password file, -p password */
    if (symKeyUtil.options[opt_PasswordFile].arg) {
        pwdata.source = PW_FROMFILE;
        pwdata.data = symKeyUtil.options[opt_PasswordFile].arg;
    } else if (symKeyUtil.options[opt_Password].arg) {
        pwdata.source = PW_PLAINTEXT;
        pwdata.data = symKeyUtil.options[opt_Password].arg;
    }

    /* -d directory */
    if (symKeyUtil.options[opt_CertDir].activated)
        SECU_ConfigDirectory(symKeyUtil.options[opt_CertDir].arg);

    /* -s key size */
    if (symKeyUtil.options[opt_KeySize].activated) {
        keySize = PORT_Atoi(symKeyUtil.options[opt_KeySize].arg);
    }

    /*  -h specify token name  */
    if (symKeyUtil.options[opt_TokenName].activated) {
        if (PL_strcmp(symKeyUtil.options[opt_TokenName].arg, "all") == 0)
            slotname = NULL;
        else
            slotname = PL_strdup(symKeyUtil.options[opt_TokenName].arg);
    }

    /* -t key type */
    if (symKeyUtil.options[opt_KeyType].activated) {
        keyType = GetKeyMechFromString(symKeyUtil.options[opt_KeyType].arg);
        if (keyType == (CK_MECHANISM_TYPE)-1) {
            PR_fprintf(PR_STDERR,
                       "%s unknown key type (%s).\n",
                       progName, symKeyUtil.options[opt_KeyType].arg);
            return 255;
        }
    }

    /* -k for import and unwrap, it specifies an input file to read from,
     * for export and wrap it specifies an output file to write to */
    if (symKeyUtil.options[opt_KeyFile].activated) {
        if (symKeyUtil.commands[cmd_ImportKey].activated ||
            symKeyUtil.commands[cmd_UnwrapKey].activated) {
            int ret = ReadBuf(symKeyUtil.options[opt_KeyFile].arg, &key);
            if (ret < 0) {
                PR_fprintf(PR_STDERR,
                           "%s Couldn't read key file (%s).\n",
                           progName, symKeyUtil.options[opt_KeyFile].arg);
                return 255;
            }
        }
    }

    /* -i specify the key ID */
    if (symKeyUtil.options[opt_KeyID].activated) {
        int ret = HexToBuf(symKeyUtil.options[opt_KeyID].arg, &keyID);
        if (ret < 0) {
            PR_fprintf(PR_STDERR,
                       "%s invalid key ID (%s).\n",
                       progName, symKeyUtil.options[opt_KeyID].arg);
            return 255;
        }
    }

    /* -i & -j are mutually exclusive */
    if ((symKeyUtil.options[opt_KeyID].activated) &&
        (symKeyUtil.options[opt_KeyIDFile].activated)) {
        PR_fprintf(PR_STDERR,
                   "%s -i and -j options are mutually exclusive.\n", progName);
        return 255;
    }

    /* -x specify the Wrap key ID */
    if (symKeyUtil.options[opt_WrapKeyID].activated) {
        int ret = HexToBuf(symKeyUtil.options[opt_WrapKeyID].arg, &wrapKeyID);
        if (ret < 0) {
            PR_fprintf(PR_STDERR,
                       "%s invalid key ID (%s).\n",
                       progName, symKeyUtil.options[opt_WrapKeyID].arg);
            return 255;
        }
    }

    /* -x & -y are mutually exclusive */
    if ((symKeyUtil.options[opt_KeyID].activated) &&
        (symKeyUtil.options[opt_KeyIDFile].activated)) {
        PR_fprintf(PR_STDERR,
                   "%s -i and -j options are mutually exclusive.\n", progName);
        return 255;
    }

    /* -y specify the key ID */
    if (symKeyUtil.options[opt_WrapKeyIDFile].activated) {
        int ret = ReadBuf(symKeyUtil.options[opt_WrapKeyIDFile].arg,
                          &wrapKeyID);
        if (ret < 0) {
            PR_fprintf(PR_STDERR,
                       "%s Couldn't read key ID file (%s).\n",
                       progName, symKeyUtil.options[opt_WrapKeyIDFile].arg);
            return 255;
        }
    }

    /*  -P certdb name prefix */
    if (symKeyUtil.options[opt_dbPrefix].activated)
        certPrefix = symKeyUtil.options[opt_dbPrefix].arg;

    /*  Check number of commands entered.  */
    commandsEntered = 0;
    for (i = 0; i < symKeyUtil.numCommands; i++) {
        if (symKeyUtil.commands[i].activated) {
            commandToRun = symKeyUtil.commands[i].flag;
            commandsEntered++;
        }
        if (commandsEntered > 1)
            break;
    }
    if (commandsEntered > 1) {
        PR_fprintf(PR_STDERR, "%s: only one command at a time!\n", progName);
        PR_fprintf(PR_STDERR, "You entered: ");
        for (i = 0; i < symKeyUtil.numCommands; i++) {
            if (symKeyUtil.commands[i].activated)
                PR_fprintf(PR_STDERR, " -%c", symKeyUtil.commands[i].flag);
        }
        PR_fprintf(PR_STDERR, "\n");
        return 255;
    }
    if (commandsEntered == 0) {
        PR_fprintf(PR_STDERR, "%s: you must enter a command!\n", progName);
        Usage(progName);
    }

    if (symKeyUtil.commands[cmd_ListKeys].activated ||
        symKeyUtil.commands[cmd_PrintHelp].activated ||
        symKeyUtil.commands[cmd_ExportKey].activated ||
        symKeyUtil.commands[cmd_WrapKey].activated) {
        readOnly = !symKeyUtil.options[opt_RW].activated;
    }

    if ((symKeyUtil.commands[cmd_ImportKey].activated ||
         symKeyUtil.commands[cmd_ExportKey].activated ||
         symKeyUtil.commands[cmd_WrapKey].activated ||
         symKeyUtil.commands[cmd_UnwrapKey].activated) &&
        !symKeyUtil.options[opt_KeyFile].activated) {
        PR_fprintf(PR_STDERR,
                   "%s -%c: keyfile is required for this command (-k).\n",
                   progName, commandToRun);
        return 255;
    }

    /*  -E, -D, -W, and all require -n, -i, or -j to identify the key  */
    if ((symKeyUtil.commands[cmd_ExportKey].activated ||
         symKeyUtil.commands[cmd_DeleteKey].activated ||
         symKeyUtil.commands[cmd_WrapKey].activated) &&
        !(symKeyUtil.options[opt_Nickname].activated ||
          symKeyUtil.options[opt_KeyID].activated ||
          symKeyUtil.options[opt_KeyIDFile].activated)) {
        PR_fprintf(PR_STDERR,
                   "%s -%c: nickname or id is required for this command (-n, -i, -j).\n",
                   progName, commandToRun);
        return 255;
    }

    /*  -W, -U, and all  -w, -x, or -y to identify the wrapping key  */
    if ((symKeyUtil.commands[cmd_WrapKey].activated ||
         symKeyUtil.commands[cmd_UnwrapKey].activated) &&
        !(symKeyUtil.options[opt_WrapKeyName].activated ||
          symKeyUtil.options[opt_WrapKeyID].activated ||
          symKeyUtil.options[opt_WrapKeyIDFile].activated)) {
        PR_fprintf(PR_STDERR,
                   "%s -%c: wrap key is required for this command (-w, -x, or -y).\n",
                   progName, commandToRun);
        return 255;
    }

    /* -M needs the target slot  (-g) */
    if (symKeyUtil.commands[cmd_MoveKey].activated &&
        !symKeyUtil.options[opt_TargetToken].activated) {
        PR_fprintf(PR_STDERR,
                   "%s -%c: target token is required for this command (-g).\n",
                   progName, commandToRun);
        return 255;
    }

    /*  Using slotname == NULL for listing keys and certs on all slots,
     *  but only that. */
    if (!(symKeyUtil.commands[cmd_ListKeys].activated) && slotname == NULL) {
        PR_fprintf(PR_STDERR,
                   "%s -%c: cannot use \"-h all\" for this command.\n",
                   progName, commandToRun);
        return 255;
    }

    name = SECU_GetOptionArg(&symKeyUtil, opt_Nickname);
    wrapName = SECU_GetOptionArg(&symKeyUtil, opt_WrapKeyName);

    PK11_SetPasswordFunc(SECU_GetModulePassword);

    /*  Initialize NSPR and NSS.  */
    PR_Init(PR_SYSTEM_THREAD, PR_PRIORITY_NORMAL, 1);
    rv = NSS_Initialize(SECU_ConfigDirectory(NULL), certPrefix, certPrefix,
                        "secmod.db", readOnly ? NSS_INIT_READONLY : 0);
    if (rv != SECSuccess) {
        SECU_PrintPRandOSError(progName);
        goto shutdown;
    }
    rv = SECFailure;

    if (PL_strcmp(slotname, "internal") == 0)
        slot = PK11_GetInternalKeySlot();
    else if (slotname != NULL)
        slot = PK11_FindSlotByName(slotname);

    /* generating a new key */
    if (symKeyUtil.commands[cmd_CreateNewKey].activated) {
        PK11SymKey *symKey;

        symKey = PK11_TokenKeyGen(slot, keyType, NULL, keySize,
                                  NULL, PR_TRUE, &pwdata);
        if (!symKey) {
            PR_fprintf(PR_STDERR, "%s: Token Key Gen Failed\n", progName);
            goto shutdown;
        }
        if (symKeyUtil.options[opt_Nickname].activated) {
            rv = PK11_SetSymKeyNickname(symKey, name);
            if (rv != SECSuccess) {
                PK11_DeleteTokenSymKey(symKey);
                PK11_FreeSymKey(symKey);
                PR_fprintf(PR_STDERR, "%s: Couldn't set nickname on key\n",
                           progName);
                goto shutdown;
            }
        }
        rv = SECSuccess;
        PrintKey(symKey);
        PK11_FreeSymKey(symKey);
    }
    if (symKeyUtil.commands[cmd_DeleteKey].activated) {
        PK11SymKey *symKey = FindKey(slot, name, &keyID, &pwdata);

        if (!symKey) {
            char *keyName = keyID.data ? BufToHex(&keyID) : PORT_Strdup(name);
            PR_fprintf(PR_STDERR, "%s: Couldn't find key %s on %s\n",
                       progName, keyName, PK11_GetTokenName(slot));
            PORT_Free(keyName);
            goto shutdown;
        }

        rv = PK11_DeleteTokenSymKey(symKey);
        FreeKeyList(symKey);
        if (rv != SECSuccess) {
            PR_fprintf(PR_STDERR, "%s: Couldn't Delete Key \n", progName);
            goto shutdown;
        }
    }
    if (symKeyUtil.commands[cmd_UnwrapKey].activated) {
        PK11SymKey *wrapKey = FindKey(slot, wrapName, &wrapKeyID, &pwdata);
        PK11SymKey *symKey;
        CK_MECHANISM_TYPE mechanism;

        if (!wrapKey) {
            char *keyName = wrapKeyID.data ? BufToHex(&wrapKeyID)
                                           : PORT_Strdup(wrapName);
            PR_fprintf(PR_STDERR, "%s: Couldn't find key %s on %s\n",
                       progName, keyName, PK11_GetTokenName(slot));
            PORT_Free(keyName);
            goto shutdown;
        }
        mechanism = GetWrapMechanism(wrapKey);
        if (mechanism == CKM_INVALID_MECHANISM) {
            char *keyName = wrapKeyID.data ? BufToHex(&wrapKeyID)
                                           : PORT_Strdup(wrapName);
            PR_fprintf(PR_STDERR, "%s: %s on %s is an invalid wrapping key\n",
                       progName, keyName, PK11_GetTokenName(slot));
            PORT_Free(keyName);
            PK11_FreeSymKey(wrapKey);
            goto shutdown;
        }

        symKey = PK11_UnwrapSymKeyWithFlagsPerm(wrapKey, mechanism, NULL,
                                                &key, keyType, CKA_ENCRYPT, keySize, 0, PR_TRUE);
        PK11_FreeSymKey(wrapKey);
        if (!symKey) {
            PR_fprintf(PR_STDERR, "%s: Unwrap Key Failed\n", progName);
            goto shutdown;
        }

        if (symKeyUtil.options[opt_Nickname].activated) {
            rv = PK11_SetSymKeyNickname(symKey, name);
            if (rv != SECSuccess) {
                PR_fprintf(PR_STDERR, "%s: Couldn't set name on key\n",
                           progName);
                PK11_DeleteTokenSymKey(symKey);
                PK11_FreeSymKey(symKey);
                goto shutdown;
            }
        }
        rv = SECSuccess;
        PrintKey(symKey);
        PK11_FreeSymKey(symKey);
    }

#define MAX_KEY_SIZE 4098
    if (symKeyUtil.commands[cmd_WrapKey].activated) {
        PK11SymKey *symKey = FindKey(slot, name, &keyID, &pwdata);
        PK11SymKey *wrapKey;
        CK_MECHANISM_TYPE mechanism;
        SECItem data;
        unsigned char buf[MAX_KEY_SIZE];
        int ret;

        if (!symKey) {
            char *keyName = keyID.data ? BufToHex(&keyID) : PORT_Strdup(name);
            PR_fprintf(PR_STDERR, "%s: Couldn't find key %s on %s\n",
                       progName, keyName, PK11_GetTokenName(slot));
            PORT_Free(keyName);
            goto shutdown;
        }

        wrapKey = FindKey(slot, wrapName, &wrapKeyID, &pwdata);
        if (!wrapKey) {
            char *keyName = wrapKeyID.data ? BufToHex(&wrapKeyID)
                                           : PORT_Strdup(wrapName);
            PR_fprintf(PR_STDERR, "%s: Couldn't find key %s on %s\n",
                       progName, keyName, PK11_GetTokenName(slot));
            PORT_Free(keyName);
            PK11_FreeSymKey(symKey);
            goto shutdown;
        }

        mechanism = GetWrapMechanism(wrapKey);
        if (mechanism == CKM_INVALID_MECHANISM) {
            char *keyName = wrapKeyID.data ? BufToHex(&wrapKeyID)
                                           : PORT_Strdup(wrapName);
            PR_fprintf(PR_STDERR, "%s: %s on %s is an invalid wrapping key\n",
                       progName, keyName, PK11_GetTokenName(slot));
            PORT_Free(keyName);
            PK11_FreeSymKey(symKey);
            PK11_FreeSymKey(wrapKey);
            goto shutdown;
        }

        data.data = buf;
        data.len = sizeof(buf);
        rv = PK11_WrapSymKey(mechanism, NULL, wrapKey, symKey, &data);
        PK11_FreeSymKey(symKey);
        PK11_FreeSymKey(wrapKey);
        if (rv != SECSuccess) {
            PR_fprintf(PR_STDERR, "%s: Couldn't wrap key\n", progName);
            goto shutdown;
        }

        /* WriteBuf outputs it's own error using SECU_PrintError */
        ret = WriteBuf(symKeyUtil.options[opt_KeyFile].arg, &data);
        if (ret < 0) {
            goto shutdown;
        }
    }

    if (symKeyUtil.commands[cmd_ImportKey].activated) {
        PK11SymKey *symKey = PK11_ImportSymKey(slot, keyType,
                                               PK11_OriginUnwrap, CKA_ENCRYPT, &key, &pwdata);
        if (!symKey) {
            PR_fprintf(PR_STDERR, "%s: Import Key Failed\n", progName);
            goto shutdown;
        }
        if (symKeyUtil.options[opt_Nickname].activated) {
            rv = PK11_SetSymKeyNickname(symKey, name);
            if (rv != SECSuccess) {
                PR_fprintf(PR_STDERR, "%s: Couldn't set name on key\n",
                           progName);
                PK11_DeleteTokenSymKey(symKey);
                PK11_FreeSymKey(symKey);
                goto shutdown;
            }
        }
        rv = SECSuccess;
        PrintKey(symKey);
        PK11_FreeSymKey(symKey);
    }

    /*  List certs (-L)  */
    if (symKeyUtil.commands[cmd_ListKeys].activated) {
        int printLabel = 1;
        if (slot) {
            rv = ListKeys(slot, &printLabel, &pwdata);
        } else {
            /* loop over all the slots */
            PK11SlotList *slotList = PK11_GetAllTokens(CKM_INVALID_MECHANISM,
                                                       PR_FALSE, PR_FALSE, &pwdata);
            if (slotList == NULL) {
                PR_fprintf(PR_STDERR, "%s: No tokens found\n", progName);
            } else {
                PK11SlotListElement *se;
                for (se = PK11_GetFirstSafe(slotList); se;
                     se = PK11_GetNextSafe(slotList, se, PR_FALSE)) {
                    rv = ListKeys(se->slot, &printLabel, &pwdata);
                    if (rv != SECSuccess) {
                        break;
                    }
                }
                if (se) {
                    PORT_CheckSuccess(PK11_FreeSlotListElement(slotList, se));
                }
                PK11_FreeSlotList(slotList);
            }
        }
    }

    /*  Move key (-M)  */
    if (symKeyUtil.commands[cmd_MoveKey].activated) {
        PK11SlotInfo *target;
        char *targetName = symKeyUtil.options[opt_TargetToken].arg;
        PK11SymKey *newKey;
        PK11SymKey *symKey = FindKey(slot, name, &keyID, &pwdata);
        char *keyName;

        if (!symKey) {
            keyName = keyID.data ? BufToHex(&keyID) : PORT_Strdup(name);
            PR_fprintf(PR_STDERR, "%s: Couldn't find key %s on %s\n",
                       progName, keyName, PK11_GetTokenName(slot));
            PORT_Free(keyName);
            goto shutdown;
        }
        target = PK11_FindSlotByName(targetName);
        if (!target) {
            PR_fprintf(PR_STDERR, "%s: Couldn't find slot %s\n",
                       progName, targetName);
            goto shutdown;
        }
        rv = PK11_Authenticate(target, PR_FALSE, &pwdata);
        if (rv != SECSuccess) {
            PR_fprintf(PR_STDERR, "%s: Failed to log into %s\n",
                       progName, targetName);
            goto shutdown;
        }
        rv = SECFailure;
        newKey = PK11_MoveSymKey(target, CKA_ENCRYPT, 0, PR_TRUE, symKey);
        if (!newKey) {
            PR_fprintf(PR_STDERR, "%s: Couldn't move the key \n", progName);
            goto shutdown;
        }
        keyName = PK11_GetSymKeyNickname(symKey);
        if (keyName) {
            rv = PK11_SetSymKeyNickname(newKey, keyName);
            if (rv != SECSuccess) {
                PK11_DeleteTokenSymKey(newKey);
                PK11_FreeSymKey(newKey);
                PR_fprintf(PR_STDERR, "%s: Couldn't set nickname on key\n",
                           progName);
                goto shutdown;
            }
        }
        PK11_FreeSymKey(newKey);
        rv = SECSuccess;
    }

shutdown:
    if (rv != SECSuccess) {
        PR_fprintf(PR_STDERR, "%s: %s\n", progName,
                   SECU_Strerror(PORT_GetError()));
    }

    if (key.data) {
        PORT_Free(key.data);
    }

    if (keyID.data) {
        PORT_Free(keyID.data);
    }

    if (slot) {
        PK11_FreeSlot(slot);
    }

    if (NSS_Shutdown() != SECSuccess) {
        exit(1);
    }

    if (rv == SECSuccess) {
        return 0;
    } else {
        return 255;
    }
}
