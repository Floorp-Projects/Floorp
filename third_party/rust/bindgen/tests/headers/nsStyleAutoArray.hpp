
template<typename T>
class nsTArray {
    T* mBuff;
};

template<typename T>
class nsStyleAutoArray
{
public:
  // This constructor places a single element in mFirstElement.
  enum WithSingleInitialElement { WITH_SINGLE_INITIAL_ELEMENT };
  explicit nsStyleAutoArray(WithSingleInitialElement) {}
  nsStyleAutoArray(const nsStyleAutoArray& aOther) { *this = aOther; }
  nsStyleAutoArray& operator=(const nsStyleAutoArray& aOther) {
    mFirstElement = aOther.mFirstElement;
    mOtherElements = aOther.mOtherElements;
    return *this;
  }

  bool operator==(const nsStyleAutoArray& aOther) const {
    return Length() == aOther.Length() &&
           mFirstElement == aOther.mFirstElement &&
           mOtherElements == aOther.mOtherElements;
  }
  bool operator!=(const nsStyleAutoArray& aOther) const {
    return !(*this == aOther);
  }

  unsigned long Length() const {
    return mOtherElements.Length() + 1;
  }
  const T& operator[](unsigned long aIndex) const {
    return aIndex == 0 ? mFirstElement : mOtherElements[aIndex - 1];
  }
  T& operator[](unsigned long aIndex) {
    return aIndex == 0 ? mFirstElement : mOtherElements[aIndex - 1];
  }

  void EnsureLengthAtLeast(unsigned long aMinLen) {
    if (aMinLen > 0) {
      mOtherElements.EnsureLengthAtLeast(aMinLen - 1);
    }
  }

  void SetLengthNonZero(unsigned long aNewLen) {
    mOtherElements.SetLength(aNewLen - 1);
  }

  void TruncateLengthNonZero(unsigned long aNewLen) {
    mOtherElements.TruncateLength(aNewLen - 1);
  }

private:
  T mFirstElement;
  nsTArray<T> mOtherElements;
};
