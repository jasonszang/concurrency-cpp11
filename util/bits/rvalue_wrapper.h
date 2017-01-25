/**
 * rvalue_wrapper.h
 */
#ifndef UTIL_BITS_RVALUE_WRAPPER_H_
#define UTIL_BITS_RVALUE_WRAPPER_H_

namespace conc11 {

/**
 * A wrapper for holding and passing rvalues through std::bind. The rvalue wrapped will be stored
 * inside the wrapper until the functor returned by std::bind is invoked, at when the stored rvalue
 * will be moved from. The functor can be invoked ONLY ONCE.
 */
template<typename T>
struct RValueWrapper {
public:
    template<typename = typename std::enable_if<std::is_rvalue_reference<T&&>::value>::type>
    explicit RValueWrapper(T &&t) noexcept :
    t(std::move(t)) {
    }
    RValueWrapper(const RValueWrapper &rhs) = default;
    RValueWrapper(RValueWrapper &&rhs) = default;
    RValueWrapper& operator=(const RValueWrapper &rhs) = default;
    RValueWrapper& operator=(RValueWrapper &&rhs) = default;

    template <class ...Args>
    T &&operator()(Args ...) {
        return std::move(t);
    }
private:
    T t;
};

template<typename T,
        typename = typename std::enable_if<std::is_rvalue_reference<T&&>::value>::type>
RValueWrapper<T> rval(T &&t) {
    return RValueWrapper<T>(std::move(t));
}

template<typename T>
RValueWrapper<T> rval(T &t) {
    return RValueWrapper<T>(T(t));
}

} // namespace conc11

namespace std {
template<typename T>
struct is_bind_expression<conc11::RValueWrapper<T>> : std::true_type {
};
}

#endif /* UTIL_BITS_RVALUE_WRAPPER_H_ */
