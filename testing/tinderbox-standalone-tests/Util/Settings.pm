# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

package Settings;

# general settings
$TestsPhoneHome                   = 1;
$results_server                   = "build-graphs.mozilla.org";
$GraphNameOverride                = '';
$BuildTag                         = '';
$Compiler                         = 'gcc';                
$CleanProfile                     = 1;     # remove profile on start
$ObjDir                           = 1;     # use objdir
$pageload_server                  = 'axolotl.mozilla.org';
$OS                               = 'Linux';
$BinaryName                       = '/home/rhelmer/Apps/firefox/firefox-bin';
$EmbedBinaryName                  = 'TestGTKEmbed';
$MozProfileName                   = 'default';
$BuildDir                         = '/home/rhelmer/Apps/firefox';

# tests to run
$LayoutPerformanceTest            = 1;
$XpcomGlueTest                    = 0;
$RegxpcomTestTimeout              = 120;
$ViewerTest                       = 0;
$XULWindowOpenTestTimeout         = 1;     # Txul
$CodesizeTestTimeout              = 0;     # Z, require
                                                     # mozilla/tools/codesighs
$AliveTest                        = 0;
$EmbedCodesizeTest                = 0;     # mZ, require 
                                                     # mozilla/tools/codesighs
$DomToTextConversionTest          = 0;
$RegxpcomTest                     = 0;
$EditorTest                       = 0;
$CodesizeTest                     = 0;     # Z, require
                                                     # mozilla/tools/codesighs
$EmbedTest                        = 0;     # Assumes you wanted 
                                                     # $BuildEmbed=1
$JavaTest                         = 0;
$NeckoUnitTest                    = 0;
$StartupPerformanceTest           = 0;

# timeouts
$StartupPerformanceTestTimeout    = 15;    # seconds
$LayoutPerformanceTestPageTimeout = 30000; # each page, ms
$RenderTestTimeout                = 1800;  # seconds
$DHTMLPerformanceTestTimeout      = 0;     # Tdhtml
$ViewerTestTimeout                = 45;
$JavaTestTimeout                  = 45;
$MailBloatTestTimeout             = 120;   # seconds
$LayoutPerformanceTestTimeout     = 1200;  # entire test, seconds
$AliveTestTimeout                 = 15;
$NeckoUnitTestTimeout             = 30;    # seconds
$QATestTimeout                    = 30;    # seconds

1;
