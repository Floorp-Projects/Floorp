#=============================================================================
#
# This package is free software and is provided "as is" without express
# or implied warranty. It may be used, redistributed and/or modified
# under the same terms as perl itself. (Either the Artistic License or
# the GPL.)
#
#=============================================================================

=head1 NAME

Net::ICal::Duration -- represent a length of time

=head1 SYNOPSIS

  use Net::ICal;
  $d = Net::ICal::Duration->new("P3DT6H15M10S");
  $d = Net::ICal::Duration->new(3600); # 1 hour in seconds

=head1 DESCRIPTION

I<Duration> Represents a length of time, such a 3 days, 30 seconds or
7 weeks. You would use this for representing an abstract block of
time; "I want to have a 1-hour meeting sometime." If you want a
calendar- and timezone-specific block of time, see Net::ICal::Period.

=cut

#=============================================================================

package Net::ICal::Libical::Duration;
use Net::ICal::Libical::Property;
use strict;
use Carp;
@Net::ICal::Libical::Duration::ISA = qw ( Net::ICal::Libical::Property );

=head1 METHODS

=head2 new

Create a new I<Duration> from:

=over 4

=item * A string in RFC2445 duration format

=item * An integer representing a number of seconds

=cut

sub new {
  my $package = shift;
  my $arg = shift;
  my $self;

  if (ref($arg) == 'HASH'){
    # Construct from dictionary 
    $self = Net::ICal::Libical::Property::new($package,$arg);
    my $val=Net::ICal::Libical::icalproperty_get_value_as_string($self->{'ref'});
    $self->{'dur'} = Net::ICal::Libical::icaldurationtype_from_string($val);
    
   return $self;

  } elsif ($arg =~ /^[-+]?\d+$/){
    # Seconds
    $self = Net::ICal::Libical::Property::new($package,'DURATION');
    $self->{'dur'} = Net::ICal::Libical::icaldurationtype_new_from_int($arg);
  } elsif ($arg) {
    # iCalendar string 
    $self = Net::ICal::Libical::Property::new($package,'DURATION');
    $self->{'dur'} = Net::ICal::Libical::icaldurationtype_new_from_string($arg);
  } else {
    die;
  }
  
  $self->_update_value();
  return $self;

}

sub _update_value {
  my $self = shift;

  die "Can't find internal icalduration reference" if !$self->{'dur'};

  $self->value(Net::ICal::Libical::icaldurationtype_as_ical_string($self->{'dur'}));

}
=head2 clone()

Return a new copy of the duration.

=cut

sub clone {
  die "Not Implemented";
  
}


=head2 is_valid()

Determine if this is a valid duration (given criteria TBD). 

=cut

sub is_valid { 

  die "Not Implemented;"

}

=head2 seconds()

Set or Get the length of the duration as seconds. 

=cut

sub seconds {
  my $self = shift;
  my $seconds = shift;

  if($seconds){
    $self->{'dur'} =  
    Net::ICal::Libical::icaldurationtype_from_int($seconds);
    $self->_update_value();
  }

  return Net::ICal::Libical::icaldurationtype_as_int($self->{'dur'});

}

=head2 add($duration)

Return a new duration that is the sum of this and $duration. Does not
modify this object.

=cut

sub add {
  my ($self, $duration) = @_;

  return new Duration($self->seconds() + $duration->seconds());
}


=head2 subtract($duration)

Return a new duration that is the difference between this and
$duration. Does not modify this object.

=cut

sub subtract {
  my ($self, $duration) = @_;

  return new Duration($self->seconds() - $duration->seconds());
}

1;
