# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is Tinderbox 3.
#
# The Initial Developer of the Original Code is
# John Keiser (john@johnkeiser.com).
# Portions created by the Initial Developer are Copyright (C) 2004
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#
# ***** END LICENSE BLOCK *****

package Tinderbox3::Bonsai;

use strict;

use CGI qw/-oldstyle_urls/;
use LWP::UserAgent;
use Date::Format;

sub new {
	my $class = shift;
	$class = ref($class) || $class;
	my $this = {};
	bless $this, $class;

  my ($start_time, $end_time, $tree, $cvs_module, $branch, $directory) = @_;

}

sub clear_cache {
  my ($dbh, $bonsai_id) = @_;
  $dbh->do("DELETE FROM tbox_bonsai_cache WHERE bonsai_id = ?", undef, $bonsai_id);
  $dbh->do("UPDATE tbox_bonsai SET start_cache = null, end_cache = null WHERE bonsai_id = ?", undef, $bonsai_id);
}

sub _grab_cache {
  my ($dbh, $start_time, $end_time, $bonsai_id, $bonsai_url, $module, $branch, $directory, $cvsroot) = @_;
  my $p = new CGI(
    { treeid => 'default',
      module => $module,
      branch => $branch,
      branchtype => 'match',
      dir => $directory,
      file => '',
      filetype => 'match',
      who => '',
      whotype => 'match',
      sortby => 'Date',
      hours => 2,
      date => 'explicit',
      mindate => $start_time,
      maxdate => $end_time,
      cvsroot => $cvsroot
    });
  my $url = $bonsai_url . "/cvsquery.cgi?" . $p->query_string;

  my $ua = new LWP::UserAgent;
  $ua->agent("TinderboxServer/0.1");
  my $req = new HTTP::Request(GET => $url);
  my $response = $ua->request($req);
  if ($response->is_success) {
    my $content = $response->content;
    
    my ($checkin_date, $who, $files, $revisions, $size_plus, $size_minus,
        $description);
    my $insert_sth = $dbh->prepare("INSERT INTO tbox_bonsai_cache (bonsai_id, checkin_date, who, files, revisions, size_plus, size_minus, description) VALUES (?, ?, ?, ?, ?, ?, ?, ?)");
    while ($content =~ m{
<tr[^>]*>\s*

<td[^>]*>\s*(?:<\w+[^>]*>\s*)*
(\d+/\d+/\d+)\D+(\d+:\d+) # 1+2=date
\s*(?:</\w+[^>]*>)*

<td[^>]*>\s*(?:<\w+[^>]*>\s*)*
([^<]*) # 3=who
\s*(?:</\w+[^>]*>\s*)*

<td[^>]*>\s*(?:<\w+[^>]*>\s*)*
([^<]*) # 4=file
\s*(?:</\w+[^>]*>\s*)*

<td[^>]*>\s*(?:<\w+[^>]*>\s*)*
([^<]*) # 5=version
\s*(?:</\w+[^>]*>\s*)*

(?:<td[^>]*>\s*(?:<\w+[^>]*>\s*)*
([^<]*) # 6=branch
\s*(?:</\w+[^>]*>\s*)*)?

<td[^>]*>\s*(?:<\w+[^>]*>\s*)*
(\d+)/(\d+) # 7/8=minus/plus lines
\s*(?:</\w+[^>]*>\s*)*(?:\&nbsp)?\s*

(?:<td[^>]*>\s*
((?:.(?!</(font|td|tr)>))*) # 9=description
\s*(?:</\w+[^>]*>\s*)*)?
}mgxi) {
      if (defined($9)) {
        if (defined($description)) {
          $checkin_date =~ s!(\d+)/(\d+)/(\d+)!$3/$1/$2!g;
          $insert_sth->execute($bonsai_id, $checkin_date, $who, $files,
                               $revisions, $size_plus, $size_minus,
                               $description);
        }
        ($checkin_date, $who, $revisions, $size_plus, $size_minus,
         $description) = ("$1 $2", $3, $5, $7, $8, $9);
      } else {
        $revisions .= ",$5";
        $size_plus += $7;
        $size_minus += $8;
      }

      # Do regexp things down here instead of above because it will disturb
      # $1-$8
      my $was_description = defined($8);
      my $file = $4;
      $file =~ s/\s//g;
      if ($was_description) {
        $files = $file;
      } else {
        $files .= ",$file";
      }
#      if ($was_description) {
#        $description =~ s/<br>/\n/mig;
#        $description =~ s/<\/?a[^>]*>//mig;
#        $description =~ s/&lt;/</mig;
#        $description =~ s/&gt;/>/mig;
#        $description =~ s/&amp;/&/mig;
#      }
    }
    if (defined($description)) {
      $insert_sth->execute($bonsai_id, $checkin_date, $who, $files, $revisions,
                           $size_plus, $size_minus, $description);
    }
  } else {
    die "Invalid Bonsai URL '$bonsai_url' : $url";
  }
}

sub update_cache {
  my ($dbh, $start_time, $end_time, $bonsai_id, $bonsai_url, $module, $branch,
      $directory, $cvsroot, $old_start_time, $old_end_time) = @_;

  # Figure out what part of the cache needs updating and update it
  my ($new_start_time, $new_end_time) = ($old_start_time, $old_end_time);
  if (!defined($old_start_time) && !defined($old_end_time)) {
    # If both are undefined, this is our first grab into the cache
    _grab_cache($dbh, $start_time, $end_time, $bonsai_id, $bonsai_url,
                $module, $branch, $directory, $cvsroot);
    ($new_start_time, $new_end_time) = ($start_time, $end_time);

  } else {
    # Otherwise we are just extending our old start and end times
    if ($end_time > $old_end_time) {
      _grab_cache($dbh, $old_end_time+1, $end_time, $bonsai_id, $bonsai_url,
                  $module, $branch, $directory, $cvsroot);
      $new_end_time = $end_time;
    }
    if ($start_time < $old_start_time) {
      _grab_cache($dbh, $start_time, $old_start_time-1, $bonsai_id, $bonsai_url,
                  $module, $branch, $directory, $cvsroot);
      $new_start_time = $start_time;
    }
  }

  $dbh->do("UPDATE tbox_bonsai SET start_cache = " . Tinderbox3::DB::sql_abstime("?") . ", end_cache = " . Tinderbox3::DB::sql_abstime("?") . " WHERE bonsai_id = ?", undef, int($new_start_time), int($new_end_time), $bonsai_id);
}

1
