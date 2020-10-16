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

    typedef R (*invoke_fn_t)(storage_t *, Args...);

    typedef void (*copy_fn_t)(storage_t *, const storage_t *);

    typedef void (*move_fn_t)(storage_t *, storage_t *);

    typedef void (*destroy_fn_t)(storage_t *);

    invoke_fn_t invoke;
    copy_fn_t copy;
    move_fn_t move;
    destroy_fn_t destroy;
};

template<typename R, typename... Args>
struct storage {
    fn_storage_t obj;
    methods<R, Args...> const *methods;

    storage() = default;

    storage(const storage &other) {
        other.methods->copy(this, &other);
    }

    storage(storage &&other) noexcept {
        other.methods->move(this, &other);
    }

    storage &operator=(const storage &rhs) {
        if (this != &rhs) {
            storage(rhs).swap(*this);
        }
        return *this;
    }

    void swap(storage &other) noexcept {
        std::swap(methods, other.methods);
        std::swap(obj, other.obj);
    }

    storage &operator=(storage &&rhs) noexcept {
        if (this != &rhs) {
            methods->destroy(this);
            rhs.methods->move(this, &rhs);
        }
        return *this;
    }

    ~storage() {
        methods->destroy(this);
    }
};

template<typename R, typename... Args>
const methods<R, Args...> *get_empty_methods() {
    typedef storage<R, Args...> storage_t;

    static constexpr methods<R, Args...> table{
            [](storage_t *, Args...) -> R {
                throw bad_function_call();
            },

            [](storage_t *to, const storage_t *from) {
                to->methods = from->methods;
            },

            [](storage_t *to, storage_t *from) {
                to->methods = from->methods;
            },

            [](storage_t *) {}
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
                [](storage_t *stg, Args... args) -> R {
                    return (*reinterpret_cast<T *&>(stg->obj))(std::forward<Args>(args)...);
                },

                [](storage_t *to, const storage_t *from) {
                    reinterpret_cast<void *&>(to->obj) = new T(*reinterpret_cast<T *const &>(from->obj));
                    to->methods = from->methods;
                },

                [](storage_t *to, storage_t *from) {
                    to->obj = from->obj;
                    to->methods = from->methods;
                    from->methods = get_empty_methods<R, Args...>();
                },

                [](storage_t *stg) {
                    delete (reinterpret_cast<T *&>(stg->obj));
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
                [](storage_t *stg, Args... args) -> R {
                    return (reinterpret_cast<T &>((stg->obj)))(std::forward<Args>(args)...);
                },

                [](storage_t *to, const storage_t *from) {
                    new(&to->obj) T(reinterpret_cast<const T &>(from->obj));
                    to->methods = from->methods;
                },

                [](storage_t *to, storage_t *from) {
                    to->obj = std::move(from->obj);
                    to->methods = from->methods;
                },

                [](storage_t *stg) {
                    reinterpret_cast<T &>(stg->obj).~T();
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

    function(function const &other) = default;

    function(function &&other) noexcept = default;

    template<typename T>
    function(T val) {
        if (is_small_v<T>) {
            new(&stg.obj) T(std::move(val));
        } else {
            reinterpret_cast<void *&>(stg.obj) = new T(val);
        }
        stg.methods = object_traits<T, is_small_v<T>>::template get_methods<R, Args...>();
    }

    function &operator=(function const &rhs) = default;

    function &operator=(function &&rhs) noexcept = default;

    ~function() = default;

    R operator()(Args... args) {
        return stg.methods->invoke(&stg, std::forward<Args>(args)...);
    }

    [[nodiscard]] explicit operator bool() const noexcept {
        return stg.methods != get_empty_methods<R, Args...>();
    }

    template<typename T>
    T *target() noexcept {
        if (object_traits<T, is_small_v<T>>().template get_methods<R, Args...>() == stg.methods) {
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
    [[nodiscard]] const T *target() const noexcept {
        if (object_traits<T, is_small_v<T>>().template get_methods<R, Args...>() == stg.methods) {
            if (is_small_v<T>) {
                return reinterpret_cast<const T *>(&stg.obj);
            } else {
                return reinterpret_cast<const T *const &>(stg.obj);
            }
        } else {
            return nullptr;
        }
    }
};
