#ifndef _SANDBOX_UTIL_H
#define _SANDBOX_UTIL_H

#include <string>

namespace sandbox {

bool write_file(const std::string &filename, const std::string &content);

}  // namespace sandbox

#endif  // _SANDBOX_UTIL_H
