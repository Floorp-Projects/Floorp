while (<>) {
    chomp;
    if (/^mozilla.exe/) {
        $start = 1;
    }
    if ($start) {
        chomp;
        @fields = split(/  */);
        $bytes = $fields[2];
        $bytes =~ s/,//g;
        $codesize += $bytes;
    }
}
printf "%8.2f K codesize\n", toK($codesize);

sub toK()
{
    return $_[0] / 1024;
}
