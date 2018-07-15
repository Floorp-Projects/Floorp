// Proxy file in order to define generic data types, to avoid binding with system headers

namespace std {

typedef unsigned long size_t;

template <class T>
class vector {
 public:
  typedef T* iterator;
  typedef const T* const_iterator;
  typedef T& reference;
  typedef const T& const_reference;
  typedef size_t size_type;

  explicit vector();
  explicit vector(size_type n);

  void swap(vector &other);
  void push_back(const T& val);

  template <class... Args> void emplace_back(Args &&... args);

  void reserve(size_t n);
  void resize(size_t n);

  size_t size();
  const_reference operator[] (size_type) const;
  reference operator[] (size_type);

  const_iterator begin() const;
  const_iterator end() const;
};

template <typename T>
class basic_string {
public:
  typedef basic_string<T> _Type;
  basic_string() {}
  basic_string(const T *p);
  basic_string(const T *p, size_t count);
  basic_string(size_t count, char ch);
  ~basic_string() {}
  size_t size() const;
  bool empty() const;
  size_t find (const char* s, size_t pos = 0) const;
  const T *c_str() const;
  _Type& assign(const T *s);
  basic_string<T> *operator+=(const basic_string<T> &) {}
  friend basic_string<T> operator+(const basic_string<T> &, const basic_string<T> &) {}
};
typedef basic_string<char> string;
typedef basic_string<wchar_t> wstring;

template <typename T>
struct default_delete {};

template <typename T, typename D = default_delete<T>>
class unique_ptr {
 public:
  unique_ptr();
  ~unique_ptr();
  explicit unique_ptr(T*);
  template <typename U, typename E>
  unique_ptr(unique_ptr<U, E>&&);
  T* release();
};

template <class Fp, class... Arguments>
class bind_rt {};

template <class Fp, class... Arguments>
bind_rt<Fp, Arguments...> bind(Fp &&, Arguments &&...);
}

typedef unsigned int uid_t;
typedef unsigned int pid_t;

int getpw(uid_t uid, char *buf);
int setuid(uid_t uid);

int mkstemp(char *tmpl);
char *mktemp(char *tmpl);

pid_t vfork(void);

int abort() { return 0; }

#define assert(x)                                                              \
  if (!(x))                                                                    \
  (void)abort()

std::size_t strlen(const char *s);
char *strncat(char *s1, const char *s2, std::size_t n);

void free(void *ptr);
void *malloc(std::size_t size);

void *memset(void *b, int c, std::size_t len);
