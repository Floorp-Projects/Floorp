/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GLib Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GLib Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GLib at ftp://ftp.gtk.org/pub/gtk/. 
 */

/* 
 * MT safe
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "glib.h"

#include <time.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <locale.h>

GDate*
g_date_new ()
{
  GDate *d = g_new0 (GDate, 1); /* happily, 0 is the invalid flag for everything. */
  
  return d;
}

GDate*
g_date_new_dmy (GDateDay day, GDateMonth m, GDateYear y)
{
  GDate *d;
  g_return_val_if_fail (g_date_valid_dmy (day, m, y), NULL);
  
  d = g_new (GDate, 1);
  
  d->julian = FALSE;
  d->dmy    = TRUE;
  
  d->month = m;
  d->day   = day;
  d->year  = y;
  
  g_assert (g_date_valid (d));
  
  return d;
}

GDate*
g_date_new_julian (guint32 j)
{
  GDate *d;
  g_return_val_if_fail (g_date_valid_julian (j), NULL);
  
  d = g_new (GDate, 1);
  
  d->julian = TRUE;
  d->dmy    = FALSE;
  
  d->julian_days = j;
  
  g_assert (g_date_valid (d));
  
  return d;
}

void
g_date_free (GDate *d)
{
  g_return_if_fail (d != NULL);
  
  g_free (d);
}

gboolean     
g_date_valid (GDate       *d)
{
  g_return_val_if_fail (d != NULL, FALSE);
  
  return (d->julian || d->dmy);
}

static const guint8 days_in_months[2][13] = 
{  /* error, jan feb mar apr may jun jul aug sep oct nov dec */
  {  0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 }, 
  {  0, 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 } /* leap year */
};

static const guint16 days_in_year[2][14] = 
{  /* 0, jan feb mar apr may  jun  jul  aug  sep  oct  nov  dec */
  {  0, 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365 }, 
  {  0, 0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366 }
};

gboolean     
g_date_valid_month (GDateMonth   m)
{ 
  return ( (m > G_DATE_BAD_MONTH) && (m < 13) );
}

gboolean     
g_date_valid_year (GDateYear    y)
{
  return ( y > G_DATE_BAD_YEAR );
}

gboolean     
g_date_valid_day (GDateDay     d)
{
  return ( (d > G_DATE_BAD_DAY) && (d < 32) );
}

gboolean     
g_date_valid_weekday (GDateWeekday w)
{
  return ( (w > G_DATE_BAD_WEEKDAY) && (w < 8) );
}

gboolean     
g_date_valid_julian (guint32      j)
{
  return (j > G_DATE_BAD_JULIAN);
}

gboolean     
g_date_valid_dmy (GDateDay     d, 
                  GDateMonth   m, 
		  GDateYear    y)
{
  return ( (m > G_DATE_BAD_MONTH) &&
           (m < 13)               && 
           (d > G_DATE_BAD_DAY)   && 
           (y > G_DATE_BAD_YEAR)  &&   /* must check before using g_date_is_leap_year */
           (d <=  (g_date_is_leap_year (y) ? 
		   days_in_months[1][m] : days_in_months[0][m])) );
}


/* "Julian days" just means an absolute number of days, where Day 1 ==
 *   Jan 1, Year 1
 */
static void
g_date_update_julian (GDate *d)
{
  GDateYear year;
  gint index;
  
  g_return_if_fail (d != NULL);
  g_return_if_fail (d->dmy);
  g_return_if_fail (!d->julian);
  g_return_if_fail (g_date_valid_dmy (d->day, d->month, d->year));
  
  /* What we actually do is: multiply years * 365 days in the year,
   *  add the number of years divided by 4, subtract the number of
   *  years divided by 100 and add the number of years divided by 400,
   *  which accounts for leap year stuff. Code from Steffen Beyer's
   *  DateCalc. 
   */
  
  year = d->year - 1; /* we know d->year > 0 since it's valid */
  
  d->julian_days = year * 365U;
  d->julian_days += (year >>= 2); /* divide by 4 and add */
  d->julian_days -= (year /= 25); /* divides original # years by 100 */
  d->julian_days += year >> 2;    /* divides by 4, which divides original by 400 */
  
  index = g_date_is_leap_year (d->year) ? 1 : 0;
  
  d->julian_days += days_in_year[index][d->month] + d->day;
  
  g_return_if_fail (g_date_valid_julian (d->julian_days));
  
  d->julian = TRUE;
}

