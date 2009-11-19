// WorkDir.cpp

#include "StdAfx.h"

#include "Common/StringConvert.h"
#include "Common/Wildcard.h"

#include "Windows/FileDir.h"
#include "Windows/FileName.h"

#include "WorkDir.h"

using namespace NWindows;
using namespace NFile;

UString GetWorkDir(const NWorkDir::CInfo &workDirInfo, const UString &path)
{
  NWorkDir::NMode::EEnum mode = workDirInfo.Mode;
  #ifndef UNDER_CE
  if (workDirInfo.ForRemovableOnly)
  {
    mode = NWorkDir::NMode::kCurrent;
    UString prefix = path.Left(3);
    if (prefix[1] == L':' && prefix[2] == L'\\')
    {
      UINT driveType = GetDriveType(GetSystemString(prefix, ::AreFileApisANSI() ? CP_ACP : CP_OEMCP));
      if (driveType == DRIVE_CDROM || driveType == DRIVE_REMOVABLE)
        mode = workDirInfo.Mode;
    }
    /*
    CParsedPath parsedPath;
    parsedPath.ParsePath(archiveName);
    UINT driveType = GetDriveType(parsedPath.Prefix);
    if ((driveType != DRIVE_CDROM) && (driveType != DRIVE_REMOVABLE))
      mode = NZipSettings::NWorkDir::NMode::kCurrent;
    */
  }
  #endif
  switch(mode)
  {
    case NWorkDir::NMode::kCurrent:
    {
      return ExtractDirPrefixFromPath(path);
    }
    case NWorkDir::NMode::kSpecified:
    {
      UString tempDir = workDirInfo.Path;
      NName::NormalizeDirPathPrefix(tempDir);
      return tempDir;
    }
    default:
    {
      UString tempDir;
      if (!NDirectory::MyGetTempPath(tempDir))
        throw 141717;
      return tempDir;
    }
  }
}
