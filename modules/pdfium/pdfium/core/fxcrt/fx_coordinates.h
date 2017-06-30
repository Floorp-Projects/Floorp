// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXCRT_FX_COORDINATES_H_
#define CORE_FXCRT_FX_COORDINATES_H_

#include "core/fxcrt/fx_basic.h"

class CFX_Matrix;

template <class BaseType>
class CFX_PTemplate {
 public:
  CFX_PTemplate() : x(0), y(0) {}
  CFX_PTemplate(BaseType new_x, BaseType new_y) : x(new_x), y(new_y) {}
  CFX_PTemplate(const CFX_PTemplate& other) : x(other.x), y(other.y) {}
  void clear() {
    x = 0;
    y = 0;
  }
  CFX_PTemplate operator=(const CFX_PTemplate& other) {
    if (this != &other) {
      x = other.x;
      y = other.y;
    }
    return *this;
  }
  bool operator==(const CFX_PTemplate& other) const {
    return x == other.x && y == other.y;
  }
  bool operator!=(const CFX_PTemplate& other) const {
    return !(*this == other);
  }
  CFX_PTemplate& operator+=(const CFX_PTemplate<BaseType>& obj) {
    x += obj.x;
    y += obj.y;
    return *this;
  }
  CFX_PTemplate& operator-=(const CFX_PTemplate<BaseType>& obj) {
    x -= obj.x;
    y -= obj.y;
    return *this;
  }
  CFX_PTemplate& operator*=(BaseType factor) {
    x *= factor;
    y *= factor;
    return *this;
  }
  CFX_PTemplate& operator/=(BaseType divisor) {
    x /= divisor;
    y /= divisor;
    return *this;
  }
  CFX_PTemplate operator+(const CFX_PTemplate& other) const {
    return CFX_PTemplate(x + other.x, y + other.y);
  }
  CFX_PTemplate operator-(const CFX_PTemplate& other) const {
    return CFX_PTemplate(x - other.x, y - other.y);
  }
  CFX_PTemplate operator*(BaseType factor) const {
    return CFX_PTemplate(x * factor, y * factor);
  }
  CFX_PTemplate operator/(BaseType divisor) const {
    return CFX_PTemplate(x / divisor, y / divisor);
  }

  BaseType x;
  BaseType y;
};
using CFX_Point = CFX_PTemplate<int32_t>;
using CFX_PointF = CFX_PTemplate<FX_FLOAT>;

template <class BaseType>
class CFX_STemplate {
 public:
  CFX_STemplate() : width(0), height(0) {}

  CFX_STemplate(BaseType new_width, BaseType new_height)
      : width(new_width), height(new_height) {}

  CFX_STemplate(const CFX_STemplate& other)
      : width(other.width), height(other.height) {}

  template <typename OtherType>
  CFX_STemplate<OtherType> As() const {
    return CFX_STemplate<OtherType>(static_cast<OtherType>(width),
                                    static_cast<OtherType>(height));
  }

  void clear() {
    width = 0;
    height = 0;
  }
  CFX_STemplate operator=(const CFX_STemplate& other) {
    if (this != &other) {
      width = other.width;
      height = other.height;
    }
    return *this;
  }
  bool operator==(const CFX_STemplate& other) const {
    return width == other.width && height == other.height;
  }
  bool operator!=(const CFX_STemplate& other) const {
    return !(*this == other);
  }
  CFX_STemplate& operator+=(const CFX_STemplate<BaseType>& obj) {
    width += obj.width;
    height += obj.height;
    return *this;
  }
  CFX_STemplate& operator-=(const CFX_STemplate<BaseType>& obj) {
    width -= obj.width;
    height -= obj.height;
    return *this;
  }
  CFX_STemplate& operator*=(BaseType factor) {
    width *= factor;
    height *= factor;
    return *this;
  }
  CFX_STemplate& operator/=(BaseType divisor) {
    width /= divisor;
    height /= divisor;
    return *this;
  }
  CFX_STemplate operator+(const CFX_STemplate& other) const {
    return CFX_STemplate(width + other.width, height + other.height);
  }
  CFX_STemplate operator-(const CFX_STemplate& other) const {
    return CFX_STemplate(width - other.width, height - other.height);
  }
  CFX_STemplate operator*(BaseType factor) const {
    return CFX_STemplate(width * factor, height * factor);
  }
  CFX_STemplate operator/(BaseType divisor) const {
    return CFX_STemplate(width / divisor, height / divisor);
  }