static void 
g_date_update_dmy (GDate *d)
{
  GDateYear y;
  GDateMonth m;
  GDateDay day;
  
  guint32 A, B, C, D, E, M;
  
  g_return_if_fail (d != NULL);
  g_return_if_fail (d->julian);
  g_return_if_fail (!d->dmy);
  g_return_if_fail (g_date_valid_julian (d->julian_days));
  
  /* Formula taken from the Calendar FAQ; the formula was for the
   *  Julian Period which starts on 1 January 4713 BC, so we add
   *  1,721,425 to the number of days before doing the formula.
   *
   * I'm sure this can be simplified for our 1 January 1 AD period
   * start, but I can't figure out how to unpack the formula.  
   */
  
  A = d->julian_days + 1721425 + 32045;
  B = ( 4 *(A + 36524) )/ 146097 - 1;
  C = A - (146097 * B)/4;
  D = ( 4 * (C + 365) ) / 1461 - 1;
  E = C - ((1461*D) / 4);
  M = (5 * (E - 1) + 2)/153;
  
  m = M + 3 - (12*(M/10));
  day = E - (153*M + 2)/5;
  y = 100 * B + D - 4800 + (M/10);
  
#ifdef G_ENABLE_DEBUG
  if (!g_date_valid_dmy (day, m, y)) 
    {
      g_warning ("\nOOPS julian: %u  computed dmy: %u %u %u\n", 
		 d->julian_days, day, m, y);
    }
#endif
  
  d->month = m;
  d->day   = day;
  d->year  = y;
  
  d->dmy = TRUE;
}

GDateWeekday 
g_date_weekday (GDate *d)
{
  g_return_val_if_fail (d != NULL, G_DATE_BAD_WEEKDAY);
  g_return_val_if_fail (g_date_valid (d), G_DATE_BAD_WEEKDAY);
  
  if (!d->julian) 
    {
      g_date_update_julian (d);
    }
  g_return_val_if_fail (d->julian, G_DATE_BAD_WEEKDAY);
  
  return ((d->julian_days - 1) % 7) + 1;
}

GDateMonth   
g_date_month (GDate *d)
{
  g_return_val_if_fail (d != NULL, G_DATE_BAD_MONTH);
  g_return_val_if_fail (g_date_valid (d), G_DATE_BAD_MONTH);
  
  if (!d->dmy) 
    {
      g_date_update_dmy (d);
    }
  g_return_val_if_fail (d->dmy, G_DATE_BAD_MONTH);
  
  return d->month;
}

GDateYear    
g_date_year (GDate *d)
{
  g_return_val_if_fail (d != NULL, G_DATE_BAD_YEAR);
  g_return_val_if_fail (g_date_valid (d), G_DATE_BAD_YEAR);
  
  if (!d->dmy) 
    {
      g_date_update_dmy (d);
    }
  g_return_val_if_fail (d->dmy, G_DATE_BAD_YEAR);  
  
  return d->year;
}

GDateDay     
g_date_day (GDate *d)
{
  g_return_val_if_fail (d != NULL, G_DATE_BAD_DAY);
  g_return_val_if_fail (g_date_valid (d), G_DATE_BAD_DAY);
  
  if (!d->dmy) 
    {
      g_date_update_dmy (d);
    }
  g_return_val_if_fail (d->dmy, G_DATE_BAD_DAY);  
  
  return d->day;
}

guint32      
g_date_julian (GDate *d)
{
  g_return_val_if_fail (d != NULL, G_DATE_BAD_JULIAN);
  g_return_val_if_fail (g_date_valid (d), G_DATE_BAD_JULIAN);
  
  if (!d->julian) 
    {
      g_date_update_julian (d);
    }
  g_return_val_if_fail (d->julian, G_DATE_BAD_JULIAN);  
  
  return d->julian_days;
}

