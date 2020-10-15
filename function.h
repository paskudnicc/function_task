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

template<typename R, typename... Args>
struct methods {
    typedef R (*invoke_fn_t)(void *, Args...);

    typedef void (*modify_fn_t)(void *, void *);

    typedef void (*destroy_fn_t)(void *);

    invoke_fn_t invoke;
    modify_fn_t copy;
    modify_fn_t move;
    destroy_fn_t destroy;
};

template<typename R, typename... Args>
struct storage {
    typename std::aligned_storage<sizeof(void *), alignof(void *)>::type obj;

    methods<R, Args...> const *methods;
};

template<typename R, typename... Args>
methods<R, Args...> const *get_empty_methods() {
    static constexpr methods<R, Args...> table{
            [](void *, Args...) -> R {
                throw bad_function_call();
            },

            [](void *to, void *from) {
                from->
            },

            [](void *to, void *from) {
                throw bad_function_call();
            },

            [](void *) {
            },
    };

    return &table;
}

template<typename T, bool isSmall>
struct object_traits;

object

template<typename T>
struct object_traits<T, false> {
    template<typename R, typename... Args>
    static methods<R, Args...> const *get_methods() {
        static constexpr methods<R, Args...> table{
                [](void *obj, Args... args) -> R {
                    return (*static_cast<T *>(obj))(args...);
                }
        };

        return &table;
    }
};

template<typename T>
struct object_traits<T, true> {
    template<typename R, typename... Args>
    static methods<R, Args...> const *get_methods() {
        static constexpr methods<R, Args...> table{
                [](void *obj, Args... args) -> R {
                    return (*static_cast<T *>(obj))(args...);
                }
        };

        return &table;
    }
};

template<typename F>
struct function;

template<typename R, typename... Args>
class function<R(Args...)> {
    storage<R, Args...> stg;
public:

    function() noexcept {
        stg.methods = get_empty_methods<R, Args...>();
    }

    function(function const &other) = default;

    function(function &&other) noexcept = default;

    template<typename T>
    explicit function(T val) {
        // T small ???
        stg.obj = new T(std::move(val));
        stg.methods = object_traits<T, is_small_v<T>>::template get_methods<R, Args...>();
    }

    function &operator=(function const &rhs) = default;

    function &operator=(function &&rhs) noexcept = default;

    ~function() = default;

    R operator()(Args... args) {
        return stg.methods->invoke(stg.obj, std::forward<Args>(args)...);
    }

    explicit operator bool() const noexcept {
        return stg.methods != get_empty_methods<R, Args...>();
    }

    template<typename T>
    T *target() noexcept {

    }

    template<typename T>
    T const *target() const noexcept;
};
