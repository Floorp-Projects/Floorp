// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/jxl/splines.h"

#include <algorithm>
#include <cmath>

#include "lib/jxl/ans_params.h"
#include "lib/jxl/base/printf_macros.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/chroma_from_luma.h"
#include "lib/jxl/common.h"
#include "lib/jxl/dct_scales.h"
#include "lib/jxl/entropy_coder.h"
#include "lib/jxl/opsin_params.h"

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "lib/jxl/splines.cc"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>

#include "lib/jxl/fast_math-inl.h"
HWY_BEFORE_NAMESPACE();
namespace jxl {
namespace HWY_NAMESPACE {
namespace {

// These templates are not found via ADL.
using hwy::HWY_NAMESPACE::Mul;
using hwy::HWY_NAMESPACE::MulAdd;
using hwy::HWY_NAMESPACE::MulSub;
using hwy::HWY_NAMESPACE::Sqrt;
using hwy::HWY_NAMESPACE::Sub;

// Given a set of DCT coefficients, this returns the result of performing cosine
// interpolation on the original samples.
float ContinuousIDCT(const float dct[32], const float t) {
  // We compute here the DCT-3 of the `dct` vector, rescaled by a factor of
  // sqrt(32). This is such that an input vector vector {x, 0, ..., 0} produces
  // a constant result of x. dct[0] was scaled in Dequantize() to allow uniform
  // treatment of all the coefficients.
  constexpr float kMultipliers[32] = {
      kPi / 32 * 0,  kPi / 32 * 1,  kPi / 32 * 2,  kPi / 32 * 3,  kPi / 32 * 4,
      kPi / 32 * 5,  kPi / 32 * 6,  kPi / 32 * 7,  kPi / 32 * 8,  kPi / 32 * 9,
      kPi / 32 * 10, kPi / 32 * 11, kPi / 32 * 12, kPi / 32 * 13, kPi / 32 * 14,
      kPi / 32 * 15, kPi / 32 * 16, kPi / 32 * 17, kPi / 32 * 18, kPi / 32 * 19,
      kPi / 32 * 20, kPi / 32 * 21, kPi / 32 * 22, kPi / 32 * 23, kPi / 32 * 24,
      kPi / 32 * 25, kPi / 32 * 26, kPi / 32 * 27, kPi / 32 * 28, kPi / 32 * 29,
      kPi / 32 * 30, kPi / 32 * 31,
  };
  HWY_CAPPED(float, 32) df;
  auto result = Zero(df);
  const auto tandhalf = Set(df, t + 0.5f);
  for (int i = 0; i < 32; i += Lanes(df)) {
    auto cos_arg = Mul(LoadU(df, kMultipliers + i), tandhalf);
    auto cos = FastCosf(df, cos_arg);
    auto local_res = Mul(LoadU(df, dct + i), cos);
    result = MulAdd(Set(df, kSqrt2), local_res, result);
  }
  return GetLane(SumOfLanes(df, result));
}

template <typename DF>
void DrawSegment(DF df, const SplineSegment& segment, const bool add,
                 const size_t y, const size_t x, float* JXL_RESTRICT rows[3]) {
  Rebind<int32_t, DF> di;
  const auto inv_sigma = Set(df, segment.inv_sigma);
  const auto half = Set(df, 0.5f);
  const auto one_over_2s2 = Set(df, 0.353553391f);
  const auto sigma_over_4_times_intensity =
      Set(df, segment.sigma_over_4_times_intensity);
  const auto dx = Sub(ConvertTo(df, Iota(di, x)), Set(df, segment.center_x));
  const auto dy = Set(df, y - segment.center_y);
  const auto sqd = MulAdd(dx, dx, Mul(dy, dy));
  const auto distance = Sqrt(sqd);
  const auto one_dimensional_factor =
      Sub(FastErff(df, Mul(MulAdd(distance, half, one_over_2s2), inv_sigma)),
          FastErff(df, Mul(MulSub(distance, half, one_over_2s2), inv_sigma)));
  auto local_intensity =
      Mul(sigma_over_4_times_intensity,
          Mul(one_dimensional_factor, one_dimensional_factor));
  for (size_t c = 0; c < 3; ++c) {
    const auto cm = Set(df, add ? segment.color[c] : -segment.color[c]);
    const auto in = LoadU(df, rows[c] + x);
    StoreU(MulAdd(cm, local_intensity, in), df, rows[c] + x);
  }
}

void DrawSegment(const SplineSegment& segment, const bool add, const size_t y,
                 const ssize_t x0, ssize_t x1, float* JXL_RESTRICT rows[3]) {
  ssize_t x =
      std::max<ssize_t>(x0, segment.center_x - segment.maximum_distance + 0.5f);
  // one-past-the-end
  x1 =
      std::min<ssize_t>(x1, segment.center_x + segment.maximum_distance + 1.5f);
  HWY_FULL(float) df;
  for (; x + static_cast<ssize_t>(Lanes(df)) <= x1; x += Lanes(df)) {
    DrawSegment(df, segment, add, y, x, rows);
  }
  for (; x < x1; ++x) {
    DrawSegment(HWY_CAPPED(float, 1)(), segment, add, y, x, rows);
  }
}

void ComputeSegments(const Spline::Point& center, const float intensity,
                     const float color[3], const float sigma,
                     std::vector<SplineSegment>& segments,
                     std::vector<std::pair<size_t, size_t>>& segments_by_y,
                     size_t* pixel_limit) {
  // In worst case zero-sized dot spans over 2 rows / columns.
  constexpr const float kThinDotSpan = 2.0f;
  // Sanity check sigma, inverse sigma and intensity
  if (!(std::isfinite(sigma) && sigma != 0.0f && std::isfinite(1.0f / sigma) &&
        std::isfinite(intensity))) {
    // Even no-draw should still be accounted.
    *pixel_limit -= std::min<size_t>(*pixel_limit, kThinDotSpan * kThinDotSpan);
    return;
  }
#if JXL_HIGH_PRECISION
  constexpr float kDistanceExp = 5;
#else
  // About 30% faster.
  constexpr float kDistanceExp = 3;
#endif
  // We cap from below colors to at least 0.01.
  float max_color = 0.01f;
  for (size_t c = 0; c < 3; c++) {
    max_color = std::max(max_color, std::abs(color[c] * intensity));
  }
  // Distance beyond which max_color*intensity*exp(-d^2 / (2 * sigma^2)) drops
  // below 10^-kDistanceExp.
  const float maximum_distance =
      std::sqrt(-2 * sigma * sigma *
                (std::log(0.1) * kDistanceExp - std::log(max_color)));
  SplineSegment segment;
  segment.center_y = center.y;
  segment.center_x = center.x;
  memcpy(segment.color, color, sizeof(segment.color));
  segment.inv_sigma = 1.0f / sigma;
  segment.sigma_over_4_times_intensity = .25f * sigma * intensity;
  segment.maximum_distance = maximum_distance;
  float cost = 2.0f * maximum_distance + kThinDotSpan;
  // Check cost^2 fits size_t.
  if (cost >= static_cast<float>(1 << 15)) {
    // Too much to rasterize.
    *pixel_limit = 0;
    return;
  }
  size_t area_cost = static_cast<size_t>(cost * cost);
  if (area_cost > *pixel_limit) {
    *pixel_limit = 0;
    return;
  }
  // TODO(eustas): perhaps we should charge less: (y1 - y0) <= cost
  *pixel_limit -= area_cost;
  ssize_t y0 = center.y - maximum_distance + .5f;
  ssize_t y1 = center.y + maximum_distance + 1.5f;  // one-past-the-end
  for (ssize_t y = std::max<ssize_t>(y0, 0); y < y1; y++) {
    segments_by_y.emplace_back(y, segments.size());
  }
  segments.push_back(segment);
}

void DrawSegments(float* JXL_RESTRICT row_x, float* JXL_RESTRICT row_y,
                  float* JXL_RESTRICT row_b, const Rect& image_rect,
                  const bool add, const SplineSegment* segments,
                  const size_t* segment_indices,
                  const size_t* segment_y_start) {
  JXL_ASSERT(image_rect.ysize() == 1);
  float* JXL_RESTRICT rows[3] = {row_x - image_rect.x0(),
                                 row_y - image_rect.x0(),
                                 row_b - image_rect.x0()};
  size_t y = image_rect.y0();
  for (size_t i = segment_y_start[y]; i < segment_y_start[y + 1]; i++) {
    DrawSegment(segments[segment_indices[i]], add, y, image_rect.x0(),
                image_rect.x0() + image_rect.xsize(), rows);
  }
}

void SegmentsFromPoints(
    const Spline& spline,
    const std::vector<std::pair<Spline::Point, float>>& points_to_draw,
    const float arc_length, std::vector<SplineSegment>& segments,
    std::vector<std::pair<size_t, size_t>>& segments_by_y,
    size_t* pixel_limit) {
  const float inv_arc_length = 1.0f / arc_length;
  int k = 0;
  for (const auto& point_to_draw : points_to_draw) {
    const Spline::Point& point = point_to_draw.first;
    const float multiplier = point_to_draw.second;
    const float progress_along_arc =
        std::min(1.f, (k * kDesiredRenderingDistance) * inv_arc_length);
    ++k;
    float color[3];
    for (size_t c = 0; c < 3; ++c) {
      color[c] =
          ContinuousIDCT(spline.color_dct[c], (32 - 1) * progress_along_arc);
    }
    const float sigma =
        ContinuousIDCT(spline.sigma_dct, (32 - 1) * progress_along_arc);
    ComputeSegments(point, multiplier, color, sigma, segments, segments_by_y,
                    pixel_limit);
    if (*pixel_limit == 0) {
      return;
    }
  }
}
}  // namespace
// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace jxl
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace jxl {
HWY_EXPORT(SegmentsFromPoints);
HWY_EXPORT(DrawSegments);

namespace {

// It is not in spec, but reasonable limit to avoid overflows.
template <typename T>
Status ValidateSplinePointPos(const T& x, const T& y) {
  constexpr T kSplinePosLimit = 1u << 23;
  if ((x >= kSplinePosLimit) || (x <= -kSplinePosLimit) ||
      (y >= kSplinePosLimit) || (y <= -kSplinePosLimit)) {
    return JXL_FAILURE("Spline coordinates out of bounds");
  }
  return true;
}

// Maximum number of spline control points per frame is
//   std::min(kMaxNumControlPoints, xsize * ysize / 2)
constexpr size_t kMaxNumControlPoints = 1u << 20u;
constexpr size_t kMaxNumControlPointsPerPixelRatio = 2;

float AdjustedQuant(const int32_t adjustment) {
  return (adjustment >= 0) ? (1.f + .125f * adjustment)
                           : 1.f / (1.f - .125f * adjustment);
}

float InvAdjustedQuant(const int32_t adjustment) {
  return (adjustment >= 0) ? 1.f / (1.f + .125f * adjustment)
                           : (1.f - .125f * adjustment);
}

// X, Y, B, sigma.
static constexpr float kChannelWeight[] = {0.0042f, 0.075f, 0.07f, .3333f};

Status DecodeAllStartingPoints(std::vector<Spline::Point>* const points,
                               BitReader* const br, ANSSymbolReader* reader,
                               const std::vector<uint8_t>& context_map,
                               const size_t num_splines) {
  points->clear();
  points->reserve(num_splines);
  int64_t last_x = 0;
  int64_t last_y = 0;
  for (size_t i = 0; i < num_splines; i++) {
    int64_t x =
        reader->ReadHybridUint(kStartingPositionContext, br, context_map);
    int64_t y =
        reader->ReadHybridUint(kStartingPositionContext, br, context_map);
    if (i != 0) {
      x = UnpackSigned(x) + last_x;
      y = UnpackSigned(y) + last_y;
    }
    JXL_RETURN_IF_ERROR(ValidateSplinePointPos(x, y));
    points->emplace_back(static_cast<float>(x), static_cast<float>(y));
    last_x = x;
    last_y = y;
  }
  return true;
}

struct Vector {
  float x, y;
  Vector operator-() const { return {-x, -y}; }
  Vector operator+(const Vector& other) const {
    return {x + other.x, y + other.y};
  }
  float SquaredNorm() const { return x * x + y * y; }
};
Vector operator*(const float k, const Vector& vec) {
  return {k * vec.x, k * vec.y};
}

Spline::Point operator+(const Spline::Point& p, const Vector& vec) {
  return {p.x + vec.x, p.y + vec.y};
}
Spline::Point operator-(const Spline::Point& p, const Vector& vec) {
  return p + -vec;
}
Vector operator-(const Spline::Point& a, const Spline::Point& b) {
  return {a.x - b.x, a.y - b.y};
}

// TODO(eustas): avoid making a copy of "points".
void DrawCentripetalCatmullRomSpline(std::vector<Spline::Point> points,
                                     std::vector<Spline::Point>& result) {
  if (points.empty()) return;
  if (points.size() == 1) {
    result.push_back(points[0]);
    return;
  }
  // Number of points to compute between each control point.
  static constexpr int kNumPoints = 16;
  result.reserve((points.size() - 1) * kNumPoints + 1);
  points.insert(points.begin(), points[0] + (points[0] - points[1]));
  points.push_back(points[points.size() - 1] +
                   (points[points.size() - 1] - points[points.size() - 2]));
  // points has at least 4 elements at this point.
  for (size_t start = 0; start < points.size() - 3; ++start) {
    // 4 of them are used, and we draw from p[1] to p[2].
    const Spline::Point* const p = &points[start];
    result.push_back(p[1]);
    float d[3];
    float t[4];
    t[0] = 0;
    for (int k = 0; k < 3; ++k) {
      // TODO(eustas): for each segment delta is calculated 3 times...
      // TODO(eustas): restrict d[k] with reasonable limit and spec it.
      d[k] = std::sqrt(hypotf(p[k + 1].x - p[k].x, p[k + 1].y - p[k].y));
      t[k + 1] = t[k] + d[k];
    }
    for (int i = 1; i < kNumPoints; ++i) {
      const float tt = d[0] + (static_cast<float>(i) / kNumPoints) * d[1];
      Spline::Point a[3];
      for (int k = 0; k < 3; ++k) {
        // TODO(eustas): reciprocal multiplication would be faster.
        a[k] = p[k] + ((tt - t[k]) / d[k]) * (p[k + 1] - p[k]);
      }
      Spline::Point b[2];
      for (int k = 0; k < 2; ++k) {
        b[k] = a[k] + ((tt - t[k]) / (d[k] + d[k + 1])) * (a[k + 1] - a[k]);
      }
      result.push_back(b[0] + ((tt - t[1]) / d[1]) * (b[1] - b[0]));
    }
  }
  result.push_back(points[points.size() - 2]);
}

// Move along the line segments defined by `points`, `kDesiredRenderingDistance`
// pixels at a time, and call `functor` with each point and the actual distance
// to the previous point (which will always be kDesiredRenderingDistance except
// possibly for the very last point).
// TODO(eustas): this method always adds the last point, but never the first
//               (unless those are one); I believe both ends matter.
template <typename Points, typename Functor>
bool ForEachEquallySpacedPoint(const Points& points, const Functor& functor) {
  JXL_ASSERT(!points.empty());
  Spline::Point current = points.front();
  functor(current, kDesiredRenderingDistance);
  auto next = points.begin();
  while (next != points.end()) {
    const Spline::Point* previous = &current;
    float arclength_from_previous = 0.f;
    for (;;) {
      if (next == points.end()) {
        return functor(*previous, arclength_from_previous);
      }
      const float arclength_to_next =
          std::sqrt((*next - *previous).SquaredNorm());
      if (arclength_from_previous + arclength_to_next >=
          kDesiredRenderingDistance) {
        current =
            *previous + ((kDesiredRenderingDistance - arclength_from_previous) /
                         arclength_to_next) *
                            (*next - *previous);
        if (!functor(current, kDesiredRenderingDistance)) {
          return false;
        }
        break;
      }
      arclength_from_previous += arclength_to_next;
      previous = &*next;
      ++next;
    }
  }
  return true;
}

}  // namespace

QuantizedSpline::QuantizedSpline(const Spline& original,
                                 const int32_t quantization_adjustment,
                                 const float y_to_x, const float y_to_b) {
  JXL_ASSERT(!original.control_points.empty());
  control_points_.reserve(original.control_points.size() - 1);
  const Spline::Point& starting_point = original.control_points.front();
  int previous_x = static_cast<int>(roundf(starting_point.x)),
      previous_y = static_cast<int>(roundf(starting_point.y));
  int previous_delta_x = 0, previous_delta_y = 0;
  for (auto it = original.control_points.begin() + 1;
       it != original.control_points.end(); ++it) {
    const int new_x = static_cast<int>(roundf(it->x));
    const int new_y = static_cast<int>(roundf(it->y));
    const int new_delta_x = new_x - previous_x;
    const int new_delta_y = new_y - previous_y;
    control_points_.emplace_back(new_delta_x - previous_delta_x,
                                 new_delta_y - previous_delta_y);
    previous_delta_x = new_delta_x;
    previous_delta_y = new_delta_y;
    previous_x = new_x;
    previous_y = new_y;
  }

  const auto to_int = [](float v) -> int {
    return static_cast<int>(roundf(v));
  };

  const auto quant = AdjustedQuant(quantization_adjustment);
  const auto inv_quant = InvAdjustedQuant(quantization_adjustment);
  for (int c : {1, 0, 2}) {
    float factor = (c == 0) ? y_to_x : (c == 1) ? 0 : y_to_b;
    for (int i = 0; i < 32; ++i) {
      const float dct_factor = (i == 0) ? kSqrt2 : 1.0f;
      const float inv_dct_factor = (i == 0) ? kSqrt0_5 : 1.0f;
      auto restored_y =
          color_dct_[1][i] * inv_dct_factor * kChannelWeight[1] * inv_quant;
      auto decorellated = original.color_dct[c][i] - factor * restored_y;
      color_dct_[c][i] =
          to_int(decorellated * dct_factor * quant / kChannelWeight[c]);
    }
  }
  for (int i = 0; i < 32; ++i) {
    const float dct_factor = (i == 0) ? kSqrt2 : 1.0f;
    sigma_dct_[i] =
        to_int(original.sigma_dct[i] * dct_factor * quant / kChannelWeight[3]);
  }
}

Status QuantizedSpline::Dequantize(const Spline::Point& starting_point,
                                   const int32_t quantization_adjustment,
                                   const float y_to_x, const float y_to_b,
                                   Spline& result) const {
  result.control_points.clear();
  result.control_points.reserve(control_points_.size() + 1);
  float px = roundf(starting_point.x);
  float py = roundf(starting_point.y);
  JXL_RETURN_IF_ERROR(ValidateSplinePointPos(px, py));
  int current_x = static_cast<int>(px);
  int current_y = static_cast<int>(py);
  result.control_points.push_back(Spline::Point{static_cast<float>(current_x),
                                                static_cast<float>(current_y)});
  int current_delta_x = 0, current_delta_y = 0;
  for (const auto& point : control_points_) {
    current_delta_x += point.first;
    current_delta_y += point.second;
    JXL_RETURN_IF_ERROR(
        ValidateSplinePointPos(current_delta_x, current_delta_y));
    current_x += current_delta_x;
    current_y += current_delta_y;
    JXL_RETURN_IF_ERROR(ValidateSplinePointPos(current_x, current_y));
    result.control_points.push_back(Spline::Point{
        static_cast<float>(current_x), static_cast<float>(current_y)});
  }

  const auto inv_quant = InvAdjustedQuant(quantization_adjustment);
  for (int c = 0; c < 3; ++c) {
    for (int i = 0; i < 32; ++i) {
      const float inv_dct_factor = (i == 0) ? kSqrt0_5 : 1.0f;
      result.color_dct[c][i] =
          color_dct_[c][i] * inv_dct_factor * kChannelWeight[c] * inv_quant;
    }
  }
  for (int i = 0; i < 32; ++i) {
    result.color_dct[0][i] += y_to_x * result.color_dct[1][i];
    result.color_dct[2][i] += y_to_b * result.color_dct[1][i];
  }
  for (int i = 0; i < 32; ++i) {
    const float inv_dct_factor = (i == 0) ? kSqrt0_5 : 1.0f;
    result.sigma_dct[i] =
        sigma_dct_[i] * inv_dct_factor * kChannelWeight[3] * inv_quant;
  }

  return true;
}

Status QuantizedSpline::Decode(const std::vector<uint8_t>& context_map,
                               ANSSymbolReader* const decoder,
                               BitReader* const br,
                               const size_t max_control_points,
                               size_t* total_num_control_points) {
  const size_t num_control_points =
      decoder->ReadHybridUint(kNumControlPointsContext, br, context_map);
  *total_num_control_points += num_control_points;
  if (*total_num_control_points > max_control_points) {
    return JXL_FAILURE("Too many control points: %" PRIuS,
                       *total_num_control_points);
  }
  control_points_.resize(num_control_points);
  // Maximal image dimension.
  constexpr int64_t kDeltaLimit = 1u << 30;
  for (std::pair<int64_t, int64_t>& control_point : control_points_) {
    control_point.first = UnpackSigned(
        decoder->ReadHybridUint(kControlPointsContext, br, context_map));
    control_point.second = UnpackSigned(
        decoder->ReadHybridUint(kControlPointsContext, br, context_map));
    // Check delta-deltas are not outrageous; it is not in spec, but there is
    // no reason to allow larger values.
    if ((control_point.first >= kDeltaLimit) ||
        (control_point.first <= -kDeltaLimit) ||
        (control_point.second >= kDeltaLimit) ||
        (control_point.second <= -kDeltaLimit)) {
      return JXL_FAILURE("Spline delta-delta is out of bounds");
    }
  }

  const auto decode_dct = [decoder, br, &context_map](int dct[32]) -> Status {
    for (int i = 0; i < 32; ++i) {
      dct[i] =
          UnpackSigned(decoder->ReadHybridUint(kDCTContext, br, context_map));
    }
    return true;
  };
  for (int c = 0; c < 3; ++c) {
    JXL_RETURN_IF_ERROR(decode_dct(color_dct_[c]));
  }
  JXL_RETURN_IF_ERROR(decode_dct(sigma_dct_));
  return true;
}

void Splines::Clear() {
  quantization_adjustment_ = 0;
  splines_.clear();
  starting_points_.clear();
  segments_.clear();
  segment_indices_.clear();
  segment_y_start_.clear();
}

Status Splines::Decode(jxl::BitReader* br, const size_t num_pixels) {
  std::vector<uint8_t> context_map;
  ANSCode code;
  JXL_RETURN_IF_ERROR(
      DecodeHistograms(br, kNumSplineContexts, &code, &context_map));
  ANSSymbolReader decoder(&code, br);
  const size_t num_splines =
      1 + decoder.ReadHybridUint(kNumSplinesContext, br, context_map);
  size_t max_control_points = std::min(
      kMaxNumControlPoints, num_pixels / kMaxNumControlPointsPerPixelRatio);
  if (num_splines > max_control_points) {
    return JXL_FAILURE("Too many splines: %" PRIuS, num_splines);
  }
  JXL_RETURN_IF_ERROR(DecodeAllStartingPoints(&starting_points_, br, &decoder,
                                              context_map, num_splines));

  quantization_adjustment_ = UnpackSigned(
      decoder.ReadHybridUint(kQuantizationAdjustmentContext, br, context_map));

  splines_.clear();
  splines_.reserve(num_splines);
  size_t num_control_points = num_splines;
  for (size_t i = 0; i < num_splines; ++i) {
    QuantizedSpline spline;
    JXL_RETURN_IF_ERROR(spline.Decode(context_map, &decoder, br,
                                      max_control_points, &num_control_points));
    splines_.push_back(std::move(spline));
  }

  JXL_RETURN_IF_ERROR(decoder.CheckANSFinalState());

  if (!HasAny()) {
    return JXL_FAILURE("Decoded splines but got none");
  }

  return true;
}

void Splines::AddTo(Image3F* const opsin, const Rect& opsin_rect,
                    const Rect& image_rect) const {
  return Apply</*add=*/true>(opsin, opsin_rect, image_rect);
}
void Splines::AddToRow(float* JXL_RESTRICT row_x, float* JXL_RESTRICT row_y,
                       float* JXL_RESTRICT row_b, const Rect& image_row) const {
  return ApplyToRow</*add=*/true>(row_x, row_y, row_b, image_row);
}

void Splines::SubtractFrom(Image3F* const opsin) const {
  return Apply</*add=*/false>(opsin, Rect(*opsin), Rect(*opsin));
}

Status Splines::InitializeDrawCache(const size_t image_xsize,
                                    const size_t image_ysize,
                                    const ColorCorrelationMap& cmap) {
  // TODO(veluca): avoid storing segments that are entirely outside image
  // boundaries.
  segments_.clear();
  segment_indices_.clear();
  segment_y_start_.clear();
  std::vector<std::pair<size_t, size_t>> segments_by_y;
  Spline spline;
  // TODO(eustas): not in the spec; limit spline pixels with image area.
  float pixel_limit = 16.0f * image_xsize * image_ysize + (1 << 16);
  // Apply some extra cap to avoid overflows.
  constexpr size_t kHardPixelLimit = 1u << 30;
  size_t px_limit = (pixel_limit < static_cast<float>(kHardPixelLimit))
                        ? static_cast<size_t>(pixel_limit)
                        : kHardPixelLimit;
  std::vector<Spline::Point> intermediate_points;
  for (size_t i = 0; i < splines_.size(); ++i) {
    JXL_RETURN_IF_ERROR(
        splines_[i].Dequantize(starting_points_[i], quantization_adjustment_,
                               cmap.YtoXRatio(0), cmap.YtoBRatio(0), spline));
    if (std::adjacent_find(spline.control_points.begin(),
                           spline.control_points.end()) !=
        spline.control_points.end()) {
      // Otherwise division by zero might occur. Once control points coincide,
      // the direction of curve is undefined...
      return JXL_FAILURE(
          "identical successive control points in spline %" PRIuS, i);
    }
    std::vector<std::pair<Spline::Point, float>> points_to_draw;
    const auto add_point = [&](const Spline::Point& point,
                               const float multiplier) -> bool {
      points_to_draw.emplace_back(point, multiplier);
      return (points_to_draw.size() <= px_limit);
    };
    intermediate_points.clear();
    DrawCentripetalCatmullRomSpline(spline.control_points, intermediate_points);
    if (!ForEachEquallySpacedPoint(intermediate_points, add_point)) {
      return JXL_FAILURE("Too many pixels covered with splines");
    }
    const float arc_length =
        (points_to_draw.size() - 2) * kDesiredRenderingDistance +
        points_to_draw.back().second;
    if (arc_length <= 0.f) {
      // This spline wouldn't have any effect.
      continue;
    }
    HWY_DYNAMIC_DISPATCH(SegmentsFromPoints)
    (spline, points_to_draw, arc_length, segments_, segments_by_y, &px_limit);
    if (px_limit == 0) {
      return JXL_FAILURE("Too many pixels covered with splines");
    }
  }
  // TODO(eustas): consider linear sorting here.
  std::sort(segments_by_y.begin(), segments_by_y.end());
  segment_indices_.resize(segments_by_y.size());
  segment_y_start_.resize(image_ysize + 1);
  for (size_t i = 0; i < segments_by_y.size(); i++) {
    segment_indices_[i] = segments_by_y[i].second;
    size_t y = segments_by_y[i].first;
    if (y < image_ysize) {
      segment_y_start_[y + 1]++;
    }
  }
  for (size_t y = 0; y < image_ysize; y++) {
    segment_y_start_[y + 1] += segment_y_start_[y];
  }
  return true;
}

template <bool add>
void Splines::ApplyToRow(float* JXL_RESTRICT row_x, float* JXL_RESTRICT row_y,
                         float* JXL_RESTRICT row_b,
                         const Rect& image_row) const {
  if (segments_.empty()) return;
  JXL_ASSERT(image_row.ysize() == 1);
  for (size_t iy = 0; iy < image_row.ysize(); iy++) {
    HWY_DYNAMIC_DISPATCH(DrawSegments)
    (row_x, row_y, row_b, image_row.Line(iy), add, segments_.data(),
     segment_indices_.data(), segment_y_start_.data());
  }
}

template <bool add>
void Splines::Apply(Image3F* const opsin, const Rect& opsin_rect,
                    const Rect& image_rect) const {
  if (segments_.empty()) return;
  for (size_t iy = 0; iy < image_rect.ysize(); iy++) {
    const size_t y0 = opsin_rect.Line(iy).y0();
    const size_t x0 = opsin_rect.x0();
    ApplyToRow<add>(opsin->PlaneRow(0, y0) + x0, opsin->PlaneRow(1, y0) + x0,
                    opsin->PlaneRow(2, y0) + x0, image_rect.Line(iy));
  }
}

}  // namespace jxl
#endif  // HWY_ONCE
