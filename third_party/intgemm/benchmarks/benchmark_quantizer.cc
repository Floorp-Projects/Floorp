#include "../intgemm/intgemm.h"
#include "../intgemm/aligned.h"
#include "../intgemm/ssse3_gemm.h"
#include "../intgemm/avx2_gemm.h"
#include "../intgemm/avx512_gemm.h"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <random>
#include <vector>

namespace {

float MaxAbsoluteBaseline(const float *begin, const float *end) {
  auto res = std::minmax_element(begin, end);
  return std::max(std::fabs(*res.first), std::fabs(*res.second));
}

void BenchmarkMaxAbsolute() {
  std::mt19937 gen;
  std::uniform_real_distribution<float> dist(0.f, 1.f);
  gen.seed(45678);

  intgemm::AlignedVector<float> v(4096 * 4096);
  for (auto& it : v) {
    it = dist(gen);
  }

  // Hopefully these don't get optimized out...
  MaxAbsoluteBaseline(v.begin(), v.end());
  auto start = std::chrono::steady_clock::now();
  MaxAbsoluteBaseline(v.begin(), v.end());
  double baseline = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
  intgemm::MaxAbsolute(v.begin(), v.end());
  start = std::chrono::steady_clock::now();
  intgemm::MaxAbsolute(v.begin(), v.end());
  double optimized = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
  std::cout << "MaxAbsolute baseline = " << baseline << " optimized = " << optimized << " speedup = " << (optimized / baseline) << '\n';
}

template <class Backend> void QuantizerBench(const float *in, int8_t *out, intgemm::Index count) {
  if (intgemm::kCPU < Backend::kUses) return;
  Backend::Quantize(in, out, 1.0, count);
  const std::size_t kTries = 60;
  auto start = std::chrono::steady_clock::now();
  for (std::size_t t = 0; t < kTries; ++t) {
    Backend::Quantize(in, out, 1.0, count);
  }
  auto end = std::chrono::steady_clock::now();
  double took = std::chrono::duration<double>(end - start).count() / kTries;
  std::cout << std::setw(9) << count << ' ' << std::fixed << std::setw(9) << std::setprecision(7) << took << ' ' << Backend::kName << std::endl;
}
} // namespace

int main() {
  BenchmarkMaxAbsolute();
  for (std::size_t count = 1; count < (1ULL<<30); count *= 2) {
    intgemm::AlignedVector<float> in(count);
    intgemm::AlignedVector<int8_t> out(count);
    std::mt19937 gen;
    std::uniform_real_distribution<float> dist(-129.0, 129.0);
    for (float &element : in) {
      element = dist(gen);
    }
    QuantizerBench<intgemm::SSSE3::Kernels8>(in.begin(), out.begin(), static_cast<intgemm::Index>(count));
#ifdef INTGEMM_COMPILER_SUPPORTS_AVX2
    QuantizerBench<intgemm::AVX2::Kernels8>(in.begin(), out.begin(), static_cast<intgemm::Index>(count));
#endif
#ifdef INTGEMM_COMPILER_SUPPORTS_AVX512BW
    QuantizerBench<intgemm::AVX512BW::Kernels8>(in.begin(), out.begin(), static_cast<intgemm::Index>(count));
#endif
  }
}
