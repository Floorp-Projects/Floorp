#!/usr/bin/env python 
# -*- Mode: python -*-
#======================================================================
# FILE: DerivedProperties.py
# CREATOR: eric 
#
# DESCRIPTION:
#   
#
#  $Id: DerivedProperties.py,v 1.2 2001/12/21 18:56:50 mikep%oeone.com Exp $
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

from Property import Property
from Time import Time
from Period import Period
from Duration import Duration

def RDate(arg):

    class RDate_Time(Time):
        def __init__(self,arg): Time.__init__(self,arg,"RDATE")
    
    class RDate_Period(Period):
        def __init__(self,arg): Period.__init__(self,arg,"RDATE")

    p = None
    for c in [RDate_Time, RDate_Period]:
        try: return c(arg)
        except Property.ConstructorFailedError, d: pass
    raise Property.ConstructorFailedError("Failed to construct RDATE from "+str(arg))


def Trigger(arg):        
    class Trigger_Time(Time): 
        def __init__(self,arg): Time.__init__(self,arg,"TRIGGER")
    
    class Trigger_Duration(Duration):
        def __init__(self,arg): Duration.__init__(self,arg,"TRIGGER")

    p = None
    for c in [Trigger_Duration, Trigger_Time]:
        try: return c(arg)
        except Property.ConstructorFailedError, d: pass        
    raise Property.ConstructorFailedError("Failed to construct TRIGGER from "+str(arg))



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

