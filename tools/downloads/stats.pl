#!/usr/local/bin/perl -w

use File::Find;
use File::Path;     # for rmtree();
use Cwd;

sub usage() {
  die <<END_USAGE
  usage: $0 [--days n] [--no-graph]
END_USAGE
}

my $script_dir = "/lxr-root/download-stats/mcafee";
my $log_dir = "/lxr-root/ftp-mozilla.netscape.com";

my $nograph = 0;
my $days = 1;

sub ParseArgs {
    my $args = {};
    my $arg;
    while ($arg = shift @ARGV) {
	if($arg eq '--help') {
	    usage();
	    exit(0);
	}
	if($arg eq '--no-graph') {
            $nograph = 1; next;
	}
	if($arg eq '--days') {
	    $arg = shift @ARGV;
	    $days = $arg;
	    next;
	}
    }
    
    return $args;
}

sub ApplyArgs {
    my ($args) = @_;

    my ($variable_name, $value);
    while (($variable_name, $value) = each %{$args}) {
        eval "\$Prefs::$variable_name = \"$value\";";
    }
}


sub get_system_cwd {
    my $a = Cwd::getcwd()||`pwd`;
    chomp($a);
    return $a;
}


# [English name, download size, filename]
my @releases = (
  ["Camino 0.7/MacOSX",       7928194,  "Camino-0.7.dmg.gz"],

  ["Firebird 0.6.1/Win32",    7095659,  "MozillaFirebird-0.6.1-win32.zip"],
  ["Firebird 0.6.1/Linux",    9686069,  "MozillaFirebird-0.6.1-i686-pc-linux-gnu.tar.gz"],
  ["Firebird 0.6.1/MacOSX",  11571629,  "MozillaFirebird-0.6.1-mac.dmg.gz"],

  ["Thunderbird 0.1/Win32",   9368099,  "thunderbird-0.1-win32.zip"],
  ["Thunderbird 0.1/Linux",   9644700,  "thunderbird-0.1-i686-pc-linux-gtk2-gnu.tar.bz2"],
  ["Thunderbird 0.1/MacOSX", 11119315,  "thunderbird-0.1-macosx.dmg.gz"],

  ["Thunderbird 0.2/Win32",   7693954,  "thunderbird-0.2-win32.zip"],
  ["Thunderbird 0.2/Linux",   9993797,  "thunderbird-0.2-i686-pc-linux-gtk2-gnu.tar.bz2"],
  ["Thunderbird 0.2/MacOSX", 11686079,  "thunderbird-0.2-macosx.dmg.gz"],

  ["Mozilla 1.4/Win32",      12263120,  "mozilla-win32-1.4-installer.exe"],
  ["Mozilla 1.4/Win32",        227024,  "mozilla-win32-1.4-stub-installer.exe"],
  ["Mozilla 1.4/Win32",      10896782,  "mozilla-win32-1.4-talkback.zip"],

  ["Mozilla 1.4/Linux",         97649,  "mozilla-i686-pc-linux-gnu-1.4-installer.tar.gz"],
  ["Mozilla 1.4/Linux",      14037580,  "mozilla-i686-pc-linux-gnu-1.4-sea.tar.gz"],
  ["Mozilla 1.4/Linux",      12528153,  "mozilla-i686-pc-linux-gnu-1.4.tar.gz"],
  ["Mozilla 1.4/Linux",      12345848,  "mozilla-i686-pc-linux-gnu-egcs112-1.4.tar.gz"],
  ["Mozilla 1.4/Linux",      34227695,  "mozilla-1.4-0.7.3.i386.rpm"],
  ["Mozilla 1.4/Linux",       9208167,  "mozilla-1.4-0.i386.rpm"],


  ["Mozilla 1.4/MacOSX",     15855366,  "mozilla-mac-MachO-1.4.dmg.gz"],
);


sub print_log {
    my ($text) = @_;
    print LOG $text;
    print $text;
}

sub run_shell_command {
    my ($shell_command) = @_;
    local $_;

    my $status = 0;
    chomp($shell_command);
    print_log "$shell_command\n";
    open CMD, "$shell_command 2>&1 |" or die "open: $!";
    print_log $_ while <CMD>;
    close CMD or $status = 1;
    return $status;
}


sub send_results_to_server {
    my ($value, $data, $testname, $tbox) = @_;

    my $tmpurl = "http://axolotl.mozilla.org/graph/collect.cgi";
    $tmpurl .= "?value=$value&data=$data&testname=$testname&tbox=$tbox";

    print "send_results_to_server(): \n";
    print "tmpurl = $tmpurl\n";

    system ("/usr/local/bin/wget", "-O", "/dev/null", $tmpurl);

}