guint        
g_date_day_of_year (GDate *d)
{
  gint index;
  
  g_return_val_if_fail (d != NULL, 0);
  g_return_val_if_fail (g_date_valid (d), 0);
  
  if (!d->dmy) 
    {
      g_date_update_dmy (d);
    }
  g_return_val_if_fail (d->dmy, 0);  
  
  index = g_date_is_leap_year (d->year) ? 1 : 0;
  
  return (days_in_year[index][d->month] + d->day);
}

guint        
g_date_monday_week_of_year (GDate *d)
{
  GDateWeekday wd;
  guint day;
  GDate first;
  
  g_return_val_if_fail (d != NULL, 0);
  g_return_val_if_fail (g_date_valid (d), 0);
  
  if (!d->dmy) 
    {
      g_date_update_dmy (d);
    }
  g_return_val_if_fail (d->dmy, 0);  
  
  g_date_clear (&first, 1);
  
  g_date_set_dmy (&first, 1, 1, d->year);
  
  wd = g_date_weekday (&first) - 1; /* make Monday day 0 */
  day = g_date_day_of_year (d) - 1;
  
  return ((day + wd)/7U + (wd == 0 ? 1 : 0));
}

guint        
g_date_sunday_week_of_year (GDate *d)
{
  GDateWeekday wd;
  guint day;
  GDate first;
  
  g_return_val_if_fail (d != NULL, 0);
  g_return_val_if_fail (g_date_valid (d), 0);
  
  if (!d->dmy) 
    {
      g_date_update_dmy (d);
    }
  g_return_val_if_fail (d->dmy, 0);  
  
  g_date_clear (&first, 1);
  
  g_date_set_dmy (&first, 1, 1, d->year);
  
  wd = g_date_weekday (&first);
  if (wd == 7) wd = 0; /* make Sunday day 0 */
  day = g_date_day_of_year (d) - 1;
  
  return ((day + wd)/7U + (wd == 0 ? 1 : 0));
}

void         
g_date_clear (GDate       *d, guint ndates)
{
  g_return_if_fail (d != NULL);
  g_return_if_fail (ndates != 0);
  
  memset (d, 0x0, ndates*sizeof (GDate)); 
}

G_LOCK_DEFINE_STATIC (g_date_global);

/* These are for the parser, output to the user should use *
 * g_date_strftime () - this creates more never-freed memory to annoy
 * all those memory debugger users. :-) 
 */

static gchar *long_month_names[13] = 
{ 
  "Error", NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL 
};

static gchar *short_month_names[13] = 
{
  "Error", NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL 
};

/* This tells us if we need to update the parse info */
static gchar *current_locale = NULL;

/* order of these in the current locale */
static GDateDMY dmy_order[3] = 
{
   G_DATE_DAY, G_DATE_MONTH, G_DATE_YEAR
};

/* Where to chop two-digit years: i.e., for the 1930 default, numbers
 * 29 and below are counted as in the year 2000, numbers 30 and above
 * are counted as in the year 1900.  
 */

static GDateYear twodigit_start_year = 1930;

/* It is impossible to enter a year between 1 AD and 99 AD with this
 * in effect.  
 */
static gboolean using_twodigit_years = FALSE;

struct _GDateParseTokens {
  gint num_ints;
  gint n[3];
  guint month;
};

typedef struct _GDateParseTokens GDateParseTokens;

#define NUM_LEN 10

/* HOLDS: g_date_global_lock */
static void
g_date_fill_parse_tokens (const gchar *str, GDateParseTokens *pt)
{
  gchar num[4][NUM_LEN+1];
  gint i;
  const gchar *s;
  
  /* We count 4, but store 3; so we can give an error
   * if there are 4.
   */
  num[0][0] = num[1][0] = num[2][0] = num[3][0] = '\0';
  
  s = str;
  pt->num_ints = 0;
  while (*s && pt->num_ints < 4) 
    {
      
      i = 0;
      while (*s && isdigit (*s) && i <= NUM_LEN)
        {
          num[pt->num_ints][i] = *s;
          ++s; 
          ++i;
        }
      
      if (i > 0) 
        {
          num[pt->num_ints][i] = '\0';
          ++(pt->num_ints);
        }
      
      if (*s == '\0') break;
      
      ++s;
    }
  
  pt->n[0] = pt->num_ints > 0 ? atoi (num[0]) : 0;
  pt->n[1] = pt->num_ints > 1 ? atoi (num[1]) : 0;
  pt->n[2] = pt->num_ints > 2 ? atoi (num[2]) : 0;
  
  pt->month = G_DATE_BAD_MONTH;
  
  if (pt->num_ints < 3)
    {
      gchar lcstr[128];
      int i = 1;
      
      strncpy (lcstr, str, 127);
      g_strdown (lcstr);
      
      while (i < 13)
        {
          if (long_month_names[i] != NULL) 
            {
              const gchar *found = strstr (lcstr, long_month_names[i]);
	      
              if (found != NULL)
                {
                  pt->month = i;
		  return;
                }
            }
	  
          if (short_month_names[i] != NULL) 
            {
              const gchar *found = strstr (lcstr, short_month_names[i]);
	      
              if (found != NULL)
                {
                  pt->month = i;
                  return;
                }
            }

          ++i;
        }      
    }
}

