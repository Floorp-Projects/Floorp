#!/usr/bin/perl -w
# -*- Mode: perl -*-
#======================================================================
#
# This package is free software and is provided "as is" without express
# or implied warranty.  It may be used, redistributed and/or modified
# under the same terms as perl itself. ( Either the Artistic License or the
# GPL. ) 
#
#
#======================================================================



=pod

=head1 NAME

Net::ICal::Time -- represent a time and date

=head1 SYNOPSIS

  $t = new Net::ICal::Time("19970101T120000Z");
  $t = new Net::ICal::Time("19970101T120000","America/Los_Angeles");
  $t = new Net::ICal::Time(time(),"America/Los_Angeles");

  $t2 = $t->add(Net::Ical::Duration("1D"));

  $duration = $t->subtract(Net::ICal::Time("19970101T110000Z"))

  # Add 5 minutes
  $t->min($t->min()+5);

  # Add 5 minutes
  $t->sec($t->sec()+300);

  # Compare
  if($t->compare($t2) > 0) {}

=head1 DESCRIPTION

I<Time> represents a time, but can also hold the time zone for the
time and indicate if the time should be treated as a date. The time
can be constructed from a variey of formats.

=head1 METHODS

=cut

package Net::ICal::Libical::Time;
use Net::ICal::Libical::Duration;
use Net::ICal::Libical::Property;
use Time::Local;
use POSIX;
use Carp qw(confess cluck);
use strict;
use UNIVERSAL qw(isa);

@Net::ICal::Libical::Time::ISA = qw(Net::ICal::Libical::Property);

=pod

=head2 new

Creates a new time object given one of:

=over 4

=item * ISO format string

=item * Some other format string, maybe whatever a Date module understands

=item * Integer representing seconds past the POSIX epoch

=back

The optional second argument is the timezone in Olsen place name format, 
which looks like "America/Los_Angeles"; it can be used to get the standard 
offset from UTC, the dates the location goes to and from Daylight Savings 
Time, and the magnitude of the Daylight Savings time offset. 

=cut

sub new{
  my $package = shift;
  my $arg = shift;

  my $self;

  if (ref($arg) == 'HASH'){
    # Construct from dictionary 
    $self = Net::ICal::Libical::Property::new($package,$arg);
    my $val=Net::ICal::Libical::icalproperty_get_value_as_string($self->{'ref'});
    $self->{'tt'} = Net::ICal::Libical::icaltime_from_string($val);
    
   return $self;

  } else {

    if ($#_ = 1){
      # iCalendar string 
      $self = Net::ICal::Libical::Property::new($package,'DTSTART');
      $self->{'tt'} = Net::ICal::Libical::icaltime_new_from_string($arg);
    } else {
      # Broken out time 
      die;
    }

    $self->_update_value();
    return $self;
  }

}


sub _update_value {
  my $self = shift;

  if(!$self->{'tt'}){
    die "Can't find reference to icaltimetype";
  }

  $self->value(Net::ICal::Libical::icaltime_as_ical_string($self->{'tt'}));

}
  
=pod

=head2 clone()

Create a new copy of this time. 

=cut

# clone a Time object. 
sub clone {
  my $self = shift;

  bless( {%$self},ref($self));

  $self->{'ref'} = Net::ICal::Libical::icalproperty_new_clone($self->{'ref'});

}



=pod

=head2 is_valid()

TBD

=cut

sub is_valid{
  my $self = shift;

  return Net::ICal::Libical::icaltime_is_null_time($self->{'tt'});
  
}



=pod

=head2 is_date([true|false])

Accessor to the is_date flag. If true, the flag indicates that the
hour, minute and second fields are set to zero and not used in
comparisons.

=cut


sub is_date {
  my $self = shift;
  if(@_){

    # Convert to true or false
    Net::ICal::Libical::icaltimetype_is_date_set($self->{'tt'},
						 !(!($_[0]))); 
  } 

  return Net::ICal::Libical::icaltimetype_is_date_get($self->{'tt'});

}


=pod

=head2 is_utc([true|false])

Is_utc indicates if the time should be interpreted in the UTC timezone. 

=cut

sub is_utc {
  my $self = shift;
  if(@_){

    # Convert to true or false
    Net::ICal::Libical::icaltimetype_is_utc_set($self->{'tt'},
						 !(!($_[0]))); 
  } 

  return Net::ICal::Libical::icaltimetype_is_utc_get($self->{'tt'});

}
=pod

=head2 timezone

Accessor to the timezone. Takes & Returns an Olsen place name
("America/Los_Angeles", etc. ) , an Abbreviation, 'UTC', or 'float' if
no zone was specified.

=cut