# Find list of logfiles that are less than a day old.
my @files;
sub generate_files_list {
  my $sub = 
  sub {
      my ($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,
	  $atime,$mtime,$ctime,$blksize,$blocks) = 0;
      
      # Don't open .gz files.  Apparently we're compressing
      # logs after 2 weeks or so.
      unless(/.gz/) {
	  open FILE, $_;
	  ($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,
	   $atime,$mtime,$ctime,$blksize,$blocks) = stat(FILE);  
	  my $time = time();
	  my $access_delta_sec = $time - $mtime;
	  my $day = 60*60*24*$days;
	  if($access_delta_sec < $day) {
	      push(@files, $_);
	  }
	  close FILE;
      } 
  };  

  File::Find::find($sub, $log_dir);
}

sub graph_friendly_name {
    my ($name) = @_;

    $name =~ s/\s/_/g;
    $name =~ s/\//_/g;
    return $name;
}


# main
{
  #usage() if $#ARGV != -1;

  my $args = ParseArgs();
  ApplyArgs($args);

  # Find list of log files to search
  generate_files_list();

  print "\n\nLooking at $#files files...\n";

  # Setup totals area if it's not there.
  unless(-d "$script_dir/totals") {
      mkdir("$script_dir/totals", 0777);
  }

  #
  # Build up daily numbers.
  #
  
  # Clean up from last run.
  File::Path::rmtree("$script_dir/dailys", 0, 0);
  mkdir("$script_dir/dailys", 0777);

  print "Releases:\n";
  foreach $release (@releases) {
      print "$release->[0] :\t ";

      my $downloads = 0;
      foreach $file (@files) {
	  my $filesize = $release->[1];
	  my $filename = $release->[2];

	  my $grep_log;
	  open FILE, "$log_dir/$file";

	  while(<FILE>) {
	      if (($_ =~ /$filesize/g) and ($_ =~ /$filename/g)){
		  $downloads++;
	      }
	  }
	  close FILE;
      }
      print "$downloads\n";

      # graph-server-friendly release name
      my $new_release = graph_friendly_name("$release->[0]");

      # Build up daily data.

      # Read last value from file.
      my $last_daily = 0;
      if(-e "$script_dir/dailys/$new_release") {
	  open LOG, "$script_dir/dailys/$new_release";
	  while(<LOG>) {
	      chomp;
	      $last_daily = $_;
              #print "daily = $last_daily + $downloads\n";
	      last;
	  }
	  close LOG;
      }

      # Compute new daily.
      my $new_daily = $last_daily + $downloads;

      # Write out new daily to file.
      open LOG, ">$script_dir/dailys/$new_release";
      print LOG $new_daily;
      close LOG;


  }

  #
  # Send to server.
  #
  
  
  if(($days == 1) and !($nograph)) {
      print "_______________________\n";
      
      opendir(DIR, "$script_dir/dailys");
      my @found_releases =
	  map "$_",
	  sort grep !/^\.\.?$/,
	  readdir DIR;
      closedir DIR;
      
      print "found releases: @found_releases\n";

      # Dig daily, total numbers out of files, send to server.
      foreach $release (@found_releases) 
      {
	  my $daily = 0;
	  my $total = 0;
	  
	  if(-e "$script_dir/dailys/$release") {
	      open LOG, "$script_dir/dailys/$release";
	      while(<LOG>) {
		  chomp;
		  $daily = $_;
		  last;
	      }
	      close LOG;
	  } else {
	      print "ERROR: no dailys file ($script_dir/dailys/$release).\n";
	  }
	  
	  #
	  # Build up total numbers.
	  #
	  
	  my $last_total = 0;
	  if(-e "$script_dir/totals/$release") {
	      open LOG, "$script_dir/totals/$release";
	      while(<LOG>) {
		  chomp;
		  $last_total = $_;
		  last;
	      }
	      close LOG;
	  } 
	  
	  # Compute new total.
	  $total = $last_total + $daily;
	  
	  # Write out new total.
	  open LOG, ">$script_dir/totals/$release";
	  print LOG $total;
	  close LOG;
	  

	  # Send results to server

	  send_results_to_server($daily, "--",
	  			 "download-daily", $release);
	  
	  send_results_to_server($total, "--", 
	  			 "download-cumulative", $release);
	  
      }
      
      
  }
  print "\n";
  
}

