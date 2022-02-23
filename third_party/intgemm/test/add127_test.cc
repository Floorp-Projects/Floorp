#include "test.h"

namespace intgemm {
namespace {

void CompareAs(int8_t * output_old, uint8_t * output_new, Index rows, Index cols) {
	for (Index r = 0; r<rows; r++) {
		for (Index c = 0; c<cols; c++) {
			int a = int(output_old[rows*c + r]);
			int b = int(output_new[rows*c + r]);
			INFO("Inaccurate at row: " << r << " column " << c << ' '
			 << a << ' ' << b);
			CHECK(a+127 == b);
		}
	}
}

template <class Routine> void TestPrepareA(Index rows, Index cols) {
  std::mt19937 gen;
  // Go somewhat out of range too.
  std::uniform_real_distribution<float> dist(-2, 2);
  // Create array.
  AlignedVector<float> inputA(rows * cols);
  for (auto& it : inputA) {
    it = dist(gen);
  }
  AlignedVector<int8_t> oldA(rows * cols);
  AlignedVector<uint8_t> newA(rows * cols);
  float quant_mult = 64; //From example
  Routine::PrepareA(inputA.begin(), oldA.begin(), quant_mult, rows, cols);
  Routine::PrepareA(inputA.begin(), newA.begin(), quant_mult, rows, cols);
  CompareAs(oldA.begin(), newA.begin(), rows, cols);
}

template <class Routine> void TestPrepareBias(Index rows, Index cols) {
  std::mt19937 gen;
  // Go somewhat out of range too.
  std::uniform_real_distribution<float> dist(-30.0, 30.0);
  // Create array.
  AlignedVector<float> inputB(rows * cols);
  for (auto& it : inputB) {
    it = dist(gen);
  }

  float alpha = 25;
  float quant_mult = 127/alpha;

  AlignedVector<int8_t> B_prep(inputB.size());
  AlignedVector<int8_t> B_quant(inputB.size());
  Routine::PrepareB(inputB.begin(), B_prep.begin(), quant_mult, rows, cols);
  Routine::Quantize(inputB.begin(), B_quant.begin(), quant_mult, static_cast<intgemm::Index>(inputB.size()));


  AlignedVector<float> inputBias(cols);
  AlignedVector<float> goldBias(cols);

  for (auto& it : goldBias) {
  	it = dist(gen);
  }
  int i = 0;
  for (auto& it : inputBias) {
    it = goldBias[i];
    i++;
  }

  float unquant_mult_forprep = (-1)*(alpha)*(alpha)/(127.0f);

  Routine::PrepareBias(B_prep.begin(), rows, cols, callbacks::UnquantizeAndAddBiasAndWrite(unquant_mult_forprep, inputBias.begin(), inputBias.begin()));

  int A_rows = 1;
  AlignedVector<int8_t> A_prep2(A_rows*rows);
  for (auto& it : A_prep2) {
    it =1;
  }
  //Routine::Multiply(A_prep2.begin(), B_prep.begin(), A_rows, rows, cols, callbacks::UnquantizeAndAddBiasAndWrite(unquant_mult_forprep, goldBias.begin(), goldBias.begin()));
  //CompareEps(goldBias.begin(), inputBias.begin(), cols, 0.0001f);
  AlignedVector<float> slowint_C(cols);
  references::Multiply(A_prep2.begin(), B_quant.begin(), slowint_C.begin(), A_rows, rows, cols, [&](int32_t sum, const callbacks::OutputBufferInfo& info) {
    return sum * unquant_mult_forprep + goldBias[info.col_idx];
  });
  CompareEps(slowint_C.begin(), inputBias.begin(), cols, 0.0001f);
}

template <class Routine> void TestMultiplyBiasNew(Index A_rows, Index width, Index B_cols,
 float int_tolerance=.1, float float_tolerance=1, float MSE_float_tolerance=0, float MSE_int_tolerance=0) {
  std::ostringstream info;
  info << Routine::kName << "\t" << A_rows << '\t' << width << '\t' << B_cols << '\n';

  // Initialize A and B.
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

  /*REFERENCE MULTIPLICATION
  *
  *
  */
  AlignedVector<int8_t> B_quant(B.size());
  Routine::Quantize(B.begin(), B_quant.begin(), quant_mult, static_cast<Index>(B.size()));
  AlignedVector<float> slowint_C(test_C.size());
  // Taking the original A_preparation which means A would be int8_t
  AlignedVector<int8_t> A_prep2(A.size());
  Routine::PrepareA(A.begin(), A_prep2.begin(), quant_mult, A_rows, width);
  references::Multiply(A_prep2.begin(), B_quant.begin(), slowint_C.begin(), A_rows, width, B_cols, [&](int32_t sum, const callbacks::OutputBufferInfo& info) {
    return sum * unquant_mult + bias[info.col_idx];
  });

  AlignedVector<float> float_C(test_C.size());
  references::Multiply(A.begin(), B.begin(), float_C.begin(), A_rows, width, B_cols, [&](double sum, const callbacks::OutputBufferInfo& info) {
    return static_cast<float>(sum) + bias[info.col_idx];
  });

  /*ACTUAL MULTIPLICATION
  *
  */
  float unquant_mult_forprep = (-1.0f)*(alpha)*(alpha)/(127.0f); //Minus one to invert add_ps later on
  Routine::PrepareBias(B_prep.begin(), width, B_cols, callbacks::UnquantizeAndAddBiasAndWrite(unquant_mult_forprep, bias.begin(), bias.begin()));
  //Routine::PrepareBias(B.begin(), bias.begin(), alpha, width, B_cols);
  Routine::Multiply8Shift(A_prep.begin(), B_prep.begin(), A_rows, width, B_cols, callbacks::UnquantizeAndAddBiasAndWrite(unquant_mult, bias.begin(), test_C.begin()));

  CompareMSE(float_C.begin(), slowint_C.begin(), test_C.begin(), test_C.size(), info.str(),
   int_tolerance, float_tolerance, MSE_float_tolerance, MSE_int_tolerance);
}

template <class Routine> void TestMultiplyShiftNonShift(Index A_rows, Index width, Index B_cols,
 float int_tolerance=.1, float float_tolerance=1, float MSE_float_tolerance=0, float MSE_int_tolerance=0) {
  std::ostringstream info;
  info << Routine::kName << "\t" << A_rows << '\t' << width << '\t' << B_cols << '\n';

  // Initialize A and B.
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
    it = 0;
  }

