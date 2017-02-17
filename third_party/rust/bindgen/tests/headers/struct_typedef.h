typedef struct {
    _Bool has_name;
} typedef_named_struct;

typedef struct {
    void *no_name;
} *struct_ptr_t, **struct_ptr_ptr_t;

typedef enum {
    ENUM_HAS_NAME=1
} typedef_named_enum;

typedef enum {
    ENUM_IS_ANON
} *enum_ptr_t, **enum_ptr_ptr_t;
