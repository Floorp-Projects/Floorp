#!/usr/bin/perl -w
# -*- Mode: perl -*-
#======================================================================
#
# This package is free software and is provided "as is" without express
# or implied warranty.  It may be used, redistributed and/or modified
# under the same terms as perl itself. ( Either the Artistic License or the
# GPL. ) 
#
# $Id: Period.pm,v 1.1 2001/12/21 19:21:20 mikep%oeone.com Exp $
#
# (C) COPYRIGHT 2000, Eric Busboom, http://www.softwarestudio.org
#
# $Log
#======================================================================


=pod
=head1 NAME

Net::ICal::Period -- represent a period of time

=head1 SYNOPSIS

  use Net::ICal;
  $p = new Net::ICal::Period("19970101T120000","19970101T123000");
  $p = new Net::ICal::Period("19970101T120000","PT3W2D40S");
  $p = new Net::ICal::Period(time(),3600);
  $p =   new Net::ICal::Period(
		      new Net::ICal::Time("19970101T120000",
					  "America/Los_Angeles"),
		      new Net::ICal::Duration("2h"));

=head1 DESCRIPTION

Use this to make an object representing a block of time on a
real schedule. You can either say, "This event starts at 12
and ends at 2" or "This event starts at 12 and lasts 2 hours."

These two ways of specifying events can be treated differently
in schedules. If you say, "The meeting is from 12 to 2, but I 
have to leave at 2," you are implying that the start date and
end date are fixed. If you say, "I have a 2-hour drive to
Chicago, and I need to leave at 4," you are saying that it will
take 2 hours no matter when you leave, and that moving the start
time will slide the end time correspondingly. 

=head1 BASIC METHODS

=cut


#=========================================================================

package Net::ICal::Period;
use strict;
use Net::ICal::Time;
use Net::ICal::Duration;

use UNIVERSAL qw(isa);

#-------------------------------------------------------------------------

=pod
=head2 new($time, $time|$duration)

Creates a new period object given to parameters: The first must be a
I<Time> object or valid argument to Net::ICal::Time::new.

The second can be either: 

=pod

=over 4

=item * a I<Time> object

=item * a valid argument to Net::ICal::Time::new. 

=item * a I<Duration> object

=item * a valid argument to Net::ICal::Duration::new. 

=back 

Either give a start time and an end time, or a start time and a duration.

=cut

