/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_WindowsDllBlocklistDefs_h
#define mozilla_WindowsDllBlocklistDefs_h

#include "mozilla/WindowsDllBlocklistCommon.h"

DLL_BLOCKLIST_DEFINITIONS_BEGIN

// EXAMPLE:
// DLL_BLOCKLIST_ENTRY("uxtheme.dll", ALL_VERSIONS)
// DLL_BLOCKLIST_ENTRY("uxtheme.dll", 0x0000123400000000ULL)
// The DLL name must be in lowercase!
// The version field is a maximum, that is, we block anything that is
// less-than or equal to that version.

// NPFFAddon - Known malware
DLL_BLOCKLIST_ENTRY("npffaddon.dll", ALL_VERSIONS)

// AVG 8 - Antivirus vendor AVG, old version, plugin already blocklisted
DLL_BLOCKLIST_ENTRY("avgrsstx.dll", MAKE_VERSION(8,5,0,401))

// calc.dll - Suspected malware
DLL_BLOCKLIST_ENTRY("calc.dll", MAKE_VERSION(1,0,0,1))

// hook.dll - Suspected malware
DLL_BLOCKLIST_ENTRY("hook.dll", ALL_VERSIONS)

// GoogleDesktopNetwork3.dll - Extremely old, unversioned instances
// of this DLL cause crashes
DLL_BLOCKLIST_ENTRY("googledesktopnetwork3.dll", UNVERSIONED)

// rdolib.dll - Suspected malware
DLL_BLOCKLIST_ENTRY("rdolib.dll", MAKE_VERSION(6,0,88,4))

// fgjk4wvb.dll - Suspected malware
DLL_BLOCKLIST_ENTRY("fgjk4wvb.dll", MAKE_VERSION(8,8,8,8))

// radhslib.dll - Naomi internet filter - unmaintained since 2006
DLL_BLOCKLIST_ENTRY("radhslib.dll", UNVERSIONED)

// Music download filter for vkontakte.ru - old instances
// of this DLL cause crashes
DLL_BLOCKLIST_ENTRY("vksaver.dll", MAKE_VERSION(2,2,2,0))

// Topcrash in Firefox 4.0b1
DLL_BLOCKLIST_ENTRY("rlxf.dll", MAKE_VERSION(1,2,323,1))

// psicon.dll - Topcrashes in Thunderbird, and some crashes in Firefox
// Adobe photoshop library, now redundant in later installations
DLL_BLOCKLIST_ENTRY("psicon.dll", ALL_VERSIONS)

// Topcrash in Firefox 4 betas (bug 618899)
DLL_BLOCKLIST_ENTRY("accelerator.dll", MAKE_VERSION(3,2,1,6))

// Topcrash with Roboform in Firefox 8 (bug 699134)
DLL_BLOCKLIST_ENTRY("rf-firefox.dll", MAKE_VERSION(7,6,1,0))
DLL_BLOCKLIST_ENTRY("roboform.dll", MAKE_VERSION(7,6,1,0))

// Topcrash with Babylon Toolbar on FF16+ (bug 721264)
DLL_BLOCKLIST_ENTRY("babyfox.dll", ALL_VERSIONS)

// sprotector.dll crashes, bug 957258
DLL_BLOCKLIST_ENTRY("sprotector.dll", ALL_VERSIONS)

// leave these two in always for tests
DLL_BLOCKLIST_ENTRY("mozdllblockingtest.dll", ALL_VERSIONS)
DLL_BLOCKLIST_ENTRY("mozdllblockingtest_versioned.dll", 0x0000000400000000ULL)

// Windows Media Foundation FLAC decoder and type sniffer (bug 839031).
DLL_BLOCKLIST_ENTRY("mfflac.dll", ALL_VERSIONS)

// Older Relevant Knowledge DLLs cause us to crash (bug 904001).
DLL_BLOCKLIST_ENTRY("rlnx.dll", MAKE_VERSION(1, 3, 334, 9))
DLL_BLOCKLIST_ENTRY("pmnx.dll", MAKE_VERSION(1, 3, 334, 9))
DLL_BLOCKLIST_ENTRY("opnx.dll", MAKE_VERSION(1, 3, 334, 9))
DLL_BLOCKLIST_ENTRY("prnx.dll", MAKE_VERSION(1, 3, 334, 9))

