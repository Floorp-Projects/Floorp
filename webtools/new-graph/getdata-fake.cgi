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

if (defined($query{"tbox"}) && defined($query{"test"})) {
  my $tbox = $query{"tbox"};
  my $test = $query{"test"};

  print "{ resultcode: 0, results: [";

  srand();

  my $lv = 200 + rand (100);
  foreach my $k (1 .. 200) {
    #my $kv = $k;
    #my $v = $k;
    my $kv = 1148589000 + ($k*60*5);
    my $v = $lv;
    $lv = $lv + (rand(10) - 5);
    print "$kv, $v, ";
  }
  print "] }";
} elsif (defined($query{"tbox"})) {
  my $tbox = $query{"tbox"};

  print "{ resultcode: 0, results: ['$tbox-test1', '$tbox-test2', '$tbox-test3'] }";
} else {
  print "{ resultcode: 0, results: ['tbox1', 'tbox2', 'tbox3'] }";
}

