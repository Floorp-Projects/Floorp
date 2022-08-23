// Copyright 2021 Google LLC
// SPDX-License-Identifier: Apache-2.0
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <stdio.h>

#include <algorithm>

#include "hwy/base.h"

// Based on A.7 in "Entwurf und Implementierung vektorisierter
// Sortieralgorithmen" and code by Mark Blacher.
void PrintMergeNetwork16x2() {
  for (int i = 8; i < 16; ++i) {
    printf("v%x = st.SwapAdjacent(d, v%x);\n", i, i);
  }
  for (int i = 0; i < 8; ++i) {
    printf("st.Sort2(d, v%x, v%x);\n", i, 15 - i);
  }
  for (int i = 0; i < 4; ++i) {
    printf("v%x = st.SwapAdjacent(d, v%x);\n", i + 4, i + 4);
    printf("v%x = st.SwapAdjacent(d, v%x);\n", i + 12, i + 12);
  }
  for (int i = 0; i < 4; ++i) {
    printf("st.Sort2(d, v%x, v%x);\n", i, 7 - i);
    printf("st.Sort2(d, v%x, v%x);\n", i + 8, 15 - i);
  }
  for (int i = 0; i < 16; i += 4) {
    printf("v%x = st.SwapAdjacent(d, v%x);\n", i + 2, i + 2);
    printf("v%x = st.SwapAdjacent(d, v%x);\n", i + 3, i + 3);
  }
  for (int i = 0; i < 16; i += 4) {
    printf("st.Sort2(d, v%x, v%x);\n", i, i + 3);
    printf("st.Sort2(d, v%x, v%x);\n", i + 1, i + 2);
  }
  for (int i = 0; i < 16; i += 2) {
    printf("v%x = st.SwapAdjacent(d, v%x);\n", i + 1, i + 1);
  }
  for (int i = 0; i < 16; i += 2) {
    printf("st.Sort2(d, v%x, v%x);\n", i, i + 1);
  }
  for (int i = 0; i < 16; ++i) {
    printf("v%x = st.SortPairsDistance1<kOrder>(d, v%x);\n", i, i);
  }
  printf("\n");
}

void PrintMergeNetwork16x4() {
  printf("\n");

  for (int i = 8; i < 16; ++i) {
    printf("v%x = st.Reverse4(d, v%x);\n", i, i);
  }
  for (int i = 0; i < 8; ++i) {
    printf("st.Sort2(d, v%x, v%x);\n", i, 15 - i);
  }
  for (int i = 0; i < 4; ++i) {
    printf("v%x = st.Reverse4(d, v%x);\n", i + 4, i + 4);
    printf("v%x = st.Reverse4(d, v%x);\n", i + 12, i + 12);
  }
  for (int i = 0; i < 4; ++i) {
    printf("st.Sort2(d, v%x, v%x);\n", i, 7 - i);
    printf("st.Sort2(d, v%x, v%x);\n", i + 8, 15 - i);
  }
  for (int i = 0; i < 16; i += 4) {
    printf("v%x = st.Reverse4(d, v%x);\n", i + 2, i + 2);
    printf("v%x = st.Reverse4(d, v%x);\n", i + 3, i + 3);
  }
  for (int i = 0; i < 16; i += 4) {
    printf("st.Sort2(d, v%x, v%x);\n", i, i + 3);
    printf("st.Sort2(d, v%x, v%x);\n", i + 1, i + 2);
  }
  for (int i = 0; i < 16; i += 2) {
    printf("v%x = st.Reverse4(d, v%x);\n", i + 1, i + 1);
  }
  for (int i = 0; i < 16; i += 2) {
    printf("st.Sort2(d, v%x, v%x);\n", i, i + 1);
  }
  for (int i = 0; i < 16; ++i) {
    printf("v%x = st.SortPairsReverse4(d, v%x);\n", i, i);
  }
  for (int i = 0; i < 16; ++i) {
    printf("v%x = st.SortPairsDistance1<kOrder>(d, v%x);\n", i, i);
  }
}