  float alpha = 2.0f;
  float quant_mult = 127.0f / alpha;
  float unquant_mult = 1.0f / (quant_mult*quant_mult);

  AlignedVector<uint8_t> A_prep(A.size());
  AlignedVector<int8_t> A_prep_old(A.size());
  AlignedVector<int8_t> B_prep(B.size());
  Routine::PrepareA(A.begin(), A_prep.begin(), quant_mult, A_rows, width);
  Routine::PrepareA(A.begin(), A_prep_old.begin(), quant_mult, A_rows, width); //Non shited version
  Routine::PrepareB(B.begin(), B_prep.begin(), quant_mult, width, B_cols);

  AlignedVector<float> test_C(A_rows * B_cols);

  /*
   * Reference non shift multiplication instead of slowint
   */
  AlignedVector<float> slowint_C(test_C.size());
  Routine::Multiply(A_prep_old.begin(), B_prep.begin(), A_rows, width, B_cols, callbacks::UnquantizeAndAddBiasAndWrite(unquant_mult, bias.begin(), slowint_C.begin()));

  AlignedVector<float> float_C(test_C.size());
  references::Multiply(A.begin(), B.begin(), float_C.begin(), A_rows, width, B_cols, [&](double sum, const callbacks::OutputBufferInfo& info) {
    return static_cast<float>(sum) + bias[info.col_idx];
  });

