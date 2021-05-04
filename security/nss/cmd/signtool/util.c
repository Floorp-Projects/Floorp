/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "signtool.h"
#include "prio.h"
#include "prmem.h"
#include "prenv.h"
#include "nss.h"

static int is_dir(char *filename);

/***********************************************************
 * Nasty hackish function definitions
 */

long *mozilla_event_queue = 0;

#ifndef XP_WIN
char *
XP_GetString(int i)
{
    /* nasty hackish cast to avoid changing the signature of
     * JAR_init_callbacks() */
    return (char *)SECU_Strerror(i);
}
#endif

void
FE_SetPasswordEnabled()
{
}

void /*MWContext*/ *
FE_GetInitContext(void)
{
    return 0;
}

void /*MWContext*/ *
XP_FindSomeContext()
{
    /* No windows context in command tools */
    return NULL;
}

void
ET_moz_CallFunction()
{
}

/*
 *  R e m o v e A l l A r c
 *
 *  Remove .arc directories that are lingering
 *  from a previous run of signtool.
 *
 */
int
RemoveAllArc(char *tree)
{
    PRDir *dir;
    PRDirEntry *entry;
    char *archive = NULL;
    int retval = 0;

    dir = PR_OpenDir(tree);
    if (!dir)
        return -1;

    for (entry = PR_ReadDir(dir, 0); entry; entry = PR_ReadDir(dir,
                                                               0)) {

        if (entry->name[0] == '.') {
            continue;
        }

        if (archive)
            PR_Free(archive);
        archive = PR_smprintf("%s/%s", tree, entry->name);

        if (PL_strcaserstr(entry->name, ".arc") ==
            (entry->name + strlen(entry->name) - 4)) {

            if (verbosity >= 0) {
                PR_fprintf(outputFD, "removing: %s\n", archive);
            }

            if (rm_dash_r(archive)) {
                PR_fprintf(errorFD, "Error removing %s\n", archive);
                errorCount++;
                retval = -1;
                goto finish;
            }
        } else if (is_dir(archive)) {
            if (RemoveAllArc(archive)) {
                retval = -1;
                goto finish;
            }
        }
    }

finish:
    PR_CloseDir(dir);
    if (archive)
        PR_Free(archive);

    return retval;
}

/*
 *  r m _ d a s h _ r
 *
 *  Remove a file, or a directory recursively.
 *
 */
int
rm_dash_r(char *path)
{
    PRDir *dir;
    PRDirEntry *entry;
    PRFileInfo fileinfo;
    char filename[FNSIZE];

    if (PR_GetFileInfo(path, &fileinfo) != PR_SUCCESS) {
        /*fprintf(stderr, "Error: Unable to access %s\n", filename);*/
        return -1;
    }
    if (fileinfo.type == PR_FILE_DIRECTORY) {

        dir = PR_OpenDir(path);
        if (!dir) {
            PR_fprintf(errorFD, "Error: Unable to open directory %s.\n", path);
            errorCount++;
            return -1;
        }

        /* Recursively delete all entries in the directory */
        while ((entry = PR_ReadDir(dir, PR_SKIP_BOTH)) != NULL) {
            sprintf(filename, "%s/%s", path, entry->name);
            if (rm_dash_r(filename)) {
                PR_CloseDir(dir);
                return -1;
            }
        }

        if (PR_CloseDir(dir) != PR_SUCCESS) {
            PR_fprintf(errorFD, "Error: Could not close %s.\n", path);
            errorCount++;
            return -1;
        }

        /* Delete the directory itself */
        if (PR_RmDir(path) != PR_SUCCESS) {
            PR_fprintf(errorFD, "Error: Unable to delete %s\n", path);
            errorCount++;
            return -1;
        }
    } else {
        if (PR_Delete(path) != PR_SUCCESS) {
            PR_fprintf(errorFD, "Error: Unable to delete %s\n", path);
            errorCount++;
            return -1;
        }
    }
    return 0;
}

/*
 *  u s a g e
 *
 *  Print some useful help information
 *
 */

