package Tinderbox3::FieldProcessors::Patch;

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

  return ($tree_columns->{PATCH_STR}{$value} || "(deleted patch)") . "<br>\n";
}

1
