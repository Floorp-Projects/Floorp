# -*- Mode: perl; indent-tabs-mode: nil -*-
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
# The Original Code is the Bugzilla Bug Tracking System.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): Terry Weissman <terry@mozilla.org>
#
#
#
# Most of this module is rudely swiped from infobot.  Here are the various
# licensing words I found with it:
#  --
#
#  The license for this stuff is yet to be written.
#  Please don't do anything good with it without at
#  least mentioning this work, perhaps the author (Kevin
#  Lenzo, lenzo@cs.cmu.edu), and the Carnegie Mellon
#  University, which supports my study.
#
#  Also, there is work being done on various bits of this
#  now by various people; if you have any corrections
#  or contributions, please send them to me.  Flat
#  ascii files of databases, made with dump_db, are
#  wonderful things to share, and a repository will be
#  set up.
#
#  ---
#
# This program is copyright Jonathan Feinberg 1999.
#
# This program is distributed under the same terms as infobot.
#
# Jonathan Feinberg   
# jdf@pobox.com
# http://pobox.com/~jdf/
#
# Version 1.0
# First public release.
#
# ---------------------- (End of licensing words) ------------------------


package babel;
use strict;
use diagnostics;

my $no_babel;

BEGIN {
    eval "use URI::Escape";    # utility functions for encoding the 
    if ($@) { $no_babel++};    # babelfish request
    eval "use LWP::UserAgent";
    if ($@) { $no_babel++};
}

BEGIN {
  # Translate some feasible abbreviations into the ones babelfish
  # expects.
    use vars qw!%lang_code $lang_regex!;
    %lang_code = (
		fr => 'fr',
		sp => 'es',
		po => 'pt',
		pt => 'pt',
		it => 'it',
		ge => 'de',
		de => 'de',
		gr => 'de',
		en => 'en'
	       );
  
  # Here's how we recognize the language you're asking for.  It looks
  # like RTSL saves you a few keystrokes in #perl, huh?
  $lang_regex = join '|', keys %lang_code;
}


sub babelfish {
    return '' if $no_babel;
  my ($direction, $lang, $phrase) = @_;
  
  $lang = $lang_code{$lang};

  my $ua = new LWP::UserAgent;
  $ua->timeout(4);

  my $req =  
    HTTP::Request->new('POST',
		       'http://babelfish.altavista.digital.com/cgi-bin/translate');
  $req->content_type('application/x-www-form-urlencoded');
  
  my $tolang = "en_$lang";
  my $toenglish = "${lang}_en";

  if ($direction eq 'to') {
    return translate($phrase, $tolang, $req, $ua);
  }
  elsif ($direction eq 'from') {
    return translate($phrase, $toenglish, $req, $ua);
  }

  my $last_english = $phrase;
  my $last_lang;
  my %results = ();
  my $i = 0;
  while ($i++ < 7) {
    last if $results{$phrase}++;
    $last_lang = $phrase = translate($phrase, $tolang, $req, $ua);
    last if $results{$phrase}++;
    $last_english = $phrase = translate($phrase, $toenglish, $req, $ua);
  }
  return $last_english;
}


sub translate {
    return '' if $no_babel;
  my ($phrase, $languagepair, $req, $ua) = @_;
  
  my $urltext = uri_escape($phrase);
  $req->content("urltext=$urltext&lp=$languagepair&doit=done");
  
  my $res = $ua->request($req);

  if ($res->is_success) {
      my $html = $res->content;
      # This method subject to change with the whims of Altavista's design
      # staff.
      my ($translated) = 
	  ($html =~ m{<br>
			  \s+
			      <font\ face="arial,\ helvetica">
				  \s*
				      (?:\*\*\s+time\ out\s+\*\*)?
					  \s*
					      ([^<]*)
					      }sx);
      $translated =~ s/\n/ /g;
      $translated =~ s/\s*$//;
      return $translated;
  } else {
      return ":("; # failure 
  }
}

"Hello.  I'm a true value.";
