#include <iostream>
#include <cstdlib>
#include <cerrno>
#include <string>
#include <cstring> // for strerror()

#include "err.h"

void fatal(const std::string &msg){
    std::cerr << "ERROR: " << msg << "\n";
    std::exit(1);
}

void syserr(const std::string &msg){
    std::cerr << "ERROR: " << msg << " (" << errno << "; " << strerror(errno) << ")\n";
    std::exit(1);
}

void warn(const std::string &msg){
    std::cerr << "WARNING: " << msg << "\n";
}

void syswarn(const std::string &msg){
    std::cerr << "WARNING: " << msg << " (" << errno << "; " << strerror(errno) << ")\n";
}