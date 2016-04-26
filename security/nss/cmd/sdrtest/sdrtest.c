/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Test program for SDR (Secret Decoder Ring) functions.
 */

#include "nspr.h"
#include "string.h"
#include "nss.h"
#include "secutil.h"
#include "cert.h"
#include "pk11func.h"

#include "plgetopt.h"
#include "pk11sdr.h"
#include "nssb64.h"

#define DEFAULT_VALUE "Test"
static const char default_value[] = { DEFAULT_VALUE };

PRFileDesc *pr_stderr;
PRBool verbose = PR_FALSE;

static void
synopsis(char *program_name)
{
    PR_fprintf(pr_stderr,
               "Usage: %s [<common>] -i <input-file>\n"
               "       %s [<common>]  -o <output-file>\n"
               "       <common> [-d dir] [-v] [-t text] [-a] [-f pwfile | -p pwd]\n",
               program_name, program_name);
}

static void
short_usage(char *program_name)
{
    PR_fprintf(pr_stderr,
               "Type %s -H for more detailed descriptions\n",
               program_name);
    synopsis(program_name);
}

static void
long_usage(char *program_name)
{
    synopsis(program_name);
    PR_fprintf(pr_stderr, "\nSecret Decoder Test:\n");
    PR_fprintf(pr_stderr,
               "  %-13s Read encrypted data from \"file\"\n",
               "-i file");
    PR_fprintf(pr_stderr,
               "  %-13s Write newly generated encrypted data to \"file\"\n",
               "-o file");
    PR_fprintf(pr_stderr,
               "  %-13s Use \"text\" as the plaintext for encryption and verification\n",
               "-t text");
    PR_fprintf(pr_stderr,
               "  %-13s Find security databases in \"dbdir\"\n",
               "-d dbdir");
    PR_fprintf(pr_stderr,
               "  %-13s read the password from \"pwfile\"\n",
               "-f pwfile");
    PR_fprintf(pr_stderr,
               "  %-13s supply \"password\" on the command line\n",
               "-p password");
}

int
readStdin(SECItem *result)
{
    unsigned int bufsize = 0;
    int cc;
    unsigned int wanted = 8192U;

    result->len = 0;
    result->data = NULL;
    do {
        if (bufsize < wanted) {
            unsigned char *tmpData = (unsigned char *)PR_Realloc(result->data, wanted);
            if (!tmpData) {
                if (verbose)
                    PR_fprintf(pr_stderr, "Allocation of buffer failed\n");
                return -1;
            }
            result->data = tmpData;
            bufsize = wanted;
        }
        cc = PR_Read(PR_STDIN, result->data + result->len, bufsize - result->len);
        if (cc > 0) {
            result->len += (unsigned)cc;
            if (result->len >= wanted)
                wanted *= 2;
        }
    } while (cc > 0);
    return cc;
}

int
readInputFile(const char *filename, SECItem *result)
{
    PRFileDesc *file /* = PR_OpenFile(input_file, 0) */;
    PRFileInfo info;
    PRStatus s;
    PRInt32 count;
    int retval = -1;

    file = PR_Open(filename, PR_RDONLY, 0);
    if (!file) {
        if (verbose)
            PR_fprintf(pr_stderr, "Open of file %s failed\n", filename);
        goto loser;
    }

    s = PR_GetOpenFileInfo(file, &info);
    if (s != PR_SUCCESS) {
        if (verbose)
            PR_fprintf(pr_stderr, "File info operation failed\n");
        goto file_loser;
    }

    result->len = info.size;
    result->data = (unsigned char *)PR_Malloc(result->len);
    if (!result->data) {
        if (verbose)
            PR_fprintf(pr_stderr, "Allocation of buffer failed\n");
        goto file_loser;
    }

    count = PR_Read(file, result->data, result->len);
    if (count != result->len) {
        if (verbose)
            PR_fprintf(pr_stderr, "Read failed\n");
        goto file_loser;
    }
    retval = 0;

file_loser:
    PR_Close(file);
loser:
    return retval;
}