void
Usage(void)
{
#define FPS PR_fprintf(outputFD,
    FPS "%s %s -a signing tool for jar files\n", LONG_PROGRAM_NAME,NSS_VERSION);
    FPS "\n\nType %s -H for more detailed descriptions\n", PROGRAM_NAME);
    FPS "\nUsage:  %s -k keyName [-b basename] [-c Compression Level]\n"
        "\t\t [-d cert-dir] [-i installer script] [-m metafile] [-x name]\n"
        "\t\t [-e extension] [-o] [-z] [-X] [--outfile] [--verbose value]\n"
        "\t\t [--norecurse] [--leavearc] [-j directory] [-Z jarfile] [-O]\n"
        "\t\t [-p password] directory-tree\n", PROGRAM_NAME);
    FPS "\t%s -J -k keyName [-b basename] [-c Compression Level]\n"
        "\t\t [-d cert-dir][-i installer script] [-m metafile] [-x name]\n"
        "\t\t [-e extension] [-o] [-z] [-X] [--outfile] [--verbose value]\n"
        "\t\t [--norecurse] [--leavearc] [-j directory] [-p password] [-O] \n"
        "\t\t directory-tree\n", PROGRAM_NAME);
    FPS "\t%s -h \n", PROGRAM_NAME);
    FPS "\t%s -H \n", PROGRAM_NAME);
    FPS "\t%s -l [-k keyName] [-d cert-dir] [--outfile] [-O] \n", PROGRAM_NAME);
    FPS "\t%s -L [-k keyName] [-d cert-dir] [--outfile] [-O] \n", PROGRAM_NAME);
    FPS "\t%s -M [--outfile] [-O] \n", PROGRAM_NAME);
    FPS "\t%s -v [-d cert-dir] [--outfile] [-O] archive\n", PROGRAM_NAME);
    FPS "\t%s -w [--outfile] [-O] archive\n" , PROGRAM_NAME);
    FPS "\t%s -G nickname [--keysize|-s size] [-t |--token tokenname]\n"
        "\t\t [--outfile] [-O] \n", PROGRAM_NAME);
    FPS "\t%s -f filename\n" , PROGRAM_NAME);
    exit(ERRX);
}

