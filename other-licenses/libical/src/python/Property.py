#!/usr/bin/env python 
# -*- Mode: python -*-
#======================================================================
# FILE: Property.py
# CREATOR: eric 
#
# DESCRIPTION:
#   
#
#  $Id: Property.py,v 1.1 2001/11/15 19:27:43 mikep%oeone.com Exp $
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
#======================================================================

from LibicalWrap import *
from types import *
import regsub
import base64
from string import index, upper

#def icalerror_supress(arg): 
#    pass

#def icalerror_restore(a,b):
#    pass

def error_type():
    error = icalerror_perror()
    return error[:index(error,':')]

class Property:
    """ Represent any iCalendar Property.

    Usage:
    Property(dict)

    Where:
    dict is a dictionary with keys of 'name', 'value_type', and 'value'.
    In addition, parameter:parameter value entries may be included.
    """

    class ConstructorFailedError(Exception):
        "Failed to construct a property"

    class UpdateFailedError(Exception):
        "Failed to update the value of a property"
        

    def __init__(self, type = None, ref = None):


        assert(ref == None or isinstance(ref,StringType))
        assert(type == None or isinstance(type,StringType))

        self._ref = None
        
        if ref != None:
            self._ref = ref
        elif type != None:
            kind  = icalproperty_string_to_kind(type)
            self._ref = icalproperty_new(kind)


        if self._ref == None or self._ref == 'NULL':
            raise Property.ConstructorFailedError("Failed to construct Property")
            
        self._deleted = 0;


        # Initialize all of the required keys


    def __del__(self):

        self._deleted = 1;

        if not self._deleted and \
           self.ref() and \
           icalproperty_get_parent(self.ref()) == 'NULL':
            
            icalproperty_free(self.ref())
            
    def name(self,v=None):
        """ Return the name of the property """
        str = icalproperty_as_ical_string(self._ref)

        idx = index(str, '\n')

        return str[:idx]

    def ref(self,v=None):
        """ Return the internal reference to the libical icalproperty """
        if(v != None):

            if not self._deleted and self._ref and \
               icalproperty_get_parent(self._ref) == 'NULL':
                
                icalproperty_free(self._ref)

            self._ref = v

        return self._ref


    def value(self,v=None, kind = None):
        """ Return the RFC2445 representation of the value """

        if(v != None):
            
            if kind != None:
                # Get the default kind of value for this property 
                default_kind = icalvalue_kind_to_string(icalproperty_kind_to_value_kind(icalproperty_string_to_kind(self.name())))

                if(kind != default_kind):
                    self.__setitem__('VALUE',kind)
                vt = kind
            elif self.__getitem__('VALUE'):
                vt = self.__getitem__('VALUE')
            else:
                vt = 'NO' # Use the kind of the existing value


            icalerror_clear_errno()

            #e1=icalerror_supress("MALFORMEDDATA")
            icalproperty_set_value_from_string(self._ref,v,vt)
            #icalerror_restore("MALFORMEDDATA",e1)

            if error_type() != "NO":
                raise Property.UpdateFailedError(error_type())

            s = icalproperty_get_value_as_string(self._ref)
            assert(s == v)

        return icalproperty_get_value_as_string(self._ref)

    def parameters(self):

        d_string = icallangbind_property_eval_string(self._ref,":")
        dict = eval(d_string)

        desc_keys = ('name', 'value', 'value_type', 'pid', 'ref', 'deleted' )
        
        def foo(k,d=dict):
            if d.has_key(k): del d[k]

        map( foo, desc_keys)
        
        return filter(lambda p, s=self: s[p] != None, dict.keys())

                    
    def as_ical_string(self):
        
        return icalproperty_as_ical_string(self._ref)

    def __getitem__(self,key):
        """ Return property values by name """
        key = upper(key)

        str = icalproperty_get_parameter_as_string(self._ref,key)

        if(str == 'NULL'): return None

        return str

    def __setitem__(self,key,value):
        """ Set Property Values by Name """
        key = upper(key)

        icalproperty_set_parameter_from_string(self._ref,key,value)

        return self.__getitem__(key)

    def __str__(self):

        str = self.as_ical_string()
        return regsub.gsub('\r?\n ?','',str)

    def __cmp__(self, other):
        s_str = str(self)
        o_str = str(other)

        return cmp(s_str,o_str)


