template<class _CharT, class _Traits, class _Allocator>
class basic_string
{
public:
    typedef unsigned long long size_type;
    typedef char value_type;
    typedef value_type * pointer;

    struct __long
    {
        size_type __cap_;
        size_type __size_;
        pointer   __data_;
    };

    enum {__min_cap = (sizeof(__long) - 1)/sizeof(value_type) > 2 ?
                        (sizeof(__long) - 1)/sizeof(value_type) : 2};

    struct __short
    {
        union
        {
            unsigned char __size_;
            value_type __lx;
        };
        value_type __data_[__min_cap];
    };

    union __ulx{__long __lx; __short __lxx;};

    enum {__n_words = sizeof(__ulx) / sizeof(size_type)};

    struct __raw
    {
        size_type __words[__n_words];
    };

    struct __rep
    {
        union
        {
            __long  __l;
            __short __s;
            __raw   __r;
        };
    };
};