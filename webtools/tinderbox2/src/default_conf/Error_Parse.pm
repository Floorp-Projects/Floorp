# -*- Mode: perl; indent-tabs-mode: nil -*-

# Error_Parse.pm - Used by processmail to turn the build logs into
# HTML.  Contains the parsing functions for highlighting the build
# errors and creating links into the source code where the errors
# occurred.

# $Revision: 1.5 $ 
# $Date: 2001/03/26 14:01:26 $ 
# $Author: kestes%tradinglinx.com $ 
# $Source: /home/hwine/cvs_conversion/cvsroot/mozilla/webtools/tinderbox2/src/default_conf/Error_Parse.pm,v $ 
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

# complete rewrite by Ken Estes:
#	 kestes@staff.mail.com Old work.
#	 kestes@tradinglinx.com New work.
#	 kestes@walrus.com Home.
# Contributor(s): 




package Error_Parse;

# This package must not use any tinderbox specific libraries.  It is
# intended to be a base class.

$VERSION = '#tinder_version#';

# the rest of the code does not depend on knowing the different
# line_types. We may need to add new types like:
#   'warning_style', 'warning_fix_required', 
#   'error_test', 'error_test_performance'

# These new types would be useful for the "build warnings page" since
# not every warning may be considered interesting by QA.  Allowing a
# flexible error type will allow QA to define types of warnigns of
# interest and other warnings may only be interesting during
# particular types of builds.

# If new types are added try and keep to a small set of colors or the
# display will get confusing.  You may find it convienent to keep a
# distinction between different kinds of warnings or different kinds
# of tests but all warnings and all tests get the same color.


# Be careful when changing the function line_type.  An improperly
# tuned version can take up 50% (as shown by perl -d:DProf ) of the
# execution time.

%LINE_TYPE2COLOR = (
                    'error' => "navy",
                    'warning' => "maroon",
                    'info' => "black",
                   );

# This block adjusts how we format the error logs, perhaps it belongs
# in another file and not the error_parse file.  Processmail is a
# candidate.

{

# window of context arround error message,  for summary log
# created by Error_Parse.pm and processmail

$LINES_AFTER_ERROR = 5;

$LINES_BEFORE_ERROR = 30;

# number of characters width the line number gets in HTML pages
# (mostly the build log pages)

$LINENO_COLUMN = 6;

}


package Error_Parse::unix;


