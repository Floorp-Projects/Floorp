/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Extension Update Service.
 *
 * The Initial Developer of the Original Code is Ben Goodger.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Ben Goodger <ben@bengoodger.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

package org.mozilla.update.extensions;

import java.util.*;

public class Version
{
  public Version(int aMajor, int aMinor, int aRelease, int aBuild, int aPlus)
  {
    this.major    = aMajor;
    this.minor    = aMinor;
    this.release  = aRelease;
    this.build    = aBuild;
    this.plus     = aPlus;
  }

  public Version(String aVersion)
  {
    Version temp = Version.decomposeVersion(aVersion);
    major   = temp.getMajor();
    minor   = temp.getMinor();
    release = temp.getRelease();
    build   = temp.getBuild();
    plus    = temp.getPlus();
  }

  public String toString()
  {
    return major + "." + minor + "." + release + "." + build + (plus == 1 ? "+" : "");
  }

  // -ve      if B is newer
  // equal    if A == B
  // +ve      if A is newer
  public static int compare(String aVersionA, String aVersionB)
  {
    Version a = Version.decomposeVersion(aVersionA);
    Version b = Version.decomposeVersion(aVersionB);

    return a.compare(b);
  }

  public int compare(Version aOtherVersion)
  {
    if (aOtherVersion.getMajor() > getMajor())
      return -1;
    else if (aOtherVersion.getMajor() < getMajor())
      return 1;
    else {
      if (aOtherVersion.getMinor() > getMinor())
        return -1;
      else if (aOtherVersion.getMinor() < getMinor())
        return 1;
      else {
        if (aOtherVersion.getRelease() > getRelease())
          return -1;
        else if (aOtherVersion.getRelease() < getRelease())
          return 1;
        else {
          if (aOtherVersion.getBuild() > getBuild())
            return -1;
          else if (aOtherVersion.getBuild() < getBuild())
            return 1;
          else {
            if (aOtherVersion.getPlus() > getPlus())
              return -1;
            else if (aOtherVersion.getPlus() < getPlus())
              return 1;
          }
        }
      }
    }
    return 0;
  }

  private int major;
  private int minor;
  private int release;
  private int build;
  private int plus;

  public int getMajor()   { return major;   }
  public int getMinor()   { return minor;   }
  public int getRelease() { return release; }
  public int getBuild()   { return build;   }
  public int getPlus()    { return plus;    }

  private static Version decomposeVersion(String aVersion)
  {
    int plus = 0;
    if (aVersion.endsWith("+")) 
    {
      aVersion = aVersion.substring(0, aVersion.lastIndexOf("+"));
      plus = 1;
    }

    int parts[] = { 0, 0, 0, 0 };
    StringTokenizer tokenizer = new StringTokenizer(aVersion, ".");
    for (int i = 0; tokenizer.hasMoreTokens() && i < 4; ++i) 
    {
      String token = tokenizer.nextToken();
      if (token.length() == 0)
        continue;
      try 
      {
        parts[i] = Integer.parseInt(token);
      }
      catch (NumberFormatException nfe) 
      {
        parts[i] = 0;
      }
    }

    return new Version(parts[0], parts[1], parts[2], parts[3], plus);
  }
}

