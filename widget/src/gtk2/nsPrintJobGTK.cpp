/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ex: set tabstop=8 softtabstop=4 shiftwidth=4 expandtab: */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Ken Herron <kherron@fastmail.us>.
 * Portions created by the Initial Developer are Copyright (C) 2004
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


#include "nscore.h"
#include "nsIDeviceContext.h"   // NS_ERROR_GFX_*
#include "nsIDeviceContextSpec.h"
#include "nsDeviceContextSpecG.h"

#include "nsPrintJobGTK.h"
#include "nsPSPrinters.h"
#include "nsReadableUtils.h"
#include "nsDirectoryServiceDefs.h"
#include "nsIFileStreams.h"

#include "prenv.h"
#include "prinit.h"
#include "prlock.h"
#include "prprf.h"

#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>


/* Routines to set environment variables. These are defined toward
 * the end of this file.
 */
static PRStatus EnvLock();
static PRStatus EnvSetPrinter(nsCString&);
static void EnvClear();


/* Interface class. It implements a spoolfile getter and destructor
 * so that none of the subclasses have to.
 */
nsIPrintJobGTK::~nsIPrintJobGTK()
{
    if (mSpoolFile)
        mSpoolFile->Remove(PR_FALSE);
}

nsresult
nsIPrintJobGTK::GetSpoolFile(nsILocalFile **aFile)
{
    if (!mSpoolFile)
        return NS_ERROR_NOT_INITIALIZED;
    *aFile = mSpoolFile;
    NS_ADDREF(*aFile);
    return NS_OK;
}

/**** nsPrintJobPreviewGTK - Stub class for print preview ****/

/* nsDeviceContextSpecGTK needs a spool file even for print preview, so this
 * class includes code to create one. Some of the other print job classes
 * inherit from this one to reuse the spoolfile initializer.
 */

nsresult
nsPrintJobPreviewGTK::InitSpoolFile(PRUint32 aPermissions)
{
    nsCOMPtr<nsIFile> spoolFile;
    nsresult rv = NS_GetSpecialDirectory(NS_OS_TEMP_DIR,
                                         getter_AddRefs(spoolFile));
    NS_ENSURE_SUCCESS(rv, NS_ERROR_GFX_PRINTER_COULD_NOT_OPEN_FILE);

    spoolFile->AppendNative(NS_LITERAL_CSTRING("tmp.prn"));
    
    rv = spoolFile->CreateUnique(nsIFile::NORMAL_FILE_TYPE, aPermissions);
    if (NS_FAILED(rv))
        return NS_ERROR_GFX_PRINTER_COULD_NOT_OPEN_FILE;
    mSpoolFile = do_QueryInterface(spoolFile, &rv);
    if (NS_FAILED(rv))
        spoolFile->Remove(PR_FALSE);

    return rv;
}

nsresult
nsPrintJobPreviewGTK::Init(nsDeviceContextSpecGTK *aSpec)
{
#ifdef DEBUG
    PRBool isPreview;
    aSpec->GetIsPrintPreview(isPreview);
    NS_PRECONDITION(isPreview, "This print job is to a printer");
#endif
    return InitSpoolFile(0600);
}

/**** nsPrintJobFileGTK - Print-to-file support ****/

/**
 * Initialize the print-to-file object from the printing spec.
 * See nsPrintJobGTK.h for details.
 */
nsresult
nsPrintJobFileGTK::Init(nsDeviceContextSpecGTK *aSpec)
{
    NS_PRECONDITION(aSpec, "aSpec must not be NULL");
#ifdef DEBUG
    PRBool toPrinter;
    aSpec->GetToPrinter(toPrinter);
    NS_PRECONDITION(!toPrinter, "This print job is to a printer");
#endif

    // The final output file will inherit the permissions of the temporary.
    nsresult rv = InitSpoolFile(0666);
    if (NS_SUCCEEDED(rv)) {
        const char *path;
        aSpec->GetPath(&path);
        rv = NS_NewNativeLocalFile(nsDependentCString(path), PR_FALSE,
                                   getter_AddRefs(mDestFile));
    }
    return rv;
}

