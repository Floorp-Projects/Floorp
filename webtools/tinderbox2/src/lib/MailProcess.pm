# -*- Mode: perl; indent-tabs-mode: nil -*-

# MailProcess.pm - all the functions which are used by more then one
# mailprocessing program.


# $Revision: 1.14 $ 
# $Date: 2003/08/17 01:44:08 $ 
# $Author: kestes%walrus.com $ 
# $Source: /home/hwine/cvs_conversion/cvsroot/mozilla/webtools/tinderbox2/src/lib/MailProcess.pm,v $ 
# $Name:  $ 


# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/NPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is the Tinderbox build tool.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#

# complete rewrite by Ken Estes for contact info see the
#     mozilla/webtools/tinderbox2/Contact file.
# Contributor(s): 


package MailProcess;

$VERSION = '#tinder_version#';



# Load standard perl libraries
use Time::Local;

# Load Tinderbox libraries




sub parse_mailprocess_args {

  Getopt::Long::config('require_order', 'auto_abbrev', 'ignore_case');

  %option_linkage = (
		     "version" => \$version,
		     "help" => \$help,
		     "skip-check" => \$main::SKIP_CHECK,
		     "force-time" => \$main::FORCE_TIME,
		    );


  Getopt::Long::GetOptions (\%option_linkage, qw(
		  version! help! force-time! skip-check!
		)) ||
		    die("Illegal options in \@ARGV: '@ARGV',");

  if ($version) {
    print "$0: Version: $VERSION\n";
    exit 0;  
  }
  
  if ($help) {
    main::usage();
  }

  # For security purposes we may wish to disable web access to the mail
  # programs but I have also found it useful to bypass sendmail and have
  # bugzilla run the mail processors directly.

  #  if ($ENV{"REQUEST_METHOD"}) {
  #    die("$0: is not a web program, can not be run from the webserver\n");
  #  }

  return 1;
} # parse_args


sub fatal_mailprocessing_error {
# all fatal errors come through this function

# log errors to the log file on the server end and send them to stderr
# so that sendmail can put the error message in the bounced mail.

  *LOG = *main::LOG;
  
  my  @error = @_;
  foreach $_ (@error) {
    print LOG "[$main::LOCALTIME] $_";
    print STDERR "[$main::LOCALTIME] $_";
  }
  print LOG "\n";
  print STDERR "\n";

  close(LOG);
  exit 9; 
}



sub parse_mail_header {

# remove the headers from the input stream. Currently only use is in
# error handling for external trees.  

# This function does not use any other functions and can not fail.

# I hope to put some sanity checks into the code. 

# 1) build machines should not lie in the buildname line about their
#    hostname

# 2) check the time the mail was sent with the time we recieved it

  my (%mail_header) =();

  my $line = '';
  my $name = '';
  my ($header, $value) = ();
  while (defined($line = <>) ) {
    chomp($line);

    # if line is empty, there are no more headers

    !($line) &&
      return %mail_header;

    # if the line does not begin with a tab and has a colon then it is
    # of the form header: value.  The header may contain _, -, and god
    # knows what else.

    if( $line =~ /^([-_\w]*)\:[ \t]+([^\n]*)$/ ){
      my ($var, $value) = ($1, $2);
      
      $var = main::extract_printable_chars($var);
      $value = main::extract_printable_chars($value);
      $value =~ s/\s+$//;
      $value =~ s/^\s+//;
      
      ($value) || next;
      ($var) || next;
      
      $mail_header{$var} .= $value;
    }
    
    # if the line begins with a tab then it is just a continuation of
    # the Received: header

    if( $line =~ /^\t/ ){
      $mail_header{$header} .= $line;
    }
  }

  # not reached
  return ;
}



