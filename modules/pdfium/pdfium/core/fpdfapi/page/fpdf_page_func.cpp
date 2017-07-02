// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfapi/page/pageint.h"

#include <limits.h>

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "core/fpdfapi/page/cpdf_psengine.h"
#include "core/fpdfapi/parser/cpdf_array.h"
#include "core/fpdfapi/parser/cpdf_dictionary.h"
#include "core/fpdfapi/parser/cpdf_simple_parser.h"
#include "core/fpdfapi/parser/cpdf_stream.h"
#include "core/fpdfapi/parser/cpdf_stream_acc.h"
#include "core/fxcrt/fx_safe_types.h"
#include "third_party/base/ptr_util.h"

namespace {

struct PDF_PSOpName {
  const FX_CHAR* name;
  PDF_PSOP op;
};

const PDF_PSOpName kPsOpNames[] = {
    {"add", PSOP_ADD},         {"sub", PSOP_SUB},
    {"mul", PSOP_MUL},         {"div", PSOP_DIV},
    {"idiv", PSOP_IDIV},       {"mod", PSOP_MOD},
    {"neg", PSOP_NEG},         {"abs", PSOP_ABS},
    {"ceiling", PSOP_CEILING}, {"floor", PSOP_FLOOR},
    {"round", PSOP_ROUND},     {"truncate", PSOP_TRUNCATE},
    {"sqrt", PSOP_SQRT},       {"sin", PSOP_SIN},
    {"cos", PSOP_COS},         {"atan", PSOP_ATAN},
    {"exp", PSOP_EXP},         {"ln", PSOP_LN},
    {"log", PSOP_LOG},         {"cvi", PSOP_CVI},
    {"cvr", PSOP_CVR},         {"eq", PSOP_EQ},
    {"ne", PSOP_NE},           {"gt", PSOP_GT},
    {"ge", PSOP_GE},           {"lt", PSOP_LT},
    {"le", PSOP_LE},           {"and", PSOP_AND},
    {"or", PSOP_OR},           {"xor", PSOP_XOR},
    {"not", PSOP_NOT},         {"bitshift", PSOP_BITSHIFT},
    {"true", PSOP_TRUE},       {"false", PSOP_FALSE},
    {"if", PSOP_IF},           {"ifelse", PSOP_IFELSE},
    {"pop", PSOP_POP},         {"exch", PSOP_EXCH},
    {"dup", PSOP_DUP},         {"copy", PSOP_COPY},
    {"index", PSOP_INDEX},     {"roll", PSOP_ROLL}};

// See PDF Reference 1.7, page 170, table 3.36.
bool IsValidBitsPerSample(uint32_t x) {
  switch (x) {
    case 1:
    case 2:
    case 4:
    case 8:
    case 12:
    case 16:
    case 24:
    case 32:
      return true;
    default:
      return false;
  }
}

// See PDF Reference 1.7, page 170.
FX_FLOAT PDF_Interpolate(FX_FLOAT x,
                         FX_FLOAT xmin,
                         FX_FLOAT xmax,
                         FX_FLOAT ymin,
                         FX_FLOAT ymax) {
  FX_FLOAT divisor = xmax - xmin;
  return ymin + (divisor ? (x - xmin) * (ymax - ymin) / divisor : 0);
}

class CPDF_PSFunc : public CPDF_Function {
 public:
  CPDF_PSFunc() : CPDF_Function(Type::kType4PostScript) {}
  ~CPDF_PSFunc() override {}

  // CPDF_Function
  bool v_Init(CPDF_Object* pObj) override;
  bool v_Call(FX_FLOAT* inputs, FX_FLOAT* results) const override;

