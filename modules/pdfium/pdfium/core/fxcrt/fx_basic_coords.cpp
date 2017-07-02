// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include <limits.h>

#include <algorithm>

#include "core/fxcrt/fx_coordinates.h"
#include "core/fxcrt/fx_ext.h"

namespace {

void MatchFloatRange(FX_FLOAT f1, FX_FLOAT f2, int* i1, int* i2) {
  int length = static_cast<int>(FXSYS_ceil(f2 - f1));
  int i1_1 = static_cast<int>(FXSYS_floor(f1));
  int i1_2 = static_cast<int>(FXSYS_ceil(f1));
  FX_FLOAT error1 = f1 - i1_1 + (FX_FLOAT)FXSYS_fabs(f2 - i1_1 - length);
  FX_FLOAT error2 = i1_2 - f1 + (FX_FLOAT)FXSYS_fabs(f2 - i1_2 - length);

  *i1 = (error1 > error2) ? i1_2 : i1_1;
  *i2 = *i1 + length;
}

}  // namespace

void FX_RECT::Normalize() {
  if (left > right) {
    int temp = left;
    left = right;
    right = temp;
  }
  if (top > bottom) {
    int temp = top;
    top = bottom;
    bottom = temp;
  }
}
void FX_RECT::Intersect(const FX_RECT& src) {
  FX_RECT src_n = src;
  src_n.Normalize();
  Normalize();
  left = left > src_n.left ? left : src_n.left;
  top = top > src_n.top ? top : src_n.top;
  right = right < src_n.right ? right : src_n.right;
  bottom = bottom < src_n.bottom ? bottom : src_n.bottom;
  if (left > right || top > bottom) {
    left = top = right = bottom = 0;
  }
}

bool GetIntersection(FX_FLOAT low1,
                     FX_FLOAT high1,
                     FX_FLOAT low2,
                     FX_FLOAT high2,
                     FX_FLOAT& interlow,
                     FX_FLOAT& interhigh) {
  if (low1 >= high2 || low2 >= high1) {
    return false;
  }
  interlow = low1 > low2 ? low1 : low2;
  interhigh = high1 > high2 ? high2 : high1;
  return true;
}
extern "C" int FXSYS_round(FX_FLOAT d) {
  if (d < (FX_FLOAT)INT_MIN) {
    return INT_MIN;
  }
  if (d > (FX_FLOAT)INT_MAX) {
    return INT_MAX;
  }

  return (int)round(d);
}
CFX_FloatRect::CFX_FloatRect(const FX_RECT& rect) {
  left = (FX_FLOAT)(rect.left);
  right = (FX_FLOAT)(rect.right);
  bottom = (FX_FLOAT)(rect.top);
  top = (FX_FLOAT)(rect.bottom);
}
void CFX_FloatRect::Normalize() {
  FX_FLOAT temp;
  if (left > right) {
    temp = left;
    left = right;
    right = temp;
  }
  if (bottom > top) {
    temp = top;
    top = bottom;
    bottom = temp;
  }
}
void CFX_FloatRect::Intersect(const CFX_FloatRect& other_rect) {
  Normalize();
  CFX_FloatRect other = other_rect;
  other.Normalize();
  left = left > other.left ? left : other.left;
  right = right < other.right ? right : other.right;
  bottom = bottom > other.bottom ? bottom : other.bottom;
  top = top < other.top ? top : other.top;
  if (left > right || bottom > top) {
    left = right = bottom = top = 0;
  }
}
void CFX_FloatRect::Union(const CFX_FloatRect& other_rect) {
  Normalize();
  CFX_FloatRect other = other_rect;
  other.Normalize();
  left = left < other.left ? left : other.left;
  right = right > other.right ? right : other.right;
  bottom = bottom < other.bottom ? bottom : other.bottom;
  top = top > other.top ? top : other.top;
}

int CFX_FloatRect::Substract4(CFX_FloatRect& s, CFX_FloatRect* pRects) {
  Normalize();
  s.Normalize();
  int nRects = 0;
  CFX_FloatRect rects[4];
  if (left < s.left) {
    rects[nRects].left = left;
    rects[nRects].right = s.left;
    rects[nRects].bottom = bottom;
    rects[nRects].top = top;
    nRects++;
  }
  if (s.left < right && s.top < top) {
    rects[nRects].left = s.left;
    rects[nRects].right = right;
    rects[nRects].bottom = s.top;
    rects[nRects].top = top;
    nRects++;
  }
  if (s.top > bottom && s.right < right) {
    rects[nRects].left = s.right;
    rects[nRects].right = right;
    rects[nRects].bottom = bottom;
    rects[nRects].top = s.top;
    nRects++;
  }
  if (s.bottom > bottom) {
    rects[nRects].left = s.left;
    rects[nRects].right = s.right;
    rects[nRects].bottom = bottom;
    rects[nRects].top = s.bottom;
    nRects++;
  }
  if (nRects == 0) {
    return 0;
  }
  for (int i = 0; i < nRects; i++) {
    pRects[i] = rects[i];
    pRects[i].Intersect(*this);
  }
  return nRects;
}