/* HOLDS: g_date_global_lock */
static void
g_date_prepare_to_parse (const gchar *str, GDateParseTokens *pt)
{
  const gchar *locale = setlocale (LC_TIME, NULL);
  gboolean recompute_localeinfo = FALSE;
  GDate d;
  
  g_return_if_fail (locale != NULL); /* should not happen */
  
  g_date_clear (&d, 1);              /* clear for scratch use */
  
  if ( (current_locale == NULL) || (strcmp (locale, current_locale) != 0) ) 
    {
      recompute_localeinfo = TRUE;  /* Uh, there used to be a reason for the temporary */
    }
  
  if (recompute_localeinfo)
    {
      int i = 1;
      GDateParseTokens testpt;
      gchar buf[128];
      
      g_free (current_locale); /* still works if current_locale == NULL */
      
      current_locale = g_strdup (locale);
      
      while (i < 13) 
        {
          g_date_set_dmy (&d, 1, i, 1);
	  
          g_return_if_fail (g_date_valid (&d));
	  
          g_date_strftime (buf, 127, "%b", &d);
          g_free (short_month_names[i]);
          g_strdown (buf);
          short_month_names[i] = g_strdup (buf);
	  
          
	  
          g_date_strftime (buf, 127, "%B", &d);
          g_free (long_month_names[i]);
          g_strdown (buf);
          long_month_names[i] = g_strdup (buf);
          
          ++i;
        }
      
      /* Determine DMY order */
      
      /* had to pick a random day - don't change this, some strftimes
       * are broken on some days, and this one is good so far. */
      g_date_set_dmy (&d, 4, 7, 1976);
      
      g_date_strftime (buf, 127, "%x", &d);
      
      g_date_fill_parse_tokens (buf, &testpt);
      
      i = 0;
      while (i < testpt.num_ints)
        {
          switch (testpt.n[i])
            {
            case 7:
              dmy_order[i] = G_DATE_MONTH;
              break;
            case 4:
              dmy_order[i] = G_DATE_DAY;
              break;
            case 76:
              using_twodigit_years = TRUE; /* FALL THRU */
            case 1976:
              dmy_order[i] = G_DATE_YEAR;
              break;
            default:
              /* leave it unchanged */
              break;
            }
          ++i;
        }
      
#ifdef G_ENABLE_DEBUG
      g_message ("**GDate prepared a new set of locale-specific parse rules.");
      i = 1;
      while (i < 13) 
        {
          g_message ("  %s   %s", long_month_names[i], short_month_names[i]);
          ++i;
        }
      if (using_twodigit_years)
        {
          g_message ("**Using twodigit years with cutoff year: %u", twodigit_start_year);
        }
      { 
        gchar *strings[3];
        i = 0;
        while (i < 3)
          {
            switch (dmy_order[i])
              {
              case G_DATE_MONTH:
                strings[i] = "Month";
                break;
              case G_DATE_YEAR:
                strings[i] = "Year";
                break;
              case G_DATE_DAY:
                strings[i] = "Day";
                break;
              default:
                strings[i] = NULL;
                break;
              }
            ++i;
          }
        g_message ("**Order: %s, %s, %s", strings[0], strings[1], strings[2]);
        g_message ("**Sample date in this locale: `%s'", buf);
      }
#endif
    }
  
  g_date_fill_parse_tokens (str, pt);
}

