#pragma once

// std::bit_cast from C++20 is supported by Clang libc++ v14 and GCC libstdc++ v11
#if defined(__cpp_lib_bit_cast)
#include <bit> // std::bit_cast
#else

// we need to define a backport of std::bit_cast
// https://stackoverflow.com/a/67539748
namespace std {
template <class T2, class T1> T2 bit_cast(T1 t1) {
  static_assert(sizeof(T1) == sizeof(T2), "T1 and T2 must have the same size");
  static_assert(std::is_pod<T1>::value, "T1 must be a POD type");
  static_assert(std::is_pod<T2>::value, "T2 must be a POD type");

  T2 t2;
  std::memcpy(std::addressof(t2), std::addressof(t1), sizeof(T1));
  return t2;
}
} // namespace std

#endif
