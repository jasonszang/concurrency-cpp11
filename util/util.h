/**
 * util.h
 */

#ifndef UTIL_UTIL_H_
#define UTIL_UTIL_H_

#include <cassert>
#include <functional>
#include <type_traits>

namespace conc11 {

template<typename T, typename ... Args>
inline std::unique_ptr<T> make_unique(Args&&... args) {
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

} // namespace conc11

#include "bits/rvalue_wrapper.h"
#include "bits/scope_guard.h"
#include "bits/invoke.h"

#endif /* UTIL_UTIL_H_ */