 private:
  CPDF_PSEngine m_PS;
};

bool CPDF_PSFunc::v_Init(CPDF_Object* pObj) {
  CPDF_StreamAcc acc;
  acc.LoadAllData(pObj->AsStream(), false);
  return m_PS.Parse(reinterpret_cast<const FX_CHAR*>(acc.GetData()),
                    acc.GetSize());
}

bool CPDF_PSFunc::v_Call(FX_FLOAT* inputs, FX_FLOAT* results) const {
  CPDF_PSEngine& PS = const_cast<CPDF_PSEngine&>(m_PS);
  PS.Reset();
  for (uint32_t i = 0; i < m_nInputs; i++)
    PS.Push(inputs[i]);
  PS.Execute();
  if (PS.GetStackSize() < m_nOutputs)
    return false;
  for (uint32_t i = 0; i < m_nOutputs; i++)
    results[m_nOutputs - i - 1] = PS.Pop();
  return true;
}

}  // namespace

class CPDF_PSOP {
 public:
  explicit CPDF_PSOP(PDF_PSOP op) : m_op(op), m_value(0) {
    ASSERT(m_op != PSOP_CONST);
    ASSERT(m_op != PSOP_PROC);
  }
  explicit CPDF_PSOP(FX_FLOAT value) : m_op(PSOP_CONST), m_value(value) {}
  explicit CPDF_PSOP(std::unique_ptr<CPDF_PSProc> proc)
      : m_op(PSOP_PROC), m_value(0), m_proc(std::move(proc)) {}

  FX_FLOAT GetFloatValue() const {
    if (m_op == PSOP_CONST)
      return m_value;

    ASSERT(false);
    return 0;
  }
  CPDF_PSProc* GetProc() const {
    if (m_op == PSOP_PROC)
      return m_proc.get();
    ASSERT(false);
    return nullptr;
  }

  PDF_PSOP GetOp() const { return m_op; }

 private:
  const PDF_PSOP m_op;
  const FX_FLOAT m_value;
  std::unique_ptr<CPDF_PSProc> m_proc;
};

bool CPDF_PSEngine::Execute() {
  return m_MainProc.Execute(this);
}

CPDF_PSProc::CPDF_PSProc() {}
CPDF_PSProc::~CPDF_PSProc() {}

bool CPDF_PSProc::Execute(CPDF_PSEngine* pEngine) {
  for (size_t i = 0; i < m_Operators.size(); ++i) {
    const PDF_PSOP op = m_Operators[i]->GetOp();
    if (op == PSOP_PROC)
      continue;

    if (op == PSOP_CONST) {
      pEngine->Push(m_Operators[i]->GetFloatValue());
      continue;
    }

    if (op == PSOP_IF) {
      if (i == 0 || m_Operators[i - 1]->GetOp() != PSOP_PROC)
        return false;

      if (static_cast<int>(pEngine->Pop()))
        m_Operators[i - 1]->GetProc()->Execute(pEngine);
    } else if (op == PSOP_IFELSE) {
      if (i < 2 || m_Operators[i - 1]->GetOp() != PSOP_PROC ||
          m_Operators[i - 2]->GetOp() != PSOP_PROC) {
        return false;
      }
      size_t offset = static_cast<int>(pEngine->Pop()) ? 2 : 1;
      m_Operators[i - offset]->GetProc()->Execute(pEngine);
    } else {
      pEngine->DoOperator(op);
    }
  }
  return true;
}

CPDF_PSEngine::CPDF_PSEngine() {
  m_StackCount = 0;
}
CPDF_PSEngine::~CPDF_PSEngine() {}
void CPDF_PSEngine::Push(FX_FLOAT v) {
  if (m_StackCount == PSENGINE_STACKSIZE) {
    return;
  }
  m_Stack[m_StackCount++] = v;
}
FX_FLOAT CPDF_PSEngine::Pop() {
  if (m_StackCount == 0) {
    return 0;
  }
  return m_Stack[--m_StackCount];
}
bool CPDF_PSEngine::Parse(const FX_CHAR* str, int size) {
  CPDF_SimpleParser parser((uint8_t*)str, size);
  CFX_ByteStringC word = parser.GetWord();
  if (word != "{") {
    return false;
  }
  return m_MainProc.Parse(&parser, 0);
}