  /*
   * Multiply8 shift multiplication
   */
  float unquant_mult_forprep = (-1.0f)*(alpha)*(alpha)/(127.0f); //Minus one to invert add_ps later on
  Routine::PrepareBias(B_prep.begin(), width, B_cols, callbacks::UnquantizeAndAddBiasAndWrite(unquant_mult_forprep, bias.begin(), bias.begin()));
  Routine::Multiply8Shift(A_prep.begin(), B_prep.begin(), A_rows, width, B_cols, callbacks::UnquantizeAndAddBiasAndWrite(unquant_mult, bias.begin(), test_C.begin()));

  CompareMSE(float_C.begin(), slowint_C.begin(), test_C.begin(), test_C.size(), info.str(),
   int_tolerance, float_tolerance, MSE_float_tolerance, MSE_int_tolerance);
}

template <class Routine> void TestMultiplyShiftInt(Index A_rows, Index width, Index B_cols,
 float int_tolerance=.1, float float_tolerance=1, float MSE_float_tolerance=0, float MSE_int_tolerance=0) {
  std::ostringstream info;
  info << Routine::kName << "\t" << A_rows << '\t' << width << '\t' << B_cols << '\n';

  // Initialize A and B.
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
    it = 0;
  }

  float alpha = 2.0f;
  float quant_mult = 127.0f / alpha;
  float unquant_mult = 1.0f / (quant_mult*quant_mult);

  AlignedVector<uint8_t> A_prep(A.size());
  AlignedVector<int8_t> A_prep_old(A.size());
  AlignedVector<int8_t> B_prep(B.size());
  Routine::PrepareA(A.begin(), A_prep.begin(), quant_mult, A_rows, width);
  Routine::PrepareA(A.begin(), A_prep_old.begin(), quant_mult, A_rows, width); //Non shited version
  Routine::PrepareB(B.begin(), B_prep.begin(), quant_mult, width, B_cols);

  AlignedVector<float> test_C(A_rows * B_cols);

  /*
   * Reference float multiplication
   */
  AlignedVector<int8_t> B_quant(B.size());
  Routine::Quantize(B.begin(), B_quant.begin(), quant_mult, static_cast<Index>(B.size()));
  AlignedVector<float> slowint_C(test_C.size());
  // Taking the original A_preparation which means A would be int8_t
  // references::Multiply(A_prep.begin(), B_quant.begin(), slowint_C.begin(), A_rows, width, B_cols, [&](int32_t sum, const callbacks::OutputBufferInfo& info) {
  //   return sum * unquant_mult + bias[info.col_idx];
  // });

  AlignedVector<float> float_C(test_C.size());
  references::Multiply(A.begin(), B.begin(), float_C.begin(), A_rows, width, B_cols, [&](double sum, const callbacks::OutputBufferInfo& info) {
    return static_cast<float>(sum) + bias[info.col_idx];
  });
  /*
   * Multiply8 shift multiplication
   */
  //First prepare SlowInteger Bias:
  AlignedVector<int8_t> A_prep2(1*width);
  for (auto& it : A_prep2) {
    it = 1;
  }
  AlignedVector<float> ShiftedBias(B_cols);
  float unquant_mult_forprep = (-1)*(alpha)*(alpha)/(127.0f); //Minus one to invert add_ps later on
  references::Multiply(A_prep2.begin(), B_quant.begin(), ShiftedBias.begin(), 1, width, B_cols, [&](int32_t sum, const callbacks::OutputBufferInfo& info) {
    return sum * unquant_mult_forprep + bias[info.col_idx];
  });
  

  //Now prepare Fast integer Bias
  Routine::PrepareBias(B_prep.begin(), width, B_cols, callbacks::UnquantizeAndAddBiasAndWrite(unquant_mult_forprep, bias.begin(), bias.begin()));
  Routine::Multiply8Shift(A_prep.begin(), B_prep.begin(), A_rows, width, B_cols, callbacks::UnquantizeAndAddBiasAndWrite(unquant_mult, bias.begin(), test_C.begin()));

