# Backwards.pm

# Copyright (C) 1999 Uri Guttman. All rights reserved.
# mail bugs, comments and feedback to uri@sysarch.com


use strict ;

package Backwards ;

use Symbol ;
use Fcntl ;
use Carp ;
use integer ;

#my $max_read_size = 3 ;

my $max_read_size = 1 << 13 ;

# support tied handles. the tied calls map directly to the object methods

*TIEHANDLE = \&new ;
*READLINE = \&readline ;

# create a new Backwards object

sub new {

	my( $class, $filename ) = @_ ;

	my( $handle, $seek_pos, $read_size, $self ) ;

# get a new handle symbol and the real file handle

	$handle = gensym() ;

# open the file for reading

	unless( open( $handle, $filename ) ) {
		carp "Can't open $filename $!" ;
		return ;
	}

# seek to the end of the file

	sysseek( $handle, 0, 2 ) ;
	$seek_pos = tell( $handle ) ;

# get the size of the first block to read,
# either a trailing partial one (the % size) or full sized one (max read size)

	$read_size = $seek_pos % $max_read_size || $max_read_size ;

# create the hash for the object, bless and return it

	$self = {
			'file_name'	=> $filename,
			'handle'	=> $handle,
			'read_size'	=> $read_size,
			'seek_pos'	=> $seek_pos,
			'lines'		=> [],
	} ;

	return( bless( $self, $class ) ) ;
}

sub readline {

	my( $self, $line_ref ) = @_ ;

	my( $handle, $lines_ref, $seek_pos, $read_cnt, $read_buf,
	    $file_size, $read_size, $text ) ;

# get the buffer of lines

	$lines_ref = $self->{'lines'} ;

	while( 1 ) {

# see if there is more than 1 line in the buffer

		if ( @{$lines_ref} > 1 ) {

# we have a complete line so return it

			return( pop @{$lines_ref} ) ;
		}

# we don't have a complete, so have to read blocks until we do

		$seek_pos = $self->{'seek_pos'} ;

# see if we are at the beginning of the file

		if ( $seek_pos == 0 ) {

# the last read never made more lines, so return the last line in the buffer
# if no lines left then undef will be returned

			return( pop @{$lines_ref} ) ;
		}

#print "c size $read_size\n" ;

# we have to read more text so get the handle and the current read size

		$handle = $self->{'handle'} ;
		$read_size = $self->{'read_size'} ;

# after the first read, always read the maximum size

		$self->{'read_size'} = $max_read_size ;

# seek to the beginning of this block and save the new seek position

		$seek_pos -= $read_size ;
		$self->{'seek_pos'} = $seek_pos ;
		sysseek( $handle, $seek_pos, 0 ) ;

#print "seek $seek_pos\n" ;

# read in the next (previous) block of text

		sysseek($handle, 0, 1);
		$read_cnt = sysread( $handle, $read_buf, $read_size ) ;

#print "Read <$read_buf>\n" ;

#   if ( $read_cnt != $read_size ) {
#   	print "bad read cnt $read_cnt != size $read_size\n" ;
#	return( undef ) ;
#   }

# prepend the read buffer to the leftover (possibly partial) line

		$text = $read_buf . ( pop @{$lines_ref} || '' ) ;

# split the buffer into a list of lines
# this may want to be $/ but reading files backwards assumes plain text and
# newline separators

		@{$lines_ref} = $text =~ m[(^.*\n|^.+)]mg ;

#print "Lines \n=>", join( "<=\n=>", @{$lines_ref} ), "<=\n" ;
	}
}

__END__


=head1 NAME

Backwards.pm -- Read a file backwards by lines.
 

=head1 SYNOPSIS

	use Backwards ;

	# Object interface

	$bw = Backwards->new( 'log_file' ) ;

	while( $log_line = $bw->readline ) {
		print $log_line ;
	}

	# Tied Handle Interface

	tie *BW, 'log_file' ;

	while( <BW> ) {
		print ;
	}

=head1 DESCRIPTION
 

This module reads a file backwards line by line. It is simple to use,
memory efficient and fast. It supports both an object and a tied handle
interface.

It is intended for processing log and other similar text files which
typically have new entries appended. It uses newline as the separator
and not $/ since it is only meant to be used for text.

It works by reading large (8kb) blocks of text from the end of the file,
splits them on newlines and stores the other lines until the buffer runs
out. Then it seeks to the previous block and splits it. When it reaches
the beginning of the file, it stops reading more blocks.  All boundary
conditions are handled correctly. If there is a trailing partial line
(no newline) it will be the first line returned. Lines larger than the
read buffer size are ok.

=head2 Object Interface
 

There are only 2 methods in Backwards' object interface, new and
readline.

=head2 new

New takes just a filename for an argument and it either returns the
object on a successful open on that file or undef.

=head2 readline

Readline takes no arguments and it returns the previous line in the file
or undef when there are no more lines in the file.


=head2 Tied Handle Interface

The only tied handle calls supported are TIEHANDLE and READLINE and they
are typeglobbed to new and readline respectively. All other tied handle
operations will generate an unknown method error. Do not seek, write or
do any other operation other than <> on the handle.
