package Tinderbox3::FieldProcessors::Warn;

use strict;

sub new {
	my $class = shift;
	$class = ref($class) || $class;
	my $this = {};
	bless $this, $class;

  return $this;
}

sub process_field {
  my $this = shift;
  my ($tree_columns, $field, $value) = @_;
  return "$field: $value";
}

1
