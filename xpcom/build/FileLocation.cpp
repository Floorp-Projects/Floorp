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
 * The Original Code is Mozilla code
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mike Hommey <mh@glandium.org>
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

#include "FileLocation.h"
#include "nsZipArchive.h"
#include "nsURLHelper.h"

namespace mozilla {

FileLocation::FileLocation(const FileLocation &file, const char *path)
{
  if (file.IsZip()) {
    if (file.mBaseFile) {
      Init(file.mBaseFile, file.mPath.get());
    } else {
      Init(file.mBaseZip, file.mPath.get());
    }
    if (path) {
      PRInt32 i = mPath.RFindChar('/');
      if (kNotFound == i) {
        mPath.Truncate(0);
      } else {
        mPath.Truncate(i + 1);
      }
      mPath += path;
    }
  } else {
    if (path) {
      nsCOMPtr<nsIFile> cfile;
      file.mBaseFile->GetParent(getter_AddRefs(cfile));
      nsCOMPtr<nsILocalFile> clfile = do_QueryInterface(cfile);

#if defined(XP_WIN) || defined(XP_OS2)
      nsCAutoString pathStr(path);
      char *p;
      PRUint32 len = pathStr.GetMutableData(&p);
      for (; len; ++p, --len) {
        if ('/' == *p) {
            *p = '\\';
        }
      }
      clfile->AppendRelativeNativePath(pathStr);
#else
      clfile->AppendRelativeNativePath(nsDependentCString(path));
#endif
      Init(clfile);
    } else {
      Init(file.mBaseFile);
    }
  }
}

void
FileLocation::GetURIString(nsACString &result) const
{
  if (mBaseFile) {
    net_GetURLSpecFromActualFile(mBaseFile, result);
  } else if (mBaseZip) {
    nsRefPtr<nsZipHandle> handler = mBaseZip->GetFD();
    handler->mFile.GetURIString(result);
  }
  if (IsZip()) {
    result.Insert("jar:", 0);
    result += "!/";
    result += mPath;
  }
}

already_AddRefed<nsILocalFile>
FileLocation::GetBaseFile()
{
  if (IsZip() && mBaseZip) {
    nsRefPtr<nsZipHandle> handler = mBaseZip->GetFD();
    if (handler)
      return handler->mFile.GetBaseFile();
    return NULL;
  }

  nsCOMPtr<nsILocalFile> file = mBaseFile;
  return file.forget();
}

bool
FileLocation::Equals(const FileLocation &file) const
{
  if (mPath != file.mPath)
    return false;

  if (mBaseFile && file.mBaseFile) {
    bool eq;
    return NS_SUCCEEDED(mBaseFile->Equals(file.mBaseFile, &eq)) && eq;
  }

  const FileLocation *a = this, *b = &file;
  if (a->mBaseZip) {
    nsRefPtr<nsZipHandle> handler = a->mBaseZip->GetFD();
    a = &handler->mFile;
  }
  if (b->mBaseZip) {
    nsRefPtr<nsZipHandle> handler = b->mBaseZip->GetFD();
    b = &handler->mFile;
  }
  return a->Equals(*b);
}

nsresult
FileLocation::GetData(Data &data)
{
  if (!IsZip()) {
    return mBaseFile->OpenNSPRFileDesc(PR_RDONLY, 0444, &data.mFd.rwget());
  }
  data.mZip = mBaseZip;
  if (!data.mZip) {
    data.mZip = new nsZipArchive();
    data.mZip->OpenArchive(mBaseFile);
  }
  data.mItem = data.mZip->GetItem(mPath.get());
  if (data.mItem)
    return NS_OK;
  return NS_ERROR_FILE_UNRECOGNIZED_PATH;
}

nsresult
FileLocation::Data::GetSize(PRUint32 *result)
{
  if (mFd) {
    PRFileInfo64 fileInfo;
    if (PR_SUCCESS != PR_GetOpenFileInfo64(mFd, &fileInfo))
      return NS_ErrorAccordingToNSPR();

    if (fileInfo.size > PRInt64(PR_UINT32_MAX))
      return NS_ERROR_FILE_TOO_BIG;

    *result = fileInfo.size;
    return NS_OK;
  } else if (mItem) {
    *result = mItem->RealSize();
    return NS_OK;
  }
  return NS_ERROR_NOT_INITIALIZED;
}

nsresult
FileLocation::Data::Copy(char *buf, PRUint32 len)
{
  if (mFd) {
    for (PRUint32 totalRead = 0; totalRead < len; ) {
      PRInt32 read = PR_Read(mFd, buf + totalRead, NS_MIN(len - totalRead, PRUint32(PR_INT32_MAX)));
      if (read < 0)
        return NS_ErrorAccordingToNSPR();
      totalRead += read;
    }
    return NS_OK;
  } else if (mItem) {
    nsZipCursor cursor(mItem, mZip, reinterpret_cast<PRUint8 *>(buf), len, true);
    PRUint32 readLen;
    cursor.Copy(&readLen);
    return (readLen == len) ? NS_OK : NS_ERROR_FILE_CORRUPTED;
  }
  return NS_ERROR_NOT_INITIALIZED;
}

} /* namespace mozilla */
