#ifndef UTIL_H
#define UTIL_H

#include <iostream>
#include <fstream>
#include <string>

#include <unistd.h>

#define throw_err(err_expr, err_append)                                                  \
  __throw_err(err_expr, err_append, __FUNCTION__, __LINE__)
void __throw_err(const std::string &err_expr,
                 const std::string &err_append,
                 const std::string &fnc_name,
                 int sc_line);

std::fstream file_open(const std::string &fname, std::ios::openmode m);
std::string file_read(std::fstream fd);

template <typename T>
void expr_format_print(const char *msg, const T &data, bool expr_result)
{
  std::cout << (expr_result ? "[ + ] " : "[ - ] ") << msg << data << std::endl;
}

#endif // UTIL_H
