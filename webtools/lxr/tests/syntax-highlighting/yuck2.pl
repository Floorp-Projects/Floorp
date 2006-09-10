#!/usr/bin/perl
sub linetag {
#$frag =~ s/\n/"\n".&linetag($virtp.$fname, $line)/ge;
#    my $tag = '<a href="'.$_[0].'#L'.$_[1].
#       '" name="L'.$_[1].'">'.$_[1].' </a>';
    my $tag;
    $tag = '<span class=line>';
    $tag .= ' ' if $_[1] < 10;
    $tag .= ' ' if $_[1] < 100;
    $tag .= ' ' if $_[1] < 1000;
    $tag .= &fileref($_[1], $_[0], $_[1]). ' ';
    $tag .= '</span>';
    $tag .= &fileref($_[1], $_[0], $_[1]).' ';
    $tag .= '</span>';
    $tag =~ s/<a/<a name=$_[1]/;
#    $_[1]++;
    return($tag);
}
