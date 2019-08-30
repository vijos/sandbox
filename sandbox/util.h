#ifndef _SANDBOX_UTIL_H
#define _SANDBOX_UTIL_H

#include <string>

namespace sandbox {

bool write_file(const std::string &filename, const std::string &content);
bool bind_node(const std::string &from, const std::string &to);
bool bind_or_link(const std::string &from, const std::string &to);

}  // namespace sandbox

#endif  // _SANDBOX_UTIL_H
