struct PositivePrivate {
private:
  PositivePrivate();
  PositivePrivate(const PositivePrivate &);
  PositivePrivate &operator=(PositivePrivate &&);
  ~PositivePrivate();
};
