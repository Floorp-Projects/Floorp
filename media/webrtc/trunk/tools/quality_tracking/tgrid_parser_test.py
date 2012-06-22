#!/usr/bin/env python
#-*- coding: utf-8 -*-
#  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
#  Use of this source code is governed by a BSD-style license
#  that can be found in the LICENSE file in the root of the source
#  tree. An additional intellectual property rights grant can be found
#  in the file PATENTS.  All contributing project authors may
#  be found in the AUTHORS file in the root of the source tree.

"""Test the tgrid parser.

   Compatible with build bot 0.8.4 P1.
"""

__author__ = 'phoglund@webrtc.org (Patrik Höglund)'

import unittest

import tgrid_parser


SAMPLE_FILE = """

<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN"
  "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
  <head>
        <meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
            <title>Buildbot</title>
    <link rel="stylesheet" href="default.css" type="text/css" />
    <link rel="alternate" type="application/rss+xml" title="RSS" href="rss">
      </head>
  <body class="interface">
    <div class="header">
        <a href=".">Home</a>
        - <a href="waterfall">Waterfall</a>
        <a href="grid">Grid</a>
        <a href="tgrid">T-Grid</a>
        <a href="console">Console</a>
        <a href="builders">Builders</a>
        <a href="one_line_per_build">Recent Builds</a>
        <a href="buildslaves">Buildslaves</a>
        <a href="changes">Changesources</a>
        - <a href="json/help">JSON API</a>
        - <a href="about">About</a>
    </div>
    <hr/>

    <div class="content">
<h1>Transposed Grid View</h1>

<table class="Grid" border="0" cellspacing="0">

<tr>
 <td class="title"><a href="http://www.chromium.org">WebRTC</a>


 </td>
   <td valign="middle" style="text-align: center" class="builder idle">
    <a href="builders/Android">Android</a></td>
   <td valign="middle" style="text-align: center" class="builder idle">
    <a href="builders/AndroidNDK">AndroidNDK</a></td>
   <td valign="middle" style="text-align: center" class="builder idle">
    <a href="builders/Chrome">Chrome</a></td>
   <td valign="middle" style="text-align: center" class="builder idle">
    <a href="builders/ChromeOS">ChromeOS</a></td>
   <td valign="middle" style="text-align: center" class="builder idle">
    <a href="builders/Linux32DBG">Linux32DBG</a></td>
   <td valign="middle" style="text-align: center" class="builder idle">
    <a href="builders/Linux32Release">Linux32Release</a></td>
   <td valign="middle" style="text-align: center" class="builder idle">
    <a href="builders/Linux64DBG">Linux64DBG</a></td>
   <td valign="middle" style="text-align: center" class="builder idle">
    <a href="builders/Linux64DBG-GCC4.6">Linux64DBG-GCC4.6</a></td>
   <td valign="middle" style="text-align: center" class="builder idle">
    <a href="builders/Linux64Release">Linux64Release</a></td>
   <td valign="middle" style="text-align: center" class="builder idle">
    <a href="builders/LinuxClang">LinuxClang</a></td>
   <td valign="middle" style="text-align: center" class="builder idle">
    <a href="builders/LinuxValgrind">LinuxValgrind</a></td>
   <td valign="middle" style="text-align: center" class="builder idle">
    <a href="builders/LinuxVideoTest">LinuxVideoTest</a></td>
   <td valign="middle" style="text-align: center" class="builder idle">
    <a href="builders/MacOS32DBG">MacOS32DBG</a></td>
   <td valign="middle" style="text-align: center" class="builder building">
    <a href="builders/MacOS32Release">MacOS32Release</a><br/>(building)</td>
   <td valign="middle" style="text-align: center" class="builder idle">
    <a href="builders/Win32Debug">Win32Debug</a></td>
   <td valign="middle" style="text-align: center" class="builder building">
    <a href="builders/Win32Release">Win32Release</a><br/>(building)</td>
 </tr>

 <tr>
 <td valign="bottom" class="sourcestamp">2006  </td>
      <td class="build success">
    <a href="builders/Android/builds/482">OK</a>
  </td>

      <td class="build success">
    <a href="builders/AndroidNDK/builds/70">OK</a>
  </td>

      <td class="build success">
    <a href="builders/Chrome/builds/243">warnings</a>
  </td>

      <td class="build success">
    <a href="builders/ChromeOS/builds/933">OK</a>
  </td>

      <td class="build success">
    <a href="builders/Linux32DBG/builds/936">OK</a>
  </td>

      <td class="build success">
    <a href="builders/Linux32Release/builds/1050">OK</a>
  </td>

      <td class="build success">
    <a href="builders/Linux64DBG/builds/1038">OK</a>
  </td>

      <td class="build success">
    <a href="builders/Linux64DBG-GCC4.6/builds/371">OK</a>
  </td>

      <td class="build success">
    <a href="builders/Linux64Release/builds/936">OK</a>
  </td>

      <td class="build success">
    <a href="builders/LinuxClang/builds/610">OK</a>
  </td>

      <td class="build success">
    <a href="builders/LinuxValgrind/builds/317">OK</a>
  </td>

      <td class="build">&nbsp;</td>

      <td class="build success">
    <a href="builders/MacOS32DBG/builds/1052">OK</a>
  </td>

      <td class="build">&nbsp;</td>

      <td class="build success">
    <a href="builders/Win32Debug/builds/822">OK</a>
  </td>

      <td class="build">&nbsp;</td>

  </tr>
 <tr>
 <td valign="bottom" class="sourcestamp">2007  </td>
      <td class="build success">
    <a href="builders/Android/builds/483">OK</a>
  </td>

      <td class="build success">
    <a href="builders/AndroidNDK/builds/71">OK</a>
  </td>

      <td class="build success">
    <a href="builders/Chrome/builds/244">OK</a>
  </td>

      <td class="build success">
    <a href="builders/ChromeOS/builds/934">OK</a>
  </td>

      <td class="build success">
    <a href="builders/Linux32DBG/builds/937">OK</a>
  </td>

      <td class="build success">
    <a href="builders/Linux32Release/builds/1051">OK</a>
  </td>

      <td class="build success">
    <a href="builders/Linux64DBG/builds/1039">OK</a>
  </td>

      <td class="build success">
    <a href="builders/Linux64DBG-GCC4.6/builds/372">OK</a>
  </td>

      <td class="build success">
    <a href="builders/Linux64Release/builds/937">OK</a>
  </td>

      <td class="build success">
    <a href="builders/LinuxClang/builds/611">OK</a>
  </td>

      <td class="build success">
    <a href="builders/LinuxValgrind/builds/318">OK</a>
  </td>

      <td class="build failure">
    <a href="builders/LinuxVideoTest/builds/731">failed<br/>voe_auto_test</a>
  </td>

      <td class="build success">
    <a href="builders/MacOS32DBG/builds/1053">OK</a>
  </td>

      <td class="build success">
    <a href="builders/MacOS32Release/builds/309">OK</a>
  </td>

      <td class="build success">
    <a href="builders/Win32Debug/builds/823">OK</a>
  </td>

      <td class="build success">
    <a href="builders/Win32Release/builds/809">OK</a>
  </td>

  </tr>
 <tr>
 <td valign="bottom" class="sourcestamp">2008  </td>
      <td class="build success">
    <a href="builders/Android/builds/484">OK</a>
  </td>

      <td class="build success">
    <a href="builders/AndroidNDK/builds/72">OK</a>
  </td>

      <td class="build success">
    <a href="builders/Chrome/builds/245">OK</a>
  </td>

      <td class="build success">
    <a href="builders/ChromeOS/builds/935">OK</a>
  </td>

      <td class="build success">
    <a href="builders/Linux32DBG/builds/938">OK</a>
  </td>

      <td class="build success">
    <a href="builders/Linux32Release/builds/1052">OK</a>
  </td>

      <td class="build success">
    <a href="builders/Linux64DBG/builds/1040">OK</a>
  </td>

      <td class="build success">
    <a href="builders/Linux64DBG-GCC4.6/builds/373">OK</a>
  </td>

      <td class="build success">
    <a href="builders/Linux64Release/builds/938">OK</a>
  </td>

      <td class="build success">
    <a href="builders/LinuxClang/builds/612">OK</a>
  </td>

      <td class="build success">
    <a href="builders/LinuxValgrind/builds/319">OK</a>
  </td>

      <td class="build success">
    <a href="builders/LinuxVideoTest/builds/732">OK</a>
  </td>

      <td class="build success">
    <a href="builders/MacOS32DBG/builds/1054">OK</a>
  </td>

      <td class="build success">
    <a href="builders/MacOS32Release/builds/310">OK</a>
  </td>

      <td class="build success">
    <a href="builders/Win32Debug/builds/824">OK</a>
  </td>

      <td class="build success">
    <a href="builders/Win32Release/builds/810">OK</a>
  </td>

  </tr>
 <tr>
 <td valign="bottom" class="sourcestamp">2010  </td>
      <td class="build success">
    <a href="builders/Android/builds/485">OK</a>
  </td>

      <td class="build success">
    <a href="builders/AndroidNDK/builds/73">OK</a>
  </td>

      <td class="build">&nbsp;</td>

      <td class="build success">
    <a href="builders/ChromeOS/builds/936">OK</a>
  </td>

      <td class="build success">
    <a href="builders/Linux32DBG/builds/939">OK</a>
  </td>

      <td class="build success">
    <a href="builders/Linux32Release/builds/1053">OK</a>
  </td>

      <td class="build success">
    <a href="builders/Linux64DBG/builds/1041">OK</a>
  </td>

      <td class="build success">
    <a href="builders/Linux64DBG-GCC4.6/builds/374">OK</a>
  </td>

      <td class="build success">
    <a href="builders/Linux64Release/builds/939">OK</a>
  </td>

      <td class="build success">
    <a href="builders/LinuxClang/builds/613">OK</a>
  </td>

      <td class="build success">
    <a href="builders/LinuxValgrind/builds/320">OK</a>
  </td>

      <td class="build success">
    <a href="builders/LinuxVideoTest/builds/733">OK</a>
  </td>

      <td class="build success">
    <a href="builders/MacOS32DBG/builds/1055">OK</a>
  </td>

      <td class="build success">
    <a href="builders/MacOS32Release/builds/311">OK</a>
  </td>

      <td class="build success">
    <a href="builders/Win32Debug/builds/825">OK</a>
  </td>

      <td class="build success">
    <a href="builders/Win32Release/builds/811">OK</a>
  </td>

  </tr>
 <tr>
 <td valign="bottom" class="sourcestamp">2011  </td>
      <td class="build success">
    <a href="builders/Android/builds/486">OK</a>
  </td>

      <td class="build success">
    <a href="builders/AndroidNDK/builds/74">OK</a>
  </td>

      <td class="build">&nbsp;</td>

      <td class="build success">
    <a href="builders/ChromeOS/builds/937">OK</a>
  </td>

      <td class="build success">
    <a href="builders/Linux32DBG/builds/940">OK</a>
  </td>

      <td class="build success">
    <a href="builders/Linux32Release/builds/1054">OK</a>
  </td>

      <td class="build success">
    <a href="builders/Linux64DBG/builds/1042">OK</a>
  </td>

      <td class="build success">
    <a href="builders/Linux64DBG-GCC4.6/builds/375">OK</a>
  </td>

      <td class="build success">
    <a href="builders/Linux64Release/builds/940">OK</a>
  </td>

      <td class="build success">
    <a href="builders/LinuxClang/builds/614">OK</a>
  </td>

      <td class="build success">
    <a href="builders/LinuxValgrind/builds/321">OK</a>
  </td>

      <td class="build success">
    <a href="builders/LinuxVideoTest/builds/734">OK</a>
  </td>

      <td class="build success">
    <a href="builders/MacOS32DBG/builds/1056">OK</a>
  </td>

      <td class="build running">
    <a href="builders/MacOS32Release/builds/313">building</a>
  </td>

      <td class="build success">
    <a href="builders/Win32Debug/builds/826">OK</a>
  </td>

      <td class="build running">
    <a href="builders/Win32Release/builds/813">building</a>
  </td>

  </tr>
  <tr>
    <td valign="bottom" class="sourcestamp">latest  </td>
      <td class="build running">
    <a href="builders/MacOS32Release/builds/313">building</a>
  </td>

      <td class="build success">
    <a href="builders/Win32Debug/builds/826">OK</a>
  </td>
  </tr>
</table>

</div><div class="footer" style="clear:both">
      <hr/>
      <a href="http://buildbot.net/">BuildBot</a> (0.8.4p1)
      working for the&nbsp;<a href="http://www.chromium.org">WebRTC
        </a>&nbsp;project.<br/>
      Page built: <b>Thu 12 Apr 2012 03:49:32</b> (CDT)
    </div>
    </body>
</html>
"""

