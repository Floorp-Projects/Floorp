#!/usr/bin/perl

use lib "../blib/lib";
use lib "../blib/arch";
use lib "../";

use Time::Local;

use Net::ICal::Libical;

use POSIX; 


  my $comp_str=<<EOM;
BEGIN:VCALENDAR
METHOD
 :PUBLISH
PRODID
 :-//ACME/DesktopCalendar//EN
VERSION
 :2.0
BEGIN:VEVENT
ORGANIZER
 :mailto:a\@example.com
ATTENDEE
 ;RSVP=TRUE
 ;ROLE=REQ-PARTICIPANT
 ;CUTYPE=GROUP
 :MAILTO:employee-A\@host.com
DTSTART
 :19970701T200000Z
DURATION
 :P3DT4H50M36S
DTSTAMP
 :19970611T190000Z
SUMMARY
 :ST. PAUL SAINTS -VS- DULUTH-SUPERIOR DUKES
UID
 :0981234-1234234-23\@example.com
END:VEVENT
END:VCALENDAR         
EOM
  
my $c;

$c = new Net::ICal::Libical::Component($comp_str);

my @props = $c->properties();

my $p;
foreach $p (@props) {
  print $p->name()." ".$p->value()."\n";
  
}

$inner = ($c->components())[0];

print "\n";

print " -------- Attendee \n";

$p = ($inner->properties('ATTENDEE'))[0];

print $p->as_ical_string(),"\n";

print $p->get_parameter('ROLE'),"\n";

die if $p->get_parameter('ROLE') ne 'REQ-PARTICIPANT';

$p->set_parameter('ROLE','INDIVIDUAL');

print $p->as_ical_string(),"\n";

print " -------- DTSTART \n";

$p = ($inner->properties('DTSTART'))[0];

print $p->as_ical_string()."\n";
print $p->as_ical_string()."\n";

print "hour: ". $p->hour()." \n";

$p->hour($p->hour() - 10);

print $p->hour(),"\n";

$p->timezone('America/Los_Angeles');

print $p->as_ical_string()."\n";


print "----------- DURATION \n";

$p = ($inner->properties('DURATION'))[0];

print $p->as_ical_string()."\n";

print $p->seconds(),"\n";

$p->seconds(3630);

print $p->as_ical_string()."\n";
print $p->seconds(),"\n";



