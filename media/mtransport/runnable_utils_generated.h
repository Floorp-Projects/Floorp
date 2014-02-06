/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


// 0 arguments --
template<typename M> class runnable_args_nm_0 : public detail::runnable_args_base<detail::NoResult> {
 public:
  runnable_args_nm_0(M m) :
    m_(m)  {}

  NS_IMETHOD Run() {
    m_();
    return NS_OK;
  }

 private:
  M m_;
};



// 0 arguments --
template<typename M, typename R> class runnable_args_nm_0_ret : public detail::runnable_args_base<detail::ReturnsResult> {
 public:
  runnable_args_nm_0_ret(M m, R *r) :
    m_(m), r_(r)  {}

  NS_IMETHOD Run() {
    *r_ = m_();
    return NS_OK;
  }

 private:
  M m_;
  R* r_;
};



// 0 arguments --
template<typename C, typename M> class runnable_args_m_0 : public detail::runnable_args_base<detail::NoResult> {
 public:
  runnable_args_m_0(C o, M m) :
    o_(o), m_(m)  {}

  NS_IMETHOD Run() {
    ((*o_).*m_)();
    return NS_OK;
  }

 private:
  C o_;
  M m_;
};



// 0 arguments --
template<typename C, typename M, typename R> class runnable_args_m_0_ret : public detail::runnable_args_base<detail::ReturnsResult> {
 public:
  runnable_args_m_0_ret(C o, M m, R *r) :
    o_(o), m_(m), r_(r)  {}

  NS_IMETHOD Run() {
    *r_ = ((*o_).*m_)();
    return NS_OK;
  }

 private:
  C o_;
  M m_;
  R* r_;
};



// 1 arguments --
template<typename M, typename A0> class runnable_args_nm_1 : public detail::runnable_args_base<detail::NoResult> {
 public:
  runnable_args_nm_1(M m, A0 a0) :
    m_(m), a0_(a0)  {}

  NS_IMETHOD Run() {
    m_(a0_);
    return NS_OK;
  }

 private:
  M m_;
  A0 a0_;
};



// 1 arguments --
template<typename M, typename A0, typename R> class runnable_args_nm_1_ret : public detail::runnable_args_base<detail::ReturnsResult> {
 public:
  runnable_args_nm_1_ret(M m, A0 a0, R *r) :
    m_(m), r_(r), a0_(a0)  {}

  NS_IMETHOD Run() {
    *r_ = m_(a0_);
    return NS_OK;
  }

 private:
  M m_;
  R* r_;
  A0 a0_;
};



// 1 arguments --
template<typename C, typename M, typename A0> class runnable_args_m_1 : public detail::runnable_args_base<detail::NoResult> {
 public:
  runnable_args_m_1(C o, M m, A0 a0) :
    o_(o), m_(m), a0_(a0)  {}

  NS_IMETHOD Run() {
    ((*o_).*m_)(a0_);
    return NS_OK;
  }

 private:
  C o_;
  M m_;
  A0 a0_;
};



// 1 arguments --
template<typename C, typename M, typename A0, typename R> class runnable_args_m_1_ret : public detail::runnable_args_base<detail::ReturnsResult> {
 public:
  runnable_args_m_1_ret(C o, M m, A0 a0, R *r) :
    o_(o), m_(m), r_(r), a0_(a0)  {}

  NS_IMETHOD Run() {
    *r_ = ((*o_).*m_)(a0_);
    return NS_OK;
  }

 private:
  C o_;
  M m_;
  R* r_;
  A0 a0_;
};



// 2 arguments --
template<typename M, typename A0, typename A1> class runnable_args_nm_2 : public detail::runnable_args_base<detail::NoResult> {
 public:
  runnable_args_nm_2(M m, A0 a0, A1 a1) :
    m_(m), a0_(a0), a1_(a1)  {}

  NS_IMETHOD Run() {
    m_(a0_, a1_);
    return NS_OK;
  }

 private:
  M m_;
  A0 a0_;
  A1 a1_;
};



// 2 arguments --
template<typename M, typename A0, typename A1, typename R> class runnable_args_nm_2_ret : public detail::runnable_args_base<detail::ReturnsResult> {
 public:
  runnable_args_nm_2_ret(M m, A0 a0, A1 a1, R *r) :
    m_(m), r_(r), a0_(a0), a1_(a1)  {}

  NS_IMETHOD Run() {
    *r_ = m_(a0_, a1_);
    return NS_OK;
  }

 private:
  M m_;
  R* r_;
  A0 a0_;
  A1 a1_;
};



// 2 arguments --
template<typename C, typename M, typename A0, typename A1> class runnable_args_m_2 : public detail::runnable_args_base<detail::NoResult> {
 public:
  runnable_args_m_2(C o, M m, A0 a0, A1 a1) :
    o_(o), m_(m), a0_(a0), a1_(a1)  {}

  NS_IMETHOD Run() {
    ((*o_).*m_)(a0_, a1_);
    return NS_OK;
  }

 private:
  C o_;
  M m_;
  A0 a0_;
  A1 a1_;
};



// 2 arguments --
template<typename C, typename M, typename A0, typename A1, typename R> class runnable_args_m_2_ret : public detail::runnable_args_base<detail::ReturnsResult> {
 public:
  runnable_args_m_2_ret(C o, M m, A0 a0, A1 a1, R *r) :
    o_(o), m_(m), r_(r), a0_(a0), a1_(a1)  {}

  NS_IMETHOD Run() {
    *r_ = ((*o_).*m_)(a0_, a1_);
    return NS_OK;
  }

 private:
  C o_;
  M m_;
  R* r_;
  A0 a0_;
  A1 a1_;
};



// 3 arguments --
template<typename M, typename A0, typename A1, typename A2> class runnable_args_nm_3 : public detail::runnable_args_base<detail::NoResult> {
 public:
  runnable_args_nm_3(M m, A0 a0, A1 a1, A2 a2) :
    m_(m), a0_(a0), a1_(a1), a2_(a2)  {}

  NS_IMETHOD Run() {
    m_(a0_, a1_, a2_);
    return NS_OK;
  }

 private:
  M m_;
  A0 a0_;
  A1 a1_;
  A2 a2_;
};



// 3 arguments --
template<typename M, typename A0, typename A1, typename A2, typename R> class runnable_args_nm_3_ret : public detail::runnable_args_base<detail::ReturnsResult> {
 public:
  runnable_args_nm_3_ret(M m, A0 a0, A1 a1, A2 a2, R *r) :
    m_(m), r_(r), a0_(a0), a1_(a1), a2_(a2)  {}

  NS_IMETHOD Run() {
    *r_ = m_(a0_, a1_, a2_);
    return NS_OK;
  }

 private:
  M m_;
  R* r_;
  A0 a0_;
  A1 a1_;
  A2 a2_;
};



// 3 arguments --
template<typename C, typename M, typename A0, typename A1, typename A2> class runnable_args_m_3 : public detail::runnable_args_base<detail::NoResult> {
 public:
  runnable_args_m_3(C o, M m, A0 a0, A1 a1, A2 a2) :
    o_(o), m_(m), a0_(a0), a1_(a1), a2_(a2)  {}

  NS_IMETHOD Run() {
    ((*o_).*m_)(a0_, a1_, a2_);
    return NS_OK;
  }

 private:
  C o_;
  M m_;
  A0 a0_;
  A1 a1_;
  A2 a2_;
};



// 3 arguments --
template<typename C, typename M, typename A0, typename A1, typename A2, typename R> class runnable_args_m_3_ret : public detail::runnable_args_base<detail::ReturnsResult> {
 public:
  runnable_args_m_3_ret(C o, M m, A0 a0, A1 a1, A2 a2, R *r) :
    o_(o), m_(m), r_(r), a0_(a0), a1_(a1), a2_(a2)  {}

  NS_IMETHOD Run() {
    *r_ = ((*o_).*m_)(a0_, a1_, a2_);
    return NS_OK;
  }

 private:
  C o_;
  M m_;
  R* r_;
  A0 a0_;
  A1 a1_;
  A2 a2_;
};