  BaseType width;
  BaseType height;
};
using CFX_Size = CFX_STemplate<int32_t>;
using CFX_SizeF = CFX_STemplate<FX_FLOAT>;

template <class BaseType>
class CFX_VTemplate : public CFX_PTemplate<BaseType> {
 public:
  using CFX_PTemplate<BaseType>::x;
  using CFX_PTemplate<BaseType>::y;

  CFX_VTemplate() : CFX_PTemplate<BaseType>() {}
  CFX_VTemplate(BaseType new_x, BaseType new_y)
      : CFX_PTemplate<BaseType>(new_x, new_y) {}

  CFX_VTemplate(const CFX_VTemplate& other) : CFX_PTemplate<BaseType>(other) {}

  CFX_VTemplate(const CFX_PTemplate<BaseType>& point1,
                const CFX_PTemplate<BaseType>& point2)
      : CFX_PTemplate<BaseType>(point2.x - point1.x, point2.y - point1.y) {}

  FX_FLOAT Length() const { return FXSYS_sqrt(x * x + y * y); }
  void Normalize() {
    FX_FLOAT fLen = Length();
    if (fLen < 0.0001f)
      return;

    x /= fLen;
    y /= fLen;
  }
  void Translate(BaseType dx, BaseType dy) {
    x += dx;
    y += dy;
  }
  void Scale(BaseType sx, BaseType sy) {
    x *= sx;
    y *= sy;
  }
  void Rotate(FX_FLOAT fRadian) {
    FX_FLOAT cosValue = FXSYS_cos(fRadian);
    FX_FLOAT sinValue = FXSYS_sin(fRadian);
    x = x * cosValue - y * sinValue;
    y = x * sinValue + y * cosValue;
  }
};
using CFX_Vector = CFX_VTemplate<int32_t>;
using CFX_VectorF = CFX_VTemplate<FX_FLOAT>;

// Rectangles.
// TODO(tsepez): Consolidate all these different rectangle classes.

// LTWH rectangles (y-axis runs downwards).
template <class BaseType>
class CFX_RTemplate {
 public:
  using PointType = CFX_PTemplate<BaseType>;
  using SizeType = CFX_STemplate<BaseType>;
  using VectorType = CFX_VTemplate<BaseType>;
  using RectType = CFX_RTemplate<BaseType>;

  CFX_RTemplate() : left(0), top(0), width(0), height(0) {}
  CFX_RTemplate(BaseType dst_left,
                BaseType dst_top,
                BaseType dst_width,
                BaseType dst_height)
      : left(dst_left), top(dst_top), width(dst_width), height(dst_height) {}
  CFX_RTemplate(BaseType dst_left, BaseType dst_top, const SizeType& dst_size)
      : left(dst_left),
        top(dst_top),
        width(dst_size.width),
        height(dst_size.height) {}
  CFX_RTemplate(const PointType& p, BaseType dst_width, BaseType dst_height)
      : left(p.x), top(p.y), width(dst_width), height(dst_height) {}
  CFX_RTemplate(const PointType& p1, const SizeType& s2)
      : left(p1.x), top(p1.y), width(s2.width), height(s2.height) {}
  CFX_RTemplate(const PointType& p1, const PointType& p2)
      : left(p1.x),
        top(p1.y),
        width(p2.width - p1.width),
        height(p2.height - p1.height) {
    Normalize();
  }
  CFX_RTemplate(const PointType& p, const VectorType& v)
      : left(p.x), top(p.y), width(v.x), height(v.y) {
    Normalize();
  }

  // NOLINTNEXTLINE(runtime/explicit)
  CFX_RTemplate(const RectType& other)
      : left(other.left),
        top(other.top),
        width(other.width),
        height(other.height) {}

  template <typename OtherType>
  CFX_RTemplate<OtherType> As() const {
    return CFX_RTemplate<OtherType>(
        static_cast<OtherType>(left), static_cast<OtherType>(top),
        static_cast<OtherType>(width), static_cast<OtherType>(height));
  }

