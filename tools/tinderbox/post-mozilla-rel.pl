#!/usr/bin/perl
#

#
# This script gets called after a full mozilla build & test.
#
# packages and delivers mozilla bits
# Assumptions:
#  mozilla tree
#  produced builds get put in Topsrcdir/installer/sea
#
#
# tinder-config variables that you should set:
#  package_creation_path: directory to run 'make installer' to create an
#                         installer, and 'make' to create a zip/tar build
#  ftp_path: directory to upload nightly builds to
#  url_path: absolute URL to nightly build directory
#  tbox_ftp_path: directory to upload tinderbox builds to
#  tbox_url_path: absolute URL to tinderbox builds directory
#  notify_list: list of email addresses to notify of nightly build completion
#  build_hour: upload the first build completed after this hour as a nightly
#  ssh_server: server to upload build to via ssh
#  ssh_user: user to log in to ssh with
#  ssh_version: if set, force ssh protocol version N
#  milestone: suffix to append to date and latest- directory names
#  stub_installer: (0/1) whether to upload a stub installer
#  sea_installer: (0/1) whether to upload a sea (blob) installer
#  archive: (0/1) whether to upload an archive (tar or zip) build
#
#  windows-specific variables:
#   as_perl_path: cygwin-ized path to Activestate Perl's bin directory

use strict;
use Sys::Hostname;

package PostMozilla;

use Cwd;

sub is_windows { return $Settings::OS =~ /^WIN/; }
sub is_linux { return $Settings::OS eq 'Linux'; }
# XXX Not tested on mac yet.  Probably needs changes.
sub is_mac { return $Settings::OS eq 'Darwin'; }
sub do_installer { return is_windows() || is_linux(); }

sub shorthost {
  my $host = ::hostname();
  $host = $1 if $host =~ /(.*?)\./;
  return $host;
}

sub stagesymbols {
  my $builddir = shift;
  TinderUtils::run_shell_command "make -C $builddir deliver";
}

sub makefullsoft {
  my $builddir = shift;
  if (is_windows()) {
    # need to convert the path in case we're using activestate perl
    $builddir = `cygpath -u $builddir`;
  }
  chomp($builddir);
  # should go in config
  my $moforoot = "cltbld\@cvs.mozilla.org:/mofo"; 
  my $fullsofttag = " ";
  $fullsofttag = " -r $Settings::BuildTag"
        unless not defined($Settings::BuildTag) or $Settings::BuildTag eq '';
  TinderUtils::run_shell_command "cd $builddir; cvs -d$moforoot co $fullsofttag -d fullsoft talkback/fullsoft";
  TinderUtils::run_shell_command "cd $builddir/fullsoft; $builddir/build/autoconf/make-makefile";
  TinderUtils::run_shell_command "make -C $builddir/fullsoft";
  TinderUtils::run_shell_command "make -C $builddir/fullsoft fullcircle-push";
  if (is_mac()) {
    TinderUtils::run_shell_command "make -C $builddir/$Settings::mac_bundle_path";
  }
}

sub processtalkback {
  # first argument is whether to make a new talkback build on server
  #                and upload debug symbols
  # second argument is where we're building our tree
  my $makefullsoft      = shift;
  my $builddir      = shift;   
  # put symbols in builddir/dist/buildid
  stagesymbols($builddir); 
  if ($makefullsoft) {
    $ENV{FC_UPLOADSYMS} = 1;
    makefullsoft($builddir);
  }
  
}