// 4 arguments --
template<typename M, typename A0, typename A1, typename A2, typename A3> class runnable_args_nm_4 : public detail::runnable_args_base<detail::NoResult> {
 public:
  runnable_args_nm_4(M m, A0 a0, A1 a1, A2 a2, A3 a3) :
    m_(m), a0_(a0), a1_(a1), a2_(a2), a3_(a3)  {}

  NS_IMETHOD Run() {
    m_(a0_, a1_, a2_, a3_);
    return NS_OK;
  }

 private:
  M m_;
  A0 a0_;
  A1 a1_;
  A2 a2_;
  A3 a3_;
};



// 4 arguments --
template<typename M, typename A0, typename A1, typename A2, typename A3, typename R> class runnable_args_nm_4_ret : public detail::runnable_args_base<detail::ReturnsResult> {
 public:
  runnable_args_nm_4_ret(M m, A0 a0, A1 a1, A2 a2, A3 a3, R *r) :
    m_(m), r_(r), a0_(a0), a1_(a1), a2_(a2), a3_(a3)  {}

  NS_IMETHOD Run() {
    *r_ = m_(a0_, a1_, a2_, a3_);
    return NS_OK;
  }

 private:
  M m_;
  R* r_;
  A0 a0_;
  A1 a1_;
  A2 a2_;
  A3 a3_;
};



// 4 arguments --
template<typename C, typename M, typename A0, typename A1, typename A2, typename A3> class runnable_args_m_4 : public detail::runnable_args_base<detail::NoResult> {
 public:
  runnable_args_m_4(C o, M m, A0 a0, A1 a1, A2 a2, A3 a3) :
    o_(o), m_(m), a0_(a0), a1_(a1), a2_(a2), a3_(a3)  {}

  NS_IMETHOD Run() {
    ((*o_).*m_)(a0_, a1_, a2_, a3_);
    return NS_OK;
  }

 private:
  C o_;
  M m_;
  A0 a0_;
  A1 a1_;
  A2 a2_;
  A3 a3_;
};



// 4 arguments --
template<typename C, typename M, typename A0, typename A1, typename A2, typename A3, typename R> class runnable_args_m_4_ret : public detail::runnable_args_base<detail::ReturnsResult> {
 public:
  runnable_args_m_4_ret(C o, M m, A0 a0, A1 a1, A2 a2, A3 a3, R *r) :
    o_(o), m_(m), r_(r), a0_(a0), a1_(a1), a2_(a2), a3_(a3)  {}

  NS_IMETHOD Run() {
    *r_ = ((*o_).*m_)(a0_, a1_, a2_, a3_);
    return NS_OK;
  }

 private:
  C o_;
  M m_;
  R* r_;
  A0 a0_;
  A1 a1_;
  A2 a2_;
  A3 a3_;
};



// 5 arguments --
template<typename M, typename A0, typename A1, typename A2, typename A3, typename A4> class runnable_args_nm_5 : public detail::runnable_args_base<detail::NoResult> {
 public:
  runnable_args_nm_5(M m, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4) :
    m_(m), a0_(a0), a1_(a1), a2_(a2), a3_(a3), a4_(a4)  {}

  NS_IMETHOD Run() {
    m_(a0_, a1_, a2_, a3_, a4_);
    return NS_OK;
  }

 private:
  M m_;
  A0 a0_;
  A1 a1_;
  A2 a2_;
  A3 a3_;
  A4 a4_;
};



// 5 arguments --
template<typename M, typename A0, typename A1, typename A2, typename A3, typename A4, typename R> class runnable_args_nm_5_ret : public detail::runnable_args_base<detail::ReturnsResult> {
 public:
  runnable_args_nm_5_ret(M m, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, R *r) :
    m_(m), r_(r), a0_(a0), a1_(a1), a2_(a2), a3_(a3), a4_(a4)  {}

  NS_IMETHOD Run() {
    *r_ = m_(a0_, a1_, a2_, a3_, a4_);
    return NS_OK;
  }

 private:
  M m_;
  R* r_;
  A0 a0_;
  A1 a1_;
  A2 a2_;
  A3 a3_;
  A4 a4_;
};



// 5 arguments --
template<typename C, typename M, typename A0, typename A1, typename A2, typename A3, typename A4> class runnable_args_m_5 : public detail::runnable_args_base<detail::NoResult> {
 public:
  runnable_args_m_5(C o, M m, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4) :
    o_(o), m_(m), a0_(a0), a1_(a1), a2_(a2), a3_(a3), a4_(a4)  {}

  NS_IMETHOD Run() {
    ((*o_).*m_)(a0_, a1_, a2_, a3_, a4_);
    return NS_OK;
  }

 private:
  C o_;
  M m_;
  A0 a0_;
  A1 a1_;
  A2 a2_;
  A3 a3_;
  A4 a4_;
};



// 5 arguments --
template<typename C, typename M, typename A0, typename A1, typename A2, typename A3, typename A4, typename R> class runnable_args_m_5_ret : public detail::runnable_args_base<detail::ReturnsResult> {
 public:
  runnable_args_m_5_ret(C o, M m, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, R *r) :
    o_(o), m_(m), r_(r), a0_(a0), a1_(a1), a2_(a2), a3_(a3), a4_(a4)  {}

  NS_IMETHOD Run() {
    *r_ = ((*o_).*m_)(a0_, a1_, a2_, a3_, a4_);
    return NS_OK;
  }

 private:
  C o_;
  M m_;
  R* r_;
  A0 a0_;
  A1 a1_;
  A2 a2_;
  A3 a3_;
  A4 a4_;
};



// 6 arguments --
template<typename M, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5> class runnable_args_nm_6 : public detail::runnable_args_base<detail::NoResult> {
 public:
  runnable_args_nm_6(M m, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5) :
    m_(m), a0_(a0), a1_(a1), a2_(a2), a3_(a3), a4_(a4), a5_(a5)  {}

  NS_IMETHOD Run() {
    m_(a0_, a1_, a2_, a3_, a4_, a5_);
    return NS_OK;
  }

 private:
  M m_;
  A0 a0_;
  A1 a1_;
  A2 a2_;
  A3 a3_;
  A4 a4_;
  A5 a5_;
};



// 6 arguments --
template<typename M, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename R> class runnable_args_nm_6_ret : public detail::runnable_args_base<detail::ReturnsResult> {
 public:
  runnable_args_nm_6_ret(M m, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, R *r) :
    m_(m), r_(r), a0_(a0), a1_(a1), a2_(a2), a3_(a3), a4_(a4), a5_(a5)  {}

  NS_IMETHOD Run() {
    *r_ = m_(a0_, a1_, a2_, a3_, a4_, a5_);
    return NS_OK;
  }

 private:
  M m_;
  R* r_;
  A0 a0_;
  A1 a1_;
  A2 a2_;
  A3 a3_;
  A4 a4_;
  A5 a5_;
};



// 6 arguments --
template<typename C, typename M, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5> class runnable_args_m_6 : public detail::runnable_args_base<detail::NoResult> {
 public:
  runnable_args_m_6(C o, M m, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5) :
    o_(o), m_(m), a0_(a0), a1_(a1), a2_(a2), a3_(a3), a4_(a4), a5_(a5)  {}

  NS_IMETHOD Run() {
    ((*o_).*m_)(a0_, a1_, a2_, a3_, a4_, a5_);
    return NS_OK;
  }

 private:
  C o_;
  M m_;
  A0 a0_;
  A1 a1_;
  A2 a2_;
  A3 a3_;
  A4 a4_;
  A5 a5_;
};



// 6 arguments --
template<typename C, typename M, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename R> class runnable_args_m_6_ret : public detail::runnable_args_base<detail::ReturnsResult> {
 public:
  runnable_args_m_6_ret(C o, M m, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, R *r) :
    o_(o), m_(m), r_(r), a0_(a0), a1_(a1), a2_(a2), a3_(a3), a4_(a4), a5_(a5)  {}

  NS_IMETHOD Run() {
    *r_ = ((*o_).*m_)(a0_, a1_, a2_, a3_, a4_, a5_);
    return NS_OK;
  }

 private:
  C o_;
  M m_;
  R* r_;
  A0 a0_;
  A1 a1_;
  A2 a2_;
  A3 a3_;
  A4 a4_;
  A5 a5_;
};



