#!/usr/bin/perl
#

#
# This script gets called after a full mozilla build & test.
#
# packages and delivers mozilla bits
# Assumptions:
#  no objdir 
#  mozilla tree
#  produced builds get put in Topsrcdir/installer/sea
#

use strict;

package PostMozilla;

sub talkbackProcessing {
  my ($mozilla_build_dir, $cacheit) = @_;
  TinderUtils::run_shell_command "make -C $mozilla_build_dir/mozilla deliver";
}

sub packit {
  my ($packaging_dir, $package_location, $url, $stagedir) = @_;
  my $status;
  $ENV{INSTALLER_URL} = "$url/windows-xpi";
  TinderUtils::print_log "INSTALLER_URL is " . $ENV{INSTALLER_URL} . "\n";
  # the one operation we care about saving status of
  $status = TinderUtils::run_shell_command "make -C $packaging_dir installer";
  

  #my $dos_stagedir = `cygpath -w $stagedir`;
  #chomp ($dos_stagedir);
  mkdir  $stagedir, 0775;
  TinderUtils::run_shell_command "cp -r $package_location/xpi $stagedir/windows-xpi";
  TinderUtils::run_shell_command "cp $package_location/sea/*.exe $package_location/stub/*.exe  $stagedir/";

  TinderUtils::run_shell_command "make -C $packaging_dir";
  TinderUtils::run_shell_command "cp $package_location/../*.zip $stagedir/";
  
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

  # need to convert the path in case we're using activestate perl
  $upload_directory = `cygpath -u $upload_directory`;
  chomp($upload_directory);
  my $short_ud = `basename $upload_directory`;
  chomp ($short_ud);

  TinderUtils::run_shell_command "ssh -1 -l $Settings::ssh_user $ssh_server mkdir -p $upload_path";
  TinderUtils::run_shell_command "scp -oProtocol=1 -r $upload_directory $Settings::ssh_user\@$ssh_server:$upload_path";
  TinderUtils::run_shell_command "ssh -1 -l $Settings::ssh_user $ssh_server chmod -R 775 $upload_path/$short_ud";

  if ($cachebuild) {
    TinderUtils::run_shell_command "ssh -1 -l $Settings::ssh_user $ssh_server cp -pf $upload_path/$short_ud/* $upload_path/latest-$Settings::milestone/";
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
  my $donemessage =   "\n" .
                      "Win32 Mozilla Build available at: \n" .
                      "$url \n";
  open(TMPMAIL, ">tmpmail.txt");
  print TMPMAIL "$donemessage \n";
  close(TMPMAIL);

  TinderUtils::print_log ("$donemessage \n");
  my $subject = "[build] $datestamp Mozilla Win32 Build Complete";
  system("$Settings::blat tmpmail.txt -to $Settings::notify_list -s \"$subject\"");
  unlink "tmpmail.txt";
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
  unless ( -e $mozilla_build_dir) {
    TinderUtils::print_log "No $mozilla_build_dir to make packages in.\n";
    return (("testfailed")) ;
  }

  #$mozilla_build_dir = `cygpath $mozilla_build_dir`; # cygnusify the path
  #chop $mozilla_build_dir; # remove whitespace 

  #unless ( -e $mozilla_build_dir) {
    #TinderUtils::print_log "No cygnified $mozilla_build_dir to make packages in.\n";
    #return (("testfailed")) ;
  #}

  # set up variables with default values
  my $last_build_day = 0;
  # need to modify the settings from tinder-config.pl
  my $package_creation_path = $mozilla_build_dir . "/" . $Settings::package_creation_path;
  my $package_location = $mozilla_build_dir . "/" . $Settings::package_location;
  my $upload_file = $mozilla_build_dir . "/" . $Settings::upload_file;
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

  # hack for cygwin installs with "unix" filetypes
  TinderUtils::run_shell_command "unix2dos $mozilla_build_dir/mozilla/LICENSE";
  TinderUtils::run_shell_command "unix2dos $mozilla_build_dir/mozilla/README.txt";

  my $upload_directory = "$ENV{COMPUTERNAME}";
  my $cachebuild = 0;

  if (cacheit($c_hour,$c_yday,$Settings::build_hour,$last_build_day)) {
    TinderUtils::print_log "attempting to cache today's build\n";
    $url_path         = $url_path . "/$datestamp";
    $upload_directory = "$datestamp";
    $cachebuild = 1;
  }else{
    $url_path   = "http://ftp.mozilla.org/pub/mozilla.org/mozilla/tinderbox-builds/$ENV{COMPUTERNAME}";
    $ftp_path   = "/home/ftp/pub/mozilla/tinderbox-builds";
  }

  talkbackProcessing($mozilla_build_dir,$cachebuild);

  $upload_directory = $package_location . "/" . $upload_directory;

  unless (packit($package_creation_path,$package_location,$url_path,$upload_directory)) {
    return returnStatus("Packaging failed", ("testfailed"));
  }

  unless (pushit($Settings::ssh_server,
		 $ftp_path,$upload_directory, $cachebuild)) {
    return returnStatus("Pushing package $upload_directory failed", ("testfailed"));
  }

  if (cacheit($c_hour,$c_yday,$Settings::build_hour,$last_build_day)) { 
    TinderUtils::run_shell_command "mv $upload_directory/../../symbols $upload_directory";
    TinderUtils::run_shell_command "mv $upload_directory $mozilla_build_dir";
    open BLAH, ">last-built"; 
    close BLAH;
    return reportRelease ("$url_path", "$datestamp");
  } else {
    return (("success"));
  }
} # end main

# Need to end with a true value, (since we're using "require").
1;