bool CPDF_PSProc::Parse(CPDF_SimpleParser* parser, int depth) {
  if (depth > kMaxDepth)
    return false;

  while (1) {
    CFX_ByteStringC word = parser->GetWord();
    if (word.IsEmpty()) {
      return false;
    }
    if (word == "}") {
      return true;
    }
    if (word == "{") {
      std::unique_ptr<CPDF_PSProc> proc(new CPDF_PSProc);
      std::unique_ptr<CPDF_PSOP> op(new CPDF_PSOP(std::move(proc)));
      m_Operators.push_back(std::move(op));
      if (!m_Operators.back()->GetProc()->Parse(parser, depth + 1)) {
        return false;
      }
    } else {
      bool found = false;
      for (const PDF_PSOpName& op_name : kPsOpNames) {
        if (word == CFX_ByteStringC(op_name.name)) {
          std::unique_ptr<CPDF_PSOP> op(new CPDF_PSOP(op_name.op));
          m_Operators.push_back(std::move(op));
          found = true;
          break;
        }
      }
      if (!found) {
        std::unique_ptr<CPDF_PSOP> op(new CPDF_PSOP(FX_atof(word)));
        m_Operators.push_back(std::move(op));
      }
    }
  }
}

bool CPDF_PSEngine::DoOperator(PDF_PSOP op) {
  int i1;
  int i2;
  FX_FLOAT d1;
  FX_FLOAT d2;
  FX_SAFE_INT32 result;
  switch (op) {
    case PSOP_ADD:
      d1 = Pop();
      d2 = Pop();
      Push(d1 + d2);
      break;
    case PSOP_SUB:
      d2 = Pop();
      d1 = Pop();
      Push(d1 - d2);
      break;
    case PSOP_MUL:
      d1 = Pop();
      d2 = Pop();
      Push(d1 * d2);
      break;
    case PSOP_DIV:
      d2 = Pop();
      d1 = Pop();
      Push(d1 / d2);
      break;
    case PSOP_IDIV:
      i2 = static_cast<int>(Pop());
      i1 = static_cast<int>(Pop());
      if (i2) {
        result = i1;
        result /= i2;
        Push(result.ValueOrDefault(0));
      } else {
        Push(0);
      }
      break;
    case PSOP_MOD:
      i2 = static_cast<int>(Pop());
      i1 = static_cast<int>(Pop());
      if (i2) {
        result = i1;
        result %= i2;
        Push(result.ValueOrDefault(0));
      } else {
        Push(0);
      }
      break;
    case PSOP_NEG:
      d1 = Pop();
      Push(-d1);
      break;
    case PSOP_ABS:
      d1 = Pop();
      Push((FX_FLOAT)FXSYS_fabs(d1));
      break;
    case PSOP_CEILING:
      d1 = Pop();
      Push((FX_FLOAT)FXSYS_ceil(d1));
      break;
    case PSOP_FLOOR:
      d1 = Pop();
      Push((FX_FLOAT)FXSYS_floor(d1));
      break;
    case PSOP_ROUND:
      d1 = Pop();
      Push(FXSYS_round(d1));
      break;
    case PSOP_TRUNCATE:
      i1 = (int)Pop();
      Push(i1);
      break;
    case PSOP_SQRT:
      d1 = Pop();
      Push((FX_FLOAT)FXSYS_sqrt(d1));
      break;
    case PSOP_SIN:
      d1 = Pop();
      Push((FX_FLOAT)FXSYS_sin(d1 * FX_PI / 180.0f));
      break;
    case PSOP_COS:
      d1 = Pop();
      Push((FX_FLOAT)FXSYS_cos(d1 * FX_PI / 180.0f));
      break;
    case PSOP_ATAN:
      d2 = Pop();
      d1 = Pop();
      d1 = (FX_FLOAT)(FXSYS_atan2(d1, d2) * 180.0 / FX_PI);
      if (d1 < 0) {
        d1 += 360;
      }
      Push(d1);
      break;
    case PSOP_EXP:
      d2 = Pop();
      d1 = Pop();
      Push((FX_FLOAT)FXSYS_pow(d1, d2));
      break;
    case PSOP_LN:
      d1 = Pop();
      Push((FX_FLOAT)FXSYS_log(d1));
      break;
    case PSOP_LOG:
      d1 = Pop();
      Push((FX_FLOAT)FXSYS_log10(d1));
      break;
    case PSOP_CVI:
      i1 = (int)Pop();
      Push(i1);
      break;
    case PSOP_CVR:
      break;
    case PSOP_EQ:
      d2 = Pop();
      d1 = Pop();
      Push((int)(d1 == d2));
      break;
    case PSOP_NE:
      d2 = Pop();
      d1 = Pop();
      Push((int)(d1 != d2));
      break;
    case PSOP_GT:
      d2 = Pop();
      d1 = Pop();
      Push((int)(d1 > d2));
      break;
    case PSOP_GE:
      d2 = Pop();
      d1 = Pop();
      Push((int)(d1 >= d2));
      break;
    case PSOP_LT:
      d2 = Pop();
      d1 = Pop();
      Push((int)(d1 < d2));
      break;
    case PSOP_LE:
      d2 = Pop();
      d1 = Pop();
      Push((int)(d1 <= d2));
      break;
    case PSOP_AND:
      i1 = (int)Pop();
      i2 = (int)Pop();
      Push(i1 & i2);
      break;
    case PSOP_OR:
      i1 = (int)Pop();
      i2 = (int)Pop();
      Push(i1 | i2);
      break;
    case PSOP_XOR:
      i1 = (int)Pop();
      i2 = (int)Pop();
      Push(i1 ^ i2);
      break;
    case PSOP_NOT:
      i1 = (int)Pop();
      Push((int)!i1);
      break;
    case PSOP_BITSHIFT: {
      int shift = (int)Pop();
      result = (int)Pop();
      if (shift > 0) {
        result <<= shift;
      } else {
        // Avoids unsafe negation of INT_MIN.
        FX_SAFE_INT32 safe_shift = shift;
        result >>= (-safe_shift).ValueOrDefault(0);
      }
      Push(result.ValueOrDefault(0));
      break;
    }
    case PSOP_TRUE:
      Push(1);
      break;
    case PSOP_FALSE:
      Push(0);
      break;
    case PSOP_POP:
      Pop();
      break;
    case PSOP_EXCH:
      d2 = Pop();
      d1 = Pop();
      Push(d2);
      Push(d1);
      break;
    case PSOP_DUP:
      d1 = Pop();
      Push(d1);
      Push(d1);
      break;
    case PSOP_COPY: {
      int n = static_cast<int>(Pop());
      if (n < 0 || m_StackCount + n > PSENGINE_STACKSIZE ||
          n > static_cast<int>(m_StackCount))
        break;
      for (int i = 0; i < n; i++)
        m_Stack[m_StackCount + i] = m_Stack[m_StackCount + i - n];
      m_StackCount += n;
      break;
    }
    case PSOP_INDEX: {
      int n = static_cast<int>(Pop());
      if (n < 0 || n >= static_cast<int>(m_StackCount))
        break;
      Push(m_Stack[m_StackCount - n - 1]);
      break;
    }
    case PSOP_ROLL: {
      int j = static_cast<int>(Pop());
      int n = static_cast<int>(Pop());
      if (j == 0 || n == 0 || m_StackCount == 0)
        break;
      if (n < 0 || n > static_cast<int>(m_StackCount))
        break;

      j %= n;
      if (j > 0)
        j -= n;
      auto begin_it = std::begin(m_Stack) + m_StackCount - n;
      auto middle_it = begin_it - j;
      auto end_it = std::begin(m_Stack) + m_StackCount;
      std::rotate(begin_it, middle_it, end_it);
      break;
    }
    default:
      break;
  }
  return true;
}

