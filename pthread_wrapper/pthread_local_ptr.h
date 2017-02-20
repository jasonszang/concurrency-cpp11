/**
 * pthread_specific_ptr.h
 */
#ifndef PTHREAD_WRAPPER_PTHREAD_LOCAL_PTR_H_
#define PTHREAD_WRAPPER_PTHREAD_LOCAL_PTR_H_

#include <pthread.h>
#include <functional>
#if __cplusplus >= 201103L
#include <type_traits>
#endif

namespace conc11 {

/**
 * A C++ wrapper utility for pthread_specific-based thread local storage that will make your life
 * a little easier with pre-C++11 thread local.
 *
 * Usage: Use this wrapper as static, global, file scope static or class static member, then use
 * operator* and operator-> to dereference like a pointer. You may also call get() to acquire the
 * pointer directly. The first get() or dereferencing will result in the allocation of a thread
 * specific object of type T. For pre-C++11 T must be DefaultConstructible.
 * With C++11 we have thread_local, but if you insist on trying this you will get a fancier
 * constructor that will take parameters of one of the constructers of T, and all thread specific
 * instances will be initialized with the parameters.
 * Other points to note:
 * 1) DO NOT pass reference wrappers to objects that might go out of scope before any further
 * thread creation, or it will become a dangling reference.
 * 2) The type of the parameters must EXACTLY match one of T's constructors.
 * 3) T's constructors that use rvalue references cannot be used
 */
template<class T>
class PThreadLocalPtr {
public:

#if __cplusplus < 201103L
    explicit PThreadLocalPtr():valid(false) {
#else
    template<class ... Args>
    explicit PThreadLocalPtr(Args&&... args):valid(false) {
#endif
        int ret;
        ret = pthread_key_create(&key, &PThreadLocalPtr<T>::deleter);
        if (ret == 0) {
            valid = true;
        }
#if __cplusplus >= 201103L
        initializer_func = std::bind(&PThreadLocalPtr<T>::initializer<Args...>,
                                     std::forward<Args>(args)...);
#endif
    }

#if __cplusplus >= 201103L
    PThreadLocalPtr(const PThreadLocalPtr &) = delete;
    PThreadLocalPtr &operator=(const PThreadLocalPtr &) = delete;
#else
    PThreadLocalPtr(const PThreadLocalPtr &);
    PThreadLocalPtr &operator=(const PThreadLocalPtr &);
#endif

    ~PThreadLocalPtr() {
        pthread_key_delete(key);
    }

    bool is_valid() const {
        return valid;
    }

    T* get() {
        T* t = static_cast<T*>(pthread_getspecific(key));
        if (t) {
            return t;
        }
#if __cplusplus >= 201103L
        t = initializer_func();
#else
        t = new T();
#endif
        pthread_setspecific(key, t);
        return t;
    }

#if __cplusplus >= 201103L
    typename std::add_lvalue_reference<T>::type operator *() {
#else
    T& operator *() {
#endif
        return *get();
    }

    T* operator->() {
        return get();
    }

private:

#if __cplusplus >= 201103L
    template<class ... Args>
    static T* initializer(Args... args) {
        return new T(args...);
    }
#endif

    static void deleter(void *t) {
        delete static_cast<T*>(t);
    }

    pthread_key_t key;
    bool valid;
#if __cplusplus >= 201103L
    std::function<T*()> initializer_func;
#endif
};

} // namespace conc11

#endif /* PTHREAD_WRAPPER_PTHREAD_LOCAL_PTR_H_ */
