#!/usr/bin/perl -w
#
# whack-license.pl - look at a file, decide what version of the [MN]PL it
# has, if any, and optionally update that boilerplate text.
#
# Dan Mosedale
# dmose@mozilla.org
#

# 
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
# 
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
# 
# The Original Code is whack-license.pl.
# 
# The Initial Developer of the Original Code is Netscape Communications
# Corp. Portions created by Netscape Communications Corp. are
# Copyright (C) 1999, Netscape Communications Corp. All
# Rights Reserved.
# 
# Contributor(s): Dan Mosedale <dmose@mozilla.org>
# 

#
# invocation options
# 
# -c  force a " */\n" to be added in after the license
# -d  turn on debugging
# -g  when updating, add the NPL/GPL alternate licensing clause
# -n  don't actually write out any files.
# -q  be quiet
# -u  update any old licenses found
# -A string  set the first part of the  "Initial Developer" clause 
#            to "string" if one is not found in the original boilerplate
#            (default is to fail for that file)
# -B string  set the second part of the  "Initial Developer" clause 
#            to "string" if one is not found in the original boilerplate
#            (default is to fail for that file)
# -C string  set the "created by" clause  to "string" if one is not found 
#            in the original boilerplate (default is to fail for that file)
# -D string  set the copyright dates clause to "string" if one is not found 
#            in the original boilerplate (defaults is to fail for that file)
# -E string  set the "copyright by" clause to "string" if one is not found 
#            in the original boilerplate (defaults is to fail for that file)
#

# XXX - weird special cases to be dealt with by hand: 
#
#  directory/perldap/README, MPL-*.txt
#  expat/expat.html
#  extensions/transformiix
#  network/cache/nu/tests/cb/nsTimeIt.h    
#  intl/lwbrk/tools/anzx4501.pl
#  intl/lwbrk/src/jisx4501class.h
#

# some configurables
#
####################
#
# we've got ugly regexps, so here's where to turn on regexp debugging,
# if necessary
#
#use re 'debugcolor';
#
# regexp to use as a guess as to weather there's anything related to the
# licenses we care about at all
#
my $licGuessExp = '\W(M|N)PL\W';
#
# size, in bytes, of the buffer used to read in sources files for modification.
# this must be big enough to hold the entire license header and everything
# that precedes in it any given source file.
#
my $bufsiz = 10000;
#
# Files with these suffixes will be ignored and not checked.
#
my @IGNORESUFFIXES = ( "Entries", "GenToc", "GenJSLib", 
		       "MacCVSLib", "MANIFESTOLib", 
		       "Repository", "Root", "TestGenJSLib",
		       ".a", ".bkp", ".bmp", 
		       ".class", "control", "core", ".cvsignore", 
		       ".dll", "edit.cgi", ".gif", ".jpg", ".mcp", "missing",
		       ".o", ".png", ".pp", "release.txt",
		       ".rsrc", ".so", ".spec",
		       ".spec.in", ".timestamp", ".xpt" );

####################
# prerequisites
#
use Getopt::Std;
use strict;

# set up vars
#
use vars qw/$opt_c $opt_n $opt_q $opt_d $opt_u $opt_g $opt_A $opt_B $opt_C
            $opt_D $opt_E/;
my @INFILE;

# parse the command line
#
getopts('cdgnquA:B:C:D:E:');

