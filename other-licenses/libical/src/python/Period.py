#!/usr/bin/env python 
# -*- Mode: python -*-
#======================================================================
# FILE: Period.py
# CREATOR: eric 
#
# DESCRIPTION:
#   
#
#  $Id: Period.py,v 1.1 2001/12/21 19:04:11 mikep%oeone.com Exp $
#  $Locker:  $
#
# (C) COPYRIGHT 2001, Eric Busboom <eric@softwarestudio.org>
# (C) COPYRIGHT 2001, Patrick Lewis <plewis@inetarena.com>  
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of either: 
#
#    The LGPL as published by the Free Software Foundation, version
#    2.1, available at: http://www.fsf.org/copyleft/lesser.html
#
#  Or:
#
#    The Mozilla Public License Version 1.0. You may obtain a copy of
#    the License at http://www.mozilla.org/MPL/
#===========================================================

from LibicalWrap import *
from Property import Property
from types import DictType, StringType, IntType
from Time import Time
from Duration import Duration

class Period(Property):
    """Represent a span of time"""
    def __init__(self,arg,name='FREEBUSY'):
        """ """

        Property.__init__(self, type = name)

        self.pt=None
        
        #icalerror_clear_errno()
        e1=icalerror_supress("MALFORMEDDATA")
        e2=icalerror_supress("BADARG")

        if isinstance(arg, DictType):
            

            es=icalerror_supress("MALFORMEDDATA")
            self.pt = icalperiodtype_from_string(arg['value'])
            icalerror_restore("MALFORMEDDATA",es)

            Property.__init__(self, ref=arg['ref'])
        else:
            if isinstance(arg, StringType):

                self.pt = icalperiodtype_from_string(arg)

            else:
                self.pt = icalperiodtype_null_period()

            Property.__init__(self,type=name)
                
        icalerror_restore("MALFORMEDDATA",e1)
        icalerror_restore("BADARG",e2)


        if self.pt == None or icalperiodtype_is_null_period(self.pt):
            raise Property.ConstructorFailedError("Failed to construct Period")

        
        try:
            self._update_value()
        except Property.UpdateFailedError:
            raise Property.ConstructorFailedError("Failed to construct Period")

    def _end_is_duration(self):        
        dur = icalperiodtype_duration_get(self.pt)
        if not icaldurationtype_is_null_duration(dur):
            return 1
        return 0

    def _end_is_time(self):
        end = icalperiodtype_end_get(self.pt)
        if not icaltime_is_null_time(end):
            return 1
        return 0

    def _update_value(self):

        self.value(icalperiodtype_as_ical_string(self.pt),"PERIOD")


    def valid(self):
        "Return true if this is a valid period"

        return not icalperiodtype_is_null_period(self.dur)

    def start(self,v=None):
        """
        Return or set start time of the period. The start time may be
        expressed as an RFC2445 format string or an instance of Time.
        The return value is an instance of Time
        """

        if(v != None):
            if isinstance(t,Time):
                t = v
            elif isinstance(t,StringType) or isinstance(t,IntType):
                t = Time(v,"DTSTART")
            else:
                raise TypeError

            icalperiodtype_start_set(self.pt,t.tt)

            self._update_value()
                
        
        return Time(icaltime_as_timet(icalperiodtype_start_get(self.pt)),
                    "DTSTART")

    def end(self,v=None):
        """
        Return or set end time of the period. The end time may be
        expressed as an RFC2445 format string or an instance of Time.
        The return value is an instance of Time.

        If the Period has a duration set, but not an end time, this
        method will caluculate the end time from the duration.  """

        if(v != None):
            
            if isinstance(t,Time):
                t = v
            elif isinstance(t,StringType) or isinstance(t,IntType):
                t = Time(v)
            else:
                raise TypeError

            if(self._end_is_duration()):
                start = icaltime_as_timet(icalperiodtype_start_get(self.pt))
                dur = t.utc_seconds()-start;
                icalperiodtype_duration_set(self.pt,
                                            icaldurationtype_from_int(dur))
            else:
                icalperiodtype_end_set(self.pt,t.tt)
                
            self._update_value()

        if(self._end_is_time()):
            rt = Time(icaltime_as_timet(icalperiodtype_end_get(self.pt)),
                      'DTEND')
            rt.timezone(self.timezone())
            return rt
        elif(self._end_is_duration()):
            start = icaltime_as_timet(icalperiodtype_start_get(self.pt))
            dur = icaldurationtype_as_int(icalperiodtype_duration_get(self.pt))
            rt = Time(start+dur,'DTEND')
            rt.timezone(self.timezone())
            return rt
        else:
            return Time({},'DTEND')



    def duration(self,v=None):
        """
        Return or set the duration of the period. The duration may be
        expressed as an RFC2445 format string or an instance of Duration.
        The return value is an instance of Duration.

        If the period has an end time set, but not a duration, this
        method will calculate the duration from the end time.  """

        if(v != None):
            
            if isinstance(t,Duration):
                d = v
            elif isinstance(t,StringType) or isinstance(t,IntType):
                d = Duration(v)
            else:
                raise TypeError

            if(self._end_is_time()):
                start = icaltime_as_timet(icalperiodtype_start_get(self.pt))
                end = start + d.seconds()

                icalperiodtype_end_set(self.pt,icaltime_from_timet(end,0))
            else:
                icalperiodtype_duration_set(self.pt,d.dur)
                
        if(self._end_is_time()):
            start =icaltime_as_timet(icalperiodtype_start_get(self.pt))
            end = icaltime_as_timet(icalperiodtype_end_get(self.pt))

            print "End is time " + str(end-start)

            return Duration(end-start,"DURATION")

        elif(self._end_is_duration()):
            dur = icaldurationtype_as_int(
                icalperiodtype_duration_get(self.pt))

            return Duration(dur,"DURATION")
        else:


            return Duration(0,"DURATION")


    def timezone(self,v=None):
        """ Return or set the timezone string for this time """
        if (v != None):
            self['TZID'] = v
        return  self['TZID']
