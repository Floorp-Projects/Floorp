#!/usr/bin/perl -w
# -*- perl -*-
use P4CGI ;
use strict ;
use CGI::Carp ;
#
#################################################################
#  CONFIGURATION INFORMATION 
#  All config info should be in P4CGI.pm
#
#################################################################
#
#  List open files
#
#################################################################

				# File argument
my $FSPC = P4CGI::cgi()->param("FSPC") ;
$FSPC = "//..." unless defined $FSPC ;
&P4CGI::bail("Invalid file spec.") if ($FSPC =~ /[<>"&:;'`]/);
my @FSPC = split(/\s*\+?\s*(?=\/\/)/,$FSPC) ;
$FSPC = "<tt>".join("</tt> and <tt>",@FSPC)."</tt>" ;
my $FSPCcmd = "\"" . join("\" \"",@FSPC) . "\"" ;

my $SORTBY = P4CGI::cgi()->param("SORTBY") ;
$SORTBY="NAME" unless defined $SORTBY and $SORTBY eq "USER" ;

my $err2null = &P4CGI::REDIRECT_ERROR_TO_NULL_DEVICE() ;

				# Get info about opened status
my @opened ;
&P4CGI::p4call(\@opened,"opened -a $FSPCcmd $err2null") ;

map { /(.*)\#(\d+) - (\S+) (\S+\s\S+) \S+ by (\S+)@(\S+)/ ;
      $_ = [$1,$2, $3,$4, $5,$6] ; } @opened ;
#           file   status user
#              rev    change client

my @legend ;

if($SORTBY eq "USER") {
    my @tmp = sort { my @a = @$a ;
		     my @b = @$b ;
		     uc("$a[4] $a[5]").$a[0] cmp uc("$b[4] $b[5]").$b[0] ; } @opened ;
    @opened = @tmp ;
    push @legend, &P4CGI::ahref("SORTBY=NAME",
				"Sort list by file name") ;
}
else {
    push @legend, &P4CGI::ahref("SORTBY=USER",
				"Sort list by user") ;
} ;

				# Create converstion hash for user -> fullname
my %userCvt ;
{
    my @users ;
    &P4CGI::p4call(\@users, "users" );
    %userCvt =  map { /^(\S+).*> \((.+)\) .*$/ ; ($1,$2) ; } @users ;
} ;


print &P4CGI::start_page("List open files for<br>$FSPC",&P4CGI::ul_list(@legend)) ;

my ($lastFile,$lastRev,$lastUser,$lastClient) = ("","","","") ;
sub printLine(@) {
    my ($file,$rev,$status,$change,$user,$client) = @_ ;
    $change =~ s/\s*change\s*// ;

    my $Puser = &P4CGI::ahref(-url => "userView.cgi",
			      "USER=$user",
			      "$user ($userCvt{$user})") ;
    my $Pclient = &P4CGI::ahref(-url => "clientView.cgi",
				"CLIENT=$client",
				"$client") ;
    my $Pfile = &P4CGI::ahref(-url => "fileLogView.cgi",
			      "FSPC=$file",
			      "$file") ;    
    my $Prev = &P4CGI::ahref(-url => "fileViewer.cgi",
			     "FSPC=$file",
			     "REV=$rev",
			     "$rev")  ;
    
    if($SORTBY eq "NAME") {	
	if($file eq $lastFile) {
	    $Pfile = "" ;
	    if($rev eq $lastRev) {
		$Prev = "" ;
	    }
	}
	print &P4CGI::table_row($Pfile,$Prev,$status,$change,$Puser,$Pclient) ;
    }
    elsif ($SORTBY eq "USER") {
	if($user eq $lastUser) {
	    $Puser = "" ;	    
	    if($client eq $lastClient) {
		$Pclient = "" ;
	    }
	}
	print &P4CGI::table_row($Puser,$Pclient,$Pfile,$Prev,$status,$change) ;
    } ;
    ($lastFile,$lastRev,$lastUser,$lastClient) = ($file,$rev,$user,$client) ;
} ;



print &P4CGI::start_table("") ;
if($SORTBY eq "NAME") {
    print &P4CGI::table_header("File/view log","Rev/view file","Status","Change","User/view","Client/view") ;
}
elsif($SORTBY eq "USER") {
    print &P4CGI::table_header("User/view","Client/view","File/view log","Rev/view file","Status","Change") ;
} ;

map { printLine(@$_) ; } @opened ;


print &P4CGI::end_table() ;

print &P4CGI::end_page() ;

#
# That's all folks
#