void
LongUsage(void)
{
    FPS "%s %s -a signing tool for jar files\n", LONG_PROGRAM_NAME,NSS_VERSION);
    FPS "\n%-20s  Signs the directory-tree\n",
        "signtool directory-tree");
    FPS "%-30s Nickname (key) of the certificate to sign with\n",
        "   -k keyname");
    FPS "%-30s Base filename for the .rsa and.sf files in the\n",
        "   -b basename");
    FPS "%-30s META-INF directory\n"," ");
    FPS "%-30s Set the compression level. 0-9, 0=none\n",
    "   -c CompressionLevel");
    FPS "%-30s Certificate database directory containing cert*db\n",
    "   -d certificate directory");
    FPS "%-30s and key*db\n"," ");
    FPS "%-30s Name of the installer script for SmartUpdate\n",
    "   -i installer script");
    FPS "%-30s Name of a metadata control file\n",
    "   -m metafile");
    FPS "%-30s For optimizing the archive for size.\n",
    "   -o");
    FPS "%-30s Omit Optional Headers\n"," ");
    FPS "%-30s Excludes the specified directory or file from\n",
    "   -x  directory or file name");
    FPS "%-30s signing\n"," ");
    FPS "%-30s To not store the signing time in digital\n",
    "   -z  directory or file name");
    FPS "%-30s signature\n"," ");
    FPS "%-30s Create XPI Compatible Archive. It requires -Z\n",
    "   -X  directory or file name");
    FPS "%-30s option\n"," ");
    FPS "%-30s Sign only files with the given extension\n",
    "   -e");
    FPS "%-30s Causes the specified directory to be signed and\n",
    "   -j");
    FPS "%-30s tags its entries as inline JavaScript\n"," ");
    FPS "%-30s Creates a JAR file with the specified name.\n",
    "   -Z");
    FPS "%-30s -Z option cannot be used with -J option\n"," ");
    FPS "%-30s Specifies a password for the private-key database\n",
    "   -p");
    FPS "%-30s (insecure)\n"," ");
    FPS "%-30s File to receive redirected output\n",
        "   --outfile filename");
    FPS "%-30s Sets the quantity of information generated in\n",
    "   --verbosity value");
    FPS "%-30s operation\n"," ");
    FPS "%-30s Blocks recursion into subdirectories\n",
    "   --norecurse");
    FPS "%-30s Retains the temporary .arc (archive) directories\n",
    "   --leavearc");
    FPS "%-30s -J option creates\n"," ");

    FPS "\n%-20s Signs a directory of HTML files containing JavaScript and\n",
    "-J" );
    FPS "%-20s creates as many archive files as are in the HTML tags.\n"," ");

    FPS "%-20s The options are same as without any command option given\n"," ");
    FPS "%-20s above. -Z and -J options are not allowed together\n"," ");

    FPS "\n%-20s Generates a new private-public key pair and corresponding\n",
    "-G nickname");
    FPS "%-20s object-signing certificates with the given nickname\n"," ");
    FPS "%-30s Specifies the size of the key for generated \n",
    "   --keysize|-s keysize");
    FPS "%-30s certificate\n"," ");
    FPS "%-30s Specifies which available token should generate\n",
    "   --token|-t token name ");
    FPS "%-30s the key and receive the certificate\n"," ");
    FPS "%-30s Specifies a file to receive redirected output\n",
    "   --outfile filename ");

    FPS "\n%-20s Display signtool help\n",
    "-h ");

    FPS "\n%-20s Display signtool help(Detailed)\n",
    "-H ");

    FPS "\n%-20s Lists signing certificates, including issuing CAs\n",
    "-l ");
    FPS "%-30s Certificate database directory containing cert*db\n",
    "   -d certificate directory");
    FPS "%-30s and key*db\n"," ");

    FPS "%-30s Specifies a file to receive redirected output\n",
    "   --outfile filename ");
    FPS "%-30s Specifies the nickname (key) of the certificate\n",
    "   -k keyname");

    FPS "\n%-20s Lists the certificates in your database\n",
    "-L ");
    FPS "%-30s Certificate database directory containing cert*db\n",
    "   -d certificate directory");
    FPS "%-30s and key*db\n"," ");

    FPS "%-30s Specifies a file to receive redirected output\n",
    "   --outfile filename ");
    FPS "%-30s Specifies the nickname (key) of the certificate\n",
    "   -k keyname");

    FPS "\n%-20s Lists the PKCS #11 modules available to signtool\n",
    "-M ");

    FPS "\n%-20s Displays the contents of an archive and verifies\n",
    "-v archive");
    FPS "%-20s cryptographic integrity\n"," ");
    FPS "%-30s Certificate database directory containing cert*db\n",
    "   -d certificate directory");
    FPS "%-30s and key*db\n"," ");
    FPS "%-30s Specifies a file to receive redirected output\n",
    "   --outfile filename ");

    FPS "\n%-20s Displays the names of signers in the archive\n",
    "-w archive");
    FPS "%-30s Specifies a file to receive redirected output\n",
    "   --outfile filename ");

    FPS "\n%-30s Common option to all the above.\n",
    "   -O");
    FPS "%-30s Enable OCSP checking\n"," ");

    FPS "\n%-20s Specifies a text file containing options and arguments in\n",
    "-f command-file");
    FPS "%-20s keyword=value format. Commands are taken from this file\n"," ");

    FPS  "\n\n\n");
    FPS  "Example:\n");
    FPS  "%-10s -d \"certificate directory\" -k \"certnickname\" \\",
         PROGRAM_NAME);
    FPS  "\n%-10s -p \"password\"  -X -Z \"file.xpi\" directory-tree\n"," " );
    FPS  "Common syntax to create an XPInstall compatible"
         " signed archive\n\n"," ");
    FPS  "\nCommand File Keywords and Example:\n");
    FPS "\nKeyword\t\tValue\n");
    FPS "basename\tSame as -b option\n");
    FPS "compression\tSame as -c option\n");
    FPS "certdir\t\tSame as -d option\n");
    FPS "extension\tSame as -e option\n");
    FPS "generate\tSame as -G option\n");
    FPS "installscript\tSame as -i option\n");
    FPS "javascriptdir\tSame as -j option\n");
    FPS "htmldir\t\tSame as -J option\n");
    FPS "certname\tNickname of certificate, as with -k  option\n");
    FPS "signdir\t\tThe directory to be signed, as with -k option\n");
    FPS "list\t\tSame as -l option. Value is ignored,\n"
        "    \t\tbut = sign must be present\n");
    FPS "listall\t\tSame as -L option. Value is ignored\n"
        "       \t\tbut = sign must be present\n");
    FPS "metafile\tSame as -m option\n");
    FPS "modules\t\tSame as -M option. Value is ignored,\n"
        "       \t\tbut = sign must be present\n");
    FPS "optimize\tSame as -o option. Value is ignored,\n"
        "        \tbut = sign must be present\n");
    FPS "ocsp\t\tSame as -O option\n");
    FPS "password\tSame as -p option\n");
    FPS "verify\t\tSame as -v option\n");
    FPS "who\t\tSame as -w option\n");
    FPS "exclude\t\tSame as -x option\n");
    FPS "notime\t\tSame as -z option. Value is ignored,\n"
        "      \t\tbut = sign must be present\n");
    FPS "jarfile\t\tSame as -Z option\n");
    FPS "outfile\t\tSame as --outfile option. The argument\n");
    FPS "       \t\tis the name of a file to which output\n");
    FPS "       \t\tof a file and error messages will be  \n");
    FPS "       \t\tredirected\n");
    FPS "leavearc\tSame as --leavearc option\n");
    FPS "verbosity\tSame as --verbosity option\n");
    FPS "keysize\t\tSame as -s option\n");
    FPS "token\t\tSame as -t option\n");
    FPS "xpi\t\tSame as -X option\n");
    FPS "\n\n");
    FPS "Here's an example of the use of the command file. The command\n\n");
    FPS "   signtool -d c:\\netscape\\users\\james -k mycert -Z myjar.jar \\\n"
        "   signdir > output.txt\n\n");
    FPS "becomes\n\n");
    FPS "   signtool -f somefile\n\n");
    FPS "where somefile contains the following lines:\n\n");
    FPS "   certdir=c:\\netscape\\users\\james\n"," ");
    FPS "   certname=mycert\n"," ");
    FPS "   jarfile=myjar.jar\n"," ");
    FPS "   signdir=signdir\n"," ");
    FPS "   outfile=output.txt\n"," ");
    exit(ERRX);
