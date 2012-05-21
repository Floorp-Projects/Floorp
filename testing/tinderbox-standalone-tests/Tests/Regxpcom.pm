# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

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
