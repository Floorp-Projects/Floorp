// bindgen-flags: --no-unstable-rust -- -std=c++11

/**
 * These typedefs are hacky, but keep our tests consistent across 64-bit
 * platforms, otherwise the id's change and our CI is unhappy.
 */
typedef unsigned char uint8_t;
typedef int int32_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;
typedef unsigned long long size_t;
typedef unsigned long long uintptr_t;


#define JS_PUNBOX64
#define IS_LITTLE_ENDIAN

/*
 * Try to get jsvals 64-bit aligned. We could almost assert that all values are
 * aligned, but MSVC and GCC occasionally break alignment.
 */
#if defined(__GNUC__) || defined(__xlc__) || defined(__xlC__)
# define JSVAL_ALIGNMENT        __attribute__((aligned (8)))
#elif defined(_MSC_VER)
  /*
   * Structs can be aligned with MSVC, but not if they are used as parameters,
   * so we just don't try to align.
   */
# define JSVAL_ALIGNMENT
#elif defined(__SUNPRO_C) || defined(__SUNPRO_CC)
# define JSVAL_ALIGNMENT
#elif defined(__HP_cc) || defined(__HP_aCC)
# define JSVAL_ALIGNMENT
#endif

#if defined(JS_PUNBOX64)
# define JSVAL_TAG_SHIFT 47
#endif

/*
 * We try to use enums so that printing a jsval_layout in the debugger shows
 * nice symbolic type tags, however we can only do this when we can force the
 * underlying type of the enum to be the desired size.
 */
#if !defined(__SUNPRO_CC) && !defined(__xlC__)

#if defined(_MSC_VER)
# define JS_ENUM_HEADER(id, type)              enum id : type
# define JS_ENUM_FOOTER(id)
#else
# define JS_ENUM_HEADER(id, type)              enum id
# define JS_ENUM_FOOTER(id)                    __attribute__((packed))
#endif

/* Remember to propagate changes to the C defines below. */
JS_ENUM_HEADER(JSValueType, uint8_t)
{
    JSVAL_TYPE_DOUBLE              = 0x00,
    JSVAL_TYPE_INT32               = 0x01,
    JSVAL_TYPE_UNDEFINED           = 0x02,
    JSVAL_TYPE_BOOLEAN             = 0x03,
    JSVAL_TYPE_MAGIC               = 0x04,
    JSVAL_TYPE_STRING              = 0x05,
    JSVAL_TYPE_SYMBOL              = 0x06,
    JSVAL_TYPE_NULL                = 0x07,
    JSVAL_TYPE_OBJECT              = 0x08,

    /* These never appear in a jsval; they are only provided as an out-of-band value. */
    JSVAL_TYPE_UNKNOWN             = 0x20,
    JSVAL_TYPE_MISSING             = 0x21
} JS_ENUM_FOOTER(JSValueType);

static_assert(sizeof(JSValueType) == 1,
              "compiler typed enum support is apparently buggy");

#if defined(JS_NUNBOX32)

/* Remember to propagate changes to the C defines below. */
JS_ENUM_HEADER(JSValueTag, uint32_t)
{
    JSVAL_TAG_CLEAR                = 0xFFFFFF80,
    JSVAL_TAG_INT32                = JSVAL_TAG_CLEAR | JSVAL_TYPE_INT32,
    JSVAL_TAG_UNDEFINED            = JSVAL_TAG_CLEAR | JSVAL_TYPE_UNDEFINED,
    JSVAL_TAG_STRING               = JSVAL_TAG_CLEAR | JSVAL_TYPE_STRING,
    JSVAL_TAG_SYMBOL               = JSVAL_TAG_CLEAR | JSVAL_TYPE_SYMBOL,
    JSVAL_TAG_BOOLEAN              = JSVAL_TAG_CLEAR | JSVAL_TYPE_BOOLEAN,
    JSVAL_TAG_MAGIC                = JSVAL_TAG_CLEAR | JSVAL_TYPE_MAGIC,
    JSVAL_TAG_NULL                 = JSVAL_TAG_CLEAR | JSVAL_TYPE_NULL,
    JSVAL_TAG_OBJECT               = JSVAL_TAG_CLEAR | JSVAL_TYPE_OBJECT
} JS_ENUM_FOOTER(JSValueTag);

static_assert(sizeof(JSValueTag) == sizeof(uint32_t),
              "compiler typed enum support is apparently buggy");

#elif defined(JS_PUNBOX64)

