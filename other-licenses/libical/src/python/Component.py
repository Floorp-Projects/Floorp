#!/usr/bin/env python 
# -*- Mode: python -*-
#======================================================================
# FILE: Component.py
# CREATOR: eric 
#
# DESCRIPTION:
#   
#
#  $Id: Component.py,v 1.1 2001/11/15 19:27:40 mikep%oeone.com Exp $
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
from Property import *
from Collection import *

class Component:

    def __init__(self,str=None, component_kind="ANY", ref=None):

        if ref != None: 
            self._ref = ref
        else:
            self._ref = None
            if str != None:
                self._ref = icalparser_parse_string(str)
            else:
                kind = icalenum_string_to_component_kind(component_kind)
                self._ref = icalcomponent_new(kind)
       
        self.cached_props = {}

    def __del__(self):
        if self._ref != None and \
           icalcomponent_get_parent(self._ref) != None:

            for k in self.cached_props.keys():
                del self.cached_props[k]
            
            icalcomponent_free(self._ref)
            self._ref = None

    def _prop_from_ref(self,p):

        d_string = icallangbind_property_eval_string(p,":")
        d = eval(d_string)
        d['ref'] = p
        
        if not self.cached_props.has_key(p):
            
            if d['value_type'] == 'DATE-TIME' or d['value_type'] == 'DATE':
                prop = Time(d,)
            elif d['value_type'] == 'PERIOD':
                prop = Period(d)
            elif d['value_type'] == 'DURATION':
                prop = Duration(d)
            elif d['name'] == 'ATTACH':
                prop = Attach(d)
            elif d['name'] == 'ATTENDEE':
                prop = Attendee(d)
            elif d['name'] == 'ORGANIZER':
                prop = Organizer(d)
            else:
                prop=Property(ref=p)
                
            self.cached_props[p] = prop

    def property(self, type):

       p = icallangbind_get_first_property(self._ref,type) 

       if p !='NULL':
           self._prop_from_ref(p)
           prop =  self.cached_props[p]
           return prop
       else :    
           return None

    def properties(self,type='ANY'): 
        """  
        Return a list of Property instances, each representing a
        property of the type 'type.'
        """

        props = []

        p = icallangbind_get_first_property(self._ref,type)

        while p !='NULL':
            self._prop_from_ref(p)
            prop =  self.cached_props[p]
            props.append(prop)
            p = icallangbind_get_next_property(self._ref,type)

        return Collection(self,props)
 
    def add_property(self, prop):
        "Adds the property object to the component."
        
        if not isinstance(prop,Property):
            raise TypeError

        prop_p = prop.ref()

        if not prop_p:
            s = str(prop)
            prop_p = icalproperty_new_from_string(s)

            if prop_p == 'NULL':
                raise "Bad property string: " + s

            prop.ref(prop_p)

        if icalproperty_get_parent(prop_p)=='NULL':
            icalcomponent_add_property(self._ref, prop_p)
        elif  icalproperty_get_parent(prop_p) != self._ref:
            raise "Property is already a child of another component"


    def remove_property(self,prop):

        if prop.ref() and self.cached_props.has_key(prop.ref()):

            del self.cached_props[prop.ref()]
            icalcomponent_remove_property(self._ref,prop.ref())

    def components(self,type='ANY'):        
        comps = []

        return comps

    def add_component(self, componentObj):
        "Adds a child component."
        pass
        

    def remove_component(self, component):
        "Removes a child component"
        pass

    def as_ical_string(self):
        return self.__str__()

    def __str__(self):

        return icalcomponent_as_ical_string(self._ref)



