#!/usr/bin/env python 
# -*- Mode: python -*-
#======================================================================
# FILE: DerivedProperties.py
# CREATOR: eric 
#
# DESCRIPTION:
#   
#
#  $Id: DerivedProperties.py,v 1.1 2001/11/15 19:27:41 mikep%oeone.com Exp $
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

from Property import Time, Period, Duration

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