#undef FPS
}

/*
 *  p r i n t _ e r r o r
 *
 *  For the undocumented -E function. If an older version
 *  of communicator gives you a numeric error, we can see what
 *  really happened without doing hex math.
 *
 */

void
print_error(int err)
{
    PR_fprintf(errorFD, "Error %d: %s\n", err, JAR_get_error(err));
    errorCount++;
    give_help(err);
}

/*
 *  o u t _ o f _ m e m o r y
 *
 *  Out of memory, exit Signtool.
 *
 */
void
out_of_memory(void)
{
    PR_fprintf(errorFD, "%s: out of memory\n", PROGRAM_NAME);
    errorCount++;
    exit(ERRX);
}

/*
 *  V e r i f y C e r t D i r
 *
 *  Validate that the specified directory
 *  contains a certificate database
 *
 */
void
VerifyCertDir(char *dir, char *keyName)
{
    char fn[FNSIZE];

    /* don't try verifying if we don't have a local directory */
    if (strncmp(dir, "multiaccess:", sizeof("multiaccess:") - 1) == 0) {
        return;
    }
    /* this function is truly evil. Tools and applications should not have
     * any knowledge of actual cert databases! */
    return;

    /* This code is really broken because it makes underlying assumptions about
     * how the NSS profile directory is laid out, but these names can change
     * from release to release. */
    sprintf(fn, "%s/cert8.db", dir);

    if (PR_Access(fn, PR_ACCESS_EXISTS)) {
        PR_fprintf(errorFD, "%s: No certificate database in \"%s\"\n",
                   PROGRAM_NAME, dir);
        PR_fprintf(errorFD, "%s: Check the -d arguments that you gave\n",
                   PROGRAM_NAME);
        errorCount++;
        exit(ERRX);
    }

    if (verbosity >= 0) {
        PR_fprintf(outputFD, "using certificate directory: %s\n", dir);
    }

    if (keyName == NULL)
        return;

    /* if the user gave the -k key argument, verify that
     a key database already exists */

    sprintf(fn, "%s/key3.db", dir);

    if (PR_Access(fn, PR_ACCESS_EXISTS)) {
        PR_fprintf(errorFD, "%s: No private key database in \"%s\"\n",
                   PROGRAM_NAME,
                   dir);
        PR_fprintf(errorFD, "%s: Check the -d arguments that you gave\n",
                   PROGRAM_NAME);
        errorCount++;
        exit(ERRX);
    }
}