def NewComponent(comp):
    "Converts a string or C icalcomponent into the right component object."

    wasStr=0 # Were we passed a string or an icalcomponent?

    if isinstance (comp, StringType):
        compStr = comp
        comp = icalparser_parse_string(comp)
        wasStr=1
    else:
        compStr = icalcomponent_as_ical_string(comp)

    kind = icalcomponent_isa(comp)
    kindStr = icalenum_component_kind_to_string(kind)
    # Do I need to free kind? (I think not).

    if kindStr == 'VEVENT':
        newComp = Event(compStr)
    elif kindStr == 'VTODO':
        newComp = Todo(compStr)
    elif kindStr == 'VJOURNAL':
        newComp = Journal(compstr)
    else:
        newComp = Component(compStr)

    # I don't think I need to free the component created when passed a string,
    # as it wasn't created with a _new function.

    return newComp


class GenericComponent(Component):

    def __init__(self):
                
        # Component.__init__(self, str) # Call from subclasses
        self._recurrence_set=None

    def _singular_property(self, name, value_type, value=None,
                           property_obj=None, enumerated_values=None):
        """Sets or gets the value of a method which exists once per Component.

        This is a constructor method for properties without a strictly defined
        object."""

        curr_properties = self.properties(name)

        # Get the value
        if value==None:
            if len(curr_properties) == 0:
                return None
            elif len(curr_properties) == 1:
                return curr_properties[0].value()
            else:
                raise ValueError, "too many properties of type %s" % propType

        # Set the value
        else:
            # Check if value is in enumerated_values
            if enumerated_values:
                value = upper(value)
                if value not in enumerated_values:
                    raise ValueError, "%s is not one of %s" \
                          % (value, enumerated_values)

            # Create the new property
            if property_obj:
                if not isinstance(value, property_obj):
                    # Create a special property_obj property
                    if property_obj == Time:
                        p = Time(value, name)
                        ## p.value_type(value_type)
                    else:
                        p = property_obj()
                        ## p.value_type(value_type)
                        p.value(value)
                else:
                    p = value # value is already a property_obj
            else:
                # Create a generic property
                p = Property(name)
                ## p.value_type(value_type)
                p.value(value)

            if len(curr_properties) == 1:
                self.remove_property(curr_properties[0])
            elif len(curr_properties) > 1:
                raise ValueError, "too many properties of type %s" % propType

            self.add_property(p)
            
    def method(self, v=None):
        "Sets or returns the value of the METHOD property."
        return self._singular_property("METHOD", "TEXT", v)

    def prodid(self, v=None):
        "Sets or returns the value of the PRODID property."
        return self._singular_property("PRODID", "TEXT", v)

    def calscale(self, v=None):
        "Sets or returns the value of the CALSCALE property."
        return self._singular_property("CALSCALE", "TEXT", v)

    def class_prop(self, v=None):  # Class is a reserved word
        "Sets or returns the value of the CLASS property."
        if v!=None:
            v = upper(v)
        return self._singular_property('CLASS', 'TEXT', v)

    def created(self, v=None):
        """Sets or returns the value of the CREATED property.

        Usage:
        created(time_obj)           # Set the value using a Time object
        created('19970101T123000Z') # Set using an iCalendar string
        created(982362522)          # Set using seconds 
        created()                   # Return an iCalendar string
        """
        return self._singular_property("CREATED", "DATE-TIME", v, Time)
        
    def description(self, v=None):
        "Sets or returns the value of the DESCRIPTION property."
        return self._singular_property("DESCRIPTION", "TEXT", v)

    def dtstamp(self, v=None):
        """Sets or returns the value of the DTSTAMP property.

        Usage:
        dtstamp(time_obj)          # Set the value using a Time object
        dtstamp('19970101T123000Z')# Set using an iCalendar string
        dtstamp(982362522)         # Set using seconds 
        dtstamp()                  # Return an iCalendar string
        """
        return self._singular_property("DTSTAMP", "DATE-TIME", v, Time)

    def dtstart(self, v=None):
        """Sets or returns the value of the DTSTART property.

        Usage:
        dtstart(time_obj)           # Set the value using a Time object
        dtstart('19970101T123000Z') # Set the value as an iCalendar string
        dtstart(982362522)          # Set the value using seconds (time_t)
        dtstart()                   # Return the time as an iCalendar string
        """
        return self._singular_property("DTSTART", "DATE-TIME", v, Time)

    def last_modified(self, v=None):
        """Sets or returns the value of the LAST-MODIFIED property.

        Usage:
        lastmodified(time_obj)          # Set the value using a Time object
        lastmodified('19970101T123000Z')# Set using an iCalendar string
        lastmodified(982362522)         # Set using seconds 
        lastmodified()                  # Return an iCalendar string
        """
        return self._singular_property("LAST-MODIFIED", "DATE-TIME", v, Time)

    def organizer(self, v=None):
        """Sets or gets the value of the ORGANIZER property.

        Usage:
        organizer(orgObj)              # Set value using an organizer object
        organizer('MAILTO:jd@not.com') # Set value using a CAL-ADDRESS string
        organizer()                    # Return a CAL-ADDRESS string
        """
        return self._singular_property('ORGANIZER', 'CAL-ADDRESS', v,
                                       Organizer)

    def recurrence_id(self, v=None):
        """Sets or gets the value for the RECURRENCE-ID property.

        Usage:
        recurrence_id(recIdObj)             # Set using a Recurrence_Id object
        recurrence_id("19700801T133000")    # Set using an iCalendar string
        recurrence_id(8349873494)           # Set using seconds from epoch
        recurrence_id()                     # Return an iCalendar string
        """
        return self._singular_property('RECURRENCE-ID', 'DATE-TIME', v,
                                       Recurrence_Id)

    def sequence(self, v=None):
        """Sets or gets the SEQUENCE value of the Event.

        Usage:
        sequence(1)     # Set the value using an integer
        sequence('2')   # Set the value using a string containing an integer
        sequence()      # Return an integer
        """
        if isinstance(v, StringType):
            v = int(str)
        return self._singular_property('SEQUENCE', 'INTEGER', v)

    def summary(self, v=None):
        "Sets or gets the SUMMARY value of the Event."
        return self._singular_property('SUMMARY', 'TEXT', v)

    def uid(self, v=None):
        "Sets or gets the UID of the Event."
        return self._singular_property('UID', 'TEXT', v)

    def url(self, v=None):
        """Sets or returns the URL property."""
        return self._singular_property('URL', 'URI', v)

    ####
    # Not quite sure if this is how we want to handle recurrence rules, but
    # this is a start.
    
    def recurrence_set(self):
        "Returns the Events RecurrenceSet object."
        if self._recurrence_set == None:  # i.e haven't initialized one
            self._recurrence_set = RecurrenceSet()
        return self._recurrence_set

    ###
    # Alarm interface.  Returns an ComponentCollection.

    def alarms(self, values=None):
        """Sets or returns ALARM components.

        Examples:
        alarms((alarm1,))   # Set using Alarm component
        alarms()            # Returns an ComponentCollection of all Alarms
        """
        if values!=None:
            for alarm in values:
                self.addComponent(alarm)
        else:
            return ComponentCollection(self, self.components('VALARM'))

    ####
    # Methods that deal with Properties that can occur multiple times are
    # below.  They use the Collection class to return their Properties.

    def _multiple_properties(self, name, value_type, values,
                             property_obj=None):
        "Processes set/get for Properties that can have multiple instances."

        # Set value
        if values!=None:
            if not isinstance(values, TupleType) \
               and not isinstance(values, ListType):
                raise TypeError, "%s is not a tuple or list."

            # Delete old properties
            for p in self.properties(name):
                self.remove_property(p)
                
            for v in values:
                if property_obj:   # Specialized properties
                    if not isinstance(v, property_obj): # Make new object
                        new_prop = property_obj()
                        new_prop.value(v)
                    else:                            # Use existing object
                        new_prop = v
                else:                  # Generic properties
                    new_prop= Property() 
                    new_prop.name(name)
                    # new_prop.value_type(value_type)
                    new_prop.value(v)
                    
                self.add_property(new_prop)
        
        # Get value
        else:
            return Collection(self, self.properties(name))

    def attachments(self, values=None):
        """Sets or returns a Collection of Attach properties.

        'values' can be a sequence containing URLs (strings) and/or file-ish
        objects.
        """
        return self._multiple_properties("ATTACH", "", value, Attach)

    def attendees(self, value=None):
        """Sets attendees or returns a Collection of Attendee objects.

        If setting the attendees, pass a sequence as the argument.
        Examples:
        # Set using Attendee objects
        attendees((attObj1, attObj2))
        # Set using a CAL-ADDRESS string
        attendees(['MAILTO:jdoe@somewhere.com'])
        # Set using a combination of Attendee objects and strings
        attendees(['MAILTO:jdoe@somewhere.com', attObj1])
        # Returns a list of Attendee objects
        attendees()

        When setting the attendees, any previous Attendee objects in the Event
        are overwritten.  If you want to add to the Attendees, one way to do it
        is:

        attendees().append(Attendee('MAILTO:jdoe@nothere.com'))
        """
        return self._multiple_properties("ATTENDEE", "", value, Attendee)

    def categories(self, value=None):
        """Sets categories or returns a Collection of CATEGORIES properties.

        If setting the categories, pass a sequence as the argument.
        Examples:
        # Set using string[s]
        categories(('APPOINTMENT', 'EDUCATION'))
        # Returns a list of Category properites
        categories()

        When setting the attendees, any previous category Properties in the
        Event are overwritten.  If you want to add to the categories, one way
        to do it is:

        new_cat=Property('CATEGORIES')
        new_cat.value_type('TEXT')
        new_cat.value('PERSONAL')
        categories().append(new_cat)
        """
        return self._multiple_properties("CATEGORIES", "TEXT", value)

    def comments(self, value=None):
        "Sets or returns a Collection of COMMENT properties."
        return self._multiple_properties('COMMENT', 'TEXT', value)

    def contacts(self, value=None):
        "Sets or returns a Collection of CONTACT properties."
        return self._multiple_properties('CONTACT', 'TEXT', value)

    def related_tos(self, value=None):
        "Sets or returns a Collectoin of RELATED-TO properties."
        return self._multiple_properties('RELATED-TO', 'TEXT', value)


