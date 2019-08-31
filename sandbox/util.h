#ifndef _SANDBOX_UTIL_H
#define _SANDBOX_UTIL_H

#include <array>
#include <streambuf>
#include <string>

namespace sandbox {

bool write_file(const std::string &filename, const std::string &content);
bool bind_node(const std::string &from, const std::string &to);
bool bind_or_link(const std::string &from, const std::string &to);

class FdStreamBuf : public std::streambuf {
public:
    explicit FdStreamBuf(int fd = -1) : fd_(fd) { preset(); }
    int underflow() override;
    int overflow(int c = EOF) override;
    int sync() override;

    void fd(int value) { fd_ = value; }

private:
    void preset() { setp(pbuf_.data(), pbuf_.data() + pbuf_.size()); }

    int fd_;
    std::array<char, 4096> gbuf_;
    std::array<char, 4096> pbuf_;
};

}  // namespace sandbox

#endif  // _SANDBOX_UTIL_H
