namespace std
{
  template < typename > struct char_traits;
}
namespace __gnu_cxx
{
  template < typename > struct char_traits;
}
namespace std
{
  template < class _CharT > struct char_traits:__gnu_cxx::char_traits <
    _CharT >
  {
  };
}