CPDF_SampledFunc::CPDF_SampledFunc() : CPDF_Function(Type::kType0Sampled) {}

CPDF_SampledFunc::~CPDF_SampledFunc() {}

bool CPDF_SampledFunc::v_Init(CPDF_Object* pObj) {
  CPDF_Stream* pStream = pObj->AsStream();
  if (!pStream)
    return false;

  CPDF_Dictionary* pDict = pStream->GetDict();
  CPDF_Array* pSize = pDict->GetArrayFor("Size");
  CPDF_Array* pEncode = pDict->GetArrayFor("Encode");
  CPDF_Array* pDecode = pDict->GetArrayFor("Decode");
  m_nBitsPerSample = pDict->GetIntegerFor("BitsPerSample");
  if (!IsValidBitsPerSample(m_nBitsPerSample))
    return false;

  m_SampleMax = 0xffffffff >> (32 - m_nBitsPerSample);
  m_pSampleStream = pdfium::MakeUnique<CPDF_StreamAcc>();
  m_pSampleStream->LoadAllData(pStream, false);
  FX_SAFE_UINT32 nTotalSampleBits = 1;
  m_EncodeInfo.resize(m_nInputs);
  for (uint32_t i = 0; i < m_nInputs; i++) {
    m_EncodeInfo[i].sizes = pSize ? pSize->GetIntegerAt(i) : 0;
    if (!pSize && i == 0)
      m_EncodeInfo[i].sizes = pDict->GetIntegerFor("Size");
    nTotalSampleBits *= m_EncodeInfo[i].sizes;
    if (pEncode) {
      m_EncodeInfo[i].encode_min = pEncode->GetFloatAt(i * 2);
      m_EncodeInfo[i].encode_max = pEncode->GetFloatAt(i * 2 + 1);
    } else {
      m_EncodeInfo[i].encode_min = 0;
      m_EncodeInfo[i].encode_max =
          m_EncodeInfo[i].sizes == 1 ? 1 : (FX_FLOAT)m_EncodeInfo[i].sizes - 1;
    }
  }
  nTotalSampleBits *= m_nBitsPerSample;
  nTotalSampleBits *= m_nOutputs;
  FX_SAFE_UINT32 nTotalSampleBytes = nTotalSampleBits;
  nTotalSampleBytes += 7;
  nTotalSampleBytes /= 8;
  if (!nTotalSampleBytes.IsValid() || nTotalSampleBytes.ValueOrDie() == 0 ||
      nTotalSampleBytes.ValueOrDie() > m_pSampleStream->GetSize()) {
    return false;
  }
  m_DecodeInfo.resize(m_nOutputs);
  for (uint32_t i = 0; i < m_nOutputs; i++) {
    if (pDecode) {
      m_DecodeInfo[i].decode_min = pDecode->GetFloatAt(2 * i);
      m_DecodeInfo[i].decode_max = pDecode->GetFloatAt(2 * i + 1);
    } else {
      m_DecodeInfo[i].decode_min = m_pRanges[i * 2];
      m_DecodeInfo[i].decode_max = m_pRanges[i * 2 + 1];
    }
  }
  return true;
}