class Time(Property):
    """ Represent iCalendar DATE, TIME and DATE-TIME """
    def __init__(self, arg, name="DTSTART"):
        """ 
        Create a new Time from a string or number of seconds past the 
        POSIX epoch

        Time("19970325T123000Z")  Construct from an iCalendar string
        Time(8349873494)          Construct from seconds past POSIX epoch
        
        """
        e1=icalerror_supress("MALFORMEDDATA")
        e2=icalerror_supress("BADARG")

        if isinstance(arg, DictType):
            # Dictionary -- used for creating from Component
            self.tt = icaltime_from_string(arg['value'])
            Property.__init__(self, ref=arg['ref'])
        else:
            if isinstance(arg, StringType):
                # Create from an iCal string
                self.tt = icaltime_from_string(arg)
            elif isinstance(arg, IntType) or   \
                 isinstance(arg, FloatType): 
                # Create from seconds past the POSIX epoch
                self.tt = icaltime_from_timet(int(arg),0)
            elif isinstance(arg, Time):
                # Copy an instance
                self.tt = arg.tt
            else:
                self.tt = icaltime_null_time()

            Property.__init__(self,type=name)

        icalerror_restore("MALFORMEDDATA",e1)
        icalerror_restore("BADARG",e2)

        if icaltime_is_null_time(self.tt):
            raise Property.ConstructorFailedError("Failed to construct a Time")

        try:
            self._update_value()
        except Property.UpdateFailedError:
            raise Property.ConstructorFailedError("Failed to construct a Time")

    def _update_value(self):
        self.tt = icaltime_normalize(self.tt)
        self.value(icaltime_as_ical_string(self.tt),"DATE-TIME")

    def valid(self):
        " Return true if this is a valid time "
        return not icaltime_is_null_time(self.tt)

    def utc_seconds(self,v=None):
        """ Return or set time in seconds past POSIX epoch"""
        if (v!=None):
            self.tt = icaltime_from_timet(v,0)
            self._update_value()

        return icaltime_as_timet(self.tt)

    def is_utc(self,v=None):
        """ Return or set boolean indicating if time is in UTC """
        if(v != None):
            icaltimetype_is_utc_set(self.tt,v)
            self._update_value()
        return icaltimetype_is_utc_get(self.tt)

    def is_date(self,v=None):
        """ Return or set boolean indicating if time is actually a date """
        if(v != None):
            icaltimetype_is_date_set(self.tt,v)
            self._update_value()
        return icaltimetype_is_date_get(self.tt)

    def timezone(self,v=None):
        """ Return or set the timezone string for this time """

        if (v != None):
            assert(isinstance(v,StringType) )
            self['TZID'] = v
        return  self['TZID']

    def second(self,v=None):
        """ Get or set the seconds component of this time """
        if(v != None):
            icaltimetype_second_set(self.tt,v)
            self._update_value()
        return icaltimetype_second_get(self.tt)

    def minute(self,v=None):
        """ Get or set the minute component of this time """
        if(v != None):
            icaltimetype_minute_set(self.tt,v)
            self._update_value()
        return icaltimetype_minute_get(self.tt)

    def hour(self,v=None):
        """ Get or set the hour component of this time """
        if(v != None):
            icaltimetype_hour_set(self.tt,v)
            self._update_value()
        return icaltimetype_hour_get(self.tt)

    def day(self,v=None):
        """ Get or set the month day component of this time """
        if(v != None):
            icaltimetype_day_set(self.tt,v)
            self._update_value()
        return icaltimetype_day_get(self.tt)

    def month(self,v=None):
        """ Get or set the month component of this time. January is month 1 """
        if(v != None):
            icaltimetype_month_set(self.tt,v)
            self._update_value()
        return icaltimetype_month_get(self.tt)

    def year(self,v=None):
        """ Get or set the year component of this time """
        if(v != None):
            icaltimetype_year_set(self.tt,v)
            self._update_value()

        return icaltimetype_year_get(self.tt)



    def __add__(self,o):

        other = Duration(o,"DURATION")      

        if not other.valid():
            return Duration(0,"DURATION")
  
        seconds = self.utc_seconds() + other.seconds()
    
        new = Time(seconds,self.name())
        new.timezone(self.timezone())
        new.is_utc(self.is_utc())

        return new

    def __radd_(self,o):
        return self.__add__(o)
    

    def __sub__(self,o):

        
        if isinstance(o,Time):
            # Subtract a time from this time and return a duration
            seconds = self.utc_seconds() - other.utc_seconds()
            return Duration(seconds)
        elif isinstance(o,Duration):
            # Subtract a duration from this time and return a time
            other = Duration(o)
            if(not other.valid()):
                return Time()

            seconds = self.utc_seconds() - other.seconds()
            return Time(seconds)
        else:
            raise TypeError, "subtraction with Time reqires Time or Duration"

