/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* Adapted from "Accurately computing running variance - John D. Cook"
   http://www.johndcook.com/standard_deviation.html */

#ifndef RUNNING_STAT_H_
#define RUNNING_STAT_H_
#include <math.h>

namespace mozilla {

class RunningStat
{
public:
  RunningStat() : mN(0) {}

  void Clear()
  {
    mN = 0;
  }

  void Push(double x)
  {
    mN++;

    // See Knuth TAOCP vol 2, 3rd edition, page 232
    if (mN == 1)
    {
      mOldM = mNewM = x;
      mOldS = 0.0;
    } else {
      mNewM = mOldM + (x - mOldM) / mN;
      mNewS = mOldS + (x - mOldM) * (x - mNewM);

      // set up for next iteration
      mOldM = mNewM;
      mOldS = mNewS;
    }
  }

  int NumDataValues() const
  {
    return mN;
  }

  double Mean() const
  {
    return (mN > 0) ? mNewM : 0.0;
  }

  double Variance() const
  {
    return (mN > 1) ? mNewS / (mN - 1) : 0.0;
  }

  double StandardDeviation() const
  {
    return sqrt(Variance());
  }

private:
  int mN;
  double mOldM, mNewM, mOldS, mNewS;
};
}
#endif //RUNNING_STAT_H_
