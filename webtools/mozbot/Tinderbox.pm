# -*- Mode: perl; indent-tabs-mode: nil -*-
#
# The contents of this file are subject to the Mozilla Public License
# Version 1.0 (the "License"); you may not use this file except in
# compliance with the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
# 
# Software distributed under the License is distributed on an "AS IS"
# basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
# License for the specific language governing rights and limitations
# under the License.
# 
# The Original Code is the Bugzilla Bug Tracking System.
# 
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are Copyright (C) 1998
# Netscape Communications Corporation. All Rights Reserved.
# 
# Contributor(s): Harrison Page <harrison@netscape.com>
#                 Terry Weissman <terry@mozilla.org>

# harrison@netscape.com
#
# 1.0 10/16/98 

package Tinderbox;

require Exporter;

use strict 'vars';
use vars qw (@ISA @EXPORT $VERSION);
use LWP::Simple;
# use HTML::Parse;
use Carp;

@ISA = qw (Exporter);
@EXPORT = qw (status);

my $VERSION = "1.0";

# status wants a reference to a list of tinderbox trees
# and a url ending with tree=, default to mozilla.org's
# server if not provided. status returns two references
# to hashes. the first contains tree names as key, 
# tree status as value. second hash contains the tree 
# name as key and the last update time as a value.
#
# tree status can be horked or success.
#
# bleh. 

sub status
	{
	my $trees = shift;
	my $url = shift;

	# maybe this is too helpful

	if (ref ($trees) ne "ARRAY")
		{
		carp "status method wants a reference to a list, not a " . ref ($trees);
		return;
		}

	$url = $url || "http://cvs-mirror.mozilla.org/webtools/tinderbox/" .
		"showbuilds.cgi?express=1&tree=";

  my $start = 0;
  my %info; 
	my %last;

	# iterate through trees

  foreach my $t (@$trees)
    {
    my $output = get $url . $t;

		# this is a quick and dirty hack.
		# 
		# the proper way to do this would be to
		# hack tinderbox and make the information
		# easier to parse. but, no, i had to go 
		# play around with regular expressions.
		# lloyd tells me express=1 format won't 
		# change anytime soon so this is FINE FOR NOW

    $output =~ s/&nbsp;/ /g;

    my ($page_tree) = $output =~
      /<a href=showbuilds.cgi\?tree=$t>([^<>]+)<\/a>/i;

    $last{$t} = $page_tree;

    $output =~ s/\n//g;

    my @l = split /<td/, $output;

    foreach my $i (0 .. $#l)
      {
      $l[$i] =~ s#(</td>?|</table>|</?tr>|</a>)##g;

      next if ($l[$i] =~ /as of/);
      my $status = ($l[$i] =~ /background=/) ? "horked" : "success";
      my @burp = split />/, $l[$i];
			my $b = $#burp;

			# strip leading, trailing spaces
      $burp[$b] =~ s/\s+$//;
      $burp[$b] =~ s/^\s+//;

      $info{$t}{$burp[$b]} = $status;
      }
    }

  return (\%info, \%last);
  }

1;

