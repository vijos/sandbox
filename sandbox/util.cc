#include "sandbox/util.h"

#include <fstream>

namespace sandbox {

bool write_file(const std::string &filename, const std::string &content) {
    std::ofstream stream(filename, std::ios::out | std::ios::binary);
    stream.write(content.data(), content.size());
    stream.close();
    return stream.good();
}

}  // namespace sandbox
