# $Id: Common.pm,v 1.18 1999/04/26 19:42:35 endico%mozilla.org Exp $

package LXR::Common;

use DB_File;
use lib '../..';
use Local;

require Exporter;
@ISA = qw(Exporter);
@EXPORT = qw($Path &warning &fatal &abortall &fflush &urlargs 
	     &fileref &idref &htmlquote &freetextmarkup &markupfile
	     &markspecials &htmlquote &freetextmarkup &markupstring
	     &init &makeheader &makefooter &expandtemplate);


$wwwdebug = 1;

$SIG{__WARN__} = 'warning';
$SIG{__DIE__}  = 'fatal';


@cterm = ('atom',	'\\\\.',	'',
	  'comment',	'/\*',		'\*/',
	  'comment',	'//',		"\n",
	  'string',	'"',		'"',
	  'string',	"'",		"'",
	  'include',	'#include',	"\n");


sub warning {
    print(STDERR "[",scalar(localtime),"] warning: $_[0]\n");
    print("<h4 align=\"center\"><i>** Warning: $_[0]</i></h4>\n") if $wwwdebug;
}


sub fatal {
    print(STDERR "[",scalar(localtime),"] fatal: $_[0]\n");
    print("<h4 align=\"center\"><i>** Fatal: $_[0]</i></h4>\n") if $wwwdebug;
    exit(1);
}


sub abortall {
    print(STDERR "[",scalar(localtime),"] abortall: $_[0]\n");
    print("Content-Type: text/html; charset=iso-8859-1\n\n",
	  "<html>\n<head>\n<title>Abort</title>\n</head>\n",
	  "<body><h1>Abort!</h1>\n",
	  "<b><i>** Aborting: $_[0]</i></b>\n",
	  "</body>\n</html>\n") if $wwwdebug;
    exit(1);
}


sub fflush {
    $| = 1; print('');
}