bool CPDF_SampledFunc::v_Call(FX_FLOAT* inputs, FX_FLOAT* results) const {
  int pos = 0;
  CFX_FixedBufGrow<FX_FLOAT, 16> encoded_input_buf(m_nInputs);
  FX_FLOAT* encoded_input = encoded_input_buf;
  CFX_FixedBufGrow<uint32_t, 32> int_buf(m_nInputs * 2);
  uint32_t* index = int_buf;
  uint32_t* blocksize = index + m_nInputs;
  for (uint32_t i = 0; i < m_nInputs; i++) {
    if (i == 0)
      blocksize[i] = 1;
    else
      blocksize[i] = blocksize[i - 1] * m_EncodeInfo[i - 1].sizes;
    encoded_input[i] =
        PDF_Interpolate(inputs[i], m_pDomains[i * 2], m_pDomains[i * 2 + 1],
                        m_EncodeInfo[i].encode_min, m_EncodeInfo[i].encode_max);
    index[i] = std::min((uint32_t)std::max(0.f, encoded_input[i]),
                        m_EncodeInfo[i].sizes - 1);
    pos += index[i] * blocksize[i];
  }
  FX_SAFE_INT32 bits_to_output = m_nOutputs;
  bits_to_output *= m_nBitsPerSample;
  if (!bits_to_output.IsValid())
    return false;

  FX_SAFE_INT32 bitpos = pos;
  bitpos *= bits_to_output.ValueOrDie();
  if (!bitpos.IsValid())
    return false;

  FX_SAFE_INT32 range_check = bitpos;
  range_check += bits_to_output.ValueOrDie();
  if (!range_check.IsValid())
    return false;

  const uint8_t* pSampleData = m_pSampleStream->GetData();
  if (!pSampleData)
    return false;

  for (uint32_t j = 0; j < m_nOutputs; j++, bitpos += m_nBitsPerSample) {
    uint32_t sample =
        GetBits32(pSampleData, bitpos.ValueOrDie(), m_nBitsPerSample);
    FX_FLOAT encoded = (FX_FLOAT)sample;
    for (uint32_t i = 0; i < m_nInputs; i++) {
      if (index[i] == m_EncodeInfo[i].sizes - 1) {
        if (index[i] == 0)
          encoded = encoded_input[i] * (FX_FLOAT)sample;
      } else {
        FX_SAFE_INT32 bitpos2 = blocksize[i];
        bitpos2 += pos;
        bitpos2 *= m_nOutputs;
        bitpos2 += j;
        bitpos2 *= m_nBitsPerSample;
        if (!bitpos2.IsValid())
          return false;
        uint32_t sample1 =
            GetBits32(pSampleData, bitpos2.ValueOrDie(), m_nBitsPerSample);
        encoded += (encoded_input[i] - index[i]) *
                   ((FX_FLOAT)sample1 - (FX_FLOAT)sample);
      }
    }
    results[j] =
        PDF_Interpolate(encoded, 0, (FX_FLOAT)m_SampleMax,
                        m_DecodeInfo[j].decode_min, m_DecodeInfo[j].decode_max);
  }
  return true;
}