// 7 arguments --
template<typename M, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6> class runnable_args_nm_7 : public detail::runnable_args_base<detail::NoResult> {
 public:
  runnable_args_nm_7(M m, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6) :
    m_(m), a0_(a0), a1_(a1), a2_(a2), a3_(a3), a4_(a4), a5_(a5), a6_(a6)  {}

  NS_IMETHOD Run() {
    m_(a0_, a1_, a2_, a3_, a4_, a5_, a6_);
    return NS_OK;
  }

 private:
  M m_;
  A0 a0_;
  A1 a1_;
  A2 a2_;
  A3 a3_;
  A4 a4_;
  A5 a5_;
  A6 a6_;
};



// 7 arguments --
template<typename M, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename R> class runnable_args_nm_7_ret : public detail::runnable_args_base<detail::ReturnsResult> {
 public:
  runnable_args_nm_7_ret(M m, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, R *r) :
    m_(m), r_(r), a0_(a0), a1_(a1), a2_(a2), a3_(a3), a4_(a4), a5_(a5), a6_(a6)  {}

  NS_IMETHOD Run() {
    *r_ = m_(a0_, a1_, a2_, a3_, a4_, a5_, a6_);
    return NS_OK;
  }

 private:
  M m_;
  R* r_;
  A0 a0_;
  A1 a1_;
  A2 a2_;
  A3 a3_;
  A4 a4_;
  A5 a5_;
  A6 a6_;
};



// 7 arguments --
template<typename C, typename M, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6> class runnable_args_m_7 : public detail::runnable_args_base<detail::NoResult> {
 public:
  runnable_args_m_7(C o, M m, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6) :
    o_(o), m_(m), a0_(a0), a1_(a1), a2_(a2), a3_(a3), a4_(a4), a5_(a5), a6_(a6)  {}

  NS_IMETHOD Run() {
    ((*o_).*m_)(a0_, a1_, a2_, a3_, a4_, a5_, a6_);
    return NS_OK;
  }

 private:
  C o_;
  M m_;
  A0 a0_;
  A1 a1_;
  A2 a2_;
  A3 a3_;
  A4 a4_;
  A5 a5_;
  A6 a6_;
};



// 7 arguments --
template<typename C, typename M, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename R> class runnable_args_m_7_ret : public detail::runnable_args_base<detail::ReturnsResult> {
 public:
  runnable_args_m_7_ret(C o, M m, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, R *r) :
    o_(o), m_(m), r_(r), a0_(a0), a1_(a1), a2_(a2), a3_(a3), a4_(a4), a5_(a5), a6_(a6)  {}

  NS_IMETHOD Run() {
    *r_ = ((*o_).*m_)(a0_, a1_, a2_, a3_, a4_, a5_, a6_);
    return NS_OK;
  }

 private:
  C o_;
  M m_;
  R* r_;
  A0 a0_;
  A1 a1_;
  A2 a2_;
  A3 a3_;
  A4 a4_;
  A5 a5_;
  A6 a6_;
};



// 8 arguments --
template<typename M, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7> class runnable_args_nm_8 : public detail::runnable_args_base<detail::NoResult> {
 public:
  runnable_args_nm_8(M m, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7) :
    m_(m), a0_(a0), a1_(a1), a2_(a2), a3_(a3), a4_(a4), a5_(a5), a6_(a6), a7_(a7)  {}

  NS_IMETHOD Run() {
    m_(a0_, a1_, a2_, a3_, a4_, a5_, a6_, a7_);
    return NS_OK;
  }

 private:
  M m_;
  A0 a0_;
  A1 a1_;
  A2 a2_;
  A3 a3_;
  A4 a4_;
  A5 a5_;
  A6 a6_;
  A7 a7_;
};



// 8 arguments --
template<typename M, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename R> class runnable_args_nm_8_ret : public detail::runnable_args_base<detail::ReturnsResult> {
 public:
  runnable_args_nm_8_ret(M m, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, R *r) :
    m_(m), r_(r), a0_(a0), a1_(a1), a2_(a2), a3_(a3), a4_(a4), a5_(a5), a6_(a6), a7_(a7)  {}

  NS_IMETHOD Run() {
    *r_ = m_(a0_, a1_, a2_, a3_, a4_, a5_, a6_, a7_);
    return NS_OK;
  }

 private:
  M m_;
  R* r_;
  A0 a0_;
  A1 a1_;
  A2 a2_;
  A3 a3_;
  A4 a4_;
  A5 a5_;
  A6 a6_;
  A7 a7_;
};



// 8 arguments --
template<typename C, typename M, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7> class runnable_args_m_8 : public detail::runnable_args_base<detail::NoResult> {
 public:
  runnable_args_m_8(C o, M m, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7) :
    o_(o), m_(m), a0_(a0), a1_(a1), a2_(a2), a3_(a3), a4_(a4), a5_(a5), a6_(a6), a7_(a7)  {}

  NS_IMETHOD Run() {
    ((*o_).*m_)(a0_, a1_, a2_, a3_, a4_, a5_, a6_, a7_);
    return NS_OK;
  }

 private:
  C o_;
  M m_;
  A0 a0_;
  A1 a1_;
  A2 a2_;
  A3 a3_;
  A4 a4_;
  A5 a5_;
  A6 a6_;
  A7 a7_;
};



// 8 arguments --
template<typename C, typename M, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename R> class runnable_args_m_8_ret : public detail::runnable_args_base<detail::ReturnsResult> {
 public:
  runnable_args_m_8_ret(C o, M m, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, R *r) :
    o_(o), m_(m), r_(r), a0_(a0), a1_(a1), a2_(a2), a3_(a3), a4_(a4), a5_(a5), a6_(a6), a7_(a7)  {}

  NS_IMETHOD Run() {
    *r_ = ((*o_).*m_)(a0_, a1_, a2_, a3_, a4_, a5_, a6_, a7_);
    return NS_OK;
  }

 private:
  C o_;
  M m_;
  R* r_;
  A0 a0_;
  A1 a1_;
  A2 a2_;
  A3 a3_;
  A4 a4_;
  A5 a5_;
  A6 a6_;
  A7 a7_;
};



// 9 arguments --
template<typename M, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8> class runnable_args_nm_9 : public detail::runnable_args_base<detail::NoResult> {
 public:
  runnable_args_nm_9(M m, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8) :
    m_(m), a0_(a0), a1_(a1), a2_(a2), a3_(a3), a4_(a4), a5_(a5), a6_(a6), a7_(a7), a8_(a8)  {}

  NS_IMETHOD Run() {
    m_(a0_, a1_, a2_, a3_, a4_, a5_, a6_, a7_, a8_);
    return NS_OK;
  }

 private:
  M m_;
  A0 a0_;
  A1 a1_;
  A2 a2_;
  A3 a3_;
  A4 a4_;
  A5 a5_;
  A6 a6_;
  A7 a7_;
  A8 a8_;
};



// 9 arguments --
template<typename M, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename R> class runnable_args_nm_9_ret : public detail::runnable_args_base<detail::ReturnsResult> {
 public:
  runnable_args_nm_9_ret(M m, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8, R *r) :
    m_(m), r_(r), a0_(a0), a1_(a1), a2_(a2), a3_(a3), a4_(a4), a5_(a5), a6_(a6), a7_(a7), a8_(a8)  {}

  NS_IMETHOD Run() {
    *r_ = m_(a0_, a1_, a2_, a3_, a4_, a5_, a6_, a7_, a8_);
    return NS_OK;
  }

 private:
  M m_;
  R* r_;
  A0 a0_;
  A1 a1_;
  A2 a2_;
  A3 a3_;
  A4 a4_;
  A5 a5_;
  A6 a6_;
  A7 a7_;
  A8 a8_;
};



// 9 arguments --
template<typename C, typename M, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8> class runnable_args_m_9 : public detail::runnable_args_base<detail::NoResult> {
 public:
  runnable_args_m_9(C o, M m, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8) :
    o_(o), m_(m), a0_(a0), a1_(a1), a2_(a2), a3_(a3), a4_(a4), a5_(a5), a6_(a6), a7_(a7), a8_(a8)  {}

  NS_IMETHOD Run() {
    ((*o_).*m_)(a0_, a1_, a2_, a3_, a4_, a5_, a6_, a7_, a8_);
    return NS_OK;
  }

 private:
  C o_;
  M m_;
  A0 a0_;
  A1 a1_;
  A2 a2_;
  A3 a3_;
  A4 a4_;
  A5 a5_;
  A6 a6_;
  A7 a7_;
  A8 a8_;
};



