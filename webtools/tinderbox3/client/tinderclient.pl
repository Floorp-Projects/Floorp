#!/usr/bin/perl -w

use strict;

use Getopt::Long;

# XXX do we really need this?
$| = 1;

#
# Catch these signals
#
$SIG{INT} = sub { print "SIGINT\n"; die; };
$SIG{TERM} = sub { print "SIGTERM\n"; die; };

my @original_args = @ARGV;

#
# PROGRAM START
#
# Get arguments
#
our %args;
$args{trust} = 1;
$args{throttle} = 60;
$args{statusinterval} = 15;
GetOptions(\%args, "throttle:i", "url:s",
                   "trust!", "usepatches!", "usecommands!", "usemozconfig!",
                   "upgrade!", "upgrade_url:s",
                   "cvs_co_date:s", "cvsroot:s", "tests:s", "clobber!",
                   "lowbandwidth!", "statusinterval:s",
                   "upload_ssh_loc:s", "upload_ssh_dir:s", "upload_dir:s",
                   "uploaded_url:s",
                   "help|h|?!");
if (!$args{url} || @ARGV != 2 || $args{help}) {
  print <<EOM;

Usage: tinderclient.pl [OPTIONS] [--help] Tree MachineName ...

Runs the build continuously, sending status to the url
Tree: the name of the tree this tinderbox is attached to
MachineName: the name of this machine (how it is identified and will show up on
             tinderbox)

--url: the url of the Tinderbox we will send status to
--throttle: minimum length of time between builds (if it is failing miserably,
            we don't want to continuously send crap to the server or even bother
            building).  Default is 60s.
--notrust: do not trust anything from the server, period--commands, cvs_co_date,
           mozconfig, patches or anything else
--nousecommands: don't obey commands from the server, such as kick or clobber
--noupgrade: do not upgrade tinderclient.pl automatically from server
--upgrade_url: url to upgrade from
--dir: the directory to work in
--lowbandwidth: transfer less verbose info to the server
--help: show this message

[Mozilla-specific options]
--nousemozconfig: do not get .mozconfig from the server
--nousepatches: bring down new patches from the tinderbox and apply them
The following options will be brought down from the server if not specified
here, unless --notrust is specified.  If --notrust is specified, defaults given
will be used instead.
--tests: the list of tests to run.  Defaults to "Tp,Ts,Txul"
--cvsroot: the cvsroot to grab Mozilla and friends from.  Defaults to
           ":pserver:anonymous\@cvs-mirror.mozilla.org:/cvsroot"
--cvs_co_date date to check out at, or blank (current) or "off".  If you do not
             set this, the server will control it.  Defaults to blank (current).
--clobber, --noclobber: clobber or depend build.  Defaults to --noclobber.
--upload_ssh_loc, --upload_ssh_dir: ssh server to upload finished builds to
                                    (jkeiser\@johnserver, public_html/builds)
--upload_dir: directory to copy finished builds to (another way to upload,
              through network drives or if you have a server locally)
--uploaded_url: url where the build can be found once uploaded (\%s will be
                replaced with the build name)

EOM
  exit(1);
}

$args{usecommands} = $args{trust} if !defined($args{usecommands});
$args{usemozconfig} = $args{trust} if !defined($args{usemozconfig});
$args{usepatches} = $args{trust} if !defined($args{usepatches});
$args{upgrade} = $args{trust} if !defined($args{upgrade});
if (!$args{trust}) {
  $args{tests} = "Tp,Ts,Txul" if !defined($args{tests});
  $args{cvsroot} = ':pserver:anonymous@cvs-mirror.mozilla.org:/cvsroot' if !defined($args{cvsroot});
  $args{cvs_co_date} = "" if !defined($args{cvs_co_date});
  $args{clobber} = 0 if !defined($args{clobber});
}

if ($args{dir}) {
  chdir($args{dir});
}

my $client = new TinderClient(\%args, $ARGV[0], $ARGV[1], \@original_args, [ "init_tests", "init_tree", "build" ]);
while (1) {
  $client->build_iteration();
}



package TinderClient;

use strict;

use LWP::UserAgent;
use CGI;
use HTTP::Date qw(time2str);
use Cwd qw(abs_path);

our $VERSION;
our $PROTOCOL_VERSION;

