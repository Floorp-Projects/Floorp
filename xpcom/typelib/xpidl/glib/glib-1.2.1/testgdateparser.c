
#include "glib.h"

#include <stdio.h>
#include <string.h>
#include <locale.h>

void g_date_debug_print(GDate* d)
{
  if (!d) g_print("NULL!\n");
  else 
    g_print("julian: %u (%s) DMY: %u %u %u (%s)\n",
	    d->julian_days, 
	    d->julian ? "valid" : "invalid",
	    d->day,
	    d->month,
	    d->year,
	    d->dmy ? "valid" : "invalid");
  
  fflush(stdout);
}

/* These only work in the POSIX locale, maybe C too - 
 * type POSIX into the program to check them
 */
char* posix_tests [] = {
  "19981024",
  "981024",
  "October 1998",
  "October 98",
  "oCT 98",
  "10/24/98",
  "10 -- 24 -- 98",
  "10/24/1998",
  "October 24, 1998",
  NULL
};

int main(int argc, char** argv)
{
  GDate* d;
  gchar* loc;
  gchar input[1024];

  loc = setlocale(LC_ALL,"");
  if (loc) 
    g_print("\nLocale set to %s\n", loc);
  else 
    g_print("\nLocale unchanged\n");

  d = g_date_new();

  while (fgets(input, 1023, stdin))
    {
      if (input[0] == '\n') 
        {
          g_print("Enter a date to parse and press enter, or type `POSIX':\n");
          continue;
        }

      if (strcmp(input,"POSIX\n") == 0) 
        {
          char** s = posix_tests;
          while (*s) {
            g_date_set_parse(d, *s);
            
            g_print("POSIXy parse test `%s' ...", *s);

            if (!g_date_valid(d))
              {
                g_print(" failed.\n");
              }
            else 
              {
                gchar buf[256];
                
                g_date_strftime(buf,100," parsed `%x' (%B %d %Y)\n",
                                d);
                g_print("%s", buf);
              }

            ++s;
          }
        }
      else 
        {
          g_date_set_parse(d, input);
          
          if (!g_date_valid(d))
            {
              g_print("Parse failed.\n");
            }
          else 
            {
              gchar buf[256];
              
              g_date_strftime(buf,100,"Parsed: `%x' (%B %d %Y)\n",
                              d);
              g_print("%s", buf);
            }
        }
    }

  g_date_free(d);

  return 0;
}


