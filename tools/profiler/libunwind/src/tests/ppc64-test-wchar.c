#include <wchar.h>
#include <stdio.h>
main ()
{
  wchar_t *wstring =
    L"Now is the time for all good men to come to the aid of their country";
  int i;
  int ret;

  printf("wcslen(wstring) = %d\n", wcslen(wstring));
  for (i = 0; i < wcslen (wstring); i++)
    {
      ret = printf ("%lc", wstring[i]);
      if (ret != 1) {
	printf("printf returned: %d\n", ret);
	perror("Linux says");
      }
    }
  printf("\n");
}
