namespace std {
template <typename T> struct less {
  bool operator()(const T &lhs, const T &rhs) { return lhs < rhs; }
};

template <typename T> struct greater {
  bool operator()(const T &lhs, const T &rhs) { return lhs > rhs; }
};

struct iterator_type {};

template <typename K, typename Cmp = less<K>> struct set {
  typedef iterator_type iterator;
  iterator find(const K &k);
  unsigned count(const K &k);

  iterator begin();
  iterator end();
  iterator begin() const;
  iterator end() const;
};

template <typename FwIt, typename K>
FwIt find(FwIt, FwIt end, const K &) { return end; }
}

template <typename T> void f(const T &t) {
  std::set<int> s;
  find(s.begin(), s.end(), 46);
}
