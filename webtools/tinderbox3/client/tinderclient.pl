#!/usr/bin/perl -w
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

use Getopt::Long;

# XXX do we really need this?
$| = 1;

# Save original arguments so we can send them to the new script if we upgrade
# ourselves
my @original_args = @ARGV;

#
# Catch these signals
#
$SIG{INT} = sub { print "SIGINT\n"; die; };
$SIG{TERM} = sub { print "SIGTERM\n"; die; };

#
# PROGRAM START
#
# Get arguments
#
my @clients;
create_clients(\@clients, \@original_args);

while (1) {
  foreach my $client (@clients) {
    $client->build_iteration();
  }
}


sub create_clients {
  my ($clients, $original_args) = @_;

      print "EEK @ARGV\n";
  my %args;
  $args{trust} = 1;
  $args{throttle} = 5*60;
  $args{statusinterval} = 15;
  GetOptions(\%args, "config:s", "dir:s",
                     "throttle:i", "url:s",
                     "trust!", "usepatches!", "usecommands!", "usemozconfig!",
                     "upgrade!", "upgrade_url:s",
                     "branch:s", "cvs_co_date:s", "cvsroot:s", "tests:s",
                     "clobber!", "lowbandwidth!", "statusinterval:s",
                     "upload_ssh_loc:s", "upload_ssh_dir:s", "upload_dir:s",
                     "uploaded_url:s", "distribute:s",
                     "use_fast_update!",
                     "help|h|?!");
  if ($args{config}) {
    # Go through each line, parse the arguments into @ARGV, and re-call this
    # function to interpret the args
    open CONFIG, $args{config} or die "Could not find config file $args{config}\n";
    while (<CONFIG>) {
      chomp;
      @ARGV = ();
      open PARSED_PARAMS, 'perl -e \'foreach (@ARGV) { print "$_\n"; }\' - ' . $_ . "|" or die "Could not parse arguments: $!";
      # throw away the -
      readline(*PARSED_PARAMS);
      while (my $param = readline(*PARSED_PARAMS)) {
        chomp $param;
        push @ARGV, $param;
      }
      close PARSED_PARAMS;
      create_clients($clients, $original_args);
    }
    close CONFIG;
    # config is mutually exclusive with other args
    return;
  }

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
--nousepatches: do not bring down new patches from the tinderbox and apply them

The following options will be brought down from the server if not specified
here, unless --notrust is specified.  If --notrust is specified, defaults given
will be used instead.
--tests: the list of tests to run.  Defaults to "Tp,Ts,Txul"
--cvsroot: the cvsroot to grab Mozilla and friends from.  Defaults to
           ":pserver:anonymous\@cvs-mirror.mozilla.org:/cvsroot"
--branch the branch to check out
--cvs_co_date date to check out at, or blank (current) or "off".  If you do not
             set this, the server will control it.  Defaults to blank (current).
--clobber, --noclobber: clobber or depend build.  Defaults to --noclobber.
--upload_ssh_loc, --upload_ssh_dir: ssh server to upload finished builds to
                                    (jkeiser\@johnserver, public_html/builds)
--upload_dir: directory to copy finished builds to (another way to upload,
              through network drives or if you have a server locally)
--uploaded_url: url where the build can be found once uploaded (\%s will be
                replaced with the build name)
--distribute: the list of things to distribute.  Defaults to "build_zip".
              "raw_zip" is another useful one, that just zips up everything in
              the dist/bin directory (actually makes a .tgz).
--raw_zip_name: the project name of the raw build (defaults to "mozilla")
--use_fast_update: whether or not to use fast-update

CONFIG MODE (SWITCHING TINDERBOX):
  tinderclient.pl --config=<file>

Specifies a text file where tbox configuration is stored.  Each line in the
file is nothing more than the arguments to the program.  If you specify multiple
lines (and thus multiple sets of arguments), the client will *switch* between
different builds and trees: i.e. it will build with the first line, then the
second, then the third, then back to the first, and so on.  It is HIGHLY
RECOMMENDED that you specify --dir for each tree, or else the tbox is likely
going to clobber your tree between each build (if the options for different
trees are different).

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
    $args{branch} = "" if !defined($args{branch});
    $args{clobber} = 0 if !defined($args{clobber});
    $args{distribute} = "build_zip" if !defined($args{distribute});
    $args{raw_zip_name} = "mozilla" if !defined($args{distribute});
    $args{use_fast_update} = 1 if !defined($args{use_fast_update});
  }

  if ($args{dir} && !-d $args{dir}) {
    die "Directory $args{dir} does not exist!";
  }

  my $current_args = [ @_ ];
  push @{$clients}, new TinderClient(\%args, $ARGV[0], $ARGV[1], $original_args, $current_args);
}



