#!/usr/bin/env python 
# -*- Mode: python -*-
#======================================================================
# FILE: Property.py
# CREATOR: eric 
#
# DESCRIPTION:
#   
#
#  $Id: Attendee.py,v 1.1 2001/12/21 19:04:11 mikep%oeone.com Exp $
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
from Property import Property
from types import DictType, StringType, IntType

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
