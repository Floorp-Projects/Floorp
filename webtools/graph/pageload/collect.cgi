#!/usr/bin/perl
use CGI::Carp qw(fatalsToBrowser);
use CGI::Request;
use POSIX qw(strftime);
use strict;

my $req  = new CGI::Request;

# incoming query string has the form: "?value=n&data=p:q:r...&tbox=foopy"
# where 'n' is the average recorded time, and p,q,r... are the raw numbers,
# and 'tbox' is a name of a tinderbox

use vars qw{$value $data $tbox $ua $ip $time};
$value  = $req->param('value');
$data = $req->param('data');
$tbox = $req->param('tbox'); $tbox =~ tr/A-Z/a-z/;
$ua   = $req->cgi->var("HTTP_USER_AGENT");
$ip   = $req->cgi->var("REMOTE_ADDR");
$time = strftime "%Y:%m:%d:%H:%M:%S", localtime;

print "Content-type: text/plain\n\n";
for (qw{value data tbox ua ip time}) {
    no strict 'refs';
    printf "%6s = %s\n", $_, $$_;
}

# close HTTP transaction, and then decide whether to record data
close(STDOUT);

my %nametable = read_config();

# punt fake inputs
if (0) {
#warn "Yer a liar" && return
#     unless $ip eq $nametable{$tbox};
warn "No 'real' browsers allowed: $ua" && return
     unless $ua =~ /^(libwww-perl|PERL_CGI_BASE)/;
warn "No 'value' parameter supplied" && return
     unless $value;
warn "No 'data' parameter supplied" && return
     unless $data;
}

# record data
open(FILE, ">> db/$tbox") ||
     die "Can't open $tbox: $!";
print FILE "$time\t$value\t$data\t$ip\t$tbox\t$ua\n";
close(FILE);

exit 0;

#
#
#
sub read_config() {
    my %nametable;
    open(CONFIG, "< db/config.txt") ||
         die "can't open config.txt: $!";
    while (<CONFIG>) {
        next if /^$/;
        next if /^#|^\s+#/;
        s/\s+#.*$//;
        my ($tinderbox, $assigned_ip) = split(/\s+/, $_);
        $nametable{$tinderbox} = $assigned_ip;
    }
    return %nametable;
}

