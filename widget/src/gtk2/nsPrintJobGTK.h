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

#ifndef nsPrintJobGTK_h__
#define nsPrintJobGTK_h__

#include "nsCUPSShim.h"
#include "nsDebug.h"
#include "nsIDeviceContext.h"   // for NS_ERROR_GFX_PRINTING_NOT_IMPLEMENTED
#include "nsILocalFile.h"
#include "nsIPrintJobGTK.h"
#include "nsString.h"
#include "nsDeviceContextSpecG.h"

/* Print job class for print preview operations. */

class nsPrintJobPreviewGTK : public nsIPrintJobGTK {
    public:
        /* see nsIPrintJobGTK.h. Print preview doesn't actually
         * implement printing.
         */
        virtual nsresult Submit()
            { return NS_ERROR_GFX_PRINTING_NOT_IMPLEMENTED; }

    protected:
        virtual nsresult Init(nsDeviceContextSpecGTK *);
        nsresult InitSpoolFile(PRUint32 aPermissions);
};


/* Print job class for print-to-file. */
class nsPrintJobFileGTK : public nsPrintJobPreviewGTK {
    public:
        virtual nsresult Submit();

    protected:
        virtual nsresult Init(nsDeviceContextSpecGTK *);
        nsCOMPtr<nsILocalFile> mDestFile;
};

/* This is the class for printing to a pipe. */
class nsPrintJobPipeGTK : public nsPrintJobPreviewGTK {
    public:
        virtual nsresult Submit();

    protected:
        virtual nsresult Init(nsDeviceContextSpecGTK *);

    private:
        nsCString mCommand;
        nsCString mPrinterName;
};


/* This class submits print jobs through CUPS. */
class nsPrintJobCUPS : public nsIPrintJobGTK {
    public:
        virtual nsresult Submit();
        virtual nsresult SetNumCopies(int aNumCopies);
        virtual void SetJobTitle(const PRUnichar *aTitle);

    protected:
        virtual nsresult Init(nsDeviceContextSpecGTK *);

    private:
        nsCUPSShim mCups;
        nsCString mPrinterName;
        nsCString mNumCopies;
        nsCString mJobTitle;        // IsVoid() if no title
        nsCString mSpoolName;
};

#endif /* nsPrintJobPS_h__ */
