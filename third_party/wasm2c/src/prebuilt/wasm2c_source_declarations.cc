const char* s_source_declarations = R"w2c_template(
#define TRAP(x) (wasm_rt_trap(WASM_RT_TRAP_##x), 0)
)w2c_template"
R"w2c_template(
#if WASM_RT_USE_STACK_DEPTH_COUNT
)w2c_template"
R"w2c_template(#define FUNC_PROLOGUE                                            \
)w2c_template"
R"w2c_template(  if (++wasm_rt_call_stack_depth > WASM_RT_MAX_CALL_STACK_DEPTH) \
)w2c_template"
R"w2c_template(    TRAP(EXHAUSTION);
)w2c_template"
R"w2c_template(
#define FUNC_EPILOGUE --wasm_rt_call_stack_depth
)w2c_template"
R"w2c_template(#else
)w2c_template"
R"w2c_template(#define FUNC_PROLOGUE
)w2c_template"
R"w2c_template(
#define FUNC_EPILOGUE
)w2c_template"
R"w2c_template(#endif
)w2c_template"
R"w2c_template(
#define UNREACHABLE TRAP(UNREACHABLE)
)w2c_template"
R"w2c_template(
static inline bool func_types_eq(const wasm_rt_func_type_t a,
)w2c_template"
R"w2c_template(                                 const wasm_rt_func_type_t b) {
)w2c_template"
R"w2c_template(  return (a == b) || LIKELY(a && b && !memcmp(a, b, 32));
)w2c_template"
R"w2c_template(}
)w2c_template"
R"w2c_template(
#define CALL_INDIRECT(table, t, ft, x, ...)              \
)w2c_template"
R"w2c_template(  (LIKELY((x) < table.size && table.data[x].func &&      \
)w2c_template"
R"w2c_template(          func_types_eq(ft, table.data[x].func_type)) || \
)w2c_template"
R"w2c_template(       TRAP(CALL_INDIRECT),                              \
)w2c_template"
R"w2c_template(   ((t)table.data[x].func)(__VA_ARGS__))
)w2c_template"
R"w2c_template(
#ifdef SUPPORT_MEMORY64
)w2c_template"
R"w2c_template(#define RANGE_CHECK(mem, offset, len)              \
)w2c_template"
R"w2c_template(  do {                                             \
)w2c_template"
R"w2c_template(    uint64_t res;                                  \
)w2c_template"
R"w2c_template(    if (__builtin_add_overflow(offset, len, &res)) \
)w2c_template"
R"w2c_template(      TRAP(OOB);                                   \
)w2c_template"
R"w2c_template(    if (UNLIKELY(res > mem->size))                 \
)w2c_template"
R"w2c_template(      TRAP(OOB);                                   \
)w2c_template"
R"w2c_template(  } while (0);
)w2c_template"
R"w2c_template(#else
)w2c_template"
R"w2c_template(#define RANGE_CHECK(mem, offset, len)               \
)w2c_template"
R"w2c_template(  if (UNLIKELY(offset + (uint64_t)len > mem->size)) \
)w2c_template"
R"w2c_template(    TRAP(OOB);
)w2c_template"
R"w2c_template(#endif
)w2c_template"
R"w2c_template(
#if WASM_RT_MEMCHECK_GUARD_PAGES
)w2c_template"
R"w2c_template(#define MEMCHECK(mem, a, t)
)w2c_template"
R"w2c_template(#else
)w2c_template"
R"w2c_template(#define MEMCHECK(mem, a, t) RANGE_CHECK(mem, a, sizeof(t))
)w2c_template"
R"w2c_template(#endif
)w2c_template"
R"w2c_template(
#ifdef __GNUC__
)w2c_template"
R"w2c_template(#define wasm_asm __asm__
)w2c_template"
R"w2c_template(#else
)w2c_template"
R"w2c_template(#define wasm_asm(X)
)w2c_template"
R"w2c_template(#endif
)w2c_template"
R"w2c_template(
#if WABT_BIG_ENDIAN
)w2c_template"
R"w2c_template(static inline void load_data(void* dest, const void* src, size_t n) {
)w2c_template"
R"w2c_template(  if (!n) {
)w2c_template"
R"w2c_template(    return;
)w2c_template"
R"w2c_template(  }
)w2c_template"
R"w2c_template(  size_t i = 0;
)w2c_template"
R"w2c_template(  u8* dest_chars = dest;
)w2c_template"
R"w2c_template(  memcpy(dest, src, n);
)w2c_template"
R"w2c_template(  for (i = 0; i < (n >> 1); i++) {
)w2c_template"
R"w2c_template(    u8 cursor = dest_chars[i];
)w2c_template"
R"w2c_template(    dest_chars[i] = dest_chars[n - i - 1];
)w2c_template"
R"w2c_template(    dest_chars[n - i - 1] = cursor;
)w2c_template"
R"w2c_template(  }
)w2c_template"
R"w2c_template(}
)w2c_template"
R"w2c_template(#define LOAD_DATA(m, o, i, s)                   \
)w2c_template"
R"w2c_template(  do {                                          \
)w2c_template"
R"w2c_template(    RANGE_CHECK((&m), m.size - o - s, s);       \
)w2c_template"
R"w2c_template(    load_data(&(m.data[m.size - o - s]), i, s); \
)w2c_template"
R"w2c_template(  } while (0)
)w2c_template"
R"w2c_template(#define DEFINE_LOAD(name, t1, t2, t3)                                  \
)w2c_template"
R"w2c_template(  static inline t3 name(wasm_rt_memory_t* mem, u64 addr) {             \
)w2c_template"
R"w2c_template(    MEMCHECK(mem, addr, t1);                                           \
)w2c_template"
R"w2c_template(    t1 result;                                                         \
)w2c_template"
R"w2c_template(    wasm_rt_memcpy(&result, &mem->data[mem->size - addr - sizeof(t1)], \
)w2c_template"
R"w2c_template(                   sizeof(t1));                                        \
)w2c_template"
R"w2c_template(    wasm_asm("" ::"r"(result));                                        \
)w2c_template"
R"w2c_template(    return (t3)(t2)result;                                             \
)w2c_template"
R"w2c_template(  }
)w2c_template"
R"w2c_template(
#define DEFINE_STORE(name, t1, t2)                                      \
)w2c_template"
R"w2c_template(  static inline void name(wasm_rt_memory_t* mem, u64 addr, t2 value) {  \
)w2c_template"
R"w2c_template(    MEMCHECK(mem, addr, t1);                                            \
)w2c_template"
R"w2c_template(    t1 wrapped = (t1)value;                                             \
)w2c_template"
R"w2c_template(    wasm_rt_memcpy(&mem->data[mem->size - addr - sizeof(t1)], &wrapped, \
)w2c_template"
R"w2c_template(                   sizeof(t1));                                         \
)w2c_template"
R"w2c_template(  }
)w2c_template"
R"w2c_template(#else
)w2c_template"
R"w2c_template(static inline void load_data(void* dest, const void* src, size_t n) {
)w2c_template"
R"w2c_template(  if (!n) {
)w2c_template"
R"w2c_template(    return;
)w2c_template"
R"w2c_template(  }
)w2c_template"
R"w2c_template(  memcpy(dest, src, n);
)w2c_template"
R"w2c_template(}
)w2c_template"
R"w2c_template(#define LOAD_DATA(m, o, i, s)      \
)w2c_template"
R"w2c_template(  do {                             \
)w2c_template"
R"w2c_template(    RANGE_CHECK((&m), o, s);       \
)w2c_template"
R"w2c_template(    load_data(&(m.data[o]), i, s); \
)w2c_template"
R"w2c_template(  } while (0)
)w2c_template"
R"w2c_template(#define DEFINE_LOAD(name, t1, t2, t3)                      \
)w2c_template"
R"w2c_template(  static inline t3 name(wasm_rt_memory_t* mem, u64 addr) { \
)w2c_template"
R"w2c_template(    MEMCHECK(mem, addr, t1);                               \
)w2c_template"
R"w2c_template(    t1 result;                                             \
)w2c_template"
R"w2c_template(    wasm_rt_memcpy(&result, &mem->data[addr], sizeof(t1)); \
)w2c_template"
R"w2c_template(    wasm_asm("" ::"r"(result));                            \
)w2c_template"
R"w2c_template(    return (t3)(t2)result;                                 \
)w2c_template"
R"w2c_template(  }
)w2c_template"
R"w2c_template(
#define DEFINE_STORE(name, t1, t2)                                     \
)w2c_template"
R"w2c_template(  static inline void name(wasm_rt_memory_t* mem, u64 addr, t2 value) { \
)w2c_template"
R"w2c_template(    MEMCHECK(mem, addr, t1);                                           \
)w2c_template"
R"w2c_template(    t1 wrapped = (t1)value;                                            \
)w2c_template"
R"w2c_template(    wasm_rt_memcpy(&mem->data[addr], &wrapped, sizeof(t1));            \
)w2c_template"
R"w2c_template(  }
)w2c_template"
R"w2c_template(#endif
)w2c_template"
R"w2c_template(
DEFINE_LOAD(i32_load, u32, u32, u32)
)w2c_template"
R"w2c_template(DEFINE_LOAD(i64_load, u64, u64, u64)
)w2c_template"
R"w2c_template(DEFINE_LOAD(f32_load, f32, f32, f32)
)w2c_template"
R"w2c_template(DEFINE_LOAD(f64_load, f64, f64, f64)
)w2c_template"
R"w2c_template(DEFINE_LOAD(i32_load8_s, s8, s32, u32)
)w2c_template"
R"w2c_template(DEFINE_LOAD(i64_load8_s, s8, s64, u64)
)w2c_template"
R"w2c_template(DEFINE_LOAD(i32_load8_u, u8, u32, u32)
)w2c_template"
R"w2c_template(DEFINE_LOAD(i64_load8_u, u8, u64, u64)
)w2c_template"
R"w2c_template(DEFINE_LOAD(i32_load16_s, s16, s32, u32)
)w2c_template"
R"w2c_template(DEFINE_LOAD(i64_load16_s, s16, s64, u64)
)w2c_template"
R"w2c_template(DEFINE_LOAD(i32_load16_u, u16, u32, u32)
)w2c_template"
R"w2c_template(DEFINE_LOAD(i64_load16_u, u16, u64, u64)
)w2c_template"
R"w2c_template(DEFINE_LOAD(i64_load32_s, s32, s64, u64)
)w2c_template"
R"w2c_template(DEFINE_LOAD(i64_load32_u, u32, u64, u64)
)w2c_template"
R"w2c_template(DEFINE_STORE(i32_store, u32, u32)
)w2c_template"
R"w2c_template(DEFINE_STORE(i64_store, u64, u64)
)w2c_template"
R"w2c_template(DEFINE_STORE(f32_store, f32, f32)
)w2c_template"
R"w2c_template(DEFINE_STORE(f64_store, f64, f64)
)w2c_template"
R"w2c_template(DEFINE_STORE(i32_store8, u8, u32)
)w2c_template"
R"w2c_template(DEFINE_STORE(i32_store16, u16, u32)
)w2c_template"
R"w2c_template(DEFINE_STORE(i64_store8, u8, u64)
)w2c_template"
R"w2c_template(DEFINE_STORE(i64_store16, u16, u64)
)w2c_template"
R"w2c_template(DEFINE_STORE(i64_store32, u32, u64)
)w2c_template"
R"w2c_template(
#if defined(_MSC_VER)
)w2c_template"
R"w2c_template(
// Adapted from
)w2c_template"
R"w2c_template(// https://github.com/nemequ/portable-snippets/blob/master/builtin/builtin.h
)w2c_template"
R"w2c_template(
static inline int I64_CLZ(unsigned long long v) {
)w2c_template"
R"w2c_template(  unsigned long r = 0;
)w2c_template"
R"w2c_template(#if defined(_M_AMD64) || defined(_M_ARM)
)w2c_template"
R"w2c_template(  if (_BitScanReverse64(&r, v)) {
)w2c_template"
R"w2c_template(    return 63 - r;
)w2c_template"
R"w2c_template(  }
)w2c_template"
R"w2c_template(#else
)w2c_template"
R"w2c_template(  if (_BitScanReverse(&r, (unsigned long)(v >> 32))) {
)w2c_template"
R"w2c_template(    return 31 - r;
)w2c_template"
R"w2c_template(  } else if (_BitScanReverse(&r, (unsigned long)v)) {
)w2c_template"
R"w2c_template(    return 63 - r;
)w2c_template"
R"w2c_template(  }
)w2c_template"
R"w2c_template(#endif
)w2c_template"
R"w2c_template(  return 64;
)w2c_template"
R"w2c_template(}
)w2c_template"
R"w2c_template(
static inline int I32_CLZ(unsigned long v) {
)w2c_template"
R"w2c_template(  unsigned long r = 0;
)w2c_template"
R"w2c_template(  if (_BitScanReverse(&r, v)) {
)w2c_template"
R"w2c_template(    return 31 - r;
)w2c_template"
R"w2c_template(  }
)w2c_template"
R"w2c_template(  return 32;
)w2c_template"
R"w2c_template(}
)w2c_template"
R"w2c_template(
static inline int I64_CTZ(unsigned long long v) {
)w2c_template"
R"w2c_template(  if (!v) {
)w2c_template"
R"w2c_template(    return 64;
)w2c_template"
R"w2c_template(  }
)w2c_template"
R"w2c_template(  unsigned long r = 0;
)w2c_template"
R"w2c_template(#if defined(_M_AMD64) || defined(_M_ARM)
)w2c_template"
R"w2c_template(  _BitScanForward64(&r, v);
)w2c_template"
R"w2c_template(  return (int)r;
)w2c_template"
R"w2c_template(#else
)w2c_template"
R"w2c_template(  if (_BitScanForward(&r, (unsigned int)(v))) {
)w2c_template"
R"w2c_template(    return (int)(r);
)w2c_template"
R"w2c_template(  }
)w2c_template"
R"w2c_template(
  _BitScanForward(&r, (unsigned int)(v >> 32));
)w2c_template"
R"w2c_template(  return (int)(r + 32);
)w2c_template"
R"w2c_template(#endif
)w2c_template"
R"w2c_template(}
)w2c_template"
R"w2c_template(
static inline int I32_CTZ(unsigned long v) {
)w2c_template"
R"w2c_template(  if (!v) {
)w2c_template"
R"w2c_template(    return 32;
)w2c_template"
R"w2c_template(  }
)w2c_template"
R"w2c_template(  unsigned long r = 0;
)w2c_template"
R"w2c_template(  _BitScanForward(&r, v);
)w2c_template"
R"w2c_template(  return (int)r;
)w2c_template"
R"w2c_template(}
)w2c_template"
R"w2c_template(
#define POPCOUNT_DEFINE_PORTABLE(f_n, T)                            \
)w2c_template"
R"w2c_template(  static inline u32 f_n(T x) {                                      \
)w2c_template"
R"w2c_template(    x = x - ((x >> 1) & (T) ~(T)0 / 3);                             \
)w2c_template"
R"w2c_template(    x = (x & (T) ~(T)0 / 15 * 3) + ((x >> 2) & (T) ~(T)0 / 15 * 3); \
)w2c_template"
R"w2c_template(    x = (x + (x >> 4)) & (T) ~(T)0 / 255 * 15;                      \
)w2c_template"
R"w2c_template(    return (T)(x * ((T) ~(T)0 / 255)) >> (sizeof(T) - 1) * 8;       \
)w2c_template"
R"w2c_template(  }
)w2c_template"
R"w2c_template(
POPCOUNT_DEFINE_PORTABLE(I32_POPCNT, u32)
)w2c_template"
R"w2c_template(POPCOUNT_DEFINE_PORTABLE(I64_POPCNT, u64)
)w2c_template"
R"w2c_template(
#undef POPCOUNT_DEFINE_PORTABLE
)w2c_template"
R"w2c_template(
#else
)w2c_template"
R"w2c_template(
#define I32_CLZ(x) ((x) ? __builtin_clz(x) : 32)
)w2c_template"
R"w2c_template(#define I64_CLZ(x) ((x) ? __builtin_clzll(x) : 64)
)w2c_template"
R"w2c_template(#define I32_CTZ(x) ((x) ? __builtin_ctz(x) : 32)
)w2c_template"
R"w2c_template(#define I64_CTZ(x) ((x) ? __builtin_ctzll(x) : 64)
)w2c_template"
R"w2c_template(#define I32_POPCNT(x) (__builtin_popcount(x))
)w2c_template"
R"w2c_template(#define I64_POPCNT(x) (__builtin_popcountll(x))
)w2c_template"
R"w2c_template(
#endif
)w2c_template"
R"w2c_template(
#define DIV_S(ut, min, x, y)                                      \
)w2c_template"
R"w2c_template(  ((UNLIKELY((y) == 0))                                           \
)w2c_template"
R"w2c_template(       ? TRAP(DIV_BY_ZERO)                                        \
)w2c_template"
R"w2c_template(       : (UNLIKELY((x) == min && (y) == -1)) ? TRAP(INT_OVERFLOW) \
)w2c_template"
R"w2c_template(                                             : (ut)((x) / (y)))
)w2c_template"
R"w2c_template(
#define REM_S(ut, min, x, y) \
)w2c_template"
R"w2c_template(  ((UNLIKELY((y) == 0))      \
)w2c_template"
R"w2c_template(       ? TRAP(DIV_BY_ZERO)   \
)w2c_template"
R"w2c_template(       : (UNLIKELY((x) == min && (y) == -1)) ? 0 : (ut)((x) % (y)))
)w2c_template"
R"w2c_template(
#define I32_DIV_S(x, y) DIV_S(u32, INT32_MIN, (s32)x, (s32)y)
)w2c_template"
R"w2c_template(#define I64_DIV_S(x, y) DIV_S(u64, INT64_MIN, (s64)x, (s64)y)
)w2c_template"
R"w2c_template(#define I32_REM_S(x, y) REM_S(u32, INT32_MIN, (s32)x, (s32)y)
)w2c_template"
R"w2c_template(#define I64_REM_S(x, y) REM_S(u64, INT64_MIN, (s64)x, (s64)y)
)w2c_template"
R"w2c_template(
#define DIVREM_U(op, x, y) \
)w2c_template"
R"w2c_template(  ((UNLIKELY((y) == 0)) ? TRAP(DIV_BY_ZERO) : ((x)op(y)))
)w2c_template"
R"w2c_template(
#define DIV_U(x, y) DIVREM_U(/, x, y)
)w2c_template"
R"w2c_template(#define REM_U(x, y) DIVREM_U(%, x, y)
)w2c_template"
R"w2c_template(
#define ROTL(x, y, mask) \
)w2c_template"
R"w2c_template(  (((x) << ((y) & (mask))) | ((x) >> (((mask) - (y) + 1) & (mask))))
)w2c_template"
R"w2c_template(#define ROTR(x, y, mask) \
)w2c_template"
R"w2c_template(  (((x) >> ((y) & (mask))) | ((x) << (((mask) - (y) + 1) & (mask))))
)w2c_template"
R"w2c_template(
#define I32_ROTL(x, y) ROTL(x, y, 31)
)w2c_template"
R"w2c_template(#define I64_ROTL(x, y) ROTL(x, y, 63)
)w2c_template"
R"w2c_template(#define I32_ROTR(x, y) ROTR(x, y, 31)
)w2c_template"
R"w2c_template(#define I64_ROTR(x, y) ROTR(x, y, 63)
)w2c_template"
R"w2c_template(
#define FMIN(x, y)                                                     \
)w2c_template"
R"w2c_template(  ((UNLIKELY((x) != (x)))                                              \
)w2c_template"
R"w2c_template(       ? NAN                                                           \
)w2c_template"
R"w2c_template(       : (UNLIKELY((y) != (y)))                                        \
)w2c_template"
R"w2c_template(             ? NAN                                                     \
)w2c_template"
R"w2c_template(             : (UNLIKELY((x) == 0 && (y) == 0)) ? (signbit(x) ? x : y) \
)w2c_template"
R"w2c_template(                                                : (x < y) ? x : y)
)w2c_template"
R"w2c_template(
#define FMAX(x, y)                                                     \
)w2c_template"
R"w2c_template(  ((UNLIKELY((x) != (x)))                                              \
)w2c_template"
R"w2c_template(       ? NAN                                                           \
)w2c_template"
R"w2c_template(       : (UNLIKELY((y) != (y)))                                        \
)w2c_template"
R"w2c_template(             ? NAN                                                     \
)w2c_template"
R"w2c_template(             : (UNLIKELY((x) == 0 && (y) == 0)) ? (signbit(x) ? y : x) \
)w2c_template"
R"w2c_template(                                                : (x > y) ? x : y)
)w2c_template"
R"w2c_template(
#define TRUNC_S(ut, st, ft, min, minop, max, x)                           \
)w2c_template"
R"w2c_template(  ((UNLIKELY((x) != (x)))                                                 \
)w2c_template"
R"w2c_template(       ? TRAP(INVALID_CONVERSION)                                         \
)w2c_template"
R"w2c_template(       : (UNLIKELY(!((x)minop(min) && (x) < (max)))) ? TRAP(INT_OVERFLOW) \
)w2c_template"
R"w2c_template(                                                     : (ut)(st)(x))
)w2c_template"
R"w2c_template(
#define I32_TRUNC_S_F32(x) \
)w2c_template"
R"w2c_template(  TRUNC_S(u32, s32, f32, (f32)INT32_MIN, >=, 2147483648.f, x)
)w2c_template"
R"w2c_template(#define I64_TRUNC_S_F32(x) \
)w2c_template"
R"w2c_template(  TRUNC_S(u64, s64, f32, (f32)INT64_MIN, >=, (f32)INT64_MAX, x)
)w2c_template"
R"w2c_template(#define I32_TRUNC_S_F64(x) \
)w2c_template"
R"w2c_template(  TRUNC_S(u32, s32, f64, -2147483649., >, 2147483648., x)
)w2c_template"
R"w2c_template(#define I64_TRUNC_S_F64(x) \
)w2c_template"
R"w2c_template(  TRUNC_S(u64, s64, f64, (f64)INT64_MIN, >=, (f64)INT64_MAX, x)
)w2c_template"
R"w2c_template(
#define TRUNC_U(ut, ft, max, x)                                          \
)w2c_template"
R"w2c_template(  ((UNLIKELY((x) != (x)))                                                \
)w2c_template"
R"w2c_template(       ? TRAP(INVALID_CONVERSION)                                        \
)w2c_template"
R"w2c_template(       : (UNLIKELY(!((x) > (ft)-1 && (x) < (max)))) ? TRAP(INT_OVERFLOW) \
)w2c_template"
R"w2c_template(                                                    : (ut)(x))
)w2c_template"
R"w2c_template(
#define I32_TRUNC_U_F32(x) TRUNC_U(u32, f32, 4294967296.f, x)
)w2c_template"
R"w2c_template(#define I64_TRUNC_U_F32(x) TRUNC_U(u64, f32, (f32)UINT64_MAX, x)
)w2c_template"
R"w2c_template(#define I32_TRUNC_U_F64(x) TRUNC_U(u32, f64, 4294967296., x)
)w2c_template"
R"w2c_template(#define I64_TRUNC_U_F64(x) TRUNC_U(u64, f64, (f64)UINT64_MAX, x)
)w2c_template"
R"w2c_template(
#define TRUNC_SAT_S(ut, st, ft, min, smin, minop, max, smax, x) \
)w2c_template"
R"w2c_template(  ((UNLIKELY((x) != (x)))                                       \
)w2c_template"
R"w2c_template(       ? 0                                                      \
)w2c_template"
R"w2c_template(       : (UNLIKELY(!((x)minop(min))))                           \
)w2c_template"
R"w2c_template(             ? smin                                             \
)w2c_template"
R"w2c_template(             : (UNLIKELY(!((x) < (max)))) ? smax : (ut)(st)(x))
)w2c_template"
R"w2c_template(
#define I32_TRUNC_SAT_S_F32(x)                                            \
)w2c_template"
R"w2c_template(  TRUNC_SAT_S(u32, s32, f32, (f32)INT32_MIN, INT32_MIN, >=, 2147483648.f, \
)w2c_template"
R"w2c_template(              INT32_MAX, x)
)w2c_template"
R"w2c_template(#define I64_TRUNC_SAT_S_F32(x)                                              \
)w2c_template"
R"w2c_template(  TRUNC_SAT_S(u64, s64, f32, (f32)INT64_MIN, INT64_MIN, >=, (f32)INT64_MAX, \
)w2c_template"
R"w2c_template(              INT64_MAX, x)
)w2c_template"
R"w2c_template(#define I32_TRUNC_SAT_S_F64(x)                                        \
)w2c_template"
R"w2c_template(  TRUNC_SAT_S(u32, s32, f64, -2147483649., INT32_MIN, >, 2147483648., \
)w2c_template"
R"w2c_template(              INT32_MAX, x)
)w2c_template"
R"w2c_template(#define I64_TRUNC_SAT_S_F64(x)                                              \
)w2c_template"
R"w2c_template(  TRUNC_SAT_S(u64, s64, f64, (f64)INT64_MIN, INT64_MIN, >=, (f64)INT64_MAX, \
)w2c_template"
R"w2c_template(              INT64_MAX, x)
)w2c_template"
R"w2c_template(
#define TRUNC_SAT_U(ut, ft, max, smax, x)               \
)w2c_template"
R"w2c_template(  ((UNLIKELY((x) != (x))) ? 0                           \
)w2c_template"
R"w2c_template(                          : (UNLIKELY(!((x) > (ft)-1))) \
)w2c_template"
R"w2c_template(                                ? 0                     \
)w2c_template"
R"w2c_template(                                : (UNLIKELY(!((x) < (max)))) ? smax : (ut)(x))
)w2c_template"
R"w2c_template(
#define I32_TRUNC_SAT_U_F32(x) \
)w2c_template"
R"w2c_template(  TRUNC_SAT_U(u32, f32, 4294967296.f, UINT32_MAX, x)
)w2c_template"
R"w2c_template(#define I64_TRUNC_SAT_U_F32(x) \
)w2c_template"
R"w2c_template(  TRUNC_SAT_U(u64, f32, (f32)UINT64_MAX, UINT64_MAX, x)
)w2c_template"
R"w2c_template(#define I32_TRUNC_SAT_U_F64(x) TRUNC_SAT_U(u32, f64, 4294967296., UINT32_MAX, x)
)w2c_template"
R"w2c_template(#define I64_TRUNC_SAT_U_F64(x) \
)w2c_template"
R"w2c_template(  TRUNC_SAT_U(u64, f64, (f64)UINT64_MAX, UINT64_MAX, x)
)w2c_template"
R"w2c_template(
#define DEFINE_REINTERPRET(name, t1, t2) \
)w2c_template"
R"w2c_template(  static inline t2 name(t1 x) {          \
)w2c_template"
R"w2c_template(    t2 result;                           \
)w2c_template"
R"w2c_template(    memcpy(&result, &x, sizeof(result)); \
)w2c_template"
R"w2c_template(    return result;                       \
)w2c_template"
R"w2c_template(  }
)w2c_template"
R"w2c_template(
DEFINE_REINTERPRET(f32_reinterpret_i32, u32, f32)
)w2c_template"
R"w2c_template(DEFINE_REINTERPRET(i32_reinterpret_f32, f32, u32)
)w2c_template"
R"w2c_template(DEFINE_REINTERPRET(f64_reinterpret_i64, u64, f64)
)w2c_template"
R"w2c_template(DEFINE_REINTERPRET(i64_reinterpret_f64, f64, u64)
)w2c_template"
R"w2c_template(
static float quiet_nanf(float x) {
)w2c_template"
R"w2c_template(  uint32_t tmp;
)w2c_template"
R"w2c_template(  memcpy(&tmp, &x, 4);
)w2c_template"
R"w2c_template(  tmp |= 0x7fc00000lu;
)w2c_template"
R"w2c_template(  memcpy(&x, &tmp, 4);
)w2c_template"
R"w2c_template(  return x;
)w2c_template"
R"w2c_template(}
)w2c_template"
R"w2c_template(
static double quiet_nan(double x) {
)w2c_template"
R"w2c_template(  uint64_t tmp;
)w2c_template"
R"w2c_template(  memcpy(&tmp, &x, 8);
)w2c_template"
R"w2c_template(  tmp |= 0x7ff8000000000000llu;
)w2c_template"
R"w2c_template(  memcpy(&x, &tmp, 8);
)w2c_template"
R"w2c_template(  return x;
)w2c_template"
R"w2c_template(}
)w2c_template"
R"w2c_template(
static double wasm_quiet(double x) {
)w2c_template"
R"w2c_template(  if (UNLIKELY(isnan(x))) {
)w2c_template"
R"w2c_template(    return quiet_nan(x);
)w2c_template"
R"w2c_template(  }
)w2c_template"
R"w2c_template(  return x;
)w2c_template"
R"w2c_template(}
)w2c_template"
R"w2c_template(
static float wasm_quietf(float x) {
)w2c_template"
R"w2c_template(  if (UNLIKELY(isnan(x))) {
)w2c_template"
R"w2c_template(    return quiet_nanf(x);
)w2c_template"
R"w2c_template(  }
)w2c_template"
R"w2c_template(  return x;
)w2c_template"
R"w2c_template(}
)w2c_template"
R"w2c_template(
static double wasm_floor(double x) {
)w2c_template"
R"w2c_template(  if (UNLIKELY(isnan(x))) {
)w2c_template"
R"w2c_template(    return quiet_nan(x);
)w2c_template"
R"w2c_template(  }
)w2c_template"
R"w2c_template(  return floor(x);
)w2c_template"
R"w2c_template(}
)w2c_template"
R"w2c_template(
static float wasm_floorf(float x) {
)w2c_template"
R"w2c_template(  if (UNLIKELY(isnan(x))) {
)w2c_template"
R"w2c_template(    return quiet_nanf(x);
)w2c_template"
R"w2c_template(  }
)w2c_template"
R"w2c_template(  return floorf(x);
)w2c_template"
R"w2c_template(}
)w2c_template"
R"w2c_template(
static double wasm_ceil(double x) {
)w2c_template"
R"w2c_template(  if (UNLIKELY(isnan(x))) {
)w2c_template"
R"w2c_template(    return quiet_nan(x);
)w2c_template"
R"w2c_template(  }
)w2c_template"
R"w2c_template(  return ceil(x);
)w2c_template"
R"w2c_template(}
)w2c_template"
R"w2c_template(
static float wasm_ceilf(float x) {
)w2c_template"
R"w2c_template(  if (UNLIKELY(isnan(x))) {
)w2c_template"
R"w2c_template(    return quiet_nanf(x);
)w2c_template"
R"w2c_template(  }
)w2c_template"
R"w2c_template(  return ceilf(x);
)w2c_template"
R"w2c_template(}
)w2c_template"
R"w2c_template(
static double wasm_trunc(double x) {
)w2c_template"
R"w2c_template(  if (UNLIKELY(isnan(x))) {
)w2c_template"
R"w2c_template(    return quiet_nan(x);
)w2c_template"
R"w2c_template(  }
)w2c_template"
R"w2c_template(  return trunc(x);
)w2c_template"
R"w2c_template(}
)w2c_template"
R"w2c_template(
static float wasm_truncf(float x) {
)w2c_template"
R"w2c_template(  if (UNLIKELY(isnan(x))) {
)w2c_template"
R"w2c_template(    return quiet_nanf(x);
)w2c_template"
R"w2c_template(  }
)w2c_template"
R"w2c_template(  return truncf(x);
)w2c_template"
R"w2c_template(}
)w2c_template"
R"w2c_template(
static float wasm_nearbyintf(float x) {
)w2c_template"
R"w2c_template(  if (UNLIKELY(isnan(x))) {
)w2c_template"
R"w2c_template(    return quiet_nanf(x);
)w2c_template"
R"w2c_template(  }
)w2c_template"
R"w2c_template(  return nearbyintf(x);
)w2c_template"
R"w2c_template(}
)w2c_template"
R"w2c_template(
static double wasm_nearbyint(double x) {
)w2c_template"
R"w2c_template(  if (UNLIKELY(isnan(x))) {
)w2c_template"
R"w2c_template(    return quiet_nan(x);
)w2c_template"
R"w2c_template(  }
)w2c_template"
R"w2c_template(  return nearbyint(x);
)w2c_template"
R"w2c_template(}
)w2c_template"
R"w2c_template(
static float wasm_fabsf(float x) {
)w2c_template"
R"w2c_template(  if (UNLIKELY(isnan(x))) {
)w2c_template"
R"w2c_template(    uint32_t tmp;
)w2c_template"
R"w2c_template(    memcpy(&tmp, &x, 4);
)w2c_template"
R"w2c_template(    tmp = tmp & ~(1UL << 31);
)w2c_template"
R"w2c_template(    memcpy(&x, &tmp, 4);
)w2c_template"
R"w2c_template(    return x;
)w2c_template"
R"w2c_template(  }
)w2c_template"
R"w2c_template(  return fabsf(x);
)w2c_template"
R"w2c_template(}
)w2c_template"
R"w2c_template(
static double wasm_fabs(double x) {
)w2c_template"
R"w2c_template(  if (UNLIKELY(isnan(x))) {
)w2c_template"
R"w2c_template(    uint64_t tmp;
)w2c_template"
R"w2c_template(    memcpy(&tmp, &x, 8);
)w2c_template"
R"w2c_template(    tmp = tmp & ~(1ULL << 63);
)w2c_template"
R"w2c_template(    memcpy(&x, &tmp, 8);
)w2c_template"
R"w2c_template(    return x;
)w2c_template"
R"w2c_template(  }
)w2c_template"
R"w2c_template(  return fabs(x);
)w2c_template"
R"w2c_template(}
)w2c_template"
R"w2c_template(
static double wasm_sqrt(double x) {
)w2c_template"
R"w2c_template(  if (UNLIKELY(isnan(x))) {
)w2c_template"
R"w2c_template(    return quiet_nan(x);
)w2c_template"
R"w2c_template(  }
)w2c_template"
R"w2c_template(  return sqrt(x);
)w2c_template"
R"w2c_template(}
)w2c_template"
R"w2c_template(
static float wasm_sqrtf(float x) {
)w2c_template"
R"w2c_template(  if (UNLIKELY(isnan(x))) {
)w2c_template"
R"w2c_template(    return quiet_nanf(x);
)w2c_template"
R"w2c_template(  }
)w2c_template"
R"w2c_template(  return sqrtf(x);
)w2c_template"
R"w2c_template(}
)w2c_template"
R"w2c_template(
static inline void memory_fill(wasm_rt_memory_t* mem, u32 d, u32 val, u32 n) {
)w2c_template"
R"w2c_template(  RANGE_CHECK(mem, d, n);
)w2c_template"
R"w2c_template(  memset(mem->data + d, val, n);
)w2c_template"
R"w2c_template(}
)w2c_template"
R"w2c_template(
static inline void memory_copy(wasm_rt_memory_t* dest,
)w2c_template"
R"w2c_template(                               const wasm_rt_memory_t* src,
)w2c_template"
R"w2c_template(                               u32 dest_addr,
)w2c_template"
R"w2c_template(                               u32 src_addr,
)w2c_template"
R"w2c_template(                               u32 n) {
)w2c_template"
R"w2c_template(  RANGE_CHECK(dest, dest_addr, n);
)w2c_template"
R"w2c_template(  RANGE_CHECK(src, src_addr, n);
)w2c_template"
R"w2c_template(  memmove(dest->data + dest_addr, src->data + src_addr, n);
)w2c_template"
R"w2c_template(}
)w2c_template"
R"w2c_template(
static inline void memory_init(wasm_rt_memory_t* dest,
)w2c_template"
R"w2c_template(                               const u8* src,
)w2c_template"
R"w2c_template(                               u32 src_size,
)w2c_template"
R"w2c_template(                               u32 dest_addr,
)w2c_template"
R"w2c_template(                               u32 src_addr,
)w2c_template"
R"w2c_template(                               u32 n) {
)w2c_template"
R"w2c_template(  if (UNLIKELY(src_addr + (uint64_t)n > src_size))
)w2c_template"
R"w2c_template(    TRAP(OOB);
)w2c_template"
R"w2c_template(  LOAD_DATA((*dest), dest_addr, src + src_addr, n);
)w2c_template"
R"w2c_template(}
)w2c_template"
R"w2c_template(
typedef struct {
)w2c_template"
R"w2c_template(  wasm_rt_func_type_t type;
)w2c_template"
R"w2c_template(  wasm_rt_function_ptr_t func;
)w2c_template"
R"w2c_template(  size_t module_offset;
)w2c_template"
R"w2c_template(} wasm_elem_segment_expr_t;
)w2c_template"
R"w2c_template(
static inline void funcref_table_init(wasm_rt_funcref_table_t* dest,
)w2c_template"
R"w2c_template(                                      const wasm_elem_segment_expr_t* src,
)w2c_template"
R"w2c_template(                                      u32 src_size,
)w2c_template"
R"w2c_template(                                      u32 dest_addr,
)w2c_template"
R"w2c_template(                                      u32 src_addr,
)w2c_template"
R"w2c_template(                                      u32 n,
)w2c_template"
R"w2c_template(                                      void* module_instance) {
)w2c_template"
R"w2c_template(  if (UNLIKELY(src_addr + (uint64_t)n > src_size))
)w2c_template"
R"w2c_template(    TRAP(OOB);
)w2c_template"
R"w2c_template(  if (UNLIKELY(dest_addr + (uint64_t)n > dest->size))
)w2c_template"
R"w2c_template(    TRAP(OOB);
)w2c_template"
R"w2c_template(  for (u32 i = 0; i < n; i++) {
)w2c_template"
R"w2c_template(    const wasm_elem_segment_expr_t* src_expr = &src[src_addr + i];
)w2c_template"
R"w2c_template(    dest->data[dest_addr + i] =
)w2c_template"
R"w2c_template(        (wasm_rt_funcref_t){src_expr->type, src_expr->func,
)w2c_template"
R"w2c_template(                            (char*)module_instance + src_expr->module_offset};
)w2c_template"
R"w2c_template(  }
)w2c_template"
R"w2c_template(}
)w2c_template"
R"w2c_template(
// Currently Wasm only supports initializing externref tables with ref.null.
)w2c_template"
R"w2c_template(static inline void externref_table_init(wasm_rt_externref_table_t* dest,
)w2c_template"
R"w2c_template(                                        u32 src_size,
)w2c_template"
R"w2c_template(                                        u32 dest_addr,
)w2c_template"
R"w2c_template(                                        u32 src_addr,
)w2c_template"
R"w2c_template(                                        u32 n) {
)w2c_template"
R"w2c_template(  if (UNLIKELY(src_addr + (uint64_t)n > src_size))
)w2c_template"
R"w2c_template(    TRAP(OOB);
)w2c_template"
R"w2c_template(  if (UNLIKELY(dest_addr + (uint64_t)n > dest->size))
)w2c_template"
R"w2c_template(    TRAP(OOB);
)w2c_template"
R"w2c_template(  for (u32 i = 0; i < n; i++) {
)w2c_template"
R"w2c_template(    dest->data[dest_addr + i] = wasm_rt_externref_null_value;
)w2c_template"
R"w2c_template(  }
)w2c_template"
R"w2c_template(}
)w2c_template"
R"w2c_template(
#define DEFINE_TABLE_COPY(type)                                              \
)w2c_template"
R"w2c_template(  static inline void type##_table_copy(wasm_rt_##type##_table_t* dest,       \
)w2c_template"
R"w2c_template(                                       const wasm_rt_##type##_table_t* src,  \
)w2c_template"
R"w2c_template(                                       u32 dest_addr, u32 src_addr, u32 n) { \
)w2c_template"
R"w2c_template(    if (UNLIKELY(dest_addr + (uint64_t)n > dest->size))                      \
)w2c_template"
R"w2c_template(      TRAP(OOB);                                                             \
)w2c_template"
R"w2c_template(    if (UNLIKELY(src_addr + (uint64_t)n > src->size))                        \
)w2c_template"
R"w2c_template(      TRAP(OOB);                                                             \
)w2c_template"
R"w2c_template(                                                                             \
)w2c_template"
R"w2c_template(    memmove(dest->data + dest_addr, src->data + src_addr,                    \
)w2c_template"
R"w2c_template(            n * sizeof(wasm_rt_##type##_t));                                 \
)w2c_template"
R"w2c_template(  }
)w2c_template"
R"w2c_template(
DEFINE_TABLE_COPY(funcref)
)w2c_template"
R"w2c_template(DEFINE_TABLE_COPY(externref)
)w2c_template"
R"w2c_template(
#define DEFINE_TABLE_GET(type)                        \
)w2c_template"
R"w2c_template(  static inline wasm_rt_##type##_t type##_table_get(  \
)w2c_template"
R"w2c_template(      const wasm_rt_##type##_table_t* table, u32 i) { \
)w2c_template"
R"w2c_template(    if (UNLIKELY(i >= table->size))                   \
)w2c_template"
R"w2c_template(      TRAP(OOB);                                      \
)w2c_template"
R"w2c_template(    return table->data[i];                            \
)w2c_template"
R"w2c_template(  }
)w2c_template"
R"w2c_template(
DEFINE_TABLE_GET(funcref)
)w2c_template"
R"w2c_template(DEFINE_TABLE_GET(externref)
)w2c_template"
R"w2c_template(
#define DEFINE_TABLE_SET(type)                                               \
)w2c_template"
R"w2c_template(  static inline void type##_table_set(const wasm_rt_##type##_table_t* table, \
)w2c_template"
R"w2c_template(                                      u32 i, const wasm_rt_##type##_t val) { \
)w2c_template"
R"w2c_template(    if (UNLIKELY(i >= table->size))                                          \
)w2c_template"
R"w2c_template(      TRAP(OOB);                                                             \
)w2c_template"
R"w2c_template(    table->data[i] = val;                                                    \
)w2c_template"
R"w2c_template(  }
)w2c_template"
R"w2c_template(
DEFINE_TABLE_SET(funcref)
)w2c_template"
R"w2c_template(DEFINE_TABLE_SET(externref)
)w2c_template"
R"w2c_template(
#define DEFINE_TABLE_FILL(type)                                               \
)w2c_template"
R"w2c_template(  static inline void type##_table_fill(const wasm_rt_##type##_table_t* table, \
)w2c_template"
R"w2c_template(                                       u32 d, const wasm_rt_##type##_t val,   \
)w2c_template"
R"w2c_template(                                       u32 n) {                               \
)w2c_template"
R"w2c_template(    if (UNLIKELY((uint64_t)d + n > table->size))                              \
)w2c_template"
R"w2c_template(      TRAP(OOB);                                                              \
)w2c_template"
R"w2c_template(    for (uint32_t i = d; i < d + n; i++) {                                    \
)w2c_template"
R"w2c_template(      table->data[i] = val;                                                   \
)w2c_template"
R"w2c_template(    }                                                                         \
)w2c_template"
R"w2c_template(  }
)w2c_template"
R"w2c_template(
DEFINE_TABLE_FILL(funcref)
)w2c_template"
R"w2c_template(DEFINE_TABLE_FILL(externref)
)w2c_template"
R"w2c_template(
#if defined(__GNUC__) || defined(__clang__)
)w2c_template"
R"w2c_template(#define FUNC_TYPE_DECL_EXTERN_T(x) extern const char* const x
)w2c_template"
R"w2c_template(#define FUNC_TYPE_EXTERN_T(x) const char* const x
)w2c_template"
R"w2c_template(#define FUNC_TYPE_T(x) static const char* const x
)w2c_template"
R"w2c_template(#else
)w2c_template"
R"w2c_template(#define FUNC_TYPE_DECL_EXTERN_T(x) extern const char x[]
)w2c_template"
R"w2c_template(#define FUNC_TYPE_EXTERN_T(x) const char x[]
)w2c_template"
R"w2c_template(#define FUNC_TYPE_T(x) static const char x[]
)w2c_template"
R"w2c_template(#endif
)w2c_template"
;
