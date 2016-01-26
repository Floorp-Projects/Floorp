// -*- mode: c++ -*-

// Copyright (c) 2010 Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// postfix_evaluator-inl.h: Postfix (reverse Polish) notation expression
// evaluator.
//
// Documentation in postfix_evaluator.h.
//
// Author: Mark Mentovai

#ifndef PROCESSOR_POSTFIX_EVALUATOR_INL_H__
#define PROCESSOR_POSTFIX_EVALUATOR_INL_H__

#include "processor/postfix_evaluator.h"

#include <stdio.h>

#include <sstream>

#include "google_breakpad/processor/memory_region.h"
#include "common/logging.h"

namespace google_breakpad {

using std::istringstream;
using std::ostringstream;


// A small class used in Evaluate to make sure to clean up the stack
// before returning failure.
template<typename ValueType>
class AutoStackClearer {
 public:
  explicit AutoStackClearer(vector<StackElem<ValueType> > *stack)
    : stack_(stack) {}
  ~AutoStackClearer() { stack_->clear(); }

 private:
  vector<StackElem<ValueType> > *stack_;
};


template<typename ValueType>
bool PostfixEvaluator<ValueType>::EvaluateToken(
    const string &token,
    const string &expression,
    DictionaryValidityType *assigned) {
  // There are enough binary operations that do exactly the same thing
  // (other than the specific operation, of course) that it makes sense
  // to share as much code as possible.
  enum BinaryOperation {
    BINARY_OP_NONE = 0,
    BINARY_OP_ADD,
    BINARY_OP_SUBTRACT,
    BINARY_OP_MULTIPLY,
    BINARY_OP_DIVIDE_QUOTIENT,
    BINARY_OP_DIVIDE_MODULUS,
    BINARY_OP_ALIGN
  };

  BinaryOperation operation = BINARY_OP_NONE;
  if (token == "+")
    operation = BINARY_OP_ADD;
  else if (token == "-")
    operation = BINARY_OP_SUBTRACT;
  else if (token == "*")
    operation = BINARY_OP_MULTIPLY;
  else if (token == "/")
    operation = BINARY_OP_DIVIDE_QUOTIENT;
  else if (token == "%")
    operation = BINARY_OP_DIVIDE_MODULUS;
  else if (token == "@")
    operation = BINARY_OP_ALIGN;

  if (operation != BINARY_OP_NONE) {
    // Get the operands.
    ValueType operand1 = ValueType();
    ValueType operand2 = ValueType();
    if (!PopValues(&operand1, &operand2)) {
      BPLOG(ERROR) << "Could not PopValues to get two values for binary "
                      "operation " << token << ": " << expression;
      return false;
    }

    // Perform the operation.
    ValueType result;
    switch (operation) {
      case BINARY_OP_ADD:
        result = operand1 + operand2;
        break;
      case BINARY_OP_SUBTRACT:
        result = operand1 - operand2;
        break;
      case BINARY_OP_MULTIPLY:
        result = operand1 * operand2;
        break;
      case BINARY_OP_DIVIDE_QUOTIENT:
        result = operand1 / operand2;
        break;
      case BINARY_OP_DIVIDE_MODULUS:
        result = operand1 % operand2;
        break;
      case BINARY_OP_ALIGN:
	result =
	  operand1 & (static_cast<ValueType>(-1) ^ (operand2 - 1));
        break;
      case BINARY_OP_NONE:
        // This will not happen, but compilers will want a default or
        // BINARY_OP_NONE case.
        BPLOG(ERROR) << "Not reached!";
        return false;
        break;
    }

    // Save the result.
    PushValue(result);
  } else if (token == "^") {
    // ^ for unary dereference.  Can't dereference without memory.
    if (!memory_) {
      BPLOG(ERROR) << "Attempt to dereference without memory: " <<
                      expression;
      return false;
    }

    ValueType address;
    if (!PopValue(&address)) {
      BPLOG(ERROR) << "Could not PopValue to get value to dereference: " <<
                      expression;
      return false;
    }

    ValueType value;
    if (!memory_->GetMemoryAtAddress(address, &value)) {
      BPLOG(ERROR) << "Could not dereference memory at address " <<
                      HexString(address) << ": " << expression;
      return false;
    }

    PushValue(value);
  } else if (token == "=") {
    // = for assignment.
    ValueType value;
    if (!PopValue(&value)) {
      BPLOG(INFO) << "Could not PopValue to get value to assign: " <<
                     expression;
      return false;
    }

    // Assignment is only meaningful when assigning into an identifier.
    // The identifier must name a variable, not a constant.  Variables
    // begin with '$'.
    const UniqueString* identifier;
    if (PopValueOrIdentifier(NULL, &identifier) != POP_RESULT_IDENTIFIER) {
      BPLOG(ERROR) << "PopValueOrIdentifier returned a value, but an "
                      "identifier is needed to assign " <<
                      HexString(value) << ": " << expression;
      return false;
    }
    if (identifier == ustr__empty() || Index(identifier,0) != '$') {
      BPLOG(ERROR) << "Can't assign " << HexString(value) << " to " <<
                      identifier << ": " << expression;
      return false;
    }

    dictionary_->set(identifier, value);
    if (assigned)
      assigned->set(identifier, true);
  } else {
    // Push it onto the stack as-is, but first convert it either to a
    // ValueType (if a literal) or to a UniqueString* (if an identifier).
    //
    // First, try to treat the value as a literal. Literals may have leading
    // '-' sign, and the entire remaining string must be parseable as
    // ValueType. If this isn't possible, it can't be a literal, so treat it
    // as an identifier instead.
    //
    // Some versions of the libstdc++, the GNU standard C++ library, have
    // stream extractors for unsigned integer values that permit a leading
    // '-' sign (6.0.13); others do not (6.0.9). Since we require it, we
    // handle it explicitly here.
    istringstream token_stream(token);
    ValueType literal = ValueType();
    bool negative = false;
    if (token_stream.peek() == '-') {
      negative = true;
      token_stream.get();
    }
    if (token_stream >> literal && token_stream.peek() == EOF) {
      PushValue(negative ? (-literal) : literal);
    } else {
      PushIdentifier(ToUniqueString(token));
    }
  }
  return true;
}

template<typename ValueType>
bool PostfixEvaluator<ValueType>::EvaluateInternal(
    const string &expression,
    DictionaryValidityType *assigned) {
  // Tokenize, splitting on whitespace.
  istringstream stream(expression);
  string token;
  while (stream >> token) {
    // Normally, tokens are whitespace-separated, but occasionally, the
    // assignment operator is smashed up against the next token, i.e.
    // $T0 $ebp 128 + =$eip $T0 4 + ^ =$ebp $T0 ^ =
    // This has been observed in program strings produced by MSVS 2010 in LTO
    // mode.
    if (token.size() > 1 && token[0] == '=') {
      if (!EvaluateToken("=", expression, assigned)) {
        return false;
      }

      if (!EvaluateToken(token.substr(1), expression, assigned)) {
        return false;
      }
    } else if (!EvaluateToken(token, expression, assigned)) {
      return false;
    }
  }

  return true;
}

template<typename ValueType>
bool PostfixEvaluator<ValueType>::Evaluate(const Module::Expr& expr,
                                           DictionaryValidityType* assigned) {
  // The expression is being exevaluated only for its side effects.  Skip
  // expressions that denote values only.
  if (expr.how_ != Module::kExprPostfix) {
    BPLOG(ERROR) << "Can't evaluate for side-effects: " << expr;
    return false;
  }

  // Ensure that the stack is cleared before returning.
  AutoStackClearer<ValueType> clearer(&stack_);

  if (!EvaluateInternal(expr.postfix_, assigned))
    return false;

  // If there's anything left on the stack, it indicates incomplete execution.
  // This is a failure case.  If the stack is empty, evalution was complete
  // and successful.
  if (stack_.empty())
    return true;

  BPLOG(ERROR) << "Incomplete execution: " << expr;
  return false;
}

template<typename ValueType>
bool PostfixEvaluator<ValueType>::EvaluateForValue(const Module::Expr& expr,
                                                   ValueType* result) {
  switch (expr.how_) {

    // Postfix expression.  Give to the evaluator and return the
    // one-and-only stack element that should be left over.
    case Module::kExprPostfix: {
      // Ensure that the stack is cleared before returning.
      AutoStackClearer<ValueType> clearer(&stack_);

      if (!EvaluateInternal(expr.postfix_, NULL))
        return false;

      // A successful execution should leave exactly one value on the stack.
      if (stack_.size() != 1) {
        BPLOG(ERROR) << "Expression yielded bad number of results: "
                     << "'" << expr << "'";
        return false;
      }

      return PopValue(result);
    }

    // Simple-form expressions
    case Module::kExprSimple:
    case Module::kExprSimpleMem: {
      // Look up the base value
      bool found = false;
      ValueType v = dictionary_->get(&found, expr.ident_);
      if (!found) {
        // The identifier wasn't found in the dictionary.  Don't imply any
        // default value, just fail.
        static uint64_t n_complaints = 0; // This isn't threadsafe.
        n_complaints++;
        if (is_power_of_2(n_complaints)) {
          BPLOG(INFO) << "Identifier " << FromUniqueString(expr.ident_)
                      << " not in dictionary (kExprSimple{Mem})"
                      << " (shown " << n_complaints << " times)";
        }
        return false;
      }

      // Form the sum
      ValueType sum = v + (int64_t)expr.offset_;

      // and dereference if necessary
      if (expr.how_ == Module::kExprSimpleMem) {
        ValueType derefd;
        if (!memory_ || !memory_->GetMemoryAtAddress(sum, &derefd)) {
          return false;
        }
        *result = derefd;
      } else {
        *result = sum;
      }
      return true;
    }

    default:
      return false;
  }
}


template<typename ValueType>
typename PostfixEvaluator<ValueType>::PopResult
PostfixEvaluator<ValueType>::PopValueOrIdentifier(
    ValueType *value, const UniqueString** identifier) {
  // There needs to be at least one element on the stack to pop.
  if (!stack_.size())
    return POP_RESULT_FAIL;

  StackElem<ValueType> el = stack_.back();
  stack_.pop_back();

  if (el.isValue) {
    if (value)
      *value = el.u.val;
    return POP_RESULT_VALUE;
  } else {
    if (identifier)
      *identifier = el.u.ustr;
    return POP_RESULT_IDENTIFIER;
  }
}


template<typename ValueType>
bool PostfixEvaluator<ValueType>::PopValue(ValueType *value) {
  ValueType literal = ValueType();
  const UniqueString* token;
  PopResult result;
  if ((result = PopValueOrIdentifier(&literal, &token)) == POP_RESULT_FAIL) {
    return false;
  } else if (result == POP_RESULT_VALUE) {
    // This is the easy case.
    *value = literal;
  } else {  // result == POP_RESULT_IDENTIFIER
    // There was an identifier at the top of the stack.  Resolve it to a
    // value by looking it up in the dictionary.
    bool found = false;
    ValueType v = dictionary_->get(&found, token);
    if (!found) {
      // The identifier wasn't found in the dictionary.  Don't imply any
      // default value, just fail.
      BPLOG(INFO) << "Identifier " << FromUniqueString(token)
                  << " not in dictionary";
      return false;
    }

    *value = v;
  }

  return true;
}


template<typename ValueType>
bool PostfixEvaluator<ValueType>::PopValues(ValueType *value1,
                                            ValueType *value2) {
  return PopValue(value2) && PopValue(value1);
}


template<typename ValueType>
void PostfixEvaluator<ValueType>::PushValue(const ValueType &value) {
  StackElem<ValueType> el(value);
  stack_.push_back(el);
}

template<typename ValueType>
void PostfixEvaluator<ValueType>::PushIdentifier(const UniqueString* str) {
  StackElem<ValueType> el(str);
  stack_.push_back(el);
}


}  // namespace google_breakpad


#endif  // PROCESSOR_POSTFIX_EVALUATOR_INL_H__
