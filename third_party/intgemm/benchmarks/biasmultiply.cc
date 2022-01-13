#include "../intgemm/intgemm.h"
#include "../intgemm/aligned.h"
#include <chrono>
#include <random>
#include <iostream>

using namespace intgemm;

template <class Routine>
void testOld(Index /*rows*/, Index /*cols*/) {
}

template <class Routine>
std::chrono::duration<double> testNew(Index A_rows, Index width, Index B_cols) {
  AlignedVector<float> A(A_rows * width);
  AlignedVector<float> B(width * B_cols);
  AlignedVector<float> bias(B_cols);
  std::mt19937 gen;
  std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
  for (auto& it : A) {
    it = dist(gen);
  }
  for (auto& it : B) {
    it = dist(gen);
  }
  for (auto& it : bias) {
    it = dist(gen);
  }

  float alpha = 2.0f;
  float quant_mult = 127.0f / alpha;
  float unquant_mult = 1.0f / (quant_mult*quant_mult);

  AlignedVector<uint8_t> A_prep(A.size());
  AlignedVector<int8_t> B_prep(B.size());
  Routine::PrepareA(A.begin(), A_prep.begin(), quant_mult, A_rows, width);
  Routine::PrepareB(B.begin(), B_prep.begin(), quant_mult, width, B_cols);

  AlignedVector<float> test_C(A_rows * B_cols);

  float unquant_mult_forprep = (-1)*(alpha)*(alpha)/(127.0f); //Minus one to invert add_ps later on
  Routine::PrepareBias(B_prep.begin(), width, B_cols, callbacks::UnquantizeAndAddBiasAndWrite(unquant_mult_forprep, bias.begin(), bias.begin()));
  auto start = std::chrono::system_clock::now();
  Routine::Multiply8Shift(A_prep.begin(), B_prep.begin(), A_rows, width, B_cols, callbacks::UnquantizeAndAddBiasAndWrite(unquant_mult, bias.begin(), test_C.begin()));
  auto end = std::chrono::system_clock::now();

  std::chrono::duration<double> elapsed_seconds = end-start;
  return elapsed_seconds;

}

template <class Routine>
std::chrono::duration<double> testOld(Index A_rows, Index width, Index B_cols) {
  AlignedVector<float> A(A_rows * width);
  AlignedVector<float> B(width * B_cols);
  AlignedVector<float> bias(B_cols);
  std::mt19937 gen;
  std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
  for (auto& it : A) {
    it = dist(gen);
  }
  for (auto& it : B) {
    it = dist(gen);
  }
  for (auto& it : bias) {
    it = dist(gen);
  }

  float alpha = 2.0f;
  float quant_mult = 127.0f / alpha;
  float unquant_mult = 1.0f / (quant_mult*quant_mult);

  AlignedVector<int8_t> A_prep(A.size());
  AlignedVector<int8_t> B_prep(B.size());
  Routine::PrepareA(A.begin(), A_prep.begin(), quant_mult, A_rows, width);
  Routine::PrepareB(B.begin(), B_prep.begin(), quant_mult, width, B_cols);

  AlignedVector<float> test_C(A_rows * B_cols);

  auto start = std::chrono::system_clock::now();
  Routine::Multiply(A_prep.begin(), B_prep.begin(), A_rows, width, B_cols, callbacks::UnquantizeAndAddBiasAndWrite(unquant_mult, bias.begin(), test_C.begin()));
  auto end = std::chrono::system_clock::now();

  std::chrono::duration<double> elapsed_seconds = end-start;
  return elapsed_seconds;

}

