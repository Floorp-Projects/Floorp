#!/usr/bin/env python 
# -*- Mode: python -*-
#======================================================================
# FILE: test.py
# CREATOR: eric 
#
# DESCRIPTION:
#   
#
#  $Id: test.py,v 1.1 2001/11/15 19:27:43 mikep%oeone.com Exp $
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

from Libical import *

def error_type():
    error = icalerror_perror()
    return error[:index(error,':')]

comp_str = """
BEGIN:VEVENT
ATTENDEE;RSVP=TRUE;ROLE=REQ-PARTICIPANT;CUTYPE=GROUP:MAILTO:employee-A@host.com
COMMENT: When in the course of writting comments and nonsense text\, it 
 becomes necessary to insert a newline
DTSTART:19972512T120000
DTSTART:19970101T120000Z
DTSTART:19970101
DURATION:P3DT4H25M
FREEBUSY:19970101T120000/19970101T120000
FREEBUSY:19970101T120000/PT3H
FREEBUSY:19970101T120000/PT3H
END:VEVENT"""


def test_property():

    print "--------------------------- Test Property ----------------------"
    
    liw = LibicalWrap
    icalprop = liw.icalproperty_new_from_string("ATTENDEE;RSVP=TRUE;ROLE=REQ-PARTICIPANT;CUTYPE=GROUP:MAILTO:employee-A@host.com")

    print liw.icalproperty_as_ical_string(icalprop)

    p = Property(ref=icalprop)

    print p.name()
    print p.parameters()
    print p['ROLE']
    
    p['ROLE'] = 'INDIVIDUAL'

    print p['ROLE']

    print p.value()
    p.value("mailto:Bob@bob.com")
    print p.value()


    print p.as_ical_string()

    try:
        p = Property()
    except Property.ConstructorFailedError:
        pass
    else:
        assert(0)

def test_time():
    "Test routine"

    print"-------------------Test Time  --------------------------------"

    t = Time("19970325T123010Z",'DTSTART')
    
    assert(t.year() == 1997)
    assert(t.month() == 3)
    assert(t.day() == 25)
    assert(t.hour() == 12)
    assert(t.minute() == 30)
    assert(t.second() == 10)
    assert(t.is_utc())
    assert(not t.is_date())
    
    print t

    t.timezone("America/Los_Angeles")
    print str(t)
    assert(str(t)=='DTSTART;TZID=America/Los_Angeles:19970325T123010Z')

    t.second(t.second()+80)

    print t

    assert(t.minute() == 31)
    assert(t.second() == 30)

    d = Duration(3600,"DURATION")
    t2 = t + d

    print t2
    assert(t2.hour() == 13)

    t2 = t - d

    print t2
    assert(isinstance(t2,Time))
    assert(t2.hour() == 11)


def test_period():    

    print"-------------------Test Period--------------------------------"

    p = Period("19970101T180000Z/19970101T233000Z")

    print p
    

    assert(str(p) == 'FREEBUSY:19970101T180000Z/19970101T233000Z')

    print p.start()
    assert(str(p.start()) == 'DTSTART:19970101T180000Z')

    print p.end()
    assert(str(p.end()) == 'DTEND:19970101T233000Z')

    print p.duration()
    assert(str(p.duration()) == 'DURATION:PT5H30M')
    p = None

    p = Period("19970101T180000Z/PT5H30M")
    print p

    print p.start()
    assert(str(p.start()) == 'DTSTART:19970101T180000Z')

    print p.end()
    assert(str(p.end()) == 'DTEND:19970101T233000Z')

    print p.duration()
    assert(str(p.duration()) == 'DURATION:PT5H30M')


def test_duration():

    print "-------------- Test Duration ----------------"

    # Ical string

    d = Duration("P3DT4H25M")

    print str(d)

    assert(str(d) == "DURATION:P3DT4H25M")

    print d.seconds()

    assert(d.seconds() == 275100)

    # seconds

    d = Duration(-275100)
           
    print str(d)

    assert(str(d) == "DURATION:-P3DT4H25M")

    print d.seconds()

    assert(d.seconds() == -275100)

    #error

    try:
        d = Duration("P10WT7M")
        print str(d)
        assert(0)
    except: pass

    try:
        d = Duration("Pgiberish")
        print str(d)
        assert(0)
    except:
        pass



