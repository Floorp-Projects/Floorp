#!/usr/bin/perl

use lib "../blib/lib";
use lib "../blib/arch";
use lib "../";

use Time::Local;

use Net::ICal::Libical;

use POSIX; 


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

my $comp_str_error=<<EOM;
BEGIN:VCALENDAR
METHOD:REQUEST
VERSION:2.0
PRODID:-//ACME/DesktopCalendar//EN
BEGIN:VEVENT
ORGANIZER:mailto:a\@example.com
DTSTAMP:19970612T190000Z
DTSTART:19970701T210000Z
DTEND:19970701T230000Z
SEQENCE:1
UID:0981234-1234234-23\@example.com
SUMMARY:ST. PAUL SAINTS -VS- DULUTH-SUPERIOR DUKES
END:VEVENT
END:VCALENDAR
EOM

print "-- Good Component --\n";
print Net::ICal::Libical::validate_component($comp_str);

print "-- BadComponent --\n";
print Net::ICal::Libical::validate_component($comp_str_error);


print "-- Generate Occurrences --\n";
$rule = "FREQ=MONTHLY;UNTIL=19971224T000000Z;INTERVAL=1;BYDAY=TU,2FR,3SA";
$limit = 25;
$start = timelocal(0,0,9,5,8,1997); # 19970905T090000Z

@occur = Net::ICal::Libical::generate_occurrences($rule,$start,$limit);

print $rule."\n";

foreach $i (@occur){

  my ($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = gmtime($i);

  printf("%s %s %2d %02d:%02d:%02d %d\n",
	 (Sun,Mon,Tue,Wed,Thur,Fri,Sat)[$wday],
	 (Jan,Feb,Mar,Apr,May,Jun,Jul,Aug,Sep,Oct,Nov,Dec)[$mon],
	 $mday, 
	 $hour,$min,$sec,
	 $year);

}

print "-- Interpret iCal data --\n";


my $comp_str=<<EOM;
BEGIN:VEVENT
ORGANIZER:mailto:a\@example.com
DTSTAMP:19970612T190000Z
DTSTART:19970701T210000Z
DTEND:19970701T230000Z
SEQUENCE:1
UID:0981234-1234234-23\@example.com
SUMMARY:ST. PAUL SAINTS -VS- DULUTH-SUPERIOR DUKES
END:VEVENT

EOM



