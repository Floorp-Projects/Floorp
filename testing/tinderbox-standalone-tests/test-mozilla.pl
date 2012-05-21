#!/usr/bin/perl -w
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


use Util;
use Util::Settings;
use File::Basename;
use strict;

use Tests::AliveTest;
use Tests::RenderPerformanceTest;
use Tests::LayoutPerformanceTest;
use Tests::DHTMLPerformanceTest;

AliveTest($Settings::BuildDir, $Settings::BinaryName);
LayoutPerformanceTest($Settings::BuildDir, $Settings::BinaryName);
DHTMLPerformanceTest($Settings::BuildDir, $Settings::BinaryName);
RenderPerformanceTest($Settings::BuildDir, $Settings::BinaryName);