sub urlargs {
    my @args = @_;
    my %args = ();
    my $val;

    foreach (@args) {
	$args{$1} = $2 if /(\S+)=(\S*)/;
    }
    @args = ();

    foreach ($Conf->allvariables) {
	$val = $args{$_} || $Conf->variable($_);
	push(@args, "$_=$val") unless ($val eq $Conf->vardefault($_));
	delete($args{$_});
    }

    foreach (keys(%args)) {
	push(@args, "$_=$args{$_}");
    }

    return($#args < 0 ? '' : '?'.join(';',@args));
}    


sub fileref {
    my ($desc, $path, $line, @args) = @_;

    #$path =~ s/\+/ /;
    # jwz: URL-quote any special characters.
    # endico: except plus. plus signs are normally used to represent spaces
    # but here we need to allow plus signs in file names for gtk+
    # hopefully this doesn't break anything else
    $path =~ s|([^-a-zA-Z0-9.+\@/_\r\n])|sprintf("%%%02X", ord($1))|ge;

    return("<a href=\"$Conf->{virtroot}/source$path".
	   &urlargs(@args).
	   ($line > 0 ? "#$line" : "").
	   "\"\>$desc</a>");
}


sub diffref {
    my ($desc, $path, $darg) = @_;

    ($darg,$dval) = $darg =~ /(.*?)=(.*)/;
    return("<a href=\"$Conf->{virtroot}/diff$path".
	   &urlargs(($darg ? "diffvar=$darg" : ""),
		    ($dval ? "diffval=$dval" : ""),
		    @args).
	   "\"\>$desc</a>");
}


sub idref {
    my ($desc, $id, @args) = @_;
    return("<a href=\"$Conf->{virtroot}/ident".
	   &urlargs(($id ? "i=$id" : ""),
		    @args).
	   "\"\>$desc</a>");
}


sub http_wash {
    my $t = shift;
    # $t =~ s/\+/%2B/g;
    #endico: don't use plus signs to represent spaces as is the normal
    #case. we need to use them in file names for gtk+

    $t =~ s/\%([\da-f][\da-f])/pack("C", hex($1))/gie;

    # Paranoia check. Regexp-searches in Glimpse won't work.
    # if ($t =~ tr/;<>*|\`&$!#()[]{}:\'\"//) {

    # Should be sufficient to keep "open" from doing unexpected stuff.
    if ($t =~ tr/<>|\"\'\`//) {
	&abortall("Illegal characters in HTTP-parameters.");
    }
    
    return($t);
}


sub markspecials {
    $_[0] =~ s/([\&\<\>])/\0$1/g;
}


sub htmlquote {
    $_[0] =~ s/\0&/&amp;/g;
    $_[0] =~ s/\0</&lt;/g;
    $_[0] =~ s/\0>/&gt;/g;
}


sub freetextmarkup {
    $_[0] =~ s#((ftp|http)://\S*[^\s.])#<a href=\"$1\">$1</a>#g;
    $_[0] =~ s/(&lt;(.*@.*)&gt;)/<a href=\"mailto:$2\">$1<\/a>/g;
}


sub linetag {
#$frag =~ s/\n/"\n".&linetag($virtp.$fname, $line)/ge;
#    my $tag = '<a href="'.$_[0].'#L'.$_[1].
#	'" name="L'.$_[1].'">'.$_[1].' </a>';
    my $tag;
    $tag .= ' ' if $_[1] < 10;
    $tag .= ' ' if $_[1] < 100;
    $tag .= &fileref($_[1], $_[0], $_[1]).' ';
    $tag =~ s/<a/<a name=$_[1]/;
#    $_[1]++;
    return($tag);
}

# dme: Smaller version of the markupfile function meant for marking up 
# the descriptions in source directory listings.
sub markupstring {
    my ($string, $virtp) = @_;

    # Mark special characters so they don't get processed just yet.
    $string =~ s/([\&\<\>])/\0$1/g;

    # Look for identifiers and create links with identifier search query.
    tie (%xref, "DB_File", $Conf->dbdir."/xref", O_RDONLY, 0664, $DB_HASH)
        || &warning("Cannot open xref database.");
    $string =~ s#(^|\s)([a-zA-Z_~][a-zA-Z0-9_]*)\b#
                $1.(is_linkworthy($2) ? &idref($2,$2) : $2)#ge;
    untie(%xref);

    # HTMLify the special characters we marked earlier,
    # but not the ones in the recently added xref html links.
    $string=~ s/\0&/&amp;/g;
    $string=~ s/\0</&lt;/g;
    $string=~ s/\0>/&gt;/g;

    # HTMLify email addresses and urls.
    $string =~ s#((ftp|http|nntp|snews|news)://(\w|\w\.\w|\~|\-|\/|\#)+(?!\.\b))#<a href=\"$1\">$1</a>#g;
    # htmlify certain addresses which aren't surrounded by <>
    $string =~ s/([\w\-\_]*\@netscape.com)(?!&gt;)/<a href=\"mailto:$1\">$1<\/a>/g;
    $string =~ s/([\w\-\_]*\@mozilla.org)(?!&gt;)/<a href=\"mailto:$1\">$1<\/a>/g;
    $string =~ s/([\w\-\_]*\@gnome.org)(?!&gt;)/<a href=\"mailto:$1\">$1<\/a>/g;
    $string =~ s/([\w\-\_]*\@linux.no)(?!&gt;)/<a href=\"mailto:$1\">$1<\/a>/g;
    $string =~ s/(&lt;)(.*@.*)(&gt;)/$1<a href=\"mailto:$2\">$2<\/a>$3/g;

    # HTMLify file names, assuming file is in the current directory.
    $string =~ s#\b(([\w-_\/]+\.(c|h|cc|cpp?|idl|java))|README)\b#<a href=\"$Conf->{virtroot}/source$virtp$1\">$1</a>#g;

    return($string);
}

# dme: Return true if string is in the identifier db and it seems like its
# use in the sentence is as an identifier and its not just some word that
# happens to have been used as a variable name somewhere. We don't want
# words like "of", "to" and "a" to get links. The string must be long 
# enough, and  either contain "_" or if some letter besides the first 
# is capitalized
sub is_linkworthy{
    my ($string) = @_;

    if  ( ($string =~ /....../) &&
	  ( ($string =~ /_/) ||
	    ($string =~ /.[A-Z]/)
	  ) &&
	  (defined($xref{$string}))
        ){
	return (1);
    }else{
	return (0);
    }
}

sub markupfile {
    my ($INFILE, $virtp, $fname, $outfun) = @_;

    $line = 1;

    # A C/C++ file 
    if ($fname =~ /\.([ch]|cpp?|idl|cc|java)$/i) { # Duplicated in genxref.

	&SimpleParse::init($INFILE, @cterm);
	
	tie (%xref, "DB_File", $Conf->dbdir."/xref", O_RDONLY, 0664, $DB_HASH)
	    || &warning("Cannot open xref database.");

	&$outfun(# "<pre>\n".
		 #"<a name=\"".$line++.'"></a>');
		 &linetag($virtp.$fname, $line++));

	($btype, $frag) = &SimpleParse::nextfrag;
	
	while (defined($frag)) {
	    &markspecials($frag);

	    if ($btype eq 'comment') {
		# Comment
		# Convert mail adresses to mailto:
		&freetextmarkup($frag);
		$frag = "<b><i>$frag</i></b>";
		$frag =~ s#\n#</i></b>\n<b><i>#g;
	    } elsif ($btype eq 'string') {
		# String
		$frag = "<i>$frag</i>";
		
	    } elsif ($btype eq 'include') { 
		# Include directive
                $frag =~ s#(\")(.*)(\")#
                    $1.&fileref($2, $virtp.$2).$3#e;
                $frag =~ s#(\0<)(.*)(\0>)#
                    $1.
                    &fileref($2, $Conf->mappath($Conf->incprefix."/$2")).
                    $3#e;
	    } else {
		# Code
		$frag =~ s#(^|[^a-zA-Z_\#0-9])([a-zA-Z_~][a-zA-Z0-9_]*)\b#
                    $1.(defined($xref{$2}) ? &idref($2,$2) : $2)#ge;
	    }

	    &htmlquote($frag);
	    $frag =~ s/\n/"\n".&linetag($virtp.$fname, $line++)/ge;
	    &$outfun($frag);
	    
	    ($btype, $frag) = &SimpleParse::nextfrag;
	}
	    
#	&$outfun("</pre>\n");
	untie(%xref);

    } elsif ($fname =~ /\.(gif|jpg|jpeg|pjpg|pjpeg|xbm)$/i) {

	&$outfun("</PRE>");
	&$outfun("<UL><TABLE><TR><TH VALIGN=CENTER><B>Image: </B></TH>");
	&$outfun("<TD VALIGN=CENTER>");

	&$outfun("<img src=\"$Conf->{virtroot}/source".$virtp.$fname.
		 &urlargs("raw=1")."\" border=\"0\" alt=\"$fname\">");

	&$outfun("</TR></TD></TABLE></UL><PRE>");

    } elsif ($fname eq 'CREDITS') {
	while (<$INFILE>) {
	    &SimpleParse::untabify($_);
	    &markspecials($_);
	    &htmlquote($_);
            s/^N:\s+(.*)/<strong>$1<\/strong>/gm;
	    s/^(E:\s+)(\S+@\S+)/$1<a href=\"mailto:$2\">$2<\/a>/gm;
	    s/^(W:\s+)(.*)/$1<a href=\"$2\">$2<\/a>/gm;
#	    &$outfun("<a name=\"L$.\"><\/a>".$_);
	    &$outfun(&linetag($virtp.$fname, $.).$_);
	}
    } else {

	my $first_line = <$INFILE>;
	my $is_binary = -1;

	$_ = $first_line;
	if ( m/^\#!/ ) {			# it's a script
	    $is_binary = 0;
	} elsif ( m/-\*-.*mode:/i ) {		# has an emacs mode spec
	    $is_binary = 0;
	} elsif (length($_) > 132) {		# no linebreaks
	    $is_binary = 1;
	} elsif ( m/[\000-\010\013\014\016-\037\200-Ÿ]/ ) {	# ctrl or ctrl+
	    $is_binary = 1;
	} else {				# no idea, but assume text.
	    $is_binary = 0;
	}

	if ( $is_binary ) {

	    &$outfun("</PRE>");
	    &$outfun("<UL><B>Binary File: ");

            # jwz: URL-quote any special characters.
            my $uname = $fname;
            $uname =~ s|([^-a-zA-Z0-9.\@/_\r\n])|sprintf("%%%02X", ord($1))|ge;

	    &$outfun("<A HREF=\"$Conf->{virtroot}/source".$virtp.$uname.
		     &urlargs("raw=1")."\">");
	    &$outfun("$fname</A></B>");
	    &$outfun("</UL><PRE>");

	} else {
	    $_ = $first_line;
	    do {
		&SimpleParse::untabify($_);
		&markspecials($_);
		&htmlquote($_);
		&freetextmarkup($_);
#	    &$outfun("<a name=\"L$.\"><\/a>".$_);
		&$outfun(&linetag($virtp.$fname, $.).$_);
	    } while (<$INFILE>);
	}
    }
}


sub fixpaths {
    $Path->{'virtf'} = '/'.shift;
    $Path->{'root'} = $Conf->sourceroot;
    
    while ($Path->{'virtf'} =~ s#/[^/]+/\.\./#/#g) {
       }
    $Path->{'virtf'} =~ s#/\.\./#/#g;
	   
    $Path->{'virtf'} .= '/' if (-d $Path->{'root'}.$Path->{'virtf'});
    $Path->{'virtf'} =~ s#//+#/#g;
    
    ($Path->{'virt'}, $Path->{'file'}) = $Path->{'virtf'} =~ m#^(.*/)([^/]*)$#;

    $Path->{'real'} = $Path->{'root'}.$Path->{'virt'};
    $Path->{'realf'} = $Path->{'root'}.$Path->{'virtf'};

    @pathelem = $Path->{'virtf'} =~ /([^\/]+$|[^\/]+\/)/g;
    
    $fpath = '';
    foreach (@pathelem) {
	$fpath .= $_;
	push(@addrelem, $fpath);
    }

    unshift(@pathelem, $Conf->sourcerootname.'/');
    unshift(@addrelem, "");
    
    foreach (0..$#pathelem) {
	if (defined($addrelem[$_])) {

	    # jwz: put a space after each / in the banner so that it's possible
	    # for the pathnames to wrap.  The <WBR> tag ought to do this, but
	    # it is ignored when sizing table cells, so we have to use a real
	    # space.  It's somewhat ugly to have these spaces be visible, but
	    # not as ugly as getting a horizontal scrollbar...
	    #
	    $Path->{'xref'} .= &fileref($pathelem[$_], "/$addrelem[$_]") . " ";
	} else {
	    $Path->{'xref'} .= $pathelem[$_];
	}
    }
    $Path->{'xref'} =~ s#/</a>#</a>/#gi;
}


sub init {
    my ($argv_0) = @_;

    my @a;

    $HTTP->{'path_info'} = &http_wash($ENV{'PATH_INFO'});
    $HTTP->{'this_url'} = &http_wash(join('', 'http://',
					  $ENV{'SERVER_NAME'},
					  ':', $ENV{'SERVER_PORT'},
					  $ENV{'SCRIPT_NAME'},
					  $ENV{'PATH_INFO'},
					  '?', $ENV{'QUERY_STRING'}));

    foreach ($ENV{'QUERY_STRING'} =~ /([^;&=]+)(?:=([^;&]+)|)/g) {
	push(@a, &http_wash($_));
    }
    $HTTP->{'param'} = {@a};

    $HTTP->{'param'}->{'v'} ||= $HTTP->{'param'}->{'version'};
    $HTTP->{'param'}->{'a'} ||= $HTTP->{'param'}->{'arch'};
    $HTTP->{'param'}->{'i'} ||= $HTTP->{'param'}->{'identifier'};


    $identifier = $HTTP->{'param'}->{'i'};
    $readraw    = $HTTP->{'param'}->{'raw'};
    
    $Conf = new LXR::Config;

    foreach ($Conf->allvariables) {
	$Conf->variable($_, $HTTP->{'param'}->{$_}) if $HTTP->{'param'}->{$_};
    }
    
    &fixpaths($HTTP->{'path_info'} || $HTTP->{'param'}->{'file'});

    if (defined($readraw)) {
	print("\n");
    } else {
        print("Content-Type: text/html; charset=iso-8859-1\n");

	#
	# Print out a Last-Modified date that is the larger of: the
	# underlying file that we are presenting; and the "source" script
	# itself (passed in as an argument to this function.)  If we can't
	# stat either of them, don't print out a L-M header.  (Note that this
	# stats lxr/source but not lxr/lib/LXR/Common.pm.  Oh well, I can
	# live with that I guess...)    -- jwz, 16-Jun-98
	#
	my $file1 = $Path->{'realf'};
	my $file2 = $argv_0;

	# make sure the thing we call stat with doesn't end in /.
	if ($file1) { $file1 =~ s@/$@@; }
	if ($file2) { $file2 =~ s@/$@@; }

	my $time1 = 0, $time2 = 0;
	if ($file1) { $time1 = (stat($file1))[9]; }
	if ($file2) { $time2 = (stat($file2))[9]; }

	my $time = ($time1 > $time2 ? $time1 : $time2);
	if ($time > 0) {
	    my @t = gmtime($time);
	    my ($sec, $min, $hour, $mday, $mon, $year,$wday) = @t;
	    my @days = ("Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun");
	    my @months = ("Jan", "Feb", "Mar", "Apr", "May", "Jun",
			  "Jul", "Aug", "Sep", "Oct", "Nov", "Dec");
	    $year += 1900;
	    $wday = $days[$wday];
	    $mon = $months[$mon];
	    # Last-Modified: Wed, 10 Dec 1997 00:55:32 GMT
	    print sprintf("Last-Modified: %s, %2d %s %d %02d:%02d:%02d GMT\n",
			  $wday, $mday, $mon, $year, $hour, $min, $sec);
	}

	# Close the HTTP header block.
        print("\n");
    }
    
    
    if (defined($readraw)) {
	open(RAW, $Path->{'realf'});
	while (<RAW>) {
	    print;
	}
	close(RAW);
	exit;
    }

    return($Conf, $HTTP, $Path);
}


sub expandtemplate {
    my ($templ, %expfunc) = @_;
    my ($expfun, $exppar);

    while ($templ =~ s/(\{[^\{\}]*)\{([^\{\}]*)\}/$1\01$2\02/s) {}
    
    $templ =~ s/(\$(\w+)(\{([^\}]*)\}|))/{
	if (defined($expfun = $expfunc{$2})) {
	    if ($3 eq '') {
		&$expfun;
	    } else {
		$exppar = $4;
		$exppar =~ s#\01#\{#gs;
		$exppar =~ s#\02#\}#gs;
		&$expfun($exppar);
	    }
	} else {
	    $1;
	}
    }/ges;

    $templ =~ s/\01/\{/gs;
    $templ =~ s/\02/\}/gs;
    return($templ);
}


# What follows is a pretty hairy way of expanding nested templates.
# State information is passed via localized variables.

# The first one is simple, the "banner" template is empty, so we
# simply return an appropriate value.
sub bannerexpand {
    if ($who eq 'source' || $who eq 'sourcedir' || $who eq 'diff') {
	return($Path->{'xref'});
    } else {
	return('');
    }
}

sub pathname {
    return $Path->{'virtf'};
}


sub treename {
    return $Conf->{'treename'};
}


sub titleexpand {
    if ($who eq 'source' || $who eq 'sourcedir' || $who eq 'diff') {
	return($Conf->sourcerootname.$Path->{'virtf'});

    } elsif ($who eq 'ident') {
	my $i = $HTTP->{'param'}->{'i'};
	return($Conf->sourcerootname.' identfier search'.
	       ($i ? " \"$i\"" : ''));

    } elsif ($who eq 'search') {
	my $s = $HTTP->{'param'}->{'string'};
	return($Conf->sourcerootname.' freetext search'.
	       ($s ? " \"$s\"" : ''));

    } elsif ($who eq 'find') {
	my $s = $HTTP->{'param'}->{'string'};
	return($Conf->sourcerootname.' file search'.
	       ($s ? " \"$s\"" : ''));
    }
}


sub thisurl {
    my $url = $HTTP->{'this_url'};

    $url =~ s/([\?\&\;\=])/sprintf('%%%02x',(unpack('c',$1)))/ge;
    return($url);
}


sub baseurl {
    return($Conf->baseurl);
}

sub dotdoturl {
    my $url = $Conf->baseurl;
    $url =~ s@/$@@;
    $url =~ s@/[^/]*$@@;
    return($url);
}

# This one isn't too bad either.  We just expand the "modes" template
# by filling in all the relevant values in the nested "modelink"
# template.
sub modeexpand {
    my $templ = shift;
    my $modex = '';
    my @mlist = ();
    local $mode;
    
    if ($who eq 'source' || $who eq 'sourcedir') {
	push(@mlist, "<b><i>source navigation</i></b>");
    } else {
	push(@mlist, &fileref("source navigation", $Path->{'virtf'}));
    }
    
    if ($who eq 'diff') {
	push(@mlist, "<b><i>diff markup</i></b>");
	
    } elsif (($who eq 'source' || $who eq 'sourcedir') && $Path->{'file'}) {
	push(@mlist, &diffref("diff markup", $Path->{'virtf'}));
    }
    
    if ($who eq 'ident') {
	push(@mlist, "<b><i>identifier search</i></b>");
    } else {
	push(@mlist, &idref("identifier search", ""));
    }

    if ($who eq 'search') {
	push(@mlist, "<b><i>freetext search</i></b>");
    } else {
	push(@mlist, "<a href=\"$Conf->{virtroot}/search".
	     &urlargs."\">freetext search</a>");
    }
    
    if ($who eq 'find') {
	push(@mlist, "<b><i>file search</i></b>");
    } else {
	push(@mlist, "<a href=\"$Conf->{virtroot}/find".
	     &urlargs."\">file search</a>");
    }
    
    foreach $mode (@mlist) {
	$modex .= &expandtemplate($templ,
				  ('modelink', sub { return($mode) }));
    }
    
    return($modex);
}

# This is where it gets a bit tricky.  varexpand expands the
# "variables" template using varname and varlinks, the latter in turn
# expands the nested "varlinks" template using varval.
sub varlinks {
    my $templ = shift;
    my $vlex = '';
    my ($val, $oldval);
    local $vallink;
    
    $oldval = $Conf->variable($var);
    foreach $val ($Conf->varrange($var)) {
	if ($val eq $oldval) {
	    $vallink = "<b><i>$val</i></b>";
	} else {
	    if ($who eq 'source' || $who eq 'sourcedir') {
		$vallink = &fileref($val, 
				    $Conf->mappath($Path->{'virtf'},
						   "$var=$val"),
				    0,
				    "$var=$val");

	    } elsif ($who eq 'diff') {
		$vallink = &diffref($val, $Path->{'virtf'}, "$var=$val");
		
	    } elsif ($who eq 'ident') {
		$vallink = &idref($val, $identifier, "$var=$val");
		
	    } elsif ($who eq 'search') {
		$vallink = "<a href=\"$Conf->{virtroot}/search".
		    &urlargs("$var=$val",
			     "string=".$HTTP->{'param'}->{'string'}).
				 "\">$val</a>";
		
	    } elsif ($who eq 'find') {
		$vallink = "<a href=\"$Conf->{virtroot}/find".
		    &urlargs("$var=$val",
			     "string=".$HTTP->{'param'}->{'string'}).
				 "\">$val</a>";
	    }
	}
	$vlex .= &expandtemplate($templ,
				 ('varvalue', sub { return($vallink) }));

    }
    return($vlex);
}


sub varexpand {
    my $templ = shift;
    my $varex = '';
    local $var;
    
    foreach $var ($Conf->allvariables) {
	$varex .= &expandtemplate($templ,
				  ('varname',  sub { 
				      return($Conf->vardescription($var))}),
				  ('varlinks', \&varlinks));
    }
    return($varex);
}


sub makeheader {
    local $who = shift;
    $template = undef;
    my $def_templ = "<html><body>\n<hr>\n";

    if ($who eq "sourcedir" && $Conf->sourcedirhead) {
	if (!open(TEMPL, $Conf->sourcedirhead)) {
	    &warning("Template ".$Conf->sourcedirhead." does not exist.");
	    $template = $def_templ;
	}
    } elsif (($who eq "source" || $who eq 'sourcedir') && $Conf->sourcehead) {
	if (!open(TEMPL, $Conf->sourcehead)) {
	    &warning("Template ".$Conf->sourcehead." does not exist.");
	    $template = $def_templ;
	}
    } elsif ($who eq "find" && $Conf->findhead) {
	if (!open(TEMPL, $Conf->findhead)) {
	    &warning("Template ".$Conf->findhead." does not exist.");
	    $template = $def_templ;
	}
    } elsif ($who eq "ident" && $Conf->identhead) {
	if (!open(TEMPL, $Conf->identhead)) {
	    &warning("Template ".$Conf->identhead." does not exist.");
	    $template = $def_templ;
	}
    } elsif ($who eq "search" && $Conf->searchhead) {
	if (!open(TEMPL, $Conf->searchhead)) {
	    &warning("Template ".$Conf->searchhead." does not exist.");
	    $template = $def_templ;
	}
    } elsif ($Conf->htmlhead) {
	if (!open(TEMPL, $Conf->htmlhead)) {
	    &warning("Template ".$Conf->htmlhead." does not exist.");
	    $template = $def_templ;
	}
    }

    if (!$template) {
	$save = $/; undef($/);
	$template = <TEMPL>;
	$/ = $save;
	close(TEMPL);
    }
    
    print(
#"<!doctype html public \"-//W3C//DTD HTML 3.2//EN\">\n",
#	  "<html>\n",
#	  "<head>\n",
#	  "<title>",$Conf->sourcerootname," Cross Reference</title>\n",
#	  "<base href=\"",$Conf->baseurl,"\">\n",
#	  "</head>\n",

    	  &expandtemplate($template,
			  ('title',	\&titleexpand),
			  ('banner',	\&bannerexpand),
			  ('baseurl',	\&baseurl),
			  ('dotdoturl',	\&dotdoturl),
			  ('thisurl',	\&thisurl),
			  ('pathname',	\&pathname),
			  ('treename',	\&treename),
    			  ('modes',	\&modeexpand),
    			  ('variables',	\&varexpand)));
}


sub makefooter {
    local $who = shift;
    $template = undef;
    my $def_templ = "<hr>\n</body>\n";

    if ($who eq "sourcedir" && $Conf->sourcedirtail) {
	if (!open(TEMPL, $Conf->sourcedirtail)) {
	    &warning("Template ".$Conf->sourcedirtail." does not exist.");
	    $template = $def_templ;
	}
    } elsif (($who eq "source" || $who eq 'sourcedir') && $Conf->sourcetail) {
	if (!open(TEMPL, $Conf->sourcetail)) {
	    &warning("Template ".$Conf->sourcetail." does not exist.");
	    $template = $def_templ;
	}
    } elsif ($who eq "find" && $Conf->findtail) {
	if (!open(TEMPL, $Conf->findtail)) {
	    &warning("Template ".$Conf->findtail." does not exist.");
	    $template = $def_templ;
	}
    } elsif ($who eq "ident" && $Conf->identtail) {
	if (!open(TEMPL, $Conf->identtail)) {
	    &warning("Template ".$Conf->identtail." does not exist.");
	    $template = $def_templ;
	}
    } elsif ($who eq "search" && $Conf->searchtail) {
	if (!open(TEMPL, $Conf->searchtail)) {
	    &warning("Template ".$Conf->searchtail." does not exist.");
	    $template = $def_templ;
	}
    } elsif ($Conf->htmltail) {
	if (!open(TEMPL, $Conf->htmltail)) {
	    &warning("Template ".$Conf->htmltail." does not exist.");
	    $template = $def_templ;
	}
    }

    if (!$template) {
	$save = $/; undef($/);
	$template = <TEMPL>;
	$/ = $save;
	close(TEMPL);
    }
    
    print(&expandtemplate($template,
			  ('banner',	\&bannerexpand),
			  ('thisurl',	\&thisurl),
    			  ('modes',	\&modeexpand),
    			  ('variables',	\&varexpand)),
	  "</html>\n");
}

1;
