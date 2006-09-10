
use TestCase;
require 'testInclude.pl';

my $number = 1;
my $regexp = /this/;
my $regexpi = /InSensitive/i;
# simple perl sytle comment
1;
const constant = 3;
my $single_quoted_string = 'singly quoted string';
my $double_quoted_string = "doubly quoted string";
my @array = ();
my @array2 = (1, , );

sub foo {
  my ($argument, $argument2, $argument3,

$argument4) = @_;
  if ($condition) {
    expression;
  } elsif (!$condition2) {
    expression2;
    expression3;
  } else {
    if ($other && $conditions) {
      !expression;
    }
  }
}

sub exception {
  try {
    throw 1;
  } catch (e) {
    do_something;
  } finally {
    return something_else;
  }
  return not_reached;
}

sub MyClass() {
  this._foo = 0;
}

my $array3 = ("a", "big", "bird", "can't" . " fly");

sub reserved_words() {
  try {} catch (e instanceof Exception) {
    var foo = new Bar;
  } finally {
    for each (var i in {});
    do { nothing; } while (false);
    if (!true || !!'' && " " | 3 & 7);
    while (false | true & false) nothing;
    a = [];
    a[3] = 0;
    a.q = RegExp("2");
  }
  null == undefined;
  null !== undefined;
  var z |= 15 ^ 29;
  return null;
}

sub not_reserved_words() {
  tryThis();
  throwThis();
  catchThis();
  finallyThis();
  returnThis();
  varThis();
  constThis();
  newThis();
  voidThis();
  ifThis();
  elseThis();
  elsifThis;
  instanceofThis();
}