sub new{
  my $package = shift;
  my $arg1 = shift;
  my $arg2 = shift;
  my $self = {};

  # Is the string in RFC2445 Format?
  if(!$arg2 and $arg1 =~ /\//){
    my $tmp = $arg1;
    ($arg1,$arg2) = split(/\//,$tmp);
  }


  if( ref($arg1) eq 'Net::ICal::Time'){
    $self->{START} = $arg1->clone();
  } else  {
    $self->{START} = new Net::ICal::Time($arg1);
  } 
    

  if(isa($arg2,'Net::ICal::Time')){ 
    $self->{END} = $arg2->clone();
  } elsif (isa($arg2,'Net::ICal::Duration')) {
    $self->{DURATION} = $arg2->clone();
  } elsif ($arg2 =~ /^P/) {
    $self->{DURATION} = new Net::ICal::Duration($arg2);
  } else {
    # Hope that it is a time string
    $self->{END} = new Net::ICal::Time($arg2);
  }

  return bless($self,$package);
}

#--------------------------------------------------------------------------
=pod
=head2 clone()

Create a copy of this component

=cut
# XXX implement this
sub clone {
    return "Not implemented";
}

#----------------------------------------------------------------------------
=pod
=head2 is_valid()

Return true if:
  There is an end time and:
     Both start and end times have no timezone ( Floating time) or
     Both start and end time have (possibly different) timezones or
     Both start and end times are in UTC and
     The end time is after the start time. 

  There is a duration and the duration is positive  

=cut

# XXX implement this

sub is_valid {
    return "Not implemented";
}

#---------------------------------------------------------------------------
=pod
=head2 start([$time])

Accessor for the start time of the event as a I<Time> object.
Can also take a valid time string or an integer (number of
seconds since the epoch) as a parameter. If a second parameter
is given, it'll set this Duration's start time. 

=cut

sub start{
  my $self = shift;
  my $t = shift;

  if($t){
    if(isa($t,'Net::ICal::Time')){ 
      $self->{START} = $t->clone();
    } else {
      $self->{START} = new Net::ICal::Time($t);
    }
  }

  return $self->{START};
} 

#-----------------------------------------------------------------
=pod
=head2 end([$time])

Accessor for the end time. Takes a I<Time> object, a valid time string,
or an integer and returns a time object. This routine is coupled to 
the I<duration> accessor. See I<duration> below for more imformation. 

=cut

sub end{

  my $self = shift;
  my $t = shift;
  my $end;

  if($t){
    if(isa($t,'Net::ICal::Time')){
      $end = $t->clone();
    } else {
      $end = new Net::ICal::Time($t);
    }
    
    # If duration exists, use the time to compute a new duration
    if ($self->{DURATION}){
      $self->{DURATION} = $end->subtract($self->{START}); 
    } else {
      $self->{END} = $end;
    }    
  }

  # Return end time, possibly computing it from DURATION
  if($self->{DURATION}){
    return $self->{START}->add($self->{DURATION});
  } else {
    return $self->{END};
  }

} 

#----------------------------------------------------------------------
=pod
=head2 duration([$duration])

Accessor for the duration of the event. Takes a I<duration> object and
returns a I<Duration> object. 

Since the end time and the duration both specify the end time, the
object will store one and access to the other will be computed. So,

if you create:

   $p = new Net::ICal::Period("19970101T120000","19970101T123000")

And then execute:

   $p->duration(45*60);

The period object will adjust the end time to be 45 minutes after
the start time. It will not replace the end time with a
duration. This is required so that a CUA can take an incoming
component from a server, modify it, and send it back out in the same
basic form.

=cut

sub duration{
  my $self = shift;
  my $d = shift;
  my $dur;

  if($d){
    if(isa($d,'Net::ICal::Duration')){ 
      $dur = $d->clone();
    } else {
      $dur = new Net::ICal::Duration($d);
    }
    
    # If end exists, use the duration to compute a new end
    # otherwise, set the duration. 
    if ($self->{END}){
      $self->{END} = $self->{START}->add($dur); 
    } else {
      $self->{DURATION} = $dur;
    }    
  }

  # Return duration, possibly computing it from END
  if($self->{END}){
    return $self->{END}->subtract($self->{START});
  } else {
    return $self->{DURATION};
  }

}

#------------------------------------------------------------------------
=pod

=head2 as_ical()

Return a string that holds the RFC2445 text form of this duration

=cut
sub as_ical {
  my $self = shift;
  my $out;

  $out = $self->{START}->as_ical() ."/";

  if($self->{DURATION}){
    $out .= $self->{DURATION}->as_ical() 
  } else {
    $out .= $self->{END}->as_ical() 
  }
 
  return $out;
 
}


#------------------------------------------------------------------------
=pod

=head2 test()

A set of developers' tests to make sure the module's working properly.

=cut

# Run this with a one-liner:
#   perl -e "use lib('/home/srl/dev/rk/reefknot/base/'); use Net::ICal::Period; Net::ICal::Period::test();"
# adjusted for your environment. 
sub test {

  print("--------- Test Net::ICal::Period --------------\n");


  my $p = new Net::ICal::Period("19970101T180000Z/19970102T070000Z");
  print $p->as_ical()."\n";
  die if $p->as_ical() ne "19970101T180000Z/19970102T070000Z";

  $p = new Net::ICal::Period("19970101T180000Z/PT5H30M");
  print $p->as_ical()."\n";
  die if $p->as_ical() ne "19970101T180000Z/PT5H30M";

  $p->duration("PT5H30M10S");
  print $p->as_ical()."\n";
  die if $p->as_ical() ne "19970101T180000Z/PT5H30M10S" ;

  $p->duration(new Net::ICal::Duration("P10DT30M5S"));
  print $p->as_ical()."\n";
  die if $p->as_ical() ne "19970101T180000Z/P10DT30M5S" ;

  $p->end("19970101T183000Z");
  print $p->as_ical()."\n";
  die if $p->as_ical() ne "19970101T180000Z/PT30M" ;

  $p = new Net::ICal::Period("19970101T180000Z/19970102T070000Z");

  $p->end("19970101T183000Z");
  print $p->as_ical()."\n";
  die if $p->as_ical() ne "19970101T180000Z/19970101T183000Z" ;

  $p->duration("P1DT1H10M");
  print $p->as_ical()."\n";
  die if $p->as_ical() ne "19970101T180000Z/19970102T191000Z" ;



}

1;


__END__