sub timezone {
  my $self = shift;
  my $tz = shift;

  if($tz){
    $self->set_parameter('TZID',$tz);
  }

  return $self->get_parameter('TZID');

}



=pod

=head2 normalize()

Adjust any out-of range values so that they are in-range. For
instance, 12:65:00 would become 13:05:00.

=cut

sub normalize{
  my $self = shift;
  
  $self->{'tt'} = Net::ICal::Libical::icaltime_normalize($self->{'tt'});
  $self->value(Net::ICal::Libical::icaltime_as_ical_string($self->{'tt'}));

}


=pod

=head2 hour([$hour])

Accessor to the hour.  Out of range values are normalized. 

=cut

=pod
=head2 minute([$min])

Accessor to the minute. Out of range values are normalized.

=cut

=pod
=head2 second([$dsecond])

Accessor to the second. Out of range values are normalized. For
instance, setting the second to -1 will decrement the minute and set
the second to 59, while setting the second to 3600 will increment the
hour.

=cut

=pod

=head2 year([$year])

Accessor to the year. Out of range values are normalized. 

=cut

=pod

=head2 month([$month])

Accessor to the month.  Out of range values are normalized. 

=cut


=pod

=head2 day([$day])

Accessor to the month day.  Out of range values are normalized. 

=cut

sub _do_accessor {
no strict;
  my $self = shift;
  my $type = shift;
  my $value = shift;
  
  $type = lc($type);

  if($value){
    my $set = "Net::ICal::Libical::icaltimetype_${type}_set";

    &$set($self->{'tt'},$value);
    $self->normalize();
    $self->_update_value();

  }

  my $get = "Net::ICal::Libical::icaltimetype_${type}_get";

  return &$get($self->{'tt'});
}


sub second {my $s = shift; my $v = shift; return $s->_do_accessor('SECOND',$v);}
sub minute {my $s = shift; my $v = shift;return $s->_do_accessor('MINUTE',$v);}
sub hour {my $s = shift;  my $v = shift; return $s->_do_accessor('HOUR',$v);}
sub day {my $s = shift; my $v = shift; return $s->_do_accessor('DAY',$v);}
sub month {my $s = shift; my $v = shift; return $s->_do_accessor('MONTH',$v);}
sub year {my $s = shift;  my $v = shift; return $s->_do_accessor('YEAR',$v);}


=pod

=head2 add($duration)

Takes a I<Duration> and returns a I<Time> that is the sum of the time
and the duration. Does not modify this time.

=cut
sub add {
  my $self = shift;
  my $dur = shift;

  cluck "Net::ICal::Time::add argument 1 requires a Net::ICal::Duration" if !isa($dur,'Net::ICal::Duration');

  my $c = $self->clone();

  $c->second($dur->as_int());

  $c->normalize();

  return $c;

}

=pod

=head2 subtract($time)

Subtract out a time of type I<Time> and return a I<Duration>. Does not
modify this time.

=cut
sub subtract {
  my $self = shift;
  my $t = shift;

  cluck "Net::ICal::Time::subtract argrument 1 requires a Net::ICal::Time" if !isa($t,'Net::ICal::Time');

  my $tint1 = $self->as_int();
  my $tint2 = $t->as_int();

  return new Net::ICal::Duration($tint1 - $tint2);

}

=pod

=head2 move_to_zone($zone);

Change the time to what it would be in the named timezone. 
The zone can be an Olsen placename or "UTC".

=cut

# XXX this needs implementing. 
sub move_to_zone {
  confess "Not Implemented\n";
} 



=pod

=head2 as_int()

Convert the time to an integer that represents seconds past the POSIX
epoch

=cut
sub as_int {
  my $self = shift;

  return Net::ICal::Libical::icaltime_as_timet($self->{'tt'});
}

=pod

=head2 as_localtime()

Convert to list format, as per localtime()

=cut
sub as_localtime {
  my $self = shift;

  return localtime($self->as_int());

}

=pod

=head2 as_gmtime()

Convert to list format, as per gmtime()

=cut
sub as_gmtime {
  my $self = shift;

  return gmtime($self->as_int());

}

=pod

=head2 compare($time)

Compare a time to this one and return -1 if the argument is earlier
than this one, 1 if it is later, and 0 if it is the same. The routine
does the comparision after converting the time to UTC. It converts
floating times using the system notion of the timezone.

=cut
sub compare {
  my $self = shift;
  my $a = $self->as_int();

  my $arg = shift;

  if(!isa($arg,'Net::ICal::Time')){
    $arg = new Net::ICal::Time($arg);
  }

  my $b = $arg->as_int();

  if($a < $b){
    return -1;
  } elsif ($a > $b) {
    return 1;
  } else {
    return 0;
  }
  
}

1; 

