/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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

#if defined(XP_WIN) || defined(XP_OS2)
      nsCAutoString pathStr(path);
      char *p;
      PRUint32 len = pathStr.GetMutableData(&p);
      for (; len; ++p, --len) {
        if ('/' == *p) {
            *p = '\\';
        }
      }
      cfile->AppendRelativeNativePath(pathStr);
#else
      cfile->AppendRelativeNativePath(nsDependentCString(path));
#endif
      Init(cfile);
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

already_AddRefed<nsIFile>
FileLocation::GetBaseFile()
{
  if (IsZip() && mBaseZip) {
    nsRefPtr<nsZipHandle> handler = mBaseZip->GetFD();
    if (handler)
      return handler->mFile.GetBaseFile();
    return NULL;
  }

  nsCOMPtr<nsIFile> file = mBaseFile;
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
