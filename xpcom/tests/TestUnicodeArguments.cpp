/**
 * On Windows, a Unicode argument is passed as UTF-16 using ShellExecuteExW.
 * On other platforms, it is passed as UTF-8
 */

static const int args_length = 4;
#if defined(XP_WIN) && defined(_MSC_VER)
#define _UNICODE
#include <tchar.h>
#include <stdio.h>

static const _TCHAR* expected_utf16[args_length] = {
  // Latin-1
  L"M\xF8z\xEEll\xE5",
  // Cyrillic
  L"\x41C\x43E\x437\x438\x43B\x43B\x430",
  // Bengali
  L"\x9AE\x9CB\x99C\x9BF\x9B2\x9BE",
  // Cuneiform
  L"\xD808\xDE2C\xD808\xDF63\xD808\xDDB7"
};

int wmain(int argc, _TCHAR* argv[]) {
  printf("argc = %d\n", argc);

  if (argc != args_length + 1)
    return -1;

  for (int i = 1; i < argc; ++i) {
    printf("expected[%d]: ", i - 1);
    for (size_t j = 0; j < _tcslen(expected_utf16[i - 1]); ++j) {
      printf("%x ", *(expected_utf16[i - 1] + j));
    }
    printf("\n");

    printf("argv[%d]: ", i);
    for (size_t j = 0; j < _tcslen(argv[i]); ++j) {
      printf("%x ", *(argv[i] + j));
    }
    printf("\n");

    if (_tcscmp(expected_utf16[i - 1], argv[i])) {
      return i;
    }
  }

  return 0;
}
#else
#include <string.h>
#include <stdio.h>

static const char* expected_utf8[args_length] = {
  // Latin-1
  "M\xC3\xB8z\xC3\xAEll\xC3\xA5",
  // Cyrillic
  "\xD0\x9C\xD0\xBE\xD0\xB7\xD0\xB8\xD0\xBB\xD0\xBB\xD0\xB0",
  // Bengali
  "\xE0\xA6\xAE\xE0\xA7\x8B\xE0\xA6\x9C\xE0\xA6\xBF\xE0\xA6\xB2\xE0\xA6\xBE",
  // Cuneiform
  "\xF0\x92\x88\xAC\xF0\x92\x8D\xA3\xF0\x92\x86\xB7"
};

int main(int argc, char* argv[]) {
  if (argc != args_length + 1)
    return -1;

  for (int i = 1; i < argc; ++i) {
    printf("argv[%d] = %s; expected = %s\n", i, argv[i], expected_utf8[i - 1]);
    if (strcmp(expected_utf8[i - 1], argv[i])) {
      return i;
    }
  }

  return 0;
}
#endif
