/**
 * make_unique.h
 */
#ifndef UTIL_BITS_MAKE_UNIQUE_H_
#define UTIL_BITS_MAKE_UNIQUE_H_

#include <cassert>
#include <type_traits>

namespace conc11 {

/**
 * "Reimplement" C++14 make_unique for C++11.
 */

template<class>
struct is_array_known_bound : std::false_type {
};

template<class T, size_t Size>
struct is_array_known_bound<T[Size]> : std::true_type {
};

template<class T, class ... Args>
inline auto make_unique(Args&&... args)
 -> typename std::enable_if<!std::is_array<T>::value, std::unique_ptr<T>>::type {
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

template<class T>
inline auto make_unique(std::size_t size)
 -> typename std::enable_if<std::is_array<T>::value && !is_array_known_bound<T>::value,
                            std::unique_ptr<T>>::type {
    return std::unique_ptr<T>(new typename std::remove_extent<T>::type[size]);
}

template<class T, class ... Args>
inline auto make_unique(Args&&... args)
 -> typename std::enable_if<is_array_known_bound<T>::value, void>::type = delete;

} // namespace conc11

#endif /* UTIL_BITS_MAKE_UNIQUE_H_ */