nsresult
nsPrintJobFileGTK::Submit()
{
    // Move the spool file to the destination
    nsAutoString destLeafName;
    nsresult rv = mDestFile->GetLeafName(destLeafName);
    if (NS_SUCCEEDED(rv)) {
        nsCOMPtr<nsIFile> mDestDir;
        rv = mDestFile->GetParent(getter_AddRefs(mDestDir));
        if (NS_SUCCEEDED(rv)) {
            rv = mSpoolFile->MoveTo(mDestDir, destLeafName);
        }
    }
    return rv;
}

/**** Print-to-Pipe for unix and unix-like systems ****/

/* This launches a command using popen(); the print job is then written
 * to the pipe.
 */

/**
 * Initialize a print-to-pipe object.
 * See nsIPrintJobGTK.h and nsPrintJobGTK.h for details.
 */
nsresult
nsPrintJobPipeGTK::Init(nsDeviceContextSpecGTK *aSpec)
{
    NS_PRECONDITION(aSpec, "argument must not be NULL");
#ifdef DEBUG
    PRBool toPrinter;
    aSpec->GetToPrinter(toPrinter);
    NS_PRECONDITION(toPrinter, "Wrong class for this print job");
#endif

    /* Spool file */
    nsresult rv = InitSpoolFile(0600);
    if (NS_FAILED(rv))
        return rv;

    /* Print command */
    const char *command;
    aSpec->GetCommand(&command);
    mCommand = command;

    /* Printer name */
    const char *printerName;
    aSpec->GetPrinterName(&printerName);
    if (printerName) {
        const char *slash = strchr(printerName, '/');
        if (slash)
            printerName = slash + 1;
        if (0 != strcmp(printerName, "default"))
            mPrinterName = printerName;
    }
    return NS_OK;
}


/* Helper for nsPrintJobPipeGTK::Submit(). Start the print command. */
static nsresult
popenPrintCommand(FILE **aPipe, nsCString &aPrinter, nsCString &aCommand)
{
    // Set up the environment for the print command
    if (PR_SUCCESS != EnvLock())
        return NS_ERROR_OUT_OF_MEMORY;  // Couldn't allocate the object?

    if (!aPrinter.IsEmpty())
        EnvSetPrinter(aPrinter);

    // Start the print command
    *aPipe = popen(aCommand.get(), "w");
    EnvClear();
    return (*aPipe) ? NS_OK : NS_ERROR_GFX_PRINTER_CMD_FAILURE;
}

/* Helper for nsPrintJobPipeGTK::Submit(). Copy data from a temporary file
 * to the command pipe.
 */
static nsresult
CopySpoolToCommand(nsIFileInputStream *aSource, FILE *aDest)
{
    nsresult rv;
    PRUint32 count;
    do {
        char buf[256];
        count = 0;
        rv = aSource->Read(buf, sizeof buf, &count);
        fwrite(buf, 1, count, aDest);
    } while (NS_SUCCEEDED(rv) && count);
    return rv;
}
        
/**
 * Launch the print command using popen(), then copy the print job data
 * to the pipe. See nsIPrintJobGTK.h and nsPrintJobGTK.h for details.
 */