/* Remember to propagate changes to the C defines below. */
JS_ENUM_HEADER(JSValueTag, uint32_t)
{
    JSVAL_TAG_MAX_DOUBLE           = 0x1FFF0,
    JSVAL_TAG_INT32                = JSVAL_TAG_MAX_DOUBLE | JSVAL_TYPE_INT32,
    JSVAL_TAG_UNDEFINED            = JSVAL_TAG_MAX_DOUBLE | JSVAL_TYPE_UNDEFINED,
    JSVAL_TAG_STRING               = JSVAL_TAG_MAX_DOUBLE | JSVAL_TYPE_STRING,
    JSVAL_TAG_SYMBOL               = JSVAL_TAG_MAX_DOUBLE | JSVAL_TYPE_SYMBOL,
    JSVAL_TAG_BOOLEAN              = JSVAL_TAG_MAX_DOUBLE | JSVAL_TYPE_BOOLEAN,
    JSVAL_TAG_MAGIC                = JSVAL_TAG_MAX_DOUBLE | JSVAL_TYPE_MAGIC,
    JSVAL_TAG_NULL                 = JSVAL_TAG_MAX_DOUBLE | JSVAL_TYPE_NULL,
    JSVAL_TAG_OBJECT               = JSVAL_TAG_MAX_DOUBLE | JSVAL_TYPE_OBJECT
} JS_ENUM_FOOTER(JSValueTag);

static_assert(sizeof(JSValueTag) == sizeof(uint32_t),
              "compiler typed enum support is apparently buggy");

JS_ENUM_HEADER(JSValueShiftedTag, uint64_t)
{
    JSVAL_SHIFTED_TAG_MAX_DOUBLE   = ((((uint64_t)JSVAL_TAG_MAX_DOUBLE) << JSVAL_TAG_SHIFT) | 0xFFFFFFFF),
    JSVAL_SHIFTED_TAG_INT32        = (((uint64_t)JSVAL_TAG_INT32)      << JSVAL_TAG_SHIFT),
    JSVAL_SHIFTED_TAG_UNDEFINED    = (((uint64_t)JSVAL_TAG_UNDEFINED)  << JSVAL_TAG_SHIFT),
    JSVAL_SHIFTED_TAG_STRING       = (((uint64_t)JSVAL_TAG_STRING)     << JSVAL_TAG_SHIFT),
    JSVAL_SHIFTED_TAG_SYMBOL       = (((uint64_t)JSVAL_TAG_SYMBOL)     << JSVAL_TAG_SHIFT),
    JSVAL_SHIFTED_TAG_BOOLEAN      = (((uint64_t)JSVAL_TAG_BOOLEAN)    << JSVAL_TAG_SHIFT),
    JSVAL_SHIFTED_TAG_MAGIC        = (((uint64_t)JSVAL_TAG_MAGIC)      << JSVAL_TAG_SHIFT),
    JSVAL_SHIFTED_TAG_NULL         = (((uint64_t)JSVAL_TAG_NULL)       << JSVAL_TAG_SHIFT),
    JSVAL_SHIFTED_TAG_OBJECT       = (((uint64_t)JSVAL_TAG_OBJECT)     << JSVAL_TAG_SHIFT)
} JS_ENUM_FOOTER(JSValueShiftedTag);

static_assert(sizeof(JSValueShiftedTag) == sizeof(uint64_t),
              "compiler typed enum support is apparently buggy");

#endif

/*
 * All our supported compilers implement C++11 |enum Foo : T| syntax, so don't
 * expose these macros. (This macro exists *only* because gcc bug 51242
 * <https://gcc.gnu.org/bugzilla/show_bug.cgi?id=51242> makes bit-fields of
 * typed enums trigger a warning that can't be turned off. Don't expose it
 * beyond this file!)
 */
#undef JS_ENUM_HEADER
#undef JS_ENUM_FOOTER

#else  /* !defined(__SUNPRO_CC) && !defined(__xlC__) */

typedef uint8_t JSValueType;
#define JSVAL_TYPE_DOUBLE            ((uint8_t)0x00)
#define JSVAL_TYPE_INT32             ((uint8_t)0x01)
#define JSVAL_TYPE_UNDEFINED         ((uint8_t)0x02)
#define JSVAL_TYPE_BOOLEAN           ((uint8_t)0x03)
#define JSVAL_TYPE_MAGIC             ((uint8_t)0x04)
#define JSVAL_TYPE_STRING            ((uint8_t)0x05)
#define JSVAL_TYPE_SYMBOL            ((uint8_t)0x06)
#define JSVAL_TYPE_NULL              ((uint8_t)0x07)
#define JSVAL_TYPE_OBJECT            ((uint8_t)0x08)
#define JSVAL_TYPE_UNKNOWN           ((uint8_t)0x20)