CPDF_ExpIntFunc::CPDF_ExpIntFunc()
    : CPDF_Function(Type::kType2ExpotentialInterpolation),
      m_pBeginValues(nullptr),
      m_pEndValues(nullptr) {}

CPDF_ExpIntFunc::~CPDF_ExpIntFunc() {
  FX_Free(m_pBeginValues);
  FX_Free(m_pEndValues);
}
bool CPDF_ExpIntFunc::v_Init(CPDF_Object* pObj) {
  CPDF_Dictionary* pDict = pObj->GetDict();
  if (!pDict) {
    return false;
  }
  CPDF_Array* pArray0 = pDict->GetArrayFor("C0");
  if (m_nOutputs == 0) {
    m_nOutputs = 1;
    if (pArray0) {
      m_nOutputs = pArray0->GetCount();
    }
  }
  CPDF_Array* pArray1 = pDict->GetArrayFor("C1");
  m_pBeginValues = FX_Alloc2D(FX_FLOAT, m_nOutputs, 2);
  m_pEndValues = FX_Alloc2D(FX_FLOAT, m_nOutputs, 2);
  for (uint32_t i = 0; i < m_nOutputs; i++) {
    m_pBeginValues[i] = pArray0 ? pArray0->GetFloatAt(i) : 0.0f;
    m_pEndValues[i] = pArray1 ? pArray1->GetFloatAt(i) : 1.0f;
  }
  m_Exponent = pDict->GetFloatFor("N");
  m_nOrigOutputs = m_nOutputs;
  if (m_nOutputs && m_nInputs > INT_MAX / m_nOutputs) {
    return false;
  }
  m_nOutputs *= m_nInputs;
  return true;
}
bool CPDF_ExpIntFunc::v_Call(FX_FLOAT* inputs, FX_FLOAT* results) const {
  for (uint32_t i = 0; i < m_nInputs; i++)
    for (uint32_t j = 0; j < m_nOrigOutputs; j++) {
      results[i * m_nOrigOutputs + j] =
          m_pBeginValues[j] +
          (FX_FLOAT)FXSYS_pow(inputs[i], m_Exponent) *
              (m_pEndValues[j] - m_pBeginValues[j]);
    }
  return true;
}

