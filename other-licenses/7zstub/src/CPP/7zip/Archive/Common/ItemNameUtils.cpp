// Archive/Common/ItemNameUtils.cpp

#include "StdAfx.h"

#include "ItemNameUtils.h"

namespace NArchive {
namespace NItemName {

static const wchar_t kOsPathSepar = WCHAR_PATH_SEPARATOR;
static const wchar_t kUnixPathSepar = L'/';

void ReplaceSlashes_OsToUnix
#if WCHAR_PATH_SEPARATOR != L'/'
  (UString &name)
  {
    name.Replace(kOsPathSepar, kUnixPathSepar);
  }
#else
  (UString &) {}
#endif


UString GetOsPath(const UString &name)
{
  #if WCHAR_PATH_SEPARATOR != L'/'
    UString newName = name;
    newName.Replace(kUnixPathSepar, kOsPathSepar);
    return newName;
  #else
    return name;
  #endif
}


UString GetOsPath_Remove_TailSlash(const UString &name)
{
  if (name.IsEmpty())
    return UString();
  UString newName = GetOsPath(name);
  if (newName.Back() == kOsPathSepar)
    newName.DeleteBack();
  return newName;
}


void ReplaceToOsSlashes_Remove_TailSlash(UString &name)
{
  if (!name.IsEmpty())
  {
    #if WCHAR_PATH_SEPARATOR != L'/'
      name.Replace(kUnixPathSepar, kOsPathSepar);
    #endif
    
    if (name.Back() == kOsPathSepar)
      name.DeleteBack();
  }
}


bool HasTailSlash(const AString &name, UINT
  #if defined(_WIN32) && !defined(UNDER_CE)
    codePage
  #endif
  )
{
  if (name.IsEmpty())
    return false;
  char c =
    #if defined(_WIN32) && !defined(UNDER_CE)
      *CharPrevExA((WORD)codePage, name, name.Ptr(name.Len()), 0);
    #else
      name.Back();
    #endif
  return (c == '/');
}


#ifndef _WIN32
UString WinPathToOsPath(const UString &name)
{
  UString newName = name;
  newName.Replace(L'\\', WCHAR_PATH_SEPARATOR);
  return newName;
}
#endif

}}