sub parse_tinderbox_vars {
    my ($error_context) = @_;

# Ignore all lines in the mail till we find the tinderbox lines, so
# the lines we want should be at the top of the mail.  We ignore the
# line following the variables and hope it is blank.  Each variables
# vaules is the concatination of all appearances of the variable, this
# allows long varibles can be split to multiple lines.


# This function does not use any other functions and failures are
# expected to be rare.

# Example input:

#
# tinderbox: tree: Project_A
# tinderbox: starttime: 934395485
# tinderbox: timenow: 934395489
# tinderbox: status: success
# tinderbox: buildname: worms-SunOS-sparc-5.6-Depend-apprunner
# tinderbox: errorparser: unix
# tinderbox: buildfamily: unix
# tinderbox: END
#

  my (%tinderbox) = ();
  my ($found_vars)= 0;
  my ($line) = '';

  while(defined($line = <>)) {
    
    if ( $line =~ m/^\s*tinderbox\s*:\s*([a-zA-Z0-9-_]*)\s*:\s*(.*)\n$/ ) {
      my ($var, $value) = ($1, $2);
      
      $var = lc(main::extract_printable_chars($var));
      $value = main::extract_printable_chars($value);
      $value =~ s/\s+$//;
      $value =~ s/^\s+//;
      
      ($value) || next;
      ($var) || next;

      $tinderbox{$var} = $value;
      $found_vars= 1;
    } elsif ( $line =~ m/^tinderbox\s*:\s*END\b/ ) {
      return %tinderbox;
    } elsif ($found_vars) {

      # If we have seen at least one varaible then we see something
      # which is not a variable, we are done.

      return %tinderbox;
    }

  }

  # bounce the mail message

  die("No Tinderbox variables found in message.\n".
      "$error_context\n");
}



# create a string which shows the contents of the tinderbox variables.
# this output is appended to the log file, to show what values were
# generated from the log.

sub format_tinder_vars {
  my (%tinderbox) = @_;

  my $output ='';
  
  foreach $key (sort keys %tinderbox) {    
    $output .= HTMLPopUp::escapeHTML("tinderbox\: $key\: $tinderbox{$key}\n");
  }

  return $output
}



sub monthstr2mon {
  my ($month_str) = @_;

  @MONTHS = qw( Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec );

  foreach $i (0 .. $#MONTHS) {
    ($month_str eq $MONTHS[$i]) && 
      return $i;
  }

  return undef;
} # monstr2mon


sub mailstring2time {
  # convert a string describing the date to the time() format

  # It would be better to use Date::Manip but It is a requirement that
  # we not use perl libraries which do not come with perl.

  my ($string) = @_;

  # the string looks something like
  # Date: Sun, 17 Sep 2000 07:26:42 -0700 (PDT)
  # Date: Sun, 28 Feb 1999 08:36:38 +0800 (CST)
  # Date: Thu, 3 Sep 1998 22:14:08 -0400 (EDT)
  
  my ($mday, $month_str, $year, $hour, $min, $sec, $tz_offset) = ();

  if ( $string =~ m/
       (\d?\d)               # day of month
       \s+
       (\w\w\w)              # month
       \s+
       (\d\d\d\d)            # year
       \s+
       (\d?\d):(\d?\d):(\d?\d)  #time 
       \s+
       ([\+\-]\d\d\d\d)      # timezone offset
       /x) {
    
    ($mday, $month_str, $year, $hour, $min, $sec, $tz_offset) = 
      ($1, $2, $3, $4, $5, $6, $7);
  }

  my ($month) = monthstr2mon($month_str);
  my ($hour_offset) = ($tz_offset/100) * (60*60);

  # remember that the local time of the machine doing the conversion
  # may not be in the same time zone as the string we are processing.

  my $time = undef;
  ($time) = timegm($sec,$min,$hour,$mday,$month,$year);
  $time -= $hour_offset;

  return $time;
} # mailstring2time



sub write_update_file {
  my ($update_type, $uniq_id, $tree, %tinderbox) = @_;

  # write out an build update file so that the server to know about
  # the new info.

  # Notice that we write this out after the WHOLE log has been parsed.
  # We may add to and modify the hash during the processing of the log.
  
  my ($update_file) = (FileStructure::get_filename($tree, 'TinderDB_Dir').
                       "/$update_type.Update.$uniq_id");

  # first we try and make the file name legal, then we check by using
  # the definiative legal filename checker.

  $update_file =~ s/([^0-9a-zA-Z\.\-\_\/\:]+)/\./g;

  $update_file = main::extract_safe_filename($update_file);

  # We are done, tell the tinderserver about this build.  

  Persistence::save_structure( 
                              \%tinderbox,
                              $update_file,
                             );

  return ;
}

1;