#if defined(JS_NUNBOX32)

typedef uint32_t JSValueTag;
#define JSVAL_TAG_CLEAR              ((uint32_t)(0xFFFFFF80))
#define JSVAL_TAG_INT32              ((uint32_t)(JSVAL_TAG_CLEAR | JSVAL_TYPE_INT32))
#define JSVAL_TAG_UNDEFINED          ((uint32_t)(JSVAL_TAG_CLEAR | JSVAL_TYPE_UNDEFINED))
#define JSVAL_TAG_STRING             ((uint32_t)(JSVAL_TAG_CLEAR | JSVAL_TYPE_STRING))
#define JSVAL_TAG_SYMBOL             ((uint32_t)(JSVAL_TAG_CLEAR | JSVAL_TYPE_SYMBOL))
#define JSVAL_TAG_BOOLEAN            ((uint32_t)(JSVAL_TAG_CLEAR | JSVAL_TYPE_BOOLEAN))
#define JSVAL_TAG_MAGIC              ((uint32_t)(JSVAL_TAG_CLEAR | JSVAL_TYPE_MAGIC))
#define JSVAL_TAG_NULL               ((uint32_t)(JSVAL_TAG_CLEAR | JSVAL_TYPE_NULL))
#define JSVAL_TAG_OBJECT             ((uint32_t)(JSVAL_TAG_CLEAR | JSVAL_TYPE_OBJECT))

#elif defined(JS_PUNBOX64)

typedef uint32_t JSValueTag;
#define JSVAL_TAG_MAX_DOUBLE         ((uint32_t)(0x1FFF0))
#define JSVAL_TAG_INT32              (uint32_t)(JSVAL_TAG_MAX_DOUBLE | JSVAL_TYPE_INT32)
#define JSVAL_TAG_UNDEFINED          (uint32_t)(JSVAL_TAG_MAX_DOUBLE | JSVAL_TYPE_UNDEFINED)
#define JSVAL_TAG_STRING             (uint32_t)(JSVAL_TAG_MAX_DOUBLE | JSVAL_TYPE_STRING)
#define JSVAL_TAG_SYMBOL             (uint32_t)(JSVAL_TAG_MAX_DOUBLE | JSVAL_TYPE_SYMBOL)
#define JSVAL_TAG_BOOLEAN            (uint32_t)(JSVAL_TAG_MAX_DOUBLE | JSVAL_TYPE_BOOLEAN)
#define JSVAL_TAG_MAGIC              (uint32_t)(JSVAL_TAG_MAX_DOUBLE | JSVAL_TYPE_MAGIC)
#define JSVAL_TAG_NULL               (uint32_t)(JSVAL_TAG_MAX_DOUBLE | JSVAL_TYPE_NULL)
#define JSVAL_TAG_OBJECT             (uint32_t)(JSVAL_TAG_MAX_DOUBLE | JSVAL_TYPE_OBJECT)

typedef uint64_t JSValueShiftedTag;
#define JSVAL_SHIFTED_TAG_MAX_DOUBLE ((((uint64_t)JSVAL_TAG_MAX_DOUBLE) << JSVAL_TAG_SHIFT) | 0xFFFFFFFF)
#define JSVAL_SHIFTED_TAG_INT32      (((uint64_t)JSVAL_TAG_INT32)      << JSVAL_TAG_SHIFT)
#define JSVAL_SHIFTED_TAG_UNDEFINED  (((uint64_t)JSVAL_TAG_UNDEFINED)  << JSVAL_TAG_SHIFT)
#define JSVAL_SHIFTED_TAG_STRING     (((uint64_t)JSVAL_TAG_STRING)     << JSVAL_TAG_SHIFT)
#define JSVAL_SHIFTED_TAG_SYMBOL     (((uint64_t)JSVAL_TAG_SYMBOL)     << JSVAL_TAG_SHIFT)
#define JSVAL_SHIFTED_TAG_BOOLEAN    (((uint64_t)JSVAL_TAG_BOOLEAN)    << JSVAL_TAG_SHIFT)
#define JSVAL_SHIFTED_TAG_MAGIC      (((uint64_t)JSVAL_TAG_MAGIC)      << JSVAL_TAG_SHIFT)
#define JSVAL_SHIFTED_TAG_NULL       (((uint64_t)JSVAL_TAG_NULL)       << JSVAL_TAG_SHIFT)
#define JSVAL_SHIFTED_TAG_OBJECT     (((uint64_t)JSVAL_TAG_OBJECT)     << JSVAL_TAG_SHIFT)