FX_RECT CFX_FloatRect::GetOuterRect() const {
  CFX_FloatRect rect1 = *this;
  FX_RECT rect;
  rect.left = (int)FXSYS_floor(rect1.left);
  rect.right = (int)FXSYS_ceil(rect1.right);
  rect.top = (int)FXSYS_floor(rect1.bottom);
  rect.bottom = (int)FXSYS_ceil(rect1.top);
  rect.Normalize();
  return rect;
}

FX_RECT CFX_FloatRect::GetInnerRect() const {
  CFX_FloatRect rect1 = *this;
  FX_RECT rect;
  rect.left = (int)FXSYS_ceil(rect1.left);
  rect.right = (int)FXSYS_floor(rect1.right);
  rect.top = (int)FXSYS_ceil(rect1.bottom);
  rect.bottom = (int)FXSYS_floor(rect1.top);
  rect.Normalize();
  return rect;
}

FX_RECT CFX_FloatRect::GetClosestRect() const {
  CFX_FloatRect rect1 = *this;
  FX_RECT rect;
  MatchFloatRange(rect1.left, rect1.right, &rect.left, &rect.right);
  MatchFloatRange(rect1.bottom, rect1.top, &rect.top, &rect.bottom);
  rect.Normalize();
  return rect;
}

bool CFX_FloatRect::Contains(const CFX_PointF& point) const {
  CFX_FloatRect n1(*this);
  n1.Normalize();
  return point.x <= n1.right && point.x >= n1.left && point.y <= n1.top &&
         point.y >= n1.bottom;
}

bool CFX_FloatRect::Contains(const CFX_FloatRect& other_rect) const {
  CFX_FloatRect n1(*this);
  CFX_FloatRect n2(other_rect);
  n1.Normalize();
  n2.Normalize();
  return n2.left >= n1.left && n2.right <= n1.right && n2.bottom >= n1.bottom &&
         n2.top <= n1.top;
}

void CFX_FloatRect::UpdateRect(FX_FLOAT x, FX_FLOAT y) {
  left = std::min(left, x);
  right = std::max(right, x);
  bottom = std::min(bottom, y);
  top = std::max(top, y);
}

CFX_FloatRect CFX_FloatRect::GetBBox(const CFX_PointF* pPoints, int nPoints) {
  if (nPoints == 0)
    return CFX_FloatRect();

  FX_FLOAT min_x = pPoints->x;
  FX_FLOAT max_x = pPoints->x;
  FX_FLOAT min_y = pPoints->y;
  FX_FLOAT max_y = pPoints->y;
  for (int i = 1; i < nPoints; i++) {
    min_x = std::min(min_x, pPoints[i].x);
    max_x = std::max(max_x, pPoints[i].x);
    min_y = std::min(min_y, pPoints[i].y);
    max_y = std::max(max_y, pPoints[i].y);
  }
  return CFX_FloatRect(min_x, min_y, max_x, max_y);
}

void CFX_Matrix::SetReverse(const CFX_Matrix& m) {
  FX_FLOAT i = m.a * m.d - m.b * m.c;
  if (FXSYS_fabs(i) == 0)
    return;

  FX_FLOAT j = -i;
  a = m.d / i;
  b = m.b / j;
  c = m.c / j;
  d = m.a / i;
  e = (m.c * m.f - m.d * m.e) / i;
  f = (m.a * m.f - m.b * m.e) / j;
}

void CFX_Matrix::Concat(const CFX_Matrix& m, bool bPrepended) {
  ConcatInternal(m, bPrepended);
}

void CFX_Matrix::ConcatInverse(const CFX_Matrix& src, bool bPrepended) {
  CFX_Matrix m;
  m.SetReverse(src);
  Concat(m, bPrepended);
}

bool CFX_Matrix::Is90Rotated() const {
  return FXSYS_fabs(a * 1000) < FXSYS_fabs(b) &&
         FXSYS_fabs(d * 1000) < FXSYS_fabs(c);
}

bool CFX_Matrix::IsScaled() const {
  return FXSYS_fabs(b * 1000) < FXSYS_fabs(a) &&
         FXSYS_fabs(c * 1000) < FXSYS_fabs(d);
}

void CFX_Matrix::Translate(FX_FLOAT x, FX_FLOAT y, bool bPrepended) {
  if (bPrepended) {
    e += x * a + y * c;
    f += y * d + x * b;
    return;
  }
  e += x;
  f += y;
}

void CFX_Matrix::Scale(FX_FLOAT sx, FX_FLOAT sy, bool bPrepended) {
  a *= sx;
  d *= sy;
  if (bPrepended) {
    b *= sx;
    c *= sy;
    return;
  }

  b *= sy;
  c *= sx;
  e *= sx;
  f *= sy;
}