  void Reset() {
    left = 0;
    top = 0;
    width = 0;
    height = 0;
  }
  RectType& operator+=(const PointType& p) {
    left += p.x;
    top += p.y;
    return *this;
  }
  RectType& operator-=(const PointType& p) {
    left -= p.x;
    top -= p.y;
    return *this;
  }
  BaseType right() const { return left + width; }
  BaseType bottom() const { return top + height; }
  void Normalize() {
    if (width < 0) {
      left += width;
      width = -width;
    }
    if (height < 0) {
      top += height;
      height = -height;
    }
  }
  void Offset(BaseType dx, BaseType dy) {
    left += dx;
    top += dy;
  }
  void Inflate(BaseType x, BaseType y) {
    left -= x;
    width += x * 2;
    top -= y;
    height += y * 2;
  }
  void Inflate(const PointType& p) { Inflate(p.x, p.y); }
  void Inflate(BaseType off_left,
               BaseType off_top,
               BaseType off_right,
               BaseType off_bottom) {
    left -= off_left;
    top -= off_top;
    width += off_left + off_right;
    height += off_top + off_bottom;
  }
  void Inflate(const RectType& rt) {
    Inflate(rt.left, rt.top, rt.left + rt.width, rt.top + rt.height);
  }
  void Deflate(BaseType x, BaseType y) {
    left += x;
    width -= x * 2;
    top += y;
    height -= y * 2;
  }
  void Deflate(const PointType& p) { Deflate(p.x, p.y); }
  void Deflate(BaseType off_left,
               BaseType off_top,
               BaseType off_right,
               BaseType off_bottom) {
    left += off_left;
    top += off_top;
    width -= off_left + off_right;
    height -= off_top + off_bottom;
  }
  void Deflate(const RectType& rt) {
    Deflate(rt.left, rt.top, rt.top + rt.width, rt.top + rt.height);
  }
  bool IsEmpty() const { return width <= 0 || height <= 0; }
  bool IsEmpty(FX_FLOAT fEpsilon) const {
    return width <= fEpsilon || height <= fEpsilon;
  }
  void Empty() { width = height = 0; }
  bool Contains(const PointType& p) const {
    return p.x >= left && p.x < left + width && p.y >= top &&
           p.y < top + height;
  }
  bool Contains(const RectType& rt) const {
    return rt.left >= left && rt.right() <= right() && rt.top >= top &&
           rt.bottom() <= bottom();
  }
  BaseType Width() const { return width; }
  BaseType Height() const { return height; }
  SizeType Size() const { return SizeType(width, height); }
  PointType TopLeft() const { return PointType(left, top); }
  PointType TopRight() const { return PointType(left + width, top); }
  PointType BottomLeft() const { return PointType(left, top + height); }
  PointType BottomRight() const {
    return PointType(left + width, top + height);
  }
  PointType Center() const {
    return PointType(left + width / 2, top + height / 2);
  }
  void Union(BaseType x, BaseType y) {
    BaseType r = right();
    BaseType b = bottom();
    if (left > x)
      left = x;
    if (r < x)
      r = x;
    if (top > y)
      top = y;
    if (b < y)
      b = y;
    width = r - left;
    height = b - top;
  }
  void Union(const PointType& p) { Union(p.x, p.y); }
  void Union(const RectType& rt) {
    BaseType r = right();
    BaseType b = bottom();
    if (left > rt.left)
      left = rt.left;
    if (r < rt.right())
      r = rt.right();
    if (top > rt.top)
      top = rt.top;
    if (b < rt.bottom())
      b = rt.bottom();
    width = r - left;
    height = b - top;
  }
  void Intersect(const RectType& rt) {
    BaseType r = right();
    BaseType b = bottom();
    if (left < rt.left)
      left = rt.left;
    if (r > rt.right())
      r = rt.right();
    if (top < rt.top)
      top = rt.top;
    if (b > rt.bottom())
      b = rt.bottom();
    width = r - left;
    height = b - top;
  }
  bool IntersectWith(const RectType& rt) const {
    RectType rect = rt;
    rect.Intersect(*this);
    return !rect.IsEmpty();
  }
  bool IntersectWith(const RectType& rt, FX_FLOAT fEpsilon) const {
    RectType rect = rt;
    rect.Intersect(*this);
    return !rect.IsEmpty(fEpsilon);
  }
  friend bool operator==(const RectType& rc1, const RectType& rc2) {
    return rc1.left == rc2.left && rc1.top == rc2.top &&
           rc1.width == rc2.width && rc1.height == rc2.height;
  }
  friend bool operator!=(const RectType& rc1, const RectType& rc2) {
    return !(rc1 == rc2);
  }

