#!/usr/bin/perl
# -*- Mode: perl -*-

use ExtUtils::MakeMaker;
WriteMakefile(
	      'MAKEFILE'=> 'Makefile',
	      'VERSION_FROM' => 'lib/Net/ICal/Libical.pm',
	      'NAME'    => 'Net::ICal::Libical',  
	      'INC'     => '-I../libical -I ../libicalss',
	      'LIBS'    => ['-L../libical/.libs -L../libicalss/.libs -lical -licalss -lm'],
	      'OBJECT'  => 'netical_wrap.o',  # Object files
              dist => {
                       COMPRESS     =>'gzip',
                       SUFFIX       =>'gz'
                      },

	     );

sub MY::top_targets
{
  my $self = shift;

  my $old = $self->MM::top_targets;

  return "all::  netical_wrap.o\n$old";

}

sub MY::postamble
{

  return <<EOM;

netical_wrap.c: netical.i
	swig -perl5 -package Net::ICal::Libical netical.i

wrap:
	swig -perl5 -package Net::ICal::Libical netical.i


EOM
}