MINIMAL_OK = """
<tr>
<td valign="bottom" class="sourcestamp">1570  </td>
<td class="build success">
<a href="builders/Android/builds/121">OK</a></td>
</tr>
"""

MINIMAL_FAIL = """
<tr>
<td valign="bottom" class="sourcestamp">1573  </td>
<td class="build failure">
  <a href="builders/LinuxVideoTest/builds/731">failed<br/>voe_auto_test</a>
</td>
</tr>
"""

MINIMAL_BUILDING = """
<tr>
<td valign="bottom" class="sourcestamp">1576  </td>
<td class="build running">
<a href="builders/Win32Debug/builds/434">building</a></td>
voe_auto_test</td>
</tr>
"""

MINIMAL_WARNED = """
<tr>
<td valign="bottom" class="sourcestamp">1576  </td>
<td class="build warnings">
<a href="builders/Chrome/builds/109">warnings</a><br />
make chrome</td>
</tr>
"""

MINIMAL_EXCEPTION = """
<tr>
<td valign="bottom" class="sourcestamp">1576  </td>
<td class="build exception">
<a href="builders/Chrome/builds/109">exception</a><br />
Sync</td>
</tr>
"""

MINIMAL_EXCEPTION_SLAVE_LOST = """
<tr>
<td valign="bottom" class="sourcestamp">1576  </td>
<td class="build retry">
  <a href="builders/LinuxValgrind/builds/324">build<br/>successful<br/>exception<br/>slave<br/>lost</a>
</td>
</tr>
"""

