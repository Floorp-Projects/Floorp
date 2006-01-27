#!/usr/bin/perl -w
# -*- perl -*-
#
# This script is eval'ed by the fileViewer.cgi script.
#
# Input is a scalar, $FILE, that contains the file text
#
#
#################################################################
#
#  Add color to C and C++ code (html) (really simple)
#
#  This is a quick late-night hack and far from complete. It also
#  tries to work for both C and C++ which might be a mistake
#
#################################################################

my $COMMENT_COLOR="red" ;
my $MACRO_COLOR="#006000" ;
my $STRING_CONSTANT_COLOR="green" ;
my $RESERVED_WORD_COLOR="blue" ;

sub c_comment {
    my $token = shift @_ ;
    my $inr = shift @_ ;
    my $outr = shift @_ ;
    $$inr =~ s/(.*?\*\/)//s ;
    $$outr .= "<font color=\"$COMMENT_COLOR\">${token}$1</font>" ;
}
sub cpp_comment {
    my $token = shift @_ ;
    my $inr = shift @_ ;
    my $outr = shift @_ ;
    $$inr =~ s/(.*?)\n/\n/i ;
    $$outr .= "<font color=\"$COMMENT_COLOR\">${token}$1</font>" ;
}
sub hash {
    my $token = shift @_ ;
    my $inr = shift @_ ;
    my $outr = shift @_ ;
    $$inr =~ s/(\s*\w+)//i ;
    $$outr .= "<font color=\"$MACRO_COLOR\">${token}$1</font>" ;
    
}
sub string {
    my $token = shift @_ ;
    my $inr = shift @_ ;
    my $outr = shift @_ ;
    $$inr =~ s/(.*?)(?<!\\)&quot;// ;
    $$outr .= "$token<font color=\"$STRING_CONSTANT_COLOR\">$1</font>&quot;" ;    
}
sub blueBold {
    my $token = shift @_ ;
    my $inr = shift @_ ;
    my $outr = shift @_ ;
    $$outr .= "<font color=\"$RESERVED_WORD_COLOR\"><b>$token</b></font>" ;
}
sub blue {
    my $token = shift @_ ;
    my $inr = shift @_ ;
    my $outr = shift @_ ;
    $$outr .= "<font color=\"$RESERVED_WORD_COLOR\">$token</font>" ;
}
sub bold {
    my $token = shift @_ ;
    my $inr = shift @_ ;
    my $outr = shift @_ ;
    $$outr .= "<b>$token</b>" ;
}

my @blueBoldWords=("if","else","while","do","goto","for","until") ;
my @blueWords=(
	       "asm",
	       "auto",
	       "bool",
	       "break",
	       "case",
	       "catch",
	       "char",
	       "class",
	       "const",
	       "continue",
	       "default",
	       "delete",
	       "do",
	       "double",
	       "else",
	       "enum",
	       "extern",
	       "false",
	       "float",
	       "for",
	       "friend",
	       "goto",
	       "if",
	       "inline",
	       "int",
	       "long",
	       "new",
	       "operator",
	       "private",
	       "protected",
	       "public",
	       "register",
	       "return",
	       "short",
	       "signed",
	       "sizeof",
	       "static",
	       "struct",
	       "switch",
	       "template",
	       "this",
	       "throw",
	       "true",
	       "try",
	       "typedef",
	       "union",
	       "unsigned",
	       "virtual",
	       "void",
	       "volatile",
	       "while"
	       ) ;
my $boldchars="{}" ;

my %routine = (
	       "/*" => \&c_comment,
		"//" => \&cpp_comment,
		"#" => \&hash,
		"&quot;" => \&string
		) ;
my @re ;
my $b ;
foreach $b (keys %routine) {
    push @re,"\Q$b\E" ;
}

foreach $b (@blueWords) {
    $routine{"$b"} = \&blue ;
    push @re,'\b'.$b.'\b' ;
}
foreach $b (@blueBoldWords) {
    if(! exists $routine{"$b"}) {
	push @re,'\b'.$b.'\b' ;
    }
    $routine{"$b"} = \&blueBold ;
}
foreach $b (split('',$boldchars)) {
    $routine{"$b"} = \&blue ;
    push @re,"\Q$b\E" ;
}

my $in = $FILE ;
$FILE = "" ;

my $re = join("|",@re) ;

while($in =~ s/^(.*?)($re)//s) {
    $FILE .= $1 ;
    my $tok = $2 ;
    if(exists $routine{$tok}) {
	&{$routine{$tok}}($tok,\$in,\$FILE) ;
    }
    else {
	$FILE .= $tok ;
    }
}
$FILE .= $in ;

#
# End
#


