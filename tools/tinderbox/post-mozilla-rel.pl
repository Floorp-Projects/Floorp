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
#  clean_objdir: (0/1) whether to wipe out the objdir (same as the srcdir if no
#                objdir set) when starting a release build cycle
#  clean_srcdir: (0/1) whether to wipe out the srcdir for a release cycle
#
#  windows-specific variables:
#   as_perl_path: cygwin-ized path to Activestate Perl's bin directory

use strict;
use Sys::Hostname;
use POSIX qw(mktime);

package PostMozilla;

use Cwd;

# This is set in PreBuild(), and is checked after each build.
my $cachebuild = 0;

sub is_windows { return $Settings::OS =~ /^WIN/; }
sub is_linux { return $Settings::OS eq 'Linux'; }
sub is_os2 { return $Settings::OS eq 'OS2'; }
# XXX Not tested on mac yet.  Probably needs changes.
sub is_mac { return $Settings::OS eq 'Darwin'; }
sub do_installer { return is_windows() || is_linux() || is_os2(); }

sub shorthost {
  my $host = ::hostname();
  $host = $1 if $host =~ /(.*?)\./;
  return $host;
}

sub print_locale_log {
    my ($text) = @_;
    print LOCLOG $text;
    print $text;
}

sub run_locale_shell_command {
    my ($shell_command) = @_;
    local $_;

    my $status = 0;
    chomp($shell_command);
    print_locale_log "$shell_command\n";
    open CMD, "$shell_command $Settings::TieStderr |" or die "open: $!";
    print_locale_log $_ while <CMD>;
    close CMD or $status = 1;
    return $status;
}

sub mail_locale_started_message {
    my ($start_time, $locale) = @_;
    my $msg_log = "build_start_msg.tmp";
    open LOCLOG, ">$msg_log";

    my $platform = $Settings::OS =~ /^WIN/ ? 'windows' : 'unix';

    print_locale_log "\n";
    print_locale_log "tinderbox: tree: $Settings::BuildTree-$locale\n";
    print_locale_log "tinderbox: builddate: $start_time\n";
    print_locale_log "tinderbox: status: building\n";
    print_locale_log "tinderbox: build: $Settings::BuildName $locale\n";
    print_locale_log "tinderbox: errorparser: $platform\n";
    print_locale_log "tinderbox: buildfamily: $platform\n";
    print_locale_log "tinderbox: version: $::Version\n";
    print_locale_log "tinderbox: END\n";
    print_locale_log "\n";

    close LOCLOG;

    if ($Settings::blat ne "" && $Settings::use_blat) {
        system("$Settings::blat $msg_log -t $Settings::Tinderbox_server");
    } else {
        system "$Settings::mail $Settings::Tinderbox_server "
            ." < $msg_log";
    }
    unlink "$msg_log";
}