  BaseType left;
  BaseType top;
  BaseType width;
  BaseType height;
};
using CFX_Rect = CFX_RTemplate<int32_t>;
using CFX_RectF = CFX_RTemplate<FX_FLOAT>;

// LTRB rectangles (y-axis runs downwards).
struct FX_RECT {
  FX_RECT() : left(0), top(0), right(0), bottom(0) {}
  FX_RECT(int l, int t, int r, int b) : left(l), top(t), right(r), bottom(b) {}

  int Width() const { return right - left; }
  int Height() const { return bottom - top; }
  bool IsEmpty() const { return right <= left || bottom <= top; }

  bool Valid() const {
    pdfium::base::CheckedNumeric<int> w = right;
    pdfium::base::CheckedNumeric<int> h = bottom;
    w -= left;
    h -= top;
    return w.IsValid() && h.IsValid();
  }

  void Normalize();

  void Intersect(const FX_RECT& src);
  void Intersect(int l, int t, int r, int b) { Intersect(FX_RECT(l, t, r, b)); }

  void Offset(int dx, int dy) {
    left += dx;
    right += dx;
    top += dy;
    bottom += dy;
  }

  bool operator==(const FX_RECT& src) const {
    return left == src.left && right == src.right && top == src.top &&
           bottom == src.bottom;
  }

  bool Contains(int x, int y) const {
    return x >= left && x < right && y >= top && y < bottom;
  }

  int32_t left;
  int32_t top;
  int32_t right;
  int32_t bottom;
};

// LTRB rectangles (y-axis runs upwards).
class CFX_FloatRect {
 public:
  CFX_FloatRect() : CFX_FloatRect(0.0f, 0.0f, 0.0f, 0.0f) {}
  CFX_FloatRect(FX_FLOAT l, FX_FLOAT b, FX_FLOAT r, FX_FLOAT t)
      : left(l), bottom(b), right(r), top(t) {}

  explicit CFX_FloatRect(const FX_FLOAT* pArray)
      : CFX_FloatRect(pArray[0], pArray[1], pArray[2], pArray[3]) {}

  explicit CFX_FloatRect(const FX_RECT& rect);

  void Normalize();

  void Reset() {
    left = 0.0f;
    right = 0.0f;
    bottom = 0.0f;
    top = 0.0f;
  }

  bool IsEmpty() const { return left >= right || bottom >= top; }

  bool Contains(const CFX_PointF& point) const;
  bool Contains(const CFX_FloatRect& other_rect) const;

  void Intersect(const CFX_FloatRect& other_rect);
  void Union(const CFX_FloatRect& other_rect);

  FX_RECT GetInnerRect() const;
  FX_RECT GetOuterRect() const;
  FX_RECT GetClosestRect() const;

  int Substract4(CFX_FloatRect& substract_rect, CFX_FloatRect* pRects);

  void InitRect(FX_FLOAT x, FX_FLOAT y) {
    left = x;
    right = x;
    bottom = y;
    top = y;
  }
  void UpdateRect(FX_FLOAT x, FX_FLOAT y);

  FX_FLOAT Width() const { return right - left; }
  FX_FLOAT Height() const { return top - bottom; }

  void Inflate(FX_FLOAT x, FX_FLOAT y) {
    Normalize();
    left -= x;
    right += x;
    bottom -= y;
    top += y;
  }

  void Inflate(FX_FLOAT other_left,
               FX_FLOAT other_bottom,
               FX_FLOAT other_right,
               FX_FLOAT other_top) {
    Normalize();
    left -= other_left;
    bottom -= other_bottom;
    right += other_right;
    top += other_top;
  }

  void Inflate(const CFX_FloatRect& rt) {
    Inflate(rt.left, rt.bottom, rt.right, rt.top);
  }

  void Deflate(FX_FLOAT x, FX_FLOAT y) {
    Normalize();
    left += x;
    right -= x;
    bottom += y;
    top -= y;
  }

  void Deflate(FX_FLOAT other_left,
               FX_FLOAT other_bottom,
               FX_FLOAT other_right,
               FX_FLOAT other_top) {
    Normalize();
    left += other_left;
    bottom += other_bottom;
    right -= other_right;
    top -= other_top;
  }

  void Deflate(const CFX_FloatRect& rt) {
    Deflate(rt.left, rt.bottom, rt.right, rt.top);
  }

