# IO::SecurePipe.pm
# Created by Ian Hickson to make exec() call if IO::Pipe more secure.
# Distributed under exactly the same licence terms as IO::Pipe.

package IO::SecurePipe;
use strict;
#use Carp;
use IO::Pipe;
use vars qw(@ISA);
@ISA = qw(IO::Pipe);

my $do_spawn = $^O eq 'os2';

sub croak {
    $0 =~ m/^(.*)$/os; # untaint $0 so that we can call it below:
    exec { $1 } ($1, '--abort'); # do not call shutdown handlers
    exit(); # exit (implicit in exec() actually)   
}

sub _doit {
    my $me = shift;
    my $rw = shift;

    my $pid = $do_spawn ? 0 : fork();

    if($pid) { # Parent
        return $pid;
    }
    elsif(defined $pid) { # Child or spawn
        my $fh;
        my $io = $rw ? \*STDIN : \*STDOUT;
        my ($mode, $save) = $rw ? "r" : "w";
        if ($do_spawn) {
          require Fcntl;
          $save = IO::Handle->new_from_fd($io, $mode);
          # Close in child:
          fcntl(shift, Fcntl::F_SETFD(), 1) or croak "fcntl: $!";
          $fh = $rw ? ${*$me}[0] : ${*$me}[1];
        } else {
          shift;
          $fh = $rw ? $me->reader() : $me->writer(); # close the other end
        }
        bless $io, "IO::Handle";
        $io->fdopen($fh, $mode);
	$fh->close;

        if ($do_spawn) {
          $pid = eval { system 1, @_ }; # 1 == P_NOWAIT
          my $err = $!;
    
          $io->fdopen($save, $mode);
          $save->close or croak "Cannot close $!";
          croak "IO::Pipe: Cannot spawn-NOWAIT: $err" if not $pid or $pid < 0;
          return $pid;
        } else {
          exec { $_[0] } @_  or  # XXX change here
            croak "IO::Pipe: Cannot exec: $!";
        }
    }
    else {
        croak "IO::Pipe: Cannot fork: $!";
    }

    # NOT Reached
}

1;