class Duration(Property):
    """ 
    Represent a length of time, like 3 minutes, or 6 days, 20 seconds.
    

    """

    def __init__(self, arg, name="DURATION"):
        """
        Create a new duration from an RFC2445 string or number of seconds.
        Construct the duration from an iCalendar string or a number of seconds.

        Duration("P3DT2H34M45S")   Construct from an iCalendar string
        Duration(3660)             Construct from seconds 
        """ 

        self.dur = None

        e=icalerror_supress("MALFORMEDDATA")

        if isinstance(arg, DictType):
            
            self.dur = icaldurationtype_from_string(arg['value'])
            Property.__init__(self,ref=arg['ref'])
        else:
            if isinstance(arg, StringType):
                self.dur = icaldurationtype_from_string(arg)
            elif isinstance(arg, IntType): 
                self.dur = icaldurationtype_from_int(arg)
            elif isinstance(arg,Duration):
                self.dur = arg.dur
            else:
                self.dur = icaldurationtype_null_duration()

            Property.__init__(self,type=name)

        icalerror_restore("MALFORMEDDATA",e)

        if self.dur == None or icaldurationtype_is_null_duration(self.dur):
            raise Property.ConstructorFailedError("Failed to construct Duration from " +str(arg))

        try:
            self._update_value()
        except Property.UpdateFailedError:
            raise Property.ConstructorFailedError("Failed to construct Duration from  " + str(arg))

    def _update_value(self):
        
        self.value(icaldurationtype_as_ical_string(self.dur),"DURATION")

    def valid(self):
        "Return true if this is a valid duration"

        return not icaldurationtype_is_null_duration(self.dur)

    def seconds(self,v=None):
        """Return or set duration in seconds"""
        if(v != None):
            self.dur = icaldurationtype_from_int(v);
            self.dict['value'] = icaltimedurationtype_as_ical_string(self.dur)
        return icaldurationtype_as_int(self.dur)


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

class Attendee(Property):
    """Class for Attendee properties.

    Usage:
    Attendee([dict])
    Attendee([address])

    Where:
    dict is an optional dictionary with keys of
     'value': CAL-ADDRESS string and any parameter: parameter_value entries.
     'name' and 'value_type' entries in dict are ignored and automatically set
     with the appropriate values.
    address is the CAL-ADDRESS (string) of the Attendee 
    """

    def __init__(self, arg={}):
        
        assert(isinstance(arg,DictType))

        ref = None
        
        if arg!={}:
            ref = arg['ref']

        Property.__init__(self,type='ATTENDEE',ref=ref)
        
    def _doParam(self, parameter, v):
        if v!=None:
            self[parameter]=v
        return self[parameter]

    # Methods for accessing enumerated parameters
    def cn(self, v=None): self._doParam('CN', v)
    def cutype(self, v=None): self._doParam('CUTYPE', v)
    def dir(self, v=None): self._doParam('DIR', v)
    def delegated_from(self, v=None): self._doParam('DELEGATED-FROM', v)
    def delegated_to(self, v=None): self._doParam('DELEGATED-TO', v)
    def language(self, v=None): self._doParam('LANGUAGE', v)
    def member(self, v=None): self._doParam('MEMBER', v)
    def partstat(self, v=None): self._doParam('PARTSTAT', v)
    def role(self, v=None): self._doParam('ROLE', v)
    def rsvp(self, v=None): self._doParam('RSVP', v)
    def sent_by(self, v=None): self._doParam('SENT-BY', v)


class Organizer(Property):
    """Class for Organizer property.
    """

    def __init__(self, arg={}):

        assert(isinstance(arg, DictType))
        
        ref = None
        if arg != {}:
            ref = arg['ref']
        Property.__init__(self, type='ORGANIZER', ref=ref)
       
##         param_t = ( 'CN', 'DIR', 'SENT-BY', 'LANGUAGE' )
##         for param in param_t:
##             self[param] = None
##         if value != None:
##             self.value(value)


    def _doParam(self, parameter, v):
        if v!=None:
            self[parameter]=v
        return self[parameter]

    def name(self):
        "Return the name of the property."
        return Property.name(self)

    def value_type(self):
        "Return the value type of the property."
        return self._desc['value_type']

    # Methods for accessing enumerated parameters
    def cn(self, v=None): self._doParam('CN', v)
    def dir(self, v=None): self._doParam('DIR', v)
    def language(self, v=None): self._doParam('LANGUAGE', v)
    def sent_by(self, v=None): self._doParam('SENT-BY', v)

