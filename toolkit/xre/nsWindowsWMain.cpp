// This file is a .cpp file meant to be included in nsBrowserApp.cpp and other
// similar bootstrap code. It converts wide-character windows wmain into UTF-8
// narrow-character strings.

#ifndef XP_WIN
#error This file only makes sense on Windows.
#endif

#include "nsUTF8Utils.h"
#include "nsSetDllDirectory.h"

#if defined(_MSC_VER) && defined(_M_IX86) && defined(XRE_WANT_DLL_BLOCKLIST)
#include "nsWindowsDllBlocklist.cpp"
#else
#undef XRE_WANT_DLL_BLOCKLIST
#endif


#ifdef __MINGW32__

/* MingW currently does not implement a wide version of the
   startup routines.  Workaround is to implement something like
   it ourselves.  See bug 411826 */

#include <shellapi.h>

int wmain(int argc, WCHAR **argv);

int main(int argc, char **argv)
{
  LPWSTR commandLine = GetCommandLineW();
  int argcw = 0;
  LPWSTR *argvw = CommandLineToArgvW(commandLine, &argcw);
  if (!argvw)
    return 127;

  int result = wmain(argcw, argvw);
  LocalFree(argvw);
  return result;
}
#endif /* __MINGW32__ */

#define main NS_internal_main

int main(int argc, char **argv);

static char*
AllocConvertUTF16toUTF8(const WCHAR *arg)
{
  // be generous... UTF16 units can expand up to 3 UTF8 units
  int len = wcslen(arg);
  char *s = new char[len * 3 + 1];
  if (!s)
    return NULL;

  ConvertUTF16toUTF8 convert(s);
  convert.write(arg, len);
  convert.write_terminator();
  return s;
}

static void
FreeAllocStrings(int argc, char **argv)
{
  while (argc) {
    --argc;
    delete [] argv[argc];
  }

  delete [] argv;
}

#ifdef WINCE
/** argc/argv are in/out parameters */
void ExtractEnvironmentFromCL(int &argc, char **&argv)
{
  for (int x = argc - 1; x >= 0; x--) {
    if (!strncmp(argv[x], "--environ:", 10)) {
      char* key_val = strdup(argv[x]+10);
      putenv(key_val);
      free(key_val);
      argc -= 1;
      char *delete_argv = argv[x];
      if (x < argc) /* if the current argument is not at the tail, shift following arguments. */
        memcpy(&argv[x], &argv[x+1], (argc - x) * sizeof(char*));
      delete [] delete_argv;
    }
  } 
}
#endif  

int wmain(int argc, WCHAR **argv)
{
#ifndef XRE_DONT_PROTECT_DLL_LOAD
  mozilla::NS_SetDllDirectory(L"");
#endif

#ifdef XRE_WANT_DLL_BLOCKLIST
  SetupDllBlocklist();
#endif
  
  char **argvConverted = new char*[argc + 1];
  if (!argvConverted)
    return 127;

  for (int i = 0; i < argc; ++i) {
    argvConverted[i] = AllocConvertUTF16toUTF8(argv[i]);
    if (!argvConverted[i]) {
      return 127;
    }
  }
#ifdef WINCE
  ExtractEnvironmentFromCL(argc, argvConverted);
#endif
  argvConverted[argc] = NULL;

  // need to save argvConverted copy for later deletion.
  char **deleteUs = new char*[argc+1];
  if (!deleteUs) {
    FreeAllocStrings(argc, argvConverted);
    return 127;
  }
  for (int i = 0; i < argc; i++)
    deleteUs[i] = argvConverted[i];
  int result = main(argc, argvConverted);

  delete[] argvConverted;
  FreeAllocStrings(argc, deleteUs);

  return result;
}