  // Reference INT VERSION HERE with ADD127
  // Taking the original A_preparation which means A would be int8_t
  references::Multiply(A_prep.begin(), B_quant.begin(), slowint_C.begin(), A_rows, width, B_cols, [&](int32_t sum, const callbacks::OutputBufferInfo& info) {
    return sum * unquant_mult + ShiftedBias[info.col_idx];
  });

  CompareMSE(float_C.begin(), slowint_C.begin(), test_C.begin(), test_C.size(), info.str(),
   int_tolerance, float_tolerance, MSE_float_tolerance, MSE_int_tolerance);
}


// Bias
TEST_CASE("PrepareBias SSSE3", "[Add127]") {
	if (kCPU < CPUType::SSSE3) return;
	TestPrepareBias<SSSE3::Kernels8>(256,256);
	TestPrepareBias<SSSE3::Kernels8>(2048,256);
	TestPrepareBias<SSSE3::Kernels8>(512,512);
}

#ifdef INTGEMM_COMPILER_SUPPORTS_AVX2
TEST_CASE("PrepareBias AVX2", "[Add127]") {
	if (kCPU < CPUType::AVX2) return;
	TestPrepareBias<AVX2::Kernels8>(256,256);
	TestPrepareBias<AVX2::Kernels8>(2048,256);
	TestPrepareBias<AVX2::Kernels8>(512,512);
}
#endif

#ifdef INTGEMM_COMPILER_SUPPORTS_AVX512BW
TEST_CASE("PrepareBias AVX512F", "[Add127]") {
	if (kCPU < CPUType::AVX512BW) return;
	TestPrepareBias<AVX512BW::Kernels8>(256,256);
	TestPrepareBias<AVX512BW::Kernels8>(2048,256);
	TestPrepareBias<AVX512BW::Kernels8>(512,512);
}
#endif

//A
TEST_CASE("PrepareA SSSE3", "[Add127]") {
	if (kCPU < CPUType::SSSE3) return;
	TestPrepareA<SSSE3::Kernels8>(64,64);
	TestPrepareA<SSSE3::Kernels8>(256,256);
	TestPrepareA<SSSE3::Kernels8>(512,512);
  TestPrepareA<SSSE3::Kernels8>(2048,256);
}

#ifdef INTGEMM_COMPILER_SUPPORTS_AVX2
TEST_CASE("PrepareA AVX2", "[Add127]") {
	if (kCPU < CPUType::AVX2) return;
	TestPrepareA<AVX2::Kernels8>(64,64);
	TestPrepareA<AVX2::Kernels8>(256,256);
	TestPrepareA<AVX2::Kernels8>(512,512);
  TestPrepareA<AVX2::Kernels8>(2048,256);
}
#endif

#ifdef INTGEMM_COMPILER_SUPPORTS_AVX512BW
TEST_CASE("PrepareA AVX512F", "[Add127]") {
	if (kCPU < CPUType::AVX512BW) return;
	TestPrepareA<AVX512BW::Kernels8>(64,64);
	TestPrepareA<AVX512BW::Kernels8>(256,256);
	TestPrepareA<AVX512BW::Kernels8>(512,512);
  TestPrepareA<AVX512BW::Kernels8>(2048,256);
}
#endif

// Multiply