template <class Routine>
std::chrono::duration<double> testOld_nobias(Index A_rows, Index width, Index B_cols) {
  AlignedVector<float> A(A_rows * width);
  AlignedVector<float> B(width * B_cols);
  std::mt19937 gen;
  std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
  for (auto& it : A) {
    it = dist(gen);
  }
  for (auto& it : B) {
    it = dist(gen);
  }

  float alpha = 2.0f;
  float quant_mult = 127.0f / alpha;
  float unquant_mult = 1.0f / (quant_mult*quant_mult);

  AlignedVector<int8_t> A_prep(A.size());
  AlignedVector<int8_t> B_prep(B.size());
  Routine::PrepareA(A.begin(), A_prep.begin(), quant_mult, A_rows, width);
  Routine::PrepareB(B.begin(), B_prep.begin(), quant_mult, width, B_cols);

  AlignedVector<float> test_C(A_rows * B_cols);

  auto start = std::chrono::system_clock::now();
  Routine::Multiply(A_prep.begin(), B_prep.begin(), A_rows, width, B_cols, callbacks::UnquantizeAndWrite(unquant_mult, test_C.begin()));
  auto end = std::chrono::system_clock::now();

  std::chrono::duration<double> elapsed_seconds = end-start;
  return elapsed_seconds;

}

