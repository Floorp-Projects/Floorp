
#include "glib.h"

#include <stdio.h>
#include <string.h>
#include <locale.h>
#include <time.h>

gboolean failed = FALSE;
guint32 passed = 0;
guint32 notpassed = 0;

#define	TEST(m,cond)	G_STMT_START { failed = !(cond); \
if (failed) \
  { ++notpassed; \
    if (!m) \
      g_print ("\n(%s:%d) failed for: %s\n", __FILE__, __LINE__, ( # cond )); \
    else \
      g_print ("\n(%s:%d) failed for: %s: (%s)\n", __FILE__, __LINE__, ( # cond ), (gchar*)m); \
  } \
else \
  ++passed;    \
  if ((passed+notpassed) % 10000 == 0) g_print ("."); fflush (stdout); \
} G_STMT_END

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

int main(int argc, char** argv)
{
  GDate* d;
  guint32 j;
  GDateMonth m;
  GDateYear y, prev_y;
  GDateDay day;
  gchar buf[101];
  gchar* loc;

  /* Try to get all the leap year cases. */
  GDateYear check_years[] = { 
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
    11, 12, 13, 14, 98, 99, 100, 101, 102, 103, 397, 
    398, 399, 400, 401, 402, 403, 404, 405, 406,
    1598, 1599, 1600, 1601, 1602, 1650, 1651,
    1897, 1898, 1899, 1900, 1901, 1902, 1903, 
    1961, 1962, 1963, 1964, 1965, 1967,
    1968, 1969, 1970, 1971, 1972, 1973, 1974, 1975, 1976,
    1977, 1978, 1979, 1980, 1981, 1982, 1983, 1984, 1985, 
    1986, 1987, 1988, 1989, 1990, 1991, 1992, 1993, 1994, 
    1995, 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003, 
    2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012,
    3000, 3001, 3002, 3998, 3999, 4000, 4001, 4002, 4003
  };
                           
  guint n_check_years = sizeof(check_years)/sizeof(GDateYear);

  guint i = 0;
  gboolean discontinuity = FALSE;

  g_print("checking GDate...");
  
  TEST("sizeof(GDate) is not more than 8 bytes on this platform", sizeof(GDate) < 9);

  d = g_date_new();

  TEST("Empty constructor produces invalid date", !g_date_valid(d));

  g_date_free(d);

  d = g_date_new_dmy(1,1,1);

  TEST("January 1, Year 1 created and valid", g_date_valid(d));

  j = g_date_julian(d);
  
  TEST("January 1, Year 1 is Julian date 1", j == 1);

  TEST("Returned month is January", g_date_month(d) == G_DATE_JANUARY);
  TEST("Returned day is 1", g_date_day(d) == 1);
  TEST("Returned year is 1", g_date_year(d) == 1);

  TEST("Bad month is invalid", !g_date_valid_month(G_DATE_BAD_MONTH));
  TEST("Month 13 is invalid",  !g_date_valid_month(13));
  TEST("Bad day is invalid",   !g_date_valid_day(G_DATE_BAD_DAY));
  TEST("Day 32 is invalid",     !g_date_valid_day(32));
  TEST("Bad year is invalid",  !g_date_valid_year(G_DATE_BAD_YEAR));
  TEST("Bad julian is invalid", !g_date_valid_julian(G_DATE_BAD_JULIAN));
  TEST("Bad weekday is invalid", !g_date_valid_weekday(G_DATE_BAD_WEEKDAY));
  TEST("Year 2000 is a leap year", g_date_is_leap_year(2000));
  TEST("Year 1999 is not a leap year", !g_date_is_leap_year(1999));
  TEST("Year 1996 is a leap year", g_date_is_leap_year(1996));
  TEST("Year 1600 is a leap year", g_date_is_leap_year(1600));
  TEST("Year 2100 is not a leap year", !g_date_is_leap_year(2100));
  TEST("Year 1800 is not a leap year", !g_date_is_leap_year(1800));

  g_date_free(d);
  
  loc = setlocale(LC_ALL,"");
  if (loc) 
    g_print("\nLocale set to %s\n", loc);
  else 
    g_print("\nLocale unchanged\n");

  d = g_date_new();
  g_date_set_time(d, time(NULL));
  TEST("Today is valid", g_date_valid(d));

  g_date_strftime(buf,100,"Today is a %A, %x\n", d);
  g_print("%s", buf);

  g_date_set_time(d, 1);
  TEST("Beginning of Unix epoch is valid", g_date_valid(d));

  g_date_strftime(buf,100,"1 second into the Unix epoch it was a %A, in the month of %B, %x\n", d);
  g_print("%s", buf);

  g_date_set_julian(d, 1);
  TEST("GDate's \"Julian\" epoch's first day is valid", g_date_valid(d));

  g_date_strftime(buf,100,"Our \"Julian\" epoch begins on a %A, in the month of %B, %x\n",
		  d);
  g_print("%s", buf);

  g_date_set_dmy(d, 10, 1, 2000);

  g_date_strftime(buf,100,"%x", d);

  g_date_set_parse(d, buf);
  /* Note: this test will hopefully work, but no promises. */
  TEST("Successfully parsed a %x-formatted string", 
       g_date_valid(d) && 
       g_date_month(d) == 1 && 
       g_date_day(d) == 10 && 
       g_date_year(d) == 2000);
  if (failed)
    g_date_debug_print(d);
  
  g_date_free(d);

  j = G_DATE_BAD_JULIAN;

  i = 0;
  discontinuity = TRUE;
  y      = check_years[0];
  prev_y = G_DATE_BAD_YEAR;
  while (i < n_check_years) 
    {
      guint32 first_day_of_year = G_DATE_BAD_JULIAN;
      guint16 days_in_year = g_date_is_leap_year(y) ? 366 : 365;
      guint   sunday_week_of_year = 0;
      guint   sunday_weeks_in_year = g_date_sunday_weeks_in_year(y);
      guint   monday_week_of_year = 0;
      guint   monday_weeks_in_year = g_date_monday_weeks_in_year(y);

      if (discontinuity)
        g_print(" (Break in sequence of requested years to check)\n");

      g_print("Checking year %u", y);

      TEST("Year is valid", g_date_valid_year(y));

      TEST("Number of Sunday weeks in year is 52 or 53", 
	   sunday_weeks_in_year == 52 || sunday_weeks_in_year == 53);
      
      TEST("Number of Monday weeks in year is 52 or 53", 
	   monday_weeks_in_year == 52 || monday_weeks_in_year == 53);
	   
      m = 1;
      while (m < 13) 
	{
	  guint8 dim = g_date_days_in_month(m,y);
	  GDate days[31];         /* This is the fast way, no allocation */

	  TEST("Sensible number of days in month", (dim > 0 && dim < 32));

	  TEST("Month between 1 and 12 is valid", g_date_valid_month(m));

	  day = 1;

	  g_date_clear(days, 31);

	  while (day <= dim) 
	    {
	      guint i;
              GDate tmp;

	      TEST("DMY triplet is valid", g_date_valid_dmy(day,m,y));

	      /* Create GDate with triplet */
	      
	      d = &days[day-1];

	      TEST("Cleared day is invalid", !g_date_valid(d));

	      g_date_set_dmy(d,day,m,y);

	      TEST("Set day is valid", g_date_valid(d));

	      if (m == G_DATE_JANUARY && day == 1) 
		{
		  first_day_of_year = g_date_julian(d);
		}

	      g_assert(first_day_of_year != G_DATE_BAD_JULIAN);

	      TEST("Date with DMY triplet is valid", g_date_valid(d));
	      TEST("Month accessor works", g_date_month(d) == m);
	      TEST("Year accessor works", g_date_year(d) == y);
	      TEST("Day of month accessor works", g_date_day(d) == day);

	      TEST("Day of year is consistent with Julian dates",
		   ((g_date_julian(d) + 1 - first_day_of_year) ==
		    (g_date_day_of_year(d))));

	      if (failed) 
		{
		  g_print("first day: %u this day: %u day of year: %u\n", 
			  first_day_of_year, 
			  g_date_julian(d),
			  g_date_day_of_year(d));
		}
	      
	      if (m == G_DATE_DECEMBER && day == 31) 
		{
		  TEST("Last day of year equals number of days in year", 
		       g_date_day_of_year(d) == days_in_year);
		  if (failed) 
		    {
		      g_print("last day: %u days in year: %u\n", 
			      g_date_day_of_year(d), days_in_year);
		    }
		}

	      TEST("Day of year is not more than number of days in the year",
		   g_date_day_of_year(d) <= days_in_year);

	      TEST("Monday week of year is not more than number of weeks in the year",
		   g_date_monday_week_of_year(d) <= monday_weeks_in_year);
	      if (failed)
		{
		  g_print("Weeks in year: %u\n", monday_weeks_in_year);
		  g_date_debug_print(d);
		}
	      TEST("Monday week of year is >= than last week of year",
		   g_date_monday_week_of_year(d) >= monday_week_of_year);

	      if (g_date_weekday(d) == G_DATE_MONDAY) 
		{
		  
		  TEST("Monday week of year on Monday 1 more than previous day's week of year",
		       (g_date_monday_week_of_year(d) - monday_week_of_year) == 1);
		}
	      else 
		{
		  TEST("Monday week of year on non-Monday 0 more than previous day's week of year",
		       (g_date_monday_week_of_year(d) - monday_week_of_year) == 0);
		}


	      monday_week_of_year = g_date_monday_week_of_year(d);


	      TEST("Sunday week of year is not more than number of weeks in the year",
		   g_date_sunday_week_of_year(d) <= sunday_weeks_in_year);
	      if (failed)
		{
		  g_date_debug_print(d);
		}
	      TEST("Sunday week of year is >= than last week of year",
		   g_date_sunday_week_of_year(d) >= sunday_week_of_year);

	      if (g_date_weekday(d) == G_DATE_SUNDAY) 
		{
		  TEST("Sunday week of year on Sunday 1 more than previous day's week of year",
		       (g_date_sunday_week_of_year(d) - sunday_week_of_year) == 1);
		}
	      else 
		{
		  TEST("Sunday week of year on non-Sunday 0 more than previous day's week of year",
		       (g_date_sunday_week_of_year(d) - sunday_week_of_year) == 0);
		}

	      sunday_week_of_year = g_date_sunday_week_of_year(d);

	      TEST("Date is equal to itself",
		   g_date_compare(d,d) == 0);


	      /*************** Increments ***********/

              i = 1;
              while (i < 402) /* Need to get 400 year increments in */ 
                {
	      
                  /***** Days ******/
                  tmp = *d;
                  g_date_add_days(d, i);

                  TEST("Adding days gives a value greater than previous",
                       g_date_compare(d, &tmp) > 0);

                  g_date_subtract_days(d, i);
                  TEST("Forward days then backward days returns us to current day",
                       g_date_day(d) == day);

                  if (failed) 
                    {
                      g_print("  (increment %u, dmy %u %u %u) ", i, day, m, y);
                      g_date_debug_print(d);
                    }

                  TEST("Forward days then backward days returns us to current month",
                       g_date_month(d) == m);

                  if (failed) 
                    {
                      g_print("  (increment %u, dmy %u %u %u) ", i, day, m, y);
                      g_date_debug_print(d);
                    }

                  TEST("Forward days then backward days returns us to current year",
                       g_date_year(d) == y);

                  if (failed) 
                    {
                      g_print("  (increment %u, dmy %u %u %u) ", i, day, m, y);
                      g_date_debug_print(d);
                    }

                  /******* Months ********/

                  tmp = *d;
                  g_date_add_months(d, i);
                  TEST("Adding months gives a larger value",
                       g_date_compare(d, &tmp) > 0);
                  g_date_subtract_months(d, i);

                  TEST("Forward months then backward months returns us to current month",
                       g_date_month(d) == m);

                  if (failed) 
                    {
                      g_print("  (increment %u, dmy %u %u %u) ", i, day, m, y);
                      g_date_debug_print(d);
                    }

                  TEST("Forward months then backward months returns us to current year",
                       g_date_year(d) == y);

                  if (failed) 
                    {
                      g_print("  (increment %u, dmy %u %u %u) ", i, day, m, y);
                      g_date_debug_print(d);
                    }

		  
                  if (day < 29) 
                    {
                      /* Day should be unchanged */
		      
                      TEST("Forward months then backward months returns us to current day",
                           g_date_day(d) == day);
		      
                      if (failed) 
                        {
                          g_print("  (increment %u, dmy %u %u %u) ", i, day, m, y);
                          g_date_debug_print(d);
                        }
                    }
                  else 
                    {
                      /* reset the day for later tests */
                      g_date_set_day(d, day);
                    }

                  /******* Years ********/

                  tmp = *d;
                  g_date_add_years(d, i);

                  TEST("Adding years gives a larger value",
                       g_date_compare(d,&tmp) > 0);
		      
                  g_date_subtract_years(d, i);

                  TEST("Forward years then backward years returns us to current month",
                       g_date_month(d) == m);

                  if (failed) 
                    {
                      g_print("  (increment %u, dmy %u %u %u) ", i, day, m, y);
                      g_date_debug_print(d);
                    }

                  TEST("Forward years then backward years returns us to current year",
                       g_date_year(d) == y);

                  if (failed) 
                    {
                      g_print("  (increment %u, dmy %u %u %u) ", i, day, m, y);
                      g_date_debug_print(d);
                    }

                  if (m != 2 && day != 29) 
                    {
                      TEST("Forward years then backward years returns us to current day",
                           g_date_day(d) == day);
		      
                      if (failed) 
                        {
                          g_print("  (increment %u, dmy %u %u %u) ", i, day, m, y);
                          g_date_debug_print(d);
                        }
                    }
                  else 
                    {
                      g_date_set_day(d, day); /* reset */
                    }

                  i += 10;
                }

	      /*****  increment test relative to our local Julian count */

              if (!discontinuity) {

                /* We can only run sequence tests between sequential years */
                
                TEST("Julians are sequential with increment 1",
                     j+1 == g_date_julian(d));
                if (failed) 
                  {
                    g_print("Out of sequence, prev: %u expected: %u got: %u\n",
                            j, j+1, g_date_julian(d));
                  }

                g_date_add_days(d,1);
                TEST("Next day has julian 1 higher",
                     g_date_julian(d) == j + 2);
                g_date_subtract_days(d, 1);
                
                if (j != G_DATE_BAD_JULIAN) 
                  {
                    g_date_subtract_days(d, 1);
                    
                    TEST("Previous day has julian 1 lower",
                         g_date_julian(d) == j);
                    
                    g_date_add_days(d, 1); /* back to original */
                  }
              }    
              discontinuity = FALSE; /* goes away now */            

              fflush(stdout);
              fflush(stderr);

	      j = g_date_julian(d); /* inc current julian */

	      ++day;
	    } 
	  ++m;
	}
      g_print(" done\n");
      ++i;
      prev_y = y;
      y = check_years[i];
      if (prev_y == G_DATE_BAD_YEAR || 
          (prev_y + 1) != y) discontinuity = TRUE;
    }
  
  
  g_print("\n%u tests passed, %u failed\n",passed, notpassed);

  return 0;
}


