#!/usr/bin/perl

print "Content-type: text/plain\n\n";

#foreach my $k (keys(%ENV)) {
#	print "$k => " . $ENV{$k} . "\n";
#}

my $QS = $ENV{"QUERY_STRING"};
my %query = ();

{
  my @qp = split /\&/,$QS;
  foreach my $q (@qp) {
    my @qp1 = split /=/,$q;
    $query{$qp1[0]} = $qp1[1];
  }
}

if (defined($query{"setid"})) {
  my $testid = $query{"setid"};

  print "{ resultcode: 0, results: [";

  srand();

  my $lv = 200 + rand (100);
  foreach my $k (1 .. 500) {
    #my $kv = $k;
    #my $v = $k;
    my $kv = 1148589000 + ($k*60*20);
    my $v = $lv;
    $lv = $lv + (rand(10) - 5);
    print "$kv, $v, ";
  }
  print "] }";
} else {
  print "{ resultcode: 0, results: [
{ id: 1, machine: 'tbox1', test: 'test1', test_type: 'perf', extra_data: null },
{ id: 4, machine: 'tbox2', test: 'test1', test_type: 'perf', extra_data: null },
{ id: 3, machine: 'tbox1', test: 'test3', test_type: 'perf', extra_data: null },
{ id: 6, machine: 'tbox3', test: 'test3', test_type: 'perf', extra_data: null },
{ id: 2, machine: 'tbox1', test: 'test2', test_type: 'perf', extra_data: null },
{ id: 5, machine: 'tbox2', test: 'test2', test_type: 'perf', extra_data: null },
] }";
}