/*
 *  f o r e a c h
 *
 *  A recursive function to loop through all names in
 *  the specified directory, as well as all subdirectories.
 *
 *  FIX: Need to see if all platforms allow multiple
 *  opendir's to be called.
 *
 */

int foreach (char *dirname, char *prefix,
             int (*fn)(char *relpath, char *basedir, char *reldir, char *filename,
                       void *arg),
             PRBool recurse, PRBool includeDirs, void *arg)
{
    char newdir[FNSIZE];
    int retval = 0;

    PRDir *dir;
    PRDirEntry *entry;

    strcpy(newdir, dirname);
    if (*prefix) {
        strcat(newdir, "/");
        strcat(newdir, prefix);
    }

    dir = PR_OpenDir(newdir);
    if (!dir)
        return -1;

    for (entry = PR_ReadDir(dir, 0); entry; entry = PR_ReadDir(dir, 0)) {
        if (strcmp(entry->name, ".") == 0 ||
            strcmp(entry->name, "..") == 0) {
            /* no infinite recursion, please */
            continue;
        }

        /* can't sign self */
        if (!strcmp(entry->name, "META-INF"))
            continue;

        /* -x option */
        if (PL_HashTableLookup(excludeDirs, entry->name))
            continue;

        strcpy(newdir, dirname);
        if (*dirname)
            strcat(newdir, "/");

        if (*prefix) {
            strcat(newdir, prefix);
            strcat(newdir, "/");
        }
        strcat(newdir, entry->name);

        if (!is_dir(newdir) || includeDirs) {
            char newpath[FNSIZE];

            strcpy(newpath, prefix);
            if (*newpath)
                strcat(newpath, "/");
            strcat(newpath, entry->name);

            if ((*fn)(newpath, dirname, prefix, (char *)entry->name,
                      arg)) {
                retval = -1;
                break;
            }
        }

        if (is_dir(newdir)) {
            if (recurse) {
                char newprefix[FNSIZE];

                strcpy(newprefix, prefix);
                if (*newprefix) {
                    strcat(newprefix, "/");
                }
                strcat(newprefix, entry->name);

                if (foreach (dirname, newprefix, fn, recurse,
                             includeDirs, arg)) {
                    retval = -1;
                    break;
                }
            }
        }
    }

    PR_CloseDir(dir);

    return retval;
}

/*
 *  i s _ d i r
 *
 *  Return 1 if file is a directory.
 *  Wonder if this runs on a mac, trust not.
 *
 */
static int
is_dir(char *filename)
{
    PRFileInfo finfo;

    if (PR_GetFileInfo(filename, &finfo) != PR_SUCCESS) {
        printf("Unable to get information about %s\n", filename);
        return 0;
    }

    return (finfo.type == PR_FILE_DIRECTORY);
}

/***************************************************************
 *
 * s e c E r r o r S t r i n g
 *
 * Returns an error string corresponding to the given error code.
 * Doesn't cover all errors; returns a default for many.
 * Returned string is only valid until the next call of this function.
 */
