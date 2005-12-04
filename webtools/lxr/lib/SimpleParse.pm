# $Id: SimpleParse.pm,v 1.2 2005/12/04 14:40:04 timeless%mozdev.org Exp $

use strict;

package SimpleParse;

require Exporter;

use vars qw(@ISA @EXPORT);

@ISA = qw(Exporter);
@EXPORT = qw(&doparse &untabify &init &nextfrag);

my $INFILE;			# Input file handle
my @frags;			# Fragments in queue
my @bodyid;			# Array of body type ids
my @open;			# Fragment opening delimiters
my @term;			# Fragment closing delimiters
my $split;			# Fragmentation regexp
my $open;			# Fragment opening regexp
my $tabwidth;			# Tab width

sub init {
    my @blksep;

    ($INFILE, @blksep) = @_;

    while (@_ = splice(@blksep,0,3)) {
	push(@bodyid, $_[0]);
	push(@open, $_[1]);
	push(@term, $_[2]);
    }

    foreach (@open) {
	$open .= "($_)|";
	$split .= "$_|";
    }
    chop($open);
    
    foreach (@term) {
	next if $_ eq '';
	$split .= "$_|";
    }
    chop($split);

    $tabwidth = 8;
}


sub untabify {
    my $t = $_[1] || 8;

    $_[0] =~ s/([^\t]*)\t/$1.(' ' x ($t - (length($1) % $t)))/ge;
    return($_[0]);
}


sub nextfrag {
    my $btype = undef;
    my $frag = undef;

    while (1) {
	if ($#frags < 0) {
	    my $line = <$INFILE>;
	    
	    if ($. == 1 &&
		$line =~ /^.*-[*]-.*?[ \t;]tab-width:[ \t]*([0-9]+).*-[*]-/) {
		$tabwidth = $1;
	    }
		
	    &untabify($line, $tabwidth);
#	    $line =~ s/([^\t]*)\t/
#		$1.(' ' x ($tabwidth - (length($1) % $tabwidth)))/ge;

	    @frags = split(/($split)/o, $line);
	}

	last if $#frags < 0;
	
	unless (length $frags[0]) {
	    shift(@frags);

	} elsif (defined($frag)) {
	    if (defined($btype)) {
		my $next = shift(@frags);
		
		$frag .= $next;
		last if $next =~ /^$term[$btype]$/;

	    } else {
		last if $frags[0] =~ /^$open$/o;
		$frag .= shift(@frags);
	    }
	} else {
	    $frag = shift(@frags);
	    if (defined($frag) && (@_ = $frag =~ /^$open$/o)) {
		my $i = 1;
		$btype = grep { $i = ($i && !defined($_)) } @_;
	    }
	}
    }
    $btype = $bodyid[$btype] if $btype;
    
    return($btype, $frag);
}

1;
