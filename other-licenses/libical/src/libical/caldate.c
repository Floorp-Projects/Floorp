/*
 *	Copyright (c) 1986-2000, Hiram Clawson 
 *	All rights reserved.
 *
 *	Redistribution and use in source and binary forms, with or
 *	without modification, are permitted provided that the following
 *	conditions are met:
 *
 *		Redistributions of source code must retain the above
 *		copyright notice, this list of conditions and the
 *		following disclaimer.
 *
 *		Redistributions in binary form must reproduce the
 *		above copyright notice, this list of conditions and
 *		the following disclaimer in the documentation and/or
 *		other materials provided with the distribution.
 *
 *		Neither name of The Museum of Hiram nor the names of
 *		its contributors may be used to endorse or promote products
 *		derived from this software without specific prior
 *		written permission. 
 *
 *	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 *	CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 *	INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 *	MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 *	IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 *	INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 *	(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 *	OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 *	HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 *	STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 *	IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 *	THE POSSIBILITY OF SUCH DAMAGE. 
 */
/*
 *	caldat computes the day of the week, the day of the year
 *	the gregorian (or julian) calendar date and the universal
 *	time from the julian decimal date.
 *	for astronomical purposes, The Gregorian calendar reform occurred
 *	on 15 Oct. 1582.  This is 05 Oct 1582 by the julian calendar.

 *	Input:	a ut_instant structure pointer, where the j_date element
 *		has been set. ( = 0 for 01 Jan 4713 B.C. 12 HR UT )
 *
 *	output:  will set all the other elements of the structure.
 *		As a convienence, the function will also return the year.
 *
 *	Reference: Astronomial formulae for calculators, meeus, p 23
 *	from fortran program by F. Espenak - April 1982 Page 277,
 *	50 Year canon of solar eclipses: 1986-2035
 *
 */

#include "astime.h"	/*	time structures	*/

long caldat( date )
struct ut_instant * date;
{
	double frac;
	long jd;
	long ka;
	long kb;
	long kc;
	long kd;
	long ke;
	long ialp;

	jd = (long) (date->j_date + 0.5);	/* integer julian date */
	frac = date->j_date + 0.5 - (double) jd + 1.0e-10; /* day fraction */
	ka = (long) jd;
	if ( jd >= 2299161L )
	{
		ialp = ( (double) jd - 1867216.25 ) / ( 36524.25 );
		ka = jd + 1L + ialp - ( ialp >> 2 );
	}
	kb = ka + 1524L;
	kc =  ( (double) kb - 122.1 ) / 365.25;
	kd = (double) kc * 365.25;
	ke = (double) ( kb - kd ) / 30.6001;
	date->day = kb - kd - ((long) ( (double) ke * 30.6001 ));
	if ( ke > 13L )
		date->month = ke - 13L;
	else
		date->month = ke - 1L;
	if ( (date->month == 2) && (date->day > 28) )
		date->day = 29;
	if ( (date->month == 2) && (date->day == 29) && (ke == 3L) )
		date->year = kc - 4716L;
	else if ( date->month > 2 )
		date->year = kc - 4716L;
	else
		date->year = kc - 4715L;
	date->i_hour = date->d_hour = frac * 24.0;	/* hour */
	date->i_minute = date->d_minute =
		( date->d_hour - (double) date->i_hour ) * 60.0; /* minute */
	date->i_second = date->d_second =
		( date->d_minute - (double) date->i_minute ) * 60.0;/* second */
	date->weekday = (jd + 1L) % 7L;	/* day of week */
	if ( date->year == ((date->year >> 2) << 2) )
		date->day_of_year =
			( ( 275 * date->month ) / 9)
			- ((date->month + 9) / 12)
			+ date->day - 30;
	else
		date->day_of_year =
			( ( 275 * date->month ) / 9)
			- (((date->month + 9) / 12) << 1)
			+ date->day - 30;
	return( date->year );
}	/*	end of	 long caldat( date )	*/

/*
 *	juldat computes the julian decimal date (j_date) from
 *	the gregorian (or Julian) calendar date.
 *	for astronomical purposes, The Gregorian calendar reform occurred
 *	on 15 Oct. 1582.  This is 05 Oct 1582 by the julian calendar.
 *	Input:  a ut_instant structure pointer where Day, Month, Year and
 *		i_hour, i_minute, d_second have been set for the date
 *		in question.
 *
 *	Output: the j_date and weekday elements of the structure will be set.
 *		Also, the return value of the function will be the j_date too.
 *
 *	Reference: Astronomial formulae for calculators, meeus, p 23
 *	from fortran program by F. Espenak - April 1982 Page 276,
 *	50 Year canon of solar eclipses: 1986-2035
 */

double juldat( date )
struct ut_instant * date;
{
	double frac, gyr;
	long iy0, im0;
	long ia, ib;
	long jd;

	/* decimal day fraction	*/
	frac = (( double)date->i_hour/ 24.0)
		+ ((double) date->i_minute / 1440.0)
		+ (date->d_second / 86400.0);
	/* convert date to format YYYY.MMDDdd	*/
	gyr = (double) date->year
		+ (0.01 * (double) date->month)
		+ (0.0001 * (double) date->day)
		+ (0.0001 * frac) + 1.0e-9;
	/* conversion factors */
	if ( date->month <= 2 )
	{
		iy0 = date->year - 1L;
		im0 = date->month + 12;
	}
	else
	{
		iy0 = date->year;
		im0 = date->month;
	}
	ia = iy0 / 100L;
	ib = 2L - ia + (ia >> 2);
	/* calculate julian date	*/
	if ( date->year < 0L )
		jd = (long) ((365.25 * (double) iy0) - 0.75)
			+ (long) (30.6001 * (im0 + 1L) )
			+ (long) date->day + 1720994L;
	else
		jd = (long) (365.25 * (double) iy0)
			+ (long) (30.6001 * (double) (im0 + 1L))
			+ (long) date->day + 1720994L;
	if ( gyr >= 1582.1015 )	/* on or after 15 October 1582	*/
		jd += ib;
	date->j_date = (double) jd + frac + 0.5;
	jd = (long) (date->j_date + 0.5);
	date->weekday = (jd + 1L) % 7L;
	return( date->j_date );
}	/*	end of	double juldat( date )	*/
