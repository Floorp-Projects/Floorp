# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is linear-regression.awk, released
# November 13, 2000.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 2000
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Chris Waterson <waterson@netscape.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

function regress(DATAPOINTS,SX,SY,SXY,SX2)
{
   b1 = (DATAPOINTS * SXY - SX * SY) / (DATAPOINTS * SX2 - SX * SX);
   b0 = (SY - b1 * SX ) / DATAPOINTS;
   return b1 " * x + " b0; 
}

BEGIN {
        if (!Skip) Skip = 0;
        if (Interval) 
        {
           Count = 0;
           IntervalCount = 0;
        }
      }

NR>Skip {
        sx += $1;
        sy += $2;
        sxy += $1 * $2;
        sx2 += $1 * $1;
        #print NR " " sx " " sy " " sxy " " sx2

        if (Interval)
        {
           if(Count == Interval-1)
           {
              IntervalCount += 1;

              print NR-Count, "-", NR, ":  ", regress(Count,isx,isy,isxy,isx2);

              Count = 0;
              isx = 0;
              isy = 0;
              isxy = 0;
              isx2 = 0;
           }
           else
           {
              Count += 1;
              isx += $1;
              isy += $2;
              isxy += $1 * $2;
              isx2 += $1 * $1;
           }
        }
     }

END {
       if(Interval) {
          print NR-Count, "-", NR, ":  ", regress(Count,isx,isy,isxy,isx2);
       }
       print regress(NR-Skip, sx, sy, sxy, sx2); 
    }
