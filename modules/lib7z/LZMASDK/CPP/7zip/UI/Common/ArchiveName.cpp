// ArchiveName.cpp

#include "StdAfx.h"

#include "Windows/FileDir.h"
#include "Windows/FileFind.h"

#include "ExtractingFilePath.h"

using namespace NWindows;

static UString CreateArchiveName2(const UString &srcName, bool fromPrev, bool keepName)
{
  UString resultName = L"Archive";
  if (fromPrev)
  {
    UString dirPrefix;
    if (NFile::NDirectory::GetOnlyDirPrefix(srcName, dirPrefix))
    {
      if (dirPrefix.Length() > 0)
        if (dirPrefix[dirPrefix.Length() - 1] == WCHAR_PATH_SEPARATOR)
        {
          dirPrefix.Delete(dirPrefix.Length() - 1);
          NFile::NFind::CFileInfoW fileInfo;
          if (fileInfo.Find(dirPrefix))
            resultName = fileInfo.Name;
        }
    }
  }
  else
  {
    NFile::NFind::CFileInfoW fileInfo;
    if (!fileInfo.Find(srcName))
      // return resultName;
      return srcName;
    resultName = fileInfo.Name;
    if (!fileInfo.IsDir() && !keepName)
    {
      int dotPos = resultName.ReverseFind('.');
      if (dotPos > 0)
      {
        UString archiveName2 = resultName.Left(dotPos);
        if (archiveName2.ReverseFind('.') < 0)
          resultName = archiveName2;
      }
    }
  }
  return resultName;
}

UString CreateArchiveName(const UString &srcName, bool fromPrev, bool keepName)
{
  return GetCorrectFsPath(CreateArchiveName2(srcName, fromPrev, keepName));
}