def test_attach():

    file = open('littlefile.txt')
    attachProp = Attach(file)
    file.close()
    attachProp.fmttype('text/ascii')
    print "\n" + attachProp.name()
    print attachProp.value_type()
    print attachProp.fmttype()
    attachProp['fmttype']=None
    print "Calling value()"
    print attachProp.value()
    print "Calling asIcalString()"
    print attachProp.as_ical_string()


def test_component():

    print "------------------- Test Component ----------------------"


    c = Component(comp_str);
    
    props = c.properties()
    
    for p in props: 
        print p.as_ical_string()
        
    dtstart = c.properties('DTSTART')[0]
        
    print dtstart
    
    print "\n Orig hour: ", dtstart.hour()
    assert(dtstart.hour() == 12)

    dtstart.hour(dtstart.hour() + 5)

    print "\n New hour: ", dtstart.hour()
    assert(dtstart.hour() == 17)

    attendee = c.properties('ATTENDEE')[0]
    
    print attendee

    t = Time("20011111T123030")
    t.name('DTEND')

    c.add_property(t)


    print c

    dtstart1 = c.properties('DTSTART')[0]
    dtstart2 = c.properties('DTSTART')[0]
    dtstart3 = c.property('DTSTART')

    assert(dtstart1 is dtstart2)
    assert(dtstart1 == dtstart2)

    assert(dtstart1 is dtstart3)
    assert(dtstart1 == dtstart3)


    p = Property(type="SUMMARY");
    p.value("This is a summary")

    c.properties().append(p)

    print c.as_ical_string()

    p = c.properties("SUMMARY")[0]
    assert(p!=None);
    print str(p)
    assert(str(p) == "SUMMARY:This is a summary")

    c.properties()[:] = [p]

    print c.as_ical_string()


def test_event():
    print "------------ Event Class ----------------------"
    event = Event()
    event.created("20010313T123000Z")
    #print "created =", event.created()
    assert (event.created() == "20010313T123000Z")

    event.organizer("MAILTO:j_doe@nowhere.com")
    org = event.properties('ORGANIZER')[0]
    #print org.cn()
    org.cn('Jane Doe')
    assert (isinstance(org, Organizer))
    #print "organizer =", event.organizer()
    assert (event.organizer() == "MAILTO:j_doe@nowhere.com")

    event.dtstart("20010401T183000Z")
    #print "dtstart =", event.dtstart()
    assert (event.dtstart()=="20010401T183000Z")

    dtend = Time('20010401T190000Z', 'DTEND')
    event.dtend(dtend)
    assert (event.dtend()==dtend.value())
    assert (event.dtend() == '20010401T190000Z')
    
    att = Attendee()
    att.value('jsmith@nothere.com')
    event.attendees(('ef_hutton@listenup.com', att))

    event.description("A short description.  Longer ones break things.")
    event.status('TeNtAtIvE')
    
    print event.as_ical_string()
    
    
def test_derivedprop():
    
    print "------------ Derived Properties -----------------"
    
    p = RDate("20011111T123030")

    print str(p)


    p = RDate("19970101T120000/19970101T123000")

    print str(p)

    try:
        p = RDate("P3DT4H25M")
        print str(p)
        assert(0)
    except: pass

    
    p = Trigger("P3DT4H25M")

    print str(p)

    p = Trigger("20011111T123030")

    print str(p)

    try:
        p = Trigger("19970101T120000/19970101T123000")
        print str(p)
        assert(0)
    except: pass


def run_tests():
    test_property()

    test_time()

    test_period()

    test_component()

    test_duration()

    test_derivedprop()

    test_event()

    #test_attach()



if __name__ == "__main__":
    run_tests()