void         
g_date_set_parse (GDate       *d, 
                  const gchar *str)
{
  GDateParseTokens pt;
  guint m = G_DATE_BAD_MONTH, day = G_DATE_BAD_DAY, y = G_DATE_BAD_YEAR;
  
  g_return_if_fail (d != NULL);
  
  /* set invalid */
  g_date_clear (d, 1);
  
  G_LOCK (g_date_global);

  g_date_prepare_to_parse (str, &pt);
  
#ifdef G_ENABLE_DEBUG
  g_message ("Found %d ints, `%d' `%d' `%d' and written out month %d", 
	     pt.num_ints, pt.n[0], pt.n[1], pt.n[2], pt.month);
#endif
  
  
  if (pt.num_ints == 4) 
    {
      G_UNLOCK (g_date_global);
      return; /* presumably a typo; bail out. */
    }
  
  if (pt.num_ints > 1)
    {
      int i = 0;
      int j = 0;
      
      g_assert (pt.num_ints < 4); /* i.e., it is 2 or 3 */
      
      while (i < pt.num_ints && j < 3) 
        {
          switch (dmy_order[j])
            {
            case G_DATE_MONTH:
	    {
	      if (pt.num_ints == 2 && pt.month != G_DATE_BAD_MONTH)
		{
		  m = pt.month;
		  ++j;      /* skip months, but don't skip this number */
		  continue;
		}
	      else 
		m = pt.n[i];
	    }
	    break;
            case G_DATE_DAY:
	    {
	      if (pt.num_ints == 2 && pt.month == G_DATE_BAD_MONTH)
		{
		  day = 1;
		  ++j;      /* skip days, since we may have month/year */
		  continue;
		}
	      day = pt.n[i];
	    }
	    break;
            case G_DATE_YEAR:
	    {
	      y  = pt.n[i];
	      
	      if (using_twodigit_years && y < 100)
		{
		  guint two     =  twodigit_start_year % 100;
		  guint century = (twodigit_start_year / 100) * 100;
		  
		  if (y < two)
		    century += 100;
		  
		  y += century;
		}
	    }
	    break;
            default:
              break;
            }
	  
          ++i;
          ++j;
        }
      
      
      if (pt.num_ints == 3 && !g_date_valid_dmy (day, m, y))
        {
          /* Try YYYY MM DD */
          y   = pt.n[0];
          m   = pt.n[1];
          day = pt.n[2];
          
          if (using_twodigit_years && y < 100) 
            y = G_DATE_BAD_YEAR; /* avoids ambiguity */
        }
    }
  else if (pt.num_ints == 1) 
    {
      if (pt.month != G_DATE_BAD_MONTH)
        {
          /* Month name and year? */
          m    = pt.month;
          day  = 1;
          y = pt.n[0];
        }
      else
        {
          /* Try yyyymmdd and yymmdd */
	  
          m   = (pt.n[0]/100) % 100;
          day = pt.n[0] % 100;
          y   = pt.n[0]/10000;
	  
          /* FIXME move this into a separate function */
          if (using_twodigit_years && y < 100)
            {
              guint two     =  twodigit_start_year % 100;
              guint century = (twodigit_start_year / 100) * 100;
              
              if (y < two)
                century += 100;
              
              y += century;
            }
        }
    }
  
  /* See if we got anything valid out of all this. */
  /* y < 8000 is to catch 19998 style typos; the library is OK up to 65535 or so */
  if (y < 8000 && g_date_valid_dmy (day, m, y)) 
    {
      d->month = m;
      d->day   = day;
      d->year  = y;
      d->dmy   = TRUE;
    }
#ifdef G_ENABLE_DEBUG
  else 
    g_message ("Rejected DMY %u %u %u", day, m, y);
#endif
  G_UNLOCK (g_date_global);
}

void         
g_date_set_time (GDate *d,
		 GTime  time)
{
  time_t t = time;
  struct tm tm;
  
  g_return_if_fail (d != NULL);
  
#ifdef HAVE_LOCALTIME_R
  localtime_r (&t, &tm);
#else
  {
    struct tm *ptm = localtime (&t);
    g_assert (ptm);
    memcpy ((void *) &tm, (void *) ptm, sizeof(struct tm));
  }
#endif
  
  d->julian = FALSE;
  
  d->month = tm.tm_mon + 1;
  d->day   = tm.tm_mday;
  d->year  = tm.tm_year + 1900;
  
  g_return_if_fail (g_date_valid_dmy (d->day, d->month, d->year));
  
  d->dmy    = TRUE;
}

