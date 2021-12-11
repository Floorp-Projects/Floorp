/*
 * Copyright 2016 WebAssembly Community Group participants
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

#ifndef WABT_IR_H_
#define WABT_IR_H_

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#include "src/binding-hash.h"
#include "src/common.h"
#include "src/intrusive-list.h"
#include "src/opcode.h"
#include "src/string-view.h"

namespace wabt {

struct Module;

enum class VarType {
  Index,
  Name,
};

struct Var {
  explicit Var(Index index = kInvalidIndex, const Location& loc = Location());
  explicit Var(string_view name, const Location& loc = Location());
  Var(Var&&);
  Var(const Var&);
  Var& operator=(const Var&);
  Var& operator=(Var&&);
  ~Var();

  VarType type() const { return type_; }
  bool is_index() const { return type_ == VarType::Index; }
  bool is_name() const { return type_ == VarType::Name; }

  Index index() const { assert(is_index()); return index_; }
  const std::string& name() const { assert(is_name()); return name_; }

  void set_index(Index);
  void set_name(std::string&&);
  void set_name(string_view);

  Location loc;

 private:
  void Destroy();

  VarType type_;
  union {
    Index index_;
    std::string name_;
  };
};
typedef std::vector<Var> VarVector;

struct Const {
  static constexpr uintptr_t kRefNullBits = ~uintptr_t(0);

  Const() : Const(Type::I32, uint32_t(0)) {}

  static Const I32(uint32_t val = 0, const Location& loc = Location()) {
    return Const(Type::I32, val, loc);
  }
  static Const I64(uint64_t val = 0, const Location& loc = Location()) {
    return Const(Type::I64, val, loc);
  }
  static Const F32(uint32_t val = 0, const Location& loc = Location()) {
    return Const(Type::F32, val, loc);
  }
  static Const F64(uint64_t val = 0, const Location& loc = Location()) {
    return Const(Type::F64, val, loc);
  }
  static Const V128(v128 val, const Location& loc = Location()) {
    return Const(Type::V128, val, loc);
  }

  Type type() const { return type_; }
  Type lane_type() const { assert(type_ == Type::V128); return lane_type_; }

  int lane_count() const {
    switch (lane_type()) {
      case Type::I8:  return 16;
      case Type::I16: return 8;
      case Type::I32: return 4;
      case Type::I64: return 2;
      case Type::F32: return 4;
      case Type::F64: return 2;
      default: WABT_UNREACHABLE;
    }
  }

  uint32_t u32() const { return data_.u32(0); }
  uint64_t u64() const { return data_.u64(0); }
  uint32_t f32_bits() const { return data_.f32_bits(0); }
  uint64_t f64_bits() const { return data_.f64_bits(0); }
  uintptr_t ref_bits() const { return data_.To<uintptr_t>(0); }
  v128 vec128() const { return data_; }

  template <typename T>
  T v128_lane(int lane) const { return data_.To<T>(lane); }

  void set_u32(uint32_t x) { From(Type::I32, x); }
  void set_u64(uint64_t x) { From(Type::I64, x); }
  void set_f32(uint32_t x) { From(Type::F32, x); }
  void set_f64(uint64_t x) { From(Type::F64, x); }

  void set_v128_u8(int lane, uint8_t x) { set_v128_lane(lane, Type::I8, x); }
  void set_v128_u16(int lane, uint16_t x) { set_v128_lane(lane, Type::I16, x); }
  void set_v128_u32(int lane, uint32_t x) { set_v128_lane(lane, Type::I32, x); }
  void set_v128_u64(int lane, uint64_t x) { set_v128_lane(lane, Type::I64, x); }
  void set_v128_f32(int lane, uint32_t x) { set_v128_lane(lane, Type::F32, x); }
  void set_v128_f64(int lane, uint64_t x) { set_v128_lane(lane, Type::F64, x); }

  // Only used for expectations. (e.g. wast assertions)
  void set_f32(ExpectedNan nan) { set_f32(0); set_expected_nan(0, nan); }
  void set_f64(ExpectedNan nan) { set_f64(0); set_expected_nan(0, nan); }
  void set_funcref()            { From<uintptr_t>(Type::FuncRef, 0); }
  void set_externref(uintptr_t x) { From(Type::ExternRef, x); }
  void set_null(Type type)      { From<uintptr_t>(type, kRefNullBits); }

  bool is_expected_nan(int lane = 0) const {
    return expected_nan(lane) != ExpectedNan::None;
  }

  ExpectedNan expected_nan(int lane = 0) const {
    return lane < 4 ? nan_[lane] : ExpectedNan::None;
  }

  void set_expected_nan(int lane, ExpectedNan nan) {
    if (lane < 4) {
      nan_[lane] = nan;
    }
  }

  // v128 support
  Location loc;

 private:
  template <typename T>
  void set_v128_lane(int lane, Type lane_type, T x) {
    lane_type_ = lane_type;
    From(Type::V128, x, lane);
    set_expected_nan(lane, ExpectedNan::None);
  }

  template <typename T>
  Const(Type type, T data, const Location& loc = Location()) : loc(loc) {
    From<T>(type, data);
  }

  template <typename T>
  void From(Type type, T data, int lane = 0) {
    static_assert(sizeof(T) <= sizeof(data_), "Invalid cast!");
    assert((lane + 1) * sizeof(T) <= sizeof(data_));
    type_ = type;
    data_.From<T>(lane, data);
    set_expected_nan(lane, ExpectedNan::None);
  }

  Type type_;
  Type lane_type_;    // Only valid if type_ == Type::V128.
  v128 data_;
  ExpectedNan nan_[4];
};
typedef std::vector<Const> ConstVector;

struct FuncSignature {
  TypeVector param_types;
  TypeVector result_types;

  Index GetNumParams() const { return param_types.size(); }
  Index GetNumResults() const { return result_types.size(); }
  Type GetParamType(Index index) const { return param_types[index]; }
  Type GetResultType(Index index) const { return result_types[index]; }

  bool operator==(const FuncSignature&) const;
};

enum class TypeEntryKind {
  Func,
  Struct,
  Array,
};

class TypeEntry {
 public:
  WABT_DISALLOW_COPY_AND_ASSIGN(TypeEntry);

  virtual ~TypeEntry() = default;

  TypeEntryKind kind() const { return kind_; }

  Location loc;
  std::string name;

 protected:
  explicit TypeEntry(TypeEntryKind kind,
                     string_view name = string_view(),
                     const Location& loc = Location())
      : loc(loc), name(name.to_string()), kind_(kind) {}

  TypeEntryKind kind_;
};

class FuncType : public TypeEntry {
 public:
  static bool classof(const TypeEntry* entry) {
    return entry->kind() == TypeEntryKind::Func;
  }

  explicit FuncType(string_view name = string_view())
      : TypeEntry(TypeEntryKind::Func, name) {}

  Index GetNumParams() const { return sig.GetNumParams(); }
  Index GetNumResults() const { return sig.GetNumResults(); }
  Type GetParamType(Index index) const { return sig.GetParamType(index); }
  Type GetResultType(Index index) const { return sig.GetResultType(index); }

  FuncSignature sig;
};

struct Field {
  std::string name;
  Type type = Type::Void;
  bool mutable_ = false;
};

class StructType : public TypeEntry {
 public:
  static bool classof(const TypeEntry* entry) {
    return entry->kind() == TypeEntryKind::Struct;
  }

  explicit StructType(string_view name = string_view())
      : TypeEntry(TypeEntryKind::Struct) {}

  std::vector<Field> fields;
};

class ArrayType : public TypeEntry {
 public:
  static bool classof(const TypeEntry* entry) {
    return entry->kind() == TypeEntryKind::Array;
  }

  explicit ArrayType(string_view name = string_view())
      : TypeEntry(TypeEntryKind::Array) {}

  Field field;
};

struct FuncDeclaration {
  Index GetNumParams() const { return sig.GetNumParams(); }
  Index GetNumResults() const { return sig.GetNumResults(); }
  Type GetParamType(Index index) const { return sig.GetParamType(index); }
  Type GetResultType(Index index) const { return sig.GetResultType(index); }

  bool has_func_type = false;
  Var type_var;
  FuncSignature sig;
};

enum class ExprType {
  AtomicLoad,
  AtomicRmw,
  AtomicRmwCmpxchg,
  AtomicStore,
  AtomicNotify,
  AtomicFence,
  AtomicWait,
  Binary,
  Block,
  Br,
  BrIf,
  BrTable,
  Call,
  CallIndirect,
  CallRef,
  Compare,
  Const,
  Convert,
  Drop,
  GlobalGet,
  GlobalSet,
  If,
  Load,
  LocalGet,
  LocalSet,
  LocalTee,
  Loop,
  MemoryCopy,
  DataDrop,
  MemoryFill,
  MemoryGrow,
  MemoryInit,
  MemorySize,
  Nop,
  RefIsNull,
  RefFunc,
  RefNull,
  Rethrow,
  Return,
  ReturnCall,
  ReturnCallIndirect,
  Select,
  SimdLaneOp,
  SimdLoadLane,
  SimdStoreLane,
  SimdShuffleOp,
  LoadSplat,
  LoadZero,
  Store,
  TableCopy,
  ElemDrop,
  TableInit,
  TableGet,
  TableGrow,
  TableSize,
  TableSet,
  TableFill,
  Ternary,
  Throw,
  Try,
  Unary,
  Unreachable,

  First = AtomicLoad,
  Last = Unreachable
};

const char* GetExprTypeName(ExprType type);

class Expr;
typedef intrusive_list<Expr> ExprList;

typedef FuncDeclaration BlockDeclaration;

struct Block {
  Block() = default;
  explicit Block(ExprList exprs) : exprs(std::move(exprs)) {}

  std::string label;
  BlockDeclaration decl;
  ExprList exprs;
  Location end_loc;
};

struct Catch {
  explicit Catch(const Location& loc = Location()) : loc(loc) {}
  explicit Catch(const Var& var, const Location& loc = Location())
      : loc(loc), var(var) {}
  Location loc;
  Var var;
  ExprList exprs;
  bool IsCatchAll() const {
    return var.is_index() && var.index() == kInvalidIndex;
  }
};
typedef std::vector<Catch> CatchVector;

enum class TryKind {
  Plain,
  Catch,
  Delegate
};

class Expr : public intrusive_list_base<Expr> {
 public:
  WABT_DISALLOW_COPY_AND_ASSIGN(Expr);
  Expr() = delete;
  virtual ~Expr() = default;

  ExprType type() const { return type_; }

  Location loc;

 protected:
  explicit Expr(ExprType type, const Location& loc = Location())
      : loc(loc), type_(type) {}

  ExprType type_;
};

const char* GetExprTypeName(const Expr& expr);

template <ExprType TypeEnum>
class ExprMixin : public Expr {
 public:
  static bool classof(const Expr* expr) { return expr->type() == TypeEnum; }

  explicit ExprMixin(const Location& loc = Location()) : Expr(TypeEnum, loc) {}
};

typedef ExprMixin<ExprType::Drop> DropExpr;
typedef ExprMixin<ExprType::MemoryGrow> MemoryGrowExpr;
typedef ExprMixin<ExprType::MemorySize> MemorySizeExpr;
typedef ExprMixin<ExprType::MemoryCopy> MemoryCopyExpr;
typedef ExprMixin<ExprType::MemoryFill> MemoryFillExpr;
typedef ExprMixin<ExprType::Nop> NopExpr;
typedef ExprMixin<ExprType::Return> ReturnExpr;
typedef ExprMixin<ExprType::Unreachable> UnreachableExpr;

template <ExprType TypeEnum>
class RefTypeExpr : public ExprMixin<TypeEnum> {
 public:
  RefTypeExpr(Type type, const Location& loc = Location())
      : ExprMixin<TypeEnum>(loc), type(type) {}

  Type type;
};

typedef RefTypeExpr<ExprType::RefNull> RefNullExpr;
typedef ExprMixin<ExprType::RefIsNull> RefIsNullExpr;

template <ExprType TypeEnum>
class OpcodeExpr : public ExprMixin<TypeEnum> {
 public:
  OpcodeExpr(Opcode opcode, const Location& loc = Location())
      : ExprMixin<TypeEnum>(loc), opcode(opcode) {}

  Opcode opcode;
};

typedef OpcodeExpr<ExprType::Binary> BinaryExpr;
typedef OpcodeExpr<ExprType::Compare> CompareExpr;
typedef OpcodeExpr<ExprType::Convert> ConvertExpr;
typedef OpcodeExpr<ExprType::Unary> UnaryExpr;
typedef OpcodeExpr<ExprType::Ternary> TernaryExpr;

class SimdLaneOpExpr : public ExprMixin<ExprType::SimdLaneOp> {
 public:
  SimdLaneOpExpr(Opcode opcode, uint64_t val, const Location& loc = Location())
      : ExprMixin<ExprType::SimdLaneOp>(loc), opcode(opcode), val(val) {}

  Opcode opcode;
  uint64_t val;
};

class SimdLoadLaneExpr : public OpcodeExpr<ExprType::SimdLoadLane> {
 public:
  SimdLoadLaneExpr(Opcode opcode,
                   Address align,
                   Address offset,
                   uint64_t val,
                   const Location& loc = Location())
      : OpcodeExpr<ExprType::SimdLoadLane>(opcode, loc),
        align(align),
        offset(offset),
        val(val) {}

  Address align;
  Address offset;
  uint64_t val;
};

class SimdStoreLaneExpr : public OpcodeExpr<ExprType::SimdStoreLane> {
 public:
  SimdStoreLaneExpr(Opcode opcode,
                    Address align,
                    Address offset,
                    uint64_t val,
                    const Location& loc = Location())
      : OpcodeExpr<ExprType::SimdStoreLane>(opcode, loc),
        align(align),
        offset(offset),
        val(val) {}

  Address align;
  Address offset;
  uint64_t val;
};

class SimdShuffleOpExpr : public ExprMixin<ExprType::SimdShuffleOp> {
 public:
  SimdShuffleOpExpr(Opcode opcode, v128 val, const Location& loc = Location())
      : ExprMixin<ExprType::SimdShuffleOp>(loc), opcode(opcode), val(val) {}

  Opcode opcode;
  v128 val;
};

template <ExprType TypeEnum>
class VarExpr : public ExprMixin<TypeEnum> {
 public:
  VarExpr(const Var& var, const Location& loc = Location())
      : ExprMixin<TypeEnum>(loc), var(var) {}

  Var var;
};

typedef VarExpr<ExprType::Br> BrExpr;
typedef VarExpr<ExprType::BrIf> BrIfExpr;
typedef VarExpr<ExprType::Call> CallExpr;
typedef VarExpr<ExprType::RefFunc> RefFuncExpr;
typedef VarExpr<ExprType::GlobalGet> GlobalGetExpr;
typedef VarExpr<ExprType::GlobalSet> GlobalSetExpr;
typedef VarExpr<ExprType::LocalGet> LocalGetExpr;
typedef VarExpr<ExprType::LocalSet> LocalSetExpr;
typedef VarExpr<ExprType::LocalTee> LocalTeeExpr;
typedef VarExpr<ExprType::ReturnCall> ReturnCallExpr;
typedef VarExpr<ExprType::Throw> ThrowExpr;
typedef VarExpr<ExprType::Rethrow> RethrowExpr;

typedef VarExpr<ExprType::MemoryInit> MemoryInitExpr;
typedef VarExpr<ExprType::DataDrop> DataDropExpr;
typedef VarExpr<ExprType::ElemDrop> ElemDropExpr;
typedef VarExpr<ExprType::TableGet> TableGetExpr;
typedef VarExpr<ExprType::TableSet> TableSetExpr;
typedef VarExpr<ExprType::TableGrow> TableGrowExpr;
typedef VarExpr<ExprType::TableSize> TableSizeExpr;
typedef VarExpr<ExprType::TableFill> TableFillExpr;

class SelectExpr : public ExprMixin<ExprType::Select> {
 public:
  SelectExpr(TypeVector type, const Location& loc = Location())
      : ExprMixin<ExprType::Select>(loc), result_type(type) {}
  TypeVector result_type;
};

class TableInitExpr : public ExprMixin<ExprType::TableInit> {
 public:
  TableInitExpr(const Var& segment_index,
                const Var& table_index,
                const Location& loc = Location())
      : ExprMixin<ExprType::TableInit>(loc),
        segment_index(segment_index),
        table_index(table_index) {}

  Var segment_index;
  Var table_index;
};

class TableCopyExpr : public ExprMixin<ExprType::TableCopy> {
 public:
  TableCopyExpr(const Var& dst,
                const Var& src,
                const Location& loc = Location())
      : ExprMixin<ExprType::TableCopy>(loc), dst_table(dst), src_table(src) {}

  Var dst_table;
  Var src_table;
};

class CallIndirectExpr : public ExprMixin<ExprType::CallIndirect> {
 public:
  explicit CallIndirectExpr(const Location& loc = Location())
      : ExprMixin<ExprType::CallIndirect>(loc) {}

  FuncDeclaration decl;
  Var table;
};

class ReturnCallIndirectExpr : public ExprMixin<ExprType::ReturnCallIndirect> {
 public:
  explicit ReturnCallIndirectExpr(const Location &loc = Location())
      : ExprMixin<ExprType::ReturnCallIndirect>(loc) {}

  FuncDeclaration decl;
  Var table;
};

class CallRefExpr : public ExprMixin<ExprType::CallRef> {
 public:
  explicit CallRefExpr(const Location &loc = Location())
      : ExprMixin<ExprType::CallRef>(loc) {}

  // This field is setup only during Validate phase,
  // so keep that in mind when you use it.
  Var function_type_index;
};

template <ExprType TypeEnum>
class BlockExprBase : public ExprMixin<TypeEnum> {
 public:
  explicit BlockExprBase(const Location& loc = Location())
      : ExprMixin<TypeEnum>(loc) {}

  Block block;
};

typedef BlockExprBase<ExprType::Block> BlockExpr;
typedef BlockExprBase<ExprType::Loop> LoopExpr;

class IfExpr : public ExprMixin<ExprType::If> {
 public:
  explicit IfExpr(const Location& loc = Location())
      : ExprMixin<ExprType::If>(loc) {}

  Block true_;
  ExprList false_;
  Location false_end_loc;
};

class TryExpr : public ExprMixin<ExprType::Try> {
 public:
  explicit TryExpr(const Location& loc = Location())
      : ExprMixin<ExprType::Try>(loc), kind(TryKind::Plain) {}

  TryKind kind;
  Block block;
  CatchVector catches;
  Var delegate_target;
};

class BrTableExpr : public ExprMixin<ExprType::BrTable> {
 public:
  BrTableExpr(const Location& loc = Location())
      : ExprMixin<ExprType::BrTable>(loc) {}

  VarVector targets;
  Var default_target;
};

class ConstExpr : public ExprMixin<ExprType::Const> {
 public:
  ConstExpr(const Const& c, const Location& loc = Location())
      : ExprMixin<ExprType::Const>(loc), const_(c) {}

  Const const_;
};

// TODO(binji): Rename this, it is used for more than loads/stores now.
template <ExprType TypeEnum>
class LoadStoreExpr : public ExprMixin<TypeEnum> {
 public:
  LoadStoreExpr(Opcode opcode,
                Address align,
                Address offset,
                const Location& loc = Location())
      : ExprMixin<TypeEnum>(loc),
        opcode(opcode),
        align(align),
        offset(offset) {}

  Opcode opcode;
  Address align;
  Address offset;
};

typedef LoadStoreExpr<ExprType::Load> LoadExpr;
typedef LoadStoreExpr<ExprType::Store> StoreExpr;
typedef LoadStoreExpr<ExprType::AtomicLoad> AtomicLoadExpr;
typedef LoadStoreExpr<ExprType::AtomicStore> AtomicStoreExpr;
typedef LoadStoreExpr<ExprType::AtomicRmw> AtomicRmwExpr;
typedef LoadStoreExpr<ExprType::AtomicRmwCmpxchg> AtomicRmwCmpxchgExpr;
typedef LoadStoreExpr<ExprType::AtomicWait> AtomicWaitExpr;
typedef LoadStoreExpr<ExprType::AtomicNotify> AtomicNotifyExpr;
typedef LoadStoreExpr<ExprType::LoadSplat> LoadSplatExpr;
typedef LoadStoreExpr<ExprType::LoadZero> LoadZeroExpr;

class AtomicFenceExpr : public ExprMixin<ExprType::AtomicFence> {
 public:
  explicit AtomicFenceExpr(uint32_t consistency_model,
                           const Location& loc = Location())
      : ExprMixin<ExprType::AtomicFence>(loc),
        consistency_model(consistency_model) {}

  uint32_t consistency_model;
};

struct Tag {
  explicit Tag(string_view name) : name(name.to_string()) {}

  std::string name;
  FuncDeclaration decl;
};

class LocalTypes {
 public:
  typedef std::pair<Type, Index> Decl;
  typedef std::vector<Decl> Decls;

  struct const_iterator {
    const_iterator(Decls::const_iterator decl, Index index)
        : decl(decl), index(index) {}
    Type operator*() const { return decl->first; }
    const_iterator& operator++();
    const_iterator operator++(int);

    Decls::const_iterator decl;
    Index index;
  };

  void Set(const TypeVector&);

  const Decls& decls() const { return decls_; }

  void AppendDecl(Type type, Index count) {
    if (count != 0) {
      decls_.emplace_back(type, count);
    }
  }

  Index size() const;
  Type operator[](Index) const;

  const_iterator begin() const { return {decls_.begin(), 0}; }
  const_iterator end() const { return {decls_.end(), 0}; }

 private:
  Decls decls_;
};

inline LocalTypes::const_iterator& LocalTypes::const_iterator::operator++() {
  ++index;
  if (index >= decl->second) {
    ++decl;
    index = 0;
  }
  return *this;
}

inline LocalTypes::const_iterator LocalTypes::const_iterator::operator++(int) {
  const_iterator result = *this;
  operator++();
  return result;
}

inline bool operator==(const LocalTypes::const_iterator& lhs,
                       const LocalTypes::const_iterator& rhs) {
  return lhs.decl == rhs.decl && lhs.index == rhs.index;
}

inline bool operator!=(const LocalTypes::const_iterator& lhs,
                       const LocalTypes::const_iterator& rhs) {
  return !operator==(lhs, rhs);
}

struct Func {
  explicit Func(string_view name) : name(name.to_string()) {}

  Type GetParamType(Index index) const { return decl.GetParamType(index); }
  Type GetResultType(Index index) const { return decl.GetResultType(index); }
  Type GetLocalType(Index index) const;
  Type GetLocalType(const Var& var) const;
  Index GetNumParams() const { return decl.GetNumParams(); }
  Index GetNumLocals() const { return local_types.size(); }
  Index GetNumParamsAndLocals() const {
    return GetNumParams() + GetNumLocals();
  }
  Index GetNumResults() const { return decl.GetNumResults(); }
  Index GetLocalIndex(const Var&) const;

  std::string name;
  FuncDeclaration decl;
  LocalTypes local_types;
  BindingHash bindings;
  ExprList exprs;
};

struct Global {
  explicit Global(string_view name) : name(name.to_string()) {}

  std::string name;
  Type type = Type::Void;
  bool mutable_ = false;
  ExprList init_expr;
};

struct Table {
  explicit Table(string_view name)
      : name(name.to_string()), elem_type(Type::FuncRef) {}

  std::string name;
  Limits elem_limits;
  Type elem_type;
};

enum class ElemExprKind {
  RefNull,
  RefFunc,
};

struct ElemExpr {
  ElemExpr() : kind(ElemExprKind::RefNull), type(Type::FuncRef) {}
  explicit ElemExpr(Var var) : kind(ElemExprKind::RefFunc), var(var) {}
  explicit ElemExpr(Type type) : kind(ElemExprKind::RefNull), type(type) {}

  ElemExprKind kind;
  Var var;    // Only used when kind == RefFunc.
  Type type;  // Only used when kind == RefNull
};

typedef std::vector<ElemExpr> ElemExprVector;

struct ElemSegment {
  explicit ElemSegment(string_view name) : name(name.to_string()) {}
  uint8_t GetFlags(const Module*) const;

  SegmentKind kind = SegmentKind::Active;
  std::string name;
  Var table_var;
  Type elem_type;
  ExprList offset;
  ElemExprVector elem_exprs;
};

struct Memory {
  explicit Memory(string_view name) : name(name.to_string()) {}

  std::string name;
  Limits page_limits;
};

struct DataSegment {
  explicit DataSegment(string_view name) : name(name.to_string()) {}
  uint8_t GetFlags(const Module*) const;

  SegmentKind kind = SegmentKind::Active;
  std::string name;
  Var memory_var;
  ExprList offset;
  std::vector<uint8_t> data;
};

class Import {
 public:
  WABT_DISALLOW_COPY_AND_ASSIGN(Import);
  Import() = delete;
  virtual ~Import() = default;

  ExternalKind kind() const { return kind_; }

  std::string module_name;
  std::string field_name;

 protected:
  Import(ExternalKind kind) : kind_(kind) {}

  ExternalKind kind_;
};

template <ExternalKind TypeEnum>
class ImportMixin : public Import {
 public:
  static bool classof(const Import* import) {
    return import->kind() == TypeEnum;
  }

  ImportMixin() : Import(TypeEnum) {}
};

class FuncImport : public ImportMixin<ExternalKind::Func> {
 public:
  explicit FuncImport(string_view name = string_view())
      : ImportMixin<ExternalKind::Func>(), func(name) {}

  Func func;
};

class TableImport : public ImportMixin<ExternalKind::Table> {
 public:
  explicit TableImport(string_view name = string_view())
      : ImportMixin<ExternalKind::Table>(), table(name) {}

  Table table;
};

class MemoryImport : public ImportMixin<ExternalKind::Memory> {
 public:
  explicit MemoryImport(string_view name = string_view())
      : ImportMixin<ExternalKind::Memory>(), memory(name) {}

  Memory memory;
};

class GlobalImport : public ImportMixin<ExternalKind::Global> {
 public:
  explicit GlobalImport(string_view name = string_view())
      : ImportMixin<ExternalKind::Global>(), global(name) {}

  Global global;
};

class TagImport : public ImportMixin<ExternalKind::Tag> {
 public:
  explicit TagImport(string_view name = string_view())
      : ImportMixin<ExternalKind::Tag>(), tag(name) {}

  Tag tag;
};

struct Export {
  std::string name;
  ExternalKind kind;
  Var var;
};

enum class ModuleFieldType {
  Func,
  Global,
  Import,
  Export,
  Type,
  Table,
  ElemSegment,
  Memory,
  DataSegment,
  Start,
  Tag
};

class ModuleField : public intrusive_list_base<ModuleField> {
 public:
  WABT_DISALLOW_COPY_AND_ASSIGN(ModuleField);
  ModuleField() = delete;
  virtual ~ModuleField() = default;

  ModuleFieldType type() const { return type_; }

  Location loc;

 protected:
  ModuleField(ModuleFieldType type, const Location& loc)
      : loc(loc), type_(type) {}

  ModuleFieldType type_;
};

typedef intrusive_list<ModuleField> ModuleFieldList;

template <ModuleFieldType TypeEnum>
class ModuleFieldMixin : public ModuleField {
 public:
  static bool classof(const ModuleField* field) {
    return field->type() == TypeEnum;
  }

  explicit ModuleFieldMixin(const Location& loc) : ModuleField(TypeEnum, loc) {}
};

class FuncModuleField : public ModuleFieldMixin<ModuleFieldType::Func> {
 public:
  explicit FuncModuleField(const Location& loc = Location(),
                           string_view name = string_view())
      : ModuleFieldMixin<ModuleFieldType::Func>(loc), func(name) {}

  Func func;
};

class GlobalModuleField : public ModuleFieldMixin<ModuleFieldType::Global> {
 public:
  explicit GlobalModuleField(const Location& loc = Location(),
                             string_view name = string_view())
      : ModuleFieldMixin<ModuleFieldType::Global>(loc), global(name) {}

  Global global;
};

class ImportModuleField : public ModuleFieldMixin<ModuleFieldType::Import> {
 public:
  explicit ImportModuleField(const Location& loc = Location())
      : ModuleFieldMixin<ModuleFieldType::Import>(loc) {}
  explicit ImportModuleField(std::unique_ptr<Import> import,
                             const Location& loc = Location())
      : ModuleFieldMixin<ModuleFieldType::Import>(loc),
        import(std::move(import)) {}

  std::unique_ptr<Import> import;
};

class ExportModuleField : public ModuleFieldMixin<ModuleFieldType::Export> {
 public:
  explicit ExportModuleField(const Location& loc = Location())
      : ModuleFieldMixin<ModuleFieldType::Export>(loc) {}

  Export export_;
};

class TypeModuleField : public ModuleFieldMixin<ModuleFieldType::Type> {
 public:
  explicit TypeModuleField(const Location& loc = Location())
      : ModuleFieldMixin<ModuleFieldType::Type>(loc) {}

  std::unique_ptr<TypeEntry> type;
};

class TableModuleField : public ModuleFieldMixin<ModuleFieldType::Table> {
 public:
  explicit TableModuleField(const Location& loc = Location(),
                            string_view name = string_view())
      : ModuleFieldMixin<ModuleFieldType::Table>(loc), table(name) {}

  Table table;
};

class ElemSegmentModuleField
    : public ModuleFieldMixin<ModuleFieldType::ElemSegment> {
 public:
  explicit ElemSegmentModuleField(const Location& loc = Location(),
                                  string_view name = string_view())
      : ModuleFieldMixin<ModuleFieldType::ElemSegment>(loc),
        elem_segment(name) {}

  ElemSegment elem_segment;
};

class MemoryModuleField : public ModuleFieldMixin<ModuleFieldType::Memory> {
 public:
  explicit MemoryModuleField(const Location& loc = Location(),
                             string_view name = string_view())
      : ModuleFieldMixin<ModuleFieldType::Memory>(loc), memory(name) {}

  Memory memory;
};

class DataSegmentModuleField
    : public ModuleFieldMixin<ModuleFieldType::DataSegment> {
 public:
  explicit DataSegmentModuleField(const Location& loc = Location(),
                                  string_view name = string_view())
      : ModuleFieldMixin<ModuleFieldType::DataSegment>(loc),
        data_segment(name) {}

  DataSegment data_segment;
};

class TagModuleField : public ModuleFieldMixin<ModuleFieldType::Tag> {
 public:
  explicit TagModuleField(const Location& loc = Location(),
                          string_view name = string_view())
      : ModuleFieldMixin<ModuleFieldType::Tag>(loc), tag(name) {}

  Tag tag;
};

class StartModuleField : public ModuleFieldMixin<ModuleFieldType::Start> {
 public:
  explicit StartModuleField(Var start = Var(), const Location& loc = Location())
      : ModuleFieldMixin<ModuleFieldType::Start>(loc), start(start) {}

  Var start;
};

struct Module {
  Index GetFuncTypeIndex(const Var&) const;
  Index GetFuncTypeIndex(const FuncDeclaration&) const;
  Index GetFuncTypeIndex(const FuncSignature&) const;
  const FuncType* GetFuncType(const Var&) const;
  FuncType* GetFuncType(const Var&);
  Index GetFuncIndex(const Var&) const;
  const Func* GetFunc(const Var&) const;
  Func* GetFunc(const Var&);
  Index GetTableIndex(const Var&) const;
  const Table* GetTable(const Var&) const;
  Table* GetTable(const Var&);
  Index GetMemoryIndex(const Var&) const;
  const Memory* GetMemory(const Var&) const;
  Memory* GetMemory(const Var&);
  Index GetGlobalIndex(const Var&) const;
  const Global* GetGlobal(const Var&) const;
  Global* GetGlobal(const Var&);
  const Export* GetExport(string_view) const;
  Tag* GetTag(const Var&) const;
  Index GetTagIndex(const Var&) const;
  const DataSegment* GetDataSegment(const Var&) const;
  DataSegment* GetDataSegment(const Var&);
  Index GetDataSegmentIndex(const Var&) const;
  const ElemSegment* GetElemSegment(const Var&) const;
  ElemSegment* GetElemSegment(const Var&);
  Index GetElemSegmentIndex(const Var&) const;

  bool IsImport(ExternalKind kind, const Var&) const;
  bool IsImport(const Export& export_) const {
    return IsImport(export_.kind, export_.var);
  }

  // TODO(binji): move this into a builder class?
  void AppendField(std::unique_ptr<DataSegmentModuleField>);
  void AppendField(std::unique_ptr<ElemSegmentModuleField>);
  void AppendField(std::unique_ptr<TagModuleField>);
  void AppendField(std::unique_ptr<ExportModuleField>);
  void AppendField(std::unique_ptr<FuncModuleField>);
  void AppendField(std::unique_ptr<TypeModuleField>);
  void AppendField(std::unique_ptr<GlobalModuleField>);
  void AppendField(std::unique_ptr<ImportModuleField>);
  void AppendField(std::unique_ptr<MemoryModuleField>);
  void AppendField(std::unique_ptr<StartModuleField>);
  void AppendField(std::unique_ptr<TableModuleField>);
  void AppendField(std::unique_ptr<ModuleField>);
  void AppendFields(ModuleFieldList*);

  Location loc;
  std::string name;
  ModuleFieldList fields;

  Index num_tag_imports = 0;
  Index num_func_imports = 0;
  Index num_table_imports = 0;
  Index num_memory_imports = 0;
  Index num_global_imports = 0;

  // Cached for convenience; the pointers are shared with values that are
  // stored in either ModuleField or Import.
  std::vector<Tag*> tags;
  std::vector<Func*> funcs;
  std::vector<Global*> globals;
  std::vector<Import*> imports;
  std::vector<Export*> exports;
  std::vector<TypeEntry*> types;
  std::vector<Table*> tables;
  std::vector<ElemSegment*> elem_segments;
  std::vector<Memory*> memories;
  std::vector<DataSegment*> data_segments;
  std::vector<Var*> starts;

  BindingHash tag_bindings;
  BindingHash func_bindings;
  BindingHash global_bindings;
  BindingHash export_bindings;
  BindingHash type_bindings;
  BindingHash table_bindings;
  BindingHash memory_bindings;
  BindingHash data_segment_bindings;
  BindingHash elem_segment_bindings;
};

enum class ScriptModuleType {
  Text,
  Binary,
  Quoted,
};

// A ScriptModule is a module that may not yet be decoded. This allows for text
// and binary parsing errors to be deferred until validation time.
class ScriptModule {
 public:
  WABT_DISALLOW_COPY_AND_ASSIGN(ScriptModule);
  ScriptModule() = delete;
  virtual ~ScriptModule() = default;

  ScriptModuleType type() const { return type_; }
  virtual const Location& location() const = 0;

 protected:
  explicit ScriptModule(ScriptModuleType type) : type_(type) {}

  ScriptModuleType type_;
};

template <ScriptModuleType TypeEnum>
class ScriptModuleMixin : public ScriptModule {
 public:
  static bool classof(const ScriptModule* script_module) {
    return script_module->type() == TypeEnum;
  }

  ScriptModuleMixin() : ScriptModule(TypeEnum) {}
};

class TextScriptModule : public ScriptModuleMixin<ScriptModuleType::Text> {
 public:
  const Location& location() const override { return module.loc; }

  Module module;
};

template <ScriptModuleType TypeEnum>
class DataScriptModule : public ScriptModuleMixin<TypeEnum> {
 public:
  const Location& location() const override { return loc; }

  Location loc;
  std::string name;
  std::vector<uint8_t> data;
};

typedef DataScriptModule<ScriptModuleType::Binary> BinaryScriptModule;
typedef DataScriptModule<ScriptModuleType::Quoted> QuotedScriptModule;

enum class ActionType {
  Invoke,
  Get,
};

class Action {
 public:
  WABT_DISALLOW_COPY_AND_ASSIGN(Action);
  Action() = delete;
  virtual ~Action() = default;

  ActionType type() const { return type_; }

  Location loc;
  Var module_var;
  std::string name;

 protected:
  explicit Action(ActionType type, const Location& loc = Location())
      : loc(loc), type_(type) {}

  ActionType type_;
};

typedef std::unique_ptr<Action> ActionPtr;

template <ActionType TypeEnum>
class ActionMixin : public Action {
 public:
  static bool classof(const Action* action) {
    return action->type() == TypeEnum;
  }

  explicit ActionMixin(const Location& loc = Location())
      : Action(TypeEnum, loc) {}
};

class GetAction : public ActionMixin<ActionType::Get> {
 public:
  explicit GetAction(const Location& loc = Location())
      : ActionMixin<ActionType::Get>(loc) {}
};

class InvokeAction : public ActionMixin<ActionType::Invoke> {
 public:
  explicit InvokeAction(const Location& loc = Location())
      : ActionMixin<ActionType::Invoke>(loc) {}

  ConstVector args;
};

enum class CommandType {
  Module,
  Action,
  Register,
  AssertMalformed,
  AssertInvalid,
  AssertUnlinkable,
  AssertUninstantiable,
  AssertReturn,
  AssertTrap,
  AssertExhaustion,

  First = Module,
  Last = AssertExhaustion,
};
static const int kCommandTypeCount = WABT_ENUM_COUNT(CommandType);

class Command {
 public:
  WABT_DISALLOW_COPY_AND_ASSIGN(Command);
  Command() = delete;
  virtual ~Command() = default;

  CommandType type;

 protected:
  explicit Command(CommandType type) : type(type) {}
};

template <CommandType TypeEnum>
class CommandMixin : public Command {
 public:
  static bool classof(const Command* cmd) { return cmd->type == TypeEnum; }
  CommandMixin() : Command(TypeEnum) {}
};

class ModuleCommand : public CommandMixin<CommandType::Module> {
 public:
  Module module;
};

template <CommandType TypeEnum>
class ActionCommandBase : public CommandMixin<TypeEnum> {
 public:
  ActionPtr action;
};

typedef ActionCommandBase<CommandType::Action> ActionCommand;

class RegisterCommand : public CommandMixin<CommandType::Register> {
 public:
  RegisterCommand(string_view module_name, const Var& var)
      : module_name(module_name), var(var) {}

  std::string module_name;
  Var var;
};

class AssertReturnCommand : public CommandMixin<CommandType::AssertReturn> {
 public:
  ActionPtr action;
  ConstVector expected;
};

template <CommandType TypeEnum>
class AssertTrapCommandBase : public CommandMixin<TypeEnum> {
 public:
  ActionPtr action;
  std::string text;
};

typedef AssertTrapCommandBase<CommandType::AssertTrap> AssertTrapCommand;
typedef AssertTrapCommandBase<CommandType::AssertExhaustion>
    AssertExhaustionCommand;

template <CommandType TypeEnum>
class AssertModuleCommand : public CommandMixin<TypeEnum> {
 public:
  std::unique_ptr<ScriptModule> module;
  std::string text;
};

typedef AssertModuleCommand<CommandType::AssertMalformed>
    AssertMalformedCommand;
typedef AssertModuleCommand<CommandType::AssertInvalid> AssertInvalidCommand;
typedef AssertModuleCommand<CommandType::AssertUnlinkable>
    AssertUnlinkableCommand;
typedef AssertModuleCommand<CommandType::AssertUninstantiable>
    AssertUninstantiableCommand;

typedef std::unique_ptr<Command> CommandPtr;
typedef std::vector<CommandPtr> CommandPtrVector;

struct Script {
  WABT_DISALLOW_COPY_AND_ASSIGN(Script);
  Script() = default;

  const Module* GetFirstModule() const;
  Module* GetFirstModule();
  const Module* GetModule(const Var&) const;

  CommandPtrVector commands;
  BindingHash module_bindings;
};

void MakeTypeBindingReverseMapping(
    size_t num_types,
    const BindingHash& bindings,
    std::vector<std::string>* out_reverse_mapping);

}  // namespace wabt

#endif /* WABT_IR_H_ */
