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

package Tinderbox3::Log;

use strict;
use Date::Format;

require Exporter;
our @ISA = qw(Exporter);
our @EXPORT = qw(compress_log get_log_fh create_logfile_name delete_logs);

our $logdir;

sub BEGIN {
  if (-d "xml") {
    $logdir = "xml/logs";
  } else {
    $logdir = "logs";
  }
}


# This is the path where gzip will be found
$ENV{PATH} = "/bin";

#
# Compress a logfile
#
sub compress_log {
  my ($machine_id, $logfile) = @_;

  if (defined($logfile) && -f "$logdir/$machine_id/$logfile") {
    # XXX Need to lock here to avoid dataloss on occasion
    system("gzip", "$logdir/$machine_id/$logfile");
  }
}

sub ensure_uncompressed {
  my ($machine_id, $logfile) = @_;

  if (!-f "$logdir/$machine_id/$logfile") {
    if (-f "$logdir/$machine_id/$logfile.gz") {
      # XXX Would be nice if this did not occur while compress_log or some
      # append function happens.  A lock would help that.
      system("gzip", "-d", "$logdir/$machine_id/$logfile.gz");
    } else {
      return 0;
    }
  }
  return 1;
}

sub create_logfile_name {
  my ($machine_id) = @_;
  # This string is detainted in showlog.pl; if you change the format
  # be sure to change the detaint expression as well.
  return time2str("%Y%m%d%H%M%S.log", time);
}

sub get_log_fh {
  my ($machine_id, $logfile, $mode) = @_;

  $mode ||= "<";
  if (!ensure_uncompressed($machine_id, $logfile)) {
    # If the file isn't there and we try to read, don't do anything
    if ($mode eq "<") {
      return undef;
    }

    # Make the directories so that > and >> will work
    if (! -d $logdir) {
      mkdir($logdir) or die "Could not mkdir $logdir: $!";
    }
    if (! -d "$logdir/$machine_id") {
      mkdir("$logdir/$machine_id") or die "Could not mkdir $logdir/$machine_id $!";
    }
  }

  my $fh;
  open $fh, $mode, "$logdir/$machine_id/$logfile" or die "Could not open: $!";
  return $fh;
}

sub delete_logfile {
  my ($machine_id, $logfile) = @_;

  if (-f "$logdir/$machine_id/$logfile.gz") {
    unlink("$logdir/$machine_id/$logfile.gz");
  }
  if (-f "$logdir/$machine_id/$logfile") {
    unlink("$logdir/$machine_id/$logfile");
  }
}

sub delete_logs {
  my ($machine_id) = @_;
  system("rm -rf $logdir/$machine_id");
}


1