// 9 arguments --
template<typename C, typename M, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename R> class runnable_args_m_9_ret : public detail::runnable_args_base<detail::ReturnsResult> {
 public:
  runnable_args_m_9_ret(C o, M m, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8, R *r) :
    o_(o), m_(m), r_(r), a0_(a0), a1_(a1), a2_(a2), a3_(a3), a4_(a4), a5_(a5), a6_(a6), a7_(a7), a8_(a8)  {}

  NS_IMETHOD Run() {
    *r_ = ((*o_).*m_)(a0_, a1_, a2_, a3_, a4_, a5_, a6_, a7_, a8_);
    return NS_OK;
  }

 private:
  C o_;
  M m_;
  R* r_;
  A0 a0_;
  A1 a1_;
  A2 a2_;
  A3 a3_;
  A4 a4_;
  A5 a5_;
  A6 a6_;
  A7 a7_;
  A8 a8_;
};



// 10 arguments --
template<typename M, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9> class runnable_args_nm_10 : public detail::runnable_args_base<detail::NoResult> {
 public:
  runnable_args_nm_10(M m, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8, A9 a9) :
    m_(m), a0_(a0), a1_(a1), a2_(a2), a3_(a3), a4_(a4), a5_(a5), a6_(a6), a7_(a7), a8_(a8), a9_(a9)  {}

  NS_IMETHOD Run() {
    m_(a0_, a1_, a2_, a3_, a4_, a5_, a6_, a7_, a8_, a9_);
    return NS_OK;
  }

 private:
  M m_;
  A0 a0_;
  A1 a1_;
  A2 a2_;
  A3 a3_;
  A4 a4_;
  A5 a5_;
  A6 a6_;
  A7 a7_;
  A8 a8_;
  A9 a9_;
};



// 10 arguments --
template<typename M, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename R> class runnable_args_nm_10_ret : public detail::runnable_args_base<detail::ReturnsResult> {
 public:
  runnable_args_nm_10_ret(M m, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8, A9 a9, R *r) :
    m_(m), r_(r), a0_(a0), a1_(a1), a2_(a2), a3_(a3), a4_(a4), a5_(a5), a6_(a6), a7_(a7), a8_(a8), a9_(a9)  {}

  NS_IMETHOD Run() {
    *r_ = m_(a0_, a1_, a2_, a3_, a4_, a5_, a6_, a7_, a8_, a9_);
    return NS_OK;
  }

 private:
  M m_;
  R* r_;
  A0 a0_;
  A1 a1_;
  A2 a2_;
  A3 a3_;
  A4 a4_;
  A5 a5_;
  A6 a6_;
  A7 a7_;
  A8 a8_;
  A9 a9_;
};



// 10 arguments --
template<typename C, typename M, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9> class runnable_args_m_10 : public detail::runnable_args_base<detail::NoResult> {
 public:
  runnable_args_m_10(C o, M m, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8, A9 a9) :
    o_(o), m_(m), a0_(a0), a1_(a1), a2_(a2), a3_(a3), a4_(a4), a5_(a5), a6_(a6), a7_(a7), a8_(a8), a9_(a9)  {}

  NS_IMETHOD Run() {
    ((*o_).*m_)(a0_, a1_, a2_, a3_, a4_, a5_, a6_, a7_, a8_, a9_);
    return NS_OK;
  }

 private:
  C o_;
  M m_;
  A0 a0_;
  A1 a1_;
  A2 a2_;
  A3 a3_;
  A4 a4_;
  A5 a5_;
  A6 a6_;
  A7 a7_;
  A8 a8_;
  A9 a9_;
};



// 10 arguments --
template<typename C, typename M, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename R> class runnable_args_m_10_ret : public detail::runnable_args_base<detail::ReturnsResult> {
 public:
  runnable_args_m_10_ret(C o, M m, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8, A9 a9, R *r) :
    o_(o), m_(m), r_(r), a0_(a0), a1_(a1), a2_(a2), a3_(a3), a4_(a4), a5_(a5), a6_(a6), a7_(a7), a8_(a8), a9_(a9)  {}

  NS_IMETHOD Run() {
    *r_ = ((*o_).*m_)(a0_, a1_, a2_, a3_, a4_, a5_, a6_, a7_, a8_, a9_);
    return NS_OK;
  }

 private:
  C o_;
  M m_;
  R* r_;
  A0 a0_;
  A1 a1_;
  A2 a2_;
  A3 a3_;
  A4 a4_;
  A5 a5_;
  A6 a6_;
  A7 a7_;
  A8 a8_;
  A9 a9_;
};



// 11 arguments --
template<typename M, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10> class runnable_args_nm_11 : public detail::runnable_args_base<detail::NoResult> {
 public:
  runnable_args_nm_11(M m, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8, A9 a9, A10 a10) :
    m_(m), a0_(a0), a1_(a1), a2_(a2), a3_(a3), a4_(a4), a5_(a5), a6_(a6), a7_(a7), a8_(a8), a9_(a9), a10_(a10)  {}

  NS_IMETHOD Run() {
    m_(a0_, a1_, a2_, a3_, a4_, a5_, a6_, a7_, a8_, a9_, a10_);
    return NS_OK;
  }

 private:
  M m_;
  A0 a0_;
  A1 a1_;
  A2 a2_;
  A3 a3_;
  A4 a4_;
  A5 a5_;
  A6 a6_;
  A7 a7_;
  A8 a8_;
  A9 a9_;
  A10 a10_;
};



// 11 arguments --
template<typename M, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename R> class runnable_args_nm_11_ret : public detail::runnable_args_base<detail::ReturnsResult> {
 public:
  runnable_args_nm_11_ret(M m, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8, A9 a9, A10 a10, R *r) :
    m_(m), r_(r), a0_(a0), a1_(a1), a2_(a2), a3_(a3), a4_(a4), a5_(a5), a6_(a6), a7_(a7), a8_(a8), a9_(a9), a10_(a10)  {}

  NS_IMETHOD Run() {
    *r_ = m_(a0_, a1_, a2_, a3_, a4_, a5_, a6_, a7_, a8_, a9_, a10_);
    return NS_OK;
  }

 private:
  M m_;
  R* r_;
  A0 a0_;
  A1 a1_;
  A2 a2_;
  A3 a3_;
  A4 a4_;
  A5 a5_;
  A6 a6_;
  A7 a7_;
  A8 a8_;
  A9 a9_;
  A10 a10_;
};



// 11 arguments --
template<typename C, typename M, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10> class runnable_args_m_11 : public detail::runnable_args_base<detail::NoResult> {
 public:
  runnable_args_m_11(C o, M m, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8, A9 a9, A10 a10) :
    o_(o), m_(m), a0_(a0), a1_(a1), a2_(a2), a3_(a3), a4_(a4), a5_(a5), a6_(a6), a7_(a7), a8_(a8), a9_(a9), a10_(a10)  {}

  NS_IMETHOD Run() {
    ((*o_).*m_)(a0_, a1_, a2_, a3_, a4_, a5_, a6_, a7_, a8_, a9_, a10_);
    return NS_OK;
  }

 private:
  C o_;
  M m_;
  A0 a0_;
  A1 a1_;
  A2 a2_;
  A3 a3_;
  A4 a4_;
  A5 a5_;
  A6 a6_;
  A7 a7_;
  A8 a8_;
  A9 a9_;
  A10 a10_;
};



// 11 arguments --
template<typename C, typename M, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename R> class runnable_args_m_11_ret : public detail::runnable_args_base<detail::ReturnsResult> {
 public:
  runnable_args_m_11_ret(C o, M m, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8, A9 a9, A10 a10, R *r) :
    o_(o), m_(m), r_(r), a0_(a0), a1_(a1), a2_(a2), a3_(a3), a4_(a4), a5_(a5), a6_(a6), a7_(a7), a8_(a8), a9_(a9), a10_(a10)  {}

  NS_IMETHOD Run() {
    *r_ = ((*o_).*m_)(a0_, a1_, a2_, a3_, a4_, a5_, a6_, a7_, a8_, a9_, a10_);
    return NS_OK;
  }

 private:
  C o_;
  M m_;
  R* r_;
  A0 a0_;
  A1 a1_;
  A2 a2_;
  A3 a3_;
  A4 a4_;
  A5 a5_;
  A6 a6_;
  A7 a7_;
  A8 a8_;
  A9 a9_;
  A10 a10_;
};



