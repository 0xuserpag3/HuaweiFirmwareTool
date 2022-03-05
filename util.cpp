#include <filesystem>
#include <sstream>
#include <cstring>
#include <cerrno>

#include "util.h"

void __throw_err(const std::string &err_expr,
                 const std::string &err_append,
                 const std::string &fnc_name,
                 int sc_line)
{
  std::ostringstream errbuf;

  errbuf << '[' << fnc_name << ':' << sc_line << ']';
  errbuf << " - ";
  errbuf << '[' << err_expr << "]:[" << err_append << ']';
  if (errno) {
    errbuf << " - " << std::strerror(errno);
  }

  throw std::runtime_error(errbuf.str());
}

std::fstream file_open(const std::string &fname, std::ios::openmode m)
{
  std::fstream fd(fname, m);

  if (!fd.is_open()) {
    throw_err("!fd.is_open()", fname);
  }

  if (!std::filesystem::is_regular_file(fname)) {
    throw_err("!is_regular_file(fname)", fname);
  }

  return fd;
}

std::string file_read(std::fstream fd)
{
  const size_t sz = fd.seekg(0, std::ios::end).tellg();
  fd.seekg(std::ios::beg);

  std::string buf(sz, '\0');
  if (!fd.read(buf.data(), buf.size())) {
    throw_err("Read file", std::to_string(buf.size()));
  }

  return buf;
}
