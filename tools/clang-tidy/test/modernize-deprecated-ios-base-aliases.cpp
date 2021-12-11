namespace std {
class ios_base {
public:
  typedef int io_state;
  typedef int open_mode;
  typedef int seek_dir;

  typedef int streampos;
  typedef int streamoff;
};

template <class CharT>
class basic_ios : public ios_base {
};
} // namespace std

// Test function return values (declaration)
std::ios_base::io_state f_5();
