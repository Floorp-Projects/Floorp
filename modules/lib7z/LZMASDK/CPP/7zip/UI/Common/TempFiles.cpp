// TempFiles.cpp

#include "StdAfx.h"

#include "TempFiles.h"

#include "Windows/FileDir.h"
#include "Windows/FileIO.h"

using namespace NWindows;
using namespace NFile;

void CTempFiles::Clear()
{
  while(!Paths.IsEmpty())
  {
    NDirectory::DeleteFileAlways((LPCWSTR)Paths.Back());
    Paths.DeleteBack();
  }
}