const char *
secErrorString(long code)
{
    static char errstring[80]; /* dynamically constructed error string */
    char *c;                   /* the returned string */

    switch (code) {
        case SEC_ERROR_IO:
            c = "io error";
            break;
        case SEC_ERROR_LIBRARY_FAILURE:
            c = "security library failure";
            break;
        case SEC_ERROR_BAD_DATA:
            c = "bad data";
            break;
        case SEC_ERROR_OUTPUT_LEN:
            c = "output length";
            break;
        case SEC_ERROR_INPUT_LEN:
            c = "input length";
            break;
        case SEC_ERROR_INVALID_ARGS:
            c = "invalid args";
            break;
        case SEC_ERROR_EXPIRED_CERTIFICATE:
            c = "expired certificate";
            break;
        case SEC_ERROR_REVOKED_CERTIFICATE:
            c = "revoked certificate";
            break;
        case SEC_ERROR_INADEQUATE_KEY_USAGE:
            c = "inadequate key usage";
            break;
        case SEC_ERROR_INADEQUATE_CERT_TYPE:
            c = "inadequate certificate type";
            break;
        case SEC_ERROR_UNTRUSTED_CERT:
            c = "untrusted cert";
            break;
        case SEC_ERROR_NO_KRL:
            c = "no key revocation list";
            break;
        case SEC_ERROR_KRL_BAD_SIGNATURE:
            c = "key revocation list: bad signature";
            break;
        case SEC_ERROR_KRL_EXPIRED:
            c = "key revocation list expired";
            break;
        case SEC_ERROR_REVOKED_KEY:
            c = "revoked key";
            break;
        case SEC_ERROR_CRL_BAD_SIGNATURE:
            c = "certificate revocation list: bad signature";
            break;
        case SEC_ERROR_CRL_EXPIRED:
            c = "certificate revocation list expired";
            break;
        case SEC_ERROR_CRL_NOT_YET_VALID:
            c = "certificate revocation list not yet valid";
            break;
        case SEC_ERROR_UNKNOWN_ISSUER:
            c = "unknown issuer";
            break;
        case SEC_ERROR_EXPIRED_ISSUER_CERTIFICATE:
            c = "expired issuer certificate";
            break;
        case SEC_ERROR_BAD_SIGNATURE:
            c = "bad signature";
            break;
        case SEC_ERROR_BAD_KEY:
            c = "bad key";
            break;
        case SEC_ERROR_NOT_FORTEZZA_ISSUER:
            c = "not fortezza issuer";
            break;
        case SEC_ERROR_CA_CERT_INVALID:
            c = "Certificate Authority certificate invalid";
            break;
        case SEC_ERROR_EXTENSION_NOT_FOUND:
            c = "extension not found";
            break;
        case SEC_ERROR_CERT_NOT_IN_NAME_SPACE:
            c = "certificate not in name space";
            break;
        case SEC_ERROR_UNTRUSTED_ISSUER:
            c = "untrusted issuer";
            break;
        default:
            sprintf(errstring, "security error %ld", code);
            c = errstring;
            break;
    }

    return c;
}

/***************************************************************
 *
 * d i s p l a y V e r i f y L o g
 *
 * Prints the log of a cert verification.
 */
void
displayVerifyLog(CERTVerifyLog *log)
{
    CERTVerifyLogNode *node;
    CERTCertificate *cert;
    char *name;

    if (!log || (log->count <= 0)) {
        return;
    }

    for (node = log->head; node != NULL; node = node->next) {

        if (!(cert = node->cert)) {
            continue;
        }

        /* Get a name for this cert */
        if (cert->nickname != NULL) {
            name = cert->nickname;
        } else if (cert->emailAddr && cert->emailAddr[0]) {
            name = cert->emailAddr;
        } else {
            name = cert->subjectName;
        }

        printf("%s%s:\n", name,
               (node->depth > 0) ? " [Certificate Authority]" : "");

        printf("\t%s\n", secErrorString(node->error));
    }
}

/*
 *  J a r L i s t M o d u l e s
 *
 *  Print a list of the PKCS11 modules that are
 *  available. This is useful for smartcard people to
 *  make sure they have the drivers loaded.
 *
 */