#endif  /* JS_PUNBOX64 */
#endif  /* !defined(__SUNPRO_CC) && !defined(__xlC__) */

#if defined(JS_NUNBOX32)

#define JSVAL_TYPE_TO_TAG(type)      ((JSValueTag)(JSVAL_TAG_CLEAR | (type)))

#define JSVAL_LOWER_INCL_TAG_OF_OBJ_OR_NULL_SET         JSVAL_TAG_NULL
#define JSVAL_UPPER_EXCL_TAG_OF_PRIMITIVE_SET           JSVAL_TAG_OBJECT
#define JSVAL_UPPER_INCL_TAG_OF_NUMBER_SET              JSVAL_TAG_INT32
#define JSVAL_LOWER_INCL_TAG_OF_GCTHING_SET             JSVAL_TAG_STRING

#elif defined(JS_PUNBOX64)

#define JSVAL_PAYLOAD_MASK           0x00007FFFFFFFFFFFLL
#define JSVAL_TAG_MASK               0xFFFF800000000000LL
#define JSVAL_TYPE_TO_TAG(type)      ((JSValueTag)(JSVAL_TAG_MAX_DOUBLE | (type)))
#define JSVAL_TYPE_TO_SHIFTED_TAG(type) (((uint64_t)JSVAL_TYPE_TO_TAG(type)) << JSVAL_TAG_SHIFT)

#define JSVAL_LOWER_INCL_TAG_OF_OBJ_OR_NULL_SET         JSVAL_TAG_NULL
#define JSVAL_UPPER_EXCL_TAG_OF_PRIMITIVE_SET           JSVAL_TAG_OBJECT
#define JSVAL_UPPER_INCL_TAG_OF_NUMBER_SET              JSVAL_TAG_INT32
#define JSVAL_LOWER_INCL_TAG_OF_GCTHING_SET             JSVAL_TAG_STRING

#define JSVAL_LOWER_INCL_SHIFTED_TAG_OF_OBJ_OR_NULL_SET  JSVAL_SHIFTED_TAG_NULL
#define JSVAL_UPPER_EXCL_SHIFTED_TAG_OF_PRIMITIVE_SET    JSVAL_SHIFTED_TAG_OBJECT
#define JSVAL_UPPER_EXCL_SHIFTED_TAG_OF_NUMBER_SET       JSVAL_SHIFTED_TAG_UNDEFINED
#define JSVAL_LOWER_INCL_SHIFTED_TAG_OF_GCTHING_SET      JSVAL_SHIFTED_TAG_STRING

#endif /* JS_PUNBOX64 */

typedef enum JSWhyMagic
{
    /** a hole in a native object's elements */
    JS_ELEMENTS_HOLE,

    /** there is not a pending iterator value */
    JS_NO_ITER_VALUE,

    /** exception value thrown when closing a generator */
    JS_GENERATOR_CLOSING,

    /** compiler sentinel value */
    JS_NO_CONSTANT,

    /** used in debug builds to catch tracing errors */
    JS_THIS_POISON,

    /** used in debug builds to catch tracing errors */
    JS_ARG_POISON,

    /** an empty subnode in the AST serializer */
    JS_SERIALIZE_NO_NODE,

    /** lazy arguments value on the stack */
    JS_LAZY_ARGUMENTS,

    /** optimized-away 'arguments' value */
    JS_OPTIMIZED_ARGUMENTS,

    /** magic value passed to natives to indicate construction */
    JS_IS_CONSTRUCTING,

    /** arguments.callee has been overwritten */
    JS_OVERWRITTEN_CALLEE,

    /** value of static block object slot */
    JS_BLOCK_NEEDS_CLONE,

    /** see class js::HashableValue */
    JS_HASH_KEY_EMPTY,

    /** error while running Ion code */
    JS_ION_ERROR,

    /** missing recover instruction result */
    JS_ION_BAILOUT,

    /** optimized out slot */
    JS_OPTIMIZED_OUT,

    /** uninitialized lexical bindings that produce ReferenceError on touch. */
    JS_UNINITIALIZED_LEXICAL,

    /** for local use */
    JS_GENERIC_MAGIC,

    JS_WHY_MAGIC_COUNT
} JSWhyMagic;