package TinderClient;

use strict;

use LWP::UserAgent;
use CGI;
use HTTP::Date qw(time2str);
use Cwd qw(abs_path getcwd);

our $VERSION;
our $PROTOCOL_VERSION;

sub new {
	my $class = shift;
	$class = ref($class) || $class;
	my $this = {};
	bless $this, $class;

  $VERSION = "0.1";
  $PROTOCOL_VERSION = "0.1";

  my ($args, $tree, $machine_name, $original_args, $current_args) = @_;
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
  # the original program arguments in case we have to upgrade
  $this->{CURRENT_ARGS} = $current_args;
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
      if (! -d "tbox_patches") {
        mkdir("tbox_patches");
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

    # Call cleanup
    foreach my $module ("init_tree", "build", "distribute", "tests") {
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
    $this->{BUILD_VARS}{fields} = {};
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
    $this->{BUILD_VARS}{fields} = {};
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
  $this->print_log("---> TINDERBOX $section " . time2str(time) . "\n");
}

sub end_section {
  my $this = shift;
  my ($section) = @_;
  $this->print_log("<--- TINDERBOX FINISHED $section " . time2str(time) . "\n");
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

use Fcntl;
use POSIX qw(:errno_h);

sub set_nonblocking {
  my $this = shift;
  my ($handle) = @_;
  my $flags = 0;
  fcntl($handle, F_GETFL, $flags) or return;
  $flags |= O_NONBLOCK;
  fcntl($handle, F_SETFL, $flags) or return;
}

sub _kill_command {
  # Kill a command and its children
  my $this = shift;
  my ($pid, $children_of) = @_;
  $this->print_log("Killing $pid\n");
  kill('INT', $pid);
  foreach my $child_pid (@{$children_of->{$pid}}) {
    $this->_kill_command($child_pid, $children_of);
  }
}

sub kill_command {
  # Kill a command and its children (children first)
  my $this = shift;
  my ($pid) = @_;
  # Get the ps -aux table and pass it to _kill_command
  my %children_of;
  open PS_AUX, $this->sysinfo()->{PS_COMMAND} . "|";
  while (<PS_AUX>) {
    print;
    if (/\s*(\d+)\s*(\d+)/) {
      if (!exists($children_of{$2})) {
        $children_of{$2} = [];
      }
      push @{$children_of{$2}}, $1;
    }
  }
  close PS_AUX;
  $this->_kill_command($pid, \%children_of);
}

sub do_command {
  my $this = shift;
  my ($command, $status, $grep_sub, $max_idle_time) = @_;

  $this->start_section("RUNNING '$command'");

  my $please_send_status = 0;

  my $handle;
  my $pid = open $handle, "$command 2>&1|";
  if (!$pid) {
    $this->end_section("(FAILURE: could not start) RUNNING '$command'");
    return 200;
  }
  $this->set_nonblocking($handle);
  my $last_read_time = time;
  my $build_error;
  while (1) {
    #
    # Read from the buffer asynchronously
    #
    my $buffer;
    my $rv = sysread($handle, $buffer, 1024);
    # If nothing was read, we check if the process is OK
    if (!$rv) {
      #
      # Check if the process is dead
      #
      my $wait_pid = waitpid($pid, POSIX::WNOHANG());
      if (($wait_pid == $pid && POSIX::WIFEXITED($?)) || $wait_pid == -1) {
        $build_error = $?;
        last;
      }
      # Kill the process if it's still alive and hung
      if ($max_idle_time && (time - $last_read_time) > $max_idle_time) {
        $this->print_log("Command appears to have hanged!");
        $build_error = 1;
        $this->kill_command($pid);
        $please_send_status = 202;
      }
    }
    $last_read_time = time;

    if ($buffer) {
      $this->print_log($grep_sub ? &$grep_sub($buffer) : $buffer);
    }

    {
      # Send status every so often (this also gives us back new commands)
      my $current_time = time;
      my $elapsed = $current_time - $this->{LAST_STATUS_SEND};
      if ($elapsed > $this->{CONFIG}{statusinterval}) {
        my $log_out = $this->{LOG_OUT};
        flush $log_out;
        my $success = $this->build_status(1);
      }
      # If we tried to kill before and we're not dead, or if the kick command
      # is around, we kill again
      if ($please_send_status == 301 || $this->eat_command("kick")) {
        $this->kill_command($pid);
        $please_send_status = 301;
      }
    }
    # If nothing was actually read, we sleep to give the cpu a chance
    if (!$rv) {
      sleep(3);
      next;
    }
  }
  close $handle;

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
Arguments: @{[join ' ', @{$this->{CURRENT_ARGS}}]}
URL: $this->{CONFIG}{url}
Tree: $this->{TREE}
Commands: @{[join(' ', sort keys %{$this->{COMMANDS}})]}
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
  my $current_dir = getcwd();
  $this->print_log("---> cd $this->{BUILD_VARS}{olddir} <---\n");
  chdir($this->{BUILD_VARS}{olddir});
  if ($this->{CONFIG}{upgrade} && $this->{CONFIG}{upgrade_url}) {
    my $time_str = $this->get_prog_mtime();
    $this->start_section("CHECKING FOR UPGRADE");
    $this->print_log("URL: $this->{CONFIG}{upgrade_url}\n");
    $this->print_log("If-Modified-Since: $time_str\n");
    my $req = new HTTP::Request(GET => $this->{CONFIG}{upgrade_url});
    $req->header('If-Modified-Since' => $time_str);
    my $res = $this->{UA}->request($req);
    if ($res->code() == 200) {
      my $old_script = "";
      open PROG, $0;
      open PROG_BAK, ">$0.bak";
      while (<PROG>) { $old_script .= $_; print PROG_BAK; }
      close PROG;
      close PROG_BAK;
      open PROG, ">$0";
      print PROG $res->content();
      close PROG;
      $this->print_log("New version found:\n");
      $this->print_log($res->content());
      $this->print_log("Overwrote $0 (now modified " . $this->get_prog_mtime() . ")\n");
      $this->end_section("CHECKING FOR UPGRADE");
      # Check if old content and new content are the same.  If so, continue on;
      # something funky must be happening with the date.
      if ($old_script eq $res->content()) { 
        $this->print_log("---> HMM.  The script I just downloaded is the same as the one before. <---\n");
        $this->print_log("---> There must be a date problem on this machine.  I think I'll just stick <---\n");
        $this->print_log("---> my head in the sand and hope the problem goes away. <---\n");
        $this->print_log("---> ... continuing with build as if nothing untoward had happened ... <---\n");
      } else {
        $this->print_log("Executing newly upgraded script ...\n");
        print "UPGRADING!  Throttling just for fun first ...\n";
        $this->build_finish(303);
        eval {
          # Throttle just in case we get in an upgrade client loop
          $this->maybe_throttle();
          exec("perl", $0, @{$this->{ORIGINAL_ARGS}});
        };
        exit(0);
      }
    } elsif ($res->code() == 304) {
      $this->print_log("Perl script not modified\n");
    } else {
      $this->print_log("Connection or URL failure (" . $res->code() . ")\n");
    }
    $this->end_section("CHECKING FOR UPGRADE");
  }
  $this->print_log("---> cd $current_dir <---\n");
  chdir($current_dir);
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

  # Initialize transient variables
  $this->{BUILD_VARS} = { fields => {} };
  $this->{BUILD_VARS}{START_TIME} = time;

  if ($this->{ARGS}{dir}) {
    $this->{BUILD_VARS}{olddir} = getcwd();
    if (!chdir($this->{ARGS}{dir})) {
      print "Could not change to directory $this->{ARGS}{dir}!\n";
      return;
    }
  }

  # Open the log
  open $this->{LOG_OUT}, ">tinderclient.log" or die "Could not output to tinder log";
  open $this->{LOG_IN}, "tinderclient.log" or die "Could not read tinder log";

  #
  # Send build start notification
  #
  if (!$this->build_start()) {
    return;
  }

  my $err = 0;
  eval {
    $this->maybe_upgrade();
    $this->print_build_info();

    # Build
    foreach my $module ("init_tree", "build", "distribute", "tests") {
      $err = $this->call_module($module, "do_action");
      last if $err;
    }

    # Call cleanup
    foreach my $module ("init_tree", "build", "distribute", "tests") {
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

  # Change back to where we were before this iteration
  if (defined($this->{BUILD_VARS}{olddir})) {
    $this->print_log("---> cd $this->{ARGS}{olddir} <---\n");
    chdir($this->{BUILD_VARS}{olddir});
  }
  $this->maybe_throttle();
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
# Sets up the system info object
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

  #
  # Set up compiler
  #
  if ($os =~ /^WIN/) {
    $this->{COMPILER} = 'cl';
  } else {
    $this->{COMPILER} = 'gcc';
  }
  if ($this->{COMPILER} eq 'cl') {
    $this->{COMPILER_VERSION} = `cl 2>&1`;
  } elsif ($this->{COMPILER} eq 'gcc') {
    $this->{COMPILER_VERSION} = `gcc --version`;
  }
  # XXX figure out version for Windows compiler

  #
  # Set up ps
  #
  if ($this->{OS} =~ /^WIN/) {
    $this->{PS_COMMAND} = "ps aux";
  } else {
    $this->{PS_COMMAND} = "ps -e -o 'pid,ppid'";
  }
  return $this;
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
  $client->get_field($content_ref, "throttle");
  $client->get_field($content_ref, "cvs_co_date");
  $client->get_field($content_ref, "cvsroot");
  $client->get_field($content_ref, "clobber");
  $client->get_field($content_ref, "branch");
  $client->get_field($content_ref, "use_fast_update");
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
  $ENV{MOZILLA_OFFICIAL} = undef;
  delete $ENV{MOZILLA_OFFICIAL};
  $ENV{BUILD_OFFICIAL} = undef;
  delete $ENV{BUILD_OFFICIAL};
  $ENV{MOZ_CO_FLAGS} = undef;
  delete $ENV{MOZ_CO_FLAGS};
  $ENV{MOZ_CO_DATE} = undef;
  delete $ENV{MOZ_CO_DATE};
  $ENV{MOZ_OBJDIR} = undef;
  delete $ENV{MOZ_OBJDIR};
}

sub get_cvs_branch {
  my ($client) = @_;
  if (open ENTRIES, "CVS/Entries") {
    while (<ENTRIES>) {
      next if /^D/;
      chomp;
      my @line = split /\//;
      if ($line[1] eq "client.mk") {
        close ENTRIES;
        return substr($line[5], 1);
      }
    }
    close ENTRIES;
  }
  $client->print_log("Warning: could not open CVS/Entries or find client.mk in it");
  return undef;
}

sub do_action {
  my ($client, $config, $persistent_vars, $build_vars) = @_;
  my $init_tree_status = 1;
  my $max_cvs_idle_time = 90*60; # 90 minutes

  #
  # We will only build if:
  # - new patches were downloaded
  # - checkout brought something down
  # - the build command was specified
  #
  $build_vars->{SHOULD_BUILD} = 0;

  #
  # Checkout client.mk if we've never done this before
  #
  my $please_checkout = 0;
  if (! -f "mozilla/client.mk") {
    my $co_params = " -PA";
    if ($config->{cvs_co_date} && $config->{cvs_co_date} ne "off") {
      $co_params .= " -D '$config->{cvs_co_date}'";
    }
    if ($config->{branch}) {
      $co_params .= " -r $config->{branch}";
    }

    $client->do_command("cvs -d$config->{cvsroot} co$co_params mozilla/client.mk", $init_tree_status, undef, $max_cvs_idle_time);
    $please_checkout = 1;

    if (! -f "mozilla/client.mk") {
      $client->print_log("Could not check out mozilla/client.mk!");
      return 200;
    }
  }

  #
  # Change to mozilla directory
  #
  $client->print_log("---> cd mozilla <---\n");
  if (!chdir("mozilla")) {
    $client->print_log("Could not cd mozilla!");
    return 200;
  }

  #
  # Create .mozconfig if necessary
  #
  # First read .mozconfig
  my $please_clobber = 0;
  my $mozconfig = $client->read_mozconfig();
  $mozconfig ||= "";
  print "@@@ mozconfig:\n$mozconfig\n@@@ network .mozconfig:\n$build_vars->{MOZCONFIG}\n";
  if ($build_vars->{MOZCONFIG}) {
    if ($mozconfig ne $build_vars->{MOZCONFIG}) {
      $ENV{MOZCONFIG} = undef;
      delete $ENV{MOZCONFIG};
      $client->start_section("CREATING MOZCONFIG");
      open MOZCONFIG, ">.mozconfig";
      print MOZCONFIG $build_vars->{MOZCONFIG};
      close MOZCONFIG;
      $mozconfig = $build_vars->{MOZCONFIG};
      $client->print_log("(Will clobber this cycle)\n");
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
  $client->end_section("PRINTING BUILD INFO");

  #
  # Remove patches
  #
  foreach my $patch (glob("tbox_patches/*.patch")) {
    $client->do_command("patch -Nt -Rp0 < $patch", $init_tree_status);
    $client->do_command("mv $patch $patch.removed");
  }
  my @old_patches;
  foreach my $patch (glob("tbox_patches/*.patch.removed")) {
    if ($patch =~ /^tbox_patches\/(.+)\.patch\.removed$/) {
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
  # Clean non-objdir stuff out to make an objdir build work
  #
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
    if ($config->{cvs_co_date} && $config->{cvs_co_date} ne "off") {
      # XXX $::ENV?
      $ENV{MOZ_CO_DATE} = $config->{cvs_co_date};
    }
    my $parsing_code = sub {
                         if ($_[0] =~ /^[UP] /m) {
                           $build_vars->{SHOULD_BUILD} = 1;
                         }
                         if ($config->{lowbandwidth} && $_[0] =~ /^\? /m) {
                           return "";
                         }
                         return $_[0];
                       };

    #
    # We only want to do a full, slow make -f client.mk checkout if:
    # - this is the first time this program has been called
    # - 24 hours have passed since the last checkout was done
    # - we were given a "checkout" command or this is the first checkout
    # - the cvs_co_date changed
    # - the branch changed
    # - use_fast_update is off
    #
    # All other times we do fast-update.
    #
    if ($config->{cvs_co_date} || $please_checkout ||
        (time - $persistent_vars->{LAST_CHECKOUT}) >= (24*60*60) ||
        get_cvs_branch($client) ne $config->{branch} ||
        !$config->{use_fast_update}) {
      $ENV{MOZ_CO_FLAGS} = "-PA";
      $err = $client->do_command("make -f client.mk checkout", $init_tree_status+1, $parsing_code, $max_cvs_idle_time);
      $persistent_vars->{LAST_CHECKOUT} = time;
    } else {
      $err = $client->do_command("make -f client.mk fast-update", $init_tree_status+1, $parsing_code, $max_cvs_idle_time);
    }
    if ($err) {
      $persistent_vars->{LAST_CHECKOUT} = 0;
    }

    if ($build_vars->{SHOULD_BUILD}) {
      $client->print_log("Found updated scripts during checkout!  Will build.\n");
    }
    $persistent_vars->{LAST_CVS_CO_DATE} = $config->{cvs_co_date};
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

  #
  # Apply patches
  #
  my @patches_applied;
  $build_vars->{fields}{patch} = [];
  if (!$err && (join(' ', sort @old_patches) ne join(' ', sort @{$build_vars->{PATCHES}}) || $build_vars->{SHOULD_BUILD})) {
    $build_vars->{SHOULD_BUILD} = 1;
    $client->start_section("APPLYING PATCHES");
    # Remove old patches
    system("rm -f tbox_patches/*.patch.removed");
    # Apply new patches
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
        } else {
          push @{$build_vars->{fields}{patch}}, $patch_id;
        }
      }
    }
    $client->end_section("APPLYING PATCHES");
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
  $ENV{MOZILLA_OFFICIAL} = undef;
  delete $ENV{MOZILLA_OFFICIAL};
  $ENV{BUILD_OFFICIAL} = undef;
  delete $ENV{BUILD_OFFICIAL};
  $ENV{MOZ_CO_DATE} = undef;
  delete $ENV{MOZ_CO_DATE};
  $ENV{MOZ_OBJDIR} = undef;
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
          if ($_[0] =~ s/^g?make.+Entering directory ['`"](.+)['`"]$/$1/mg) {
            return $_[0];
          }
          if ($_[0] =~ /^\S+$/) {
            return $_[0];
          }
          return "";
        } :
        undef);
  } else {
    $client->print_log("Skipping build because no changes were made\n");
  }

  if (!$build_vars->{SHOULD_BUILD}) {
    $err = 304;
  }
  return $err;
}

package TinderClient::Modules::distribute;

use strict;

sub get_config {
  my ($client, $config, $persistent_vars, $build_vars, $content_ref) = @_;
  $client->get_field($content_ref, "upload_ssh_loc");
  $client->get_field($content_ref, "upload_ssh_dir");
  $client->get_field($content_ref, "upload_dir");
  $client->get_field($content_ref, "uploaded_url");
  $client->get_field($content_ref, "distribute");
  foreach my $distribution (split(/,/, $config->{distribute})) {
    $client->call_module($distribution, "get_config", $content_ref);
  }
}

sub finish_build {
  my ($client, $config, $persistent_vars, $build_vars) = @_;
  foreach my $distribution (split(/,/, $config->{distribute})) {
    $client->call_module($distribution, "finish_build");
  }
}

sub do_action {
  my ($client, $config, $persistent_vars, $build_vars) = @_;

  #
  # Build and upload distribution
  #
  my $err = 0;
  $build_vars->{PACKAGES} = {};
  # Do not build distributions unless we built
  if ($build_vars->{SHOULD_BUILD}) {
    #
    # Build distributions
    #
    if (!$err) {
      foreach my $distribution (split(/,/, $config->{distribute})) {
        $err = $client->call_module($distribution, "do_action");
        last if $err;
      }
    }

    #
    # Upload installer or distribution
    #
    if (!$err) {
      # Get build id
      my $build_id = "";
      if (open BUILD_NUMBER, "objdir/config/build_number") {
        $build_id = <BUILD_NUMBER>;
        chomp $build_id;
        close BUILD_NUMBER;
      }
      if (!$build_id) {
        $build_id = time2str("%Y%m%d%H", time);
      }

      # Upload
      foreach my $field_name (keys %{$build_vars->{PACKAGES}}) {
        $build_vars->{PACKAGES}{$field_name} =~ /([^\/]*)$/;
        my $upload_file = $1;
        $upload_file =~ s/(\..*)$/-$build_id$1/;
        upload_build($client, $config, $build_vars, $field_name, $build_vars->{PACKAGES}{$field_name}, $upload_file);
      }
    }
  } else {
    $client->print_log("Skipping distribution because no build was done\n");
  }

  return $err;
}

sub upload_build {
  my ($client, $config, $build_vars, $field_name, $local_name, $upload_name) = @_;
  if ($config->{upload_ssh_loc} && `which scp`) {
    $config->{upload_ssh_dir} .= "/" if $config->{upload_ssh_dir} && $config->{upload_ssh_dir} !~ /\/$/;
    $client->do_command("scp $local_name $config->{upload_ssh_loc}:$config->{upload_ssh_dir}$upload_name");
    set_upload_dir($config, $field_name, $build_vars, $upload_name);
  }
  if ($config->{upload_dir}) {
    $config->{upload_dir} .= "/" if $config->{upload_dir} && $config->{upload_dir} !~ /\/$/;
    $client->do_command("cp $local_name $config->{upload_dir}$upload_name");
    set_upload_dir($config, $field_name, $build_vars, $upload_name);
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


package TinderClient::Modules::installer;

sub get_config {
  my ($client, $config, $persistent_vars, $build_vars, $content_ref) = @_;
}

sub finish_build {
  my ($client, $config, $persistent_vars, $build_vars) = @_;
}

sub do_action {
  my ($client, $config, $persistent_vars, $build_vars) = @_;

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
  
  #if ($local_file) {
  #  $build_vars->{PACKAGES}{build_zip} = $local_file;
  #}
  $client->print_log("---> installer is not supported until it works with objdir builds (bug 162079) <---\n");
  return 0;
}

package TinderClient::Modules::build_zip;

sub get_config {
  my ($client, $config, $persistent_vars, $build_vars, $content_ref) = @_;
}

sub finish_build {
  my ($client, $config, $persistent_vars, $build_vars) = @_;
}

sub do_action {
  my ($client, $config, $persistent_vars, $build_vars) = @_;

  $client->do_command("make -C objdir/xpinstall/packager", 11);

  # Find zipped build
  my ($local_file) = glob("objdir/dist/mozilla*.tgz");
  if (!$local_file) {
    ($local_file) = glob("objdir/dist/mozilla*.tar.gz");
  }
  if (!$local_file) {
    ($local_file) = glob("objdir/dist/mozilla*.zip");
  }
  if ($local_file) {
    $build_vars->{PACKAGES}{build_zip} = $local_file;
  }
  return 0;
}

package TinderClient::Modules::raw_zip;

sub get_config {
  my ($client, $config, $persistent_vars, $build_vars, $content_ref) = @_;
  $client->get_field($content_ref, "raw_zip_name");
}

sub finish_build {
  my ($client, $config, $persistent_vars, $build_vars) = @_;
}

sub do_action {
  my ($client, $config, $persistent_vars, $build_vars) = @_;

  my $sysinfo = $client->sysinfo();
  if (chdir("objdir/dist/bin")) {
    my $os = $sysinfo->{OS};
    $os =~ tr/A-Z/a-z/;
    $os =~ s/[^a-z]//g;
    $client->do_command("tar czvfh ../$config->{raw_zip_name}-$os.tar.gz *", 12);
    chdir("../../..");
    $build_vars->{PACKAGES}{raw_zip} = "objdir/dist/$config->{raw_zip_name}-$os.tar.gz";
  }
  return 0;
}

package TinderClient::Modules::tests;

use strict;

sub get_config {
  my ($client, $config, $persistent_vars, $build_vars, $content_ref) = @_;
  $client->get_field($content_ref, "tests");
  foreach my $module (split(/,/, $config->{tests})) {
    $client->call_module($module, "get_config", $content_ref);
  }
}

sub finish_build {
  my ($client, $config, $persistent_vars, $build_vars) = @_;
  foreach my $module (split(/,/, $config->{tests})) {
    $client->call_module($module, "finish_build");
  }
}

sub do_action {
  my ($client, $config, $persistent_vars, $build_vars) = @_;
  foreach my $module (split(/,/, $config->{tests})) {
    my $err = $client->call_module($module, "do_action");
    if ($err) {
      return $err;
    }
  }
  return 0;
}


package TinderClient::Modules::Tp;

use strict;

sub get_config {
  my ($client, $config, $persistent_vars, $build_vars, $content_ref) = @_;
  $client->parse_simple_tag($content_ref, "pageloader_url");
}

sub finish_build {
  my ($client, $config, $persistent_vars, $build_vars) = @_;
}

sub do_action {
  my ($client, $config, $persistent_vars, $build_vars) = @_;
  #my $err = $client->do_command("objdir/dist/bin/mozilla -CreateProfile tinder");
#dist/bin/mozilla -P tinder http://cowtools.mcom.com/page-loader/loader.pl?delay=1000\&nocache=0\&maxcyc=1\&timeout=15000\&auto=1
  #return $err;
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
