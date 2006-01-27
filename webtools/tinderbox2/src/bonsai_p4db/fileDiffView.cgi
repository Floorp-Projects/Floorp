#!/usr/bin/perl -Tw
# -*- perl -*-

use lib '.';
use P4CGI ;
use strict ;
#use FileHandle; Can't do! Won't work for all perls... Y? Who knows?

#
#################################################################
#  CONFIGURATION INFORMATION 
#  All config info should be in P4CGI.pm
#
#################################################################
#
#  P4 file diff viewer
#  View diff between two files or two revisions
#
#################################################################

# Get file spec argument
my $FSPC = P4CGI::cgi()->param("FSPC") ;
$FSPC = &P4CGI::extract_printable_chars($FSPC);
&P4CGI::bail("Invalid file spec.") if ($FSPC =~ /[<>"&:;'`]/);
my @files = split /,/,$FSPC;
&P4CGI::bail("No file specified") unless @files > 0 ;

my $REV = P4CGI::cgi()->param("REV");
$REV = &P4CGI::extract_digits($REV);
my @revs = split / /,$REV if defined $REV;
&P4CGI::bail("Invalid file revisions.") unless ($REV =~ /^[0-9 ]*$/);
$files[0] =~ s/^([^\#]+)\#(\d+)/$1/ and do { $revs[0] = $2 ; } ;
&P4CGI::bail("No revision specified") unless @revs > 0 ;

my $ACT = P4CGI::cgi()->param("ACT");
&P4CGI::bail("Invalid mode(s).") if ($ACT =~ /[<>"&:;'`]/);
my @modes = split / /,$ACT if defined $ACT;
&P4CGI::bail("No mode specified") unless @modes > 0 ;

my $FSPC2 = P4CGI::cgi()->param("FSPC2");
$FSPC2 = &P4CGI::extract_printable_chars($FSPC2);
&P4CGI::bail("Invalid file spec.") if ($FSPC2 =~ /[<>"&:;'`]/);
my @files2 = split /,/,$FSPC2 if defined $FSPC2;
    
my $REV2 = P4CGI::cgi()->param("REV2");
$REV2 = &P4CGI::extract_digits($REV2);
my @revs2 = split / /,$REV2 if defined $REV2;
&P4CGI::bail("Invalid revisions specified.") unless ($REV2 =~ /^[0-9 ]*$/);
if(defined $files2[0]) {
    $files2[0] =~ s/^([^\#]+)\#(\d+)/$1/ and do { $revs2[0] = $2 ; } ;
} ;

my $change = P4CGI::cgi()->param("CH") ;
$change = &P4CGI::extract_digits($change);
&P4CGI::bail("Invalid change specified.") unless ($change =~ /^[0-9]*$/);
# Constants for the file diff display

# $NCONTEXT - number of lines context before and after a diff
my $NCONTEXT = P4CGI::cgi()->param("CONTEXT") ;
$NCONTEXT = &P4CGI::extract_digits($NCONTEXT);
$NCONTEXT = 10 unless defined $NCONTEXT ;
&P4CGI::bail("Invalid number of context lines.") unless ($NCONTEXT =~ /^[0-9]*$/);

# $MAXCONTEXT - max number of lines context between diffs
my $MAXCONTEXT = $NCONTEXT+20;


my $n ;
for($n = 0; $n < @files ; $n++) {
    $files2[$n] = $files[$n] unless defined $files2[$n] ;
    $revs2[$n] = $revs[$n]-1 unless defined $revs2[$n] ;
}

if((@files != @revs) ||
   (@files != @files2) ||
   (@files != @revs2)) {
    &P4CGI::bail("Argument counts not correct") ;    
} ;

my $title ;

if(@files == 1) {
    if($files[0] eq $files2[0]) {
	if($revs[0] < $revs2[0]) {
	    my $r = $revs2[0] ;
	    $revs2[0] = $revs[0] ;
	    $revs[0] = $r ;
	}
	$title = "Diff<br><code>$files[0]</code><br>\#$revs2[0] -&gt; \#$revs[0] " ;
    }
    else {
	$title = "Diff between<br><code>$files[0]\#$revs[0]</code><br>and<br><code>$files2[0]\#$revs2[0]" ;
    }
}
else {
    $title = "Diffs for change $change" ;
}

my $nextNCONTEXT= $NCONTEXT*2 ;
my @pstr ;
my $p ;
foreach $p (&P4CGI::cgi()->param) {
    next if $p eq "CONTEXT" ;
    push @pstr, $p . "=" . P4CGI::cgi()->param($p) ;
}
my $moreContext=&P4CGI::ahref(@pstr,
			      "CONTEXT=$nextNCONTEXT",
			      "Show more context") ;
my $showWholeFile=&P4CGI::ahref(@pstr,
				"CONTEXT=9999999",
				"Show complete file") ;

my $legend ;
if($NCONTEXT < 9999999) {
    $legend = 
	&P4CGI::ul_list("<b>Line numbers</b> -- Goto line in file vewer",
			"<b>$moreContext</b> -- Click here to get more context",
			"<b>$showWholeFile</b> -- Click here to see whole file with diffs") ;     
}
else {
    $legend = 
	&P4CGI::ul_list("<b>Line numbers</b> -- Goto line in file vewer") ;
}

print
    "",
    &P4CGI::start_page($title,$legend) ;		       



my $currentFile ;
my $currentRev ;

local *P4 ;
my $P4lineNo ; 

sub getLine() 
{
    $P4lineNo++ if defined $P4lineNo ;
    return <P4> ;
}
   
while(@files>0) {

    my $f1start= "<font color=blue>" ;
    my $f1end="</font>" ;
    my $f2start = "<font color=red><strike>" ;
    my $f2end = "</strike></font>" ;
    
    my $file   = shift @files ;
    my $file2  = shift @files2 ;
    my $rev    = shift @revs ;
    my $rev2   = shift @revs2 ;
    my $mode   = shift @modes ;

    if($file eq $file2) {
	if($rev < $rev2) {
	    my $r = $rev2 ;
	    $rev2 = $rev ;
	    $rev = $r ;
	}
    }
    else {
	$f2start = "<font color=green>" ;
	$f2end = "</font>" ;
    }

    $currentFile = $file ;
    $currentRev = $rev ;

    print
	&P4CGI::start_table("width=100% align=center bgcolor=white"),
	&P4CGI::table_row({-align=>"center",
			   -text =>"<BIG>$f1start$file\#$rev$f1end<br>$f2start$file2\#$rev2$f2end</BIG>"}),
	&P4CGI::end_table(),
	"<pre>" ;
	
    my $f1 = "$file#$rev";
    my $f2 = "$file2#$rev2";

    ##
    ## Use "p4 diff2" to get a list of modifications (diff chunks)
    ##

    my $nchunk =0; # Counter for diff chunks 
    my @start ;    # Start line for chunk in latest file
    my @dels ;     # No. of deleted lines in chunk
    my @adds ;     # No. of added lines in chunk
    my @delLines ; # Memory for deleted lines
    
    if ($mode ne 'add' && $mode ne 'delete' && $mode ne 'branch') {
	
	&P4CGI::p4call(*P4, "diff2 \"$f2\" \"$f1\"");
	$_ = <P4>; 
	while (<P4>) {
	    
	    # Check if line matches start of a diff chunk
	    /(\d+),?(\d*)([acd])(\d+),?(\d*)/  or do { next ; } ;

	    # $la, $lb: start and end line in old (left) file
	    # $op: operation (one of a,d or c)
	    # $ra, $rb: start and end line in new (right) file
	    my ( $la, $lb, $op, $ra, $rb ) = ($1,$2,$3,$4,$5) ;
	    
	    # End lines may have to be calculated
	    if( !$lb ) { $lb = $op ne 'a' ? $la : $la - 1; }
	    if( !$rb ) { $rb = $op ne 'd' ? $ra : $ra - 1; }
	    	
	    my ( $dels, $adds ); # Temporary vars for No of adds/deletes
	    
	    # Calc. start position in new (right) file
	    $start[ $nchunk ] = $op ne 'd' ? $ra : $ra + 1;
	    # Calc. No. of deleted lines
	    $dels[ $nchunk ] = $dels = $lb - $la + 1;
	    # Calc. No. of added lines
	    $adds[ $nchunk ] = $adds = $rb - $ra + 1;
	    # Init deleted lines 
	    $delLines[ $nchunk ] = "";
	    
	    # Get the deleted lines from the old (left) file
	    while( $dels-- ) {
		$_ = <P4>;
		s/^. //;
		$_ = &P4CGI::fixSpecChar($_) ;
		$delLines[ $nchunk ] .= 
		    "<small>      </small> <font color=red>|</font>$_";
	    }
	    
	    # If it was a change, skip over separator
	    if ($op eq 'c') {	
		$_ = <P4>; 
	    }
	    
	    # Skip over added lines (we don't need to know them yet)
	    while( $adds-- ) {
		$_ = <P4>;
	    }

	    # Next chunk.
	    $nchunk++;
	}	
	close P4;
    }
    
    # Now walk through the diff chunks, reading the new (right) file and
    # displaying it as necessary.

    &P4CGI::p4call(*P4, "print -q \"$f1\"");
    
    $P4lineNo = 0; # Current line    
    my $n ;
    for( $n = 0; $n < $nchunk; $n++ )
    {
	# print up to this chunk.
	&catchup($start[ $n ] - $P4lineNo - 1) ;
	
	# display deleted lines -- we saved these from the diff
	if( $dels[ $n ] )
	{
	    print "$f2start";
	    print $delLines[ $n ];
	    print "$f2end";
	}
	
	# display added lines -- these are in the file stream.
	if( $adds[ $n ] )
	{
	    print "$f1start";
	    &display($adds[ $n ] );
	    print "$f1end";
	}
	
	# $curlin = $start[ $n ] + $adds[ $n ];
    }
    
    &catchup(999999999 );
    
    close P4;

    print "</pre>" ;
}

print &P4CGI::end_page() ;


# Support for processing diff chunks.
#
# skip: skip lines in source file
# display: display lines in source file, handling funny chars 
# catchup: display & skip as necessary
#

##
## skip(<handle>,no of lines)
##    Returns: 0 or No. of lines not skipped if file ends
sub skip {
    my $to = shift @_;
    while( $to > 0 && ( $_ = &getLine() ) ) {
	$to--;
    }
    return $to;
}

##
## display(<handle>,no of lines)
##   Displays a number of lines from handle
sub display {
    my $to = shift @_;
    
    while( $to-- > 0 && ( $_ = &getLine() ) ) {
	my $line = &P4CGI::fixSpecChar($_) ;
	my $ls ;
	if(($P4lineNo % 5) == 0) {
	    $ls = sprintf("<small>%5d:</small>",$P4lineNo) ;
	    $ls = &P4CGI::ahref(-url=>"fileViewer.cgi",
				-anchor => "L$P4lineNo",			  
				"FSPC=$currentFile",
				"REV=$currentRev",

				$ls) ;
	}
	else {
	    $ls = "<small>      </small>" ;
	}
	print "$ls <font color=red>|</font>$line" ;
    }
}

##
## catchup(<handle>,no of lines)
##   Print/skip lines to next diff chunk
sub catchup {
    my $to = shift @_;

    if( $to > $MAXCONTEXT )
    {
	my $skipped = $to - $NCONTEXT ; 
	if($P4lineNo > 0) {		
	    &display($NCONTEXT );
	    $skipped -= $NCONTEXT ;
	}
	$skipped -= &skip($skipped );
	
	print 
	    "<hr><center><strong>",
	    "$skipped lines skipped",
	    "</strong></center><hr>\n" if( $skipped );
	
	&display($NCONTEXT );
    }
    else
    {
	&display($to);
    }
}

#
# That's all folks
#
