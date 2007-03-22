#!/perl/bin/perl

@ARGV[0];
$Dir = $ARGV[0];
sleep (3);
print ($Dir);
$temp = $Dir;
$slash = "-"."-";
print ("\n");
print ($slash);
$temp =~ s/'\'/$slash/g;
print ("\n");
print ($temp);