void PrintMergeNetwork16x8() {
  printf("\n");

  for (int i = 8; i < 16; ++i) {
    printf("v%x = st.ReverseKeys8(d, v%x);\n", i, i);
  }
  for (int i = 0; i < 8; ++i) {
    printf("st.Sort2(d, v%x, v%x);\n", i, 15 - i);
  }
  for (int i = 0; i < 4; ++i) {
    printf("v%x = st.ReverseKeys8(d, v%x);\n", i + 4, i + 4);
    printf("v%x = st.ReverseKeys8(d, v%x);\n", i + 12, i + 12);
  }
  for (int i = 0; i < 4; ++i) {
    printf("st.Sort2(d, v%x, v%x);\n", i, 7 - i);
    printf("st.Sort2(d, v%x, v%x);\n", i + 8, 15 - i);
  }
  for (int i = 0; i < 16; i += 4) {
    printf("v%x = st.ReverseKeys8(d, v%x);\n", i + 2, i + 2);
    printf("v%x = st.ReverseKeys8(d, v%x);\n", i + 3, i + 3);
  }
  for (int i = 0; i < 16; i += 4) {
    printf("st.Sort2(d, v%x, v%x);\n", i, i + 3);
    printf("st.Sort2(d, v%x, v%x);\n", i + 1, i + 2);
  }
  for (int i = 0; i < 16; i += 2) {
    printf("v%x = st.ReverseKeys8(d, v%x);\n", i + 1, i + 1);
  }
  for (int i = 0; i < 16; i += 2) {
    printf("st.Sort2(d, v%x, v%x);\n", i, i + 1);
  }
  for (int i = 0; i < 16; ++i) {
    printf("v%x = st.SortPairsReverse8(d, v%x);\n", i, i);
  }
  for (int i = 0; i < 16; ++i) {
    printf("v%x = st.SortPairsDistance2<kOrder>(d, v%x);\n", i, i);
  }
  for (int i = 0; i < 16; ++i) {
    printf("v%x = st.SortPairsDistance1<kOrder>(d, v%x);\n", i, i);
  }
}

void PrintMergeNetwork16x16() {
  printf("\n");

  for (int i = 8; i < 16; ++i) {
    printf("v%x = st.ReverseKeys16(d, v%x);\n", i, i);
  }
  for (int i = 0; i < 8; ++i) {
    printf("st.Sort2(d, v%x, v%x);\n", i, 15 - i);
  }
  for (int i = 0; i < 4; ++i) {
    printf("v%x = st.ReverseKeys16(d, v%x);\n", i + 4, i + 4);
    printf("v%x = st.ReverseKeys16(d, v%x);\n", i + 12, i + 12);
  }
  for (int i = 0; i < 4; ++i) {
    printf("st.Sort2(d, v%x, v%x);\n", i, 7 - i);
    printf("st.Sort2(d, v%x, v%x);\n", i + 8, 15 - i);
  }
  for (int i = 0; i < 16; i += 4) {
    printf("v%x = st.ReverseKeys16(d, v%x);\n", i + 2, i + 2);
    printf("v%x = st.ReverseKeys16(d, v%x);\n", i + 3, i + 3);
  }
  for (int i = 0; i < 16; i += 4) {
    printf("st.Sort2(d, v%x, v%x);\n", i, i + 3);
    printf("st.Sort2(d, v%x, v%x);\n", i + 1, i + 2);
  }
  for (int i = 0; i < 16; i += 2) {
    printf("v%x = st.ReverseKeys16(d, v%x);\n", i + 1, i + 1);
  }
  for (int i = 0; i < 16; i += 2) {
    printf("st.Sort2(d, v%x, v%x);\n", i, i + 1);
  }
  for (int i = 0; i < 16; ++i) {
    printf("v%x = st.SortPairsReverse16<kOrder>(d, v%x);\n", i, i);
  }
  for (int i = 0; i < 16; ++i) {
    printf("v%x = st.SortPairsDistance4<kOrder>(d, v%x);\n", i, i);
  }
  for (int i = 0; i < 16; ++i) {
    printf("v%x = st.SortPairsDistance2<kOrder>(d, v%x);\n", i, i);
  }
  for (int i = 0; i < 16; ++i) {
    printf("v%x = st.SortPairsDistance1<kOrder>(d, v%x);\n", i, i);
  }
}

int main(int argc, char** argv) {
  PrintMergeNetwork16x2();
  PrintMergeNetwork16x4();
  PrintMergeNetwork16x8();
  PrintMergeNetwork16x16();
  return 0;
}