sub packit {
  my ($packaging_dir, $package_location, $url, $stagedir) = @_;
  my $status;

  if (do_installer()) {
    if (is_windows()) {
      $ENV{INSTALLER_URL} = "$url/windows-xpi";
    } elsif (is_linux()) {
      $ENV{INSTALLER_URL} = "$url/linux-xpi/";
    } else {
      die "Can't make installer for this platform.\n";
    }
    TinderUtils::print_log "INSTALLER_URL is " . $ENV{INSTALLER_URL} . "\n";

    mkdir($package_location, 0775);

    # the Windows installer scripts currently require Activestate Perl.
    # Put it ahead of cygwin perl in the path.
    my $save_path;
    if (is_windows()) {
      $save_path = $ENV{PATH};
      $ENV{PATH} = $Settings::as_perl_path.":".$ENV{PATH};
    }

    # the one operation we care about saving status of
    if ($Settings::sea_installer || $Settings::stub_installer) {
      $status = TinderUtils::run_shell_command "make -C $packaging_dir installer";
    } else {
      $status = 0;
    }

    if (is_windows()) {
      $ENV{PATH} = $save_path;
      #my $dos_stagedir = `cygpath -w $stagedir`;
      #chomp ($dos_stagedir);
    }
    mkdir($stagedir, 0775);
    if (is_windows()) {
      if ($Settings::stub_installer) {
        TinderUtils::run_shell_command "cp -r $package_location/xpi $stagedir/windows-xpi";
        TinderUtils::run_shell_command "cp $package_location/stub/*.exe $stagedir/";
      }
      if ($Settings::sea_installer) {
        TinderUtils::run_shell_command "cp $package_location/sea/*.exe $stagedir/";
      }
    } elsif (is_linux()) {
      if ($Settings::stub_installer) {
        TinderUtils::run_shell_command "cp -r $package_location/raw/xpi $stagedir/linux-xpi";
        TinderUtils::run_shell_command "cp $package_location/stub/*.tar.gz $stagedir/";
      }
      if ($Settings::sea_installer) {
        TinderUtils::run_shell_command "cp $package_location/sea/*.tar.gz $stagedir/";
      }
    }
  }

  if ($Settings::archive) {
    TinderUtils::run_shell_command "make -C $packaging_dir";
    if (is_windows()) {
      TinderUtils::run_shell_command "cp $package_location/../*.zip $stagedir/";
    } elsif (is_mac()) {
      system("mkdir -p $package_location");
      system("mkdir -p $stagedir");
      TinderUtils::run_shell_command "cp $package_location/../*.dmg.gz $stagedir/";
    } else {
      TinderUtils::run_shell_command "cp $package_location/../dist/*.tar.gz $stagedir/";
    }
  }

  # need to reverse status, since it's a "unix" truth value, where 0 means 
  # success
  return ($status)?0:1;
}

sub pad_digit {
  my ($digit) = @_;
  if ($digit < 10) { return "0" . $digit };
  return $digit;
}

sub pushit {
  my ($ssh_server,$upload_path,$upload_directory,$cachebuild) = @_;

  unless ( -d $upload_directory) {
    TinderUtils::print_log "No $upload_directory to upload\n";
    return 0;
  }

  if (is_windows()) {
    # need to convert the path in case we're using activestate perl
    $upload_directory = `cygpath -u $upload_directory`;
  }
  chomp($upload_directory);
  my $short_ud = `basename $upload_directory`;
  chomp ($short_ud);

  my $ssh_opts = "";
  my $scp_opts = "";
  if (defined($Settings::ssh_version) && $Settings::ssh_version ne '') {
    $ssh_opts = "-".$Settings::ssh_version;
    $scp_opts = "-oProtocol=".$Settings::ssh_version;
  }

  TinderUtils::run_shell_command "ssh $ssh_opts -l $Settings::ssh_user $ssh_server mkdir -p $upload_path";
  TinderUtils::run_shell_command "scp $scp_opts -r $upload_directory $Settings::ssh_user\@$ssh_server:$upload_path";
  TinderUtils::run_shell_command "ssh $ssh_opts -l $Settings::ssh_user $ssh_server chmod -R 775 $upload_path/$short_ud";

  if ($cachebuild) {
    TinderUtils::run_shell_command "ssh $ssh_opts -l $Settings::ssh_user $ssh_server cp -pf $upload_path/$short_ud/* $upload_path/latest-$Settings::milestone/";
  }
  return 1;
}


# Cache builds in a dated directory if:
#  * The hour of the day is $Settings::build_hour or higher 
#    (set in tinder-config.pl)
#    -and-
#  * the "last-built" file indicates a day before today's date
#
sub cacheit {
  my ($c_hour, $c_yday, $target_hour, $last_build_day) = @_;
  TinderUtils::print_log "c_hour = $c_hour\n";
  TinderUtils::print_log "c_yday = $c_yday\n";
  TinderUtils::print_log "last_build_day = $last_build_day\n";
  if (($c_hour > $target_hour) && 
      (($last_build_day < $c_yday) || ($c_yday == 0))) {
        return 1;
      } else {
	return 0;
      }
}

