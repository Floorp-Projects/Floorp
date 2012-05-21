# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

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
