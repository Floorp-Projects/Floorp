// Windows/DLL.h

#ifndef __WINDOWS_DLL_H
#define __WINDOWS_DLL_H

#include "../Common/MyString.h"

namespace NWindows {
namespace NDLL {

#ifdef UNDER_CE
#define My_GetProcAddress(module, proceName) GetProcAddressA(module, proceName)
#else
#define My_GetProcAddress(module, proceName) ::GetProcAddress(module, proceName)
#endif
 
class CLibrary
{
  bool LoadOperations(HMODULE newModule);
protected:
  HMODULE _module;
public:
  CLibrary(): _module(NULL) {};
  ~CLibrary() { Free(); }

  operator HMODULE() const { return _module; }
  HMODULE* operator&() { return &_module; }
  bool IsLoaded() const { return (_module != NULL); };

  void Attach(HMODULE m)
  {
    Free();
    _module = m;
  }
  HMODULE Detach()
  {
    HMODULE m = _module;
    _module = NULL;
    return m;
  }

  bool Free();
  bool LoadEx(LPCTSTR fileName, DWORD flags = LOAD_LIBRARY_AS_DATAFILE);
  bool Load(LPCTSTR fileName);
  #ifndef _UNICODE
  bool LoadEx(LPCWSTR fileName, DWORD flags = LOAD_LIBRARY_AS_DATAFILE);
  bool Load(LPCWSTR fileName);
  #endif
  FARPROC GetProc(LPCSTR procName) const { return My_GetProcAddress(_module, procName); }
};

bool MyGetModuleFileName(HMODULE hModule, CSysString &result);
#ifndef _UNICODE
bool MyGetModuleFileName(HMODULE hModule, UString &result);
#endif

}}

#endif
