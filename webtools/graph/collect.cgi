#!/usr/bin/perl
use CGI::Carp qw(fatalsToBrowser);
use CGI::Request;
use POSIX qw(strftime);
use strict;

my $req  = new CGI::Request;

# incoming query string has the form: "?value=n&data=p:q:r...&tbox=foopy" 
# where 'n' is the average recorded time, and p,q,r... are the raw numbers,
# and 'tbox' is a name of a tinderbox

use vars qw{$value $data $tbox $testname $ua $ip $time};
$value    = $req->param('value');
$data     = $req->param('data');
$tbox     = $req->param('tbox'); $tbox =~ tr/A-Z/a-z/;
$testname = $req->param('testname');
$ua       = $req->cgi->var("HTTP_USER_AGENT");
$ip       = $req->cgi->var("REMOTE_ADDR");
$time     = strftime "%Y:%m:%d:%H:%M:%S", localtime;

#$value    = "1234";    # $req->param('value');
#$data     = "1:2:3:4"; #$req->param('data');
#$tbox     = "coffee";  #$tbox =~ tr/A-Z/a-z/;
#$testname = "startup"; #$req->param('testname');
#$ua       = "ua";      #$req->cgi->var("HTTP_USER_AGENT");
#$ip       = "1.2.3.4"; #$req->cgi->var("REMOTE_ADDR");
#$time     = "now";     #strftime "%Y:%m:%d:%H:%M:%S", localtime;


print "Content-type: text/plain\n\n";
for (qw{value data tbox testname ua ip time}) {
    no strict 'refs';
    printf "%10s = %s\n", $_, $$_;
}

# close HTTP transaction, and then decide whether to record data
#
# XXXX I had to comment this out, not sure why.  startup version
#      of this cgi had this and worked.  -mcafee
#close(STDOUT); 

#my %nametable = read_config();

# punt fake inputs
#die "Yer a liar"
#     unless $ip eq $nametable{$tbox} or $ip eq '208.12.39.125';
#die "No 'real' browsers allowed: $ua" 
#     unless $ua =~ /^(libwww-perl|PERL_CGI_BASE)/;
die "No 'value' parameter supplied" 
     unless $value;
die "No 'testname' parameter supplied" 
     unless $testname;
die "No 'tbox' parameter supplied" 
     unless $tbox;
die "No 'data' parameter supplied" 
     unless $data;


# Test for dirs.
unless (-d "db") {
  mkdir("db", 0755);
}


unless (-d "db/$testname") {
  mkdir("db/$testname", 0755);
}

# If file doesn't exist, try creating empty file.
unless (-f "db/$testname/$tbox") {
  open(FILE, "> db/$testname/$tbox") || die "Can't create new file db/$testname/$tbox: $!";
  close(FILE);
}

# record data
open(FILE, ">> db/$testname/$tbox") ||
     die "Can't open db/$tbox: $!";
print FILE "$time\t$value\t$data\t$ip\t$tbox\t$ua\n";     
close(FILE);

exit 0;

#
#
#
#sub read_config() {
#    my %nametable;
#    open(CONFIG, "< db/config.txt") || 
#	 die "can't open config.txt: $!";
#    while (<CONFIG>) {
#	next if /^$/;
#	next if /^#|^\s+#/;
#	s/\s+#.*$//;
#	my ($tinderbox, $assigned_ip) = split(/\s+/, $_);
#	$nametable{$tinderbox} = $assigned_ip;
#    }
#    return %nametable;
#}