TEST_CASE ("Multiply SSSE3 8bit Shift with bias", "[Add127]") {
  if (kCPU < CPUType::SSSE3) return;
  TestMultiplyBiasNew<SSSE3::Kernels8>(1, 64, 8, 0.11f, 0.1f, 0.06f, 0.05f);
  TestMultiplyBiasNew<SSSE3::Kernels8>(8, 256, 256, 0.45f, 0.54f, 0.17f, 0.16f);
  TestMultiplyBiasNew<SSSE3::Kernels8>(8, 2048, 256, 1.7f, 1.7f, 0.46f, 0.43f);
  TestMultiplyBiasNew<SSSE3::Kernels8>(320, 256, 256, 0.56f, 0.64f, 0.16f, 0.15f);
  TestMultiplyBiasNew<SSSE3::Kernels8>(472, 256, 256, 0.46f, 0.62f, 0.17f, 0.16f);
  TestMultiplyBiasNew<SSSE3::Kernels8>(248, 256, 256, 0.48f, 0.64f, 0.16f, 0.15f);
  TestMultiplyBiasNew<SSSE3::Kernels8>(200, 256, 256, 0.55f, 0.74f, 0.17f, 0.16f);
}

#ifdef INTGEMM_COMPILER_SUPPORTS_AVX2
TEST_CASE ("Multiply AVX2 8bit Shift with bias", "[Add127]") {
  if (kCPU < CPUType::AVX2) return;
  TestMultiplyBiasNew<AVX2::Kernels8>(1, 64, 8, 0.11f, 0.11f, 0.06f, 0.05f);
  TestMultiplyBiasNew<AVX2::Kernels8>(8, 256, 256, 0.49f, 0.54f, 0.17f, 0.16f);
  TestMultiplyBiasNew<AVX2::Kernels8>(8, 2048, 256, 1.57f, 1.66f, 0.46f, 0.46f);
  TestMultiplyBiasNew<AVX2::Kernels8>(320, 256, 256, 0.49f, 0.64f, 0.16f, 0.15f);
  TestMultiplyBiasNew<AVX2::Kernels8>(472, 256, 256, 0.46f, 0.62f, 0.17f, 0.16f);
  TestMultiplyBiasNew<AVX2::Kernels8>(248, 256, 256, 0.48f, 0.64f, 0.16f, 0.15f);
  TestMultiplyBiasNew<AVX2::Kernels8>(200, 256, 256, 0.55f, 0.74f, 0.17f, 0.16f);
}
#endif

#ifdef INTGEMM_COMPILER_SUPPORTS_AVX512BW
TEST_CASE ("Multiply AVX512F 8bit Shift with bias", "[Add127]") {
  if (kCPU < CPUType::AVX512BW) return;
  TestMultiplyBiasNew<AVX512BW::Kernels8>(1, 64, 8, 0.0001f, 0.05f, 0.03f, 0.001f);
  TestMultiplyBiasNew<AVX512BW::Kernels8>(8, 256, 256, 0.0001f, 0.22f, 0.06f, 0.001f);
  TestMultiplyBiasNew<AVX512BW::Kernels8>(8, 2048, 256, 0.0001f, 0.61f, 0.17f, 0.001f);
  TestMultiplyBiasNew<AVX512BW::Kernels8>(320, 256, 256, 0.0001f, 0.27f, 0.06f, 0.001f);
  TestMultiplyBiasNew<AVX512BW::Kernels8>(472, 256, 256, 0.0001f, 0.33f, 0.06f, 0.001f);
  TestMultiplyBiasNew<AVX512BW::Kernels8>(248, 256, 256, 0.0001f, 0.27f, 0.06f, 0.001f);
  TestMultiplyBiasNew<AVX512BW::Kernels8>(200, 256, 256, 0.0001f, 0.28f, 0.06f, 0.001f);
}
#endif

