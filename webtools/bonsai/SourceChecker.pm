# -*- Mode: perl; tab-width: 4; indent-tabs-mode: nil -*-
#
# The contents of this file are subject to the Netscape Public License
# Version 1.0 (the "License"); you may not use this file except in
# compliance with the License. You may obtain a copy of the License at
# http://www.mozilla.org/NPL/
#
# Software distributed under the License is distributed on an "AS IS"
# basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
# License for the specific language governing rights and limitations
# under the License.
#
# The Original Code is the Bonsai CVS tool.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are Copyright (C) 1998
# Netscape Communications Corporation. All Rights Reserved.

package				SourceChecker;
require				Exporter;
@ISA				=	qw(Exporter);
@EXPORT			= qw(%token_dictionary add_good_english add_good_words add_bad_words add_names tokenize_line markup_line);
@EXPORT_OK	=	qw($GOOD_TOKEN $UNKNOWN_TOKEN $BAD_TOKEN $NAME_TOKEN add_token canonical_token @markup_prefix @markup_suffix);

$GOOD_TOKEN			= \-1;
$UNKNOWN_TOKEN	= \0;
$NAME_TOKEN			= \1;
$BAD_TOKEN			= \2;


@markup_prefix	= ('<FONT COLOR="green">', '<FONT COLOR="red">', '<FONT COLOR="blue">');
@markup_suffix	= ('</FONT>', '</FONT>', '</FONT>');



sub canonical_token($)
	{
		my $token = shift;

		if ( defined $token )
			{
				$token =~ s/[\'Õ\&]+//g;
				$token = length($token)>2 ? lc $token : undef;
			}

		$token;
	}



sub _push_tokens($$)
	{
			# Note: inherits |@exploded_phrases| and |@exploded_tokens| from caller(s)
		push @exploded_phrases, shift;
		push @exploded_tokens, canonical_token(shift);
	}



sub _explode_line($)
	{
			# Note: inherits (and returns results into) |@exploded_phrases| and |@exploded_tokens| from caller(s)

		my $line = shift;

		my $between_tokens = 0;
		foreach $phrase ( split /([A-Za-z\'Õ\&]+)/, $line )
			{
				if ( $between_tokens = !$between_tokens )
					{
						_push_tokens($phrase, undef);
						next;
					}

				for ( $_ = $phrase; $_; )
					{
						m/^[A-Z\'Õ\&]*[a-z\'Õ\&]*/;
						$token = $&;
						$_ = $';

						if ( ($token =~ m/[A-Z][a-z\'Õ]+/) && $` )
							{
								$token = $&;
								_push_tokens($`, $`);
							}
						_push_tokens($token, $token);
					}
			}

		$#exploded_phrases;
	}


sub tokenize_line($)
	{
		my $line = shift;
		local @exploded_tokens;
		_explode_line($line);

		my $i = -1;
		foreach $token ( @exploded_tokens )
			{
				$exploded_tokens[++$i] = $token if defined $token;
			}
		$#exploded_tokens = $i;
		@exploded_tokens;
	}



sub markup_line($)
	{
		my $line = shift;

		local @exploded_phrases;
		local @exploded_tokens;

		_explode_line($line);

		$i = 0;
		foreach $phrase ( @exploded_phrases )
			{
				$phrase =~ s/&/&amp;/g;
				$phrase =~ s/</&lt;/g;
				$phrase =~ s/>/&gt;/g;

				my $token = $exploded_tokens[$i];
				if ( defined $token && ($token_kind = $token_dictionary{$token}) >= 0 )
					{
						$phrase = $markup_prefix[$token_kind] . $phrase . $markup_suffix[$token_kind];
					}
				++$i;
			}

		join '', @exploded_phrases;
	}


sub add_token($$)
	{
		(my $token, my $token_kind) = @_;
		if ( !defined $token_dictionary{$token} || ($token_kind > $token_dictionary{$token}) )
			{
				$token_dictionary{$token} = $token_kind;
			}
	}

sub add_good_english($)
	{
		my $line = shift;
 
		foreach $token ( tokenize_line($line) )
			{
				add_token($token, $$GOOD_TOKEN);

				my $initial_char = substr($token, 0, 1);
				(my $remainder = substr($token, 1)) =~ s/[aeiouy]+//g;

				$abbreviated_length = length($remainder) + 1;
				if ( $abbreviated_length != length($token) && $abbreviated_length > 2 )
					{
						add_token("$initial_char$remainder", $$GOOD_TOKEN);
					}
			}
	}

sub _add_tokens($$)
	{
		(my $line, my $token_kind) = @_;

		foreach $token ( tokenize_line($line) )
			{
				add_token($token, $token_kind);
			}
	}

sub add_good_words($)
	{
		_add_tokens(shift, $$GOOD_TOKEN);
	}

sub add_bad_words($)
	{
		_add_tokens(shift, $$BAD_TOKEN);
	}

sub add_names($)
	{
		_add_tokens(shift, $$NAME_TOKEN);
	}

1;