class Recurrence_Id(Time):
    """Class for RECURRENCE-ID property.

    Usage:
    Reccurence_Id(dict)         # A normal property dictionary
    Reccurence_Id("19960401")   # An iCalendar string
    Reccurence_Id(8349873494)   # Seconds from epoch

    If the 'dict' constructor is used, 'name' and 'value_type'
    entries in dict are ignored and automatically set with the appropriate
    values.
    """

    def __init__(self, dict={}):
        Time.__init__(self, dict)
        Property.name(self, 'RECURRENCE-ID')

    def name(self):
        return Property.name(self)

    def _doParam(self, parameter, v):
        if v!=None:
            self[parameter]=v
        return self[parameter]

    # Enumerated parameters
    def value_parameter(self, v=None):
        """Sets or gets the VALUE parameter value.

        The value passed should be either "DATE-TIME" or "DATE".  Setting this
        parameter has no impact on the property's value_type.  Doing something
        like:

        rid=Recurrence_Id("19960401")    # Sets value & makes value_type="DATE"
        rid.value_parameter("DATE-TIME") # Sets the parameter VALUE=DATE-TIME

        Would be allowed (even though it is wrong), so pay attention.
        Verifying the component will reveal the error.
        """
        if v!=None and v!="DATE" and v!="DATE-TIME":
            raise ValueError, "%s is an invalid VALUE parameter value" % str(v)
        self._doParam("VALUE", v)

    def tzid(self, v=None):
        "Sets or gets the TZID parameter value."
        self._doParam("TZID", v)

    def range_parameter(self, v=None): # 'range' is a builtin function
        "Sets or gets the RANGE parameter value."
        if v!=None and v!="THISANDPRIOR" and v!= "THISANDFUTURE":
            raise ValueError, "%s is an invalid RANGE parameter value" % str(v)
        self._doParam("RANGE", v)

class Attach(Property):
    """A class representing an ATTACH property.

    Usage:
    Attach(uriString [, parameter_dict])
    Attach(fileObj [, parameter_dict])
    """

    def __init__(self, value=None, parameter_dict={}):
        Property.__init__(self, parameter_dict)
        Property.name(self, 'ATTACH')
        self.value(value)

    def value(self, v=None):
        "Returns or sets the value of the property."
        if v != None:
            if isinstance(v, StringType):  # Is a URI
                self._desc['value']=v
                Property.value_type(self, 'URI')
            else:
                try:
                    tempStr = v.read()
                except:
                    raise TypeError,"%s must be a URL string or file-ish type"\
                          % str(v)
                self._desc['value'] = base64.encodestring(tempStr)
                Property.value_type(self, 'BINARY')
        else:
            return self._desc['value']

    def name(self):
        "Returns the name of the property."
        return Property.name(self)

    def value_type(self):
        return Property.value_type(self)

    def fmttype(self, v=None):
        "Gets or sets the FMTYPE parameter."
        if v!= None:
            self['FMTTYPE']=v
        else:
            return self['FMTTYPE']

class RecurrenceSet: 
    """
    Represents a set of event occurrences. This
    class controls a component's RRULE, EXRULE, RDATE and EXDATE
    properties and can produce from them a set of occurrences. 
    """

    def __init__(self):
        pass

    def include(self, **params): 
        """ 
        Include a date or rule to the set. 

        Use date= or pass in a
        Time instance to include a date. Included dates will add an
        RDATE property or will remove an EXDATE property of the same
        date.

        Use rule= or pass in a string to include a rule. Included
        rules with either add a RRULE property or remove an EXRULE
        property.

        """
        pass

    def exclude(self, **params): 
        """ 
        Exclude date or rule to the set. 

        Use date= or pass in a Time instance to exclude a
        date. Excluded dates will add an EXDATE property or will remove
        an RDATE property of the same date.

        Use rule= or pass in a string to exclude a rule. Excluded
        rules with either add an EXRULE property or remove an RRULE
        property.

        """
        pass
        
    def occurrences(self, count=None):
        """
        Return 'count' occurrences as a tuple of Time instances.
        """
        pass


