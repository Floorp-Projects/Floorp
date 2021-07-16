/*
 * Copyright 2017 WebAssembly Community Group participants
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "src/wast-parser.h"

#include "src/binary-reader-ir.h"
#include "src/binary-reader.h"
#include "src/cast.h"
#include "src/expr-visitor.h"
#include "src/make-unique.h"
#include "src/resolve-names.h"
#include "src/stream.h"
#include "src/utf8.h"
#include "src/validator.h"

#define WABT_TRACING 0
#include "src/tracing.h"

#define EXPECT(token_type) CHECK_RESULT(Expect(TokenType::token_type))

namespace wabt {

namespace {

static const size_t kMaxErrorTokenLength = 80;

bool IsPowerOfTwo(uint32_t x) {
  return x && ((x & (x - 1)) == 0);
}

template <typename OutputIter>
void RemoveEscapes(string_view text, OutputIter dest) {
  // Remove surrounding quotes; if any. This may be empty if the string was
  // invalid (e.g. if it contained a bad escape sequence).
  if (text.size() <= 2) {
    return;
  }

  text = text.substr(1, text.size() - 2);

  const char* src = text.data();
  const char* end = text.data() + text.size();

  while (src < end) {
    if (*src == '\\') {
      src++;
      switch (*src) {
        case 'n':
          *dest++ = '\n';
          break;
        case 'r':
          *dest++ = '\r';
          break;
        case 't':
          *dest++ = '\t';
          break;
        case '\\':
          *dest++ = '\\';
          break;
        case '\'':
          *dest++ = '\'';
          break;
        case '\"':
          *dest++ = '\"';
          break;
        default: {
          // The string should be validated already, so we know this is a hex
          // sequence.
          uint32_t hi;
          uint32_t lo;
          if (Succeeded(ParseHexdigit(src[0], &hi)) &&
              Succeeded(ParseHexdigit(src[1], &lo))) {
            *dest++ = (hi << 4) | lo;
          } else {
            assert(0);
          }
          src++;
          break;
        }
      }
      src++;
    } else {
      *dest++ = *src++;
    }
  }
}

typedef std::vector<string_view> TextVector;

template <typename OutputIter>
void RemoveEscapes(const TextVector& texts, OutputIter out) {
  for (string_view text : texts)
    RemoveEscapes(text, out);
}

bool IsPlainInstr(TokenType token_type) {
  switch (token_type) {
    case TokenType::Unreachable:
    case TokenType::Nop:
    case TokenType::Drop:
    case TokenType::Select:
    case TokenType::Br:
    case TokenType::BrIf:
    case TokenType::BrTable:
    case TokenType::Return:
    case TokenType::ReturnCall:
    case TokenType::ReturnCallIndirect:
    case TokenType::Call:
    case TokenType::CallIndirect:
    case TokenType::LocalGet:
    case TokenType::LocalSet:
    case TokenType::LocalTee:
    case TokenType::GlobalGet:
    case TokenType::GlobalSet:
    case TokenType::Load:
    case TokenType::Store:
    case TokenType::Const:
    case TokenType::Unary:
    case TokenType::Binary:
    case TokenType::Compare:
    case TokenType::Convert:
    case TokenType::MemoryCopy:
    case TokenType::DataDrop:
    case TokenType::MemoryFill:
    case TokenType::MemoryGrow:
    case TokenType::MemoryInit:
    case TokenType::MemorySize:
    case TokenType::TableCopy:
    case TokenType::ElemDrop:
    case TokenType::TableInit:
    case TokenType::TableGet:
    case TokenType::TableSet:
    case TokenType::TableGrow:
    case TokenType::TableSize:
    case TokenType::TableFill:
    case TokenType::Throw:
    case TokenType::Rethrow:
    case TokenType::RefFunc:
    case TokenType::RefNull:
    case TokenType::RefIsNull:
    case TokenType::AtomicLoad:
    case TokenType::AtomicStore:
    case TokenType::AtomicRmw:
    case TokenType::AtomicRmwCmpxchg:
    case TokenType::AtomicNotify:
    case TokenType::AtomicFence:
    case TokenType::AtomicWait:
    case TokenType::Ternary:
    case TokenType::SimdLaneOp:
    case TokenType::SimdLoadLane:
    case TokenType::SimdStoreLane:
    case TokenType::SimdShuffleOp:
      return true;
    default:
      return false;
  }
}

bool IsBlockInstr(TokenType token_type) {
  switch (token_type) {
    case TokenType::Block:
    case TokenType::Loop:
    case TokenType::If:
    case TokenType::Try:
      return true;
    default:
      return false;
  }
}

bool IsPlainOrBlockInstr(TokenType token_type) {
  return IsPlainInstr(token_type) || IsBlockInstr(token_type);
}

bool IsExpr(TokenTypePair pair) {
  return pair[0] == TokenType::Lpar && IsPlainOrBlockInstr(pair[1]);
}

bool IsInstr(TokenTypePair pair) {
  return IsPlainOrBlockInstr(pair[0]) || IsExpr(pair);
}

bool IsCatch(TokenType token_type) {
  return token_type == TokenType::Catch || token_type == TokenType::CatchAll;
}

bool IsModuleField(TokenTypePair pair) {
  if (pair[0] != TokenType::Lpar) {
    return false;
  }

  switch (pair[1]) {
    case TokenType::Data:
    case TokenType::Elem:
    case TokenType::Event:
    case TokenType::Export:
    case TokenType::Func:
    case TokenType::Type:
    case TokenType::Global:
    case TokenType::Import:
    case TokenType::Memory:
    case TokenType::Start:
    case TokenType::Table:
      return true;
    default:
      return false;
  }
}

bool IsCommand(TokenTypePair pair) {
  if (pair[0] != TokenType::Lpar) {
    return false;
  }

  switch (pair[1]) {
    case TokenType::AssertExhaustion:
    case TokenType::AssertInvalid:
    case TokenType::AssertMalformed:
    case TokenType::AssertReturn:
    case TokenType::AssertTrap:
    case TokenType::AssertUnlinkable:
    case TokenType::Get:
    case TokenType::Invoke:
    case TokenType::Input:
    case TokenType::Module:
    case TokenType::Output:
    case TokenType::Register:
      return true;
    default:
      return false;
  }
}

bool IsEmptySignature(const FuncSignature& sig) {
  return sig.result_types.empty() && sig.param_types.empty();
}

bool ResolveFuncTypeWithEmptySignature(const Module& module,
                                       FuncDeclaration* decl) {
  // Resolve func type variables where the signature was not specified
  // explicitly, e.g.: (func (type 1) ...)
  if (decl->has_func_type && IsEmptySignature(decl->sig)) {
    const FuncType* func_type = module.GetFuncType(decl->type_var);
    if (func_type) {
      decl->sig = func_type->sig;
      return true;
    }
  }
  return false;
}

void ResolveImplicitlyDefinedFunctionType(const Location& loc,
                                          Module* module,
                                          const FuncDeclaration& decl) {
  // Resolve implicitly defined function types, e.g.: (func (param i32) ...)
  if (!decl.has_func_type) {
    Index func_type_index = module->GetFuncTypeIndex(decl.sig);
    if (func_type_index == kInvalidIndex) {
      auto func_type_field = MakeUnique<TypeModuleField>(loc);
      auto func_type = MakeUnique<FuncType>();
      func_type->sig = decl.sig;
      func_type_field->type = std::move(func_type);
      module->AppendField(std::move(func_type_field));
    }
  }
}

Result CheckTypeIndex(const Location& loc,
                      Type actual,
                      Type expected,
                      const char* desc,
                      Index index,
                      const char* index_kind,
                      Errors* errors) {
  // Types must match exactly; no subtyping should be allowed.
  if (actual != expected) {
    errors->emplace_back(ErrorLevel::Error, loc,
                         StringPrintf("type mismatch for %s %" PRIindex
                                      " of %s. got %s, expected %s",
                                      index_kind, index, desc, actual.GetName(),
                                      expected.GetName()));
    return Result::Error;
  }
  return Result::Ok;
}

Result CheckTypes(const Location& loc,
                  const TypeVector& actual,
                  const TypeVector& expected,
                  const char* desc,
                  const char* index_kind,
                  Errors* errors) {
  Result result = Result::Ok;
  if (actual.size() == expected.size()) {
    for (size_t i = 0; i < actual.size(); ++i) {
      result |= CheckTypeIndex(loc, actual[i], expected[i], desc, i, index_kind,
                               errors);
    }
  } else {
    errors->emplace_back(
        ErrorLevel::Error, loc,
        StringPrintf("expected %" PRIzd " %ss, got %" PRIzd, expected.size(),
                     index_kind, actual.size()));
    result = Result::Error;
  }
  return result;
}

Result CheckFuncTypeVarMatchesExplicit(const Location& loc,
                                       const Module& module,
                                       const FuncDeclaration& decl,
                                       Errors* errors) {
  Result result = Result::Ok;
  if (decl.has_func_type) {
    const FuncType* func_type = module.GetFuncType(decl.type_var);
    if (func_type) {
      result |=
          CheckTypes(loc, decl.sig.result_types, func_type->sig.result_types,
                     "function", "result", errors);
      result |=
          CheckTypes(loc, decl.sig.param_types, func_type->sig.param_types,
                     "function", "argument", errors);
    } else if (!(decl.sig.param_types.empty() &&
                 decl.sig.result_types.empty())) {
      // We want to check whether the function type at the explicit index
      // matches the given param and result types. If they were omitted then
      // they'll be resolved automatically (see
      // ResolveFuncTypeWithEmptySignature), but if they are provided then we
      // have to check. If we get here then the type var is invalid, so we
      // can't check whether they match.
      errors->emplace_back(ErrorLevel::Error, loc,
                           StringPrintf("invalid func type index %" PRIindex,
                                        decl.type_var.index()));
      result = Result::Error;
    }
  }
  return result;
}

bool IsInlinableFuncSignature(const FuncSignature& sig) {
  return sig.GetNumParams() == 0 && sig.GetNumResults() <= 1;
}

class ResolveFuncTypesExprVisitorDelegate : public ExprVisitor::DelegateNop {
 public:
  explicit ResolveFuncTypesExprVisitorDelegate(Module* module, Errors* errors)
      : module_(module), errors_(errors) {}

  void ResolveBlockDeclaration(const Location& loc, BlockDeclaration* decl) {
    ResolveFuncTypeWithEmptySignature(*module_, decl);
    if (!IsInlinableFuncSignature(decl->sig)) {
      ResolveImplicitlyDefinedFunctionType(loc, module_, *decl);
    }
  }

  Result BeginBlockExpr(BlockExpr* expr) override {
    ResolveBlockDeclaration(expr->loc, &expr->block.decl);
    return CheckFuncTypeVarMatchesExplicit(expr->loc, *module_,
                                           expr->block.decl, errors_);
  }

  Result BeginIfExpr(IfExpr* expr) override {
    ResolveBlockDeclaration(expr->loc, &expr->true_.decl);
    return CheckFuncTypeVarMatchesExplicit(expr->loc, *module_,
                                           expr->true_.decl, errors_);
  }

  Result BeginLoopExpr(LoopExpr* expr) override {
    ResolveBlockDeclaration(expr->loc, &expr->block.decl);
    return CheckFuncTypeVarMatchesExplicit(expr->loc, *module_,
                                           expr->block.decl, errors_);
  }

  Result BeginTryExpr(TryExpr* expr) override {
    ResolveBlockDeclaration(expr->loc, &expr->block.decl);
    return CheckFuncTypeVarMatchesExplicit(expr->loc, *module_,
                                           expr->block.decl, errors_);
  }

  Result OnCallIndirectExpr(CallIndirectExpr* expr) override {
    ResolveFuncTypeWithEmptySignature(*module_, &expr->decl);
    ResolveImplicitlyDefinedFunctionType(expr->loc, module_, expr->decl);
    return CheckFuncTypeVarMatchesExplicit(expr->loc, *module_, expr->decl,
                                           errors_);
  }

  Result OnReturnCallIndirectExpr(ReturnCallIndirectExpr* expr) override {
    ResolveFuncTypeWithEmptySignature(*module_, &expr->decl);
    ResolveImplicitlyDefinedFunctionType(expr->loc, module_, expr->decl);
    return CheckFuncTypeVarMatchesExplicit(expr->loc, *module_, expr->decl,
                                           errors_);
  }

 private:
  Module* module_;
  Errors* errors_;
};

Result ResolveFuncTypes(Module* module, Errors* errors) {
  Result result = Result::Ok;
  for (ModuleField& field : module->fields) {
    Func* func = nullptr;
    FuncDeclaration* decl = nullptr;
    if (auto* func_field = dyn_cast<FuncModuleField>(&field)) {
      func = &func_field->func;
      decl = &func->decl;
    } else if (auto* event_field = dyn_cast<EventModuleField>(&field)) {
      decl = &event_field->event.decl;
    } else if (auto* import_field = dyn_cast<ImportModuleField>(&field)) {
      if (auto* func_import =
              dyn_cast<FuncImport>(import_field->import.get())) {
        // Only check the declaration, not the function itself, since it is an
        // import.
        decl = &func_import->func.decl;
      } else if (auto* event_import =
                     dyn_cast<EventImport>(import_field->import.get())) {
        decl = &event_import->event.decl;
      } else {
        continue;
      }
    } else {
      continue;
    }

    bool has_func_type_and_empty_signature = false;

    if (decl) {
      has_func_type_and_empty_signature =
          ResolveFuncTypeWithEmptySignature(*module, decl);
      ResolveImplicitlyDefinedFunctionType(field.loc, module, *decl);
      result |=
          CheckFuncTypeVarMatchesExplicit(field.loc, *module, *decl, errors);
    }

    if (func) {
      if (has_func_type_and_empty_signature) {
        // The call to ResolveFuncTypeWithEmptySignature may have updated the
        // function signature so there are parameters. Since parameters and
        // local variables share the same index space, we need to increment the
        // local indexes bound to a given name by the number of parameters in
        // the function.
        for (auto& pair: func->bindings) {
          pair.second.index += func->GetNumParams();
        }
      }

      ResolveFuncTypesExprVisitorDelegate delegate(module, errors);
      ExprVisitor visitor(&delegate);
      result |= visitor.VisitFunc(func);
    }
  }
  return result;
}

void AppendInlineExportFields(Module* module,
                              ModuleFieldList* fields,
                              Index index) {
  Location last_field_loc = module->fields.back().loc;

  for (ModuleField& field : *fields) {
    auto* export_field = cast<ExportModuleField>(&field);
    export_field->export_.var = Var(index, last_field_loc);
  }

  module->AppendFields(fields);
}

}  // End of anonymous namespace

WastParser::WastParser(WastLexer* lexer,
                       Errors* errors,
                       WastParseOptions* options)
    : lexer_(lexer), errors_(errors), options_(options) {}

void WastParser::Error(Location loc, const char* format, ...) {
  WABT_SNPRINTF_ALLOCA(buffer, length, format);
  errors_->emplace_back(ErrorLevel::Error, loc, buffer);
}

Token WastParser::GetToken() {
  if (tokens_.empty()) {
    tokens_.push_back(lexer_->GetToken(this));
  }
  return tokens_.front();
}

Location WastParser::GetLocation() {
  return GetToken().loc;
}

TokenType WastParser::Peek(size_t n) {
  while (tokens_.size() <= n) {
    Token cur = lexer_->GetToken(this);
    if (cur.token_type() != TokenType::LparAnn) {
      tokens_.push_back(cur);
    } else {
      // Custom annotation. For now, discard until matching Rpar.
      if (!options_->features.annotations_enabled()) {
        Error(cur.loc, "annotations not enabled: %s", cur.to_string().c_str());
        tokens_.push_back(Token(cur.loc, TokenType::Invalid));
        continue;
      }
      int indent = 1;
      while (indent > 0) {
        cur = lexer_->GetToken(this);
        switch (cur.token_type()) {
          case TokenType::Lpar:
          case TokenType::LparAnn:
            indent++;
            break;

          case TokenType::Rpar:
            indent--;
            break;

          default:
            break;
        }
      }
    }
  }
  return tokens_.at(n).token_type();
}

TokenTypePair WastParser::PeekPair() {
  return TokenTypePair{{Peek(), Peek(1)}};
}

bool WastParser::PeekMatch(TokenType type) {
  return Peek() == type;
}

bool WastParser::PeekMatchLpar(TokenType type) {
  return Peek() == TokenType::Lpar && Peek(1) == type;
}

bool WastParser::PeekMatchExpr() {
  return IsExpr(PeekPair());
}

bool WastParser::Match(TokenType type) {
  if (PeekMatch(type)) {
    Consume();
    return true;
  }
  return false;
}

bool WastParser::MatchLpar(TokenType type) {
  if (PeekMatchLpar(type)) {
    Consume();
    Consume();
    return true;
  }
  return false;
}

Result WastParser::Expect(TokenType type) {
  if (!Match(type)) {
    Token token = Consume();
    Error(token.loc, "unexpected token %s, expected %s.",
          token.to_string_clamp(kMaxErrorTokenLength).c_str(),
          GetTokenTypeName(type));
    return Result::Error;
  }

  return Result::Ok;
}

Token WastParser::Consume() {
  assert(!tokens_.empty());
  Token token = tokens_.front();
  tokens_.pop_front();
  return token;
}

Result WastParser::Synchronize(SynchronizeFunc func) {
  static const int kMaxConsumed = 10;
  for (int i = 0; i < kMaxConsumed; ++i) {
    if (func(PeekPair())) {
      return Result::Ok;
    }

    Token token = Consume();
    if (token.token_type() == TokenType::Reserved) {
      Error(token.loc, "unexpected token %s.",
            token.to_string_clamp(kMaxErrorTokenLength).c_str());
    }
  }

  return Result::Error;
}

void WastParser::ErrorUnlessOpcodeEnabled(const Token& token) {
  Opcode opcode = token.opcode();
  if (!opcode.IsEnabled(options_->features)) {
    Error(token.loc, "opcode not allowed: %s", opcode.GetName());
  }
}

Result WastParser::ErrorExpected(const std::vector<std::string>& expected,
                                 const char* example) {
  Token token = Consume();
  std::string expected_str;
  if (!expected.empty()) {
    expected_str = ", expected ";
    for (size_t i = 0; i < expected.size(); ++i) {
      if (i != 0) {
        if (i == expected.size() - 1) {
          expected_str += " or ";
        } else {
          expected_str += ", ";
        }
      }

      expected_str += expected[i];
    }

    if (example) {
      expected_str += " (e.g. ";
      expected_str += example;
      expected_str += ")";
    }
  }

  Error(token.loc, "unexpected token \"%s\"%s.",
        token.to_string_clamp(kMaxErrorTokenLength).c_str(),
        expected_str.c_str());
  return Result::Error;
}

Result WastParser::ErrorIfLpar(const std::vector<std::string>& expected,
                               const char* example) {
  if (Match(TokenType::Lpar)) {
    GetToken();
    return ErrorExpected(expected, example);
  }
  return Result::Ok;
}

bool WastParser::ParseBindVarOpt(std::string* name) {
  WABT_TRACE(ParseBindVarOpt);
  if (!PeekMatch(TokenType::Var)) {
    return false;
  }
  Token token = Consume();
  *name = token.text().to_string();
  return true;
}

Result WastParser::ParseVar(Var* out_var) {
  WABT_TRACE(ParseVar);
  if (PeekMatch(TokenType::Nat)) {
    Token token = Consume();
    string_view sv = token.literal().text;
    uint64_t index = kInvalidIndex;
    if (Failed(ParseUint64(sv.begin(), sv.end(), &index))) {
      // Print an error, but don't fail parsing.
      Error(token.loc, "invalid int \"" PRIstringview "\"",
            WABT_PRINTF_STRING_VIEW_ARG(sv));
    }

    *out_var = Var(index, token.loc);
    return Result::Ok;
  } else if (PeekMatch(TokenType::Var)) {
    Token token = Consume();
    *out_var = Var(token.text(), token.loc);
    return Result::Ok;
  } else {
    return ErrorExpected({"a numeric index", "a name"}, "12 or $foo");
  }
}

bool WastParser::ParseVarOpt(Var* out_var, Var default_var) {
  WABT_TRACE(ParseVarOpt);
  if (PeekMatch(TokenType::Nat) || PeekMatch(TokenType::Var)) {
    Result result = ParseVar(out_var);
    // Should always succeed, the only way it could fail is if the token
    // doesn't match.
    assert(Succeeded(result));
    WABT_USE(result);
    return true;
  } else {
    *out_var = default_var;
    return false;
  }
}

Result WastParser::ParseOffsetExpr(ExprList* out_expr_list) {
  WABT_TRACE(ParseOffsetExpr);
  if (!ParseOffsetExprOpt(out_expr_list)) {
    return ErrorExpected({"an offset expr"}, "(i32.const 123)");
  }
  return Result::Ok;
}

bool WastParser::ParseOffsetExprOpt(ExprList* out_expr_list) {
  WABT_TRACE(ParseOffsetExprOpt);
  if (MatchLpar(TokenType::Offset)) {
    CHECK_RESULT(ParseTerminatingInstrList(out_expr_list));
    EXPECT(Rpar);
    return true;
  } else if (PeekMatchExpr()) {
    CHECK_RESULT(ParseExpr(out_expr_list));
    return true;
  } else {
    return false;
  }
}

Result WastParser::ParseTextList(std::vector<uint8_t>* out_data) {
  WABT_TRACE(ParseTextList);
  if (!ParseTextListOpt(out_data)) {
    // TODO(binji): Add error message here.
    return Result::Error;
  }

  return Result::Ok;
}

bool WastParser::ParseTextListOpt(std::vector<uint8_t>* out_data) {
  WABT_TRACE(ParseTextListOpt);
  TextVector texts;
  while (PeekMatch(TokenType::Text))
    texts.push_back(Consume().text());

  RemoveEscapes(texts, std::back_inserter(*out_data));
  return !texts.empty();
}

Result WastParser::ParseVarList(VarVector* out_var_list) {
  WABT_TRACE(ParseVarList);
  Var var;
  while (ParseVarOpt(&var)) {
    out_var_list->emplace_back(var);
  }
  if (out_var_list->empty()) {
    return ErrorExpected({"a var"}, "12 or $foo");
  } else {
    return Result::Ok;
  }
}

bool WastParser::ParseElemExprOpt(ElemExpr* out_elem_expr) {
  Location loc = GetLocation();
  bool item = MatchLpar(TokenType::Item);
  bool lpar = Match(TokenType::Lpar);
  if (Match(TokenType::RefNull)) {
    if (!(options_->features.bulk_memory_enabled() ||
          options_->features.reference_types_enabled())) {
      Error(loc, "ref.null not allowed");
    }
    Type type;
    CHECK_RESULT(ParseRefKind(&type));
    *out_elem_expr = ElemExpr(type);
  } else if (Match(TokenType::RefFunc)) {
    Var var;
    CHECK_RESULT(ParseVar(&var));
    *out_elem_expr = ElemExpr(var);
  } else {
    return false;
  }
  if (lpar) {
    EXPECT(Rpar);
  }
  if (item) {
    EXPECT(Rpar);
  }
  return true;
}

bool WastParser::ParseElemExprListOpt(ElemExprVector* out_list) {
  ElemExpr elem_expr;
  while (ParseElemExprOpt(&elem_expr)) {
    out_list->push_back(elem_expr);
  }
  return !out_list->empty();
}

bool WastParser::ParseElemExprVarListOpt(ElemExprVector* out_list) {
  WABT_TRACE(ParseElemExprVarListOpt);
  Var var;
  while (ParseVarOpt(&var)) {
    out_list->emplace_back(var);
  }
  return !out_list->empty();
}

Result WastParser::ParseValueType(Type* out_type) {
  WABT_TRACE(ParseValueType);
  if (!PeekMatch(TokenType::ValueType)) {
    return ErrorExpected({"i32", "i64", "f32", "f64", "v128", "externref"});
  }

  Token token = Consume();
  Type type = token.type();
  bool is_enabled;
  switch (type) {
    case Type::V128:
      is_enabled = options_->features.simd_enabled();
      break;
    case Type::FuncRef:
    case Type::ExternRef:
      is_enabled = options_->features.reference_types_enabled();
      break;
    default:
      is_enabled = true;
      break;
  }

  if (!is_enabled) {
    Error(token.loc, "value type not allowed: %s", type.GetName());
    return Result::Error;
  }

  *out_type = type;
  return Result::Ok;
}

Result WastParser::ParseValueTypeList(TypeVector* out_type_list) {
  WABT_TRACE(ParseValueTypeList);
  while (PeekMatch(TokenType::ValueType))
    out_type_list->push_back(Consume().type());

  return Result::Ok;
}

Result WastParser::ParseRefKind(Type* out_type) {
  WABT_TRACE(ParseRefKind);
  if (!IsTokenTypeRefKind(Peek())) {
    return ErrorExpected({"func", "extern", "exn"});
  }

  Token token = Consume();
  Type type = token.type();

  if ((type == Type::ExternRef &&
       !options_->features.reference_types_enabled()) ||
      ((type == Type::Struct || type == Type::Array) &&
       !options_->features.gc_enabled())) {
    Error(token.loc, "value type not allowed: %s", type.GetName());
    return Result::Error;
  }

  *out_type = type;
  return Result::Ok;
}

Result WastParser::ParseRefType(Type* out_type) {
  WABT_TRACE(ParseRefType);
  if (!PeekMatch(TokenType::ValueType)) {
    return ErrorExpected({"funcref", "externref"});
  }

  Token token = Consume();
  Type type = token.type();
  if (type == Type::ExternRef &&
      !options_->features.reference_types_enabled()) {
    Error(token.loc, "value type not allowed: %s", type.GetName());
    return Result::Error;
  }

  *out_type = type;
  return Result::Ok;
}

bool WastParser::ParseRefTypeOpt(Type* out_type) {
  WABT_TRACE(ParseRefTypeOpt);
  if (!PeekMatch(TokenType::ValueType)) {
    return false;
  }

  Token token = Consume();
  Type type = token.type();
  if (type == Type::ExternRef &&
      !options_->features.reference_types_enabled()) {
    return false;
  }

  *out_type = type;
  return true;
}

Result WastParser::ParseQuotedText(std::string* text) {
  WABT_TRACE(ParseQuotedText);
  if (!PeekMatch(TokenType::Text)) {
    return ErrorExpected({"a quoted string"}, "\"foo\"");
  }

  Token token = Consume();
  RemoveEscapes(token.text(), std::back_inserter(*text));
  if (!IsValidUtf8(text->data(), text->length())) {
    Error(token.loc, "quoted string has an invalid utf-8 encoding");
  }
  return Result::Ok;
}

bool WastParser::ParseOffsetOpt(Address* out_offset) {
  WABT_TRACE(ParseOffsetOpt);
  if (PeekMatch(TokenType::OffsetEqNat)) {
    Token token = Consume();
    uint64_t offset64;
    string_view sv = token.text();
    if (Failed(ParseInt64(sv.begin(), sv.end(), &offset64,
                          ParseIntType::SignedAndUnsigned))) {
      Error(token.loc, "invalid offset \"" PRIstringview "\"",
            WABT_PRINTF_STRING_VIEW_ARG(sv));
    }
    // FIXME: make this depend on the current memory.
    if (offset64 > UINT32_MAX) {
      Error(token.loc, "offset must be less than or equal to 0xffffffff");
    }
    *out_offset = offset64;
    return true;
  } else {
    *out_offset = 0;
    return false;
  }
}

bool WastParser::ParseAlignOpt(Address* out_align) {
  WABT_TRACE(ParseAlignOpt);
  if (PeekMatch(TokenType::AlignEqNat)) {
    Token token = Consume();
    string_view sv = token.text();
    if (Failed(ParseInt64(sv.begin(), sv.end(), out_align,
                          ParseIntType::UnsignedOnly))) {
      Error(token.loc, "invalid alignment \"" PRIstringview "\"",
            WABT_PRINTF_STRING_VIEW_ARG(sv));
    }

    if (!IsPowerOfTwo(*out_align)) {
      Error(token.loc, "alignment must be power-of-two");
    }

    return true;
  } else {
    *out_align = WABT_USE_NATURAL_ALIGNMENT;
    return false;
  }
}

Result WastParser::ParseLimitsIndex(Limits* out_limits) {
  WABT_TRACE(ParseLimitsIndex);

  if (PeekMatch(TokenType::ValueType)) {
    if (GetToken().type() == Type::I64) {
      Consume();
      out_limits->is_64 = true;
    } else if (GetToken().type() == Type::I32) {
      Consume();
      out_limits->is_64 = false;
    }
  }

  return Result::Ok;
}

Result WastParser::ParseLimits(Limits* out_limits) {
  WABT_TRACE(ParseLimits);

  CHECK_RESULT(ParseNat(&out_limits->initial));
  if (PeekMatch(TokenType::Nat)) {
    CHECK_RESULT(ParseNat(&out_limits->max));
    out_limits->has_max = true;
  } else {
    out_limits->has_max = false;
  }

  if (Match(TokenType::Shared)) {
    out_limits->is_shared = true;
  }

  return Result::Ok;
}

Result WastParser::ParseNat(uint64_t* out_nat) {
  WABT_TRACE(ParseNat);
  if (!PeekMatch(TokenType::Nat)) {
    return ErrorExpected({"a natural number"}, "123");
  }

  Token token = Consume();
  string_view sv = token.literal().text;
  if (Failed(ParseUint64(sv.begin(), sv.end(), out_nat)) ||
      *out_nat > 0xffffffffu) {
    Error(token.loc, "invalid int \"" PRIstringview "\"",
          WABT_PRINTF_STRING_VIEW_ARG(sv));
  }

  return Result::Ok;
}

Result WastParser::ParseModule(std::unique_ptr<Module>* out_module) {
  WABT_TRACE(ParseModule);
  auto module = MakeUnique<Module>();

  if (PeekMatchLpar(TokenType::Module)) {
    // Starts with "(module". Allow text and binary modules, but no quoted
    // modules.
    CommandPtr command;
    CHECK_RESULT(ParseModuleCommand(nullptr, &command));
    auto module_command = cast<ModuleCommand>(std::move(command));
    *module = std::move(module_command->module);
  } else if (IsModuleField(PeekPair())) {
    // Parse an inline module (i.e. one with no surrounding (module)).
    CHECK_RESULT(ParseModuleFieldList(module.get()));
  } else {
    ConsumeIfLpar();
    ErrorExpected({"a module field", "a module"});
  }

  EXPECT(Eof);
  if (errors_->size() == 0) {
    *out_module = std::move(module);
    return Result::Ok;
  } else {
    return Result::Error;
  }
}

Result WastParser::ParseScript(std::unique_ptr<Script>* out_script) {
  WABT_TRACE(ParseScript);
  auto script = MakeUnique<Script>();

  // Don't consume the Lpar yet, even though it is required. This way the
  // sub-parser functions (e.g. ParseFuncModuleField) can consume it and keep
  // the parsing structure more regular.
  if (IsModuleField(PeekPair())) {
    // Parse an inline module (i.e. one with no surrounding (module)).
    auto command = MakeUnique<ModuleCommand>();
    command->module.loc = GetLocation();
    CHECK_RESULT(ParseModuleFieldList(&command->module));
    script->commands.emplace_back(std::move(command));
  } else if (IsCommand(PeekPair())) {
    CHECK_RESULT(ParseCommandList(script.get(), &script->commands));
  } else {
    ConsumeIfLpar();
    ErrorExpected({"a module field", "a command"});
  }

  EXPECT(Eof);
  if (errors_->size() == 0) {
    *out_script = std::move(script);
    return Result::Ok;
  } else {
    return Result::Error;
  }
}

Result WastParser::ParseModuleFieldList(Module* module) {
  WABT_TRACE(ParseModuleFieldList);
  while (IsModuleField(PeekPair())) {
    if (Failed(ParseModuleField(module))) {
      CHECK_RESULT(Synchronize(IsModuleField));
    }
  }
  CHECK_RESULT(ResolveFuncTypes(module, errors_));
  CHECK_RESULT(ResolveNamesModule(module, errors_));
  return Result::Ok;
}

Result WastParser::ParseModuleField(Module* module) {
  WABT_TRACE(ParseModuleField);
  switch (Peek(1)) {
    case TokenType::Data:   return ParseDataModuleField(module);
    case TokenType::Elem:   return ParseElemModuleField(module);
    case TokenType::Event:  return ParseEventModuleField(module);
    case TokenType::Export: return ParseExportModuleField(module);
    case TokenType::Func:   return ParseFuncModuleField(module);
    case TokenType::Type:   return ParseTypeModuleField(module);
    case TokenType::Global: return ParseGlobalModuleField(module);
    case TokenType::Import: return ParseImportModuleField(module);
    case TokenType::Memory: return ParseMemoryModuleField(module);
    case TokenType::Start:  return ParseStartModuleField(module);
    case TokenType::Table:  return ParseTableModuleField(module);
    default:
      assert(
          !"ParseModuleField should only be called if IsModuleField() is true");
      return Result::Error;
  }
}

Result WastParser::ParseDataModuleField(Module* module) {
  WABT_TRACE(ParseDataModuleField);
  EXPECT(Lpar);
  Location loc = GetLocation();
  EXPECT(Data);
  std::string name;
  ParseBindVarOpt(&name);
  auto field = MakeUnique<DataSegmentModuleField>(loc, name);

  if (PeekMatchLpar(TokenType::Memory)) {
    EXPECT(Lpar);
    EXPECT(Memory);
    CHECK_RESULT(ParseVar(&field->data_segment.memory_var));
    EXPECT(Rpar);
    CHECK_RESULT(ParseOffsetExpr(&field->data_segment.offset));
  } else if (ParseVarOpt(&field->data_segment.memory_var, Var(0, loc))) {
    CHECK_RESULT(ParseOffsetExpr(&field->data_segment.offset));
  } else if (!ParseOffsetExprOpt(&field->data_segment.offset)) {
    if (!options_->features.bulk_memory_enabled()) {
      Error(loc, "passive data segments are not allowed");
      return Result::Error;
    }

    field->data_segment.kind = SegmentKind::Passive;
  }

  ParseTextListOpt(&field->data_segment.data);
  EXPECT(Rpar);
  module->AppendField(std::move(field));
  return Result::Ok;
}

Result WastParser::ParseElemModuleField(Module* module) {
  WABT_TRACE(ParseElemModuleField);
  EXPECT(Lpar);
  Location loc = GetLocation();
  EXPECT(Elem);

  // With MVP text format the name here was intended to refer to the table
  // that the elem segment was part of, but we never did anything with this name
  // since there was only one table anyway.
  // With bulk-memory enabled this introduces a new name for the particular
  // elem segment.
  std::string initial_name;
  bool has_name = ParseBindVarOpt(&initial_name);

  std::string segment_name = initial_name;
  if (!options_->features.bulk_memory_enabled()) {
    segment_name = "";
  }
  auto field = MakeUnique<ElemSegmentModuleField>(loc, segment_name);
  if (options_->features.reference_types_enabled() &&
      Match(TokenType::Declare)) {
    field->elem_segment.kind = SegmentKind::Declared;
  }

  // Optional table specifier
  if (options_->features.bulk_memory_enabled()) {
    if (PeekMatchLpar(TokenType::Table)) {
      EXPECT(Lpar);
      EXPECT(Table);
      CHECK_RESULT(ParseVar(&field->elem_segment.table_var));
      EXPECT(Rpar);
    } else {
      ParseVarOpt(&field->elem_segment.table_var, Var(0, loc));
    }
  } else {
    if (has_name) {
      field->elem_segment.table_var = Var(initial_name, loc);
    } else {
      ParseVarOpt(&field->elem_segment.table_var, Var(0, loc));
    }
  }

  // Parse offset expression, if not declared/passive segment.
  if (options_->features.bulk_memory_enabled()) {
    if (field->elem_segment.kind != SegmentKind::Declared &&
        !ParseOffsetExprOpt(&field->elem_segment.offset)) {
      field->elem_segment.kind = SegmentKind::Passive;
    }
  } else {
    CHECK_RESULT(ParseOffsetExpr(&field->elem_segment.offset));
  }

  if (ParseRefTypeOpt(&field->elem_segment.elem_type)) {
    ParseElemExprListOpt(&field->elem_segment.elem_exprs);
  } else {
    field->elem_segment.elem_type = Type::FuncRef;
    if (PeekMatch(TokenType::Func)) {
      EXPECT(Func);
    }
    ParseElemExprVarListOpt(&field->elem_segment.elem_exprs);
  }
  EXPECT(Rpar);
  module->AppendField(std::move(field));
  return Result::Ok;
}

Result WastParser::ParseEventModuleField(Module* module) {
  WABT_TRACE(ParseEventModuleField);
  EXPECT(Lpar);
  auto field = MakeUnique<EventModuleField>(GetLocation());
  EXPECT(Event);
  ParseBindVarOpt(&field->event.name);
  CHECK_RESULT(ParseTypeUseOpt(&field->event.decl));
  CHECK_RESULT(ParseUnboundFuncSignature(&field->event.decl.sig));
  EXPECT(Rpar);
  module->AppendField(std::move(field));
  return Result::Ok;
}

Result WastParser::ParseExportModuleField(Module* module) {
  WABT_TRACE(ParseExportModuleField);
  EXPECT(Lpar);
  auto field = MakeUnique<ExportModuleField>(GetLocation());
  EXPECT(Export);
  CHECK_RESULT(ParseQuotedText(&field->export_.name));
  CHECK_RESULT(ParseExportDesc(&field->export_));
  EXPECT(Rpar);
  module->AppendField(std::move(field));
  return Result::Ok;
}

Result WastParser::ParseFuncModuleField(Module* module) {
  WABT_TRACE(ParseFuncModuleField);
  EXPECT(Lpar);
  Location loc = GetLocation();
  EXPECT(Func);
  std::string name;
  ParseBindVarOpt(&name);

  ModuleFieldList export_fields;
  CHECK_RESULT(ParseInlineExports(&export_fields, ExternalKind::Func));

  if (PeekMatchLpar(TokenType::Import)) {
    CheckImportOrdering(module);
    auto import = MakeUnique<FuncImport>(name);
    Func& func = import->func;
    CHECK_RESULT(ParseInlineImport(import.get()));
    CHECK_RESULT(ParseTypeUseOpt(&func.decl));
    CHECK_RESULT(ParseFuncSignature(&func.decl.sig, &func.bindings));
    CHECK_RESULT(ErrorIfLpar({"type", "param", "result"}));
    auto field =
        MakeUnique<ImportModuleField>(std::move(import), GetLocation());
    module->AppendField(std::move(field));
  } else {
    auto field = MakeUnique<FuncModuleField>(loc, name);
    Func& func = field->func;
    CHECK_RESULT(ParseTypeUseOpt(&func.decl));
    CHECK_RESULT(ParseFuncSignature(&func.decl.sig, &func.bindings));
    TypeVector local_types;
    CHECK_RESULT(ParseBoundValueTypeList(TokenType::Local, &local_types,
                                         &func.bindings, func.GetNumParams()));
    func.local_types.Set(local_types);
    CHECK_RESULT(ParseTerminatingInstrList(&func.exprs));
    module->AppendField(std::move(field));
  }

  AppendInlineExportFields(module, &export_fields, module->funcs.size() - 1);

  EXPECT(Rpar);
  return Result::Ok;
}

Result WastParser::ParseTypeModuleField(Module* module) {
  WABT_TRACE(ParseTypeModuleField);
  EXPECT(Lpar);
  auto field = MakeUnique<TypeModuleField>(GetLocation());
  EXPECT(Type);

  std::string name;
  ParseBindVarOpt(&name);
  EXPECT(Lpar);
  Location loc = GetLocation();

  if (Match(TokenType::Func)) {
    auto func_type = MakeUnique<FuncType>(name);
    BindingHash bindings;
    CHECK_RESULT(ParseFuncSignature(&func_type->sig, &bindings));
    CHECK_RESULT(ErrorIfLpar({"param", "result"}));
    field->type = std::move(func_type);
  } else if (Match(TokenType::Struct)) {
    if (!options_->features.gc_enabled()) {
      Error(loc, "struct not allowed");
      return Result::Error;
    }
    auto struct_type = MakeUnique<StructType>(name);
    CHECK_RESULT(ParseFieldList(&struct_type->fields));
    field->type = std::move(struct_type);
  } else if (Match(TokenType::Array)) {
    if (!options_->features.gc_enabled()) {
      Error(loc, "array type not allowed");
    }
    auto array_type = MakeUnique<ArrayType>(name);
    CHECK_RESULT(ParseField(&array_type->field));
    field->type = std::move(array_type);
  } else {
    return ErrorExpected({"func", "struct", "array"});
  }

  EXPECT(Rpar);
  EXPECT(Rpar);
  module->AppendField(std::move(field));
  return Result::Ok;
}

Result WastParser::ParseField(Field* field) {
  WABT_TRACE(ParseField);
  auto parse_mut_valuetype = [&]() -> Result {
    // TODO: Share with ParseGlobalType?
    if (MatchLpar(TokenType::Mut)) {
      field->mutable_ = true;
      CHECK_RESULT(ParseValueType(&field->type));
      EXPECT(Rpar);
    } else {
      field->mutable_ = false;
      CHECK_RESULT(ParseValueType(&field->type));
    }
    return Result::Ok;
  };

  if (MatchLpar(TokenType::Field)) {
    ParseBindVarOpt(&field->name);
    CHECK_RESULT(parse_mut_valuetype());
    EXPECT(Rpar);
  } else {
    CHECK_RESULT(parse_mut_valuetype());
  }

  return Result::Ok;
}

Result WastParser::ParseFieldList(std::vector<Field>* fields) {
  WABT_TRACE(ParseFieldList);
  while (PeekMatch(TokenType::ValueType) || PeekMatch(TokenType::Lpar)) {
    Field field;
    CHECK_RESULT(ParseField(&field));
    fields->push_back(field);
  }
  return Result::Ok;
}

Result WastParser::ParseGlobalModuleField(Module* module) {
  WABT_TRACE(ParseGlobalModuleField);
  EXPECT(Lpar);
  Location loc = GetLocation();
  EXPECT(Global);
  std::string name;
  ParseBindVarOpt(&name);

  ModuleFieldList export_fields;
  CHECK_RESULT(ParseInlineExports(&export_fields, ExternalKind::Global));

  if (PeekMatchLpar(TokenType::Import)) {
    CheckImportOrdering(module);
    auto import = MakeUnique<GlobalImport>(name);
    CHECK_RESULT(ParseInlineImport(import.get()));
    CHECK_RESULT(ParseGlobalType(&import->global));
    auto field =
        MakeUnique<ImportModuleField>(std::move(import), GetLocation());
    module->AppendField(std::move(field));
  } else {
    auto field = MakeUnique<GlobalModuleField>(loc, name);
    CHECK_RESULT(ParseGlobalType(&field->global));
    CHECK_RESULT(ParseTerminatingInstrList(&field->global.init_expr));
    module->AppendField(std::move(field));
  }

  AppendInlineExportFields(module, &export_fields, module->globals.size() - 1);

  EXPECT(Rpar);
  return Result::Ok;
}

Result WastParser::ParseImportModuleField(Module* module) {
  WABT_TRACE(ParseImportModuleField);
  EXPECT(Lpar);
  Location loc = GetLocation();
  CheckImportOrdering(module);
  EXPECT(Import);
  std::string module_name;
  std::string field_name;
  CHECK_RESULT(ParseQuotedText(&module_name));
  CHECK_RESULT(ParseQuotedText(&field_name));
  EXPECT(Lpar);

  std::unique_ptr<ImportModuleField> field;
  std::string name;

  switch (Peek()) {
    case TokenType::Func: {
      Consume();
      ParseBindVarOpt(&name);
      auto import = MakeUnique<FuncImport>(name);
      if (PeekMatchLpar(TokenType::Type)) {
        import->func.decl.has_func_type = true;
        CHECK_RESULT(ParseTypeUseOpt(&import->func.decl));
        EXPECT(Rpar);
      } else {
        CHECK_RESULT(
            ParseFuncSignature(&import->func.decl.sig, &import->func.bindings));
        CHECK_RESULT(ErrorIfLpar({"param", "result"}));
        EXPECT(Rpar);
      }
      field = MakeUnique<ImportModuleField>(std::move(import), loc);
      break;
    }

    case TokenType::Table: {
      Consume();
      ParseBindVarOpt(&name);
      auto import = MakeUnique<TableImport>(name);
      CHECK_RESULT(ParseLimits(&import->table.elem_limits));
      CHECK_RESULT(ParseRefType(&import->table.elem_type));
      EXPECT(Rpar);
      field = MakeUnique<ImportModuleField>(std::move(import), loc);
      break;
    }

    case TokenType::Memory: {
      Consume();
      ParseBindVarOpt(&name);
      auto import = MakeUnique<MemoryImport>(name);
      CHECK_RESULT(ParseLimitsIndex(&import->memory.page_limits));
      CHECK_RESULT(ParseLimits(&import->memory.page_limits));
      EXPECT(Rpar);
      field = MakeUnique<ImportModuleField>(std::move(import), loc);
      break;
    }

    case TokenType::Global: {
      Consume();
      ParseBindVarOpt(&name);
      auto import = MakeUnique<GlobalImport>(name);
      CHECK_RESULT(ParseGlobalType(&import->global));
      EXPECT(Rpar);
      field = MakeUnique<ImportModuleField>(std::move(import), loc);
      break;
    }

    case TokenType::Event: {
      Consume();
      ParseBindVarOpt(&name);
      auto import = MakeUnique<EventImport>(name);
      CHECK_RESULT(ParseTypeUseOpt(&import->event.decl));
      CHECK_RESULT(ParseUnboundFuncSignature(&import->event.decl.sig));
      EXPECT(Rpar);
      field = MakeUnique<ImportModuleField>(std::move(import), loc);
      break;
    }

    default:
      return ErrorExpected({"an external kind"});
  }

  field->import->module_name = module_name;
  field->import->field_name = field_name;

  module->AppendField(std::move(field));
  EXPECT(Rpar);
  return Result::Ok;
}

Result WastParser::ParseMemoryModuleField(Module* module) {
  WABT_TRACE(ParseMemoryModuleField);
  EXPECT(Lpar);
  Location loc = GetLocation();
  EXPECT(Memory);
  std::string name;
  ParseBindVarOpt(&name);

  ModuleFieldList export_fields;
  CHECK_RESULT(ParseInlineExports(&export_fields, ExternalKind::Memory));

  if (PeekMatchLpar(TokenType::Import)) {
    CheckImportOrdering(module);
    auto import = MakeUnique<MemoryImport>(name);
    CHECK_RESULT(ParseInlineImport(import.get()));
    CHECK_RESULT(ParseLimitsIndex(&import->memory.page_limits));
    CHECK_RESULT(ParseLimits(&import->memory.page_limits));
    auto field =
        MakeUnique<ImportModuleField>(std::move(import), GetLocation());
    module->AppendField(std::move(field));
  } else {
    auto field = MakeUnique<MemoryModuleField>(loc, name);
    CHECK_RESULT(ParseLimitsIndex(&field->memory.page_limits));
    if (MatchLpar(TokenType::Data)) {
      auto data_segment_field = MakeUnique<DataSegmentModuleField>(loc);
      DataSegment& data_segment = data_segment_field->data_segment;
      data_segment.memory_var = Var(module->memories.size());
      data_segment.offset.push_back(MakeUnique<ConstExpr>(
          field->memory.page_limits.is_64 ? Const::I64(0) : Const::I32(0)));
      data_segment.offset.back().loc = loc;
      ParseTextListOpt(&data_segment.data);
      EXPECT(Rpar);

      uint32_t byte_size = WABT_ALIGN_UP_TO_PAGE(data_segment.data.size());
      uint32_t page_size = WABT_BYTES_TO_PAGES(byte_size);
      field->memory.page_limits.initial = page_size;
      field->memory.page_limits.max = page_size;
      field->memory.page_limits.has_max = true;

      module->AppendField(std::move(field));
      module->AppendField(std::move(data_segment_field));
    } else {
      CHECK_RESULT(ParseLimits(&field->memory.page_limits));
      module->AppendField(std::move(field));
    }
  }

  AppendInlineExportFields(module, &export_fields, module->memories.size() - 1);

  EXPECT(Rpar);
  return Result::Ok;
}

Result WastParser::ParseStartModuleField(Module* module) {
  WABT_TRACE(ParseStartModuleField);
  EXPECT(Lpar);
  Location loc = GetLocation();
  if (module->starts.size() > 0) {
    Error(loc, "multiple start sections");
    return Result::Error;
  }
  EXPECT(Start);
  Var var;
  CHECK_RESULT(ParseVar(&var));
  EXPECT(Rpar);
  module->AppendField(MakeUnique<StartModuleField>(var, loc));
  return Result::Ok;
}

Result WastParser::ParseTableModuleField(Module* module) {
  WABT_TRACE(ParseTableModuleField);
  EXPECT(Lpar);
  Location loc = GetLocation();
  EXPECT(Table);
  std::string name;
  ParseBindVarOpt(&name);

  ModuleFieldList export_fields;
  CHECK_RESULT(ParseInlineExports(&export_fields, ExternalKind::Table));

  if (PeekMatchLpar(TokenType::Import)) {
    CheckImportOrdering(module);
    auto import = MakeUnique<TableImport>(name);
    CHECK_RESULT(ParseInlineImport(import.get()));
    CHECK_RESULT(ParseLimits(&import->table.elem_limits));
    CHECK_RESULT(ParseRefType(&import->table.elem_type));
    auto field =
        MakeUnique<ImportModuleField>(std::move(import), GetLocation());
    module->AppendField(std::move(field));
  } else if (PeekMatch(TokenType::ValueType)) {
    Type elem_type;
    CHECK_RESULT(ParseRefType(&elem_type));

    EXPECT(Lpar);
    EXPECT(Elem);

    auto elem_segment_field = MakeUnique<ElemSegmentModuleField>(loc);
    ElemSegment& elem_segment = elem_segment_field->elem_segment;
    elem_segment.table_var = Var(module->tables.size());
    elem_segment.offset.push_back(MakeUnique<ConstExpr>(Const::I32(0)));
    elem_segment.offset.back().loc = loc;
    elem_segment.elem_type = elem_type;
    // Syntax is either an optional list of var (legacy), or a non-empty list
    // of elem expr.
    ElemExpr elem_expr;
    if (ParseElemExprOpt(&elem_expr)) {
      elem_segment.elem_exprs.push_back(elem_expr);
      // Parse the rest.
      ParseElemExprListOpt(&elem_segment.elem_exprs);
    } else {
      ParseElemExprVarListOpt(&elem_segment.elem_exprs);
    }
    EXPECT(Rpar);

    auto table_field = MakeUnique<TableModuleField>(loc, name);
    table_field->table.elem_limits.initial = elem_segment.elem_exprs.size();
    table_field->table.elem_limits.max = elem_segment.elem_exprs.size();
    table_field->table.elem_limits.has_max = true;
    table_field->table.elem_type = elem_type;
    module->AppendField(std::move(table_field));
    module->AppendField(std::move(elem_segment_field));
  } else {
    auto field = MakeUnique<TableModuleField>(loc, name);
    CHECK_RESULT(ParseLimits(&field->table.elem_limits));
    CHECK_RESULT(ParseRefType(&field->table.elem_type));
    module->AppendField(std::move(field));
  }

  AppendInlineExportFields(module, &export_fields, module->tables.size() - 1);

  EXPECT(Rpar);
  return Result::Ok;
}

Result WastParser::ParseExportDesc(Export* export_) {
  WABT_TRACE(ParseExportDesc);
  EXPECT(Lpar);
  switch (Peek()) {
    case TokenType::Func:   export_->kind = ExternalKind::Func; break;
    case TokenType::Table:  export_->kind = ExternalKind::Table; break;
    case TokenType::Memory: export_->kind = ExternalKind::Memory; break;
    case TokenType::Global: export_->kind = ExternalKind::Global; break;
    case TokenType::Event:  export_->kind = ExternalKind::Event; break;
    default:
      return ErrorExpected({"an external kind"});
  }
  Consume();
  CHECK_RESULT(ParseVar(&export_->var));
  EXPECT(Rpar);
  return Result::Ok;
}

Result WastParser::ParseInlineExports(ModuleFieldList* fields,
                                      ExternalKind kind) {
  WABT_TRACE(ParseInlineExports);
  while (PeekMatchLpar(TokenType::Export)) {
    EXPECT(Lpar);
    auto field = MakeUnique<ExportModuleField>(GetLocation());
    field->export_.kind = kind;
    EXPECT(Export);
    CHECK_RESULT(ParseQuotedText(&field->export_.name));
    EXPECT(Rpar);
    fields->push_back(std::move(field));
  }
  return Result::Ok;
}

Result WastParser::ParseInlineImport(Import* import) {
  WABT_TRACE(ParseInlineImport);
  EXPECT(Lpar);
  EXPECT(Import);
  CHECK_RESULT(ParseQuotedText(&import->module_name));
  CHECK_RESULT(ParseQuotedText(&import->field_name));
  EXPECT(Rpar);
  return Result::Ok;
}

Result WastParser::ParseTypeUseOpt(FuncDeclaration* decl) {
  WABT_TRACE(ParseTypeUseOpt);
  if (MatchLpar(TokenType::Type)) {
    decl->has_func_type = true;
    CHECK_RESULT(ParseVar(&decl->type_var));
    EXPECT(Rpar);
  } else {
    decl->has_func_type = false;
  }
  return Result::Ok;
}

Result WastParser::ParseFuncSignature(FuncSignature* sig,
                                      BindingHash* param_bindings) {
  WABT_TRACE(ParseFuncSignature);
  CHECK_RESULT(ParseBoundValueTypeList(TokenType::Param, &sig->param_types,
                                       param_bindings));
  CHECK_RESULT(ParseResultList(&sig->result_types));
  return Result::Ok;
}

Result WastParser::ParseUnboundFuncSignature(FuncSignature* sig) {
  WABT_TRACE(ParseUnboundFuncSignature);
  CHECK_RESULT(ParseUnboundValueTypeList(TokenType::Param, &sig->param_types));
  CHECK_RESULT(ParseResultList(&sig->result_types));
  return Result::Ok;
}

Result WastParser::ParseBoundValueTypeList(TokenType token,
                                           TypeVector* types,
                                           BindingHash* bindings,
                                           Index binding_index_offset) {
  WABT_TRACE(ParseBoundValueTypeList);
  while (MatchLpar(token)) {
    if (PeekMatch(TokenType::Var)) {
      std::string name;
      Type type;
      Location loc = GetLocation();
      ParseBindVarOpt(&name);
      CHECK_RESULT(ParseValueType(&type));
      bindings->emplace(name,
                        Binding(loc, binding_index_offset + types->size()));
      types->push_back(type);
    } else {
      CHECK_RESULT(ParseValueTypeList(types));
    }
    EXPECT(Rpar);
  }
  return Result::Ok;
}

Result WastParser::ParseUnboundValueTypeList(TokenType token,
                                             TypeVector* types) {
  WABT_TRACE(ParseUnboundValueTypeList);
  while (MatchLpar(token)) {
    CHECK_RESULT(ParseValueTypeList(types));
    EXPECT(Rpar);
  }
  return Result::Ok;
}

Result WastParser::ParseResultList(TypeVector* result_types) {
  WABT_TRACE(ParseResultList);
  return ParseUnboundValueTypeList(TokenType::Result, result_types);
}

Result WastParser::ParseInstrList(ExprList* exprs) {
  WABT_TRACE(ParseInstrList);
  ExprList new_exprs;
  while (IsInstr(PeekPair())) {
    if (Succeeded(ParseInstr(&new_exprs))) {
      exprs->splice(exprs->end(), new_exprs);
    } else {
      CHECK_RESULT(Synchronize(IsInstr));
    }
  }
  return Result::Ok;
}

Result WastParser::ParseTerminatingInstrList(ExprList* exprs) {
  WABT_TRACE(ParseTerminatingInstrList);
  Result result = ParseInstrList(exprs);
  // An InstrList often has no further Lpar following it, because it would have
  // gobbled it up. So if there is a following Lpar it is an error. If we
  // handle it here we can produce a nicer error message.
  CHECK_RESULT(ErrorIfLpar({"an instr"}));
  return result;
}

Result WastParser::ParseInstr(ExprList* exprs) {
  WABT_TRACE(ParseInstr);
  if (IsPlainInstr(Peek())) {
    std::unique_ptr<Expr> expr;
    CHECK_RESULT(ParsePlainInstr(&expr));
    exprs->push_back(std::move(expr));
    return Result::Ok;
  } else if (IsBlockInstr(Peek())) {
    std::unique_ptr<Expr> expr;
    CHECK_RESULT(ParseBlockInstr(&expr));
    exprs->push_back(std::move(expr));
    return Result::Ok;
  } else if (PeekMatchExpr()) {
    return ParseExpr(exprs);
  } else {
    assert(!"ParseInstr should only be called when IsInstr() is true");
    return Result::Error;
  }
}

template <typename T>
Result WastParser::ParsePlainInstrVar(Location loc,
                                      std::unique_ptr<Expr>* out_expr) {
  Var var;
  CHECK_RESULT(ParseVar(&var));
  out_expr->reset(new T(var, loc));
  return Result::Ok;
}

template <typename T>
Result WastParser::ParsePlainLoadStoreInstr(Location loc,
                                            Token token,
                                            std::unique_ptr<Expr>* out_expr) {
  Opcode opcode = token.opcode();
  Address offset;
  Address align;
  ParseOffsetOpt(&offset);
  ParseAlignOpt(&align);
  out_expr->reset(new T(opcode, align, offset, loc));
  return Result::Ok;
}

Result WastParser::ParseSimdLane(Location loc, uint64_t* lane_idx) {
  if (!PeekMatch(TokenType::Nat) && !PeekMatch(TokenType::Int)) {
    return ErrorExpected({"a natural number in range [0, 32)"});
  }

  Literal literal = Consume().literal();

  Result result = ParseInt64(literal.text.begin(), literal.text.end(),
                             lane_idx, ParseIntType::UnsignedOnly);

  if (Failed(result)) {
    Error(loc, "invalid literal \"" PRIstringview "\"",
          WABT_PRINTF_STRING_VIEW_ARG(literal.text));
    return Result::Error;
  }

  // The valid range is only [0, 32), but it's only malformed if it can't
  // fit in a byte.
  if (*lane_idx > 255) {
    Error(loc, "lane index \"" PRIstringview "\" out-of-range [0, 32)",
          WABT_PRINTF_STRING_VIEW_ARG(literal.text));
    return Result::Error;
  }

  return Result::Ok;
}

Result WastParser::ParsePlainInstr(std::unique_ptr<Expr>* out_expr) {
  WABT_TRACE(ParsePlainInstr);
  Location loc = GetLocation();
  switch (Peek()) {
    case TokenType::Unreachable:
      Consume();
      out_expr->reset(new UnreachableExpr(loc));
      break;

    case TokenType::Nop:
      Consume();
      out_expr->reset(new NopExpr(loc));
      break;

    case TokenType::Drop:
      Consume();
      out_expr->reset(new DropExpr(loc));
      break;

    case TokenType::Select: {
      Consume();
      TypeVector result;
      if (options_->features.reference_types_enabled() &&
          MatchLpar(TokenType::Result)) {
        CHECK_RESULT(ParseValueTypeList(&result));
        EXPECT(Rpar);
      }
      out_expr->reset(new SelectExpr(result, loc));
      break;
    }

    case TokenType::Br:
      Consume();
      CHECK_RESULT(ParsePlainInstrVar<BrExpr>(loc, out_expr));
      break;

    case TokenType::BrIf:
      Consume();
      CHECK_RESULT(ParsePlainInstrVar<BrIfExpr>(loc, out_expr));
      break;

    case TokenType::BrTable: {
      Consume();
      auto expr = MakeUnique<BrTableExpr>(loc);
      CHECK_RESULT(ParseVarList(&expr->targets));
      expr->default_target = expr->targets.back();
      expr->targets.pop_back();
      *out_expr = std::move(expr);
      break;
    }

    case TokenType::Return:
      Consume();
      out_expr->reset(new ReturnExpr(loc));
      break;

    case TokenType::Call:
      Consume();
      CHECK_RESULT(ParsePlainInstrVar<CallExpr>(loc, out_expr));
      break;

    case TokenType::CallIndirect: {
      Consume();
      auto expr = MakeUnique<CallIndirectExpr>(loc);
      ParseVarOpt(&expr->table, Var(0, loc));
      CHECK_RESULT(ParseTypeUseOpt(&expr->decl));
      CHECK_RESULT(ParseUnboundFuncSignature(&expr->decl.sig));
      *out_expr = std::move(expr);
      break;
    }

    case TokenType::ReturnCall:
      ErrorUnlessOpcodeEnabled(Consume());
      CHECK_RESULT(ParsePlainInstrVar<ReturnCallExpr>(loc, out_expr));
      break;

    case TokenType::ReturnCallIndirect: {
      ErrorUnlessOpcodeEnabled(Consume());
      auto expr = MakeUnique<ReturnCallIndirectExpr>(loc);
      CHECK_RESULT(ParseTypeUseOpt(&expr->decl));
      CHECK_RESULT(ParseUnboundFuncSignature(&expr->decl.sig));
      ParseVarOpt(&expr->table, Var(0, loc));
      *out_expr = std::move(expr);
      break;
    }

    case TokenType::LocalGet:
      Consume();
      CHECK_RESULT(ParsePlainInstrVar<LocalGetExpr>(loc, out_expr));
      break;

    case TokenType::LocalSet:
      Consume();
      CHECK_RESULT(ParsePlainInstrVar<LocalSetExpr>(loc, out_expr));
      break;

    case TokenType::LocalTee:
      Consume();
      CHECK_RESULT(ParsePlainInstrVar<LocalTeeExpr>(loc, out_expr));
      break;

    case TokenType::GlobalGet:
      Consume();
      CHECK_RESULT(ParsePlainInstrVar<GlobalGetExpr>(loc, out_expr));
      break;

    case TokenType::GlobalSet:
      Consume();
      CHECK_RESULT(ParsePlainInstrVar<GlobalSetExpr>(loc, out_expr));
      break;

    case TokenType::Load:
      CHECK_RESULT(
          ParsePlainLoadStoreInstr<LoadExpr>(loc, Consume(), out_expr));
      break;

    case TokenType::Store:
      CHECK_RESULT(
          ParsePlainLoadStoreInstr<StoreExpr>(loc, Consume(), out_expr));
      break;

    case TokenType::Const: {
      Const const_;
      CHECK_RESULT(ParseConst(&const_, ConstType::Normal));
      out_expr->reset(new ConstExpr(const_, loc));
      break;
    }

    case TokenType::Unary: {
      Token token = Consume();
      ErrorUnlessOpcodeEnabled(token);
      out_expr->reset(new UnaryExpr(token.opcode(), loc));
      break;
    }

    case TokenType::Binary: {
      Token token = Consume();
      ErrorUnlessOpcodeEnabled(token);
      out_expr->reset(new BinaryExpr(token.opcode(), loc));
      break;
    }

    case TokenType::Compare:
      out_expr->reset(new CompareExpr(Consume().opcode(), loc));
      break;

    case TokenType::Convert: {
      Token token = Consume();
      ErrorUnlessOpcodeEnabled(token);
      out_expr->reset(new ConvertExpr(token.opcode(), loc));
      break;
    }

    case TokenType::MemoryCopy:
      ErrorUnlessOpcodeEnabled(Consume());
      out_expr->reset(new MemoryCopyExpr(loc));
      break;

    case TokenType::MemoryFill:
      ErrorUnlessOpcodeEnabled(Consume());
      out_expr->reset(new MemoryFillExpr(loc));
      break;

    case TokenType::DataDrop:
      ErrorUnlessOpcodeEnabled(Consume());
      CHECK_RESULT(ParsePlainInstrVar<DataDropExpr>(loc, out_expr));
      break;

    case TokenType::MemoryInit:
      ErrorUnlessOpcodeEnabled(Consume());
      CHECK_RESULT(ParsePlainInstrVar<MemoryInitExpr>(loc, out_expr));
      break;

    case TokenType::MemorySize:
      Consume();
      out_expr->reset(new MemorySizeExpr(loc));
      break;

    case TokenType::MemoryGrow:
      Consume();
      out_expr->reset(new MemoryGrowExpr(loc));
      break;

    case TokenType::TableCopy: {
      ErrorUnlessOpcodeEnabled(Consume());
      Var dst(0, loc);
      Var src(0, loc);
      if (options_->features.reference_types_enabled()) {
        ParseVarOpt(&dst, dst);
        ParseVarOpt(&src, src);
      }
      out_expr->reset(new TableCopyExpr(dst, src, loc));
      break;
    }

    case TokenType::ElemDrop:
      ErrorUnlessOpcodeEnabled(Consume());
      CHECK_RESULT(ParsePlainInstrVar<ElemDropExpr>(loc, out_expr));
      break;

    case TokenType::TableInit: {
      ErrorUnlessOpcodeEnabled(Consume());
      Var segment_index(0, loc);
      CHECK_RESULT(ParseVar(&segment_index));
      Var table_index(0, loc);
      if (ParseVarOpt(&table_index, table_index)) {
        // Here are the two forms:
        //
        //   table.init $elemidx ...
        //   table.init $tableidx $elemidx ...
        //
        // So if both indexes are provided, we need to swap them.
        std::swap(segment_index, table_index);
      }
      out_expr->reset(new TableInitExpr(segment_index, table_index, loc));
      break;
    }

    case TokenType::TableGet:
      ErrorUnlessOpcodeEnabled(Consume());
      CHECK_RESULT(ParsePlainInstrVar<TableGetExpr>(loc, out_expr));
      break;

    case TokenType::TableSet:
      ErrorUnlessOpcodeEnabled(Consume());
      // TODO: Table index.
      CHECK_RESULT(ParsePlainInstrVar<TableSetExpr>(loc, out_expr));
      break;

    case TokenType::TableGrow:
      ErrorUnlessOpcodeEnabled(Consume());
      // TODO: Table index.
      CHECK_RESULT(ParsePlainInstrVar<TableGrowExpr>(loc, out_expr));
      break;

    case TokenType::TableSize:
      ErrorUnlessOpcodeEnabled(Consume());
      // TODO: Table index.
      CHECK_RESULT(ParsePlainInstrVar<TableSizeExpr>(loc, out_expr));
      break;

    case TokenType::TableFill:
      ErrorUnlessOpcodeEnabled(Consume());
      // TODO: Table index.
      CHECK_RESULT(ParsePlainInstrVar<TableFillExpr>(loc, out_expr));
      break;

    case TokenType::RefFunc:
      ErrorUnlessOpcodeEnabled(Consume());
      CHECK_RESULT(ParsePlainInstrVar<RefFuncExpr>(loc, out_expr));
      break;

    case TokenType::RefNull: {
      ErrorUnlessOpcodeEnabled(Consume());
      Type type;
      CHECK_RESULT(ParseRefKind(&type));
      out_expr->reset(new RefNullExpr(type, loc));
      break;
    }

    case TokenType::RefIsNull:
      ErrorUnlessOpcodeEnabled(Consume());
      out_expr->reset(new RefIsNullExpr(loc));
      break;

    case TokenType::Throw:
      ErrorUnlessOpcodeEnabled(Consume());
      CHECK_RESULT(ParsePlainInstrVar<ThrowExpr>(loc, out_expr));
      break;

    case TokenType::Rethrow:
      ErrorUnlessOpcodeEnabled(Consume());
      CHECK_RESULT(ParsePlainInstrVar<RethrowExpr>(loc, out_expr));
      break;

    case TokenType::AtomicNotify: {
      Token token = Consume();
      ErrorUnlessOpcodeEnabled(token);
      CHECK_RESULT(
          ParsePlainLoadStoreInstr<AtomicNotifyExpr>(loc, token, out_expr));
      break;
    }

    case TokenType::AtomicFence: {
      Token token = Consume();
      ErrorUnlessOpcodeEnabled(token);
      uint32_t consistency_model = 0x0;
      out_expr->reset(new AtomicFenceExpr(consistency_model, loc));
      break;
    }

    case TokenType::AtomicWait: {
      Token token = Consume();
      ErrorUnlessOpcodeEnabled(token);
      CHECK_RESULT(
          ParsePlainLoadStoreInstr<AtomicWaitExpr>(loc, token, out_expr));
      break;
    }

    case TokenType::AtomicLoad: {
      Token token = Consume();
      ErrorUnlessOpcodeEnabled(token);
      CHECK_RESULT(
          ParsePlainLoadStoreInstr<AtomicLoadExpr>(loc, token, out_expr));
      break;
    }

    case TokenType::AtomicStore: {
      Token token = Consume();
      ErrorUnlessOpcodeEnabled(token);
      CHECK_RESULT(
          ParsePlainLoadStoreInstr<AtomicStoreExpr>(loc, token, out_expr));
      break;
    }

    case TokenType::AtomicRmw: {
      Token token = Consume();
      ErrorUnlessOpcodeEnabled(token);
      CHECK_RESULT(
          ParsePlainLoadStoreInstr<AtomicRmwExpr>(loc, token, out_expr));
      break;
    }

    case TokenType::AtomicRmwCmpxchg: {
      Token token = Consume();
      ErrorUnlessOpcodeEnabled(token);
      CHECK_RESULT(
          ParsePlainLoadStoreInstr<AtomicRmwCmpxchgExpr>(loc, token, out_expr));
      break;
    }

    case TokenType::Ternary: {
      Token token = Consume();
      ErrorUnlessOpcodeEnabled(token);
      out_expr->reset(new TernaryExpr(token.opcode(), loc));
      break;
    }

    case TokenType::SimdLaneOp: {
      Token token = Consume();
      ErrorUnlessOpcodeEnabled(token);

      uint64_t lane_idx = 0;
      Result result = ParseSimdLane(loc, &lane_idx);

      if (Failed(result)) {
        return Result::Error;
      }

      out_expr->reset(new SimdLaneOpExpr(token.opcode(), lane_idx, loc));
      break;
    }

    case TokenType::SimdLoadLane: {
      Token token = Consume();
      ErrorUnlessOpcodeEnabled(token);

      Address offset;
      Address align;
      ParseOffsetOpt(&offset);
      ParseAlignOpt(&align);

      uint64_t lane_idx = 0;
      Result result = ParseSimdLane(loc, &lane_idx);

      if (Failed(result)) {
        return Result::Error;
      }

      out_expr->reset(new SimdLoadLaneExpr(token.opcode(), align, offset, lane_idx, loc));
      break;
    }

    case TokenType::SimdStoreLane: {
      Token token = Consume();
      ErrorUnlessOpcodeEnabled(token);

      Address offset;
      Address align;
      ParseOffsetOpt(&offset);
      ParseAlignOpt(&align);

      uint64_t lane_idx = 0;
      Result result = ParseSimdLane(loc, &lane_idx);

      if (Failed(result)) {
        return Result::Error;
      }

      out_expr->reset(new SimdStoreLaneExpr(token.opcode(), align, offset, lane_idx, loc));
      break;
    }

    case TokenType::SimdShuffleOp: {
      Token token = Consume();
      ErrorUnlessOpcodeEnabled(token);
      v128 values;
      for (int lane = 0; lane < 16; ++lane) {
        Location loc = GetLocation();
        uint64_t lane_idx;
        Result result = ParseSimdLane(loc, &lane_idx);
        if (Failed(result)) {
          return Result::Error;
        }

        values.set_u8(lane, static_cast<uint8_t>(lane_idx));
      }

      out_expr->reset(
          new SimdShuffleOpExpr(token.opcode(), values, loc));
      break;
    }

    default:
      assert(
          !"ParsePlainInstr should only be called when IsPlainInstr() is true");
      return Result::Error;
  }

  return Result::Ok;
}

Result WastParser::ParseSimdV128Const(Const* const_,
                                      TokenType token_type,
                                      ConstType const_type) {
  WABT_TRACE(ParseSimdV128Const);

  uint8_t lane_count = 0;
  bool integer = true;
  switch (token_type) {
    case TokenType::I8X16: { lane_count = 16; break; }
    case TokenType::I16X8: { lane_count = 8; break; }
    case TokenType::I32X4: { lane_count = 4; break; }
    case TokenType::I64X2: { lane_count = 2; break; }
    case TokenType::F32X4: { lane_count = 4; integer = false; break; }
    case TokenType::F64X2: { lane_count = 2; integer = false; break; }
    default: {
      Error(
        const_->loc,
        "Unexpected type at start of simd constant. "
        "Expected one of: i8x16, i16x8, i32x4, i64x2, f32x4, f64x2. "
        "Found \"%s\".",
        GetTokenTypeName(token_type)
      );
      return Result::Error;
    }
  }
  Consume();

  const_->loc = GetLocation();

  for (int lane = 0; lane < lane_count; ++lane) {
    Location loc = GetLocation();

    // Check that the lane literal type matches the element type of the v128:
    Token token = GetToken();
    switch (token.token_type()) {
      case TokenType::Nat:
      case TokenType::Int:
        // OK.
        break;

      case TokenType::Float:
      case TokenType::NanArithmetic:
      case TokenType::NanCanonical:
        if (integer) {
          goto error;
        }
        break;

      error:
      default:
        if (integer) {
          return ErrorExpected({"a Nat or Integer literal"}, "123");
        } else {
          return ErrorExpected({"a Float literal"}, "42.0");
        }
    }

    Result result;

    // For each type, parse the next literal, bound check it, and write it to
    // the array of bytes:
    if (integer) {
      string_view sv = Consume().literal().text;
      const char* s = sv.begin();
      const char* end = sv.end();

      switch (lane_count) {
        case 16: {
          uint8_t value = 0;
          result = ParseInt8(s, end, &value, ParseIntType::SignedAndUnsigned);
          const_->set_v128_u8(lane, value);
          break;
        }
        case 8: {
          uint16_t value = 0;
          result = ParseInt16(s, end, &value, ParseIntType::SignedAndUnsigned);
          const_->set_v128_u16(lane, value);
          break;
        }
        case 4: {
          uint32_t value = 0;
          result = ParseInt32(s, end, &value, ParseIntType::SignedAndUnsigned);
          const_->set_v128_u32(lane, value);
          break;
        }
        case 2: {
          uint64_t value = 0;
          result = ParseInt64(s, end, &value, ParseIntType::SignedAndUnsigned);
          const_->set_v128_u64(lane, value);
          break;
        }
      }
    } else {
      Const lane_const_;
      switch (lane_count) {
        case 4:
          result = ParseF32(&lane_const_, const_type);
          const_->set_v128_f32(lane, lane_const_.f32_bits());
          break;

        case 2:
          result = ParseF64(&lane_const_, const_type);
          const_->set_v128_f64(lane, lane_const_.f64_bits());
          break;
      }

      const_->set_expected_nan(lane, lane_const_.expected_nan());
    }

    if (Failed(result)) {
      Error(loc, "invalid literal \"%s\"", token.to_string().c_str());
      return Result::Error;
    }
  }

  return Result::Ok;
}

Result WastParser::ParseExpectedNan(ExpectedNan* expected) {
  WABT_TRACE(ParseExpectedNan);
  TokenType token_type = Peek();
  switch (token_type) {
    case TokenType::NanArithmetic:
      *expected = ExpectedNan::Arithmetic;
      break;
    case TokenType::NanCanonical:
      *expected = ExpectedNan::Canonical;
      break;
    default:
      return Result::Error;
  }
  Consume();
  return Result::Ok;
}

Result WastParser::ParseF32(Const* const_, ConstType const_type) {
  ExpectedNan expected;
  if (const_type == ConstType::Expectation &&
      Succeeded(ParseExpectedNan(&expected))) {
    const_->set_f32(expected);
    return Result::Ok;
  }
  auto literal = Consume().literal();
  uint32_t f32_bits;
  Result result = ParseFloat(literal.type, literal.text.begin(),
                             literal.text.end(), &f32_bits);
  const_->set_f32(f32_bits);
  return result;
}

Result WastParser::ParseF64(Const* const_, ConstType const_type) {
  ExpectedNan expected;
  if (const_type == ConstType::Expectation &&
      Succeeded(ParseExpectedNan(&expected))) {
    const_->set_f64(expected);
    return Result::Ok;
  }
  auto literal = Consume().literal();
  uint64_t f64_bits;
  Result result = ParseDouble(literal.type, literal.text.begin(),
                              literal.text.end(), &f64_bits);
  const_->set_f64(f64_bits);
  return result;
}

Result WastParser::ParseConst(Const* const_, ConstType const_type) {
  WABT_TRACE(ParseConst);
  Token opcode_token = Consume();
  Opcode opcode = opcode_token.opcode();
  const_->loc = GetLocation();
  Token token = GetToken();

  // V128 is fully handled by ParseSimdV128Const:
  if (opcode != Opcode::V128Const) {
    switch (token.token_type()) {
      case TokenType::Nat:
      case TokenType::Int:
      case TokenType::Float:
        // OK.
        break;
      case TokenType::NanArithmetic:
      case TokenType::NanCanonical:
        break;
      default:
        return ErrorExpected({"a numeric literal"}, "123, -45, 6.7e8");
    }
  }

  Result result;
  switch (opcode) {
    case Opcode::I32Const: {
      auto sv = Consume().literal().text;
      uint32_t u32;
      result = ParseInt32(sv.begin(), sv.end(), &u32,
                          ParseIntType::SignedAndUnsigned);
      const_->set_u32(u32);
      break;
    }

    case Opcode::I64Const: {
      auto sv = Consume().literal().text;
      uint64_t u64;
      result = ParseInt64(sv.begin(), sv.end(), &u64,
                          ParseIntType::SignedAndUnsigned);
      const_->set_u64(u64);
      break;
    }

    case Opcode::F32Const:
      result = ParseF32(const_, const_type);
      break;

    case Opcode::F64Const:
      result = ParseF64(const_, const_type);
      break;

    case Opcode::V128Const:
      ErrorUnlessOpcodeEnabled(opcode_token);
      // Parse V128 Simd Const (16 bytes).
      result = ParseSimdV128Const(const_, token.token_type(), const_type);
      // ParseSimdV128Const report error already, just return here if parser get
      // errors.
      if (Failed(result)) {
        return Result::Error;
      }
      break;

    default:
      assert(!"ParseConst called with invalid opcode");
      return Result::Error;
  }

  if (Failed(result)) {
    Error(const_->loc, "invalid literal \"%s\"", token.to_string().c_str());
    // Return if parser get errors.
    return Result::Error;
  }

  return Result::Ok;
}

Result WastParser::ParseExternref(Const* const_) {
  WABT_TRACE(ParseExternref);
  Token token = Consume();
  if (!options_->features.reference_types_enabled()) {
    Error(token.loc, "externref not allowed");
    return Result::Error;
  }

  Literal literal;
  string_view sv;
  const char* s;
  const char* end;
  const_->loc = GetLocation();
  TokenType token_type = Peek();

  switch (token_type) {
    case TokenType::Nat:
    case TokenType::Int: {
      literal = Consume().literal();
      sv = literal.text;
      s = sv.begin();
      end = sv.end();
      break;
    }
    default:
      return ErrorExpected({"a numeric literal"}, "123");
  }

  uint64_t ref_bits;
  Result result = ParseInt64(s, end, &ref_bits, ParseIntType::UnsignedOnly);

  const_->set_externref(static_cast<uintptr_t>(ref_bits));

  if (Failed(result)) {
    Error(const_->loc, "invalid literal \"" PRIstringview "\"",
          WABT_PRINTF_STRING_VIEW_ARG(literal.text));
    // Return if parser get errors.
    return Result::Error;
  }

  return Result::Ok;
}

Result WastParser::ParseConstList(ConstVector* consts, ConstType type) {
  WABT_TRACE(ParseConstList);
  while (PeekMatchLpar(TokenType::Const) || PeekMatchLpar(TokenType::RefNull) ||
         PeekMatchLpar(TokenType::RefExtern) ||
         PeekMatchLpar(TokenType::RefFunc)) {
    Consume();
    Const const_;
    switch (Peek()) {
      case TokenType::Const:
        CHECK_RESULT(ParseConst(&const_, type));
        break;
      case TokenType::RefNull: {
        auto token = Consume();
        Type type;
        CHECK_RESULT(ParseRefKind(&type));
        ErrorUnlessOpcodeEnabled(token);
        const_.loc = GetLocation();
        const_.set_null(type);
        break;
      }
      case TokenType::RefFunc: {
        auto token = Consume();
        ErrorUnlessOpcodeEnabled(token);
        const_.loc = GetLocation();
        const_.set_funcref();
        break;
      }
      case TokenType::RefExtern:
        CHECK_RESULT(ParseExternref(&const_));
        break;
      default:
        assert(!"unreachable");
        return Result::Error;
    }
    EXPECT(Rpar);
    consts->push_back(const_);
  }

  return Result::Ok;
}

Result WastParser::ParseBlockInstr(std::unique_ptr<Expr>* out_expr) {
  WABT_TRACE(ParseBlockInstr);
  Location loc = GetLocation();

  switch (Peek()) {
    case TokenType::Block: {
      Consume();
      auto expr = MakeUnique<BlockExpr>(loc);
      CHECK_RESULT(ParseLabelOpt(&expr->block.label));
      CHECK_RESULT(ParseBlock(&expr->block));
      EXPECT(End);
      CHECK_RESULT(ParseEndLabelOpt(expr->block.label));
      *out_expr = std::move(expr);
      break;
    }

    case TokenType::Loop: {
      Consume();
      auto expr = MakeUnique<LoopExpr>(loc);
      CHECK_RESULT(ParseLabelOpt(&expr->block.label));
      CHECK_RESULT(ParseBlock(&expr->block));
      EXPECT(End);
      CHECK_RESULT(ParseEndLabelOpt(expr->block.label));
      *out_expr = std::move(expr);
      break;
    }

    case TokenType::If: {
      Consume();
      auto expr = MakeUnique<IfExpr>(loc);
      CHECK_RESULT(ParseLabelOpt(&expr->true_.label));
      CHECK_RESULT(ParseBlock(&expr->true_));
      if (Match(TokenType::Else)) {
        CHECK_RESULT(ParseEndLabelOpt(expr->true_.label));
        CHECK_RESULT(ParseTerminatingInstrList(&expr->false_));
        expr->false_end_loc = GetLocation();
      }
      EXPECT(End);
      CHECK_RESULT(ParseEndLabelOpt(expr->true_.label));
      *out_expr = std::move(expr);
      break;
    }

    case TokenType::Try: {
      ErrorUnlessOpcodeEnabled(Consume());
      auto expr = MakeUnique<TryExpr>(loc);
      CatchVector catches;
      CHECK_RESULT(ParseLabelOpt(&expr->block.label));
      CHECK_RESULT(ParseBlock(&expr->block));
      if (IsCatch(Peek())) {
        CHECK_RESULT(ParseCatchInstrList(&expr->catches));
        expr->kind = TryKind::Catch;
      } else if (PeekMatch(TokenType::Unwind)) {
        Consume();
        CHECK_RESULT(ParseInstrList(&expr->unwind));
        expr->kind = TryKind::Unwind;
      } else if (PeekMatch(TokenType::Delegate)) {
        Consume();
        Var var;
        CHECK_RESULT(ParseVar(&var));
        expr->delegate_target = var;
        expr->kind = TryKind::Delegate;
      } else {
        return ErrorExpected({"catch", "catch_all", "unwind", "delegate"});
      }
      CHECK_RESULT(ErrorIfLpar({"a valid try clause"}));
      expr->block.end_loc = GetLocation();
      if (expr->kind != TryKind::Delegate) {
        EXPECT(End);
      }
      CHECK_RESULT(ParseEndLabelOpt(expr->block.label));
      *out_expr = std::move(expr);
      break;
    }

    default:
      assert(
          !"ParseBlockInstr should only be called when IsBlockInstr() is true");
      return Result::Error;
  }

  return Result::Ok;
}

Result WastParser::ParseLabelOpt(std::string* out_label) {
  WABT_TRACE(ParseLabelOpt);
  if (PeekMatch(TokenType::Var)) {
    *out_label = Consume().text().to_string();
  } else {
    out_label->clear();
  }
  return Result::Ok;
}

Result WastParser::ParseEndLabelOpt(const std::string& begin_label) {
  WABT_TRACE(ParseEndLabelOpt);
  Location loc = GetLocation();
  std::string end_label;
  CHECK_RESULT(ParseLabelOpt(&end_label));
  if (!end_label.empty()) {
    if (begin_label.empty()) {
      Error(loc, "unexpected label \"%s\"", end_label.c_str());
    } else if (begin_label != end_label) {
      Error(loc, "mismatching label \"%s\" != \"%s\"", begin_label.c_str(),
            end_label.c_str());
    }
  }
  return Result::Ok;
}

Result WastParser::ParseBlockDeclaration(BlockDeclaration* decl) {
  WABT_TRACE(ParseBlockDeclaration);
  FuncDeclaration func_decl;
  CHECK_RESULT(ParseTypeUseOpt(&func_decl));
  CHECK_RESULT(ParseUnboundFuncSignature(&func_decl.sig));
  decl->has_func_type = func_decl.has_func_type;
  decl->type_var = func_decl.type_var;
  decl->sig = func_decl.sig;
  return Result::Ok;
}

Result WastParser::ParseBlock(Block* block) {
  WABT_TRACE(ParseBlock);
  CHECK_RESULT(ParseBlockDeclaration(&block->decl));
  CHECK_RESULT(ParseInstrList(&block->exprs));
  block->end_loc = GetLocation();
  return Result::Ok;
}

Result WastParser::ParseExprList(ExprList* exprs) {
  WABT_TRACE(ParseExprList);
  ExprList new_exprs;
  while (PeekMatchExpr()) {
    if (Succeeded(ParseExpr(&new_exprs))) {
      exprs->splice(exprs->end(), new_exprs);
    } else {
      CHECK_RESULT(Synchronize(IsExpr));
    }
  }
  return Result::Ok;
}

Result WastParser::ParseExpr(ExprList* exprs) {
  WABT_TRACE(ParseExpr);
  if (!PeekMatch(TokenType::Lpar)) {
    return Result::Error;
  }

  if (IsPlainInstr(Peek(1))) {
    Consume();
    std::unique_ptr<Expr> expr;
    CHECK_RESULT(ParsePlainInstr(&expr));
    CHECK_RESULT(ParseExprList(exprs));
    CHECK_RESULT(ErrorIfLpar({"an expr"}));
    exprs->push_back(std::move(expr));
  } else {
    Location loc = GetLocation();

    switch (Peek(1)) {
      case TokenType::Block: {
        Consume();
        Consume();
        auto expr = MakeUnique<BlockExpr>(loc);
        CHECK_RESULT(ParseLabelOpt(&expr->block.label));
        CHECK_RESULT(ParseBlock(&expr->block));
        exprs->push_back(std::move(expr));
        break;
      }

      case TokenType::Loop: {
        Consume();
        Consume();
        auto expr = MakeUnique<LoopExpr>(loc);
        CHECK_RESULT(ParseLabelOpt(&expr->block.label));
        CHECK_RESULT(ParseBlock(&expr->block));
        exprs->push_back(std::move(expr));
        break;
      }

      case TokenType::If: {
        Consume();
        Consume();
        auto expr = MakeUnique<IfExpr>(loc);

        CHECK_RESULT(ParseLabelOpt(&expr->true_.label));
        CHECK_RESULT(ParseBlockDeclaration(&expr->true_.decl));

        if (PeekMatchExpr()) {
          ExprList cond;
          CHECK_RESULT(ParseExpr(&cond));
          exprs->splice(exprs->end(), cond);
        }

        if (MatchLpar(TokenType::Then)) {
          CHECK_RESULT(ParseTerminatingInstrList(&expr->true_.exprs));
          expr->true_.end_loc = GetLocation();
          EXPECT(Rpar);

          if (MatchLpar(TokenType::Else)) {
            CHECK_RESULT(ParseTerminatingInstrList(&expr->false_));
            EXPECT(Rpar);
          } else if (PeekMatchExpr()) {
            CHECK_RESULT(ParseExpr(&expr->false_));
          }
          expr->false_end_loc = GetLocation();
        } else if (PeekMatchExpr()) {
          CHECK_RESULT(ParseExpr(&expr->true_.exprs));
          expr->true_.end_loc = GetLocation();
          if (PeekMatchExpr()) {
            CHECK_RESULT(ParseExpr(&expr->false_));
            expr->false_end_loc = GetLocation();
          }
        } else {
          ConsumeIfLpar();
          return ErrorExpected({"then block"}, "(then ...)");
        }

        exprs->push_back(std::move(expr));
        break;
      }

      case TokenType::Try: {
        Consume();
        ErrorUnlessOpcodeEnabled(Consume());

        auto expr = MakeUnique<TryExpr>(loc);
        CHECK_RESULT(ParseLabelOpt(&expr->block.label));
        CHECK_RESULT(ParseBlockDeclaration(&expr->block.decl));
        EXPECT(Lpar);
        EXPECT(Do);
        CHECK_RESULT(ParseInstrList(&expr->block.exprs));
        EXPECT(Rpar);
        EXPECT(Lpar);
        TokenType type = Peek();
        switch (type) {
          case TokenType::Catch:
          case TokenType::CatchAll:
            CHECK_RESULT(ParseCatchExprList(&expr->catches));
            expr->kind = TryKind::Catch;
            break;
          case TokenType::Unwind:
            Consume();
            CHECK_RESULT(ParseTerminatingInstrList(&expr->unwind));
            expr->kind = TryKind::Unwind;
            EXPECT(Rpar);
            break;
          case TokenType::Delegate: {
            Consume();
            Var var;
            CHECK_RESULT(ParseVar(&var));
            expr->delegate_target = var;
            expr->kind = TryKind::Delegate;
            EXPECT(Rpar);
            break;
          }
          default:
            ErrorExpected({"catch", "catch_all", "unwind", "delegate"});
            break;
        }
        CHECK_RESULT(ErrorIfLpar({"a valid try clause"}));
        expr->block.end_loc = GetLocation();
        exprs->push_back(std::move(expr));
        break;
      }

      default:
        assert(!"ParseExpr should only be called when IsExpr() is true");
        return Result::Error;
    }
  }

  EXPECT(Rpar);
  return Result::Ok;
}

Result WastParser::ParseCatchInstrList(CatchVector* catches) {
  WABT_TRACE(ParseCatchInstrList);
  bool parsedCatch = false;
  bool parsedCatchAll = false;

  while (IsCatch(Peek())) {
    Catch catch_(GetLocation());

    auto token = Consume();
    if (token.token_type() == TokenType::Catch) {
      CHECK_RESULT(ParseVar(&catch_.var));
    } else {
      if (parsedCatchAll) {
        Error(token.loc, "multiple catch_all clauses not allowed");
        return Result::Error;
      }
      parsedCatchAll = true;
    }

    CHECK_RESULT(ParseInstrList(&catch_.exprs));
    catches->push_back(std::move(catch_));
    parsedCatch = true;
  }

  if (!parsedCatch) {
    return ErrorExpected({"catch"});
  }

  return Result::Ok;
}

Result WastParser::ParseCatchExprList(CatchVector* catches) {
  WABT_TRACE(ParseCatchExprList);
  bool parsedCatchAll = false;

  do {
    Catch catch_(GetLocation());

    auto token = Consume();
    if (token.token_type() == TokenType::Catch) {
      CHECK_RESULT(ParseVar(&catch_.var));
    } else {
      if (parsedCatchAll) {
        Error(token.loc, "multiple catch_all clauses not allowed");
        return Result::Error;
      }
      parsedCatchAll = true;
    }

    CHECK_RESULT(ParseTerminatingInstrList(&catch_.exprs));
    EXPECT(Rpar);
    catches->push_back(std::move(catch_));
  } while (Match(TokenType::Lpar) && IsCatch(Peek()));

  return Result::Ok;
}

Result WastParser::ParseGlobalType(Global* global) {
  WABT_TRACE(ParseGlobalType);
  if (MatchLpar(TokenType::Mut)) {
    global->mutable_ = true;
    CHECK_RESULT(ParseValueType(&global->type));
    CHECK_RESULT(ErrorIfLpar({"i32", "i64", "f32", "f64"}));
    EXPECT(Rpar);
  } else {
    CHECK_RESULT(ParseValueType(&global->type));
  }

  return Result::Ok;
}

Result WastParser::ParseCommandList(Script* script,
                                    CommandPtrVector* commands) {
  WABT_TRACE(ParseCommandList);
  while (IsCommand(PeekPair())) {
    CommandPtr command;
    if (Succeeded(ParseCommand(script, &command))) {
      commands->push_back(std::move(command));
    } else {
      CHECK_RESULT(Synchronize(IsCommand));
    }
  }
  return Result::Ok;
}

Result WastParser::ParseCommand(Script* script, CommandPtr* out_command) {
  WABT_TRACE(ParseCommand);
  switch (Peek(1)) {
    case TokenType::AssertExhaustion:
      return ParseAssertExhaustionCommand(out_command);

    case TokenType::AssertInvalid:
      return ParseAssertInvalidCommand(out_command);

    case TokenType::AssertMalformed:
      return ParseAssertMalformedCommand(out_command);

    case TokenType::AssertReturn:
      return ParseAssertReturnCommand(out_command);

    case TokenType::AssertTrap:
      return ParseAssertTrapCommand(out_command);

    case TokenType::AssertUnlinkable:
      return ParseAssertUnlinkableCommand(out_command);

    case TokenType::Get:
    case TokenType::Invoke:
      return ParseActionCommand(out_command);

    case TokenType::Module:
      return ParseModuleCommand(script, out_command);

    case TokenType::Register:
      return ParseRegisterCommand(out_command);

    case TokenType::Input:
      return ParseInputCommand(out_command);

    case TokenType::Output:
      return ParseOutputCommand(out_command);

    default:
      assert(!"ParseCommand should only be called when IsCommand() is true");
      return Result::Error;
  }
}

Result WastParser::ParseAssertExhaustionCommand(CommandPtr* out_command) {
  WABT_TRACE(ParseAssertExhaustionCommand);
  return ParseAssertActionTextCommand<AssertExhaustionCommand>(
      TokenType::AssertExhaustion, out_command);
}

Result WastParser::ParseAssertInvalidCommand(CommandPtr* out_command) {
  WABT_TRACE(ParseAssertInvalidCommand);
  return ParseAssertScriptModuleCommand<AssertInvalidCommand>(
      TokenType::AssertInvalid, out_command);
}

Result WastParser::ParseAssertMalformedCommand(CommandPtr* out_command) {
  WABT_TRACE(ParseAssertMalformedCommand);
  return ParseAssertScriptModuleCommand<AssertMalformedCommand>(
      TokenType::AssertMalformed, out_command);
}

Result WastParser::ParseAssertReturnCommand(CommandPtr* out_command) {
  WABT_TRACE(ParseAssertReturnCommand);
  EXPECT(Lpar);
  EXPECT(AssertReturn);
  auto command = MakeUnique<AssertReturnCommand>();
  CHECK_RESULT(ParseAction(&command->action));
  CHECK_RESULT(ParseConstList(&command->expected, ConstType::Expectation));
  EXPECT(Rpar);
  *out_command = std::move(command);
  return Result::Ok;
}

Result WastParser::ParseAssertTrapCommand(CommandPtr* out_command) {
  WABT_TRACE(ParseAssertTrapCommand);
  EXPECT(Lpar);
  EXPECT(AssertTrap);
  if (PeekMatchLpar(TokenType::Module)) {
    auto command = MakeUnique<AssertUninstantiableCommand>();
    CHECK_RESULT(ParseScriptModule(&command->module));
    CHECK_RESULT(ParseQuotedText(&command->text));
    *out_command = std::move(command);
  } else {
    auto command = MakeUnique<AssertTrapCommand>();
    CHECK_RESULT(ParseAction(&command->action));
    CHECK_RESULT(ParseQuotedText(&command->text));
    *out_command = std::move(command);
  }
  EXPECT(Rpar);
  return Result::Ok;
}

Result WastParser::ParseAssertUnlinkableCommand(CommandPtr* out_command) {
  WABT_TRACE(ParseAssertUnlinkableCommand);
  return ParseAssertScriptModuleCommand<AssertUnlinkableCommand>(
      TokenType::AssertUnlinkable, out_command);
}

Result WastParser::ParseActionCommand(CommandPtr* out_command) {
  WABT_TRACE(ParseActionCommand);
  auto command = MakeUnique<ActionCommand>();
  CHECK_RESULT(ParseAction(&command->action));
  *out_command = std::move(command);
  return Result::Ok;
}

Result WastParser::ParseModuleCommand(Script* script, CommandPtr* out_command) {
  WABT_TRACE(ParseModuleCommand);
  std::unique_ptr<ScriptModule> script_module;
  CHECK_RESULT(ParseScriptModule(&script_module));

  auto command = MakeUnique<ModuleCommand>();
  Module& module = command->module;

  switch (script_module->type()) {
    case ScriptModuleType::Text:
      module = std::move(cast<TextScriptModule>(script_module.get())->module);
      break;

    case ScriptModuleType::Binary: {
      auto* bsm = cast<BinaryScriptModule>(script_module.get());
      ReadBinaryOptions options;
#if WABT_TRACING
      auto log_stream = FileStream::CreateStdout();
      options.log_stream = log_stream.get();
#endif
      options.features = options_->features;
      Errors errors;
      const char* filename = "<text>";
      ReadBinaryIr(filename, bsm->data.data(), bsm->data.size(), options,
                   &errors, &module);
      module.name = bsm->name;
      module.loc = bsm->loc;
      for (const auto& error: errors) {
        assert(error.error_level == ErrorLevel::Error);
        if (error.loc.offset == kInvalidOffset) {
          Error(bsm->loc, "error in binary module: %s", error.message.c_str());
        } else {
          Error(bsm->loc, "error in binary module: @0x%08" PRIzx ": %s",
                error.loc.offset, error.message.c_str());
        }
      }
      break;
    }

    case ScriptModuleType::Quoted:
      return ErrorExpected({"a binary module", "a text module"});
  }

  // script is nullptr when ParseModuleCommand is called from ParseModule.
  if (script) {
    Index command_index = script->commands.size();

    if (!module.name.empty()) {
      script->module_bindings.emplace(module.name,
                                      Binding(module.loc, command_index));
    }

    last_module_index_ = command_index;
  }

  *out_command = std::move(command);
  return Result::Ok;
}

Result WastParser::ParseRegisterCommand(CommandPtr* out_command) {
  WABT_TRACE(ParseRegisterCommand);
  EXPECT(Lpar);
  Location loc = GetLocation();
  EXPECT(Register);
  std::string text;
  Var var;
  CHECK_RESULT(ParseQuotedText(&text));
  ParseVarOpt(&var, Var(last_module_index_, loc));
  EXPECT(Rpar);
  out_command->reset(new RegisterCommand(text, var));
  return Result::Ok;
}

Result WastParser::ParseInputCommand(CommandPtr*) {
  // Parse the input command, but always fail since this command is not
  // actually supported.
  WABT_TRACE(ParseInputCommand);
  EXPECT(Lpar);
  Location loc = GetLocation();
  EXPECT(Input);
  Error(loc, "input command is not supported");
  Var var;
  std::string text;
  ParseVarOpt(&var);
  CHECK_RESULT(ParseQuotedText(&text));
  EXPECT(Rpar);
  return Result::Error;
}

Result WastParser::ParseOutputCommand(CommandPtr*) {
  // Parse the output command, but always fail since this command is not
  // actually supported.
  WABT_TRACE(ParseOutputCommand);
  EXPECT(Lpar);
  Location loc = GetLocation();
  EXPECT(Output);
  Error(loc, "output command is not supported");
  Var var;
  std::string text;
  ParseVarOpt(&var);
  if (Peek() == TokenType::Text) {
    CHECK_RESULT(ParseQuotedText(&text));
  }
  EXPECT(Rpar);
  return Result::Error;
}

Result WastParser::ParseAction(ActionPtr* out_action) {
  WABT_TRACE(ParseAction);
  EXPECT(Lpar);
  Location loc = GetLocation();

  switch (Peek()) {
    case TokenType::Invoke: {
      Consume();
      auto action = MakeUnique<InvokeAction>(loc);
      ParseVarOpt(&action->module_var, Var(last_module_index_, loc));
      CHECK_RESULT(ParseQuotedText(&action->name));
      CHECK_RESULT(ParseConstList(&action->args, ConstType::Normal));
      *out_action = std::move(action);
      break;
    }

    case TokenType::Get: {
      Consume();
      auto action = MakeUnique<GetAction>(loc);
      ParseVarOpt(&action->module_var, Var(last_module_index_, loc));
      CHECK_RESULT(ParseQuotedText(&action->name));
      *out_action = std::move(action);
      break;
    }

    default:
      return ErrorExpected({"invoke", "get"});
  }
  EXPECT(Rpar);
  return Result::Ok;
}

Result WastParser::ParseScriptModule(
    std::unique_ptr<ScriptModule>* out_module) {
  WABT_TRACE(ParseScriptModule);
  EXPECT(Lpar);
  Location loc = GetLocation();
  EXPECT(Module);
  std::string name;
  ParseBindVarOpt(&name);

  switch (Peek()) {
    case TokenType::Bin: {
      Consume();
      std::vector<uint8_t> data;
      // TODO(binji): The spec allows this to be empty, switch to
      // ParseTextListOpt.
      CHECK_RESULT(ParseTextList(&data));

      auto bsm = MakeUnique<BinaryScriptModule>();
      bsm->name = name;
      bsm->loc = loc;
      bsm->data = std::move(data);
      *out_module = std::move(bsm);
      break;
    }

    case TokenType::Quote: {
      Consume();
      std::vector<uint8_t> data;
      // TODO(binji): The spec allows this to be empty, switch to
      // ParseTextListOpt.
      CHECK_RESULT(ParseTextList(&data));

      auto qsm = MakeUnique<QuotedScriptModule>();
      qsm->name = name;
      qsm->loc = loc;
      qsm->data = std::move(data);
      *out_module = std::move(qsm);
      break;
    }

    default: {
      auto tsm = MakeUnique<TextScriptModule>();
      tsm->module.name = name;
      tsm->module.loc = loc;
      if (IsModuleField(PeekPair())) {
        CHECK_RESULT(ParseModuleFieldList(&tsm->module));
      } else if (!PeekMatch(TokenType::Rpar)) {
        ConsumeIfLpar();
        return ErrorExpected({"a module field"});
      }
      *out_module = std::move(tsm);
      break;
    }
  }

  EXPECT(Rpar);
  return Result::Ok;
}

template <typename T>
Result WastParser::ParseAssertActionCommand(TokenType token_type,
                                            CommandPtr* out_command) {
  WABT_TRACE(ParseAssertActionCommand);
  EXPECT(Lpar);
  CHECK_RESULT(Expect(token_type));
  auto command = MakeUnique<T>();
  CHECK_RESULT(ParseAction(&command->action));
  EXPECT(Rpar);
  *out_command = std::move(command);
  return Result::Ok;
}

template <typename T>
Result WastParser::ParseAssertActionTextCommand(TokenType token_type,
                                                CommandPtr* out_command) {
  WABT_TRACE(ParseAssertActionTextCommand);
  EXPECT(Lpar);
  CHECK_RESULT(Expect(token_type));
  auto command = MakeUnique<T>();
  CHECK_RESULT(ParseAction(&command->action));
  CHECK_RESULT(ParseQuotedText(&command->text));
  EXPECT(Rpar);
  *out_command = std::move(command);
  return Result::Ok;
}

template <typename T>
Result WastParser::ParseAssertScriptModuleCommand(TokenType token_type,
                                                  CommandPtr* out_command) {
  WABT_TRACE(ParseAssertScriptModuleCommand);
  EXPECT(Lpar);
  CHECK_RESULT(Expect(token_type));
  auto command = MakeUnique<T>();
  CHECK_RESULT(ParseScriptModule(&command->module));
  CHECK_RESULT(ParseQuotedText(&command->text));
  EXPECT(Rpar);
  *out_command = std::move(command);
  return Result::Ok;
}

void WastParser::CheckImportOrdering(Module* module) {
  if (module->funcs.size() != module->num_func_imports ||
      module->tables.size() != module->num_table_imports ||
      module->memories.size() != module->num_memory_imports ||
      module->globals.size() != module->num_global_imports ||
      module->events.size() != module->num_event_imports) {
    Error(GetLocation(),
          "imports must occur before all non-import definitions");
  }
}

Result ParseWatModule(WastLexer* lexer,
                      std::unique_ptr<Module>* out_module,
                      Errors* errors,
                      WastParseOptions* options) {
  assert(out_module != nullptr);
  assert(options != nullptr);
  WastParser parser(lexer, errors, options);
  CHECK_RESULT(parser.ParseModule(out_module));
  return Result::Ok;
}

Result ParseWastScript(WastLexer* lexer,
                       std::unique_ptr<Script>* out_script,
                       Errors* errors,
                       WastParseOptions* options) {
  assert(out_script != nullptr);
  assert(options != nullptr);
  WastParser parser(lexer, errors, options);
  CHECK_RESULT(parser.ParseScript(out_script));
  CHECK_RESULT(ResolveNamesScript(out_script->get(), errors));
  return Result::Ok;
}

}  // namespace wabt