# do this for all requested files
#
foreach my $filename ( @ARGV ) {

    my $foundGuess = 0;
    my $foundUnique = "";

    # 
    # skip files that end in a suffix listed in @IGNORESUFFIXES
    #
    next if ( grep  
	      { substr($filename, -1 * length($_) ) eq $_ } 
	      @IGNORESUFFIXES );

    # open the file and read it into an array
    #
    open(INFILE, $filename) || die "could not open $_";
    @INFILE = <INFILE>;

    # traverse the file, looking for a reference to the license
    #
  SCANFILE:
    foreach ( @INFILE ) {
      if ( /$licGuessExp/o ) {
	
	$foundGuess = 1;
	
	# traverse it again, looking for unique license text
	#
	foreach ( 0 .. $#INFILE ) {
	  
	  if ( $INFILE[$_] =~ /Netscape Public License *\r?$/ &&
	       $INFILE[$_ + 1] =~ 
	       /^(.{0,10})Version 1\.0 *\(the ["'](License|NPL)['"]\)/ )
	    {
	      
	      $foundUnique = "NPL 1.0";
	      last SCANFILE;
	      
	    } elsif ( $INFILE[$_] =~ /Netscape Public *\r?$/ &&
		      $INFILE[$_ + 1] =~ 
	    /^(.{0,10})License Version 1\.1 *\(the ["'](License|NPL)['"]\)/ 

               	      ||

		      $INFILE[$_] =~ /Netscape Public License *\r?$/ &&
                      $INFILE[$_ + 1] =~ 
	              /^(.{0,10})Version 1\.1 *\(the ["'](License|NPL)['"]\)/ 

		      ||

		      $INFILE[$_] =~ /Netscape Public License Version *$/ &&
                      $INFILE[$_ + 1] =~ 
	              /^(.{0,10})1\.1 *\(the ["'](License|NPL)['"]\)/ 

                     )

	      {
		$foundUnique = "NPL 1.1";
		last SCANFILE;
		
	      } elsif ( $INFILE[$_] =~ /Mozilla Public License *$/ &&
			$INFILE[$_ + 1] =~
		      /^(.{0,10})Version 1\.0 *\(the ["'](License|MPL)['"]\)/ )
		{
		  $foundUnique = "MPL 1.0";
		  last SCANFILE;
		  
		} elsif ( $INFILE[$_] =~ /Mozilla Public License *$/ &&
			  $INFILE[$_ + 1] =~
		      /^(.{0,10})Version 1\.1 *\(the ["'](License|MPL)['"]\)/ 

		          ||

		          $INFILE[$_] =~ /Mozilla Public *$/ &&
			  $INFILE[$_ + 1] =~
             q/^(.{0,10})License Version 1\.1 *\(the ["'](License|MPL)['"]\)/ 
                        )
		  {
		    $foundUnique = "MPL 1.1";
		    last SCANFILE;
		  }

	}
	last SCANFILE;
      }
    }

    # tell the world what we found (unless we're being quiet)
    #
    if ( !$opt_q ) {
	if ( $foundUnique ne "" ) {
	    print "$filename contains $foundUnique unique license text";
	} elsif ( $foundGuess == 1 ) {
	    print "$filename mentions a relevant license, " . 
	      "but is missing expected unique text";
	} else {
	    print "$filename contains no relevant license reference";
	} 
    }

    # update the file (if invoked with -u)
    #
    if ( $opt_u && $foundUnique ) {
      
      my $updated = 0;           # did we actually update anything?

      # go back to beginning of file
      #
      seek (INFILE, 0, 0) ||  
	die ("couldn't seek to beginning of $filename");

      # read the file head into $buffer
      # 
      read INFILE, my($buffer), $bufsiz;
      die ("error reading from $filename")
	if ( !defined($buffer));

      # update
      #
      if ( $foundUnique eq "NPL 1.0" ) {
	
	# replace the necessary license text
	#
	if ( !update10($buffer) ) {
	  $updated = -1;
	} else {
	  $updated = 1;
	}
	;

      } elsif ( $foundUnique eq "NPL 1.1" ) {

	# NPL 1.1 stuff is (currently) considered up-to-date, so 
	# don't do anything.
	#
	;

      } elsif ( $foundUnique eq "MPL 1.0" ) {
	
	# replace the necessary license text
	#
	if ( !update10($buffer) ) {
	  $updated = -1;
	} else {
	  $updated = 1;
	}
	;

      } elsif ( $foundUnique eq "MPL 1.1" ) {

	# MPL 1.1 stuff is (currently) considered up-to-date, so 
	# don't do anything.
	#
	;

      } else {
	die ("Internal error: unknown license type");
      }

      # if we actually updated the license, it's time to finish up by
      # writing the file and telling the user
      #
      if ( $updated == 1 ) {

	# unless we've specifically been told to do nothing, write out the
	# updated file.
	#
	if (! $opt_n ) {

	  # write the file
	  #
	  open OUTFILE,">${filename}.new" ||
	    die ("couldn't open $filename.new");
	  print OUTFILE $buffer || 
	    die ("error writing header to $filename.new");
	  while ( <INFILE> ) {
	    print OUTFILE $_ || 
	      die ("error writing body to $filename.new");
	  }
	  close OUTFILE || die("couldn't close $filename.new after writing");
	  rename ($filename, "$filename.bkp") || 
	    die("couldn't rename $filename to $filename.bkp");
	  rename ("$filename.new", $filename) ||
	    die("couldn't rename $filename.new to $filename");
	}

	if ( !$opt_q ) {
	  print "... updated.";
	}

      } elsif ( $updated == -1 ) {

	# die ("regexp replacement error attempting to update $filename");

	if ( !$opt_q) {
	  print "... failed.";
	}
      }

    }
    print "\n" if ( !$opt_q );

    # close the file
    #
    close(INFILE) || die("couldn't close $filename after reading");
}