sub line_type {
  my ($line) = @_;

  $error = (

            ($line =~ /\sORA-\d/)		||		# Oracle
            ($line =~ /\bNo such file or directory\b/)	||
            ($line =~ /\b[Uu]nable to\b/)	||		
            ($line =~ /\bnot found\b/)		||		# shell path
            ($line =~ /\b[Dd]oes not\b/)	||		# javac error
            ($line =~ /\b[Cc]ould not\b/)	||		# javac error
            ($line =~ /\b[Cc]an\'t\b/)		||		# javac error
            ($line =~ /\b[Cc]an not\b/)		||		# javac error
            ($line =~ /\b\[javac\]\b/)		||		# javac error
            # Remember: some source files are called $prefix/error.suffix
            ($line =~ /\b(?<!\/)[Ee]rror(?!\.)\b/)||		# C make error
            ($line =~ /\b[Ff]atal\b/)		||		# link error
            ($line =~ /\b[Ee]xception\b/)	||		# javac error
            ($line =~ /\b[Dd]eprecated\b/)	||		# java error
            ($line =~ /\b[Aa]ssertion\b/)	||		# test error
            ($line =~ /\b[Aa]borted\b/)		||		# cvs error
            ($line =~ /\b[Ff]ailed\b/)		||		# java nmake

            ($line =~ /Unknown host /)		||		# cvs error
            ($line =~ /\: cannot find module/)	||		# cvs error
            ($line =~ /\^C /)			||		# cvs merge conflict
            ($line =~ /Couldn\'t find project file /)	 ||	# CW project error
            ($line =~ /Creating new precompiled header/) ||	# Wastes time.
            ($line =~ /No such file or directory/)	 ||	# cpp error
            ($line =~ /jmake.MakerFailedException:/) ||         # Java error
            0);

  if ($error) {
    return('error');
  }
  
  $warning = (

              ($line =~ m/^[-._\/A-Za-z0-9]+\.[A-Za-z0-9]+\:[0-9]+\:/) ||
              ($line =~ m/^\"[-._\/A-Za-z0-9]+\.[A-Za-z0-9]+\"\, line [0-9]+\:/) ||

              ($line =~ m/\b[Ww]arning\b/) ||
              ($line =~ m/not implemented:/) ||

              0);

  if ($warning) {
    return('warning');
  }
  
  return('info');
}


sub parse_errorline {
    local( $line ) = @_;

    if( $line =~ /^(([A-Za-z0-9_]+\.[A-Za-z0-9]+)\:([0-9]+)\:)/ ){
      my ($error_msg) = $1;
      my ($error_file_ref) = $2;
      my ($error_line) = $3;
      return ($error_file_ref, $error_line);
    }
    if ( $line =~ /^(\"([A-Za-z0-9_]+\.[A-Za-z0-9]+)\"\, line ([0-9]+)\:)/  ){
      my ($error_msg) = $1;
      my ($error_file_ref) = $2;
      my ($error_line) = $3;
        return ($error_file_ref, $error_line);
    }
    return undef;
}

package Error_Parse::windows;

sub line_type {
  my ($line) = @_;

  $error = (

            ($line =~ /\b[Ee]rror\b/)		||		# C make error
            ($line =~ /\b[Ff]atal\b/)		||		# link error
            ($line =~ /\b[Aa]ssertion\b/)	||		# test error
            ($line =~ /\b[Aa]borted\b/)		||		# cvs error
            ($line =~ /\b[Ff]ailed\b/)		||		# java nmake

            ($line =~ /Unknown host /)		||		# cvs error
            ($line =~ /\: cannot find module/)	||		# cvs error
            ($line =~ /\^C /)			||		# cvs merge conflict
            ($line =~ /Couldn\'t find project file /) ||	# CW project error
            ($line =~ /Creating new precompiled header/) ||	# Wastes time.
            ($line =~ /No such file or directory/) ||		# cpp error

    0);

  if ($error) {
    return('error');
  }
  
  $warning = (
              ($line =~ m/^[-._\/A-Za-z0-9]+\.[A-Za-z0-9]+\:[0-9]+\:/) ||
              ($line =~ m/^\"[-._\/A-Za-z0-9]+\.[A-Za-z0-9]+\"\, line [0-9]+\:/) ||
              ($line =~ m/\bwarning\b/) ||
              ($line =~ m/not implemented:/) ||
              0);

  if ($warning) {
    return('warning');
  }

  return('info');
}



sub parse_errorline {
    local( $line ) = @_;

    if( $line =~ m@(ns([\\/][a-z0-9\._]+)*)@i ){
      my $error_file = $1;
      my $error_file_ref = lc $error_file;
      $error_file_ref =~ s@\\@/@g;
      
      $line =~ m/\(([0-9]+)\)/;
      my $error_line = $1;
      return ($error_file_ref, $error_line);
    }

    if( $line =~ m@(^([A-Za-z0-9_]+\.[A-Za-z])+\(([0-9]+)\))@ ){
      my $error_file = $1;
      my $error_file_ref = lc $2;
      my $error_line = $3;
      $error_file_ref =~ s@\\@/@g;
      return ($error_file_ref, $error_line);
    }

    return ;
}


package Error_Parse::mac;



sub line_type {
  my ($line) = @_;

  $error = (
            ($line =~ /\b[Ee]rror\b/)		||		# C make error
            ($line =~ /\b[Ff]atal\b/)		||		# link error
            ($line =~ /\b[Aa]ssertion\b/)	||		# test error
            ($line =~ /\b[Aa]borted\b/)		||		# cvs error
            ($line =~ /\b[Ff]ailed\b/)		||		# java nmake

            ($line =~ /Unknown host /)		||		# cvs error
            ($line =~ /\: cannot find module/)	||		# cvs error
            ($line =~ /\^C /)			||		# cvs merge conflict
            ($line =~ /\bCouldn\'t find project file /) ||	# CW project
            ($line =~ /\bCan\'t (create)|(open)|(find) /) ||	# CW project error
    0);

  ($error) && 
    return('error');
  
  $warning = (
              ($line =~ m/^[-._\/A-Za-z0-9]+\.[A-Za-z0-9]+\:[0-9]+\:/) ||
              ($line =~ m/^\"[-._\/A-Za-z0-9]+\.[A-Za-z0-9]+\"\, line [0-9]+\:/) ||
              ($line =~ m/warning/i) ||
              ($line =~ m/not implemented:/i) ||
              0);

  ($warning) &&
    return('warning');

  return('info');
}




sub parse_errorline {
    local( $line ) = @_;

    if( $line =~ /^(([A-Za-z0-9_]+\.[A-Za-z0-9]+) line ([0-9]+))/ ){
        my $error_file = $1;
        my $error_file_ref = $2;
        my $error_line = $3;
      return ($error_file_ref, $error_line);
    }
    return ;
}


=head1 NAME

Error_Parser - methods used by showlogs.cgi

=head1 SYNOPSIS

C<require Error_Parse::unix;>

=head1 DESCRIPTION

The methods provided by this package are designed to be used in
conjunction with showlogs.cgi.  This file contains code for parsing
out the error messages of various build tools.  There are different
name spaces for each build type and a set of parsing programs in each
namespace.  Currently build types are the major OS (Unix, Mac,
Windows).  It may be possible in the future to have a single universal
parsing program or alternatly to have build types specify a list of
parsing programs for each tool (compler, make language, script) which
is used in the build process then a build would also need to specify a
list of tools that are used.  This may be necessary as people run unix
compilers on NT and vice versa.


=head1 METHODS

=over 2

=item line_type

returns a string discribing if the line has any errors or warnings.
The list of types may grow in the future as some warnings become more
important then others.  The possible return codes are:

	'error'
	'warning'
	'info'


=cut


=item has_errorline

returns undef if the input line does not contains a parsable error.

If the line contains a parsable error it returns the list

	($error_file_ref, $error_line);

where:


$error_file_ref: the file which has the error

$error_line:  the line the error was found on

=head1 AUTHOR

Ken Estes (kestes@staff.mail.com)

=cut

1;