void         
g_date_set_month (GDate     *d, 
                  GDateMonth m)
{
  g_return_if_fail (d != NULL);
  g_return_if_fail (g_date_valid_month (m));

  if (d->julian && !d->dmy) g_date_update_dmy(d);
  d->julian = FALSE;
  
  d->month = m;
  
  if (g_date_valid_dmy (d->day, d->month, d->year))
    d->dmy = TRUE;
  else 
    d->dmy = FALSE;
}

void         
g_date_set_day (GDate     *d, 
                GDateDay day)
{
  g_return_if_fail (d != NULL);
  g_return_if_fail (g_date_valid_day (day));
  
  if (d->julian && !d->dmy) g_date_update_dmy(d);
  d->julian = FALSE;
  
  d->day = day;
  
  if (g_date_valid_dmy (d->day, d->month, d->year))
    d->dmy = TRUE;
  else 
    d->dmy = FALSE;
}

void         
g_date_set_year (GDate     *d, 
                 GDateYear  y)
{
  g_return_if_fail (d != NULL);
  g_return_if_fail (g_date_valid_year (y));
  
  if (d->julian && !d->dmy) g_date_update_dmy(d);
  d->julian = FALSE;
  
  d->year = y;
  
  if (g_date_valid_dmy (d->day, d->month, d->year))
    d->dmy = TRUE;
  else 
    d->dmy = FALSE;
}

void         
g_date_set_dmy (GDate     *d, 
                GDateDay   day, 
                GDateMonth m, 
                GDateYear  y)
{
  g_return_if_fail (d != NULL);
  g_return_if_fail (g_date_valid_dmy (day, m, y));
  
  d->julian = FALSE;
  
  d->month = m;
  d->day   = day;
  d->year  = y;
  
  d->dmy = TRUE;
}

void         
g_date_set_julian (GDate *d, guint32 j)
{
  g_return_if_fail (d != NULL);
  g_return_if_fail (g_date_valid_julian (j));
  
  d->julian_days = j;
  d->julian = TRUE;
  d->dmy = FALSE;
}


gboolean     
g_date_is_first_of_month (GDate *d)
{
  g_return_val_if_fail (d != NULL, FALSE);
  g_return_val_if_fail (g_date_valid (d), FALSE);
  
  if (!d->dmy) 
    {
      g_date_update_dmy (d);
    }
  g_return_val_if_fail (d->dmy, FALSE);  
  
  if (d->day == 1) return TRUE;
  else return FALSE;
}

gboolean     
g_date_is_last_of_month (GDate *d)
{
  gint index;
  
  g_return_val_if_fail (d != NULL, FALSE);
  g_return_val_if_fail (g_date_valid (d), FALSE);
  
  if (!d->dmy) 
    {
      g_date_update_dmy (d);
    }
  g_return_val_if_fail (d->dmy, FALSE);  
  
  index = g_date_is_leap_year (d->year) ? 1 : 0;
  
  if (d->day == days_in_months[index][d->month]) return TRUE;
  else return FALSE;
}

void         
g_date_add_days (GDate *d, guint ndays)
{
  g_return_if_fail (d != NULL);
  g_return_if_fail (g_date_valid (d));
  
  if (!d->julian)
    {
      g_date_update_julian (d);
    }
  g_return_if_fail (d->julian);
  
  d->julian_days += ndays;
  d->dmy = FALSE;
}

void         
g_date_subtract_days (GDate *d, guint ndays)
{
  g_return_if_fail (d != NULL);
  g_return_if_fail (g_date_valid (d));
  
  if (!d->julian)
    {
      g_date_update_julian (d);
    }
  g_return_if_fail (d->julian);
  g_return_if_fail (d->julian_days > ndays);
  
  d->julian_days -= ndays;
  d->dmy = FALSE;
}

