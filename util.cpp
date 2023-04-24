#include <cerrno>
#include <cstring>
#include <sstream>
#include <iostream>
#include <filesystem>
#include "util.hpp"

void
__throw_err(const std::string &err_expr,
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

void
usage(std::initializer_list<std::string> example, std::initializer_list<std::string> help)
{
    std::ostream &os = std::cerr;

    os << "Usage:";
    for (auto &e_arg : example) {
        os << ' ' << e_arg;
    }

    os << std::endl;

    os << "Help:" << std::endl;
    for (auto &e_opt : help) {
        os << ' ' << e_opt << std::endl;
    }

    std::exit(EXIT_FAILURE);
}

std::string
FilePathOnFS(const std::string &dir, const std::string &base)
{
    std::filesystem::path p;
    p = std::filesystem::path(base);
    p = std::filesystem::path(dir) /= p.relative_path();

    return p.make_preferred().string();
};

std::fstream
FileOpen(const std::string &fname, std::ios::openmode m)
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

std::string
FileRead(std::fstream fd)
{
    const size_t sz = fd.seekg(0, std::ios::end).tellg();
    fd.seekg(std::ios::beg);

    std::string buf(sz, '\0');

    if (!fd.read(buf.data(), buf.size())) {
        throw_err("Read file", std::to_string(buf.size()));
    }

    return buf;
}

std::string
FileRead(const std::string &fname, std::ios::openmode m)
{
    return FileRead(FileOpen(fname, m));
}
