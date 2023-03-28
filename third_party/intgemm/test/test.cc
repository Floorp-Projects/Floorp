#define CATCH_CONFIG_RUNNER
#include "test.h"

#include <cmath>

int main(int argc, char ** argv) {
  return Catch::Session().run(argc, argv);
}

namespace intgemm {

void CompareMSE(const float *float_ref, const float *int_ref, const float *int_test, std::size_t size, std::string test_info,
             float int_tolerance, float float_tolerance, float MSE_float_tolerance, float MSE_int_tolerance) {
  float int_sum = 0.0, float_sum = 0.0;
  for (std::size_t i = 0; i < size; ++i) {
    float int_diff = int_ref[i] - int_test[i];
    float float_diff = float_ref[i] - int_test[i];
    CHECK_MESSAGE(std::fabs(int_diff) <= int_tolerance, test_info << "Inaccurate compared to int reference at " << i << ' ' << int_ref[i] << ' ' << int_test[i]);
    CHECK_MESSAGE(std::fabs(float_diff) <= float_tolerance, test_info << "Inaccurate compared to float reference at " << i << ' ' << float_ref[i] << ' ' << int_test[i]);
    int_sum += int_diff * int_diff;
    float_sum += float_diff * float_diff;
  }
  CHECK_MESSAGE(std::fabs(sqrt(float_sum / size)) <= MSE_float_tolerance, test_info << "Float MSE = " << sqrt(float_sum / size));
  CHECK_MESSAGE(std::fabs(sqrt(int_sum / size)) <= MSE_int_tolerance, test_info << "Int MSE = " << sqrt(int_sum / size));
}

} // namespace intgemm
