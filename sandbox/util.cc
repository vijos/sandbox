#include "sandbox/util.h"

#include <array>
#include <fstream>
#include <fcntl.h>
#include <sys/mount.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

namespace sandbox {

bool write_file(const std::string &filename, const std::string &content) {
    std::ofstream stream(filename, std::ios::out | std::ios::binary);
    stream.write(content.data(), content.size());
    stream.close();
    return stream.good();
}

bool bind_node(const std::string &from, const std::string &to) {
    if (mknod(to.c_str(), 0600, 0)) {
        return false;
    }
    if (mount(from.c_str(), to.c_str(), "", MS_BIND | MS_NOSUID, nullptr)) {
        return false;
    }
    return true;
}

bool bind_or_link(const std::string &from, const std::string &to) {
    struct stat stat;
    if (lstat(from.c_str(), &stat)) {
        return false;
    }
    std::array<char, 4096> buffer;
    if (S_ISLNK(stat.st_mode)) {
        ssize_t ret = readlink(from.c_str(), buffer.data(), buffer.size() - 1);
        if (ret < 0) {
            return false;
        }
        buffer[ret] = '\0';
        if (symlink(buffer.data(), to.c_str())) {
            return false;
        }
    } else {
        if (mkdir(to.c_str(), 0755)) {
            return false;
        }
        if (mount(from.c_str(), to.c_str(), "", MS_BIND | MS_NOSUID, nullptr)) {
            return false;
        }
        if (mount(from.c_str(), to.c_str(), "",
                  MS_BIND | MS_REMOUNT | MS_RDONLY | MS_NOSUID, nullptr)) {
            return false;
        }
    }
    return true;
}

int FdStreamBuf::underflow() {
    ssize_t ret = read(fd_, gbuf_.data(), gbuf_.size());
    if (ret <= 0) {
        return EOF;
    }
    setg(gbuf_.data(), gbuf_.data(), gbuf_.data() + ret);
    return gbuf_.front();
}

int FdStreamBuf::overflow(int c) {
    int ret = write(fd_, pbase(), pptr() - pbase());
    preset();
    if (c != EOF) {
        *pbase() = static_cast<char>(c);
        pbump(1);
    }
    return ret >= 0 ? ret : EOF;
}

int FdStreamBuf::sync() {
    return overflow() == EOF ? -1 : 0;
}

}  // namespace sandbox