sub mail_locale_finished_message {
    my ($start_time, $build_status, $logfile, $locale) = @_;

    # Rewrite LOG to OUTLOG, shortening lines.
    open OUTLOG, ">$logfile.last" or die "Unable to open logfile, $logfile: $!";

    my $platform = $Settings::OS =~ /^WIN/ ? 'windows' : 'unix';

    # Put the status at the top of the log, so the server will not
    # have to search through the entire log to find it.
    print OUTLOG "\n";
    print OUTLOG "tinderbox: tree: $Settings::BuildTree-$locale\n";
    print OUTLOG "tinderbox: builddate: $start_time\n";
    print OUTLOG "tinderbox: status: $build_status\n";
    print OUTLOG "tinderbox: build: $Settings::BuildName $locale\n";
    print OUTLOG "tinderbox: errorparser: $platform\n";
    print OUTLOG "tinderbox: buildfamily: $platform\n";
    print OUTLOG "tinderbox: version: $::Version\n";
    print OUTLOG "tinderbox: utilsversion: $::UtilsVersion\n";
    print OUTLOG "tinderbox: logcompression: $Settings::LogCompression\n";
    print OUTLOG "tinderbox: logencoding: $Settings::LogEncoding\n";
    print OUTLOG "tinderbox: END\n";

    if ($Settings::LogCompression eq 'gzip') {
        open GZIPLOG, "gzip -c $logfile |" or die "Couldn't open gzip'd logfile: $!\n";
        TinderUtils::encode_log(\*GZIPLOG, \*OUTLOG);
        close GZIPLOG;
    }
    elsif ($Settings::LogCompression eq 'bzip2') {
        open BZ2LOG, "bzip2 -c $logfile |" or die "Couldn't open bzip2'd logfile: $!\n";
        TinderUtils::encode_log(\*BZ2LOG, \*OUTLOG);
        close BZ2LOG;
    }
    else {
        open LOCLOG, "$logfile" or die "Couldn't open logfile, $logfile: $!";
        TinderUtils::encode_log(\*LOCLOG, \*OUTLOG);
        close LOCLOG;
    }    
    close OUTLOG;
    unlink($logfile);

    # If on Windows, make sure the log mail has unix lineendings, or
    # we'll confuse the log scraper.
    if ($platform eq 'windows') {
        open(IN,"$logfile.last") || die ("$logfile.last: $!\n");
        open(OUT,">$logfile.new") || die ("$logfile.new: $!\n");
        while (<IN>) {
            s/\r\n$/\n/;
            print OUT "$_";
        } 
        close(IN);
        close(OUT);
        File::Copy::move("$logfile.new", "$logfile.last") or die("move: $!\n");
    }

    if ($Settings::ReportStatus and $Settings::ReportFinalStatus) {
        if ($Settings::blat ne "" && $Settings::use_blat) {
            system("$Settings::blat $logfile.last -t $Settings::Tinderbox_server");
        } else {
            system "$Settings::mail $Settings::Tinderbox_server "
                ." < $logfile.last";
        }
    }
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
  $ENV{CVS_RSH} = "ssh" unless defined($ENV{CVS_RSH});
  my $fullsofttag = " ";
  $fullsofttag = " -r $Settings::BuildTag"
        unless not defined($Settings::BuildTag) or $Settings::BuildTag eq '';
  TinderUtils::run_shell_command "cd $builddir; cvs -d$moforoot co $fullsofttag -d fullsoft talkback/fullsoft";
  TinderUtils::run_shell_command "cd $builddir/fullsoft; $builddir/build/autoconf/make-makefile -d ..";
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
  my ($packaging_dir, $package_location, $url, $stagedir, $builddir, $cachebuild) = @_;
  my $status;

  if (is_windows()) {
    # need to convert the path in case we're using activestate perl
    $builddir = `cygpath -u $builddir`;
  }
  chomp($builddir);

  mkdir($stagedir, 0775);

  if (do_installer()) {
    if (is_windows()) {
      $ENV{INSTALLER_URL} = "$url/windows-xpi";
    } elsif (is_linux()) {
      $ENV{INSTALLER_URL} = "$url/linux-xpi/";
    } elsif (is_os2()) {
      $ENV{INSTALLER_URL} = "$url/os2-xpi/";
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

    my $push_raw_xpis;
    if ($Settings::stub_installer) {
      $push_raw_xpis = 1;
    } else {
      $push_raw_xpis = $Settings::push_raw_xpis;
    }

    if (is_windows() || is_os2()) {
      if ($Settings::stub_installer) {
        TinderUtils::run_shell_command "cp $package_location/stub/*.exe $stagedir/";
      }
      if ($Settings::sea_installer) {
        TinderUtils::run_shell_command "cp $package_location/sea/*.exe $stagedir/";
      }

      # If mozilla/dist/install/*.msi exists, copy it to the staging
      # directory.
      my @msi = grep { -f $_ } <${package_location}/*.msi>;
      if ( scalar(@msi) gt 0 ) {
        my $msi_files = join(' ', @msi);
        TinderUtils::run_shell_command "cp $msi_files $stagedir/";
      }

      if ($push_raw_xpis) {
        # We need to recreate the xpis with compression on, for update.
        # Since we've already copied over the 7zip-compressed installer, just
        # re-run the installer creation with 7zip disabled to get compressed
        # xpi's, then copy them to stagedir.
        #
        # Also set MOZ_PACKAGE_MSI to null to avoid repackaging it
        # unnecessarily.
        my $save_msi = $ENV{MOZ_PACKAGE_MSI};
        my $save_7zip = $ENV{MOZ_INSTALLER_USE_7ZIP};
        $ENV{MOZ_PACKAGE_MSI} = "";
        $ENV{MOZ_INSTALLER_USE_7ZIP} = "";
        TinderUtils::run_shell_command "make -C $packaging_dir installer";
        $ENV{MOZ_PACKAGE_MSI} = $save_msi;
        $ENV{MOZ_INSTALLER_USE_7ZIP} = $save_7zip;
        TinderUtils::run_shell_command "cp -r $package_location/xpi $stagedir/windows-xpi";
      }
    } elsif (is_linux()) {
      TinderUtils::run_shell_command "cp -r $package_location/raw/xpi $stagedir/linux-xpi";
      if ($Settings::stub_installer) {
        TinderUtils::run_shell_command "cp $package_location/stub/*.tar.* $stagedir/";
      }
      if ($Settings::sea_installer) {
        TinderUtils::run_shell_command "cp $package_location/sea/*.tar.* $stagedir/";
      }
      if ($push_raw_xpis) {
        my $xpi_loc = $package_location;
        if ($Settings::package_creation_path eq "/xpinstall/packager") {
          $xpi_loc = "$xpi_loc/raw";
        }
        TinderUtils::run_shell_command "cp -r $xpi_loc/xpi $stagedir/linux-xpi";
      }
    }
  } # do_installer

  if ($Settings::archive) {
    TinderUtils::run_shell_command "make -C $packaging_dir";

    my(@xforms_xpi);
    if ($Settings::BuildXForms) {
      TinderUtils::run_shell_command "cd $builddir/extensions/xforms; $builddir/build/autoconf/make-makefile -t $builddir -d ../..";
      TinderUtils::run_shell_command "make -C $builddir/extensions/xforms";

      @xforms_xpi = grep { -f $_ } <${builddir}/dist/xpi-stage/xforms.xpi>;
    }

    if (is_windows()) {
      TinderUtils::run_shell_command "cp $package_location/../*.zip $stagedir/";
      if ( scalar(@xforms_xpi) gt 0 ) {
        my $xforms_xpi_files = join(' ', @xforms_xpi);
        TinderUtils::run_shell_command "mkdir -p $stagedir/windows-xpi/" if ( ! -e "$stagedir/windows-xpi/" );
        TinderUtils::run_shell_command "cp $xforms_xpi_files $stagedir/windows-xpi/";
      }
    } elsif (is_mac()) {
      system("mkdir -p $package_location");
      system("mkdir -p $stagedir");

      # If .../*.dmg.gz exists, copy it to the staging directory.  Otherwise, copy
      # .../*.dmg if it exists.
      my @dmg;
      @dmg = grep { -f $_ } glob "${package_location}/../*.dmg.gz";
      if ( scalar(@dmg) eq 0 ) {
        @dmg = grep { -f $_ } glob "${package_location}/../*.dmg";
      }

      if ( scalar(@dmg) gt 0 ) {
        my $dmg_files = join(' ', @dmg);
        TinderUtils::print_log "Copying $dmg_files to $stagedir/\n";
        TinderUtils::run_shell_command "cp $dmg_files $stagedir/";
      } else {
        TinderUtils::print_log "No files to copy\n";
      }

      if ( scalar(@xforms_xpi) gt 0 ) {
        my $xforms_xpi_files = join(' ', @xforms_xpi);
        TinderUtils::run_shell_command "mkdir -p $stagedir/mac-xpi/" if ( ! -e "$stagedir/mac-xpi/" );
        TinderUtils::run_shell_command "cp $xforms_xpi_files $stagedir/mac-xpi/";
      }
    } elsif (is_os2()) {
      TinderUtils::run_shell_command "cp $package_location/../*.zip $stagedir/";
      if ( scalar(@xforms_xpi) gt 0 ) {
        my $xforms_xpi_files = join(' ', @xforms_xpi);
        TinderUtils::run_shell_command "mkdir -p $stagedir/os2-xpi/" if ( ! -e "$stagedir/os2-xpi/" );
        TinderUtils::run_shell_command "cp $xforms_xpi_files $stagedir/os2-xpi/";
      }
    } else {
      my $archive_loc = "$package_location/..";
      if ($Settings::package_creation_path eq "/xpinstall/packager") {
        $archive_loc = "$archive_loc/dist";
      }
      TinderUtils::run_shell_command "cp $archive_loc/*.tar.* $stagedir/";
      if ( scalar(@xforms_xpi) gt 0 ) {
        my $xforms_xpi_files = join(' ', @xforms_xpi);
        TinderUtils::run_shell_command "mkdir -p $stagedir/linux-xpi/" if ( ! -e "$stagedir/linux-xpi/" );
        TinderUtils::run_shell_command "cp $xforms_xpi_files $stagedir/linux-xpi/";
      }
    }
  }

  if ($cachebuild and $Settings::update_package) {
    update_create_package( objdir => $builddir,
                           stagedir => $stagedir,
                           url => $url,
                         );
  }

  # need to reverse status, since it's a "unix" truth value, where 0 means 
  # success
  return ($status)?0:1;
}

sub update_create_package {
    my %args = @_;

    my $locale = $args{'locale'};
    my $objdir = $args{'objdir'};
    my $stagedir = $args{'stagedir'};
    my $distdir = $args{'distdir'};
    my $url = $args{'url'};

    # $distdir refers to the directory containing the app we plan to package.
    # Note: Other uses of $objdir/dist should not be changed to use $distdir.
    $distdir = "$objdir/dist" if !defined($distdir);
    $locale = "en-US" if !defined($locale);

    my($update_product, $update_version, $update_platform);
    my($update_appv, $update_extv);

    if ( defined($Settings::update_product) ) {
        $update_product = $Settings::update_product;
    } else {
        TinderUtils::print_log "update_product is undefined, skipping update generation.\n";
        goto NOUPDATE;
    }

    if ( defined($Settings::update_version) ) {
        $update_version = $Settings::update_version;
    } else {
        TinderUtils::print_log "update_version is undefined, skipping update generation.\n";
        goto NOUPDATE;
    }

    if ( defined($Settings::update_platform) ) {
        $update_platform = $Settings::update_platform;
    } else {
        TinderUtils::print_log "update_platform is undefined, skipping update generation.\n";
        goto NOUPDATE;
    }

    if ( defined($Settings::update_appv) ) {
        $update_appv = $Settings::update_appv;
    } else {
        TinderUtils::print_log "update_appv is undefined, skipping update generation.\n";
        goto NOUPDATE;
    }

    if ( defined($Settings::update_extv) ) {
        $update_extv = $Settings::update_extv;
    } else {
        TinderUtils::print_log "update_extv is undefined, skipping update generation.\n";
        goto NOUPDATE;
    }

    # We're making an update.
    TinderUtils::print_log "\nGenerating complete update...\n";
    my $temp_stagedir = "$stagedir/build.$$";
    system("mkdir -p $temp_stagedir");
    TinderUtils::run_shell_command "make -C $objdir/tools/update-packaging full-update STAGE_DIR=$temp_stagedir DIST=$distdir AB_CD=$locale";

    my $update_file = "update.mar";
    my @updatemar;
    @updatemar = grep { -f $_ } <${temp_stagedir}/*.mar>;
    if ( scalar(@updatemar) ge 1 ) {
      $update_file = $updatemar[0];
      $update_file =~ s:^$temp_stagedir/(.*)$:$1:g;
    }

    system("rsync -av $temp_stagedir/$update_file $stagedir/");
    system("rm -rf $temp_stagedir");

    my $update_path = "$stagedir/$update_file";
    my $update_fullurl = "$url/$update_file";

    if ( -f $update_path ) {
      # Make update dist directory.
      TinderUtils::run_shell_command "mkdir -p $objdir/dist/update/";
      TinderUtils::print_log "\nGathering complete update info...\n";

      my $buildid;

      # First try to get the build ID from the files in dist/.
      my $find_master = `find $distdir -iname master.ini -print`;
      my @find_output = split(/\n/, $find_master);

      if (scalar(@find_output) gt 0) {
          my $master = read_file($find_output[0]);
          # BuildID = "2005100517"
          if ( $master =~ /^BuildID\s+=\s+\"(\d+)\"\s+$/m ) {
              $buildid = $1;
          }
      }

      # If the first method of getting the build ID failed, grab it from config.
      if (!defined($buildid)) {
          $buildid = `cd $objdir/config/ && cat build_number`;
          chomp($buildid);
      }

      TinderUtils::print_log "Got build ID $buildid.\n";
      # Gather stats for update file.
      update_create_stats( update => $update_path,
                           type => "complete",
                           output_file => "$objdir/dist/update/update.snippet",
                           url => $update_fullurl,
                           buildid => $buildid,
                           appversion => $update_appv,
                           extversion => $update_extv,
                         );

      # Push update information to update-staging/auslite.

      # Only push the build schema 0 data if this is a trunk build.
      if ( 0 and $update_version eq "trunk" ) {
          TinderUtils::print_log "\nPushing first-gen update info...\n";
          my $path = "/opt/aus2/incoming/0";
          $path = "$path/$update_product/$update_platform";

          TinderUtils::run_shell_command "ssh -i $ENV{HOME}/.ssh/aus cltbld\@aus-staging.mozilla.org mkdir -p $path";
          TinderUtils::run_shell_command "scp -i $ENV{HOME}/.ssh/aus $objdir/dist/update/update.snippet.0 cltbld\@aus-staging.mozilla.org:$path/$locale.txt";
      } else {
          TinderUtils::print_log "\nNot pushing first-gen update info...\n";
      }

      # Push the build schema 1 data.
      if ( 0 ) {
          TinderUtils::print_log "\nPushing second-gen update info...\n";
          my $path = "/opt/aus2/incoming/1";
          $path = "$path/$update_product/$update_version/$update_platform";

          TinderUtils::run_shell_command "ssh -i $ENV{HOME}/.ssh/aus cltbld\@aus-staging.mozilla.org mkdir -p $path";
          TinderUtils::run_shell_command "scp -i $ENV{HOME}/.ssh/aus $objdir/dist/update/update.snippet.0 cltbld\@aus-staging.mozilla.org:$path/$locale.txt";
      }

      # Push the build schema 2 data.
      {
          TinderUtils::print_log "\nPushing third-gen update info...\n";
          #my $path = "/opt/aus2/build/0";
          my $path = "/tmp/l10n-test3/opt/aus2/build/0";
          $path = "$path/$update_product/$update_version/$update_platform/$buildid/$locale";

          TinderUtils::run_shell_command "ssh -i $ENV{HOME}/.ssh/aus cltbld\@aus-staging.mozilla.org mkdir -p $path";
          TinderUtils::run_shell_command "scp -i $ENV{HOME}/.ssh/aus $objdir/dist/update/update.snippet.1 cltbld\@aus-staging.mozilla.org:$path/complete.txt";
      }

      TinderUtils::print_log "\nCompleted pushing update info...\n";
      TinderUtils::print_log "\nUpdate build completed.\n\n";
    } else {
      TinderUtils::print_log "Error: Unable to get info on '$update_path' or include in upload because it doesn't exist!\n";
    }

    NOUPDATE:
}

sub read_file {
  my ($filename) = @_;

  if ( ! -e $filename ) {
    die("read_file: file $filename doesn't exist!");
  }

  local($/) = undef;

  open(FILE, "$filename") or die("read_file: unable to open $filename for reading!");
  my $text = <FILE>;
  close(FILE);

  return $text;
}

sub update_create_stats {
  my %args = @_;
  my $update = $args{'update'};
  my $type = $args{'type'};
  my $output_file_base = $args{'output_file'};
  my $url = $args{'url'};
  my $buildid = $args{'buildid'};
  my $appversion = $args{'appversion'};
  my $extversion = $args{'extversion'};

  my($hashfunction, $hashvalue, $size, $output, $output_file);

  $hashfunction = "md5";
  if ( defined($Settings::update_hash) ) {
    $hashfunction = $Settings::update_hash;
  }
  ($size) = (stat($update))[7];
  $hashvalue;

  if ($hashfunction eq "sha1") {
    $hashvalue = `sha1sum $update`;
  } else {
    $hashvalue = `md5sum $update`;
  }
  chomp($hashvalue);

  $hashvalue =~ s:^(\w+)\s.*$:$1:g;
  $hashfunction = uc($hashfunction);

  if ( defined($Settings::update_filehost) ) {
    $url =~ s|^([^:]*)://([^/:]*)(.*)$|$1://$Settings::update_filehost$3|g;
  }

  $output  = "$type\n";
  $output .= "$url\n";
  $output .= "$hashfunction\n";
  $output .= "$hashvalue\n";
  $output .= "$size\n";
  $output .= "$buildid\n";

  $output_file = "$output_file_base.0";
  if (defined($output_file)) {
    open(UPDATE_FILE, ">$output_file")
      or die "ERROR: Can't open '$output_file' for writing!";
    print UPDATE_FILE $output;
    close(OUTPUT_FILE);
  } else {
    printf($output);
  }

  $output  = "$type\n";
  $output .= "$url\n";
  $output .= "$hashfunction\n";
  $output .= "$hashvalue\n";
  $output .= "$size\n";
  $output .= "$buildid\n";
  $output .= "$appversion\n";
  $output .= "$extversion\n";

  $output_file = "$output_file_base.1";
  if (defined($output_file)) {
    open(UPDATE_FILE, ">$output_file")
      or die "ERROR: Can't open '$output_file' for writing!";
    print UPDATE_FILE $output;
    close(OUTPUT_FILE);
  } else {
    printf($output);
  }
}

sub packit_l10n {
  my ($srcdir, $objdir, $packaging_dir, $package_location, $url, $stagedir, $cachebuild) = @_;
  my $status;

  TinderUtils::print_log "Starting l10n builds\n";

  foreach my $wgeturl (keys(%Settings::WGetFiles)) {
    my $status = TinderUtils::run_shell_command_with_timeout("wget --non-verbose --output-document \"$Settings::WGetFiles{$wgeturl}\" $wgeturl",
                                                             $Settings::WGetTimeout);
    if ($status->{exit_value} != 0) {
      TinderUtils::print_log "Error: wget failed or timed out.\n";
      return (("busted"));
    }
  }

  unless (open(ALL_LOCALES, "<$srcdir/$Settings::LocaleProduct/locales/all-locales")) {
      TinderUtils::print_log "Error: Couldn't read $srcdir/$Settings::LocaleProduct/locales/all-locales.\n";
      return (("testfailed"));
  }

  my @locales = <ALL_LOCALES>;
  close ALL_LOCALES;

  map { /([a-z-]+)/i; $_ = $1 } @locales;

  TinderUtils::print_log "Building following locales: @locales\n";

  my $start_time = TinderUtils::adjust_start_time(time());
  foreach my $locale (@locales) {
      mail_locale_started_message($start_time, $locale);
      TinderUtils::print_log "$locale...";

      my $logfile = "$Settings::DirName-$locale.log";
      my $tinderstatus = 'success';
      open LOCLOG, ">$logfile";

      # Make the log file flush on every write.
      my $oldfh = select(LOCLOG);
      $| = 1;
      select($oldfh);

    if (do_installer()) {
      if (is_windows()) {
        $ENV{INSTALLER_URL} = "$url/windows-xpi";
      } elsif (is_linux()) {
        $ENV{INSTALLER_URL} = "$url/linux-xpi/";
      } else {
        die "Can't make installer for this platform.\n";
      }
  
      mkdir($package_location, 0775);
  
      # the Windows installer scripts currently require Activestate Perl.
      # Put it ahead of cygwin perl in the path.
      my $save_path;
      if (is_windows()) {
        $save_path = $ENV{PATH};
        $ENV{PATH} = $Settings::as_perl_path.":".$ENV{PATH};
      }

      # the one operation we care about saving status of
      $status = run_locale_shell_command "$Settings::Make -C $objdir/$Settings::LocaleProduct/locales installers-$locale $Settings::BuildLocalesArgs";
      if ($status != 0) {
        $tinderstatus = 'busted';
      }
  
      if ($tinderstatus eq 'success') {
        if (is_windows()) {
          run_locale_shell_command "mkdir -p $stagedir/windows-xpi/";
          run_locale_shell_command "cp $package_location/*$locale.langpack.xpi $stagedir/windows-xpi/$locale.xpi";
        } elsif (is_mac()) {
          # Instead of adding something here (which will never be called),
          # please add your code to the is_mac() section below.
        } elsif (is_linux()) {
          run_locale_shell_command "mkdir -p $stagedir/linux-xpi/";
          run_locale_shell_command "cp $package_location/*$locale.langpack.xpi $stagedir/linux-xpi/$locale.xpi";
        }
      }

      if (is_windows()) {
        $ENV{PATH} = $save_path;
        #my $dos_stagedir = `cygpath -w $stagedir`;
        #chomp ($dos_stagedir);
      }
      mkdir($stagedir, 0775);
  
      if (is_windows()) {
        if ($Settings::stub_installer && $tinderstatus ne 'busted' ) {
          run_locale_shell_command "cp $package_location/stub/*.exe $stagedir/";
        }
        if ($Settings::sea_installer && $tinderstatus ne 'busted' ) {
          run_locale_shell_command "cp $package_location/sea/*.exe $stagedir/";
        }
      } elsif (is_linux()) {
        if ($Settings::stub_installer && $tinderstatus ne 'busted' ) {
          run_locale_shell_command "cp $package_location/stub/*.tar.* $stagedir/";
        }
        if ($Settings::sea_installer && $tinderstatus ne 'busted' ) {
          run_locale_shell_command "cp $package_location/sea/*.tar.* $stagedir/";
        }
      }
    } # do_installer
  
    if ($Settings::archive && $tinderstatus ne 'busted' ) {
      if (is_windows()) {
        run_locale_shell_command "cp $package_location/../*$locale*.zip $stagedir/";
      } elsif (is_mac()) {
        $status = run_locale_shell_command "$Settings::Make -C $objdir/$Settings::LocaleProduct/locales installers-$locale $Settings::BuildLocalesArgs";
        if ($status != 0) {
          $tinderstatus = 'busted';
        }
        system("mkdir -p $package_location");
        system("mkdir -p $stagedir");

        # If .../*.dmg.gz exists, copy it to the staging directory.  Otherwise, copy
        # .../*.dmg if it exists.
        my @dmg;
        @dmg = grep { -f $_ } glob "${package_location}/../*$locale*.dmg.gz";
        if ( scalar(@dmg) eq 0 ) {
          @dmg = grep { -f $_ } glob "${package_location}/../*$locale*.dmg";
        }

        if ( scalar(@dmg) gt 0 ) {
          my $dmg_files = join(' ', @dmg);
          TinderUtils::print_log "Copying $dmg_files to $stagedir/\n";
          TinderUtils::run_shell_command "cp $dmg_files $stagedir/";
        } else {
          TinderUtils::print_log "No files to copy\n";
        }

        if ($tinderstatus eq 'success') {
          run_locale_shell_command "mkdir -p $stagedir/mac-xpi/";
          run_locale_shell_command "cp $package_location/*$locale.langpack.xpi $stagedir/mac-xpi/$locale.xpi";
        }
      } else {
        my $archive_loc = "$package_location/..";
        if ($Settings::package_creation_path eq "/xpinstall/packager") {
          $archive_loc = "$archive_loc/dist";
        }
        run_locale_shell_command "cp $archive_loc/*$locale*.tar.* $stagedir/";
      }

      if ($tinderstatus eq 'success' and
          $cachebuild and
          $Settings::update_package) {
        if ( ! -d "$objdir/dist/l10n-stage" ) {
            die "packit_l10n: $objdir/dist/l10n-stage is not a directory!\n";
        }

        if ( ! -d "$objdir/dist/host" ) {
            die "packit_l10n: $objdir/dist/host is not a directory!\n";
        }

        system("cd $objdir/dist/l10n-stage; ln -s ../host .");
        update_create_package( objdir => $objdir,
                               stagedir => $stagedir,
                               url => $url,
                               locale => $locale,
                               distdir => "$objdir/dist/l10n-stage",
                             );
      }
    }

    for my $localetest (@Settings::CompareLocaleDirs) {
      my $originaldir = "$srcdir/$localetest/locales/en-US";
      my $localedir;
      if ($Settings::CompareLocalesAviary) {
        $localedir = "$srcdir/$localetest/locales/$locale";
      }
      else {
        $localedir = "$srcdir/../l10n/$locale/$localetest";
      }
      $status = run_locale_shell_command "$^X $srcdir/toolkit/locales/compare-locales.pl $originaldir $localedir";
      if ($tinderstatus eq 'success' && $status != 0) {
        $tinderstatus = 'testfailed';
      }
    }
    close LOCLOG;

    mail_locale_finished_message($start_time, $tinderstatus, $logfile, $locale);
    TinderUtils::print_log "$tinderstatus.\n";

  } # foreach

  # remove en-US files since we're building that on a different system
  TinderUtils::run_shell_command "rm -f $stagedir/*en-US* $stagedir/*-xpi";

  TinderUtils::print_log "locales completed.\n";

    # need to reverse status, since it's a "unix" truth value, where 0 means 
    # success
  return ($status)?0:1;

} # packit_l10n
        

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

  # The ReleaseToDated and ReleaseToLatest configuration settings give us the
  # ability to fine-tune where release files are stored.  ReleaseToDated
  # will store the release files in a directory of the form
  #
  #   nightly/YYYY-MM-DD-HH-<milestone>
  #
  # while ReleaseToLatest stores the release files in a directory of the form
  #
  #   nightly/latest-<milestone>
  #
  # Before we allowed the fine-tuning, we either published to both dated and
  # latest, or to neither.  In case some installations don't have these
  # variables defined yet, we want to set them to default values here.
  # Hopefully, though, we'll have also updated tinder-defaults.pl if we've
  # updated post-mozilla-rel.pl from CVS.

  $Settings::ReleaseToDated = 1 if !defined($Settings::ReleaseToDated);
  $Settings::ReleaseToLatest = 1 if !defined($Settings::ReleaseToLatest);

  if ( $Settings::ReleaseToDated ) {
    TinderUtils::run_shell_command "ssh $ssh_opts -l $Settings::ssh_user $ssh_server mkdir -p $upload_path/$short_ud";
    TinderUtils::run_shell_command "rsync -av -e \"ssh $ssh_opts\" $upload_directory/ $Settings::ssh_user\@$ssh_server:$upload_path/$short_ud/";
    TinderUtils::run_shell_command "ssh $ssh_opts -l $Settings::ssh_user $ssh_server chmod -R 775 $upload_path/$short_ud";

    if ( $cachebuild and $Settings::ReleaseToLatest ) {
      TinderUtils::run_shell_command "ssh $ssh_opts -l $Settings::ssh_user $ssh_server cp -dpf $upload_path/$short_ud/* $upload_path/latest-$Settings::milestone/";
    }
  } elsif ( $Settings::ReleaseToLatest ) {
    TinderUtils::run_shell_command "ssh $ssh_opts -l $Settings::ssh_user $ssh_server mkdir -p $upload_path";
    TinderUtils::run_shell_command "scp $scp_opts -r $upload_directory/* $Settings::ssh_user\@$ssh_server:$upload_path/latest-$Settings::milestone/";
    TinderUtils::run_shell_command "ssh $ssh_opts -l $Settings::ssh_user $ssh_server chmod -R 775 $upload_path/latest-$Settings::milestone/";
  }

  return 1;
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

sub cleanup {
  # As it's a temporary file meant to control the last-built file, remove
  # last-built.new when we are done.

  if ( -e "last-built.new" ) {
    unlink "last-built.new";
  }
}

sub returnStatus{
  # build-seamonkey-util.pl expects an array, if no package is uploaded,
  # a single-element array with 'busted', 'testfailed', or 'success' works.
  my ($logtext, @status) = @_;
  TinderUtils::print_log "$logtext\n";
  return @status;
}

sub PreBuild {

  # last-built.new is used to track a respin as it is currently happening and
  # later takes the place of last-built.  If it exists at this point in the
  # build, remove it because we do not want old last-built.new files to
  # introduce noise or odd conditions in the build.

  if ( -e "last-built.new" ) {
    unlink "last-built.new";
  }

  # We want to be able to remove the last-built file at any time of
  # day to trigger a respin, even if it's before the designated build
  # hour.  This means that we want to do a cached build if any of the
  # following are true:
  # 1. there is no last-built file
  # 2. the last-built file is more than 24 hours old
  # 3. the last-built file is before the most recent occurrence of the
  #    build hour (which may not be today!)
  # (3) subsumes (2), so there's no need to check for (2) explicitly.

  if ( -e "last-built" ) {
    my $last = (stat "last-built")[9];
    TinderUtils::print_log "Most recent nightly build: " .
      localtime($last) . "\n";

    # Figure out the most recent past time that's at
    # $Settings::build_hour.
    my $now = time;
    my $most_recent_build_hour =
      POSIX::mktime(0, 0, $Settings::build_hour,
                    (localtime($now))[3 .. 8]);
    if ($most_recent_build_hour > $now) {
      $most_recent_build_hour -= 86400; # subtract a day
    }
    TinderUtils::print_log "Most recent build hour:    " .
      localtime($most_recent_build_hour) . "\n";

    # If that time is older than last-built, then it's time for a new
    # nightly.
    $cachebuild = ($most_recent_build_hour > $last);
  } else {
    # No last-built file.  Either it's the first build or somebody
    # removed it to trigger a respin.
    TinderUtils::print_log
      "No record of generating a nightly build.\n";
    $cachebuild = 1;
  }

  if ($cachebuild) {
    # Within a cachebuild cycle (when it's determined a release build is
    # needed), create a last-built.new file.  Later, if last-built.new exists,
    # we move last-built.new to last-built.  If last-built.new doesn't exist,
    # we do nothing.
    #
    # This allows us to remove last-built.new and have the next build cycle
    # be a respin even if the current cycle already is one.

    if ( ! -e "last-built.new" ) {
      open BLAH, ">last-built.new";
      close BLAH;
    }

    TinderUtils::print_log "starting nightly release build\n";
    # clobber the tree if set to do so
    $Settings::clean_objdir = 1 if !defined($Settings::clean_objdir);
    $Settings::clean_srcdir = 1 if !defined($Settings::clean_srcdir);
    if ($Settings::ObjDir eq '') {
      $Settings::clean_srcdir = ($Settings::clean_srcdir || $Settings::clean_objdir);
      $Settings::clean_objdir = 0;
    }
    if ($Settings::clean_objdir) {
      my $objdir = 'mozilla/'.$Settings::ObjDir;
      if ( -d $objdir) {
        TinderUtils::run_shell_command "rm -rf $objdir";
      }
    }
    if ($Settings::clean_srcdir) {
      if ( -d "mozilla") {
        TinderUtils::run_shell_command "rm -rf mozilla";
      }
    }
    if ( -d "l10n" ) {
      TinderUtils::run_shell_command "rm -rf l10n";
    }
  } else {
    TinderUtils::print_log "starting non-release build\n";
  }
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

  my $srcdir = "$mozilla_build_dir/${Settings::Topsrcdir}";
  my $objdir = $srcdir;
  if ($Settings::ObjDir ne '') {
    $objdir .= "/${Settings::ObjDir}";
  }
  unless ( -e $objdir) {
    TinderUtils::print_log "No $objdir to make packages in.\n";
    return (("testfailed")) ;
  }

  system("rm -f $objdir/dist/bin/codesighs");

  # set up variables with default values
  # need to modify the settings from tinder-config.pl
  my $package_creation_path = $objdir . $Settings::package_creation_path;
  my $package_location;
  if (is_windows() || is_mac() || is_os2() || $Settings::package_creation_path ne "/xpinstall/packager") {
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

  if (is_windows()) {
    # hack for cygwin installs with "unix" filetypes
    TinderUtils::run_shell_command "unix2dos $mozilla_build_dir/mozilla/LICENSE";
    TinderUtils::run_shell_command "unix2dos $mozilla_build_dir/mozilla/mail/LICENSE.txt";
    TinderUtils::run_shell_command "unix2dos $mozilla_build_dir/mozilla/README.txt";
    TinderUtils::run_shell_command "unix2dos $mozilla_build_dir/mozilla/browser/EULA";
  }

  my $upload_directory;

  if ($cachebuild) {
    TinderUtils::print_log "uploading nightly release build\n";
    $upload_directory = "$datestamp";
    $url_path         = $url_path . "/" . $upload_directory;
  } else {
    $ftp_path   = $Settings::tbox_ftp_path;
    $upload_directory = shorthost() . "-" . "$Settings::milestone";
    $url_path   = $Settings::tbox_url_path . "/" . $upload_directory;
  }

  if (!is_os2()) {
    processtalkback($cachebuild && $Settings::shiptalkback, $objdir, "$mozilla_build_dir/${Settings::Topsrcdir}");
  }

  $upload_directory = $package_location . "/" . $upload_directory;

  if (!$Settings::ConfigureOnly) {
    unless (packit($package_creation_path,$package_location,$url_path,$upload_directory,$objdir,$cachebuild)) {
      cleanup();
      return returnStatus("Packaging failed", ("testfailed"));
    }
  }

  if ($Settings::BuildLocales) {
    # Check for existing mar tool.
    my $mar_tool = "$srcdir/modules/libmar/tool/mar";
    if (! -f $mar_tool) {
      TinderUtils::run_shell_command "make -C $srcdir/nsprpub && make -C $srcdir/config && make -C $srcdir/modules/libmar";
      # mar tool should exist now.
      if (! -f $mar_tool) {
	TinderUtils::print_log "Failed to build $mar_tool\n";
      }
    }

    packit_l10n($srcdir,$objdir,$package_creation_path,$package_location,$url_path,$upload_directory,$cachebuild);
  }

  unless (pushit($Settings::ssh_server,
                 $ftp_path,$upload_directory, $cachebuild)) {
    cleanup();
    return returnStatus("Pushing package $upload_directory failed", ("testfailed"));
  }

  if ($cachebuild) { 
    # remove saved builds older than a week and save current build
    TinderUtils::run_shell_command "find $mozilla_build_dir -type d -name \"200*-*\" -maxdepth 1 -prune -mtime +7 -print | xargs rm -rf";
    TinderUtils::run_shell_command "cp -rpf $upload_directory $mozilla_build_dir/$datestamp";

    # Above, we created last-built.new at the start of the cachebuild cycle.
    # If the last-built.new file still exists, move it to the last-built file.

    if ( -e "last-built.new" ) {
      system("mv last-built.new last-built");
    }

    return reportRelease ("$url_path\/", "$datestamp");
  } else {
    return (("success"));
  }
} # end main

# Need to end with a true value, (since we're using "require").
1;