sub reportRelease {
  my ($url, $datestamp)       = @_;

  if ($Settings::notify_list ne "") {
    my $donemessage =   "\n" .
                        "$Settings::OS $Settings::ProductName Build available at: \n" .
                        "$url \n";
    open(TMPMAIL, ">tmpmail.txt");
    print TMPMAIL "$donemessage \n";
    close(TMPMAIL);

    TinderUtils::print_log ("$donemessage \n");
    my $subject = "[build] $datestamp $Settings::ProductName $Settings::OS Build Complete";
    if ($Settings::blat ne "" && $Settings::use_blat) {
      system("$Settings::blat tmpmail.txt -to $Settings::notify_list -s \"$subject\"");
    } else {
      system "$Settings::mail -s \"$subject\" $Settings::notify_list < tmpmail.txt";
    }
    unlink "tmpmail.txt";
  }
  return (("success", "$url"));
}

sub returnStatus{
  # build-seamonkey-util.pl expects an array, if no package is uploaded,
  # a single-element array with 'busted', 'testfailed', or 'success' works.
  my ($logtext, @status) = @_;
  TinderUtils::print_log "$logtext\n";
  return @status;
}

sub main {
  # Get build directory from caller.
  my ($mozilla_build_dir) = @_;
  TinderUtils::print_log "Post-Build packaging/uploading commencing.\n";

  chdir $mozilla_build_dir;

  if (is_windows()) {
    #$mozilla_build_dir = `cygpath $mozilla_build_dir`; # cygnusify the path
    #chop $mozilla_build_dir; # remove whitespace
  
    #unless ( -e $mozilla_build_dir) {
      #TinderUtils::print_log "No cygnified $mozilla_build_dir to make packages in.\n";
      #return (("testfailed")) ;
    #}
  }

  my $objdir = "$mozilla_build_dir/${Settings::Topsrcdir}";
  if ($Settings::ObjDir ne '') {
    $objdir .= "/${Settings::ObjDir}";
  }
  unless ( -e $objdir) {
    TinderUtils::print_log "No $objdir to make packages in.\n";
    return (("testfailed")) ;
  }

  # set up variables with default values
  my $last_build_day = 0;
  # need to modify the settings from tinder-config.pl
  my $package_creation_path = $objdir . $Settings::package_creation_path;
  my $package_location;
  if (is_windows() || is_mac()) {
    $package_location = $objdir . "/dist/install";
  } else {
    $package_location = $objdir . "/installer";
  }
  my $ftp_path = $Settings::ftp_path;
  my $url_path = $Settings::url_path;

  my ($c_hour,$c_day,$c_month,$c_year,$c_yday) = (localtime(time))[2,3,4,5,7];
  $c_year       = $c_year + 1900; # ftso perl
  $c_month      = $c_month + 1; # ftso perl
  $c_hour       = pad_digit($c_hour);
  $c_day        = pad_digit($c_day);
  $c_month      = pad_digit($c_month);
  my $datestamp = "$c_year-$c_month-$c_day-$c_hour-$Settings::milestone";

  if ( -e "last-built"){
    ($last_build_day) = (localtime((stat "last-built")[9]))[7];
  } else {
    $last_build_day = -1;
  }

  if (is_windows()) {
    # hack for cygwin installs with "unix" filetypes
    TinderUtils::run_shell_command "unix2dos $mozilla_build_dir/mozilla/LICENSE";
    TinderUtils::run_shell_command "unix2dos $mozilla_build_dir/mozilla/README.txt";
  }

  my $upload_directory;
  my $cachebuild;

  if (cacheit($c_hour,$c_yday,$Settings::build_hour,$last_build_day)) {
    TinderUtils::print_log "attempting to cache today's build\n";
    $upload_directory = "$datestamp";
    $url_path         = $url_path . "/" . $upload_directory;
    $cachebuild = 1;
  } else {
    $ftp_path   = $Settings::tbox_ftp_path;
    $upload_directory = shorthost();
    $url_path   = $Settings::tbox_url_path . "/" . $upload_directory;
    $cachebuild = 0;
  }

  processtalkback($cachebuild && $Settings::shiptalkback, $objdir);

  $upload_directory = $package_location . "/" . $upload_directory;

  unless (packit($package_creation_path,$package_location,$url_path,$upload_directory)) {
    return returnStatus("Packaging failed", ("testfailed"));
  }

  unless (pushit($Settings::ssh_server,
		 $ftp_path,$upload_directory, $cachebuild)) {
    return returnStatus("Pushing package $upload_directory failed", ("testfailed"));
  }

  if ($cachebuild) { 
    unlink "last-built";
    open BLAH, ">last-built"; 
    close BLAH;
    return reportRelease ("$url_path\/", "$datestamp");
  } else {
    return (("success"));
  }
} # end main

# Need to end with a true value, (since we're using "require").
1;
