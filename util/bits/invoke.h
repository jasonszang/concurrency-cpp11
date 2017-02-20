/**
 * invoke.h
 */
#ifndef UTIL_BITS_INVOKE_H_
#define UTIL_BITS_INVOKE_H_

#include <functional>
#include <type_traits>
#include "scope_guard.h"

namespace conc11 {

// "Reimplement" C++17 std::invoke
// DO NOT format this code with code formatters. They are just too stupid to format it correctly.
namespace detail {
template <class T>
struct is_reference_wrapper : std::false_type {};
template <class U>
struct is_reference_wrapper<std::reference_wrapper<U>> : std::true_type {};

template <class Base, class T, class Derived, class... Args>
auto _invoke(T Base::*pmf, Derived&& ref, Args&&... args)
    noexcept(noexcept((std::forward<Derived>(ref).*pmf)(std::forward<Args>(args)...)))
 -> typename std::enable_if<std::is_function<T>::value &&
                   std::is_base_of<Base, typename std::decay<Derived>::type>::value,
                   decltype((std::forward<Derived>(ref).*pmf)(std::forward<Args>(args)...))>
                         ::type {
    return (std::forward<Derived>(ref).*pmf)(std::forward<Args>(args)...);
}

template <class Base, class T, class RefWrap, class... Args>
auto _invoke(T Base::*pmf, RefWrap&& ref, Args&&... args)
    noexcept(noexcept((ref.get().*pmf)(std::forward<Args>(args)...)))
 -> typename std::enable_if<std::is_function<T>::value &&
                   is_reference_wrapper<typename std::decay<RefWrap>::type>::value,
                   decltype((ref.get().*pmf)(std::forward<Args>(args)...))>::type {
    return (ref.get().*pmf)(std::forward<Args>(args)...);
}

template <class Base, class T, class Pointer, class... Args>
auto _invoke(T Base::*pmf, Pointer&& ptr, Args&&... args)
    noexcept(noexcept(((*std::forward<Pointer>(ptr)).*pmf)(std::forward<Args>(args)...)))
 -> typename std::enable_if<std::is_function<T>::value &&
                   !is_reference_wrapper<typename std::decay<Pointer>::type>::value &&
                   !std::is_base_of<Base, typename std::decay<Pointer>::type>::value,
                   decltype(((*std::forward<Pointer>(ptr)).*pmf)(std::forward<Args>(args)...))>
                         ::type {
    return ((*std::forward<Pointer>(ptr)).*pmf)(std::forward<Args>(args)...);
}

template <class Base, class T, class Derived>
auto _invoke(T Base::*pmd, Derived&& ref)
    noexcept(noexcept(std::forward<Derived>(ref).*pmd))
 -> typename std::enable_if<!std::is_function<T>::value &&
                   std::is_base_of<Base, typename std::decay<Derived>::type>::value,
                   decltype(std::forward<Derived>(ref).*pmd)>::type {
    return std::forward<Derived>(ref).*pmd;
}

template <class Base, class T, class RefWrap>
auto _invoke(T Base::*pmd, RefWrap&& ref)
    noexcept(noexcept(ref.get().*pmd))
 -> typename std::enable_if<!std::is_function<T>::value &&
                   is_reference_wrapper<typename std::decay<RefWrap>::type>::value,
                   decltype(ref.get().*pmd)>::type {
    return ref.get().*pmd;
}

template <class Base, class T, class Pointer>
auto _invoke(T Base::*pmd, Pointer&& ptr)
    noexcept(noexcept((*std::forward<Pointer>(ptr)).*pmd))
 -> typename std::enable_if<!std::is_function<T>::value &&
                   !is_reference_wrapper<typename std::decay<Pointer>::type>::value &&
                   !std::is_base_of<Base, typename std::decay<Pointer>::type>::value,
                   decltype((*std::forward<Pointer>(ptr)).*pmd)>::type {
    return (*std::forward<Pointer>(ptr)).*pmd;
}

template <class F, class... Args>
auto _invoke(F&& f, Args&&... args)
    noexcept(noexcept(std::forward<F>(f)(std::forward<Args>(args)...)))
 -> typename std::enable_if<!std::is_member_pointer<typename std::decay<F>::type>::value,
                   decltype(std::forward<F>(f)(std::forward<Args>(args)...))>::type {
    return std::forward<F>(f)(std::forward<Args>(args)...);
}
} // namespace detail

/**
 * Invoke a callable. Should behave like C++17 std::invoke().
 */
template< class F, class... ArgTypes >
auto invoke(F&& f, ArgTypes&&... args)
    // exception specification for QoI
    noexcept(noexcept(detail::_invoke(std::forward<F>(f), std::forward<ArgTypes>(args)...)))
 -> decltype(detail::_invoke(std::forward<F>(f), std::forward<ArgTypes>(args)...)) {
    return detail::_invoke(std::forward<F>(f), std::forward<ArgTypes>(args)...);
}

// Some extra gadgets based on conc11::invoke()

namespace detail {
template<class Rep, class Period>
struct TimerEndFunctor{
    TimerEndFunctor(std::chrono::duration<Rep, Period>* dur) :
            begin(std::chrono::steady_clock::now()), dur(dur) {
    }
    void operator()() {
        std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
        *dur = std::chrono::duration_cast<std::chrono::duration<Rep, Period>>(
                            end - begin);
    }
    std::chrono::steady_clock::time_point begin;
    std::chrono::duration<Rep, Period>* dur;
};


template<class Rep, class Period>
ScopeGuardImpl<placeholder_t, detail::TimerEndFunctor<Rep, Period>> make_time_guard(
        std::chrono::duration<Rep, Period> *time_elapsed) {
    return make_scope_guard(TimerEndFunctor<Rep, Period>(time_elapsed));
}

}  // namespace detail

/**
 * Invokes a callable and records its execution time. The first parameter is a pointer to a
 * std::chrono::duration for returning execution time, and the original return value of the
 * callable will be returned as the function return value. Second to last parameters match
 * those of C++17 std::invoke().
 */
template <class Rep, class Period, class Callable, class ...Args>
auto timed_invoke(std::chrono::duration<Rep, Period> *time_elapsed, Callable c, Args&& ...args)
    noexcept(noexcept(invoke(std::forward<Callable>(c), std::forward<Args>(args)...)))
 -> decltype(invoke(std::forward<Callable>(c), std::forward<Args>(args)...)) {
    ScopeGuard time_guard = detail::make_time_guard(time_elapsed);
    return invoke(std::forward<Callable>(c), std::forward<Args>(args)...);
}

} // namespace conc11

#endif /* UTIL_BITS_INVOKE_H_ */