CPDF_StitchFunc::CPDF_StitchFunc()
    : CPDF_Function(Type::kType3Stitching),
      m_pBounds(nullptr),
      m_pEncode(nullptr) {}

CPDF_StitchFunc::~CPDF_StitchFunc() {
  FX_Free(m_pBounds);
  FX_Free(m_pEncode);
}

bool CPDF_StitchFunc::v_Init(CPDF_Object* pObj) {
  CPDF_Dictionary* pDict = pObj->GetDict();
  if (!pDict) {
    return false;
  }
  if (m_nInputs != kRequiredNumInputs) {
    return false;
  }
  CPDF_Array* pArray = pDict->GetArrayFor("Functions");
  if (!pArray) {
    return false;
  }
  uint32_t nSubs = pArray->GetCount();
  if (nSubs == 0)
    return false;
  m_nOutputs = 0;
  for (uint32_t i = 0; i < nSubs; i++) {
    CPDF_Object* pSub = pArray->GetDirectObjectAt(i);
    if (pSub == pObj)
      return false;
    std::unique_ptr<CPDF_Function> pFunc(CPDF_Function::Load(pSub));
    if (!pFunc)
      return false;
    // Check that the input dimensionality is 1, and that all output
    // dimensionalities are the same.
    if (pFunc->CountInputs() != kRequiredNumInputs)
      return false;
    if (pFunc->CountOutputs() != m_nOutputs) {
      if (m_nOutputs)
        return false;

      m_nOutputs = pFunc->CountOutputs();
    }

    m_pSubFunctions.push_back(std::move(pFunc));
  }
  m_pBounds = FX_Alloc(FX_FLOAT, nSubs + 1);
  m_pBounds[0] = m_pDomains[0];
  pArray = pDict->GetArrayFor("Bounds");
  if (!pArray)
    return false;
  for (uint32_t i = 0; i < nSubs - 1; i++)
    m_pBounds[i + 1] = pArray->GetFloatAt(i);
  m_pBounds[nSubs] = m_pDomains[1];
  m_pEncode = FX_Alloc2D(FX_FLOAT, nSubs, 2);
  pArray = pDict->GetArrayFor("Encode");
  if (!pArray)
    return false;

  for (uint32_t i = 0; i < nSubs * 2; i++)
    m_pEncode[i] = pArray->GetFloatAt(i);
  return true;
}

bool CPDF_StitchFunc::v_Call(FX_FLOAT* inputs, FX_FLOAT* outputs) const {
  FX_FLOAT input = inputs[0];
  size_t i;
  for (i = 0; i < m_pSubFunctions.size() - 1; i++) {
    if (input < m_pBounds[i + 1])
      break;
  }
  input = PDF_Interpolate(input, m_pBounds[i], m_pBounds[i + 1],
                          m_pEncode[i * 2], m_pEncode[i * 2 + 1]);
  int nresults;
  m_pSubFunctions[i]->Call(&input, kRequiredNumInputs, outputs, nresults);
  return true;
}

// static
std::unique_ptr<CPDF_Function> CPDF_Function::Load(CPDF_Object* pFuncObj) {
  std::unique_ptr<CPDF_Function> pFunc;
  if (!pFuncObj)
    return pFunc;

  int iType = -1;
  if (CPDF_Stream* pStream = pFuncObj->AsStream())
    iType = pStream->GetDict()->GetIntegerFor("FunctionType");
  else if (CPDF_Dictionary* pDict = pFuncObj->AsDictionary())
    iType = pDict->GetIntegerFor("FunctionType");

  Type type = IntegerToFunctionType(iType);
  if (type == Type::kType0Sampled)
    pFunc = pdfium::MakeUnique<CPDF_SampledFunc>();
  else if (type == Type::kType2ExpotentialInterpolation)
    pFunc = pdfium::MakeUnique<CPDF_ExpIntFunc>();
  else if (type == Type::kType3Stitching)
    pFunc = pdfium::MakeUnique<CPDF_StitchFunc>();
  else if (type == Type::kType4PostScript)
    pFunc = pdfium::MakeUnique<CPDF_PSFunc>();

  if (!pFunc || !pFunc->Init(pFuncObj))
    return nullptr;

  return pFunc;
}