void
JarListModules(void)
{
    int i;
    int count = 0;

    SECMODModuleList *modules = NULL;
    static SECMODListLock *moduleLock = NULL;

    SECMODModuleList *mlp;

    if ((moduleLock = SECMOD_GetDefaultModuleListLock()) == NULL) {
        /* this is the wrong text */
        PR_fprintf(errorFD, "%s: unable to acquire lock on module list\n",
                   PROGRAM_NAME);
        errorCount++;
        exit(ERRX);
    }

    SECMOD_GetReadLock(moduleLock);

    modules = SECMOD_GetDefaultModuleList();

    if (modules == NULL) {
        SECMOD_ReleaseReadLock(moduleLock);
        PR_fprintf(errorFD, "%s: Can't get module list\n", PROGRAM_NAME);
        errorCount++;
        exit(ERRX);
    }

    PR_fprintf(outputFD, "\nListing of PKCS11 modules\n");
    PR_fprintf(outputFD, "-----------------------------------------------\n");

    for (mlp = modules; mlp != NULL; mlp = mlp->next) {
        count++;
        PR_fprintf(outputFD, "%3d. %s\n", count, mlp->module->commonName);

        if (mlp->module->internal)
            PR_fprintf(outputFD, "          (this module is internally loaded)\n");
        else
            PR_fprintf(outputFD, "          (this is an external module)\n");

        if (mlp->module->dllName)
            PR_fprintf(outputFD, "          DLL name: %s\n",
                       mlp->module->dllName);

        if (mlp->module->slotCount == 0)
            PR_fprintf(outputFD, "          slots: There are no slots attached to this module\n");
        else
            PR_fprintf(outputFD, "          slots: %d slots attached\n",
                       mlp->module->slotCount);

        if (mlp->module->loaded == 0)
            PR_fprintf(outputFD, "          status: Not loaded\n");
        else
            PR_fprintf(outputFD, "          status: loaded\n");

        for (i = 0; i < mlp->module->slotCount; i++) {
            PK11SlotInfo *slot = mlp->module->slots[i];

            PR_fprintf(outputFD, "\n");
            PR_fprintf(outputFD, "    slot: %s\n", PK11_GetSlotName(slot));
            PR_fprintf(outputFD, "   token: %s\n", PK11_GetTokenName(slot));
        }
    }

    PR_fprintf(outputFD, "-----------------------------------------------\n");

    if (count == 0)
        PR_fprintf(outputFD,
                   "Warning: no modules were found (should have at least one)\n");

    SECMOD_ReleaseReadLock(moduleLock);
}

/**********************************************************************
 * c h o p
 *
 * Eliminates leading and trailing whitespace.  Returns a pointer to the
 * beginning of non-whitespace, or an empty string if it's all whitespace.
 */
char *
chop(char *str)
{
    char *start, *end;

    if (str) {
        start = str;

        /* Nip leading whitespace */
        while (isspace(*start)) {
            start++;
        }

        /* Nip trailing whitespace */
        if (*start) {
            end = start + strlen(start) - 1;
            while (isspace(*end) && end > start) {
                end--;
            }
            *(end + 1) = '\0';
        }

        return start;
    } else {
        return NULL;
    }
}

/***********************************************************************
 *
 * F a t a l E r r o r
 *
 * Outputs an error message and bails out of the program.
 */
void
FatalError(char *msg)
{
    if (!msg)
        msg = "";

    PR_fprintf(errorFD, "FATAL ERROR: %s\n", msg);
    errorCount++;
    exit(ERRX);
}

/*************************************************************************
 *
 * I n i t C r y p t o
 */
int
InitCrypto(char *cert_dir, PRBool readOnly)
{
    SECStatus rv;
    static int prior = 0;
    PK11SlotInfo *slotinfo;

    if (prior == 0) {
        /* some functions such as OpenKeyDB expect this path to be
     * implicitly set prior to calling */
        if (readOnly) {
            rv = NSS_Init(cert_dir);
        } else {
            rv = NSS_InitReadWrite(cert_dir);
        }
        if (rv != SECSuccess) {
            SECU_PrintPRandOSError(PROGRAM_NAME);
            exit(-1);
        }

        SECU_ConfigDirectory(cert_dir);

        /* Been there done that */
        prior++;

        PK11_SetPasswordFunc(SECU_GetModulePassword);

        /* Must login to FIPS before you do anything else */
        if (PK11_IsFIPS()) {
            slotinfo = PK11_GetInternalSlot();
            if (!slotinfo) {
                fprintf(stderr, "%s: Unable to get PKCS #11 Internal Slot."
                                "\n",
                        PROGRAM_NAME);
                return -1;
            }
            if (PK11_Authenticate(slotinfo, PR_FALSE /*loadCerts*/,
                                  &pwdata) != SECSuccess) {
                fprintf(stderr, "%s: Unable to authenticate to %s.\n",
                        PROGRAM_NAME, PK11_GetSlotName(slotinfo));
                PK11_FreeSlot(slotinfo);
                return -1;
            }
            PK11_FreeSlot(slotinfo);
        }

        /* Make sure there is a password set on the internal key slot */
        slotinfo = PK11_GetInternalKeySlot();
        if (!slotinfo) {
            fprintf(stderr, "%s: Unable to get PKCS #11 Internal Key Slot."
                            "\n",
                    PROGRAM_NAME);
            return -1;
        }
        if (PK11_NeedUserInit(slotinfo)) {
            PR_fprintf(errorFD,
                       "\nWARNING: No password set on internal key database.  Most operations will fail."
                       "\nYou must create a password.\n");
            warningCount++;
        }

        /* Make sure we can authenticate to the key slot in FIPS mode */
        if (PK11_IsFIPS()) {
            if (PK11_Authenticate(slotinfo, PR_FALSE /*loadCerts*/,
                                  &pwdata) != SECSuccess) {
                fprintf(stderr, "%s: Unable to authenticate to %s.\n",
                        PROGRAM_NAME, PK11_GetSlotName(slotinfo));
                PK11_FreeSlot(slotinfo);
                return -1;
            }
        }
        PK11_FreeSlot(slotinfo);
    }

    return 0;
}