#ifdef INTGEMM_COMPILER_SUPPORTS_AVX512VNNI
  TEST_CASE ("Multiply AVX512VNNI 8bit Shift with bias", "[Add127]") {
    if (kCPU < CPUType::AVX512VNNI) return;
    TestMultiplyBiasNew<AVX512VNNI::Kernels8>(1, 64, 8, 0.0001f, 0.05f, 0.03f, 0.001f);
    TestMultiplyBiasNew<AVX512VNNI::Kernels8>(8, 256, 256, 0.0001f, 0.22f, 0.06f, 0.001f);
    TestMultiplyBiasNew<AVX512VNNI::Kernels8>(8, 2048, 256, 0.0001f, 0.61f, 0.17f, 0.001f);
    TestMultiplyBiasNew<AVX512VNNI::Kernels8>(320, 256, 256, 0.0001f, 0.27f, 0.06f, 0.001f);
    TestMultiplyBiasNew<AVX512VNNI::Kernels8>(472, 256, 256, 0.0001f, 0.33f, 0.06f, 0.001f);
    TestMultiplyBiasNew<AVX512VNNI::Kernels8>(248, 256, 256, 0.0001f, 0.27f, 0.06f, 0.001f);
    TestMultiplyBiasNew<AVX512VNNI::Kernels8>(200, 256, 256, 0.0001f, 0.28f, 0.06f, 0.001f);
  }
#endif

//Multiply old vs new
TEST_CASE ("Multiply SSSE3 8bit Shift vs nonshift", "[Add127]") {
  if (kCPU < CPUType::SSSE3) return;
  TestMultiplyShiftNonShift<SSSE3::Kernels8>(1, 64, 8, 0.00001f, 0.1f, 0.06f, 0.00001f);
  TestMultiplyShiftNonShift<SSSE3::Kernels8>(8, 256, 256, 0.00001f, 0.54f, 0.17f, 0.00001f);
  TestMultiplyShiftNonShift<SSSE3::Kernels8>(8, 2048, 256, 17.9f, 1.7f, 0.46f, 4.2f); //Big difference here because the non-shift version is very bad
  TestMultiplyShiftNonShift<SSSE3::Kernels8>(320, 256, 256, 1.2f, 0.64f, 0.16f, 0.006f);
  TestMultiplyShiftNonShift<SSSE3::Kernels8>(472, 256, 256, 1.1f, 0.62f, 0.17f, 0.006f);
  TestMultiplyShiftNonShift<SSSE3::Kernels8>(248, 256, 256, 0.9f, 0.64f, 0.16f, 0.007f);
  TestMultiplyShiftNonShift<SSSE3::Kernels8>(200, 256, 256, 1, 0.74f, 0.17f, 0.006f);
}

#ifdef INTGEMM_COMPILER_SUPPORTS_AVX2
TEST_CASE ("Multiply AVX2 8bit Shift vs nonshift", "[Add127]") {
  if (kCPU < CPUType::AVX2) return;
  TestMultiplyShiftNonShift<AVX2::Kernels8>(1, 64, 8, 0.00001f, 0.11f, 0.06f, 0.00001f);
  TestMultiplyShiftNonShift<AVX2::Kernels8>(8, 256, 256, 0.00001f, 0.54f, 0.17f, 0.00001f);
  TestMultiplyShiftNonShift<AVX2::Kernels8>(8, 2048, 256, 9.4f, 1.66f, 0.46f, 1.67f); //Big difference here because the non-shift version is very bad
  TestMultiplyShiftNonShift<AVX2::Kernels8>(320, 256, 256, 0.0001f, 0.64f, 0.16f, 0.0001f);
  TestMultiplyShiftNonShift<AVX2::Kernels8>(472, 256, 256, 0.0001f, 0.62f, 0.17f, 0.0001f);
  TestMultiplyShiftNonShift<AVX2::Kernels8>(248, 256, 256, 0.0001f, 0.64f, 0.16f, 0.0001f);
  TestMultiplyShiftNonShift<AVX2::Kernels8>(200, 256, 256, 0.0001f, 0.74f, 0.17f, 0.0001f);
}
#endif

