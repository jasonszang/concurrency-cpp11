/**
 * scope_guard.h
 */
#ifndef UTIL_BITS_SCOPE_GUARD_H_
#define UTIL_BITS_SCOPE_GUARD_H_

#include <cassert>

namespace conc11 {

namespace detail {

class ScopeGuardBase {
public:
    void release() const {
        released = true;
    }

protected:
    ScopeGuardBase() {
    }

    ScopeGuardBase(const ScopeGuardBase&) = delete;
    ScopeGuardBase& operator=(const ScopeGuardBase&) = delete;

    ScopeGuardBase(ScopeGuardBase&& rhs) noexcept:released(rhs.released) {
        rhs.released = true;
    }

    ScopeGuardBase& operator=(ScopeGuardBase&& rhs) noexcept {
        assert(this != &rhs);
        released = rhs.released;
        rhs.released = true;
        return *this;
    }

    ~ScopeGuardBase() { // Intentionally made non-virtual. Don't use ScopeGuardBase* directly.
    }

    bool is_released() const {
        return released;
    }

private:
    mutable bool released = false;
};

template<class Pre, class Post>
class ScopeGuardImpl : public ScopeGuardBase {
public:
    ScopeGuardImpl(Post&& post) :
            post(std::forward<Post>(post)) {
    }

    ScopeGuardImpl(Pre&& pre, Post&& post) :
            post(std::forward<Post>(post)) {
        pre();
    }

    ScopeGuardImpl(const ScopeGuardImpl&) = delete;
    ScopeGuardImpl& operator=(const ScopeGuardImpl&) = delete;

    ScopeGuardImpl(ScopeGuardImpl&&) = default;
    ScopeGuardImpl& operator=(ScopeGuardImpl&&) = default;

    ~ScopeGuardImpl() {
        if (!is_released()) {
            post();
        }
    }

private:
    Post post;
};

} // namespace detail

typedef const detail::ScopeGuardBase& ScopeGuard;
struct placeholder_t {
};

template<class Post>
detail::ScopeGuardImpl<placeholder_t, Post> make_scope_guard(Post&& post) {
    return detail::ScopeGuardImpl<placeholder_t, Post>(std::forward<Post>(post));
}

template<class Pre, class Post>
detail::ScopeGuardImpl<Pre, Post> make_scope_guard(Pre&& pre, Post&& post) {
    return detail::ScopeGuardImpl<Pre, Post>(std::forward<Pre>(pre), std::forward<Post>(post));
}

} // namespace conc11

#endif /* UTIL_BITS_SCOPE_GUARD_H_ */
