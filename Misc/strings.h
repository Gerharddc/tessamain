#ifndef STRINGS
#define STRINGS

#include <string>
#include <stdio.h>

template <typename... Ts>
std::string format_string(const std::string &fmt, Ts... vs)
{
    char b;
    unsigned required = snprintf(&b, 0, fmt.c_str(), vs...) + 1;
    // the +1 is necessary, while the first parameter
    // can also be set to nullptr

    char bytes[required];
    snprintf(bytes, required, fmt.c_str(), vs...);

    return std::string(bytes);
}

#endif // STRINGS