nsresult
nsPrintJobPipeGTK::Submit()
{
    NS_PRECONDITION(mSpoolFile, "No spool file");

    // Open the spool file
    nsCOMPtr<nsIFileInputStream> spoolStream =
        do_CreateInstance("@mozilla.org/network/file-input-stream;1");
    if (!spoolStream)
        return NS_ERROR_OUT_OF_MEMORY;
    nsresult rv = spoolStream->Init(mSpoolFile, -1, -1,
        nsIFileInputStream::DELETE_ON_CLOSE|nsIFileInputStream::CLOSE_ON_EOF);
    NS_ENSURE_SUCCESS(rv, rv);
    
    // Start the print command
    FILE *destPipe = NULL;
    rv = popenPrintCommand(&destPipe, mPrinterName, mCommand);
    if (NS_SUCCEEDED(rv)) {
        rv = CopySpoolToCommand(spoolStream, destPipe);
        int presult = pclose(destPipe);
        if (NS_SUCCEEDED(rv)) {
            if (!WIFEXITED(presult) || (EXIT_SUCCESS != WEXITSTATUS(presult)))
                rv = NS_ERROR_GFX_PRINTER_CMD_FAILURE;
        }
    }            
    spoolStream->Close();
    return rv;
}

/**** Print via CUPS ****/

/* nsPrintJobCUPS doesn't inherit its spoolfile code from the preview
 * class. CupsPrintFile() needs the filename as an 8-bit string, but an
 * nsILocalFile doesn't portably expose the filename in this format.
 * So it's necessary to construct the spoolfile name outside of the 
 * nsILocalFile object, and retain it as a string for use when calling
 * CupsPrintFile().
 */

/**
 * Initialize a print-to-CUPS object.
 * See nsIPrintJobGTK.h and nsPrintJobGTK.h for details.
 */
nsresult
nsPrintJobCUPS::Init(nsDeviceContextSpecGTK *aSpec)
{
    NS_PRECONDITION(aSpec, "argument must not be NULL");
#ifdef DEBUG
    PRBool toPrinter;
    aSpec->GetToPrinter(toPrinter);
    NS_PRECONDITION(toPrinter, "Wrong class for this print job");
#endif

    NS_ENSURE_TRUE(mCups.Init(), NS_ERROR_NOT_INITIALIZED);

    /* Printer name */
    const char *printerName = nsnull;
    aSpec->GetPrinterName(&printerName);
    NS_ENSURE_TRUE(printerName, NS_ERROR_GFX_PRINTER_NAME_NOT_FOUND);

    const char *slash = strchr(printerName, '/');
    mPrinterName = slash ? slash + 1 : printerName;
    mJobTitle.SetIsVoid(PR_TRUE);
    
    /* Spool file */
    int fd;
    char buf[FILENAME_MAX];

    fd = (mCups.mCupsTempFd)(buf, sizeof buf);
    // The CUPS manual doesn't describe what cupsTempFd() returns to
    // indicate failure. -1 is a likely value.
    NS_ENSURE_TRUE(fd > 0, NS_ERROR_GFX_PRINTER_COULD_NOT_OPEN_FILE);
    close(fd);
    
    nsresult rv = NS_NewNativeLocalFile(nsDependentCString(buf), PR_FALSE,
                                        getter_AddRefs(mSpoolFile));
    if (NS_FAILED(rv)) {
        unlink(buf);
        return NS_ERROR_GFX_PRINTER_COULD_NOT_OPEN_FILE;
    }
    mSpoolName = buf;
    return NS_OK;
}

nsresult
nsPrintJobCUPS::SetNumCopies(int aNumCopies)
{
    mNumCopies.Truncate();
    if (aNumCopies > 1)
        mNumCopies.AppendInt(aNumCopies);
    return NS_OK;
}


/* According to the cups.development forum, only plain ASCII may be
 * reliably used for CUPS print job titles. See
 * <http://www.cups.org/newsgroups.php?s523+gcups.development+v530+T0>.
 */
void
nsPrintJobCUPS::SetJobTitle(const PRUnichar *aTitle)
{
    if (aTitle) {
        LossyCopyUTF16toASCII(aTitle, mJobTitle);
    }
}