sub new {
	my $class = shift;
	$class = ref($class) || $class;
	my $this = {};
	bless $this, $class;

  $VERSION = "0.1";
  $PROTOCOL_VERSION = "0.1";

  my ($args, $tree, $machine_name, $original_args, $modules) = @_;
  # The arguments hash
  $this->{ARGS} = $args;
  $this->{CONFIG} = { %{$args} };
  # The tree
  $this->{TREE} = $tree;
  # The machine
  $this->{MACHINE_NAME} = $machine_name;
  # The user agent object
  $this->{UA} = new LWP::UserAgent;
  $this->{UA}->agent("TinderboxClient/" . $TinderClient::PROTOCOL_VERSION);
  # the tinderclient.log out
  $this->{LOG_OUT} = undef;
  # the tinderclient.log in
  $this->{LOG_IN} = undef;
  # the un-dealt-with commands
  $this->{COMMANDS} = {};
  # system information
  $this->{SYSINFO} = TinderClient::SysInfo::get_sysinfo();
  # the original program arguments in case we have to upgrade
  $this->{ORIGINAL_ARGS} = $original_args;
  # modules
  $this->{MODULES} = [ @{$modules} ];
  # persistent vars for the build modules
  $this->{PERSISTENT_VARS} = {};

  return $this;
}

sub get_patch {
  my $this = shift;
  my ($patch_id) = @_;
  if (! -f "tbox_patches/$patch_id.patch") {
    my $req = new HTTP::Request(GET => $this->{CONFIG}{url} . "/get_patch.pl?patch_id=$patch_id");
    my $res = $this->{UA}->request($req);
    if ($res->is_success) {
      if (! -d "fixes") {
        mkdir("fixes");
      }
      if (!open OUTFILE, ">tbox_patches/$patch_id.patch") {
        $this->print_log("ERROR: unable to create patchfile: $!\n");
        return "";
      }
      print OUTFILE ${$res->content_ref()};
      close OUTFILE;
    } else {
      $this->print_log("ERROR reaching $this->{CONFIG}{url}/get_patch.pl?patch_id=$patch_id ...\n");
      return "";
    }

  }
  return "tbox_patches/$patch_id.patch";
}

sub form_data_request {
  my $this = shift;
  my ($boundary, $name, $value) = @_;
  my $request_content;
  $request_content = "--" . $boundary . "\r\n";
  $request_content .= "Content-Disposition: form-data; name=\"$name\"\r\n\r\n";
  $request_content .= $value . "\r\n";
  return $request_content;
}

sub send_request {
  my $this = shift;
  my ($script, $params) = @_;

  my $boundary = "----------------------------------" . int(rand()*1000000000000);
  # Create a request
  my $req = new HTTP::Request(POST => $this->{CONFIG}{url} . "/xml/" . $script);
  #my $req = new HTTP::Request(POST => "http://www.mozilla.gr.jp:4321/");
  $req->content_type("multipart/form-data; boundary=$boundary");

  ${$req->content_ref} .= $this->form_data_request($boundary, 'tree', $this->{TREE});
  foreach my $param (keys %{$params}) {
    ${$req->content_ref} .= $this->form_data_request($boundary, $param, $params->{$param});
  }
  if (defined($this->{LOG_IN})) {
    my $started_sending = 0;
    my $log_in = $this->{LOG_IN};
    while (<$log_in>) {
      if (!$started_sending) {
        ${$req->content_ref} .= "--" . $boundary . "\r\n";
        ${$req->content_ref} .= "Content-Disposition: form-data; name=\"log\"; filename=\"log.txt\"\r\n";
        ${$req->content_ref} .= "Content-Type: text/plain\r\n\r\n";
        $started_sending = 1;
      }
      ${$req->content_ref} .= $_;
    }
    if ($started_sending) {
      ${$req->content_ref} .= "\r\n";
    }
  }
  ${$req->content_ref} .= "--" . $boundary . "--\r\n";
  print "----- REQUEST TO $this->{CONFIG}{url}/xml/$script -----\n";
  #print $req->content();
  #print "----- END REQUEST TO $this->{CONFIG}{url}/xml/$script -----\n";

  # Pass request to the user agent and get a response back
  return $this->{UA}->request($req);
}

sub parse_simple_tag {
  my $this = shift;
  my ($content_ref, $tagname) = @_;
  if (${$content_ref} =~ /<$tagname[^>]*>([^<]*)/) {
    return $1;
  }
  return "";
}

sub get_field {
  my $this = shift;
  my ($content_ref, $field) = @_;
  if (!exists($this->{ARGS}{$field})) {
    $this->{CONFIG}{$field} = $this->parse_simple_tag($content_ref, $field);
  }
}