// Older belgian ID card software causes Firefox to crash or hang on
// shutdown, bug 831285 and 918399.
DLL_BLOCKLIST_ENTRY("beid35cardlayer.dll", MAKE_VERSION(3, 5, 6, 6968))

// bug 925459, bitguard crashes
DLL_BLOCKLIST_ENTRY("bitguard.dll", ALL_VERSIONS)

// bug 812683 - crashes in Windows library when Asus Gamer OSD is installed
// Software is discontinued/unsupported
DLL_BLOCKLIST_ENTRY("atkdx11disp.dll", ALL_VERSIONS)

// Topcrash with Conduit SearchProtect, bug 944542
DLL_BLOCKLIST_ENTRY("spvc32.dll", ALL_VERSIONS)

// Topcrash with V-bates, bug 1002748 and bug 1023239
DLL_BLOCKLIST_ENTRY("libinject.dll", UNVERSIONED)
DLL_BLOCKLIST_ENTRY("libinject2.dll", 0x537DDC93, DllBlockInfo::USE_TIMESTAMP)
DLL_BLOCKLIST_ENTRY("libredir2.dll", 0x5385B7ED, DllBlockInfo::USE_TIMESTAMP)

// Crashes with RoboForm2Go written against old SDK, bug 988311/1196859
DLL_BLOCKLIST_ENTRY("rf-firefox-22.dll", ALL_VERSIONS)
DLL_BLOCKLIST_ENTRY("rf-firefox-40.dll", ALL_VERSIONS)

// Crashes with DesktopTemperature, bug 1046382
DLL_BLOCKLIST_ENTRY("dtwxsvc.dll", 0x53153234, DllBlockInfo::USE_TIMESTAMP)

// Startup crashes with Lenovo Onekey Theater, bug 1123778
DLL_BLOCKLIST_ENTRY("activedetect32.dll", UNVERSIONED)
DLL_BLOCKLIST_ENTRY("activedetect64.dll", UNVERSIONED)
DLL_BLOCKLIST_ENTRY("windowsapihookdll32.dll", UNVERSIONED)
DLL_BLOCKLIST_ENTRY("windowsapihookdll64.dll", UNVERSIONED)

// Flash crashes with RealNetworks RealDownloader, bug 1132663
DLL_BLOCKLIST_ENTRY("rndlnpshimswf.dll", ALL_VERSIONS)
DLL_BLOCKLIST_ENTRY("rndlmainbrowserrecordplugin.dll", ALL_VERSIONS)

// Startup crashes with RealNetworks Browser Record Plugin, bug 1170141
DLL_BLOCKLIST_ENTRY("nprpffbrowserrecordext.dll", ALL_VERSIONS)
DLL_BLOCKLIST_ENTRY("nprndlffbrowserrecordext.dll", ALL_VERSIONS)

// Crashes with CyberLink YouCam, bug 1136968
DLL_BLOCKLIST_ENTRY("ycwebcamerasource.ax", MAKE_VERSION(2, 0, 0, 1611))

// Old version of WebcamMax crashes WebRTC, bug 1130061
DLL_BLOCKLIST_ENTRY("vwcsource.ax", MAKE_VERSION(1, 5, 0, 0))

// NetOp School, discontinued product, bug 763395
DLL_BLOCKLIST_ENTRY("nlsp.dll", MAKE_VERSION(6, 23, 2012, 19))

// Orbit Downloader, bug 1222819
DLL_BLOCKLIST_ENTRY("grabdll.dll", MAKE_VERSION(2, 6, 1, 0))
DLL_BLOCKLIST_ENTRY("grabkernel.dll", MAKE_VERSION(1, 0, 0, 1))

// ESET, bug 1229252
DLL_BLOCKLIST_ENTRY("eoppmonitor.dll", ALL_VERSIONS)

// SS2OSD, bug 1262348
DLL_BLOCKLIST_ENTRY("ss2osd.dll", ALL_VERSIONS)
DLL_BLOCKLIST_ENTRY("ss2devprops.dll", ALL_VERSIONS)

// NHASUSSTRIXOSD.DLL, bug 1269244
DLL_BLOCKLIST_ENTRY("nhasusstrixosd.dll", ALL_VERSIONS)
DLL_BLOCKLIST_ENTRY("nhasusstrixdevprops.dll", ALL_VERSIONS)

