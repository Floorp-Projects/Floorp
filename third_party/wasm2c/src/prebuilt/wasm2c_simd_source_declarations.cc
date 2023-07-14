const char* s_simd_source_declarations = R"w2c_template(#ifdef __x86_64__
)w2c_template"
R"w2c_template(#define SIMD_FORCE_READ(var) wasm_asm("" ::"x"(var));
)w2c_template"
R"w2c_template(#else
)w2c_template"
R"w2c_template(#define SIMD_FORCE_READ(var)
)w2c_template"
R"w2c_template(#endif
)w2c_template"
R"w2c_template(// TODO: equivalent constraint for ARM and other architectures
)w2c_template"
R"w2c_template(
#define DEFINE_SIMD_LOAD_FUNC(name, func, t)                 \
)w2c_template"
R"w2c_template(  static inline v128 name(wasm_rt_memory_t* mem, u64 addr) { \
)w2c_template"
R"w2c_template(    MEMCHECK(mem, addr, t);                                  \
)w2c_template"
R"w2c_template(    v128 result = func((v128*)&mem->data[addr]);             \
)w2c_template"
R"w2c_template(    SIMD_FORCE_READ(result);                                 \
)w2c_template"
R"w2c_template(    return result;                                           \
)w2c_template"
R"w2c_template(  }
)w2c_template"
R"w2c_template(
#define DEFINE_SIMD_LOAD_LANE(name, func, t, lane)                     \
)w2c_template"
R"w2c_template(  static inline v128 name(wasm_rt_memory_t* mem, u64 addr, v128 vec) { \
)w2c_template"
R"w2c_template(    MEMCHECK(mem, addr, t);                                            \
)w2c_template"
R"w2c_template(    v128 result = func((v128*)&mem->data[addr], vec, lane);            \
)w2c_template"
R"w2c_template(    SIMD_FORCE_READ(result);                                           \
)w2c_template"
R"w2c_template(    return result;                                                     \
)w2c_template"
R"w2c_template(  }
)w2c_template"
R"w2c_template(
#define DEFINE_SIMD_STORE(name, t)                                       \
)w2c_template"
R"w2c_template(  static inline void name(wasm_rt_memory_t* mem, u64 addr, v128 value) { \
)w2c_template"
R"w2c_template(    MEMCHECK(mem, addr, t);                                              \
)w2c_template"
R"w2c_template(    simde_wasm_v128_store((v128*)&mem->data[addr], value);               \
)w2c_template"
R"w2c_template(  }
)w2c_template"
R"w2c_template(
#define DEFINE_SIMD_STORE_LANE(name, func, t, lane)                      \
)w2c_template"
R"w2c_template(  static inline void name(wasm_rt_memory_t* mem, u64 addr, v128 value) { \
)w2c_template"
R"w2c_template(    MEMCHECK(mem, addr, t);                                              \
)w2c_template"
R"w2c_template(    func((v128*)&mem->data[addr], value, lane);                          \
)w2c_template"
R"w2c_template(  }
)w2c_template"
R"w2c_template(
// clang-format off
)w2c_template"
R"w2c_template(DEFINE_SIMD_LOAD_FUNC(v128_load, simde_wasm_v128_load, v128)
)w2c_template"
R"w2c_template(
DEFINE_SIMD_LOAD_FUNC(v128_load8_splat, simde_wasm_v128_load8_splat, u8)
)w2c_template"
R"w2c_template(DEFINE_SIMD_LOAD_FUNC(v128_load16_splat, simde_wasm_v128_load16_splat, u16)
)w2c_template"
R"w2c_template(DEFINE_SIMD_LOAD_FUNC(v128_load32_splat, simde_wasm_v128_load32_splat, u32)
)w2c_template"
R"w2c_template(DEFINE_SIMD_LOAD_FUNC(v128_load64_splat, simde_wasm_v128_load64_splat, u64)
)w2c_template"
R"w2c_template(
DEFINE_SIMD_LOAD_FUNC(i16x8_load8x8, simde_wasm_i16x8_load8x8, u64)
)w2c_template"
R"w2c_template(DEFINE_SIMD_LOAD_FUNC(u16x8_load8x8, simde_wasm_u16x8_load8x8, u64)
)w2c_template"
R"w2c_template(DEFINE_SIMD_LOAD_FUNC(i32x4_load16x4, simde_wasm_i32x4_load16x4, u64)
)w2c_template"
R"w2c_template(DEFINE_SIMD_LOAD_FUNC(u32x4_load16x4, simde_wasm_u32x4_load16x4, u64)
)w2c_template"
R"w2c_template(DEFINE_SIMD_LOAD_FUNC(i64x2_load32x2, simde_wasm_i64x2_load32x2, u64)
)w2c_template"
R"w2c_template(DEFINE_SIMD_LOAD_FUNC(u64x2_load32x2, simde_wasm_u64x2_load32x2, u64)
)w2c_template"
R"w2c_template(
DEFINE_SIMD_LOAD_FUNC(v128_load32_zero, simde_wasm_v128_load32_zero, u32)
)w2c_template"
R"w2c_template(DEFINE_SIMD_LOAD_FUNC(v128_load64_zero, simde_wasm_v128_load64_zero, u64)
)w2c_template"
R"w2c_template(
DEFINE_SIMD_LOAD_LANE(v128_load8_lane0, simde_wasm_v128_load8_lane, u8, 0)
)w2c_template"
R"w2c_template(DEFINE_SIMD_LOAD_LANE(v128_load8_lane1, simde_wasm_v128_load8_lane, u8, 1)
)w2c_template"
R"w2c_template(DEFINE_SIMD_LOAD_LANE(v128_load8_lane2, simde_wasm_v128_load8_lane, u8, 2)
)w2c_template"
R"w2c_template(DEFINE_SIMD_LOAD_LANE(v128_load8_lane3, simde_wasm_v128_load8_lane, u8, 3)
)w2c_template"
R"w2c_template(DEFINE_SIMD_LOAD_LANE(v128_load8_lane4, simde_wasm_v128_load8_lane, u8, 4)
)w2c_template"
R"w2c_template(DEFINE_SIMD_LOAD_LANE(v128_load8_lane5, simde_wasm_v128_load8_lane, u8, 5)
)w2c_template"
R"w2c_template(DEFINE_SIMD_LOAD_LANE(v128_load8_lane6, simde_wasm_v128_load8_lane, u8, 6)
)w2c_template"
R"w2c_template(DEFINE_SIMD_LOAD_LANE(v128_load8_lane7, simde_wasm_v128_load8_lane, u8, 7)
)w2c_template"
R"w2c_template(DEFINE_SIMD_LOAD_LANE(v128_load8_lane8, simde_wasm_v128_load8_lane, u8, 8)
)w2c_template"
R"w2c_template(DEFINE_SIMD_LOAD_LANE(v128_load8_lane9, simde_wasm_v128_load8_lane, u8, 9)
)w2c_template"
R"w2c_template(DEFINE_SIMD_LOAD_LANE(v128_load8_lane10, simde_wasm_v128_load8_lane, u8, 10)
)w2c_template"
R"w2c_template(DEFINE_SIMD_LOAD_LANE(v128_load8_lane11, simde_wasm_v128_load8_lane, u8, 11)
)w2c_template"
R"w2c_template(DEFINE_SIMD_LOAD_LANE(v128_load8_lane12, simde_wasm_v128_load8_lane, u8, 12)
)w2c_template"
R"w2c_template(DEFINE_SIMD_LOAD_LANE(v128_load8_lane13, simde_wasm_v128_load8_lane, u8, 13)
)w2c_template"
R"w2c_template(DEFINE_SIMD_LOAD_LANE(v128_load8_lane14, simde_wasm_v128_load8_lane, u8, 14)
)w2c_template"
R"w2c_template(DEFINE_SIMD_LOAD_LANE(v128_load8_lane15, simde_wasm_v128_load8_lane, u8, 15)
)w2c_template"
R"w2c_template(DEFINE_SIMD_LOAD_LANE(v128_load16_lane0, simde_wasm_v128_load16_lane, u16, 0)
)w2c_template"
R"w2c_template(DEFINE_SIMD_LOAD_LANE(v128_load16_lane1, simde_wasm_v128_load16_lane, u16, 1)
)w2c_template"
R"w2c_template(DEFINE_SIMD_LOAD_LANE(v128_load16_lane2, simde_wasm_v128_load16_lane, u16, 2)
)w2c_template"
R"w2c_template(DEFINE_SIMD_LOAD_LANE(v128_load16_lane3, simde_wasm_v128_load16_lane, u16, 3)
)w2c_template"
R"w2c_template(DEFINE_SIMD_LOAD_LANE(v128_load16_lane4, simde_wasm_v128_load16_lane, u16, 4)
)w2c_template"
R"w2c_template(DEFINE_SIMD_LOAD_LANE(v128_load16_lane5, simde_wasm_v128_load16_lane, u16, 5)
)w2c_template"
R"w2c_template(DEFINE_SIMD_LOAD_LANE(v128_load16_lane6, simde_wasm_v128_load16_lane, u16, 6)
)w2c_template"
R"w2c_template(DEFINE_SIMD_LOAD_LANE(v128_load16_lane7, simde_wasm_v128_load16_lane, u16, 7)
)w2c_template"
R"w2c_template(DEFINE_SIMD_LOAD_LANE(v128_load32_lane0, simde_wasm_v128_load32_lane, u32, 0)
)w2c_template"
R"w2c_template(DEFINE_SIMD_LOAD_LANE(v128_load32_lane1, simde_wasm_v128_load32_lane, u32, 1)
)w2c_template"
R"w2c_template(DEFINE_SIMD_LOAD_LANE(v128_load32_lane2, simde_wasm_v128_load32_lane, u32, 2)
)w2c_template"
R"w2c_template(DEFINE_SIMD_LOAD_LANE(v128_load32_lane3, simde_wasm_v128_load32_lane, u32, 3)
)w2c_template"
R"w2c_template(DEFINE_SIMD_LOAD_LANE(v128_load64_lane0, simde_wasm_v128_load64_lane, u64, 0)
)w2c_template"
R"w2c_template(DEFINE_SIMD_LOAD_LANE(v128_load64_lane1, simde_wasm_v128_load64_lane, u64, 1)
)w2c_template"
R"w2c_template(
DEFINE_SIMD_STORE(v128_store, v128)
)w2c_template"
R"w2c_template(
DEFINE_SIMD_STORE_LANE(v128_store8_lane0, simde_wasm_v128_store8_lane, u8, 0)
)w2c_template"
R"w2c_template(DEFINE_SIMD_STORE_LANE(v128_store8_lane1, simde_wasm_v128_store8_lane, u8, 1)
)w2c_template"
R"w2c_template(DEFINE_SIMD_STORE_LANE(v128_store8_lane2, simde_wasm_v128_store8_lane, u8, 2)
)w2c_template"
R"w2c_template(DEFINE_SIMD_STORE_LANE(v128_store8_lane3, simde_wasm_v128_store8_lane, u8, 3)
)w2c_template"
R"w2c_template(DEFINE_SIMD_STORE_LANE(v128_store8_lane4, simde_wasm_v128_store8_lane, u8, 4)
)w2c_template"
R"w2c_template(DEFINE_SIMD_STORE_LANE(v128_store8_lane5, simde_wasm_v128_store8_lane, u8, 5)
)w2c_template"
R"w2c_template(DEFINE_SIMD_STORE_LANE(v128_store8_lane6, simde_wasm_v128_store8_lane, u8, 6)
)w2c_template"
R"w2c_template(DEFINE_SIMD_STORE_LANE(v128_store8_lane7, simde_wasm_v128_store8_lane, u8, 7)
)w2c_template"
R"w2c_template(DEFINE_SIMD_STORE_LANE(v128_store8_lane8, simde_wasm_v128_store8_lane, u8, 8)
)w2c_template"
R"w2c_template(DEFINE_SIMD_STORE_LANE(v128_store8_lane9, simde_wasm_v128_store8_lane, u8, 9)
)w2c_template"
R"w2c_template(DEFINE_SIMD_STORE_LANE(v128_store8_lane10, simde_wasm_v128_store8_lane, u8, 10)
)w2c_template"
R"w2c_template(DEFINE_SIMD_STORE_LANE(v128_store8_lane11, simde_wasm_v128_store8_lane, u8, 11)
)w2c_template"
R"w2c_template(DEFINE_SIMD_STORE_LANE(v128_store8_lane12, simde_wasm_v128_store8_lane, u8, 12)
)w2c_template"
R"w2c_template(DEFINE_SIMD_STORE_LANE(v128_store8_lane13, simde_wasm_v128_store8_lane, u8, 13)
)w2c_template"
R"w2c_template(DEFINE_SIMD_STORE_LANE(v128_store8_lane14, simde_wasm_v128_store8_lane, u8, 14)
)w2c_template"
R"w2c_template(DEFINE_SIMD_STORE_LANE(v128_store8_lane15, simde_wasm_v128_store8_lane, u8, 15)
)w2c_template"
R"w2c_template(DEFINE_SIMD_STORE_LANE(v128_store16_lane0, simde_wasm_v128_store16_lane, u16, 0)
)w2c_template"
R"w2c_template(DEFINE_SIMD_STORE_LANE(v128_store16_lane1, simde_wasm_v128_store16_lane, u16, 1)
)w2c_template"
R"w2c_template(DEFINE_SIMD_STORE_LANE(v128_store16_lane2, simde_wasm_v128_store16_lane, u16, 2)
)w2c_template"
R"w2c_template(DEFINE_SIMD_STORE_LANE(v128_store16_lane3, simde_wasm_v128_store16_lane, u16, 3)
)w2c_template"
R"w2c_template(DEFINE_SIMD_STORE_LANE(v128_store16_lane4, simde_wasm_v128_store16_lane, u16, 4)
)w2c_template"
R"w2c_template(DEFINE_SIMD_STORE_LANE(v128_store16_lane5, simde_wasm_v128_store16_lane, u16, 5)
)w2c_template"
R"w2c_template(DEFINE_SIMD_STORE_LANE(v128_store16_lane6, simde_wasm_v128_store16_lane, u16, 6)
)w2c_template"
R"w2c_template(DEFINE_SIMD_STORE_LANE(v128_store16_lane7, simde_wasm_v128_store16_lane, u16, 7)
)w2c_template"
R"w2c_template(DEFINE_SIMD_STORE_LANE(v128_store32_lane0, simde_wasm_v128_store32_lane, u32, 0)
)w2c_template"
R"w2c_template(DEFINE_SIMD_STORE_LANE(v128_store32_lane1, simde_wasm_v128_store32_lane, u32, 1)
)w2c_template"
R"w2c_template(DEFINE_SIMD_STORE_LANE(v128_store32_lane2, simde_wasm_v128_store32_lane, u32, 2)
)w2c_template"
R"w2c_template(DEFINE_SIMD_STORE_LANE(v128_store32_lane3, simde_wasm_v128_store32_lane, u32, 3)
)w2c_template"
R"w2c_template(DEFINE_SIMD_STORE_LANE(v128_store64_lane0, simde_wasm_v128_store64_lane, u64, 0)
)w2c_template"
R"w2c_template(DEFINE_SIMD_STORE_LANE(v128_store64_lane1, simde_wasm_v128_store64_lane, u64, 1)
)w2c_template"
R"w2c_template(// clang-format on
)w2c_template"
;
