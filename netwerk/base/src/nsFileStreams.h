/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
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

#ifndef nsFileStreams_h__
#define nsFileStreams_h__

#include "nsIFileStreams.h"
#include "nsIFile.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsISeekableStream.h"
#include "nsILineInputStream.h"
#include "nsCOMPtr.h"
#include "prlog.h"
#include "prio.h"

struct nsLineBuffer;

////////////////////////////////////////////////////////////////////////////////

class nsFileStream : public nsISeekableStream
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISEEKABLESTREAM

    nsFileStream();
    virtual ~nsFileStream();

    nsresult Close();
    nsresult InitWithFileDescriptor(PRFileDesc* fd, nsISupports* parent);

protected:
    PRFileDesc*           mFD;
    nsCOMPtr<nsISupports> mParent; // strong reference to parent nsFileIO,
                                   // which ensures mFD remains valid.
    PRBool                mCloseFD;
};

////////////////////////////////////////////////////////////////////////////////

class nsFileInputStream : public nsFileStream,
                          public nsIFileInputStream,
                          public nsILineInputStream
{
public:
    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_NSIINPUTSTREAM
    NS_DECL_NSIFILEINPUTSTREAM
    NS_DECL_NSILINEINPUTSTREAM
    
    // Overrided from nsFileStream
    NS_IMETHOD Seek(PRInt32 aWhence, PRInt64 aOffset);

    nsFileInputStream() : nsFileStream() 
    {
        mLineBuffer = nsnull;
        mBehaviorFlags = 0;
    }
    virtual ~nsFileInputStream() 
    {
        Close();
    }

    static NS_METHOD
    Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);

protected:
    nsLineBuffer     *mLineBuffer;

    /**
     * The file being opened.  Only stored when DELETE_ON_CLOSE or
     * REOPEN_ON_REWIND are true.
     */
    nsCOMPtr<nsIFile> mFile;
    /**
     * The IO flags passed to Init() for the file open.
     * Only set for REOPEN_ON_REWIND.
     */
    PRInt32 mIOFlags;
    /**
     * The permissions passed to Init() for the file open.
     * Only set for REOPEN_ON_REWIND.
     */
    PRInt32 mPerm;
    /**
     * Flags describing our behavior.  See the IDL file for possible values.
     */
    PRInt32 mBehaviorFlags;

protected:
    /**
     * Internal, called to open a file.  Parameters are the same as their
     * Init() analogues.
     */
    nsresult Open(nsIFile* file, PRInt32 ioFlags, PRInt32 perm);
    /**
     * Reopen the file (for OPEN_ON_READ only!)
     */
    nsresult Reopen() { return Open(mFile, mIOFlags, mPerm); }
};

////////////////////////////////////////////////////////////////////////////////

class nsFileOutputStream : public nsFileStream,
                           public nsIFileOutputStream
{
public:
    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_NSIOUTPUTSTREAM
    NS_DECL_NSIFILEOUTPUTSTREAM

    nsFileOutputStream() : nsFileStream() {}
    virtual ~nsFileOutputStream() { nsFileOutputStream::Close(); }
    
    static NS_METHOD
    Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);
};

////////////////////////////////////////////////////////////////////////////////

class nsSafeFileOutputStream : public nsFileOutputStream,
                               public nsISafeFileOutputStream
{
public:
    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_NSISAFEFILEOUTPUTSTREAM

    nsSafeFileOutputStream() :
        mWriteResult(NS_OK) {}

    virtual ~nsSafeFileOutputStream() { nsSafeFileOutputStream::Close(); }

    NS_IMETHODIMP Close();
    NS_IMETHODIMP Write(const char *buf, PRUint32 count, PRUint32 *result);
    NS_IMETHODIMP Init(nsIFile* file, PRInt32 ioFlags, PRInt32 perm, PRInt32 behaviorFlags);

protected:
    nsCOMPtr<nsIFile>         mTargetFile;
    nsCOMPtr<nsIFile>         mTempFile;

    PRBool   mTargetFileExists;
    nsresult mWriteResult; // Internally set in Write()
};

////////////////////////////////////////////////////////////////////////////////

#endif // nsFileStreams_h__