// Crashes with PremierOpinion/RelevantKnowledge, bug 1277846
DLL_BLOCKLIST_ENTRY("opls.dll", ALL_VERSIONS)
DLL_BLOCKLIST_ENTRY("opls64.dll", ALL_VERSIONS)
DLL_BLOCKLIST_ENTRY("pmls.dll", ALL_VERSIONS)
DLL_BLOCKLIST_ENTRY("pmls64.dll", ALL_VERSIONS)
DLL_BLOCKLIST_ENTRY("prls.dll", ALL_VERSIONS)
DLL_BLOCKLIST_ENTRY("prls64.dll", ALL_VERSIONS)
DLL_BLOCKLIST_ENTRY("rlls.dll", ALL_VERSIONS)
DLL_BLOCKLIST_ENTRY("rlls64.dll", ALL_VERSIONS)

// Vorbis DirectShow filters, bug 1239690.
DLL_BLOCKLIST_ENTRY("vorbis.acm", MAKE_VERSION(0, 0, 3, 6))

// AhnLab Internet Security, bug 1311969
DLL_BLOCKLIST_ENTRY("nzbrcom.dll", ALL_VERSIONS)

// K7TotalSecurity, bug 1339083.
DLL_BLOCKLIST_ENTRY("k7pswsen.dll", MAKE_VERSION(15, 2, 2, 95))

// smci*.dll - goobzo crashware (bug 1339908)
DLL_BLOCKLIST_ENTRY("smci32.dll", ALL_VERSIONS)
DLL_BLOCKLIST_ENTRY("smci64.dll", ALL_VERSIONS)

// Crashes with Internet Download Manager, bug 1333486
DLL_BLOCKLIST_ENTRY("idmcchandler7.dll", ALL_VERSIONS)
DLL_BLOCKLIST_ENTRY("idmcchandler7_64.dll", ALL_VERSIONS)
DLL_BLOCKLIST_ENTRY("idmcchandler5.dll", ALL_VERSIONS)
DLL_BLOCKLIST_ENTRY("idmcchandler5_64.dll", ALL_VERSIONS)

// Nahimic 2 breaks applicaton update (bug 1356637)
DLL_BLOCKLIST_ENTRY("nahimic2devprops.dll", MAKE_VERSION(2, 5, 19, 0xffff))
// Nahimic is causing crashes, bug 1233556
DLL_BLOCKLIST_ENTRY("nahimicmsiosd.dll", UNVERSIONED)
// Nahimic is causing crashes, bug 1360029
DLL_BLOCKLIST_ENTRY("nahimicvrdevprops.dll", UNVERSIONED)
DLL_BLOCKLIST_ENTRY("nahimic2osd.dll", MAKE_VERSION(2, 5, 19, 0xffff))
DLL_BLOCKLIST_ENTRY("nahimicmsidevprops.dll", UNVERSIONED)

// Bug 1268470 - crashes with Kaspersky Lab on Windows 8
DLL_BLOCKLIST_ENTRY("klsihk64.dll", MAKE_VERSION(14, 0, 456, 0xffff), DllBlockInfo::BLOCK_WIN8_ONLY)

// Bug 1407337, crashes with OpenSC < 0.16.0
DLL_BLOCKLIST_ENTRY("onepin-opensc-pkcs11.dll", MAKE_VERSION(0, 15, 0xffff, 0xffff))

// Avecto Privilege Guard causes crashes, bug 1385542
DLL_BLOCKLIST_ENTRY("pghook.dll", ALL_VERSIONS)

// Old versions of G DATA BankGuard, bug 1421991
DLL_BLOCKLIST_ENTRY("banksafe64.dll", MAKE_VERSION(1, 2, 15299, 65535))

// Old versions of G DATA, bug 1043775
DLL_BLOCKLIST_ENTRY("gdkbfltdll64.dll", MAKE_VERSION(1, 0, 14141, 240))

// Dell Backup and Recovery tool causes crashes, bug 1433408
DLL_BLOCKLIST_ENTRY("dbroverlayiconnotbackuped.dll", MAKE_VERSION(1, 8, 0, 9))
DLL_BLOCKLIST_ENTRY("dbroverlayiconbackuped.dll", MAKE_VERSION(1, 8, 0, 9))

DLL_BLOCKLIST_DEFINITIONS_END

#endif // mozilla_WindowsDllBlocklistDefs_h
