#!/usr/bin/perl

use lib "../blib/lib";
use lib "../blib/arch";
use lib "../";

use Net::ICal::Libical;


my $comp_str=<<EOM;
BEGIN:VCALENDAR
METHOD:PUBLISH
VERSION:2.0
PRODID:-//ACME/DesktopCalendar//EN
BEGIN:VEVENT
ORGANIZER:mailto:a\@example.com
DTSTAMP:19970612T190000Z
DTSTART:19970701T210000Z
DTEND:19970701T230000Z
SEQUENCE:1
UID:0981234-1234234-23\@example.com
SUMMARY:ST. PAUL SAINTS -VS- DULUTH-SUPERIOR DUKES
END:VEVENT
END:VCALENDAR
EOM

$c = Net::ICal::Libical::icalparser_parse_string($comp_str);   

print Net::ICal::Libical::icalcomponent_as_ical_string($c)."\n";
