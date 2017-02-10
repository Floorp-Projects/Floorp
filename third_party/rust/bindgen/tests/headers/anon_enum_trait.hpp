
template<typename _Tp>
class DataType {
public:
    typedef _Tp         value_type;
    typedef value_type  work_type;
    typedef value_type  channel_type;
    typedef value_type  vec_type;
    enum { generic_type = 1,
           depth        = -1,
           channels     = 1,
           fmt          = 0,
           type = -1,
         };
};

struct Foo {
  enum {
    Bar = 0,
    Baz = 0,
  };
};