class Event(GenericComponent):
    "The iCalendar Event object."

    def __init__(self, str=None):
        Component.__init__(self, str, "VEVENT")
        GenericComponent.__init__(self)
        
    def component_type(self):
        "Returns the type of component for the object."
        return "VEVENT"

    def clone(self):
        "Returns a copy of the object."
        return Event(self.asIcalString())

    def dtend(self, v=None):
        """Sets or returns the value of the DTEND property.

        Usage:
        dtend(time_obj)             # Set the value using a Time object
        dtend('19970101T123000Z')   # Set the value as an iCalendar string
        dtend(982362522)            # Set the value using seconds (time_t)
        dtend()                     # Return the time as an iCalendar string

        If the dtend value is being set and duration() has a value, the
        duration property will be removed.
        """
        if v != None:
            duration = self.properties('DURATION')
            for d in duration:          # Clear DURATION properties
                self.remove_property(d)
        return self._singular_property("DTEND", "DATE-TIME", v, Time)
            
    def duration(self, v=None):
        """Sets or returns the value of the duration property.

        Usage:
        duration(dur_obj)       # Set the value using a Duration object
        duration("P3DT12H")     # Set value as an iCalendar string
        duration(3600)          # Set duration using seconds
        duration()              # Return duration as an iCalendar string

        If the duration value is being set and dtend() has a value, the dtend
        property will be removed.
        """

        if v != None:
            dtend = self.properites('DTEND')
            for d in dtend:
                self.remove_property(d)  # Clear DTEND properties
        return self._singular_property("DURATION", "DURATION", v, Duration)

    def status(self, v=None):
        "Sets or returns the value of the STATUS property."

        # These values are only good for VEVENT components (i.e. don't copy
        # & paste into VTODO or VJOURNAL
        valid_values=('TENTATIVE', 'CONFIRMED', 'CANCELLED')
        return self._singular_property("STATUS", "TEXT", v,
                                       enumerated_values=valid_values)

    def geo(self, v=None):
        """Sets or returns the value of the GEO property.

        Usage:
        geo(value) or
        geo()           # Returns the icalendar string

        'value' is either a icalendar GEO string or a sequence with two 'float'
        numbers.

        Examples:
        geo('40.232;-115.9531')     # Set value using string
        geo((40.232, -115.9531))    # Set value using a sequence
        geo()                       # Returns "40.232;-115.9531"

        To get the GEO property represented as a tuple and numbers instead of
        the iCalendar string, use geo_get_tuple().
        """
        
        if isinstance(v, ListType) or isinstance(v, TupleType):
            v = "%s;%s" % (float(v[0]), float(v[1]))
        return self._singular_property("GEO", "FLOAT", v)

    def geo_get_tuple(self):
        """Returns the GEO property as a tuple."""

        geo = self.geo()
        geo = split(geo, ';')
        return float(geo[0]), float(geo[1])

    def location(self, v=None):
        """Sets or returns the LOCATION property."""
        return self._singular_property("LOCATION", "TEXT", v)

    def transp(self, v=None):
        """Sets or returns the TRANSP property."""
        ok_values = ('OPAQUE', 'TRANSPARENT')
        return self._singular_property('TRANSP', 'TEXT', v,
                                       enumerated_values=ok_values)

    def resources(self, v=None):
        pass

