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

// Given a set of DCT coefficients, this returns the result of performing cosine
// interpolation on the original samples.
float ContinuousIDCT(const float dct[32], float t) {
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
    auto cos_arg = LoadU(df, kMultipliers + i) * tandhalf;
    auto cos = FastCosf(df, cos_arg);
    auto local_res = LoadU(df, dct + i) * cos;
    result = MulAdd(Set(df, square_root<2>::value), local_res, result);
  }
  return GetLane(SumOfLanes(result));
}

template <typename DF>
void DrawSegment(DF df, const SplineSegment& segment, bool add, size_t y,
                 size_t x, float* JXL_RESTRICT rows[3]) {
  Rebind<int32_t, DF> di;
  const auto inv_sigma = Set(df, segment.inv_sigma);
  const auto half = Set(df, 0.5f);
  const auto one_over_2s2 = Set(df, 0.353553391f);
  const auto sigma_over_4_times_intensity =
      Set(df, segment.sigma_over_4_times_intensity);
  const auto dx = ConvertTo(df, Iota(di, x)) - Set(df, segment.center_x);
  const auto dy = Set(df, y - segment.center_y);
  const auto sqd = MulAdd(dx, dx, dy * dy);
  const auto distance = Sqrt(sqd);
  const auto one_dimensional_factor =
      FastErff(df, MulAdd(distance, half, one_over_2s2) * inv_sigma) -
      FastErff(df, MulSub(distance, half, one_over_2s2) * inv_sigma);
  auto local_intensity = sigma_over_4_times_intensity * one_dimensional_factor *
                         one_dimensional_factor;
  for (size_t c = 0; c < 3; ++c) {
    const auto cm = Set(df, add ? segment.color[c] : -segment.color[c]);
    const auto in = LoadU(df, rows[c] + x);
    StoreU(MulAdd(cm, local_intensity, in), df, rows[c] + x);
  }
}