void CFX_Matrix::Rotate(FX_FLOAT fRadian, bool bPrepended) {
  FX_FLOAT cosValue = FXSYS_cos(fRadian);
  FX_FLOAT sinValue = FXSYS_sin(fRadian);
  ConcatInternal(CFX_Matrix(cosValue, sinValue, -sinValue, cosValue, 0, 0),
                 bPrepended);
}

void CFX_Matrix::RotateAt(FX_FLOAT fRadian,
                          FX_FLOAT dx,
                          FX_FLOAT dy,
                          bool bPrepended) {
  Translate(dx, dy, bPrepended);
  Rotate(fRadian, bPrepended);
  Translate(-dx, -dy, bPrepended);
}

void CFX_Matrix::Shear(FX_FLOAT fAlphaRadian,
                       FX_FLOAT fBetaRadian,
                       bool bPrepended) {
  ConcatInternal(
      CFX_Matrix(1, FXSYS_tan(fAlphaRadian), FXSYS_tan(fBetaRadian), 1, 0, 0),
      bPrepended);
}

void CFX_Matrix::MatchRect(const CFX_FloatRect& dest,
                           const CFX_FloatRect& src) {
  FX_FLOAT fDiff = src.left - src.right;
  a = FXSYS_fabs(fDiff) < 0.001f ? 1 : (dest.left - dest.right) / fDiff;

  fDiff = src.bottom - src.top;
  d = FXSYS_fabs(fDiff) < 0.001f ? 1 : (dest.bottom - dest.top) / fDiff;
  e = dest.left - src.left * a;
  f = dest.bottom - src.bottom * d;
  b = 0;
  c = 0;
}

FX_FLOAT CFX_Matrix::GetXUnit() const {
  if (b == 0)
    return (a > 0 ? a : -a);
  if (a == 0)
    return (b > 0 ? b : -b);
  return FXSYS_sqrt(a * a + b * b);
}

FX_FLOAT CFX_Matrix::GetYUnit() const {
  if (c == 0)
    return (d > 0 ? d : -d);
  if (d == 0)
    return (c > 0 ? c : -c);
  return FXSYS_sqrt(c * c + d * d);
}

CFX_FloatRect CFX_Matrix::GetUnitRect() const {
  CFX_FloatRect rect(0, 0, 1, 1);
  TransformRect(rect);
  return rect;
}

FX_FLOAT CFX_Matrix::TransformXDistance(FX_FLOAT dx) const {
  FX_FLOAT fx = a * dx;
  FX_FLOAT fy = b * dx;
  return FXSYS_sqrt(fx * fx + fy * fy);
}

FX_FLOAT CFX_Matrix::TransformDistance(FX_FLOAT dx, FX_FLOAT dy) const {
  FX_FLOAT fx = a * dx + c * dy;
  FX_FLOAT fy = b * dx + d * dy;
  return FXSYS_sqrt(fx * fx + fy * fy);
}

FX_FLOAT CFX_Matrix::TransformDistance(FX_FLOAT distance) const {
  return distance * (GetXUnit() + GetYUnit()) / 2;
}

CFX_PointF CFX_Matrix::Transform(const CFX_PointF& point) const {
  return CFX_PointF(a * point.x + c * point.y + e,
                    b * point.x + d * point.y + f);
}

void CFX_Matrix::TransformRect(CFX_RectF& rect) const {
  FX_FLOAT right = rect.right(), bottom = rect.bottom();
  TransformRect(rect.left, right, bottom, rect.top);
  rect.width = right - rect.left;
  rect.height = bottom - rect.top;
}

void CFX_Matrix::TransformRect(FX_FLOAT& left,
                               FX_FLOAT& right,
                               FX_FLOAT& top,
                               FX_FLOAT& bottom) const {
  CFX_PointF points[] = {
      {left, top}, {left, bottom}, {right, top}, {right, bottom}};
  for (int i = 0; i < 4; i++)
    points[i] = Transform(points[i]);

  right = points[0].x;
  left = points[0].x;
  top = points[0].y;
  bottom = points[0].y;
  for (int i = 1; i < 4; i++) {
    right = std::max(right, points[i].x);
    left = std::min(left, points[i].x);
    top = std::max(top, points[i].y);
    bottom = std::min(bottom, points[i].y);
  }
}

void CFX_Matrix::ConcatInternal(const CFX_Matrix& other, bool prepend) {
  CFX_Matrix left;
  CFX_Matrix right;
  if (prepend) {
    left = other;
    right = *this;
  } else {
    left = *this;
    right = other;
  }

  a = left.a * right.a + left.b * right.c;
  b = left.a * right.b + left.b * right.d;
  c = left.c * right.a + left.d * right.c;
  d = left.c * right.b + left.d * right.d;
  e = left.e * right.a + left.f * right.c + right.e;
  f = left.e * right.b + left.f * right.d + right.f;
}
