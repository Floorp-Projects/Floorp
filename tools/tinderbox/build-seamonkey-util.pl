#!/usr/bin/perl

require 5.000;

use Sys::Hostname;
use POSIX "sys_wait_h";
use Cwd;

sub InitVars {
  for (@ARGV) {
    # Save DATA section for printing the example.
    return if /^--example-config$/;
  }
  eval while <DATA>;  # See __END__ section below
}


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

sub LoadConfig {
  if (-r 'tinder-config.pl') {
    require 'tinder-config.pl';
  } else {
    warn "Error: Need tinderbox config file, tinder-config.pl\n";
    warn "       To get started, run the following,\n";
    warn "          $0 --example-config > tinder-config.pl\n";
    exit;
  }

}

sub SetupEnv {
  umask 0;
  $ENV{LD_LIBRARY_PATH} = "$BaseDir/$DirName/mozilla/${ObjDir}dist/bin:/usr/lib/png:"
     ."/usr/local/lib:$BaseDir/$DirName/mozilla/dist/bin";
  $ENV{DISPLAY} = $DisplayServer;
  $ENV{MOZCONFIG} = "$BaseDir/$MozConfigFileName" if $MozConfigFileName ne '';
}

sub SetupPath {
  my $comptmp;
  $comptmp = '';
  #print "Path before: $ENV{PATH}\n";
  
  if ($OS eq 'AIX') {
    $ENV{PATH}        = "/builds/local/bin:$ENV{PATH}:/usr/lpp/xlC/bin";
    $ConfigureArgs   .= '--x-includes=/usr/include/X11 '
                      . '--x-libraries=/usr/lib --disable-shared';
    $ConfigureEnvArgs = 'CC=xlC_r CXX=xlC_r';
    $Compiler         = 'xlC_r';
    $NSPRArgs        .= 'NS_USE_NATIVE=1 USE_PTHREADS=1';
  }
  
  if ($OS eq 'BSD_OS') {
    $ENV{PATH}        = "/usr/contrib/bin:/bin:/usr/bin:$ENV{PATH}";
    $ConfigureArgs   .= '--disable-shared';
    $ConfigureEnvArgs = 'CC=shlicc2 CXX=shlicc2';
    $Compiler         = 'shlicc2';
    $mail             = '/usr/ucb/mail';
    # Because ld dies if it encounters -include
    $MakeOverrides    = 'CPP_PROG_LINK=0 CCF=shlicc2';
    $NSPRArgs        .= 'NS_USE_GCC=1 NS_USE_NATIVE=';
  }

  if ($OS eq 'FreeBSD') {
    $ENV{PATH}        = "/bin:/usr/bin:$ENV{PATH}";
    if ($ENV{HOST} eq 'angelus.mcom.com') {
      $ConfigureEnvArgs = 'CC=egcc CXX=eg++';
      $Compiler       = 'egcc';
    }
    $mail             = '/usr/bin/mail';
  }

  if ($OS eq 'HP-UX') {
    $ENV{PATH}        = "/opt/ansic/bin:/opt/aCC/bin:/builds/local/bin:"
                      . "$ENV{PATH}";
    $ENV{LPATH}       = "/usr/lib:$ENV{LD_LIBRARY_PATH}:/builds/local/lib";
    $ENV{SHLIB_PATH}  = $ENV{LPATH};
    $ConfigureArgs   .= '--x-includes=/usr/include/X11 '
                      . '--x-libraries=/usr/lib --disable-gtktest ';
    $ConfigureEnvArgs = 'CC="cc -Ae" CXX="aCC -ext"';
    $Compiler         = 'cc/aCC';
    # Use USE_PTHREADS=1 instead of CLASSIC_NSPR if you've got DCE installed.
    $NSPRArgs        .= 'NS_USE_NATIVE=1 CLASSIC_NSPR=1';
  }
  
  if ($OS eq 'IRIX') {
    $ENV{PATH}        = "/opt/bin:$ENV{PATH}";
    $ENV{LD_LIBRARY_PATH}   .= ':/opt/lib';
    $ENV{LD_LIBRARYN32_PATH} = $ENV{LD_LIBRARY_PATH};
    $ConfigureEnvArgs = 'CC=cc CXX=CC CFLAGS="-n32 -O" CXXFLAGS="-n32 -O"';
    $Compiler         = 'cc/CC';
    $NSPRArgs        .= 'NS_USE_NATIVE=1 USE_PTHREADS=1';
  }
  
  if ($OS eq 'NetBSD') {
    $ENV{PATH}        = "/bin:/usr/bin:$ENV{PATH}";
    $ENV{LD_LIBRARY_PATH} .= ':/usr/X11R6/lib';
    $ConfigureEnvArgs = 'CC=egcc CXX=eg++';
    $Compiler         = 'egcc';
    $mail             = '/usr/bin/mail';
  }
  
  if ($OS eq 'OSF1') {
    $ENV{PATH}        = "/usr/gnu/bin:$ENV{PATH}";
    $ENV{LD_LIBRARY_PATH} .= ':/usr/gnu/lib';
    $ConfigureEnvArgs = 'CC="cc -readonly_strings" CXX="cxx"';
    $Compiler         = 'cc/cxx';
    $MakeOverrides    = 'SHELL=/usr/bin/ksh';
    $NSPRArgs        .= 'NS_USE_NATIVE=1 USE_PTHREADS=1';
    $ShellOverride    = '/usr/bin/ksh';
  }
  
  if ($OS eq 'QNX') {
    $ENV{PATH}        = "/usr/local/bin:$ENV{PATH}";
    $ENV{LD_LIBRARY_PATH} .= ':/usr/X11/lib';
    $ConfigureArgs   .= '--x-includes=/usr/X11/include '
                      . '--x-libraries=/usr/X11/lib --disable-shared ';
    $ConfigureEnvArgs = 'CC="cc -DQNX" CXX="cc -DQNX"';
    $Compiler         = 'cc';
    $mail             = '/usr/bin/sendmail';
  }

  if ($OS eq 'SunOS') {
    if ($OSVerMajor eq '4') {
      $ENV{PATH}      = "/usr/gnu/bin:/usr/local/sun4/bin:/usr/bin:$ENV{PATH}";
      $ENV{LD_LIBRARY_PATH} = "/home/motif/usr/lib:$ENV{LD_LIBRARY_PATH}";
      $ConfigureArgs .= '--x-includes=/home/motif/usr/include/X11 '
                      . '--x-libraries=/home/motif/usr/lib';
      $ConfigureEnvArgs = 'CC="egcc -DSUNOS4" CXX="eg++ -DSUNOS4"';
      $Compiler       = 'egcc';
    } else {
      $ENV{PATH}      = '/usr/ccs/bin:' . $ENV{PATH};
    }
    if ($CPU eq 'i86pc') {
      $ENV{PATH}      = '/opt/gnu/bin:' . $ENV{PATH};
      $ENV{LD_LIBRARY_PATH} .= ':/opt/gnu/lib';
      $ConfigureEnvArgs = 'CC=egcc CXX=eg++';
      $Compiler       = 'egcc';

      # Possible NSPR bug... If USE_PTHREADS is defined, then
      #   _PR_HAVE_ATOMIC_CAS gets defined (erroneously?) and
      #   libnspr21 does not work.
      $NSPRArgs      .= 'CLASSIC_NSPR=1 NS_USE_GCC=1 NS_USE_NATIVE=';
    } else {
      # This is utterly lame....
      if ($ENV{HOST} eq 'fugu') {
        $ENV{PATH}    = "/tools/ns/workshop/bin:/usrlocal/bin:$ENV{PATH}";
        $ENV{LD_LIBRARY_PATH} = '/tools/ns/workshop/lib:/usrlocal/lib:'
                      . $ENV{LD_LIBRARY_PATH};
        $ConfigureEnvArgs = 'CC=cc CXX=CC';
        $comptmp      = `cc -V 2>&1 | head -1`;
        chomp($comptmp);
        $Compiler     = "cc/CC \($comptmp\)";
        $NSPRArgs    .= 'NS_USE_NATIVE=1';
      } else {
        $NSPRArgs    .= 'NS_USE_GCC=1 NS_USE_NATIVE=';
      }
      if ($OSVerMajor eq '5') {
        $NSPRArgs    .= ' USE_PTHREADS=1';
      }
    }
  }
  #print "Path after:  $ENV{PATH}\n";
}


# Need to end with a true value, (since we're using "require").
1;
