#!/perl/bin/perl

@ARGV[0];
$BaseDir = $ARGV[0];
#print ($BaseDir, "\n");
system ("apprunner.pl \"$BaseDir\"");
system ("url.pl \"$BaseDir\"");
system ("mail.pl \"$BaseDir\"");
system ("smtp.pl \"$BaseDir\"");
system ("pop3.pl \"$BaseDir\"");
system ("Intlurl.pl \"$BaseDir\"");
