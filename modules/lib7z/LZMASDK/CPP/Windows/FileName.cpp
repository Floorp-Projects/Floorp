// Windows/FileName.cpp

#include "StdAfx.h"

#include "Windows/FileName.h"
#include "Common/Wildcard.h"

namespace NWindows {
namespace NFile {
namespace NName {

void NormalizeDirPathPrefix(CSysString &dirPath)
{
  if (dirPath.IsEmpty())
    return;
  if (dirPath.ReverseFind(kDirDelimiter) != dirPath.Length() - 1)
    dirPath += kDirDelimiter;
}

#ifndef _UNICODE
void NormalizeDirPathPrefix(UString &dirPath)
{
  if (dirPath.IsEmpty())
    return;
  if (dirPath.ReverseFind(wchar_t(kDirDelimiter)) != dirPath.Length() - 1)
    dirPath += wchar_t(kDirDelimiter);
}
#endif

const wchar_t kExtensionDelimiter = L'.';

void SplitNameToPureNameAndExtension(const UString &fullName,
    UString &pureName, UString &extensionDelimiter, UString &extension)
{
  int index = fullName.ReverseFind(kExtensionDelimiter);
  if (index < 0)
  {
    pureName = fullName;
    extensionDelimiter.Empty();
    extension.Empty();
  }
  else
  {
    pureName = fullName.Left(index);
    extensionDelimiter = kExtensionDelimiter;
    extension = fullName.Mid(index + 1);
  }
}

}}}
