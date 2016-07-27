#ifndef ANTI_GCC_UTILS_HPP
#define ANTI_GCC_UTILS_HPP

#include <memory>
#include <utility>

/**
 * GCC 4.8 does not support make_unique and has some issues with properly handling some of
 * basic_ios constructors. So here's workaround for that.
 */

template<typename T, typename...Args>
std::unique_ptr<T> make_unique(Args&&...args) {
    return std::unique_ptr<T>( new T(std::forward<Args>(args)...) );
}

template<typename T, typename...Args>
std::unique_ptr<std::ifstream> make_up(Args&&...args) {
   return make_unique<T>(std::forward<Args>(args)...);
}

#endif