// 12 arguments --
template<typename M, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11> class runnable_args_nm_12 : public detail::runnable_args_base<detail::NoResult> {
 public:
  runnable_args_nm_12(M m, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8, A9 a9, A10 a10, A11 a11) :
    m_(m), a0_(a0), a1_(a1), a2_(a2), a3_(a3), a4_(a4), a5_(a5), a6_(a6), a7_(a7), a8_(a8), a9_(a9), a10_(a10), a11_(a11)  {}

  NS_IMETHOD Run() {
    m_(a0_, a1_, a2_, a3_, a4_, a5_, a6_, a7_, a8_, a9_, a10_, a11_);
    return NS_OK;
  }

 private:
  M m_;
  A0 a0_;
  A1 a1_;
  A2 a2_;
  A3 a3_;
  A4 a4_;
  A5 a5_;
  A6 a6_;
  A7 a7_;
  A8 a8_;
  A9 a9_;
  A10 a10_;
  A11 a11_;
};



// 12 arguments --
template<typename M, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename R> class runnable_args_nm_12_ret : public detail::runnable_args_base<detail::ReturnsResult> {
 public:
  runnable_args_nm_12_ret(M m, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8, A9 a9, A10 a10, A11 a11, R *r) :
    m_(m), r_(r), a0_(a0), a1_(a1), a2_(a2), a3_(a3), a4_(a4), a5_(a5), a6_(a6), a7_(a7), a8_(a8), a9_(a9), a10_(a10), a11_(a11)  {}

  NS_IMETHOD Run() {
    *r_ = m_(a0_, a1_, a2_, a3_, a4_, a5_, a6_, a7_, a8_, a9_, a10_, a11_);
    return NS_OK;
  }

 private:
  M m_;
  R* r_;
  A0 a0_;
  A1 a1_;
  A2 a2_;
  A3 a3_;
  A4 a4_;
  A5 a5_;
  A6 a6_;
  A7 a7_;
  A8 a8_;
  A9 a9_;
  A10 a10_;
  A11 a11_;
};



// 12 arguments --
template<typename C, typename M, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11> class runnable_args_m_12 : public detail::runnable_args_base<detail::NoResult> {
 public:
  runnable_args_m_12(C o, M m, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8, A9 a9, A10 a10, A11 a11) :
    o_(o), m_(m), a0_(a0), a1_(a1), a2_(a2), a3_(a3), a4_(a4), a5_(a5), a6_(a6), a7_(a7), a8_(a8), a9_(a9), a10_(a10), a11_(a11)  {}

  NS_IMETHOD Run() {
    ((*o_).*m_)(a0_, a1_, a2_, a3_, a4_, a5_, a6_, a7_, a8_, a9_, a10_, a11_);
    return NS_OK;
  }

 private:
  C o_;
  M m_;
  A0 a0_;
  A1 a1_;
  A2 a2_;
  A3 a3_;
  A4 a4_;
  A5 a5_;
  A6 a6_;
  A7 a7_;
  A8 a8_;
  A9 a9_;
  A10 a10_;
  A11 a11_;
};



// 12 arguments --
template<typename C, typename M, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename R> class runnable_args_m_12_ret : public detail::runnable_args_base<detail::ReturnsResult> {
 public:
  runnable_args_m_12_ret(C o, M m, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8, A9 a9, A10 a10, A11 a11, R *r) :
    o_(o), m_(m), r_(r), a0_(a0), a1_(a1), a2_(a2), a3_(a3), a4_(a4), a5_(a5), a6_(a6), a7_(a7), a8_(a8), a9_(a9), a10_(a10), a11_(a11)  {}

  NS_IMETHOD Run() {
    *r_ = ((*o_).*m_)(a0_, a1_, a2_, a3_, a4_, a5_, a6_, a7_, a8_, a9_, a10_, a11_);
    return NS_OK;
  }

 private:
  C o_;
  M m_;
  R* r_;
  A0 a0_;
  A1 a1_;
  A2 a2_;
  A3 a3_;
  A4 a4_;
  A5 a5_;
  A6 a6_;
  A7 a7_;
  A8 a8_;
  A9 a9_;
  A10 a10_;
  A11 a11_;
};



// 13 arguments --
template<typename M, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12> class runnable_args_nm_13 : public detail::runnable_args_base<detail::NoResult> {
 public:
  runnable_args_nm_13(M m, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8, A9 a9, A10 a10, A11 a11, A12 a12) :
    m_(m), a0_(a0), a1_(a1), a2_(a2), a3_(a3), a4_(a4), a5_(a5), a6_(a6), a7_(a7), a8_(a8), a9_(a9), a10_(a10), a11_(a11), a12_(a12)  {}

  NS_IMETHOD Run() {
    m_(a0_, a1_, a2_, a3_, a4_, a5_, a6_, a7_, a8_, a9_, a10_, a11_, a12_);
    return NS_OK;
  }

 private:
  M m_;
  A0 a0_;
  A1 a1_;
  A2 a2_;
  A3 a3_;
  A4 a4_;
  A5 a5_;
  A6 a6_;
  A7 a7_;
  A8 a8_;
  A9 a9_;
  A10 a10_;
  A11 a11_;
  A12 a12_;
};



// 13 arguments --
template<typename M, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename R> class runnable_args_nm_13_ret : public detail::runnable_args_base<detail::ReturnsResult> {
 public:
  runnable_args_nm_13_ret(M m, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8, A9 a9, A10 a10, A11 a11, A12 a12, R *r) :
    m_(m), r_(r), a0_(a0), a1_(a1), a2_(a2), a3_(a3), a4_(a4), a5_(a5), a6_(a6), a7_(a7), a8_(a8), a9_(a9), a10_(a10), a11_(a11), a12_(a12)  {}

  NS_IMETHOD Run() {
    *r_ = m_(a0_, a1_, a2_, a3_, a4_, a5_, a6_, a7_, a8_, a9_, a10_, a11_, a12_);
    return NS_OK;
  }

 private:
  M m_;
  R* r_;
  A0 a0_;
  A1 a1_;
  A2 a2_;
  A3 a3_;
  A4 a4_;
  A5 a5_;
  A6 a6_;
  A7 a7_;
  A8 a8_;
  A9 a9_;
  A10 a10_;
  A11 a11_;
  A12 a12_;
};



// 13 arguments --
template<typename C, typename M, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12> class runnable_args_m_13 : public detail::runnable_args_base<detail::NoResult> {
 public:
  runnable_args_m_13(C o, M m, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8, A9 a9, A10 a10, A11 a11, A12 a12) :
    o_(o), m_(m), a0_(a0), a1_(a1), a2_(a2), a3_(a3), a4_(a4), a5_(a5), a6_(a6), a7_(a7), a8_(a8), a9_(a9), a10_(a10), a11_(a11), a12_(a12)  {}

  NS_IMETHOD Run() {
    ((*o_).*m_)(a0_, a1_, a2_, a3_, a4_, a5_, a6_, a7_, a8_, a9_, a10_, a11_, a12_);
    return NS_OK;
  }

 private:
  C o_;
  M m_;
  A0 a0_;
  A1 a1_;
  A2 a2_;
  A3 a3_;
  A4 a4_;
  A5 a5_;
  A6 a6_;
  A7 a7_;
  A8 a8_;
  A9 a9_;
  A10 a10_;
  A11 a11_;
  A12 a12_;
};



// 13 arguments --
template<typename C, typename M, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename R> class runnable_args_m_13_ret : public detail::runnable_args_base<detail::ReturnsResult> {
 public:
  runnable_args_m_13_ret(C o, M m, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8, A9 a9, A10 a10, A11 a11, A12 a12, R *r) :
    o_(o), m_(m), r_(r), a0_(a0), a1_(a1), a2_(a2), a3_(a3), a4_(a4), a5_(a5), a6_(a6), a7_(a7), a8_(a8), a9_(a9), a10_(a10), a11_(a11), a12_(a12)  {}

  NS_IMETHOD Run() {
    *r_ = ((*o_).*m_)(a0_, a1_, a2_, a3_, a4_, a5_, a6_, a7_, a8_, a9_, a10_, a11_, a12_);
    return NS_OK;
  }

 private:
  C o_;
  M m_;
  R* r_;
  A0 a0_;
  A1 a1_;
  A2 a2_;
  A3 a3_;
  A4 a4_;
  A5 a5_;
  A6 a6_;
  A7 a7_;
  A8 a8_;
  A9 a9_;
  A10 a10_;
  A11 a11_;
  A12 a12_;
};