  void Translate(FX_FLOAT e, FX_FLOAT f) {
    left += e;
    right += e;
    top += f;
    bottom += f;
  }

  static CFX_FloatRect GetBBox(const CFX_PointF* pPoints, int nPoints);

  FX_RECT ToFxRect() const {
    return FX_RECT(static_cast<int32_t>(left), static_cast<int32_t>(top),
                   static_cast<int32_t>(right), static_cast<int32_t>(bottom));
  }

  static CFX_FloatRect FromCFXRectF(const CFX_RectF& rect) {
    return CFX_FloatRect(rect.left, rect.top, rect.right(), rect.bottom());
  }

  FX_FLOAT left;
  FX_FLOAT bottom;
  FX_FLOAT right;
  FX_FLOAT top;
};

class CFX_Matrix {
 public:
  CFX_Matrix() { SetIdentity(); }

  explicit CFX_Matrix(const FX_FLOAT n[6])
      : a(n[0]), b(n[1]), c(n[2]), d(n[3]), e(n[4]), f(n[5]) {}

  CFX_Matrix(const CFX_Matrix& other)
      : a(other.a),
        b(other.b),
        c(other.c),
        d(other.d),
        e(other.e),
        f(other.f) {}

  CFX_Matrix(FX_FLOAT a1,
             FX_FLOAT b1,
             FX_FLOAT c1,
             FX_FLOAT d1,
             FX_FLOAT e1,
             FX_FLOAT f1)
      : a(a1), b(b1), c(c1), d(d1), e(e1), f(f1) {}

  void operator=(const CFX_Matrix& other) {
    a = other.a;
    b = other.b;
    c = other.c;
    d = other.d;
    e = other.e;
    f = other.f;
  }

  void SetIdentity() {
    a = 1;
    b = 0;
    c = 0;
    d = 1;
    e = 0;
    f = 0;
  }

  void SetReverse(const CFX_Matrix& m);

  void Concat(const CFX_Matrix& m, bool bPrepended = false);
  void ConcatInverse(const CFX_Matrix& m, bool bPrepended = false);

  bool IsIdentity() const {
    return a == 1 && b == 0 && c == 0 && d == 1 && e == 0 && f == 0;
  }

  bool Is90Rotated() const;
  bool IsScaled() const;
  bool WillScale() const { return a != 1.0f || b != 0 || c != 0 || d != 1.0f; }

  void Translate(FX_FLOAT x, FX_FLOAT y, bool bPrepended = false);
  void Translate(int32_t x, int32_t y, bool bPrepended = false) {
    Translate(static_cast<FX_FLOAT>(x), static_cast<FX_FLOAT>(y), bPrepended);
  }

  void Scale(FX_FLOAT sx, FX_FLOAT sy, bool bPrepended = false);
  void Rotate(FX_FLOAT fRadian, bool bPrepended = false);
  void RotateAt(FX_FLOAT fRadian,
                FX_FLOAT x,
                FX_FLOAT y,
                bool bPrepended = false);

  void Shear(FX_FLOAT fAlphaRadian,
             FX_FLOAT fBetaRadian,
             bool bPrepended = false);

  void MatchRect(const CFX_FloatRect& dest, const CFX_FloatRect& src);

  FX_FLOAT GetXUnit() const;
  FX_FLOAT GetYUnit() const;
  CFX_FloatRect GetUnitRect() const;

  FX_FLOAT TransformXDistance(FX_FLOAT dx) const;
  FX_FLOAT TransformDistance(FX_FLOAT dx, FX_FLOAT dy) const;
  FX_FLOAT TransformDistance(FX_FLOAT distance) const;

  CFX_PointF Transform(const CFX_PointF& point) const;

  void TransformRect(CFX_RectF& rect) const;
  void TransformRect(FX_FLOAT& left,
                     FX_FLOAT& right,
                     FX_FLOAT& top,
                     FX_FLOAT& bottom) const;
  void TransformRect(CFX_FloatRect& rect) const {
    TransformRect(rect.left, rect.right, rect.top, rect.bottom);
  }

  FX_FLOAT a;
  FX_FLOAT b;
  FX_FLOAT c;
  FX_FLOAT d;
  FX_FLOAT e;
  FX_FLOAT f;

 private:
  void ConcatInternal(const CFX_Matrix& other, bool prepend);
};

#endif  // CORE_FXCRT_FX_COORDINATES_H_
