namespace std {
  template <typename CharT>
  class basic_ostream {
    public:
    template <typename T>
    basic_ostream& operator<<(T);
    basic_ostream& operator<<(basic_ostream<CharT>& (*)(basic_ostream<CharT>&));
  };

  template <typename CharT>
  class basic_iostream : public basic_ostream<CharT> {};

  using ostream = basic_ostream<char>;
  using wostream = basic_ostream<wchar_t>;

  using iostream = basic_iostream<char>;
  using wiostream = basic_iostream<wchar_t>;

  ostream cout;
  wostream wcout;

  ostream cerr;
  wostream wcerr;

  ostream clog;
  wostream wclog;

  template<typename CharT>
  basic_ostream<CharT>& endl(basic_ostream<CharT>&);
} // namespace std

int main() {
  std::cout << "Hello" << std::endl;
}
