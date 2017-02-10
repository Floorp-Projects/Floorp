// bindgen-flags: -- -std=c++11

struct false_type {};

template<typename _From, typename _To, bool>
struct __is_base_to_derived_ref;

template<typename _From, typename _To>
struct __is_base_to_derived_ref<_From, _To, true>
{
  typedef _To type;

  static constexpr bool value = type::value;
};

template<typename _From, typename _To>
struct __is_base_to_derived_ref<_From, _To, false>
: public false_type
{ };