sub parse_content {
  my $this = shift;
  my ($content_ref, $is_start) = @_;
  if ($this->{CONFIG}{usecommands}) {
    foreach (split(/,/, $this->parse_simple_tag($content_ref, "commands"))) {
      $this->print_log("---> New command $_! <---\n");
      $this->{COMMANDS}{$_} = 1;
    }
  }

  if ($is_start) {
    if (${$content_ref} =~ /<machine[^>]+\bid=['"](\d+)['"]>/) {
      $this->{MACHINE_ID} = $1;
    } else {
      return 0;
    }
    $this->get_field($content_ref, "upgrade_url");
    foreach my $module (@{$this->{MODULES}}) {
      $this->call_module($module, "get_config", $content_ref);
    }
  }

  return 1;
}

sub sysinfo {
  my $this = shift;
  return $this->{SYSINFO};
}

sub field_vars_hash {
  my $this = shift;
  my $retval = {};
  my $i = 0;
  while (my ($field, $field_val) = each %{$this->{BUILD_VARS}{fields}}) {
    if (ref($field_val) eq "ARRAY") {
      foreach my $val (@{$field_val}) {
        $retval->{"field_${i}"} = $field;
        $retval->{"field_${i}_val"} = $val;
        $i++;
      }
    } else {
      $retval->{"field_${i}"} = $field;
      $retval->{"field_${i}_val"} = $field_val;
      $i++;
    }
  }
  return %{$retval};
}

sub build_start {
  my $this = shift;

  # Check the outcome of the response
  my $res = $this->send_request("build_start.pl", { machine_name => $this->{MACHINE_NAME}, os => $this->{SYSINFO}{OS}, os_version => $this->{SYSINFO}{OS_VERSION}, compiler => $this->{SYSINFO}{COMPILER}, clobber => ($this->{CONFIG}{clobber} ? 1 : 0), $this->field_vars_hash() });
  my $success = $res->is_success || $res->content() !~ /<error>/;
  if ($success) {
    $this->{LAST_STATUS_SEND} = time;
    print "\nCONTENT: " . $res->content() . "\n";
    return $this->parse_content($res->content_ref(), 1);
  }
  return 0;
}

sub build_status {
  my $this = shift;
  my ($status) = @_;

  # Check the outcome of the response
  my $res = $this->send_request("build_status.pl", { machine_id => $this->{MACHINE_ID}, status => $status, $this->field_vars_hash() });
  my $success = $res->is_success || $res->content() !~ /<error>/;
  if ($success) {
    $this->{LAST_STATUS_SEND} = time;
    print "build_status success\n";
    print "\nCONTENT: " . $res->content() . "\n";
    return $this->parse_content($res->content_ref(), 0);
  }
  return 0;
}

sub build_finish {
  my $this = shift;
  my ($status) = @_;
  print "build_finish($status)\n";
  close $this->{LOG_OUT};
  my $retval = $this->build_status($status);
  close $this->{LOG_IN};
  return $retval;
}

sub print_log {
  my $this = shift;
  my ($line) = @_;
  print $line;
  my $log_out = $this->{LOG_OUT};
  print $log_out $line;
}

sub start_section {
  my $this = shift;
  my ($section) = @_;
  $this->print_log("---> TINDERBOX $section\n");
}

sub end_section {
  my $this = shift;
  my ($section) = @_;
  $this->print_log("<--- TINDERBOX FINISHED $section\n");
}

# NOTE: added modules won't get their configuration called if they are added
# after the config cycle
sub add_module {
  my $this = shift;
  my ($module) = @_;
  push @{$this->{MODULES}}, $module;
}

sub remove_module {
  my $this = shift;
  my ($module) = @_;
  for (my $i=0; $i < @{$this->{MODULES}}; $i++) {
    if ($this->{MODULES}[$i] eq $module) {
      $this->call_module($module, "finish_build");
      splice @{$this->{MODULES}}, $i, 1;
      return 1;
    }
  }
  return 0;
}

sub eat_command {
  my $this = shift;
  my ($command) = @_;
  if ($this->{COMMANDS}{$command}) {
    $this->print_log("---> Eating command $command! <---\n");
    delete $this->{COMMANDS}{$command};
    return 1;
  }
  return 0;
}

sub do_command {
  my $this = shift;
  my ($command, $status, $grep_sub) = @_;

  $this->start_section("RUNNING '$command'");

  my $please_send_status = 0;

  my $pid = open BUILD, "$command 2>&1|";
  if (!$pid) {
    die "Could not start build: $!";
  }
  # The time we first tried to kill the process
  while (<BUILD>) {
    $this->print_log($grep_sub ? &$grep_sub($_) : $_);

    {
      # Send status and check whether we need to kill every 3 minutes
      my $current_time = time;
      my $elapsed = $current_time - $this->{LAST_STATUS_SEND};
      if ($elapsed > $this->{CONFIG}{statusinterval}) {
        my $log_out = $this->{LOG_OUT};
        flush $log_out;
        my $success = $this->build_status(1);
        if ($success) {
          if ($this->{COMMANDS}{kick}) {
            print "\nKilling $pid\n\n";
            kill('INT', $pid);
            $please_send_status = 301;
            delete $this->{COMMANDS}{kick};
          }
        }
      }
    }

  }
  close BUILD;

  my $build_error = $?;

  if ($build_error) {
    $this->end_section("(FAILURE: $build_error) RUNNING '$command'");
  } else {
    $this->end_section("(SUCCESS) RUNNING '$command'");
  }

  if ($please_send_status) {
    return $please_send_status;
  } else {
    return $build_error ? 200 : 0;
  }
}

sub read_mozconfig {
  my $this = shift;
  my $mozconfig = "";
  if ($ENV{MOZCONFIG}) {
    if (open MOZCONFIG, $ENV{MOZCONFIG}) {
      while (<MOZCONFIG>) {
        $mozconfig .= $_;
      }
    }
    close MOZCONFIG;
  } elsif (open MOZCONFIG, ".mozconfig") {
    while (<MOZCONFIG>) {
      $mozconfig .= $_;
    }
    close MOZCONFIG;
  } elsif (open MOZCONFIG, "~/.mozconfig") {
    while (<MOZCONFIG>) {
      $mozconfig .= $_;
    }
    close MOZCONFIG;
  }
  return $mozconfig;
}

sub print_build_info {
  my $this = shift;
  $this->start_section("PRINTING CONFIGURATION");
  $this->print_log(<<EOM);
== Tinderbox Info
Time: @{[time2str(time)]}
OS: $this->{SYSINFO}{OS}
$this->{SYSINFO}{OS_VERSION}
Compiler: $this->{SYSINFO}{COMPILER} $this->{SYSINFO}{COMPILER_VERSION}
Tinderbox Client: $TinderClient::VERSION
Tinderbox Client Last Modified: @{[$this->get_prog_mtime()]}
Tinderbox Protocol: $TinderClient::PROTOCOL_VERSION
Arguments: @{[join ' ', @{$this->{ORIGINAL_ARGS}}]}
URL: $this->{CONFIG}{url}
Tree: $this->{TREE}
Commands: @{[join(' ', sort keys %{$this->{COMMANDS}})]}
Modules: @{[join(' ', @{$this->{MODULES}})]}
Config:
@{[join("\n", map { $_ . " = '" . $this->{CONFIG}{$_} . "'" } sort keys %{$this->{CONFIG}})]}
== End Tinderbox Client Info
EOM
  $this->do_command("env", 1);
  $this->end_section("PRINTING CONFIGURATION");
}


sub maybe_throttle {
  my $this = shift;
  my $elapsed = time - $this->{BUILD_VARS}{START_TIME};
  if ($elapsed < $this->{CONFIG}{throttle}) {
    print "Throttling!  Sleeping " . ($this->{CONFIG}{throttle} - $elapsed) . "s\n";
    sleep($this->{CONFIG}{throttle} - $elapsed);
  }
}

sub get_prog_mtime {
  my @prog_stat = stat($0);
  my $prog_mtime = $prog_stat[9];
  my $time_str = time2str($prog_mtime);
}

sub maybe_upgrade {
  my $this = shift;
  if ($this->{CONFIG}{upgrade} && $this->{CONFIG}{upgrade_url}) {
    my $time_str = $this->get_prog_mtime();
    $this->start_section("CHECKING FOR UPGRADE");
    $this->print_log("URL: $this->{CONFIG}{upgrade_url}\n");
    $this->print_log("If-Modified-Since: $time_str\n");
    my $req = new HTTP::Request(GET => $this->{CONFIG}{upgrade_url});
    $req->header('If-Modified-Since' => $time_str);
    my $res = $this->{UA}->request($req);
    if ($res->code() == 200) {
      open PROG, $0;
      open PROG_BAK, ">$0.bak";
      while (<PROG>) { print PROG_BAK; }
      close PROG;
      close PROG_BAK;
      open PROG, ">$0";
      print PROG $res->content();
      close PROG;
      $this->print_log("New version found:\n");
      $this->print_log($res->content());
      $this->end_section("CHECKING FOR UPGRADE");
      $this->print_log("Executing newly upgraded script ...\n");
      print "UPGRADING!  Throttling just for fun first ...\n";
      $this->build_finish(303);
      eval {
        # Throttle just in case we get in an upgrade client loop
        $this->maybe_throttle();
        exec("perl", $0, @{$this->{ORIGINAL_ARGS}});
      };
      exit(0);
    } elsif ($res->code() == 304) {
      $this->print_log("Perl script not modified\n");
    } else {
      $this->print_log("Connection or URL failure (" . $res->code() . ")\n");
    }
    $this->end_section("CHECKING FOR UPGRADE");
  }
}

sub call_module {
  my $this = shift;
  my ($module, $method, $content_ref) = @_;
  my $code = "TinderClient::Modules::${module}::${method}(\$this, \$this->{CONFIG}, \$this->{PERSISTENT_VARS}, \$this->{BUILD_VARS}, \$content_ref)";
  my $retval = eval $code;
  # Handle ctrl+c
  if ($@) {
    die;
  }
  return $retval;
}

sub build_iteration {
  my $this = shift;

  # Open the log
  open $this->{LOG_OUT}, ">tinderclient.log" or die "Could not output to tinder log";
  open $this->{LOG_IN}, "tinderclient.log" or die "Could not read tinder log";

  # Initialize transient variables
  $this->{BUILD_VARS} = { fields => {} };

  $this->{BUILD_VARS}{START_TIME} = time;

  #
  # Send build start notification
  #
  if (!$this->build_start()) {
    $this->maybe_throttle();
    return;
  }

  my $err = 0;
  eval {
    $this->maybe_upgrade();
    $this->print_build_info();

    # Build
    foreach my $module (@{$this->{MODULES}}) {
      $err = $this->call_module($module, "do_action");
      last if $err;
    }

    # Call cleanup
    foreach my $module (@{$this->{MODULES}}) {
      $this->call_module($module, "finish_build");
    }
  };

  # Handle ctrl+c
  if ($@) {
    $this->print_log("ERROR: $@\n");
    $this->build_finish(302);
    die;
  }

  # Send build finish notification
  $this->build_finish($err || 100);

  # Determine if we need to throttle
  $this->maybe_throttle();

  chdir(".."); #
}


package TinderClient::SysInfo;

use strict;

#
# Get the sysinfo object for this system
#
sub get_sysinfo {
  #
  # Decide which OS SysInfo instance to create
  # (largely copied from the old tinderbox client)
  #
  my $os = `uname -s`;
  my $os_ver = `uname -r`;
  my $os_alt_ver = `uname -v`;
  my $cpu = `uname -m`;
  chomp($os, $os_ver, $os_alt_ver, $cpu);

  #
  # Handle aliases and weird version numbers
  #
  my %os_aliases = (
    'BSD_OS' => 'BSD/OS',
    'IRIX64' => 'IRIX',
  );
  if ($os_aliases{$os}) {
    $os = $os_aliases{$os};
  }
  if ($os eq 'SCO_SV') {
    $os = 'SCOOS';
    $os_ver = '5.0';
  } elsif ($os eq 'QNX') {
    $os_ver = $os_alt_ver;
    $os_ver =~ s/^([0-9])([0-9]*)$/$1.$2/;
  } elsif ($os eq 'AIX') {
    $os_ver = "$os_alt_ver.$os_ver";
  } elsif ($os =~ /^CYGWIN_([^-]*)-(.*)$/) {
    $os = "WIN$1";
    $os_ver = $2;
  } elsif ($os eq "SunOS" && $cpu ne 'i86pc' && substr($os_ver, 0, 1) ne '4') {
    $cpu = 'sparc';
  }

  return new TinderClient::SysInfo($os, $os_ver, $cpu);
}

#
# Sets up the system info object; for os's to override
#
sub new {
	my $class = shift;
	$class = ref($class) || $class;
	my $this = {};
	bless $this, $class;

  my ($os, $os_ver, $cpu) = @_;
  $this->{OS} = $os;
  $this->{OS_VERSION} = $os_ver;
  $this->{CPU} = $cpu;

  if ($os =~ /^WIN/) {
    $this->{COMPILER} = 'cl';
  } else {
    $this->{COMPILER} = 'gcc';
  }
  if ($this->{COMPILER} eq 'cl') {
    $this->{COMPILER_VERSION} = `cl`;
  } elsif ($this->{COMPILER} eq 'gcc') {
    $this->{COMPILER_VERSION} = `gcc --version`;
  }
  # XXX figure out version for Windows compiler

  return $this;
}


package TinderClient::Modules::init_tests;

use strict;

# We add these modules so that they will be able to parse their own config
sub get_config {
  my ($client, $config, $persistent_vars, $build_vars, $content_ref) = @_;
  $client->get_field($content_ref, "tests");
  foreach my $module (split(/,/, $config->{tests})) {
    $client->add_module($module);
  }
}

sub finish_build {
  my ($client, $config, $persistent_vars, $build_vars) = @_;
  foreach my $module (split(/,/, $config->{tests})) {
    $client->remove_module($module);
  }
}

sub do_action {
  my ($client, $config, $persistent_vars, $build_vars) = @_;
  return 0;
}


package TinderClient::Modules::init_tree;

use strict;

use Cwd;

sub get_config {
  my ($client, $config, $persistent_vars, $build_vars, $content_ref) = @_;

  if ($config->{usemozconfig}) {
    if (${$content_ref} =~ /<mozconfig[^>]*>(.*)<\/mozconfig>/sm) {
      $build_vars->{MOZCONFIG} = $1;
    }
  }
  $client->get_field($content_ref, "cvs_co_date");
  $client->get_field($content_ref, "cvsroot");
  $client->get_field($content_ref, "clobber");
  if ($config->{usepatches}) {
    $build_vars->{PATCHES} = [];
    while (${$content_ref} =~ /<patch[^>]+id\s*=\s*['"](\d+)['"]/g) {
      push @{$build_vars->{PATCHES}}, $1;
    }
  }

  $persistent_vars->{LAST_CHECKOUT} = 0 if !exists($persistent_vars->{LAST_CHECKOUT});
}

sub finish_build {
  my ($client, $config, $persistent_vars, $build_vars) = @_;
}

sub do_action {
  my ($client, $config, $persistent_vars, $build_vars) = @_;
  my $init_tree_status = 1;

  #
  # We will only build if:
  # - new patches were downloaded
  # - checkout brought something down
  # - the build command was specified
  #
  $build_vars->{SHOULD_BUILD} = 0;

  #
  # Checkout client.mk if necessary
  #
  my $please_checkout = 0;
  if (! -f "mozilla/client.mk") {
    $client->do_command("cvs -d$config->{cvsroot} co mozilla/client.mk", $init_tree_status);
    $please_checkout = 1;
  }
  if (-f "mozilla/client.mk") {
    chdir("mozilla");
  } else {
    die "Must be just above the mozilla/ directory!";
  }

  #
  # Create .mozconfig if necessary
  #
  # First read .mozconfig
  my $please_clobber = 0;
  my $mozconfig = $client->read_mozconfig();
  print "@@@ mozconfig:\n$mozconfig\n@@@ network .mozconfig:\n$build_vars->{MOZCONFIG}\n";
  if ($build_vars->{MOZCONFIG}) {
    if ($mozconfig ne $build_vars->{MOZCONFIG}) {
      delete $ENV{MOZCONFIG};
      $client->start_section("CREATING MOZCONFIG");
      open MOZCONFIG, ">.mozconfig";
      print MOZCONFIG $build_vars->{MOZCONFIG};
      close MOZCONFIG;
      $mozconfig = $_;
      $client->print_log("(Will clobber this cycle)");
      $client->end_section("CREATING MOZCONFIG");
      $please_clobber = 1;
    }
  }

  #
  # Print build info
  #
  $client->start_section("PRINTING BUILD INFO");
  $client->print_log(<<EOM);
== .mozconfig
$mozconfig
== End .mozconfig
EOM
  $client->start_section("PRINTING BUILD INFO");

  #
  # Remove patches
  #
  my @old_patches;
  foreach my $patch (glob("tbox_patches/*.patch")) {
    $client->do_command("patch -Nt -Rp0 < $patch", $init_tree_status);
    if ($patch =~ /^tbox_patches\/(.+)\.patch$/) {
      push @old_patches, $1;
    }
  }

  #
  # Make build official
  #
  $client->start_section("SETTING MOZILLA_OFFICIAL=1,BUILD_OFFICIAL=1");
  $ENV{MOZILLA_OFFICIAL}=1;
  $ENV{BUILD_OFFICIAL}=1;
  $client->end_section("SETTING MOZILLA_OFFICIAL=1,BUILD_OFFICIAL=1");

  #
  # Set MOZ_OBJDIR
  #
  if (! -d "objdir") {
    $client->start_section("MAKING OBJDIR");
    if (!mkdir("objdir")) {
      $client->print_log("Unable to make objdir.");
      $client->end_section("MAKING OBJDIR");
      return 200;
    }
    $client->end_section("MAKING OBJDIR");
  }
  $client->start_section("SETTING OBJDIR");
  $ENV{MOZ_OBJDIR} = getcwd() . "/objdir";
  $client->print_log("Set to $ENV{MOZ_OBJDIR}\n");
  $client->end_section("SETTING MOZ_OBJDIR");

  #
  # Clean non-objdir stuff out
  #
  # XXX only the rm -rf is necessary now, this is temporary while I clean out
  # my personal trees
  if (-f "Makefile") {
    $client->do_command("make distclean", $init_tree_status+2);
  }

  #
  # Checkout
  #
  # - If cvs co date is off, do nothing
  # - If cvs co date exists, and is the same as last time, we already checked
  #   out, so do nothing
  # - If it is blank, we check out every time
  # - If we are asked to do "checkout" or this is a new build, we check out
  #   regardless (and we *always* do a full checkout, not fast-update).
  my $err = 0;
  if ($client->eat_command("checkout")) {
    $please_checkout = 1;
  }
  if ($please_checkout ||
      ($config->{cvs_co_date} ne "off" &&
      !($config->{cvs_co_date} &&
        $config->{cvs_co_date} eq $persistent_vars->{LAST_CVS_CO_DATE}))) {
    if ($config->{cvs_co_date}) {
      # XXX $::ENV?
      $ENV{MOZ_CO_DATE} = $config->{cvs_co_date};
    }
    my $parsing_code = sub {
                         if ($_[0] =~ /^[UP] /) {
                           $build_vars->{SHOULD_BUILD} = 1;
                         }
                         if ($config->{lowbandwidth} && $_[0] =~ /\? /) {
                           return "";
                         }
                         return $_[0];
                       };

    #
    # We only want to do a full, slow make -f client.mk checkout if:
    # - this is the first time this program has been called
    # - 24 hours have passed since the last checkout was done
    # - we were given a "checkout" command or this is the first checkout
    # - there is a cvs_co_date
    #
    # All other times we do fast-update.
    #
    if ($config->{cvs_co_date} || $please_checkout ||
        (time - $persistent_vars->{LAST_CHECKOUT}) >= (24*60*60)) {
      $err = $client->do_command("make -f client.mk checkout", $init_tree_status+1, $parsing_code);
      $persistent_vars->{LAST_CHECKOUT} = time;
    } else {
      $err = $client->do_command("make -f client.mk fast-update", $init_tree_status+1, $parsing_code);
    }
    if ($err) {
      $persistent_vars->{LAST_CHECKOUT} = 0;
    }

    $persistent_vars->{LAST_CVS_CO_DATE} = $config->{cvs_co_date};
  }

  #
  # Apply patches
  #
  if (!$err && @{$build_vars->{PATCHES}}) {
    #
    # If the set of patches is different, we need to rebuild
    #
    if (join(' ', sort @old_patches) ne join(' ', sort @{$build_vars->{PATCHES}})) {
      $build_vars->{SHOULD_BUILD} = 1;
    }
    $client->start_section("APPLYING PATCHES");
    foreach my $patch_id (@{$build_vars->{PATCHES}}) {
      my $patch = $client->get_patch($patch_id);
      $client->print_log("PATCH: $patch\n");
      if (! $patch) {
        $err = 200;
      } else {
        my $local_err = $client->do_command("patch --dry-run -Nt -p0 < $patch", $init_tree_status+2);
        if (!$local_err) {
          $local_err = $client->do_command("patch -Nt -p0 < $patch", $init_tree_status);
        }
        if ($local_err) {
          unlink($patch);
        }
      }
    }
    $client->end_section("APPLYING PATCHES");
  }

  #
  # Clobber
  #
  # We clobber:
  # - when we need to build and we are a clobber build
  # - when there is a clobber command in the queue or the mozconfig changed
  #
  if (!$err && (($build_vars->{SHOULD_BUILD} && $config->{clobber}) ||
      $client->eat_command("clobber") || $please_clobber)) {
    $client->do_command("rm -rf objdir", $init_tree_status+2);
    $build_vars->{SHOULD_BUILD} = 1;
  }

  #
  # If the build command is specified, we build no matter what
  #
  if ($client->eat_command("build")) {
    $build_vars->{SHOULD_BUILD} = 1;
  }

  return $err;
}


package TinderClient::Modules::build;

use strict;

sub get_config {
  my ($client, $config, $persistent_vars, $build_vars, $content_ref) = @_;
  $client->get_field($content_ref, "upload_ssh_loc");
  $client->get_field($content_ref, "upload_ssh_dir");
  $client->get_field($content_ref, "upload_dir");
  $client->get_field($content_ref, "uploaded_url");
}

sub finish_build {
  my ($client, $config, $persistent_vars, $build_vars) = @_;
  delete $ENV{MOZILLA_OFFICIAL};
  delete $ENV{BUILD_OFFICIAL};
  delete $ENV{MOZ_CO_DATE};
  delete $ENV{MOZ_OBJDIR};
}

sub do_action {
  my ($client, $config, $persistent_vars, $build_vars) = @_;

  #
  # Build
  #
  my $err = 0;
  if ($build_vars->{SHOULD_BUILD}) {
    $err = $client->do_command("make -f client.mk build", 10,
                               $config->{lowbandwidth} ?
        sub {
          if ($_[0] =~ s/^g?make.+Entering directory ['`"](.+)['`"]$/$1/) {
            return $_[0];
          }
          if ($_[0] =~ /^\S+$/) {
            return $_[0];
          }
          return "";
        } :
        undef);

    #
    # Build distribution
    #
    if (!$err) {
      $client->do_command("make -C objdir/xpinstall/packager", 11);
    #  # We always do the installer if we can, to ensure that it compiles
    #  if ($client->sysinfo()->{OS} =~ /^WIN/) {
    #    if (chdir("xpinstall/wizard/windows/builder")) {
    #      $err = $client->do_command("perl build.pl", 12);
    #      chdir("../../../..");
    #    }
    #  }# elsif ($client->sysinfo()->{OS} =~ /Linux/) {
    #  #  if (chdir("xpinstall/packager/unix")) {
    #  #    $client->do_command("perl deliver.pl", 12);
    #  #    chdir("../../..");
    #  #  }
    #  #}
    }

    #
    # Upload installer or distribution
    #
    if (!$err) {
      # Get build id
      open BUILD_NUMBER, "objdir/config/build_number";
      my $build_id = <BUILD_NUMBER>;
      chomp $build_id;
      close BUILD_NUMBER;

      # Find zipped build
      my ($local_file) = glob("objdir/dist/mozilla*.tgz");
      if (!$local_file) {
        ($local_file) = glob("objdir/dist/mozilla*.tar.gz");
      }
      if (!$local_file) {
        ($local_file) = glob("objdir/dist/mozilla*.zip");
      }

      # Upload
      if ($local_file) {
        $local_file =~ /([^\/]*)$/;
        my $upload_file = $1;
        $upload_file =~ s/(\..*)$/-$build_id$1/;
        upload_build($config, $build_vars, $local_file, $upload_file);
      }
    }
  } else {
    $client->print_log("Skipping build because no changes were made\n");
  }
  return $err;
}

sub upload_build {
  my ($config, $build_vars, $local_name, $upload_name) = @_;
  if ($config->{upload_ssh_loc} && `which scp`) {
    $config->{upload_ssh_dir} .= "/" if $config->{upload_ssh_dir} && $config->{upload_ssh_dir} !~ /\/$/;
    $client->do_command("scp $local_name $config->{upload_ssh_loc}:$config->{upload_ssh_dir}$upload_name");
    set_upload_dir($config, "build_zip", $build_vars, $upload_name);
  }
  if ($config->{upload_dir}) {
    $config->{upload_dir} .= "/" if $config->{upload_dir} && $config->{upload_dir} !~ /\/$/;
    $client->do_command("cp $local_name $config->{upload_dir}$upload_name");
    set_upload_dir($config, "build_zip", $build_vars, $upload_name);
  }
}

sub set_upload_dir {
  my ($config, $field_name, $build_vars, $upload_name) = @_;
  if ($config->{uploaded_url}) {
    my $url = $config->{uploaded_url};
    $url =~ s/\%s/$upload_name/g;
    if (!$build_vars->{fields}{$field_name}) {
      $build_vars->{fields}{$field_name} = [];
    }
    push @{$build_vars->{fields}{$field_name}}, $url;
  }
}


package TinderClient::Modules::Tp;

use strict;

sub get_config {
  my ($client, $config, $persistent_vars, $build_vars, $content_ref) = @_;
}

sub finish_build {
  my ($client, $config, $persistent_vars, $build_vars) = @_;
}

sub do_action {
  my ($client, $config, $persistent_vars, $build_vars) = @_;
  return 0;
}


package TinderClient::Modules::Txul;

use strict;

sub get_config {
  my ($client, $config, $persistent_vars, $build_vars, $content_ref) = @_;
}

sub finish_build {
  my ($client, $config, $persistent_vars, $build_vars) = @_;
}

sub do_action {
  my ($client, $config, $persistent_vars, $build_vars) = @_;
  return 0;
}


package TinderClient::Modules::Ts;

use strict;

sub get_config {
  my ($client, $config, $persistent_vars, $build_vars, $content_ref) = @_;
}

sub finish_build {
  my ($client, $config, $persistent_vars, $build_vars) = @_;
}

sub do_action {
  my ($client, $config, $persistent_vars, $build_vars) = @_;
  return 0;
}
