#ifndef ERR_H
#define ERR_H

#include <string> 

// Print information about an error and exit with 1
void fatal(const std::string &msg);

// Print information about a system error and exit with 1
void syserr(const std::string &msg);

// Print information about a warning and continue
void warn(const std::string &msg);

// Print information about a system warning and continue
void syswarn(const std::string &msg);

#endif //ERR_H