#ifdef INTGEMM_COMPILER_SUPPORTS_AVX512BW
TEST_CASE ("Multiply AVX512F 8bit Shift vs nonshift", "[Add127]") {
  if (kCPU < CPUType::AVX512BW) return;
  TestMultiplyShiftNonShift<AVX512BW::Kernels8>(1, 64, 8, 0.0001f, 0.05f, 0.03f, 0.001f);
  TestMultiplyShiftNonShift<AVX512BW::Kernels8>(8, 256, 256, 0.0001f, 0.22f, 0.06f, 0.001f);
  TestMultiplyShiftNonShift<AVX512BW::Kernels8>(8, 2048, 256, 3.51f, 0.61f, 0.17f, 0.3f);
  TestMultiplyShiftNonShift<AVX512BW::Kernels8>(320, 256, 256, 0.0001f, 0.27f, 0.06f, 0.001f);
  TestMultiplyShiftNonShift<AVX512BW::Kernels8>(472, 256, 256, 0.0001f, 0.33f, 0.06f, 0.001f);
  TestMultiplyShiftNonShift<AVX512BW::Kernels8>(248, 256, 256, 0.0001f, 0.27f, 0.06f, 0.001f);
  TestMultiplyShiftNonShift<AVX512BW::Kernels8>(200, 256, 256, 0.0001f, 0.28f, 0.06f, 0.001f);
}
#endif

#ifdef INTGEMM_COMPILER_SUPPORTS_AVX512VNNI
  TEST_CASE ("Multiply AVX512VNNI 8bit Shift vs nonshift", "[Add127]") {
    if (kCPU < CPUType::AVX512VNNI) return;
    TestMultiplyShiftNonShift<AVX512VNNI::Kernels8>(1, 64, 8, 0.00001f, 0.05f, 0.03f, 0.00001f);
    TestMultiplyShiftNonShift<AVX512VNNI::Kernels8>(8, 256, 256, 0.00001f, 0.22f, 0.06f, 0.00001f);
    TestMultiplyShiftNonShift<AVX512VNNI::Kernels8>(8, 2048, 256, 0.0001f, 0.61f, 0.17f, 0.0001f);
    TestMultiplyShiftNonShift<AVX512VNNI::Kernels8>(320, 256, 256, 0.00001f, 0.27f, 0.06f, 0.00001f);
    TestMultiplyShiftNonShift<AVX512VNNI::Kernels8>(472, 256, 256, 0.00001f, 0.33f, 0.06f, 0.00001f);
    TestMultiplyShiftNonShift<AVX512VNNI::Kernels8>(248, 256, 256, 0.00001f, 0.27f, 0.06f, 0.00001f);
    TestMultiplyShiftNonShift<AVX512VNNI::Kernels8>(200, 256, 256, 0.00001f, 0.28f, 0.06f, 0.00001f);
  }
#endif

//Multiply Shift vs int shift implementation
TEST_CASE ("Multiply SSSE3 8bit Shift vs Int", "[Add127]") {
  if (kCPU < CPUType::SSSE3) return;
  TestMultiplyShiftInt<SSSE3::Kernels8>(1, 64, 8, 0.0001f, 0.1f, 0.06f, 0.0001f);
  TestMultiplyShiftInt<SSSE3::Kernels8>(8, 256, 256, 0.0001f, 0.54f, 0.17f, 0.0001f);
  TestMultiplyShiftInt<SSSE3::Kernels8>(8, 2048, 256, 0.0001f, 1.7f, 0.46f, 0.0001f);
  TestMultiplyShiftInt<SSSE3::Kernels8>(320, 256, 256, 0.0001f, 0.64f, 0.16f, 0.0001f);
  TestMultiplyShiftInt<SSSE3::Kernels8>(472, 256, 256, 0.0001f, 0.62f, 0.17f, 0.0001f);
  TestMultiplyShiftInt<SSSE3::Kernels8>(248, 256, 256, 0.0001f, 0.64f, 0.16f, 0.0001f);
  TestMultiplyShiftInt<SSSE3::Kernels8>(200, 256, 256, 0.0001f, 0.74f, 0.17f, 0.0001f);
}

