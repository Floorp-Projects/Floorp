#!/usr/bin/perl -wT -I.
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

use strict;
use Fcntl qw/:seek/;
use CGI;
use CGI::Carp qw(fatalsToBrowser);
use Tinderbox3::Log;
use Tinderbox3::Header;
use Tinderbox3::Login;
use Tinderbox3::DB;

my $p = new CGI;
my $machine_id = $p->param('machine_id') || "";
my $logfile = $p->param('logfile') || "";

# Detaint machine id / logfile
$machine_id =~ /^(\d+)$/s;
$machine_id = $1;
$logfile =~ /^(\d+\.log)$/s;
$logfile = $1;


if (!$machine_id || !$logfile) {
  die "Must specify machine_id and logfile!";
}

my $log_fh = get_log_fh($machine_id, $logfile, "<");
if (!defined($log_fh)) {
  die "No such log found!";
}

if ($p->param('format') && $p->param('format') eq 'raw') {
  print $p->header("text/plain");
  while (<$log_fh>) {
    print;
  }
  close $log_fh;
} else {
  if (!$p->param('page') && !$p->param('format')) {
    $p->param('format', 'summary');
  }
  my $dbh = get_dbh();
  my ($login, $cookie) = check_session($p, $dbh);
  my $bytes_per_page = 100 * 1024;
  my @log_stat = stat($log_fh);
  my $log_size = $log_stat[7];
  my $num_pages = int($log_size / $bytes_per_page) + 1;
  my $page;
  if (!$p->param('format') || $p->param('format') ne 'summary') {
    $page = $p->param('page');
  }
  if (!$page || $p->param('page') !~ /^\d+$/ || $page > $num_pages) {
    $page = $num_pages;
  }
  my $title;
  if ($p->param('format') && $p->param('format') eq 'summary') {
    $title = "Summary";
  } else {
    $title = "Page $page";
  }

  #
  # Print the header
  #
  my $machine_info = $dbh->selectrow_arrayref("SELECT tree_name, machine_name FROM tbox_machine WHERE machine_id = ?", undef, $machine_id);
  my ($tree, $machine_name) = @{$machine_info};
  header($p, $login, $cookie, "Log $logfile - $title", $tree, $machine_id, $machine_name);

  # Print the header links
  my $pages_per_line = 15;
  {
    my $num_kbytes = int($log_size / 1024) + 1;
    my $q = new CGI($p);
    $q->delete('page');
    $q->param('format', 'raw');
    my $q2 = new CGI($p);
    $q2->delete('page');
    $q2->param('format', 'summary');
    print "<table align=center style='border: 1px solid black'><tr><th style='border-bottom: 1px solid black' colspan=$pages_per_line>$title<br><a href='showlog.pl?" . $q2->query_string() . "'>Summary</a> - <a href='showlog.pl?" . $q->query_string() . "'>Raw Log</a> (${num_kbytes}K)</th></tr>\n";
  }

  # Print the page links
  foreach my $i (1..$num_pages) {
    if (($i % $pages_per_line) == 1) {
      print "<tr>\n";
    }
    print "<td>";
    if ($title eq "Summary" || $i ne $page) {
      $p->delete('format');
      $p->param('page', $i);
      print "<a href='showlog.pl?" . $p->query_string() . "'>";
    }
    print $i;
    if ($title eq "Summary" || $i ne $page) {
      print "</a>";
    }
    print "</td>\n";
    if (($i % $pages_per_line) == 0) {
      print "</tr>\n";
    }
  }
  print "</table></p>\n";

  #
  # Print the log summary
  #
  if ($title eq "Summary") {
    print "<h3>Event Summary</h3>\n";
    print "<pre>\n";
    my $tab = 0;
    my $pos;
    my $max_page_size = length($num_pages);
    my $last_page = -1;
    while (($pos = tell($log_fh)) != -1 && ($_ = readline($log_fh))) {
      if (/^<---/) {
        $tab--;
      }
      if (/^--->/ || /^<---/) {
        my $text_page = int($pos / $bytes_per_page) + 1;
        if ($text_page != $last_page) {
          $last_page = $text_page;
          my $q = new CGI($p);
          $q->delete('format');
          $q->param('page', $text_page);
          my $query_string = $q->query_string();

          print "<a href='showlog.pl?$query_string'>p.", sprintf("%-${max_page_size}s", $text_page), "</a> ";
        } else {
          print " " x ($max_page_size+3);
        }
        print " " x ($tab*5);
        print;
      }
      if (/^--->/ && !/<---$/) {
        $tab++;
      }
    }
    print "</pre>\n";
    print "<h3>Last Page - Page $page</h3>\n";
  }

  #
  # Print the log
  #

  # Go to the right place in the log
  seek($log_fh, ($page - 1) * $bytes_per_page, SEEK_SET);
  # Go to the end of the line for page > 0
  if ($page > 1) {
    my $buf;
    while (1) {
      last if !read($log_fh,$buf,1);
      last if $buf eq "\n";
    }
  }

  # Print the page
  print "<pre class=log>";
  my $pos = tell($log_fh);
  my $buf;
  while ($pos <= $page * $bytes_per_page && ($buf = readline($log_fh))) {
    print $buf;
    $pos = tell($log_fh);
  }
  print "</pre>\n";
  footer($p);
  $dbh->disconnect();
}