int main(int argc, char ** argv) {
	int repeat = 1000;
	if (argc > 1) {
		repeat = atoi(argv[1]);
	}

	std::chrono::duration<double> oldSSSE3_nobias = testOld_nobias<SSSE3::Kernels8>(1, 64, 8);
	for (int i = 0; i<repeat; i++) {
		oldSSSE3_nobias += testOld_nobias<SSSE3::Kernels8>(8, 256, 256);
		oldSSSE3_nobias += testOld_nobias<SSSE3::Kernels8>(8, 2048, 256);
		oldSSSE3_nobias += testOld_nobias<SSSE3::Kernels8>(320, 256, 256);
		oldSSSE3_nobias += testOld_nobias<SSSE3::Kernels8>(472, 256, 256);
		oldSSSE3_nobias += testOld_nobias<SSSE3::Kernels8>(248, 256, 256);
		oldSSSE3_nobias += testOld_nobias<SSSE3::Kernels8>(200, 256, 256);
	}

	std::cout << repeat << " iterations of SSSE3 without bias took: " << oldSSSE3_nobias.count() << " seconds." << std::endl;

	std::chrono::duration<double> oldSSSE3 = testOld<SSSE3::Kernels8>(1, 64, 8);
	for (int i = 0; i<repeat; i++) {
		oldSSSE3 += testOld<SSSE3::Kernels8>(8, 256, 256);
		oldSSSE3 += testOld<SSSE3::Kernels8>(8, 2048, 256);
		oldSSSE3 += testOld<SSSE3::Kernels8>(320, 256, 256);
		oldSSSE3 += testOld<SSSE3::Kernels8>(472, 256, 256);
		oldSSSE3 += testOld<SSSE3::Kernels8>(248, 256, 256);
		oldSSSE3 += testOld<SSSE3::Kernels8>(200, 256, 256);
	}

	std::cout << repeat << " iterations of SSSE3 took: " << oldSSSE3.count() << " seconds." << std::endl;

	std::chrono::duration<double> newTimeSSSE3 = testOld<SSSE3::Kernels8>(1, 64, 8);
	for (int i = 0; i<repeat; i++) {
		newTimeSSSE3 += testNew<SSSE3::Kernels8>(8, 256, 256);
		newTimeSSSE3 += testNew<SSSE3::Kernels8>(8, 2048, 256);
		newTimeSSSE3 += testNew<SSSE3::Kernels8>(320, 256, 256);
		newTimeSSSE3 += testNew<SSSE3::Kernels8>(472, 256, 256);
		newTimeSSSE3 += testNew<SSSE3::Kernels8>(248, 256, 256);
		newTimeSSSE3 += testNew<SSSE3::Kernels8>(200, 256, 256);
	}

	std::cout << repeat << " iterations of Shifted SSSE3 took: " << newTimeSSSE3.count() << " seconds." << std::endl;

#ifdef INTGEMM_COMPILER_SUPPORTS_AVX2
	std::chrono::duration<double> oldAVX2_nobias = testOld_nobias<AVX2::Kernels8>(1, 64, 8);
	for (int i = 0; i<repeat; i++) {
		oldAVX2_nobias += testOld_nobias<AVX2::Kernels8>(8, 256, 256);
		oldAVX2_nobias += testOld_nobias<AVX2::Kernels8>(8, 2048, 256);
		oldAVX2_nobias += testOld_nobias<AVX2::Kernels8>(320, 256, 256);
		oldAVX2_nobias += testOld_nobias<AVX2::Kernels8>(472, 256, 256);
		oldAVX2_nobias += testOld_nobias<AVX2::Kernels8>(248, 256, 256);
		oldAVX2_nobias += testOld_nobias<AVX2::Kernels8>(200, 256, 256);
	}

	std::cout << repeat << " iterations of AVX2 without bias took: " << oldAVX2_nobias.count() << " seconds." << std::endl;

	std::chrono::duration<double> oldAVX2 = testOld<AVX2::Kernels8>(1, 64, 8);
	for (int i = 0; i<repeat; i++) {
		oldAVX2 += testOld<AVX2::Kernels8>(8, 256, 256);
		oldAVX2 += testOld<AVX2::Kernels8>(8, 2048, 256);
		oldAVX2 += testOld<AVX2::Kernels8>(320, 256, 256);
		oldAVX2 += testOld<AVX2::Kernels8>(472, 256, 256);
		oldAVX2 += testOld<AVX2::Kernels8>(248, 256, 256);
		oldAVX2 += testOld<AVX2::Kernels8>(200, 256, 256);
	}

	std::cout << repeat << " iterations of AVX2 took: " << oldAVX2.count() << " seconds." << std::endl;

	std::chrono::duration<double> newTimeAVX2 = testOld<AVX2::Kernels8>(1, 64, 8);
	for (int i = 0; i<repeat; i++) {
		newTimeAVX2 += testNew<AVX2::Kernels8>(8, 256, 256);
		newTimeAVX2 += testNew<AVX2::Kernels8>(8, 2048, 256);
		newTimeAVX2 += testNew<AVX2::Kernels8>(320, 256, 256);
		newTimeAVX2 += testNew<AVX2::Kernels8>(472, 256, 256);
		newTimeAVX2 += testNew<AVX2::Kernels8>(248, 256, 256);
		newTimeAVX2 += testNew<AVX2::Kernels8>(200, 256, 256);
	}

	std::cout << repeat << " iterations of Shifted AVX2 took: " << newTimeAVX2.count() << " seconds." << std::endl;
#endif
#ifdef INTGEMM_COMPILER_SUPPORTS_AVX512BW
	if (kCPU < CPUType::AVX512BW) return 0;
	std::chrono::duration<double> oldAVX512_nobias = testOld_nobias<AVX512BW::Kernels8>(1, 64, 8);
	for (int i = 0; i<repeat; i++) {
		oldAVX512_nobias += testOld_nobias<AVX512BW::Kernels8>(8, 256, 256);
		oldAVX512_nobias += testOld_nobias<AVX512BW::Kernels8>(8, 2048, 256);
		oldAVX512_nobias += testOld_nobias<AVX512BW::Kernels8>(320, 256, 256);
		oldAVX512_nobias += testOld_nobias<AVX512BW::Kernels8>(472, 256, 256);
		oldAVX512_nobias += testOld_nobias<AVX512BW::Kernels8>(248, 256, 256);
		oldAVX512_nobias += testOld_nobias<AVX512BW::Kernels8>(200, 256, 256);
	}

	std::cout << repeat << " iterations of AVX512 without bias took: " << oldAVX512_nobias.count() << " seconds." << std::endl;

	std::chrono::duration<double> oldAVX512 = testOld<AVX512BW::Kernels8>(1, 64, 8);
	for (int i = 0; i<repeat; i++) {
		oldAVX512 += testOld<AVX512BW::Kernels8>(8, 256, 256);
		oldAVX512 += testOld<AVX512BW::Kernels8>(8, 2048, 256);
		oldAVX512 += testOld<AVX512BW::Kernels8>(320, 256, 256);
		oldAVX512 += testOld<AVX512BW::Kernels8>(472, 256, 256);
		oldAVX512 += testOld<AVX512BW::Kernels8>(248, 256, 256);
		oldAVX512 += testOld<AVX512BW::Kernels8>(200, 256, 256);
	}

	std::cout << repeat << " iterations of AVX512 took: " << oldAVX512.count() << " seconds." << std::endl;

	std::chrono::duration<double> newTimeAVX512 = testOld<AVX512BW::Kernels8>(1, 64, 8);
	for (int i = 0; i<repeat; i++) {
		newTimeAVX512 += testNew<AVX512BW::Kernels8>(8, 256, 256);
		newTimeAVX512 += testNew<AVX512BW::Kernels8>(8, 2048, 256);
		newTimeAVX512 += testNew<AVX512BW::Kernels8>(320, 256, 256);
		newTimeAVX512 += testNew<AVX512BW::Kernels8>(472, 256, 256);
		newTimeAVX512 += testNew<AVX512BW::Kernels8>(248, 256, 256);
		newTimeAVX512 += testNew<AVX512BW::Kernels8>(200, 256, 256);
	}

	std::cout << repeat << " iterations of Shifted AVX512 took: " << newTimeAVX512.count() << " seconds." << std::endl;
#endif
#ifdef INTGEMM_COMPILER_SUPPORTS_AVX512VNNI
  if (kCPU < CPUType::AVX512VNNI) return 0;
  std::chrono::duration<double> oldAVX512VNNI_nobias = testOld_nobias<AVX512BW::Kernels8>(1, 64, 8);
  for (int i = 0; i<repeat; i++) {
          oldAVX512VNNI_nobias += testOld_nobias<AVX512VNNI::Kernels8>(8, 256, 256);
          oldAVX512VNNI_nobias += testOld_nobias<AVX512VNNI::Kernels8>(8, 2048, 256);
          oldAVX512VNNI_nobias += testOld_nobias<AVX512VNNI::Kernels8>(320, 256, 256);
          oldAVX512VNNI_nobias += testOld_nobias<AVX512VNNI::Kernels8>(472, 256, 256);
          oldAVX512VNNI_nobias += testOld_nobias<AVX512VNNI::Kernels8>(248, 256, 256);
          oldAVX512VNNI_nobias += testOld_nobias<AVX512VNNI::Kernels8>(200, 256, 256);
  }

  std::cout << repeat << " iterations of AVX512VNNI without bias took: " << oldAVX512VNNI_nobias.count() << " seconds." << std::endl;

  std::chrono::duration<double> oldAVX512VNNI = testOld<AVX512BW::Kernels8>(1, 64, 8);
  for (int i = 0; i<repeat; i++) {
          oldAVX512VNNI += testOld<AVX512VNNI::Kernels8>(8, 256, 256);
          oldAVX512VNNI += testOld<AVX512VNNI::Kernels8>(8, 2048, 256);
          oldAVX512VNNI += testOld<AVX512VNNI::Kernels8>(320, 256, 256);
          oldAVX512VNNI += testOld<AVX512VNNI::Kernels8>(472, 256, 256);
          oldAVX512VNNI += testOld<AVX512VNNI::Kernels8>(248, 256, 256);
          oldAVX512VNNI += testOld<AVX512VNNI::Kernels8>(200, 256, 256);
  }

  std::cout << repeat << " iterations of AVX512VNNI took: " << oldAVX512VNNI.count() << " seconds." << std::endl;

  std::chrono::duration<double> newTimeAVX512VNNI = testOld<AVX512BW::Kernels8>(1, 64, 8);
  for (int i = 0; i<repeat; i++) {
    newTimeAVX512VNNI += testNew<AVX512VNNI::Kernels8>(8, 256, 256);
    newTimeAVX512VNNI += testNew<AVX512VNNI::Kernels8>(8, 2048, 256);
    newTimeAVX512VNNI += testNew<AVX512VNNI::Kernels8>(320, 256, 256);
    newTimeAVX512VNNI += testNew<AVX512VNNI::Kernels8>(472, 256, 256);
    newTimeAVX512VNNI += testNew<AVX512VNNI::Kernels8>(248, 256, 256);
    newTimeAVX512VNNI += testNew<AVX512VNNI::Kernels8>(200, 256, 256);
  }

  std::cout << repeat << " iterations of Shifted AVX512VNNI took: " << newTimeAVX512VNNI.count() << " seconds." << std::endl;
#endif

}
