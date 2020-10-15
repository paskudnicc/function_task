#pragma once

#include <type_traits>
#include <exception>

template<typename T>
constexpr bool is_small_v = sizeof(T) <= sizeof(void *)
                            && (alignof(void *) % alignof(T) == 0);

struct bad_function_call : std::exception {
    [[nodiscard]] const char *what() const noexcept override {
        return "empty function call";
    }
};

typedef std::aligned_storage<sizeof(void *), alignof(void *)>::type fn_storage_t;

template<typename R, typename... Args>
struct storage;

template<typename R, typename... Args>
struct methods {
    typedef storage<R, Args...> storage_t;

    typedef R (*invoke_fn_t)(fn_storage_t *, Args...);

    typedef void (*copy_fn_t)(storage_t *, const storage_t *);

    typedef void (*move_fn_t)(storage_t *, storage_t *);

    typedef void (*destroy_fn_t)(fn_storage_t *);

    invoke_fn_t invoke;
    copy_fn_t copy;
    move_fn_t move;
    destroy_fn_t destroy;
};

template<typename R, typename... Args>
struct storage {
    fn_storage_t obj;

    methods<R, Args...> const *methods;
};

template<typename R, typename... Args>
const methods<R, Args...> *get_empty_methods() {
    typedef storage<R, Args...> storage_t;

    static constexpr methods<R, Args...> table{
            [](fn_storage_t *, Args...) -> R {
                throw bad_function_call();
            },

            [](storage_t *to, const storage_t *from) {
                to->methods = from->methods;
            },

            [](storage_t *to, storage_t *from) {
                to->methods = from->methods;
            },

            [](fn_storage_t *) {}
    };

    return &table;
}

template<typename T, bool isSmall>
struct object_traits;

template<typename T>
struct object_traits<T, false> {
    template<typename R, typename... Args>
    static methods<R, Args...> const *get_methods() {
        typedef storage<R, Args...> storage_t;

        static constexpr methods<R, Args...> table{
                [](fn_storage_t *obj, Args... args) -> R {
                    return (*reinterpret_cast<const T *>(obj))(std::forward<Args>(args)...);
                },

                [](storage_t *to, const storage_t *from) {
                    reinterpret_cast<void *&>(to->obj) = new T(*reinterpret_cast<const T *>(&from->obj));
                    to->methods = from->methods;
                },

                [](storage_t *to, storage_t *from) {
                    reinterpret_cast<void *&>(to->obj) = reinterpret_cast<T *>(&from->obj);
                    reinterpret_cast<void *&>(from->obj) = nullptr;
                    to->methods = from->methods;
                    from->methods = get_empty_methods<R, Args...>();
                },

                [](fn_storage_t *obj) {
                    delete (reinterpret_cast<T *>(obj));
                }
        };

        return &table;
    }
};

template<typename T>
struct object_traits<T, true> {
    template<typename R, typename... Args>
    static methods<R, Args...> const *get_methods() {
        typedef storage<R, Args...> storage_t;

        static constexpr methods<R, Args...> table{
                [](fn_storage_t *obj, Args... args) -> R {
                    return (reinterpret_cast<T &>(*obj))(std::forward<Args>(args)...);
                },

                [](storage_t *to, const storage_t *from) {
                    new(&to->obj) T(reinterpret_cast<const T &>(from->obj));
                    to->methods = from->methods;
                },

                [](storage_t *to, storage_t *from) {
                    new(&to->obj) T(std::move(reinterpret_cast<T &>(from->obj)));
                    to->methods = from->methods;
                    from->methods = get_empty_methods<R, Args...>();
                },

                [](fn_storage_t *obj) {
                    reinterpret_cast<T &>(*obj).~T();
                }
        };

        return &table;
    }

};

template<typename F>
struct function;

template<typename R, typename... Args>
struct function<R(Args...)> {
private:
    storage<R, Args...> stg;
public:

    function() noexcept {
        stg.methods = get_empty_methods<R, Args...>();
    }

    function(function const &other) {
        stg.methods->copy(&stg, &other.stg);
    }

    function(function &&other) noexcept {
        stg.methods->move(&stg, &other.stg);
    }

    template<typename T>
    function(T val) {
        if (is_small_v<T>) {
            new(&stg.obj) T(std::move(val));
        } else {
            reinterpret_cast<void *&>(stg.obj) = new T(std::move(val));
        }
        stg.methods = object_traits<T, is_small_v<T>>::template get_methods<R, Args...>();
    }

    void swap(function &other) noexcept {
        std::swap(stg.obj, other.stg.obj);
        std::swap(stg.methods, other.stg.methods);
    }

    function &operator=(function const &rhs) {
        if (this != &rhs) {
            function(rhs).swap(*this);
        }
        return *this;
    }

    function &operator=(function &&rhs) noexcept {
        if (this != &rhs) {
            function(std::move(rhs)).swap(*this);
        }
        return *this;
    }

    ~function() {
        stg.methods->destroy(&stg.obj);
    }

    R operator()(Args... args) {
        return stg.methods->invoke(&stg.obj, std::forward<Args>(args)...);
    }

    explicit operator bool() const noexcept {
        return stg.methods != get_empty_methods<R, Args...>();
    }

    template<typename T>
    T *target() noexcept {
        if (stg.methods == object_traits<T, is_small_v<T>>().template get_methods<R, Args...>()) {
            if (is_small_v<T>) {
                return reinterpret_cast<T *>(&stg.obj);
            } else {
                return reinterpret_cast<T *&>(stg.obj);
            }
        } else {
            return nullptr;
        }
    }

    template<typename T>
    const T *target() const noexcept {
        if (stg.methods == object_traits<T, is_small_v<T>>().template get_methods<R, Args...>()) {
            if (is_small_v<T>) {
                return reinterpret_cast<const T *>(&stg.obj);
            } else {
                return reinterpret_cast<const T * const &>(stg.obj);
            }
        } else {
            return nullptr;
        }
    }
};