# all done!
#
exit 0;

# the return value here is only meaningful in the sense of whether 
# it's 0 or non-zero.
#
sub update10 ($) {

  my $gplTag;  
  my $ecrTag = "";
  my $cccTag = "XXX__CCCTAG__XXX";
  my $needCCommentClose = 0;
  my $ocTag = "mozilla.org code.";
  my $initDevATag = "";
  my $initDevBTag = "";
  my $createdByTag = "";
  my $datesTag = "";
  my $copyrightByTag = "";

  # in the mongo-huge s// statement, some of the positional match vars
  # (eg $2, $3, $4, $6) do not get defined.  however, whenever this is
  # the case, it just so happens that we want a null string to be
  # inserted anyway, so unless debugging is turned on, we turn off
  # warnings, in order to suppress the "uninitialized variable".
  #
  if ( ! $opt_d ) {
    $SIG{__WARN__} = sub { return };
  }

  # if we need to add on GPL alternate licensing, this is a temporary
  # tag to help us do so.
  #
  if ( $opt_g ) {
    $gplTag = "XXX__GPLTAG__XXX";
  } else {
    $gplTag = "";
  }

  my $result =
    $_[0] =~ 

s{(^.{0,10}?) ?The contents of this (file|directory) ar(?:handle)?e subject to the (Netscape|Mozilla) Public License {0,5}
(^.{0,10}?) ?Version 1\.0 ?\(the ["'](?:License|NPL)['"]\)(?:=0)?;? you may not use (this file|the files in this directory) except(?: in compliance {0,5}
\4 ?with | in {0,5}
\4 ?compliance with | {0,5}
\4 ?in compliance with )the (?:License|NPL)\.[ \t]{1,4}You may obtain a copy of the (?:License|NPL) at {0,5}
\4 ?http:/{1,2}wwwt?\.m5?ozilla\.org/([MN])P(?:L/|\\|L/\.) {0,5}
(?:.{0,10}) {0,25}(?:
\4 ?Software distributed under the License is distributed on an ['"]AS IS["'] {0,5}
\4 ?basis, WITHOUT WARRANTY OF ANY KIND, either express or implied\. {1,2}See(?: the {0,5}
\4 ?License| {0,5}
\4 ?the License) for the specific language governing rights and limitations {0,5}
\4 ?under the License\. {0,5}|
?\4 ?Software distributed under the (?:NPL|License) is distributed on an ['"]AS IS["'] basis, {0,5}
\4 ?WITHOUT WARRANTY OF ANY KIND, either (?:express|dtd) or implied\. See the (?:NPL|License)(?: {0,5}
{1,3}?\4 ?for | for {0,5}
\4 ?)the specific language governing (?:righ|lef)ts and limitations under theg?(?: License\. {0,5}| {0,5}
\4 ?(?:NPL|License)\.) {0,5})(?:
(?:.{0,10}) {0,4})?(?:
\4 ?The [Oo]riginal [Cc]ode is (.+) {0,5}
(?:(\4)( ?)(.+
))?(?:.{0,10}))?(?:
\4 ?The Initial Developers? of (?:the Original Code|this code under the [MN]PL) (?:is|are) ?(.*) {0,5}
\4 ?(.*(?:\. )?) {0,2}Portions created by (.*) are Copyright \(C\) (199[6-9]|1998-1999|(?:1997, )?1998, ?1999|1997-1999)(?: (\b.*?)\.?)? {0,5}
\4 ?(.*?)\.? {0,2}All Rights Reserved\.|
\4 ?The Initial Developers? of this code under the [MN]PL (?:is|are) ?(.*) {0,5}
\4 ?(.*(?:\. )?)[ \t]{0,4}Portions created by (.*) are {0,5}
\4 ?Copy(?:righ|lef)t ?\(C\) (199[6-9]|1998-1999|1998, ?1999|1997-1999) (.*)\.[ \t]{1,4}All (?:Righ|lef)ts {0,5}
\4 ?Reserved\.1?|
\4 ?The Initial Developers? of (?:the Original Code|this code under the [MN]PL) (?:is|are) ?(.*) {0,5}
\4 ?(.*(?:\. )?) {0,2}Portions created by (.*) are(?: Copyright)? {0,5}
?\4? ?(?:Copyright )?\(C\) (199[6-9]|1998-1999|1998, ?1999|1997-1999) (.*)\. {1,2}All Rights
?\4? ?Reserved\.|
|)(?:( \*/)? {0,5}
)(?:(?:\4 {0,5}
)?\4 ?Contributor\(?s?\)?: ?((?s:.*?))
)?((?!\4)|\4/? ?
)?}{$1 The contents of this $2 are subject to the $3 Public
$4 License Version 1.1 (the "License"); you may not use $5
$4 except in compliance with the License. You may obtain a copy of
$4 the License at http://www.mozilla.org/$6PL/
$4
$4 Software distributed under the License is distributed on an "AS
$4 IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
$4 implied. See the License for the specific language governing
$4 rights and limitations under the License.
$4
$4 The Original Code is XXX__OCTAG__XXX
$8$9$10$4
$4 The Initial Developer of the Original Code is XXX__INITDEVATAG__XXX
$4 XXX__INITDEVBTAG__XXXPortions created by XXX__CREATEDBYTAG__XXX are
$4 Copyright (C) XXX__DATESTAG__XXX XXX__COPYRIGHTBYTAG__XXXXXX__ENDCOPYRIGHTTAG__XXX. All
$4 Rights Reserved.
$4
$4 Contributor(s): $28
$gplTag$29$cccTag}m;

  # restore the usual warning behavior
  #
  delete $SIG{__WARN__};

  # if something went wrong, give up
  #
  if ( !$result ) { return $result; }

  # set the initial developer clause to be whatever we found in the 
  # match.  
  #
  $initDevATag = "$11$17$22";

  # if we didn't find anything, and a string was given on the command-line
  # with -A, use that.
  #
  if ( $initDevATag eq "" ) 
  {
    if ( $opt_A ne "" ) 
      {
	$initDevATag = $opt_A
      }
  }

  # set the initial developer clause to be whatever we found in the 
  # match.  
  #
  $initDevBTag = "$12$18$23";

  # if we didn't find anything, and a string was given on the command-line
  # with -B, use that.
  #
  if ( $initDevBTag eq "" ) 
  {
    if ( $opt_B ne "" ) 
      {
	$initDevBTag = $opt_B
      }
  }

  # but if BOTH A and B tags remain unset, abort
  #
  if ( $initDevATag eq "" and $initDevBTag eq "" ) 
  {
    return 0;
  }

  # set the created by tag to be whatever we found in the match.  if
  # we didn't find anything, and were supplied with a default on the
  # command-line by -C, use that.  otherwise abort.
  #
  $createdByTag = "$13$19$24";
  if ( $createdByTag eq "") 
  {
    if ( $opt_C ne "" ) 
      {
	$createdByTag = $opt_C;
      }
  }
  return 0 if ( $createdByTag eq "" );

  # set the dates tag to be whatever we found in the match.  if
  # we didn't find anything, and were supplied with a default on the
  # command-line by -D, use that.  otherwise abort.
  #
  $datesTag = "$14$20$25";
  if ( $datesTag eq "") 
  {
    if ( $opt_D ne "" ) 
      {
	$datesTag = $opt_D;
      }
  }
  return 0 if ( $datesTag eq "" );


  # set the "copyright by" tag to be whatever we found in the match.  if
  # we didn't find anything, and were supplied with a default on the
  # command-line by -E, use that.  otherwise abort.
  #
  $copyrightByTag = "$15$21$26";
  if ( $copyrightByTag eq "") 
  {
    if ( $opt_E ne "" ) 
      {
	$copyrightByTag = $opt_E;
      }
  }

  # if there was a final clause in the copyright, we need to add
  # a space before it
  #
  if ( $16 ne "" ) 
  {
    if ( $copyrightByTag ne "" ) 
      {
	$ecrTag = " " . $16;
      } else {
	$ecrTag = $16; 
      }
  }
  
  return 0 if ( $copyrightByTag eq "" and $ecrTag eq "");

  # some copies of the NPL have the C-language comment close sequence
  # incorrectly wrapped into the license text.  we notice that here.
  #
  if ( $27 eq " */" ) { $needCCommentClose = 1 };

  # if there wasn't an "Original Code" clause in the original, 
  # add a generic one now
  #
  if ( $7 ne "" ) { $ocTag = $7; }
    
  # insert the Original Code clause
  #
  $result = $_[0] =~ s!XXX__OCTAG__XXX!$ocTag!;
  if ( !$result ) { return $result; }

  # insert the first part of the initial developer tag
  #
  $result = $_[0] =~ s!XXX__INITDEVATAG__XXX!$initDevATag!;
  if ( !$result ) { return $result; }

  # insert the second part of the initial developer tag
  #
  $result = $_[0] =~ s!XXX__INITDEVBTAG__XXX!$initDevBTag!;
  if ( !$result ) { return $result; }

  # insert the "created by" tag
  #
  $result = $_[0] =~ s!XXX__CREATEDBYTAG__XXX!$createdByTag!;
  if ( !$result ) { return $result; }

  # insert the "copyright by" tag
  #
  $result = $_[0] =~ s!XXX__COPYRIGHTBYTAG__XXX!$copyrightByTag!;
  if ( !$result ) { return $result; }

  # insert the "dates" tag
  #
  $result = $_[0] =~ s!XXX__DATESTAG__XXX!$datesTag!;
  if ( !$result ) { return $result; }


  # insert the ending of the copyright tag 
  #
  $result = $_[0] =~ s!XXX__ENDCOPYRIGHTTAG__XXX!$ecrTag!;
  if ( !$result ) { return $result; }

  # 
  # add in the GPL alternate license clause, if requested
  #
  if ( $opt_g ) {

    # remember the comment prefix
    #
    my $cp = $4;

    $result = 
      $_[0] =~ 

s!XXX__GPLTAG__XXX!$cp
$cp Alternatively, the contents of this file may be used under the
$cp terms of the GNU Public License (the "GPL"), in which case the
$cp provisions of the GPL are applicable instead of those above.
$cp If you wish to allow use of your version of this file only
$cp under the terms of the GPL and not to allow others to use your
$cp version of this file under the NPL, indicate your decision by
$cp deleting the provisions above and replace them with the notice
$cp and other provisions required by the GPL.  If you do not delete
$cp the provisions above, a recipient may use your version of this
$cp file under either the NPL or the GPL.
!;
   }

  # if we need a c comment closer, substitute it in here
  #
  if ( $needCCommentClose || $opt_c ) {
    $result =  $_[0] =~ s!XXX__CCCTAG__XXX! */\n!;

  # otherwise, get rid of the tag that we put in earlier.
  #
  } else {
    $result =  $_[0] =~ s!XXX__CCCTAG__XXX!!;
  }

  return $result;

}