void         
g_date_add_months (GDate       *d, 
                   guint        nmonths)
{
  guint years, months;
  gint index;
  
  g_return_if_fail (d != NULL);
  g_return_if_fail (g_date_valid (d));
  
  if (!d->dmy) 
    {
      g_date_update_dmy (d);
    }
  g_return_if_fail (d->dmy);  
  
  nmonths += d->month - 1;
  
  years  = nmonths/12;
  months = nmonths%12;
  
  d->month = months + 1;
  d->year  += years;
  
  index = g_date_is_leap_year (d->year) ? 1 : 0;
  
  if (d->day > days_in_months[index][d->month])
    d->day = days_in_months[index][d->month];
  
  d->julian = FALSE;
  
  g_return_if_fail (g_date_valid (d));
}

void         
g_date_subtract_months (GDate       *d, 
                        guint        nmonths)
{
  guint years, months;
  gint index;
  
  g_return_if_fail (d != NULL);
  g_return_if_fail (g_date_valid (d));
  
  if (!d->dmy) 
    {
      g_date_update_dmy (d);
    }
  g_return_if_fail (d->dmy);  
  
  years  = nmonths/12;
  months = nmonths%12;
  
  g_return_if_fail (d->year > years);
  
  d->year  -= years;
  
  if (d->month > months) d->month -= months;
  else 
    {
      months -= d->month;
      d->month = 12 - months;
      d->year -= 1;
    }
  
  index = g_date_is_leap_year (d->year) ? 1 : 0;
  
  if (d->day > days_in_months[index][d->month])
    d->day = days_in_months[index][d->month];
  
  d->julian = FALSE;
  
  g_return_if_fail (g_date_valid (d));
}

void         
g_date_add_years (GDate       *d, 
                  guint        nyears)
{
  g_return_if_fail (d != NULL);
  g_return_if_fail (g_date_valid (d));
  
  if (!d->dmy) 
    {
      g_date_update_dmy (d);
    }
  g_return_if_fail (d->dmy);  
  
  d->year += nyears;
  
  if (d->month == 2 && d->day == 29)
    {
      if (!g_date_is_leap_year (d->year))
        {
          d->day = 28;
        }
    }
  
  d->julian = FALSE;
}

void         
g_date_subtract_years (GDate       *d, 
                       guint        nyears)
{
  g_return_if_fail (d != NULL);
  g_return_if_fail (g_date_valid (d));
  
  if (!d->dmy) 
    {
      g_date_update_dmy (d);
    }
  g_return_if_fail (d->dmy);  
  g_return_if_fail (d->year > nyears);
  
  d->year -= nyears;
  
  if (d->month == 2 && d->day == 29)
    {
      if (!g_date_is_leap_year (d->year))
        {
          d->day = 28;
        }
    }
  
  d->julian = FALSE;
}


gboolean     
g_date_is_leap_year (GDateYear  year)
{
  g_return_val_if_fail (g_date_valid_year (year), FALSE);
  
  return ( (((year % 4) == 0) && ((year % 100) != 0)) ||
           (year % 400) == 0 );
}

guint8         
g_date_days_in_month (GDateMonth month, 
                      GDateYear  year)
{
  gint index;
  
  g_return_val_if_fail (g_date_valid_year (year), 0);
  g_return_val_if_fail (g_date_valid_month (month), 0);
  
  index = g_date_is_leap_year (year) ? 1 : 0;
  
  return days_in_months[index][month];
}

guint8       
g_date_monday_weeks_in_year (GDateYear  year)
{
  GDate d;
  
  g_return_val_if_fail (g_date_valid_year (year), 0);
  
  g_date_clear (&d, 1);
  g_date_set_dmy (&d, 1, 1, year);
  if (g_date_weekday (&d) == G_DATE_MONDAY) return 53;
  g_date_set_dmy (&d, 31, 12, year);
  if (g_date_weekday (&d) == G_DATE_MONDAY) return 53;
  if (g_date_is_leap_year (year)) 
    {
      g_date_set_dmy (&d, 2, 1, year);
      if (g_date_weekday (&d) == G_DATE_MONDAY) return 53;
      g_date_set_dmy (&d, 30, 12, year);
      if (g_date_weekday (&d) == G_DATE_MONDAY) return 53;
    }
  return 52;
}