// 14 arguments --
template<typename M, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13> class runnable_args_nm_14 : public detail::runnable_args_base<detail::NoResult> {
 public:
  runnable_args_nm_14(M m, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8, A9 a9, A10 a10, A11 a11, A12 a12, A13 a13) :
    m_(m), a0_(a0), a1_(a1), a2_(a2), a3_(a3), a4_(a4), a5_(a5), a6_(a6), a7_(a7), a8_(a8), a9_(a9), a10_(a10), a11_(a11), a12_(a12), a13_(a13)  {}

  NS_IMETHOD Run() {
    m_(a0_, a1_, a2_, a3_, a4_, a5_, a6_, a7_, a8_, a9_, a10_, a11_, a12_, a13_);
    return NS_OK;
  }

 private:
  M m_;
  A0 a0_;
  A1 a1_;
  A2 a2_;
  A3 a3_;
  A4 a4_;
  A5 a5_;
  A6 a6_;
  A7 a7_;
  A8 a8_;
  A9 a9_;
  A10 a10_;
  A11 a11_;
  A12 a12_;
  A13 a13_;
};



// 14 arguments --
template<typename M, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename R> class runnable_args_nm_14_ret : public detail::runnable_args_base<detail::ReturnsResult> {
 public:
  runnable_args_nm_14_ret(M m, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8, A9 a9, A10 a10, A11 a11, A12 a12, A13 a13, R *r) :
    m_(m), r_(r), a0_(a0), a1_(a1), a2_(a2), a3_(a3), a4_(a4), a5_(a5), a6_(a6), a7_(a7), a8_(a8), a9_(a9), a10_(a10), a11_(a11), a12_(a12), a13_(a13)  {}

  NS_IMETHOD Run() {
    *r_ = m_(a0_, a1_, a2_, a3_, a4_, a5_, a6_, a7_, a8_, a9_, a10_, a11_, a12_, a13_);
    return NS_OK;
  }

 private:
  M m_;
  R* r_;
  A0 a0_;
  A1 a1_;
  A2 a2_;
  A3 a3_;
  A4 a4_;
  A5 a5_;
  A6 a6_;
  A7 a7_;
  A8 a8_;
  A9 a9_;
  A10 a10_;
  A11 a11_;
  A12 a12_;
  A13 a13_;
};



// 14 arguments --
template<typename C, typename M, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13> class runnable_args_m_14 : public detail::runnable_args_base<detail::NoResult> {
 public:
  runnable_args_m_14(C o, M m, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8, A9 a9, A10 a10, A11 a11, A12 a12, A13 a13) :
    o_(o), m_(m), a0_(a0), a1_(a1), a2_(a2), a3_(a3), a4_(a4), a5_(a5), a6_(a6), a7_(a7), a8_(a8), a9_(a9), a10_(a10), a11_(a11), a12_(a12), a13_(a13)  {}

  NS_IMETHOD Run() {
    ((*o_).*m_)(a0_, a1_, a2_, a3_, a4_, a5_, a6_, a7_, a8_, a9_, a10_, a11_, a12_, a13_);
    return NS_OK;
  }

 private:
  C o_;
  M m_;
  A0 a0_;
  A1 a1_;
  A2 a2_;
  A3 a3_;
  A4 a4_;
  A5 a5_;
  A6 a6_;
  A7 a7_;
  A8 a8_;
  A9 a9_;
  A10 a10_;
  A11 a11_;
  A12 a12_;
  A13 a13_;
};



// 14 arguments --
template<typename C, typename M, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename R> class runnable_args_m_14_ret : public detail::runnable_args_base<detail::ReturnsResult> {
 public:
  runnable_args_m_14_ret(C o, M m, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8, A9 a9, A10 a10, A11 a11, A12 a12, A13 a13, R *r) :
    o_(o), m_(m), r_(r), a0_(a0), a1_(a1), a2_(a2), a3_(a3), a4_(a4), a5_(a5), a6_(a6), a7_(a7), a8_(a8), a9_(a9), a10_(a10), a11_(a11), a12_(a12), a13_(a13)  {}

  NS_IMETHOD Run() {
    *r_ = ((*o_).*m_)(a0_, a1_, a2_, a3_, a4_, a5_, a6_, a7_, a8_, a9_, a10_, a11_, a12_, a13_);
    return NS_OK;
  }

 private:
  C o_;
  M m_;
  R* r_;
  A0 a0_;
  A1 a1_;
  A2 a2_;
  A3 a3_;
  A4 a4_;
  A5 a5_;
  A6 a6_;
  A7 a7_;
  A8 a8_;
  A9 a9_;
  A10 a10_;
  A11 a11_;
  A12 a12_;
  A13 a13_;
};






// 0 arguments --
template<typename M>
runnable_args_nm_0<M>* WrapRunnableNM(M m) {
  return new runnable_args_nm_0<M>
    (m);
}

// 0 arguments --
template<typename M, typename R>
runnable_args_nm_0_ret<M, R>* WrapRunnableNMRet(M m, R* r) {
  return new runnable_args_nm_0_ret<M, R>
    (m, r);
}

// 0 arguments --
template<typename C, typename M>
runnable_args_m_0<C, M>* WrapRunnable(C o, M m) {
  return new runnable_args_m_0<C, M>
    (o, m);
}

// 0 arguments --
template<typename C, typename M, typename R>
runnable_args_m_0_ret<C, M, R>* WrapRunnableRet(C o, M m, R* r) {
  return new runnable_args_m_0_ret<C, M, R>
    (o, m, r);
}

// 1 arguments --
template<typename M, typename A0>
runnable_args_nm_1<M, A0>* WrapRunnableNM(M m, A0 a0) {
  return new runnable_args_nm_1<M, A0>
    (m, a0);
}

// 1 arguments --
template<typename M, typename A0, typename R>
runnable_args_nm_1_ret<M, A0, R>* WrapRunnableNMRet(M m, A0 a0, R* r) {
  return new runnable_args_nm_1_ret<M, A0, R>
    (m, a0, r);
}

// 1 arguments --
template<typename C, typename M, typename A0>
runnable_args_m_1<C, M, A0>* WrapRunnable(C o, M m, A0 a0) {
  return new runnable_args_m_1<C, M, A0>
    (o, m, a0);
}

// 1 arguments --
template<typename C, typename M, typename A0, typename R>
runnable_args_m_1_ret<C, M, A0, R>* WrapRunnableRet(C o, M m, A0 a0, R* r) {
  return new runnable_args_m_1_ret<C, M, A0, R>
    (o, m, a0, r);
}

// 2 arguments --
template<typename M, typename A0, typename A1>
runnable_args_nm_2<M, A0, A1>* WrapRunnableNM(M m, A0 a0, A1 a1) {
  return new runnable_args_nm_2<M, A0, A1>
    (m, a0, a1);
}

// 2 arguments --
template<typename M, typename A0, typename A1, typename R>
runnable_args_nm_2_ret<M, A0, A1, R>* WrapRunnableNMRet(M m, A0 a0, A1 a1, R* r) {
  return new runnable_args_nm_2_ret<M, A0, A1, R>
    (m, a0, a1, r);
}

// 2 arguments --
template<typename C, typename M, typename A0, typename A1>
runnable_args_m_2<C, M, A0, A1>* WrapRunnable(C o, M m, A0 a0, A1 a1) {
  return new runnable_args_m_2<C, M, A0, A1>
    (o, m, a0, a1);
}

// 2 arguments --
template<typename C, typename M, typename A0, typename A1, typename R>
runnable_args_m_2_ret<C, M, A0, A1, R>* WrapRunnableRet(C o, M m, A0 a0, A1 a1, R* r) {
  return new runnable_args_m_2_ret<C, M, A0, A1, R>
    (o, m, a0, a1, r);
}