void DrawSegment(const SplineSegment& segment, bool add, size_t y, ssize_t x0,
                 ssize_t x1, float* JXL_RESTRICT rows[3]) {
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
                     std::vector<std::pair<size_t, size_t>>& segments_by_y) {
  // Sanity check sigma, inverse sigma and intensity
  if (!(std::isfinite(sigma) && sigma != 0.0f && std::isfinite(1.0f / sigma) &&
        std::isfinite(intensity))) {
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
  ssize_t y0 = center.y - maximum_distance + .5f;
  ssize_t y1 = center.y + maximum_distance + 1.5f;  // one-past-the-end
  for (ssize_t y = std::max<ssize_t>(y0, 0); y < y1; y++) {
    segments_by_y.emplace_back(y, segments.size());
  }
  segments.push_back(segment);
}

void DrawSegments(Image3F* const opsin, const Rect& opsin_rect,
                  const Rect& image_rect, bool add,
                  const SplineSegment* segments, const size_t* segment_indices,
                  const size_t* segment_y_start) {
  JXL_ASSERT(image_rect.ysize() == 1);
  float* JXL_RESTRICT rows[3] = {
      opsin_rect.PlaneRow(opsin, 0, 0) - image_rect.x0(),
      opsin_rect.PlaneRow(opsin, 1, 0) - image_rect.x0(),
      opsin_rect.PlaneRow(opsin, 2, 0) - image_rect.x0(),
  };
  size_t y = image_rect.y0();
  for (size_t i = segment_y_start[y]; i < segment_y_start[y + 1]; i++) {
    DrawSegment(segments[segment_indices[i]], add, y, image_rect.x0(),
                image_rect.x0() + image_rect.xsize(), rows);
  }
}

void SegmentsFromPoints(
    const Spline& spline,
    const std::vector<std::pair<Spline::Point, float>>& points_to_draw,
    float arc_length, std::vector<SplineSegment>& segments,
    std::vector<std::pair<size_t, size_t>>& segments_by_y) {
  float inv_arc_length = 1.0f / arc_length;
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
    ComputeSegments(point, multiplier, color, sigma, segments, segments_by_y);
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

// Maximum number of spline control points per frame is
//   std::min(kMaxNumControlPoints, xsize * ysize / 2)
constexpr size_t kMaxNumControlPoints = 1u << 20u;
constexpr size_t kMaxNumControlPointsPerPixelRatio = 2;

// X, Y, B, sigma.
float ColorQuantizationWeight(const int32_t adjustment, const int channel,
                              const int i) {
  const float multiplier = adjustment >= 0 ? 1.f + .125f * adjustment
                                           : 1.f / (1.f + .125f * -adjustment);

  static constexpr float kChannelWeight[] = {0.0042f, 0.075f, 0.07f, .3333f};

  return multiplier / kChannelWeight[channel];
}

Status DecodeAllStartingPoints(std::vector<Spline::Point>* const points,
                               BitReader* const br, ANSSymbolReader* reader,
                               const std::vector<uint8_t>& context_map,
                               size_t num_splines) {
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

std::vector<Spline::Point> DrawCentripetalCatmullRomSpline(
    std::vector<Spline::Point> points) {
  if (points.size() <= 1) return points;
  // Number of points to compute between each control point.
  static constexpr int kNumPoints = 16;
  std::vector<Spline::Point> result;
  result.reserve((points.size() - 1) * kNumPoints + 1);
  points.insert(points.begin(), points[0] + (points[0] - points[1]));
  points.push_back(points[points.size() - 1] +
                   (points[points.size() - 1] - points[points.size() - 2]));
  // points has at least 4 elements at this point.
  for (size_t start = 0; start < points.size() - 3; ++start) {
    // 4 of them are used, and we draw from p[1] to p[2].
    const Spline::Point* const p = &points[start];
    result.push_back(p[1]);
    float t[4] = {0};
    for (int k = 1; k < 4; ++k) {
      t[k] = std::sqrt(hypotf(p[k].x - p[k - 1].x, p[k].y - p[k - 1].y)) +
             t[k - 1];
    }
    for (int i = 1; i < kNumPoints; ++i) {
      const float tt =
          t[1] + (static_cast<float>(i) / kNumPoints) * (t[2] - t[1]);
      Spline::Point a[3];
      for (int k = 0; k < 3; ++k) {
        a[k] = p[k] + ((tt - t[k]) / (t[k + 1] - t[k])) * (p[k + 1] - p[k]);
      }
      Spline::Point b[2];
      for (int k = 0; k < 2; ++k) {
        b[k] = a[k] + ((tt - t[k]) / (t[k + 2] - t[k])) * (a[k + 1] - a[k]);
      }
      result.push_back(b[0] + ((tt - t[1]) / (t[2] - t[1])) * (b[1] - b[0]));
    }
  }
  result.push_back(points[points.size() - 2]);
  return result;
}

// Move along the line segments defined by `points`, `kDesiredRenderingDistance`
// pixels at a time, and call `functor` with each point and the actual distance
// to the previous point (which will always be kDesiredRenderingDistance except
// possibly for the very last point).
template <typename Points, typename Functor>
void ForEachEquallySpacedPoint(const Points& points, const Functor& functor) {
  JXL_ASSERT(!points.empty());
  Spline::Point current = points.front();
  functor(current, kDesiredRenderingDistance);
  auto next = points.begin();
  while (next != points.end()) {
    const Spline::Point* previous = &current;
    float arclength_from_previous = 0.f;
    for (;;) {
      if (next == points.end()) {
        functor(*previous, arclength_from_previous);
        return;
      }
      const float arclength_to_next =
          std::sqrt((*next - *previous).SquaredNorm());
      if (arclength_from_previous + arclength_to_next >=
          kDesiredRenderingDistance) {
        current =
            *previous + ((kDesiredRenderingDistance - arclength_from_previous) /
                         arclength_to_next) *
                            (*next - *previous);
        functor(current, kDesiredRenderingDistance);
        break;
      }
      arclength_from_previous += arclength_to_next;
      previous = &*next;
      ++next;
    }
  }
}

}  // namespace

QuantizedSpline::QuantizedSpline(const Spline& original,
                                 const int32_t quantization_adjustment,
                                 float ytox, float ytob) {
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

  for (int c = 0; c < 3; ++c) {
    float factor = c == 0 ? ytox : c == 1 ? 0 : ytob;
    for (int i = 0; i < 32; ++i) {
      const float coefficient =
          original.color_dct[c][i] -
          factor * color_dct_[1][i] /
              ColorQuantizationWeight(quantization_adjustment, 1, i);
      color_dct_[c][i] = static_cast<int>(
          roundf(coefficient *
                 ColorQuantizationWeight(quantization_adjustment, c, i)));
    }
  }
  for (int i = 0; i < 32; ++i) {
    sigma_dct_[i] = static_cast<int>(
        roundf(original.sigma_dct[i] *
               ColorQuantizationWeight(quantization_adjustment, 3, i)));
  }
}

Spline QuantizedSpline::Dequantize(const Spline::Point& starting_point,
                                   const int32_t quantization_adjustment,
                                   float ytox, float ytob) const {
  Spline result;

  result.control_points.reserve(control_points_.size() + 1);
  int current_x = static_cast<int>(roundf(starting_point.x)),
      current_y = static_cast<int>(roundf(starting_point.y));
  result.control_points.push_back(Spline::Point{static_cast<float>(current_x),
                                                static_cast<float>(current_y)});
  int current_delta_x = 0, current_delta_y = 0;
  for (const auto& point : control_points_) {
    current_delta_x += point.first;
    current_delta_y += point.second;
    current_x += current_delta_x;
    current_y += current_delta_y;
    result.control_points.push_back(Spline::Point{
        static_cast<float>(current_x), static_cast<float>(current_y)});
  }

  for (int c = 0; c < 3; ++c) {
    for (int i = 0; i < 32; ++i) {
      result.color_dct[c][i] =
          color_dct_[c][i] * (i == 0 ? 1.0f / square_root<2>::value : 1.0f) /
          ColorQuantizationWeight(quantization_adjustment, c, i);
    }
  }
  for (int i = 0; i < 32; ++i) {
    result.color_dct[0][i] += ytox * result.color_dct[1][i];
    result.color_dct[2][i] += ytob * result.color_dct[1][i];
  }
  for (int i = 0; i < 32; ++i) {
    result.sigma_dct[i] =
        sigma_dct_[i] * (i == 0 ? 1.0f / square_root<2>::value : 1.0f) /
        ColorQuantizationWeight(quantization_adjustment, 3, i);
  }

  return result;
}

Status QuantizedSpline::Decode(const std::vector<uint8_t>& context_map,
                               ANSSymbolReader* const decoder,
                               BitReader* const br, size_t max_control_points,
                               size_t* total_num_control_points) {
  const size_t num_control_points =
      decoder->ReadHybridUint(kNumControlPointsContext, br, context_map);
  *total_num_control_points += num_control_points;
  if (*total_num_control_points > max_control_points) {
    return JXL_FAILURE("Too many control points: %" PRIuS,
                       *total_num_control_points);
  }
  control_points_.resize(num_control_points);
  for (std::pair<int64_t, int64_t>& control_point : control_points_) {
    control_point.first = UnpackSigned(
        decoder->ReadHybridUint(kControlPointsContext, br, context_map));
    control_point.second = UnpackSigned(
        decoder->ReadHybridUint(kControlPointsContext, br, context_map));
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

Status Splines::Decode(jxl::BitReader* br, size_t num_pixels) {
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

void Splines::SubtractFrom(Image3F* const opsin) const {
  return Apply</*add=*/false>(opsin, Rect(*opsin), Rect(*opsin));
}

Status Splines::InitializeDrawCache(size_t image_xsize, size_t image_ysize,
                                    const ColorCorrelationMap& cmap) {
  // TODO(veluca): avoid storing segments that are entirely outside image
  // boundaries.
  segments_.clear();
  segment_indices_.clear();
  segment_y_start_.clear();
  std::vector<std::pair<size_t, size_t>> segments_by_y;
  for (size_t i = 0; i < splines_.size(); ++i) {
    const Spline spline =
        splines_[i].Dequantize(starting_points_[i], quantization_adjustment_,
                               cmap.YtoXRatio(0), cmap.YtoBRatio(0));
    if (std::adjacent_find(spline.control_points.begin(),
                           spline.control_points.end()) !=
        spline.control_points.end()) {
      return JXL_FAILURE(
          "identical successive control points in spline %" PRIuS, i);
    }
    std::vector<std::pair<Spline::Point, float>> points_to_draw;
    ForEachEquallySpacedPoint(
        DrawCentripetalCatmullRomSpline(spline.control_points),
        [&](const Spline::Point& point, const float multiplier) {
          points_to_draw.emplace_back(point, multiplier);
        });
    const float arc_length =
        (points_to_draw.size() - 2) * kDesiredRenderingDistance +
        points_to_draw.back().second;
    if (arc_length <= 0.f) {
      // This spline wouldn't have any effect.
      continue;
    }
    HWY_DYNAMIC_DISPATCH(SegmentsFromPoints)
    (spline, points_to_draw, arc_length, segments_, segments_by_y);
  }
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
void Splines::Apply(Image3F* const opsin, const Rect& opsin_rect,
                    const Rect& image_rect) const {
  if (segments_.empty()) return;
  for (size_t iy = 0; iy < image_rect.ysize(); iy++) {
    HWY_DYNAMIC_DISPATCH(DrawSegments)
    (opsin, opsin_rect.Line(iy), image_rect.Line(iy), add, segments_.data(),
     segment_indices_.data(), segment_y_start_.data());
  }
}

}  // namespace jxl
#endif  // HWY_ONCE