nsresult
nsPrintJobCUPS::Submit()
{
    NS_ENSURE_TRUE(mCups.IsInitialized(), NS_ERROR_NOT_INITIALIZED);
    NS_PRECONDITION(!mSpoolName.IsEmpty(), "No spool file");

    nsCStringArray printer(3);
    printer.ParseString(mPrinterName.get(),"/");

    cups_dest_t *dests, *dest;
    int num_dests = (mCups.mCupsGetDests)(&dests);
    
    if (printer.Count() == 1) {
        dest = (mCups.mCupsGetDest)(printer.CStringAt(0)->get(), NULL, num_dests, dests);
    } else {
        dest = (mCups.mCupsGetDest)(printer.CStringAt(0)->get(), 
                                    printer.CStringAt(1)->get(), num_dests, dests);
    }

    // Setting result just to get rid of compilation warning
    int result=0;
    if (dest != NULL) {
        if (!mNumCopies.IsEmpty())
            dest->num_options = (mCups.mCupsAddOption)("copies",
                                                       mNumCopies.get(),
                                                       dest->num_options,
                                                       &dest->options);
        const char *title = mJobTitle.IsVoid() ?
            "Untitled Document" : mJobTitle.get();
        result = (mCups.mCupsPrintFile)(printer.CStringAt(0)->get(),
                                            mSpoolName.get(), title, 
                                            dest->num_options, dest->options);
    }
    (mCups.mCupsFreeDests)(num_dests, dests);

    // cupsPrintFile() result codes below 0x0300 indicate success.
    // Individual success codes are defined in the cups headers, but
    // we're not including those.
    if (dest == NULL)
        return NS_ERROR_GFX_PRINTER_NAME_NOT_FOUND;
    else
        return (result < 0x0300) ? NS_OK : NS_ERROR_GFX_PRINTER_CMD_FAILURE;
}

/* Routines to set the MOZ_PRINTER_NAME environment variable and to
 * single-thread print jobs while the variable is set.
 */

static PRLock *EnvLockObj;
static PRCallOnceType EnvLockOnce;

/* EnvLock callback function */
static PRStatus
EnvLockInit()
{
    EnvLockObj = PR_NewLock();
    return EnvLockObj ? PR_SUCCESS : PR_FAILURE;
}


/**
 * Get the lock for setting printing-related environment variables and
 * running print commands.
 * @return PR_SUCCESS on success
 *         PR_FAILURE if the lock object could not be initialized.
 *                 
 */
static PRStatus
EnvLock()
{
    if (PR_FAILURE == PR_CallOnce(&EnvLockOnce, EnvLockInit))
        return PR_FAILURE;
    PR_Lock(EnvLockObj);
    return PR_SUCCESS;
}


static char *EnvPrinterString;
static const char EnvPrinterName[] = { "MOZ_PRINTER_NAME" };


/**
 * Set MOZ_PRINTER_NAME to the specified string.
 * @param aPrinter The value for MOZ_PRINTER_NAME. May be an empty string.
 * @return PR_SUCCESS on success.
 *         PR_FAILURE if memory could not be allocated.
 */
static PRStatus
EnvSetPrinter(nsCString& aPrinter)
{
    /* Construct the new environment string */
    char *newVar = PR_smprintf("%s=%s", EnvPrinterName, aPrinter.get());
    if (!newVar)
        return PR_FAILURE;

    /* Add it to the environment and dispose of any old string */
    PR_SetEnv(newVar);
    if (EnvPrinterString)
        PR_smprintf_free(EnvPrinterString);
    EnvPrinterString = newVar;

    return PR_SUCCESS;
}


/**
 * Clear the printer environment variable and release the environment lock.
 */
static void
EnvClear()
{
    if (EnvPrinterString) {
        /* On some systems, setenv("FOO") will remove FOO
         * from the environment.
         */
        PR_SetEnv(EnvPrinterName);
        if (!PR_GetEnv(EnvPrinterName)) {
            /* It must have worked */
            PR_smprintf_free(EnvPrinterString);
            EnvPrinterString = nsnull;
        }
    }
    PR_Unlock(EnvLockObj);
}