class TGridParserTest(unittest.TestCase):
  def test_parser_throws_exception_on_empty_html(self):
    self.assertRaises(tgrid_parser.FailedToParseBuildStatus,
                      tgrid_parser.parse_tgrid_page, '');

  def test_parser_finds_successful_bot(self):
    result = tgrid_parser.parse_tgrid_page(MINIMAL_OK)

    self.assertEqual(1, len(result), 'There is only one bot in the sample.')
    first_mapping = result.items()[0]

    self.assertEqual('1570--Android', first_mapping[0])
    self.assertEqual('121--OK', first_mapping[1])

  def test_parser_finds_failed_bot(self):
    result = tgrid_parser.parse_tgrid_page(MINIMAL_FAIL)

    self.assertEqual(1, len(result), 'There is only one bot in the sample.')
    first_mapping = result.items()[0]

    self.assertEqual('1573--LinuxVideoTest', first_mapping[0])
    self.assertEqual('731--failed', first_mapping[1])

  def test_parser_finds_building_bot(self):
    result = tgrid_parser.parse_tgrid_page(MINIMAL_BUILDING)

    self.assertEqual(1, len(result), 'There is only one bot in the sample.')
    first_mapping = result.items()[0]

    self.assertEqual('1576--Win32Debug', first_mapping[0])
    self.assertEqual('434--building', first_mapping[1])

  def test_parser_finds_warnings(self):
    result = tgrid_parser.parse_tgrid_page(MINIMAL_WARNED)

    self.assertEqual(1, len(result), 'There is only one bot in the sample.')
    first_mapping = result.items()[0]

    self.assertEqual('1576--Chrome', first_mapping[0])
    self.assertEqual('109--warnings', first_mapping[1])

  def test_parser_finds_exception_and_maps_to_failed(self):
    result = tgrid_parser.parse_tgrid_page(MINIMAL_EXCEPTION)

    self.assertEqual(1, len(result), 'There is only one bot in the sample.')
    first_mapping = result.items()[0]

    self.assertEqual('1576--Chrome', first_mapping[0])
    self.assertEqual('109--failed', first_mapping[1])

  def test_parser_finds_exception_slave_lost_and_maps_to_failed(self):
    # This is to work around a bug in build bot 0.8.4p1 where it may say that
    # the build was successful AND the slave was lost. In this case the build
    # is not actually successful, so treat it as such.
    result = tgrid_parser.parse_tgrid_page(MINIMAL_EXCEPTION_SLAVE_LOST)

    self.assertEqual(1, len(result), 'There is only one bot in the sample.')
    first_mapping = result.items()[0]

    self.assertEqual('1576--LinuxValgrind', first_mapping[0])
    self.assertEqual('324--failed', first_mapping[1])

  def test_parser_finds_all_bots_and_revisions_except_forced_builds(self):
    result = tgrid_parser.parse_tgrid_page(SAMPLE_FILE)

    # 5*16 = 80 bots in sample. There's also five empty results because some
    # bots did not run for some revisions, so 80 - 5 = 75 results. There are
    # two additional statuses under an explicit 'latest' revision, which should
    # be ignored since that means the build was forced.
    self.assertEqual(75, len(result))

    # Make some samples
    self.assertTrue(result.has_key('2006--ChromeOS'))
    self.assertEquals('933--OK', result['2006--ChromeOS'])

    self.assertTrue(result.has_key('2006--Chrome'))
    self.assertEquals('243--warnings', result['2006--Chrome'])

    self.assertTrue(result.has_key('2006--LinuxClang'))
    self.assertEquals('610--OK', result['2006--LinuxClang'])

    # This one happened to not get reported in revision 2006, but it should be
    # there in the next revision:
    self.assertFalse(result.has_key('2006--Win32Release'))
    self.assertTrue(result.has_key('2007--Win32Release'))
    self.assertEquals('809--OK', result['2007--Win32Release'])

    self.assertTrue(result.has_key('2007--ChromeOS'))
    self.assertEquals('934--OK', result['2007--ChromeOS'])

    self.assertTrue(result.has_key('2007--LinuxVideoTest'))
    self.assertEquals('731--failed', result['2007--LinuxVideoTest'])

    self.assertTrue(result.has_key('2011--Win32Release'))
    self.assertEquals('813--building', result['2011--Win32Release'])


if __name__ == '__main__':
  unittest.main()
