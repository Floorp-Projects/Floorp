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

#ifndef nsIPrintJobGTK_h__
#define nsIPrintJobGTK_h__

#include "nsCOMPtr.h"

class nsDeviceContextSpecGTK;
class nsILocalFile;

/*
 * This is an interface for a class that accepts and submits print jobs.
 *
 * Instances should be obtained through nsPrintJobFactoryGTK::CreatePrintJob().
 * After obtaining a print job object, the caller can retrieve the spool file
 * object associated with the print job and write the print job to that.
 * Once that is done, the caller may call Submit() to finalize and print
 * the job, or Cancel() to abort the job.
 */

class nsIPrintJobGTK
{
public:
    virtual ~nsIPrintJobGTK();

    /* Allow the print job factory to create instances */
    friend class nsPrintJobFactoryGTK;

    /**
     * Set the number of copies for this print job. Some printing systems
     * allow setting this out of band, instead of embedding it into the
     * postscript.
     * @param aNumCopies Number of copies requested. Values <= 1 are
     *                   interpreted as "do not specify a copy count to the
     *                   printing system" when possible, or else as
     *                   one copy.
     * @return NS_ERROR_NOT_IMPLEMENTED if this print job class doesn't
     *                   support the specific copy count requested.
     * @return NS_OK     The print job class will request the specified
     *                   number of copies when printing the job.
     */
    virtual nsresult SetNumCopies(int aNumCopies)
            { return NS_ERROR_NOT_IMPLEMENTED; }

    /**
     * Set the print job title. Some printing systems accept a job title
     * which is displayed on a banner page, in a print queue listing, etc.
     *
     * This must be called after Init() and before StartSubmission().
     * nsIPrintJobGTK provides a stub implementation because most classes
     * do not make use of this information.
     * 
     * @param aTitle  The job title.
     */
    virtual void SetJobTitle(const PRUnichar *aTitle) { }

    /**
     * Get the temporary file that the print job should be written to. The
     * caller may open this file for writing and write the text of the print
     * job to it.
     * 
     * The spool file is owned by the print job object, and will be removed
     * by the print job dtor. Each print job object has one spool file.
     * 
     * @return NS_ERROR_NOT_INITIALIZED if the object hasn't been initialized.
     *         NS_OK otherwise.
     * 
     * This is currently non-virtual for efficiency.
     */
    nsresult GetSpoolFile(nsILocalFile **aFile); 

    /**
     * Submit a finished print job. The caller may call this after
     * calling GetSpoolFile(), writing the text of the print job to the
     * spool file, and closing the spool file. The return value indicates
     * the overall success or failure of the print operation.
     * 
     * The caller must not try to access the spool file in any way after
     * calling this function.
     *
     * @return NS_ERROR_GFX_PRINTING_NOT_IMPLEMENTED if the print
     *               job object doesn't actually support printing (e.g.
     *               for print preview)
     *         NS_OK for success
     *         Other values indicate failure of the print operation.
     */
    virtual nsresult Submit() = 0;

protected:
    /**
     * Initialize an object from a device context spec. This must be
     * called before any of the public methods. Implementations must
     * initialize mSpoolFile declared below.
     * 
     * @param aContext The device context spec describing the
     *                 desired print job.
     * @return NS_OK or a suitable error value.
     */
    virtual nsresult Init(nsDeviceContextSpecGTK *aContext) = 0;
    
    /* The spool file */
    nsCOMPtr<nsILocalFile> mSpoolFile;
};


#endif /* nsIPrintJobGTK_h__ */