#if defined(IS_LITTLE_ENDIAN)
# if defined(JS_NUNBOX32)
typedef union jsval_layout
{
    uint64_t asBits;
    struct {
        union {
            int32_t        i32;
            uint32_t       u32;
            uint32_t       boo;     // Don't use |bool| -- it must be four bytes.
            JSString*      str;
            JS::Symbol*    sym;
            JSObject*      obj;
            js::gc::Cell*  cell;
            void*          ptr;
            JSWhyMagic     why;
            size_t         word;
            uintptr_t      uintptr;
        } payload;
        JSValueTag tag;
    } s;
    double asDouble;
    void* asPtr;
} JSVAL_ALIGNMENT jsval_layout;
# elif defined(JS_PUNBOX64)
typedef union jsval_layout
{
    uint64_t asBits;
#if !defined(_WIN64)
    /* MSVC does not pack these correctly :-( */
    struct {
        uint64_t           payload47 : 47;
        JSValueTag         tag : 17;
    } debugView;
#endif
    struct {
        union {
            int32_t        i32;
            uint32_t       u32;
            JSWhyMagic     why;
        } payload;
    } s;
    double asDouble;
    void* asPtr;
    size_t asWord;
    uintptr_t asUIntPtr;
} JSVAL_ALIGNMENT jsval_layout;
# endif  /* JS_PUNBOX64 */
#else   /* defined(IS_LITTLE_ENDIAN) */
# if defined(JS_NUNBOX32)
typedef union jsval_layout
{
    uint64_t asBits;
    struct {
        JSValueTag tag;
        union {
            int32_t        i32;
            uint32_t       u32;
            uint32_t       boo;     // Don't use |bool| -- it must be four bytes.
            JSString*      str;
            JS::Symbol*    sym;
            JSObject*      obj;
            js::gc::Cell*  cell;
            void*          ptr;
            JSWhyMagic     why;
            size_t         word;
            uintptr_t      uintptr;
        } payload;
    } s;
    double asDouble;
    void* asPtr;
} JSVAL_ALIGNMENT jsval_layout;
# elif defined(JS_PUNBOX64)
typedef union jsval_layout
{
    uint64_t asBits;
    struct {
        JSValueTag         tag : 17;
        uint64_t           payload47 : 47;
    } debugView;
    struct {
        uint32_t           padding;
        union {
            int32_t        i32;
            uint32_t       u32;
            JSWhyMagic     why;
        } payload;
    } s;
    double asDouble;
    void* asPtr;
    size_t asWord;
    uintptr_t asUIntPtr;
} JSVAL_ALIGNMENT jsval_layout;
# endif /* JS_PUNBOX64 */
#endif  /* defined(IS_LITTLE_ENDIAN) */

/*
 * For codesize purposes on some platforms, it's important that the
 * compiler know that JS::Values constructed from constant values can be
 * folded to constant bit patterns at compile time, rather than
 * constructed at runtime.  Doing this requires a fair amount of C++11
 * features, which are not supported on all of our compilers.  Set up
 * some defines and helper macros in an attempt to confine the ugliness
 * here, rather than scattering it all about the file.  The important
 * features are:
 *
 * - constexpr;
 * - defaulted functions;
 * - C99-style designated initializers.
 */
#if defined(__clang__)
#  if __has_feature(cxx_constexpr) && __has_feature(cxx_defaulted_functions)
#    define JS_VALUE_IS_CONSTEXPR
#  endif
#elif defined(__GNUC__)
/*
 * We need 4.5 for defaulted functions, 4.6 for constexpr, 4.7 because 4.6
 * doesn't understand |(X) { .field = ... }| syntax, and 4.7.3 because
 * versions prior to that have bugs in the C++ front-end that cause crashes.
 */
#  if MOZ_GCC_VERSION_AT_LEAST(4, 7, 3)
#    define JS_VALUE_IS_CONSTEXPR
#  endif
#endif

#if defined(JS_VALUE_IS_CONSTEXPR)
#  define JS_RETURN_LAYOUT_FROM_BITS(BITS) \
    return (jsval_layout) { .asBits = (BITS) }
#  define JS_VALUE_CONSTEXPR MOZ_CONSTEXPR
#  define JS_VALUE_CONSTEXPR_VAR MOZ_CONSTEXPR_VAR
#else
#  define JS_RETURN_LAYOUT_FROM_BITS(BITS) \
    jsval_layout l;                        \
    l.asBits = (BITS);                     \
    return l;
#  define JS_VALUE_CONSTEXPR
#  define JS_VALUE_CONSTEXPR_VAR const
#endif

struct Value {
    jsval_layout data;
};