/* Windows foolishness is now in the secutil lib */

/*****************************************************************
 *  g e t _ d e f a u l t _ c e r t _ d i r
 *
 *  Attempt to locate a certificate directory.
 *  Failing that, complain that the user needs to
 *  use the -d(irectory) parameter.
 *
 */
char *
get_default_cert_dir(void)
{
    char *home;

    char *cd = NULL;
    static char db[FNSIZE];

#ifdef XP_UNIX
    home = PR_GetEnvSecure("HOME");

    if (home && *home) {
        sprintf(db, "%s/.netscape", home);
        cd = db;
    }
#endif

#ifdef XP_PC
    FILE *fp;

    /* first check the environment override */

    home = PR_GetEnvSecure("JAR_HOME");

    if (home && *home) {
        sprintf(db, "%s/cert7.db", home);

        if ((fp = fopen(db, "r")) != NULL) {
            fclose(fp);
            cd = home;
        }
    }

    /* try the old navigator directory */

    if (cd == NULL) {
        home = "c:/Program Files/Netscape/Navigator";

        sprintf(db, "%s/cert7.db", home);

        if ((fp = fopen(db, "r")) != NULL) {
            fclose(fp);
            cd = home;
        }
    }

    /* Try the current directory, I wonder if this
     is really a good idea. Remember, Windows only.. */

    if (cd == NULL) {
        home = ".";

        sprintf(db, "%s/cert7.db", home);

        if ((fp = fopen(db, "r")) != NULL) {
            fclose(fp);
            cd = home;
        }
    }

#endif

    if (!cd) {
        PR_fprintf(errorFD,
                   "You must specify the location of your certificate directory\n");
        PR_fprintf(errorFD,
                   "with the -d option. Example: -d ~/.netscape in many cases with Unix.\n");
        errorCount++;
        exit(ERRX);
    }

    return cd;
}

/************************************************************************
 * g i v e _ h e l p
 */
void
give_help(int status)
{
    if (status == SEC_ERROR_UNKNOWN_ISSUER) {
        PR_fprintf(errorFD,
                   "The Certificate Authority (CA) for this certificate\n");
        PR_fprintf(errorFD,
                   "does not appear to be in your database. You should contact\n");
        PR_fprintf(errorFD,
                   "the organization which issued this certificate to obtain\n");
        PR_fprintf(errorFD, "a copy of its CA Certificate.\n");
    }
}

/**************************************************************************
 *
 * p r _ f g e t s
 *
 * fgets implemented with NSPR.
 */
char *
pr_fgets(char *buf, int size, PRFileDesc *file)
{
    int i;
    int status;
    char c;

    i = 0;
    while (i < size - 1) {
        status = PR_Read(file, &c, 1);
        if (status == -1) {
            return NULL;
        } else if (status == 0) {
            if (i == 0) {
                return NULL;
            }
            break;
        }
        buf[i++] = c;
        if (c == '\n') {
            break;
        }
    }
    buf[i] = '\0';

    return buf;
}
