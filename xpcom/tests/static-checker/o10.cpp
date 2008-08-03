typedef int PRUint32;
typedef int PRInt32;

typedef PRUint32 nsresult;
typedef short PRUnichar;

class nsAString {
public:
  void Read() const;
  void Mutate();
};

nsresult bar();

nsresult foo(nsAString &s) {
  nsresult rv = bar();
  s.Read();
  if (rv == 0) {
    s.Mutate();
  }
  return rv;
}
