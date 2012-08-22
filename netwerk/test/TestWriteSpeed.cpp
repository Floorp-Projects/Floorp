/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "prio.h"
#include "prinrval.h"
#include "prmem.h"
#include <stdio.h>
#include <math.h>

void 
NS_MeanAndStdDev(double n, double sumOfValues, double sumOfSquaredValues,
                 double *meanResult, double *stdDevResult)
{
  double mean = 0.0, var = 0.0, stdDev = 0.0;
  if (n > 0.0 && sumOfValues >= 0) {
    mean = sumOfValues / n;
    double temp = (n * sumOfSquaredValues) - (sumOfValues * sumOfValues);
    if (temp < 0.0 || n <= 1)
      var = 0.0;
    else
      var = temp / (n * (n - 1));
    // for some reason, Windows says sqrt(0.0) is "-1.#J" (?!) so do this:
    stdDev = var != 0.0 ? sqrt(var) : 0.0;
  }
  *meanResult = mean;
  *stdDevResult = stdDev;
}

int
Test(const char* filename, int32_t minSize, int32_t maxSize, 
     int32_t sizeIncrement, int32_t iterations)
{
    fprintf(stdout, "      size  write:    mean     stddev      iters  total:    mean     stddev      iters\n");
    for (int32_t size = minSize; size <= maxSize; size += sizeIncrement) {
        // create a buffer of stuff to write
        char* buf = (char*)PR_Malloc(size);
        if (buf == NULL)
            return -1;

        // initialize it with a pattern
        int32_t i;
        char hex[] = "0123456789ABCDEF";
        for (i = 0; i < size; i++) {
            buf[i] = hex[i & 0xF];
        }

        double writeCount = 0, writeRate = 0, writeRateSquared = 0;
        double totalCount = 0, totalRate = 0, totalRateSquared = 0;
        for (i = 0; i < iterations; i++) {
            PRIntervalTime start = PR_IntervalNow();

            char name[1024];
            sprintf(name, "%s_%d", filename, i);
            PRFileDesc* fd = PR_Open(name, PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE, 0664);
            if (fd == NULL)
                return -1;

            PRIntervalTime writeStart = PR_IntervalNow();
            int32_t rv = PR_Write(fd, buf, size);
            if (rv < 0) return rv;
            if (rv != size) return -1;
            PRIntervalTime writeStop = PR_IntervalNow();

            PRStatus st = PR_Close(fd);
            if (st == PR_FAILURE) return -1; 

            PRIntervalTime stop = PR_IntervalNow();

            PRIntervalTime writeTime = PR_IntervalToMilliseconds(writeStop - writeStart);
            if (writeTime > 0) {
                double wr = size / writeTime;
                writeRate += wr;
                writeRateSquared += wr * wr;
                writeCount++;
            }

            PRIntervalTime totalTime = PR_IntervalToMilliseconds(stop - start);
            if (totalTime > 0) {
                double t = size / totalTime;
                totalRate += t;
                totalRateSquared += t * t;
                totalCount++;
            }
        }

        PR_Free(buf);

        double writeMean, writeStddev;
        double totalMean, totalStddev;
        NS_MeanAndStdDev(writeCount, writeRate, writeRateSquared,
                         &writeMean, &writeStddev);
        NS_MeanAndStdDev(totalCount, totalRate, totalRateSquared,
                         &totalMean, &totalStddev);
        fprintf(stdout, "%10d      %10.2f %10.2f %10d      %10.2f %10.2f %10d\n",
                size, writeMean, writeStddev, (int32_t)writeCount, 
                totalMean, totalStddev, (int32_t)totalCount);
    }
    return 0;
}

int
main(int argc, char* argv[])
{
    if (argc != 5) {
        printf("usage: %s <min buf size (K)> <max buf size (K)> <size increment (K)> <iterations>\n", argv[0]);
        return -1;
    }
    Test("y:\\foo",
         atoi(argv[1]) * 1024,
         atoi(argv[2]) * 1024,
         atoi(argv[3]) * 1024,
         atoi(argv[4]));
    return 0;
}
