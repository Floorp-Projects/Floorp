sub Regxpcom {
    #
    # Before running tests, run regxpcom so that we don't crash when
    # people change contractids on us (since we don't autoreg opt builds)
    #
    unlink("$binary_dir/components/compreg.dat") or warn "$binary_dir/components/compreg.dat not removed\n";
    if($Settings::RegxpcomTest) {
        my $args;
        if ($Settings::BinaryName =~ /^(firefox|thunderbird)/) {
            $args = [$binary, "-register"];
        } else {
            $args = ["$binary_dir/regxpcom"];
        }
        AliveTest("regxpcom", $binary_dir, $args,
                  $Settings::RegxpcomTestTimeout);
    }
}

1;
