/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef INTERVAL_H_
#define INTERVAL_H_

#include "nsTArray.h"
#include <algorithm>

namespace mp4_demuxer
{

template <typename T>
struct Interval
{
  Interval() : start(0), end(0) {}
  Interval(T aStart, T aEnd) : start(aStart), end(aEnd)
  {
    MOZ_ASSERT(aStart <= aEnd);
  }
  T Length() { return end - start; }
  Interval Intersection(const Interval& aOther) const
  {
    T s = start > aOther.start ? start : aOther.start;
    T e = end < aOther.end ? end : aOther.end;
    if (s > e) {
      return Interval();
    }
    return Interval(s, e);
  }
  bool Contains(const Interval& aOther) const
  {
    return aOther.start >= start && aOther.end <= end;
  }
  bool operator==(const Interval& aOther) const
  {
    return start == aOther.start && end == aOther.end;
  }
  bool operator!=(const Interval& aOther) const { return !(*this == aOther); }
  bool IsNull() const
  {
    return end == start;
  }
  Interval Extents(const Interval& aOther) const
  {
    if (IsNull()) {
      return aOther;
    }
    return Interval(std::min(start, aOther.start),
                    std::max(end, aOther.end));
  }

  T start;
  T end;

  static void SemiNormalAppend(nsTArray<Interval<T>>& aIntervals,
                               Interval<T> aInterval)
  {
    if (!aIntervals.IsEmpty() &&
        aIntervals.LastElement().end == aInterval.start) {
      aIntervals.LastElement().end = aInterval.end;
    } else {
      aIntervals.AppendElement(aInterval);
    }
  }

  static void Normalize(const nsTArray<Interval<T>>& aIntervals,
                        nsTArray<Interval<T>>* aNormalized)
  {
    if (!aNormalized || !aIntervals.Length()) {
      MOZ_ASSERT(aNormalized);
      return;
    }
    MOZ_ASSERT(aNormalized->IsEmpty());

    nsTArray<Interval<T>> sorted;
    sorted = aIntervals;
    sorted.Sort(Compare());

    Interval<T> current = sorted[0];
    for (size_t i = 1; i < sorted.Length(); i++) {
      MOZ_ASSERT(sorted[i].start <= sorted[i].end);
      if (current.Contains(sorted[i])) {
        continue;
      }
      if (current.end >= sorted[i].start) {
        current.end = sorted[i].end;
      } else {
        aNormalized->AppendElement(current);
        current = sorted[i];
      }
    }
    aNormalized->AppendElement(current);
  }

  static void Intersection(const nsTArray<Interval<T>>& a0,
                           const nsTArray<Interval<T>>& a1,
                           nsTArray<Interval<T>>* aIntersection)
  {
    MOZ_ASSERT(IsNormalized(a0));
    MOZ_ASSERT(IsNormalized(a1));
    size_t i0 = 0;
    size_t i1 = 0;
    while (i0 < a0.Length() && i1 < a1.Length()) {
      Interval i = a0[i0].Intersection(a1[i1]);
      if (i.Length()) {
        aIntersection->AppendElement(i);
      }
      if (a0[i0].end < a1[i1].end) {
        i0++;
        // Assert that the array is sorted
        MOZ_ASSERT(i0 == a0.Length() || a0[i0 - 1].start < a0[i0].start);
      } else {
        i1++;
        // Assert that the array is sorted
        MOZ_ASSERT(i1 == a1.Length() || a1[i1 - 1].start < a1[i1].start);
      }
    }
  }

  static bool IsNormalized(const nsTArray<Interval<T>>& aIntervals)
  {
    for (size_t i = 1; i < aIntervals.Length(); i++) {
      if (aIntervals[i - 1].end >= aIntervals[i].start) {
        return false;
      }
    }
    return true;
  }

  struct Compare
  {
    bool Equals(const Interval<T>& a0, const Interval<T>& a1) const
    {
      return a0.start == a1.start && a0.end == a1.end;
    }

    bool LessThan(const Interval<T>& a0, const Interval<T>& a1) const
    {
      return a0.start < a1.start;
    }
  };
};
}

#endif
