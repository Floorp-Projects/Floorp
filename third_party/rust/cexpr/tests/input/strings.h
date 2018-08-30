#define Str_ ""
#define Str_str "str"
#define Str_unicode u"unicode"
#define Str_long L"long"
#define Str_concat u"con" L"cat"
#define Str_concat_parens ("concat" U"_parens")
#define Str_concat_identifier (Str_concat L"_identifier")
#define Fn_Str_no_args() "no_args"
#define Fn_Str_no_args_concat() "no_args_" Str_concat
#define Fn_Str_prepend_arg(arg) "prepend_" arg
#define Fn_Str_two_args(two, args) two "_" args
#define Fn_Str_three_args(three, _, args) three _ args
