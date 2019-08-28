#ifndef _SANDBOX_TYPES_H
#define _SANDBOX_TYPES_H

#include <string>

namespace sandbox {

enum class MountType {
    in,
    out,
    bind_or_link,
};

struct Mount {
    MountType type;
    std::string from;
    std::string to;
};

}  // namespace sandbox

#endif  // _SANDBOX_TYPES_H
