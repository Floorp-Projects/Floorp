#!/usr/bin/perl

require 5.000;

use Sys::Hostname;
use POSIX "sys_wait_h";
use Cwd;

sub GetSystemInfo {
  $OS        = `uname -s`;
  $OSVer     = `uname -r`;
  $CPU       = `uname -m`;
  $BuildName = ''; 
  
  my $host   = hostname;
  $host =~ s/\..*$//;
  
  chomp($OS, $OSVer, $CPU, $host);

  if ($OS eq 'AIX') {
    my $osAltVer = `uname -v`;
    chomp($osAltVer);
    $OSVer = "$osAltVer.$OSVer";
  }
  
  $OS = 'BSD_OS' if $OS eq 'BSD/OS';
  $OS = 'IRIX'   if $OS eq 'IRIX64';

  if ($OS eq 'QNX') {
    $OSVer = `uname -v`;
    chomp($OSVer);
    $OSVer =~ s/^([0-9])([0-9]*)$/$1.$2/;
  }
  if ($OS eq 'SCO_SV') {
    $OS = 'SCOOS';
    $OSVer = '5.0';
  }
  
  unless ("$host" eq '') {
    $BuildName = "$host $OS ". ($BuildDepend ? 'Depend' : 'Clobber');
  }
  $DirName = "${OS}_${OSVer}_". ($BuildDepend ? 'depend' : 'clobber');
  $logfile = "${DirName}.log";

  #
  # Make the build names reflect architecture/OS
  #
  
  if ($OS eq 'AIX') {
    # Assumes 4.2.1 for now.
  }
  if ($OS eq 'BSD_OS') {
    $BuildName = "$host BSD/OS $OSVer ". ($BuildDepend ? 'Depend' : 'Clobber');
  }
  if ($OS eq 'FreeBSD') {
    $BuildName = "$host $OS/$CPU $OSVer ". ($BuildDepend ? 'Depend':'Clobber');
  }
  if ($OS eq 'HP-UX') {
  }
  if ($OS eq 'IRIX') {
  }
  if ($OS eq 'Linux') {
    if ($CPU eq 'alpha' or $CPU eq 'sparc') {
      $BuildName = "$host  $OS/$CPU $OSVer "
                 . ($BuildDepend ? 'Depend' : 'Clobber');
    } elsif ($CPU eq 'armv4l' or $CPU eq 'sa110') {
      $BuildName = "$host $OS/arm $OSVer ". ($BuildDepend?'Depend':'Clobber');
    } elsif ($CPU eq 'ppc') {
      $BuildName = "$host $OS/$CPU $OSVer ". ($BuildDepend?'Depend':'Clobber');
    } else {
      $BuildName = "$host $OS ". ($BuildDepend?'Depend':'Clobber');

      # What's the right way to test for this?
      $ObjDir .= 'libc1' if $host eq 'truth';
    }
  }
  if ($OS eq 'NetBSD') {
    $BuildName = "$host $OS/$CPU $OSVer ". ($BuildDepend?'Depend':'Clobber');
  }
  if ($OS eq 'OSF1') {
    # Assumes 4.0D for now.
  }
  if ($OS eq 'QNX') {
  }
  if ($OS eq 'SunOS') {
    if ($CPU eq 'i86pc') {
      $BuildName = "$host $OS/i386 $OSVer ". ($BuildDepend?'Depend':'Clobber');
    } else {
      $OSVerMajor = substr($OSVer, 0, 1);
      if ($OSVerMajor ne '4') {
        $BuildName = "$host $OS/sparc $OSVer "
                   . ($BuildDepend?'Depend':'Clobber');
      }
    }
  }
}

# Need to end with a true value, (since we're using "require").
1;