// static
CPDF_Function::Type CPDF_Function::IntegerToFunctionType(int iType) {
  switch (iType) {
    case 0:
    case 2:
    case 3:
    case 4:
      return static_cast<Type>(iType);
    default:
      return Type::kTypeInvalid;
  }
}

CPDF_Function::CPDF_Function(Type type)
    : m_pDomains(nullptr), m_pRanges(nullptr), m_Type(type) {}

CPDF_Function::~CPDF_Function() {
  FX_Free(m_pDomains);
  FX_Free(m_pRanges);
}

bool CPDF_Function::Init(CPDF_Object* pObj) {
  CPDF_Stream* pStream = pObj->AsStream();
  CPDF_Dictionary* pDict = pStream ? pStream->GetDict() : pObj->AsDictionary();

  CPDF_Array* pDomains = pDict->GetArrayFor("Domain");
  if (!pDomains)
    return false;

  m_nInputs = pDomains->GetCount() / 2;
  if (m_nInputs == 0)
    return false;

  m_pDomains = FX_Alloc2D(FX_FLOAT, m_nInputs, 2);
  for (uint32_t i = 0; i < m_nInputs * 2; i++) {
    m_pDomains[i] = pDomains->GetFloatAt(i);
  }
  CPDF_Array* pRanges = pDict->GetArrayFor("Range");
  m_nOutputs = 0;
  if (pRanges) {
    m_nOutputs = pRanges->GetCount() / 2;
    m_pRanges = FX_Alloc2D(FX_FLOAT, m_nOutputs, 2);
    for (uint32_t i = 0; i < m_nOutputs * 2; i++)
      m_pRanges[i] = pRanges->GetFloatAt(i);
  }
  uint32_t old_outputs = m_nOutputs;
  if (!v_Init(pObj))
    return false;
  if (m_pRanges && m_nOutputs > old_outputs) {
    m_pRanges = FX_Realloc(FX_FLOAT, m_pRanges, m_nOutputs * 2);
    if (m_pRanges) {
      FXSYS_memset(m_pRanges + (old_outputs * 2), 0,
                   sizeof(FX_FLOAT) * (m_nOutputs - old_outputs) * 2);
    }
  }
  return true;
}

bool CPDF_Function::Call(FX_FLOAT* inputs,
                         uint32_t ninputs,
                         FX_FLOAT* results,
                         int& nresults) const {
  if (m_nInputs != ninputs) {
    return false;
  }
  nresults = m_nOutputs;
  for (uint32_t i = 0; i < m_nInputs; i++) {
    if (inputs[i] < m_pDomains[i * 2])
      inputs[i] = m_pDomains[i * 2];
    else if (inputs[i] > m_pDomains[i * 2 + 1])
      inputs[i] = m_pDomains[i * 2] + 1;
  }
  v_Call(inputs, results);
  if (m_pRanges) {
    for (uint32_t i = 0; i < m_nOutputs; i++) {
      if (results[i] < m_pRanges[i * 2])
        results[i] = m_pRanges[i * 2];
      else if (results[i] > m_pRanges[i * 2 + 1])
        results[i] = m_pRanges[i * 2 + 1];
    }
  }
  return true;
}

const CPDF_SampledFunc* CPDF_Function::ToSampledFunc() const {
  return m_Type == Type::kType0Sampled
             ? static_cast<const CPDF_SampledFunc*>(this)
             : nullptr;
}

const CPDF_ExpIntFunc* CPDF_Function::ToExpIntFunc() const {
  return m_Type == Type::kType2ExpotentialInterpolation
             ? static_cast<const CPDF_ExpIntFunc*>(this)
             : nullptr;
}

const CPDF_StitchFunc* CPDF_Function::ToStitchFunc() const {
  return m_Type == Type::kType3Stitching
             ? static_cast<const CPDF_StitchFunc*>(this)
             : nullptr;
}
