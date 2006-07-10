#!/usr/bin/perl -w

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