guint8       
g_date_sunday_weeks_in_year (GDateYear  year)
{
  GDate d;
  
  g_return_val_if_fail (g_date_valid_year (year), 0);
  
  g_date_clear (&d, 1);
  g_date_set_dmy (&d, 1, 1, year);
  if (g_date_weekday (&d) == G_DATE_SUNDAY) return 53;
  g_date_set_dmy (&d, 31, 12, year);
  if (g_date_weekday (&d) == G_DATE_SUNDAY) return 53;
  if (g_date_is_leap_year (year)) 
    {
      g_date_set_dmy (&d, 2, 1, year);
      if (g_date_weekday (&d) == G_DATE_SUNDAY) return 53;
      g_date_set_dmy (&d, 30, 12, year);
      if (g_date_weekday (&d) == G_DATE_SUNDAY) return 53;
    }
  return 52;
}

gint         
g_date_compare (GDate     *lhs, 
                GDate     *rhs)
{
  g_return_val_if_fail (lhs != NULL, 0);
  g_return_val_if_fail (rhs != NULL, 0);
  g_return_val_if_fail (g_date_valid (lhs), 0);
  g_return_val_if_fail (g_date_valid (rhs), 0);
  
  /* Remember the self-comparison case! I think it works right now. */
  
  while (TRUE)
    {
      
      if (lhs->julian && rhs->julian) 
        {
          if (lhs->julian_days < rhs->julian_days) return -1;
          else if (lhs->julian_days > rhs->julian_days) return 1;
          else                                          return 0;
        }
      else if (lhs->dmy && rhs->dmy) 
        {
          if (lhs->year < rhs->year)               return -1;
          else if (lhs->year > rhs->year)               return 1;
          else 
            {
              if (lhs->month < rhs->month)         return -1;
              else if (lhs->month > rhs->month)         return 1;
              else 
                {
                  if (lhs->day < rhs->day)              return -1;
                  else if (lhs->day > rhs->day)              return 1;
                  else                                       return 0;
                }
              
            }
          
        }
      else
        {
          if (!lhs->julian) g_date_update_julian (lhs);
          if (!rhs->julian) g_date_update_julian (rhs);
          g_return_val_if_fail (lhs->julian, 0);
          g_return_val_if_fail (rhs->julian, 0);
        }
      
    }
  return 0; /* warnings */
}


void        
g_date_to_struct_tm (GDate      *d, 
                     struct tm   *tm)
{
  GDateWeekday day;
     
  g_return_if_fail (d != NULL);
  g_return_if_fail (g_date_valid (d));
  g_return_if_fail (tm != NULL);
  
  if (!d->dmy) 
    {
      g_date_update_dmy (d);
    }
  g_return_if_fail (d->dmy);
  
  /* zero all the irrelevant fields to be sure they're valid */
  
  /* On Linux and maybe other systems, there are weird non-POSIX
   * fields on the end of struct tm that choke strftime if they
   * contain garbage.  So we need to 0 the entire struct, not just the
   * fields we know to exist. 
   */
  
  memset (tm, 0x0, sizeof (struct tm));
  
  tm->tm_mday = d->day;
  tm->tm_mon  = d->month - 1; /* 0-11 goes in tm */
  tm->tm_year = ((int)d->year) - 1900; /* X/Open says tm_year can be negative */
  
  day = g_date_weekday (d);
  if (day == 7) day = 0; /* struct tm wants days since Sunday, so Sunday is 0 */
  
  tm->tm_wday = (int)day;
  
  tm->tm_yday = g_date_day_of_year (d) - 1; /* 0 to 365 */
  tm->tm_isdst = -1; /* -1 means "information not available" */
}

gsize     
g_date_strftime (gchar       *s, 
                 gsize        slen, 
                 const gchar *format, 
                 GDate       *d)
{
  struct tm tm;
  gsize retval;
  
  g_return_val_if_fail (d != NULL, 0);
  g_return_val_if_fail (g_date_valid (d), 0);
  g_return_val_if_fail (slen > 0, 0); 
  g_return_val_if_fail (format != 0, 0);
  g_return_val_if_fail (s != 0, 0);
  
  g_date_to_struct_tm (d, &tm);
  
  retval = strftime (s, slen, format, &tm);
  if (retval == 0)
    {
      /* If retval == 0, the contents of s are undefined.  We define
       *  them. 
       */
      s[0] = '\0';
    }
  return retval;
}
