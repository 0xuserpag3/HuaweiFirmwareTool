#ifndef UTIL_H
#define UTIL_H

#include <string>
#include <fstream>
#include <unistd.h>

#define throw_err(err_expr, err_append)                                                  \
    __throw_err(err_expr, err_append, __FUNCTION__, __LINE__)
void __throw_err(const std::string &err_expr,
                 const std::string &err_append,
                 const std::string &fnc_name,
                 int sc_line);

constexpr inline uint16_t
BSWAP16(uint16_t x)
{
    return ((x & 0xFF00) >> 8) | ((x & 0x00FF) << 8);
}

constexpr inline uint32_t
BSWAP32(uint32_t x)
{
    return (((x & 0xFF000000u) >> 24) | ((x & 0x00FF0000u) >> 8) |
            ((x & 0x0000FF00u) << 8) | ((x & 0x000000FFu) << 24));
}

void usage(std::initializer_list<std::string> example,
           std::initializer_list<std::string> help);

std::string FilePathOnFS(const std::string &dir, const std::string &base);
std::fstream FileOpen(const std::string &fname, std::ios::openmode m);
std::string FileRead(std::fstream fd);
std::string FileRead(const std::string &fname, std::ios::openmode m);

#endif // UTIL_H