#ifdef INTGEMM_COMPILER_SUPPORTS_AVX2
TEST_CASE ("Multiply AVX2 8bit Shift vs Int", "[Add127]") {
  if (kCPU < CPUType::AVX2) return;
  TestMultiplyShiftInt<AVX2::Kernels8>(1, 64, 8, 0.0001f, 0.11f, 0.06f, 0.0001f);
  TestMultiplyShiftInt<AVX2::Kernels8>(8, 256, 256, 0.0001f, 0.54f, 0.17f, 0.0001f);
  TestMultiplyShiftInt<AVX2::Kernels8>(8, 2048, 256, 0.0001f, 1.66f, 0.46f, 0.0001f);
  TestMultiplyShiftInt<AVX2::Kernels8>(320, 256, 256, 0.0001f, 0.64f, 0.16f, 0.0001f);
  TestMultiplyShiftInt<AVX2::Kernels8>(472, 256, 256, 0.0001f, 0.62f, 0.17f, 0.0001f);
  TestMultiplyShiftInt<AVX2::Kernels8>(248, 256, 256, 0.0001f, 0.64f, 0.16f, 0.0001f);
  TestMultiplyShiftInt<AVX2::Kernels8>(200, 256, 256, 0.0001f, 0.74f, 0.17f, 0.0001f);
}
#endif

#ifdef INTGEMM_COMPILER_SUPPORTS_AVX512BW
TEST_CASE ("Multiply AVX512F 8bit Shift vs Int", "[Add127]") {
  if (kCPU < CPUType::AVX512BW) return;
  TestMultiplyShiftInt<AVX512BW::Kernels8>(1, 64, 8, 0.0001f, 0.05f, 0.03f, 0.0001f);
  TestMultiplyShiftInt<AVX512BW::Kernels8>(8, 256, 256, 0.0001f, 0.22f, 0.06f, 0.0001f);
  TestMultiplyShiftInt<AVX512BW::Kernels8>(8, 2048, 256, 0.0001f, 0.61f, 0.17f, 0.0001f);
  TestMultiplyShiftInt<AVX512BW::Kernels8>(320, 256, 256, 0.0001f, 0.27f, 0.06f, 0.0001f);
  TestMultiplyShiftInt<AVX512BW::Kernels8>(472, 256, 256, 0.0001f, 0.33f, 0.06f, 0.0001f);
  TestMultiplyShiftInt<AVX512BW::Kernels8>(248, 256, 256, 0.0001f, 0.27f, 0.06f, 0.0001f);
  TestMultiplyShiftInt<AVX512BW::Kernels8>(200, 256, 256, 0.0001f, 0.28f, 0.06f, 0.0001f);
}
#endif

#ifdef INTGEMM_COMPILER_SUPPORTS_AVX512VNNI
TEST_CASE ("Multiply AVX512VNNI 8bit Shift vs Int", "[Add127]") {
  if (kCPU < CPUType::AVX512VNNI) return;
  TestMultiplyShiftInt<AVX512VNNI::Kernels8>(1, 64, 8, 0.0001f, 0.05f, 0.03f, 0.0001f);
  TestMultiplyShiftInt<AVX512VNNI::Kernels8>(8, 256, 256, 0.0001f, 0.22f, 0.06f, 0.0001f);
  TestMultiplyShiftInt<AVX512VNNI::Kernels8>(8, 2048, 256, 0.0001f, 0.61f, 0.17f, 0.0001f);
  TestMultiplyShiftInt<AVX512VNNI::Kernels8>(320, 256, 256, 0.0001f, 0.27f, 0.06f, 0.0001f);
  TestMultiplyShiftInt<AVX512VNNI::Kernels8>(472, 256, 256, 0.0001f, 0.33f, 0.06f, 0.0001f);
  TestMultiplyShiftInt<AVX512VNNI::Kernels8>(248, 256, 256, 0.0001f, 0.27f, 0.06f, 0.0001f);
  TestMultiplyShiftInt<AVX512VNNI::Kernels8>(200, 256, 256, 0.0001f, 0.28f, 0.06f, 0.0001f);
}
#endif

} // namespace
} // namespace intgemm
