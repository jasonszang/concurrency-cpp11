/*
 * in_memory_lru.h
 */
#ifndef UTIL_IN_MEMORY_LRU_H_
#define UTIL_IN_MEMORY_LRU_H_

#include <cassert>
#include <string>
#include <unordered_map>

/**
 * An in memory LRU caching container for storing key-value pairs. Value type is required to be
 * Assignable.
 */
template<class TK, class TV, class Hash = std::hash<TK>, class Eql = std::equal_to<TK>>
class InMemoryLru {
public:

    InMemoryLru(size_t capacity) :
            mem(), capacity(capacity), lru_list_head(nullptr), lru_list_tail(nullptr) {
    }

    ~InMemoryLru() {
    }

    /**
     * Store or update a key-value pair.
     */
    template<class UK, class UV,
            class = typename std::enable_if<std::is_convertible<UK, TK>::value>::type,
            class = typename std::enable_if<std::is_convertible<UV, TV>::value>::type>
    void set(UK&& key, UV&& value) {
        auto iter = mem.find(key);
        if (iter == mem.end()) {
            auto ret = mem.emplace(std::piecewise_construct,
                    std::forward_as_tuple(std::forward<UK>(key)),
                    std::forward_as_tuple(std::forward<UV>(value)));
            auto new_pair_iter = ret.first;
            list_insert_head(&(*new_pair_iter));
            if (mem.size() > capacity) {
                gc1();
            }
        } else {
            iter->second.value = std::forward<UV>(value);
            list_move_to_head(&(*iter));
        }
    }

    /**
     * Returns pointer to the stored value, or nullptr if key not found.
     * The returned pointer may become invalidated after a set, erase or clear operation and
     * should not be stored.
     */
    TV* get(const TK& key) {
        auto iter = mem.find(key);
        if (iter == mem.end()) {
            return nullptr;
        }
        list_move_to_head(&(*iter));
        return &(iter->second.value);
    }

    /**
     * Returns true and return a copy of the stored value var copy-assigning *out_value, or
     * return false if key not found.
     */
    bool get_copy(const TK& key, TV* out_value) {
        auto iter = mem.find(key);
        if (iter == mem.end()) {
            return false;
        }
        list_move_to_head(&(*iter));
        *out_value = iter->second.value;
        return true;
    }

    /**
     * If key is found, returns true and return the stored value var move-assigning *out_value,
     * then erase key from the cache. Return false if key not found.
     */
    bool get_move(const TK& key, TV* out_value) {
        auto iter = mem.find(key);
        if (iter == mem.end()) {
            return false;
        }
        *out_value = std::move(iter->second.value);
        list_release(&(*iter));
        mem.erase(iter);
        return true;
    }

    /**
     * If key is found, returns true and return the object pointed by value, a.k.a. (*value).
     * Returns false if key not found. This variant of get method can be used with smart pointers
     * like unique_ptr and make copies of pointees without having to copy the smart pointer.
     */
    template<class TV_ = TV>
    bool get_copy_pointee(const TK& key,
                          typename std::remove_reference<
                                  decltype(*(std::declval<TV_>()))
                          >::type* out_value_pointee) {
        auto iter = mem.find(key);
        if (iter == mem.end()) {
            return false;
        }
        list_move_to_head(&(*iter));
        *out_value_pointee = *(iter->second.value);
        return true;
    }

    /**
     * Erase a key and its corresponding value from the cache. Return false if key not found.
     */
    bool erase(const TK& key) {
        auto iter = mem.find(key);
        if (iter == mem.end()) {
            return false;
        }
        PairType* p = &(*iter);
        list_release(p);
        mem.erase(iter);
        return true;
    }

    /**
     * Clear cache.
     */
    void clear() {
        mem.clear();
        lru_list_head = nullptr;
        lru_list_tail = nullptr;
    }

private:

    struct Entry;
    using PairType = std::pair<const TK, Entry>;

    struct Entry {
        TV value;
        PairType* parent;
        PairType* child;
        explicit Entry(const TV& t) :
                value(t), parent(nullptr), child(nullptr) {
        }
        explicit Entry(TV&& t) :
                value(std::move(t)), parent(nullptr), child(nullptr) {
        }
    };

    /**
     * Insert a node pointed to by e to the head of the lru list.
     * Undefined behaviour if e does not point to a new node not yet in the list.
     */
    void list_insert_head(PairType* e) {
        if (!lru_list_head) {
            lru_list_head = e;
            lru_list_tail = e;
            e->second.parent = nullptr;
            e->second.child = nullptr;
        } else {
            e->second.parent = nullptr;
            e->second.child = lru_list_head;
            lru_list_head->second.parent = e;
            lru_list_head = e;
        }
    }

    /**
     * Release the node pointed to by e from the lru list without destroying the node.
     * Undefined behaviour if e does not point to an existing node in the list.
     */
    void list_release(PairType* e) {
        if (e->second.parent == nullptr) {
            lru_list_head = e->second.child;
        }
        else {
            e->second.parent->second.child = e->second.child;
        }
        if (e->second.child == nullptr) {
            lru_list_tail = e->second.parent;
        }
        else {
            e->second.child->second.parent = e->second.parent;
        }
    }

    void list_move_to_head(PairType* e) {
        list_release(e);
        list_insert_head(e);
    }

    /**
     * Delete the least recently used key-value pair.
     */
    void gc1() {
        assert(lru_list_tail != nullptr);
        PairType* e = lru_list_tail;
        list_release(e);
        mem.erase(e->first);
    }

    std::unordered_map<TK, Entry, Hash, Eql> mem;
    const uint32_t capacity;
    PairType* lru_list_head;
    PairType* lru_list_tail;
};

#endif /* UTIL_IN_MEMORY_LRU_H_ */