// 3 arguments --
template<typename M, typename A0, typename A1, typename A2>
runnable_args_nm_3<M, A0, A1, A2>* WrapRunnableNM(M m, A0 a0, A1 a1, A2 a2) {
  return new runnable_args_nm_3<M, A0, A1, A2>
    (m, a0, a1, a2);
}

// 3 arguments --
template<typename M, typename A0, typename A1, typename A2, typename R>
runnable_args_nm_3_ret<M, A0, A1, A2, R>* WrapRunnableNMRet(M m, A0 a0, A1 a1, A2 a2, R* r) {
  return new runnable_args_nm_3_ret<M, A0, A1, A2, R>
    (m, a0, a1, a2, r);
}

// 3 arguments --
template<typename C, typename M, typename A0, typename A1, typename A2>
runnable_args_m_3<C, M, A0, A1, A2>* WrapRunnable(C o, M m, A0 a0, A1 a1, A2 a2) {
  return new runnable_args_m_3<C, M, A0, A1, A2>
    (o, m, a0, a1, a2);
}

// 3 arguments --
template<typename C, typename M, typename A0, typename A1, typename A2, typename R>
runnable_args_m_3_ret<C, M, A0, A1, A2, R>* WrapRunnableRet(C o, M m, A0 a0, A1 a1, A2 a2, R* r) {
  return new runnable_args_m_3_ret<C, M, A0, A1, A2, R>
    (o, m, a0, a1, a2, r);
}

// 4 arguments --
template<typename M, typename A0, typename A1, typename A2, typename A3>
runnable_args_nm_4<M, A0, A1, A2, A3>* WrapRunnableNM(M m, A0 a0, A1 a1, A2 a2, A3 a3) {
  return new runnable_args_nm_4<M, A0, A1, A2, A3>
    (m, a0, a1, a2, a3);
}

// 4 arguments --
template<typename M, typename A0, typename A1, typename A2, typename A3, typename R>
runnable_args_nm_4_ret<M, A0, A1, A2, A3, R>* WrapRunnableNMRet(M m, A0 a0, A1 a1, A2 a2, A3 a3, R* r) {
  return new runnable_args_nm_4_ret<M, A0, A1, A2, A3, R>
    (m, a0, a1, a2, a3, r);
}

// 4 arguments --
template<typename C, typename M, typename A0, typename A1, typename A2, typename A3>
runnable_args_m_4<C, M, A0, A1, A2, A3>* WrapRunnable(C o, M m, A0 a0, A1 a1, A2 a2, A3 a3) {
  return new runnable_args_m_4<C, M, A0, A1, A2, A3>
    (o, m, a0, a1, a2, a3);
}

// 4 arguments --
template<typename C, typename M, typename A0, typename A1, typename A2, typename A3, typename R>
runnable_args_m_4_ret<C, M, A0, A1, A2, A3, R>* WrapRunnableRet(C o, M m, A0 a0, A1 a1, A2 a2, A3 a3, R* r) {
  return new runnable_args_m_4_ret<C, M, A0, A1, A2, A3, R>
    (o, m, a0, a1, a2, a3, r);
}

// 5 arguments --
template<typename M, typename A0, typename A1, typename A2, typename A3, typename A4>
runnable_args_nm_5<M, A0, A1, A2, A3, A4>* WrapRunnableNM(M m, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4) {
  return new runnable_args_nm_5<M, A0, A1, A2, A3, A4>
    (m, a0, a1, a2, a3, a4);
}

// 5 arguments --
template<typename M, typename A0, typename A1, typename A2, typename A3, typename A4, typename R>
runnable_args_nm_5_ret<M, A0, A1, A2, A3, A4, R>* WrapRunnableNMRet(M m, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, R* r) {
  return new runnable_args_nm_5_ret<M, A0, A1, A2, A3, A4, R>
    (m, a0, a1, a2, a3, a4, r);
}

// 5 arguments --
template<typename C, typename M, typename A0, typename A1, typename A2, typename A3, typename A4>
runnable_args_m_5<C, M, A0, A1, A2, A3, A4>* WrapRunnable(C o, M m, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4) {
  return new runnable_args_m_5<C, M, A0, A1, A2, A3, A4>
    (o, m, a0, a1, a2, a3, a4);
}

// 5 arguments --
template<typename C, typename M, typename A0, typename A1, typename A2, typename A3, typename A4, typename R>
runnable_args_m_5_ret<C, M, A0, A1, A2, A3, A4, R>* WrapRunnableRet(C o, M m, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, R* r) {
  return new runnable_args_m_5_ret<C, M, A0, A1, A2, A3, A4, R>
    (o, m, a0, a1, a2, a3, a4, r);
}

// 6 arguments --
template<typename M, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5>
runnable_args_nm_6<M, A0, A1, A2, A3, A4, A5>* WrapRunnableNM(M m, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5) {
  return new runnable_args_nm_6<M, A0, A1, A2, A3, A4, A5>
    (m, a0, a1, a2, a3, a4, a5);
}

// 6 arguments --
template<typename M, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename R>
runnable_args_nm_6_ret<M, A0, A1, A2, A3, A4, A5, R>* WrapRunnableNMRet(M m, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, R* r) {
  return new runnable_args_nm_6_ret<M, A0, A1, A2, A3, A4, A5, R>
    (m, a0, a1, a2, a3, a4, a5, r);
}

// 6 arguments --
template<typename C, typename M, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5>
runnable_args_m_6<C, M, A0, A1, A2, A3, A4, A5>* WrapRunnable(C o, M m, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5) {
  return new runnable_args_m_6<C, M, A0, A1, A2, A3, A4, A5>
    (o, m, a0, a1, a2, a3, a4, a5);
}

// 6 arguments --
template<typename C, typename M, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename R>
runnable_args_m_6_ret<C, M, A0, A1, A2, A3, A4, A5, R>* WrapRunnableRet(C o, M m, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, R* r) {
  return new runnable_args_m_6_ret<C, M, A0, A1, A2, A3, A4, A5, R>
    (o, m, a0, a1, a2, a3, a4, a5, r);
}

// 7 arguments --
template<typename M, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6>
runnable_args_nm_7<M, A0, A1, A2, A3, A4, A5, A6>* WrapRunnableNM(M m, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6) {
  return new runnable_args_nm_7<M, A0, A1, A2, A3, A4, A5, A6>
    (m, a0, a1, a2, a3, a4, a5, a6);
}

// 7 arguments --
template<typename M, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename R>
runnable_args_nm_7_ret<M, A0, A1, A2, A3, A4, A5, A6, R>* WrapRunnableNMRet(M m, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, R* r) {
  return new runnable_args_nm_7_ret<M, A0, A1, A2, A3, A4, A5, A6, R>
    (m, a0, a1, a2, a3, a4, a5, a6, r);
}

// 7 arguments --
template<typename C, typename M, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6>
runnable_args_m_7<C, M, A0, A1, A2, A3, A4, A5, A6>* WrapRunnable(C o, M m, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6) {
  return new runnable_args_m_7<C, M, A0, A1, A2, A3, A4, A5, A6>
    (o, m, a0, a1, a2, a3, a4, a5, a6);
}

// 7 arguments --
template<typename C, typename M, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename R>
runnable_args_m_7_ret<C, M, A0, A1, A2, A3, A4, A5, A6, R>* WrapRunnableRet(C o, M m, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, R* r) {
  return new runnable_args_m_7_ret<C, M, A0, A1, A2, A3, A4, A5, A6, R>
    (o, m, a0, a1, a2, a3, a4, a5, a6, r);
}

// 8 arguments --
template<typename M, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7>
runnable_args_nm_8<M, A0, A1, A2, A3, A4, A5, A6, A7>* WrapRunnableNM(M m, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7) {
  return new runnable_args_nm_8<M, A0, A1, A2, A3, A4, A5, A6, A7>
    (m, a0, a1, a2, a3, a4, a5, a6, a7);
}

// 8 arguments --
template<typename M, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename R>
runnable_args_nm_8_ret<M, A0, A1, A2, A3, A4, A5, A6, A7, R>* WrapRunnableNMRet(M m, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, R* r) {
  return new runnable_args_nm_8_ret<M, A0, A1, A2, A3, A4, A5, A6, A7, R>
    (m, a0, a1, a2, a3, a4, a5, a6, a7, r);
}

