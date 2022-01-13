#pragma once

#include <tuple>

namespace intgemm {
namespace callbacks {

/*
 * Sequence meta-config
 */
template <typename... Configs>
std::tuple<Configs...> Sequence(const Configs&... configs) {
  return std::make_tuple(configs...);
}

/*
 * Configs
 */
struct Dummy {
};

template <typename Type>
struct Write {
  Type* output_addr;

  Write(Type* output_addr) : output_addr(output_addr) {}
};

struct Unquantize {
  float unquant_mult;

  Unquantize(float unquant_mult) : unquant_mult(unquant_mult) {}
};

struct UnquantizeAndWrite {
  float unquant_mult;
  float* output_addr;

  UnquantizeAndWrite(float unquant_mult, float* output_addr) : unquant_mult(unquant_mult), output_addr(output_addr) {}
};

struct UnquantizeAndWriteRelu {
  float unquant_mult;
  float* output_addr;

  UnquantizeAndWriteRelu(float unquant_mult, float* output_addr) : unquant_mult(unquant_mult), output_addr(output_addr) {}
};

struct AddBiasAndWrite {
  const int* bias_addr;
  int* output_addr;

  AddBiasAndWrite(const int* bias_addr, int* output_addr) :  bias_addr(bias_addr), output_addr(output_addr) {}
};

struct UnquantizeAndAddBiasAndWrite {
  float unquant_mult;
  const float* bias_addr;
  float* output_addr;

  UnquantizeAndAddBiasAndWrite(float unquant_mult, const float* bias_addr, float* output_addr) : unquant_mult(unquant_mult), bias_addr(bias_addr), output_addr(output_addr) {}
};

struct UnquantizeAndAddBiasAndWriteRelu {
  float unquant_mult;
  const float* bias_addr;
  float* output_addr;

  UnquantizeAndAddBiasAndWriteRelu(float unquant_mult, const float* bias_addr, float* output_addr) : unquant_mult(unquant_mult), bias_addr(bias_addr), output_addr(output_addr) {}
};

}
}
