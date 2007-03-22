# File/Copy.pm. Written in 1994 by Aaron Sherman <ajs@ajs.com>. This
# source code has been placed in the public domain by the author.
# Please be kind and preserve the documentation.
#

package File::Copy;

require Exporter;
use Carp;

@ISA=qw(Exporter);
@EXPORT=qw(copy);
@EXPORT_OK=qw(copy cp);

$File::Copy::VERSION = '1.5';
$File::Copy::Too_Big = 1024 * 1024 * 2;

sub VERSION {
    # Version of File::Copy
    return $File::Copy::VERSION;
}

sub copy {
    croak("Usage: copy( file1, file2 [, buffersize]) ")
      unless(@_ == 2 || @_ == 3);

    my $from = shift;
    my $to = shift;
    my $recsep = $\;
    my $closefrom=0;
    my $closeto=0;
    my ($size, $status, $r, $buf);
    local(*FROM, *TO);

    $\ = '';

    if (ref(\$from) eq 'GLOB') {
	*FROM = $from;
    } elsif (defined ref $from and
	     (ref($from) eq 'GLOB' || ref($from) eq 'FileHandle')) {
	*FROM = *$from;
    } else {
	open(FROM,"<$from")||goto(fail_open1);
	$closefrom = 1;
    }

    if (ref(\$to) eq 'GLOB') {
	*TO = $to;
    } elsif (defined ref $to and
	     (ref($to) eq 'GLOB' || ref($to) eq 'FileHandle')) {
	*TO = *$to;
    } else {
	open(TO,">$to")||goto(fail_open2);
	$closeto=1;
    }

    if (@_) {
	$size = shift(@_) + 0;
	croak("Bad buffer size for copy: $size\n") unless ($size > 0);
    } else {
	$size = -s FROM;
	$size = 1024 if ($size < 512);
	$size = $File::Copy::Too_Big if ($size > $File::Copy::Too_Big);
    }

    $buf = '';
    while(defined($r = read(FROM,$buf,$size)) && $r > 0) {
	if (syswrite (TO,$buf,$r) != $r) {
	    goto fail_inner;    
	}
    }
    goto fail_inner unless(defined($r));
    close(TO) || goto fail_open2 if $closeto;
    close(FROM) || goto fail_open1 if $closefrom;
    $\ = $recsep;
    return 1;
    
    # All of these contortions try to preserve error messages...
  fail_inner:
    if ($closeto) {
	$status = $!;
	$! = 0;
	close TO;
	$! = $status unless $!;
    }
  fail_open2:
    if ($closefrom) {
	$status = $!;
	$! = 0;
	close FROM;
	$! = $status unless $!;
    }
  fail_open1:
    $\ = $recsep;
    return 0;
}
*cp = \&copy;

1;

__END__
=head1 NAME

File::Copy - Copy files or filehandles

=head1 USAGE

  	use File::Copy;

	copy("file1","file2");
  	copy("Copy.pm",\*STDOUT);'

  	use POSIX;
	use File::Copy cp;

	$n=FileHandle->new("/dev/null","r");
	cp($n,"x");'

=head1 DESCRIPTION

The Copy module provides one function (copy) which takes two
parameters: a file to copy from and a file to copy to. Either
argument may be a string, a FileHandle reference or a FileHandle
glob. Obviously, if the first argument is a filehandle of some
sort, it will be read from, and if it is a file I<name> it will
be opened for reading. Likewise, the second argument will be
written to (and created if need be).

An optional third parameter can be used to specify the buffer
size used for copying. This is the number of bytes from the
first file, that wil be held in memory at any given time, before
being written to the second file. The default buffer size depends
upon the file, but will generally be the whole file (up to 2Mb), or
1k for filehandles that do not reference files (eg. sockets).

You may use the syntax C<use File::Copy "cp"> to get at the
"cp" alias for this function. The syntax is I<exactly> the same.

=head1 RETURN

Returns 1 on success, 0 on failure. $! will be set if an error was
encountered.

=head1 AUTHOR

File::Copy was written by Aaron Sherman <ajs@ajs.com> in 1995.

=cut