class Todo(GenericComponent):
    "The iCalendar TODO component."

    def component_type(self):
        "Returns the type of component for the object."
        return "VTODO"

    def clone(self):
        "Returns a copy of the object."
        return Todo(self.asIcalString())

    def completed(self, value=None):
        return self._singular_property('COMPLETED', 'DATE-TIME', value, Time)

    def geo(self, value=None):
        if isinstance(v, ListType) or isinstance(v, TupleType):
            v = "%s;%s" % (float(v[0]), float(v[1]))
        return self._singular_property("GEO", "FLOAT", value)

    def location(self, value=None):
        return self._singular_property('LOCATION', 'TEXT', value)

    def percent(self, value=None):
        if value!=None:
            value = str(int(value))
        return self._singular_property('PERCENT', 'INTEGER', value)

    def status(self, value=None):
        if value!=None:
            value=upper(value)
        ok_values = ('NEEDS-ACTION', 'COMPLETED', 'IN-PROCESS', 'CANCELLED')
        return self._singular_property('STATUS', 'TEXT', value,
                                       enumerated_values=ok_values)

    def due(self, value=None):
        if value != None:
            duration = self.properties('DURATION')
            for d in duration:
                self.remove_property(d) # Clear DURATION properties
        return self._singular_property('DUE', 'DATE-TIME', value, Time)

    def duration(self, value=None):
        if value != None:
            due = self.properites('DUE')
            for d in due:
                self.remove_property(d)  # Clear DUE properties
        return self._singular_property("DURATION", "DURATION", value, Duration)

    def resources():
        pass
    

class Journal(GenericComponent):
    "The iCalendar JOURNAL component."

    def component_type(self):
        "Returns the type of component for the object."
        return "VJOURNAL"

    def clone(self):
        "Returns a copy of the object."
        return Journal(self.asIcalString())

    def status(self, v=None):
        if v!=None:
            v = upper(v)
        ok_values=('DRAFT', 'FINAL', 'CANCELLED')
        return self._singular_property('STATUS', 'TEXT', v,
                                       enumerated_values=ok_values)
        