// 8 arguments --
template<typename C, typename M, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7>
runnable_args_m_8<C, M, A0, A1, A2, A3, A4, A5, A6, A7>* WrapRunnable(C o, M m, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7) {
  return new runnable_args_m_8<C, M, A0, A1, A2, A3, A4, A5, A6, A7>
    (o, m, a0, a1, a2, a3, a4, a5, a6, a7);
}

// 8 arguments --
template<typename C, typename M, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename R>
runnable_args_m_8_ret<C, M, A0, A1, A2, A3, A4, A5, A6, A7, R>* WrapRunnableRet(C o, M m, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, R* r) {
  return new runnable_args_m_8_ret<C, M, A0, A1, A2, A3, A4, A5, A6, A7, R>
    (o, m, a0, a1, a2, a3, a4, a5, a6, a7, r);
}

// 9 arguments --
template<typename M, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8>
runnable_args_nm_9<M, A0, A1, A2, A3, A4, A5, A6, A7, A8>* WrapRunnableNM(M m, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8) {
  return new runnable_args_nm_9<M, A0, A1, A2, A3, A4, A5, A6, A7, A8>
    (m, a0, a1, a2, a3, a4, a5, a6, a7, a8);
}

// 9 arguments --
template<typename M, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename R>
runnable_args_nm_9_ret<M, A0, A1, A2, A3, A4, A5, A6, A7, A8, R>* WrapRunnableNMRet(M m, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8, R* r) {
  return new runnable_args_nm_9_ret<M, A0, A1, A2, A3, A4, A5, A6, A7, A8, R>
    (m, a0, a1, a2, a3, a4, a5, a6, a7, a8, r);
}

// 9 arguments --
template<typename C, typename M, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8>
runnable_args_m_9<C, M, A0, A1, A2, A3, A4, A5, A6, A7, A8>* WrapRunnable(C o, M m, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8) {
  return new runnable_args_m_9<C, M, A0, A1, A2, A3, A4, A5, A6, A7, A8>
    (o, m, a0, a1, a2, a3, a4, a5, a6, a7, a8);
}

// 9 arguments --
template<typename C, typename M, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename R>
runnable_args_m_9_ret<C, M, A0, A1, A2, A3, A4, A5, A6, A7, A8, R>* WrapRunnableRet(C o, M m, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8, R* r) {
  return new runnable_args_m_9_ret<C, M, A0, A1, A2, A3, A4, A5, A6, A7, A8, R>
    (o, m, a0, a1, a2, a3, a4, a5, a6, a7, a8, r);
}

// 10 arguments --
template<typename M, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9>
runnable_args_nm_10<M, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9>* WrapRunnableNM(M m, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8, A9 a9) {
  return new runnable_args_nm_10<M, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9>
    (m, a0, a1, a2, a3, a4, a5, a6, a7, a8, a9);
}

// 10 arguments --
template<typename M, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename R>
runnable_args_nm_10_ret<M, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, R>* WrapRunnableNMRet(M m, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8, A9 a9, R* r) {
  return new runnable_args_nm_10_ret<M, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, R>
    (m, a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, r);
}

// 10 arguments --
template<typename C, typename M, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9>
runnable_args_m_10<C, M, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9>* WrapRunnable(C o, M m, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8, A9 a9) {
  return new runnable_args_m_10<C, M, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9>
    (o, m, a0, a1, a2, a3, a4, a5, a6, a7, a8, a9);
}

// 10 arguments --
template<typename C, typename M, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename R>
runnable_args_m_10_ret<C, M, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, R>* WrapRunnableRet(C o, M m, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8, A9 a9, R* r) {
  return new runnable_args_m_10_ret<C, M, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, R>
    (o, m, a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, r);
}

// 11 arguments --
template<typename M, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10>
runnable_args_nm_11<M, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10>* WrapRunnableNM(M m, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8, A9 a9, A10 a10) {
  return new runnable_args_nm_11<M, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10>
    (m, a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10);
}

// 11 arguments --
template<typename M, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename R>
runnable_args_nm_11_ret<M, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, R>* WrapRunnableNMRet(M m, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8, A9 a9, A10 a10, R* r) {
  return new runnable_args_nm_11_ret<M, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, R>
    (m, a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, r);
}

// 11 arguments --
template<typename C, typename M, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10>
runnable_args_m_11<C, M, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10>* WrapRunnable(C o, M m, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8, A9 a9, A10 a10) {
  return new runnable_args_m_11<C, M, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10>
    (o, m, a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10);
}

// 11 arguments --
template<typename C, typename M, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename R>
runnable_args_m_11_ret<C, M, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, R>* WrapRunnableRet(C o, M m, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8, A9 a9, A10 a10, R* r) {
  return new runnable_args_m_11_ret<C, M, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, R>
    (o, m, a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, r);
}

// 12 arguments --
template<typename M, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11>
runnable_args_nm_12<M, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11>* WrapRunnableNM(M m, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8, A9 a9, A10 a10, A11 a11) {
  return new runnable_args_nm_12<M, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11>
    (m, a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11);
}

// 12 arguments --
template<typename M, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename R>
runnable_args_nm_12_ret<M, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, R>* WrapRunnableNMRet(M m, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8, A9 a9, A10 a10, A11 a11, R* r) {
  return new runnable_args_nm_12_ret<M, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, R>
    (m, a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, r);
}

// 12 arguments --
template<typename C, typename M, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11>
runnable_args_m_12<C, M, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11>* WrapRunnable(C o, M m, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8, A9 a9, A10 a10, A11 a11) {
  return new runnable_args_m_12<C, M, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11>
    (o, m, a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11);
}

// 12 arguments --
template<typename C, typename M, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename R>
runnable_args_m_12_ret<C, M, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, R>* WrapRunnableRet(C o, M m, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8, A9 a9, A10 a10, A11 a11, R* r) {
  return new runnable_args_m_12_ret<C, M, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, R>
    (o, m, a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, r);
}

// 13 arguments --
template<typename M, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12>
runnable_args_nm_13<M, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12>* WrapRunnableNM(M m, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8, A9 a9, A10 a10, A11 a11, A12 a12) {
  return new runnable_args_nm_13<M, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12>
    (m, a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12);
}

// 13 arguments --
template<typename M, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename R>
runnable_args_nm_13_ret<M, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, R>* WrapRunnableNMRet(M m, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8, A9 a9, A10 a10, A11 a11, A12 a12, R* r) {
  return new runnable_args_nm_13_ret<M, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, R>
    (m, a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, r);
}

// 13 arguments --
template<typename C, typename M, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12>
runnable_args_m_13<C, M, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12>* WrapRunnable(C o, M m, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8, A9 a9, A10 a10, A11 a11, A12 a12) {
  return new runnable_args_m_13<C, M, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12>
    (o, m, a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12);
}

// 13 arguments --
template<typename C, typename M, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename R>
runnable_args_m_13_ret<C, M, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, R>* WrapRunnableRet(C o, M m, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8, A9 a9, A10 a10, A11 a11, A12 a12, R* r) {
  return new runnable_args_m_13_ret<C, M, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, R>
    (o, m, a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, r);
}

// 14 arguments --
template<typename M, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13>
runnable_args_nm_14<M, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13>* WrapRunnableNM(M m, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8, A9 a9, A10 a10, A11 a11, A12 a12, A13 a13) {
  return new runnable_args_nm_14<M, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13>
    (m, a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13);
}

// 14 arguments --
template<typename M, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename R>
runnable_args_nm_14_ret<M, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, R>* WrapRunnableNMRet(M m, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8, A9 a9, A10 a10, A11 a11, A12 a12, A13 a13, R* r) {
  return new runnable_args_nm_14_ret<M, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, R>
    (m, a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, r);
}

// 14 arguments --
template<typename C, typename M, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13>
runnable_args_m_14<C, M, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13>* WrapRunnable(C o, M m, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8, A9 a9, A10 a10, A11 a11, A12 a12, A13 a13) {
  return new runnable_args_m_14<C, M, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13>
    (o, m, a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13);
}

// 14 arguments --
template<typename C, typename M, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename R>
runnable_args_m_14_ret<C, M, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, R>* WrapRunnableRet(C o, M m, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8, A9 a9, A10 a10, A11 a11, A12 a12, A13 a13, R* r) {
  return new runnable_args_m_14_ret<C, M, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, R>
    (o, m, a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, r);
}