int
main(int argc, char **argv)
{
    int retval = 0; /* 0 - test succeeded.  -1 - test failed */
    SECStatus rv;
    PLOptState *optstate;
    PLOptStatus optstatus;
    char *program_name;
    const char *input_file = NULL;     /* read encrypted data from here (or create) */
    const char *output_file = NULL;    /* write new encrypted data here */
    const char *value = default_value; /* Use this for plaintext */
    SECItem data;
    SECItem result = { 0, 0, 0 };
    SECItem text;
    PRBool ascii = PR_FALSE;
    secuPWData pwdata = { PW_NONE, 0 };

    pr_stderr = PR_STDERR;
    result.data = 0;
    text.data = 0;
    text.len = 0;

    program_name = PL_strrchr(argv[0], '/');
    program_name = program_name ? (program_name + 1) : argv[0];

    optstate = PL_CreateOptState(argc, argv, "?Had:i:o:t:vf:p:");
    if (optstate == NULL) {
        SECU_PrintError(program_name, "PL_CreateOptState failed");
        return -1;
    }

    while ((optstatus = PL_GetNextOpt(optstate)) == PL_OPT_OK) {
        switch (optstate->option) {
            case '?':
                short_usage(program_name);
                return retval;

            case 'H':
                long_usage(program_name);
                return retval;

            case 'a':
                ascii = PR_TRUE;
                break;

            case 'd':
                SECU_ConfigDirectory(optstate->value);
                break;

            case 'i':
                input_file = optstate->value;
                break;

            case 'o':
                output_file = optstate->value;
                break;

            case 't':
                value = optstate->value;
                break;

            case 'f':
                if (pwdata.data) {
                    PORT_Free(pwdata.data);
                    short_usage(program_name);
                    return -1;
                }
                pwdata.source = PW_FROMFILE;
                pwdata.data = PORT_Strdup(optstate->value);
                break;

            case 'p':
                if (pwdata.data) {
                    PORT_Free(pwdata.data);
                    short_usage(program_name);
                    return -1;
                }
                pwdata.source = PW_PLAINTEXT;
                pwdata.data = PORT_Strdup(optstate->value);
                break;

            case 'v':
                verbose = PR_TRUE;
                break;
        }
    }
    PL_DestroyOptState(optstate);
    if (optstatus == PL_OPT_BAD) {
        short_usage(program_name);
        return -1;
    }
    if (!output_file && !input_file && value == default_value) {
        short_usage(program_name);
        PR_fprintf(pr_stderr, "Must specify at least one of -t, -i or -o \n");
        return -1;
    }

    /*
     * Initialize the Security libraries.
     */
    PK11_SetPasswordFunc(SECU_GetModulePassword);

    if (output_file) {
        rv = NSS_InitReadWrite(SECU_ConfigDirectory(NULL));
    } else {
        rv = NSS_Init(SECU_ConfigDirectory(NULL));
    }
    if (rv != SECSuccess) {
        SECU_PrintError(program_name, "NSS_Init failed");
        retval = -1;
        goto prdone;
    }

    /* Convert value into an item */
    data.data = (unsigned char *)value;
    data.len = strlen(value);

    /* Get the encrypted result, either from the input file
     * or from encrypting the plaintext value
     */
    if (input_file) {
        if (verbose)
            printf("Reading data from %s\n", input_file);

        if (!strcmp(input_file, "-")) {
            retval = readStdin(&result);
            ascii = PR_TRUE;
        } else {
            retval = readInputFile(input_file, &result);
        }
        if (retval != 0)
            goto loser;
        if (ascii) {
            /* input was base64 encoded.  Decode it. */
            SECItem newResult = { 0, 0, 0 };
            SECItem *ok = NSSBase64_DecodeBuffer(NULL, &newResult,
                                                 (const char *)result.data, result.len);
            if (!ok) {
                SECU_PrintError(program_name, "Base 64 decode failed");
                retval = -1;
                goto loser;
            }
            SECITEM_ZfreeItem(&result, PR_FALSE);
            result = *ok;
        }
    } else {
        SECItem keyid = { 0, 0, 0 };
        SECItem outBuf = { 0, 0, 0 };
        PK11SlotInfo *slot = NULL;

        /* sigh, initialize the key database */
        slot = PK11_GetInternalKeySlot();
        if (slot && PK11_NeedUserInit(slot)) {
            switch (pwdata.source) {
                case PW_FROMFILE:
                    rv = SECU_ChangePW(slot, 0, pwdata.data);
                    break;
                case PW_PLAINTEXT:
                    rv = SECU_ChangePW(slot, pwdata.data, 0);
                    break;
                default:
                    rv = SECU_ChangePW(slot, "", 0);
                    break;
            }
            if (rv != SECSuccess) {
                SECU_PrintError(program_name, "Failed to initialize slot \"%s\"",
                                PK11_GetSlotName(slot));
                return SECFailure;
            }
        }
        if (slot) {
            PK11_FreeSlot(slot);
        }

        rv = PK11SDR_Encrypt(&keyid, &data, &result, &pwdata);
        if (rv != SECSuccess) {
            if (verbose)
                SECU_PrintError(program_name, "Encrypt operation failed\n");
            retval = -1;
            goto loser;
        }

        if (verbose)
            printf("Encrypted result is %d bytes long\n", result.len);

        if (!strcmp(output_file, "-")) {
            ascii = PR_TRUE;
        }

        if (ascii) {
            /* base64 encode output. */
            char *newResult = NSSBase64_EncodeItem(NULL, NULL, 0, &result);
            if (!newResult) {
                SECU_PrintError(program_name, "Base 64 encode failed\n");
                retval = -1;
                goto loser;
            }
            outBuf.data = (unsigned char *)newResult;
            outBuf.len = strlen(newResult);
            if (verbose)
                printf("Base 64 encoded result is %d bytes long\n", outBuf.len);
        } else {
            outBuf = result;
        }

        /* -v printf("Result is %.*s\n", text.len, text.data); */
        if (output_file) {
            PRFileDesc *file;
            PRInt32 count;

            if (verbose)
                printf("Writing result to %s\n", output_file);
            if (!strcmp(output_file, "-")) {
                file = PR_STDOUT;
            } else {
                /* Write to file */
                file = PR_Open(output_file, PR_CREATE_FILE | PR_WRONLY, 0666);
            }
            if (!file) {
                if (verbose)
                    SECU_PrintError(program_name,
                                    "Open of output file %s failed\n",
                                    output_file);
                retval = -1;
                goto loser;
            }

            count = PR_Write(file, outBuf.data, outBuf.len);

            if (file == PR_STDOUT) {
                puts("");
            } else {
                PR_Close(file);
            }

            if (count != outBuf.len) {
                if (verbose)
                    SECU_PrintError(program_name, "Write failed\n");
                retval = -1;
                goto loser;
            }
            if (ascii) {
                free(outBuf.data);
            }
        }
    }

    /* Decrypt the value */
    rv = PK11SDR_Decrypt(&result, &text, &pwdata);
    if (rv != SECSuccess) {
        if (verbose)
            SECU_PrintError(program_name, "Decrypt operation failed\n");
        retval = -1;
        goto loser;
    }

    if (verbose)
        printf("Decrypted result is \"%.*s\"\n", text.len, text.data);

    /* Compare to required value */
    if (text.len != data.len || memcmp(data.data, text.data, text.len) != 0) {
        if (verbose)
            PR_fprintf(pr_stderr, "Comparison failed\n");
        retval = -1;
        goto loser;
    }

loser:
    if (text.data)
        SECITEM_ZfreeItem(&text, PR_FALSE);
    if (result.data)
        SECITEM_ZfreeItem(&result, PR_FALSE);
    if (NSS_Shutdown() != SECSuccess) {
        exit(1);
    }

prdone:
    PR_Cleanup();
    if (pwdata.data) {
        PORT_Free(pwdata.data);
    }
    return retval;
}
