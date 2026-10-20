// SPDX-License-Identifier: Apache-2.0

#include <unistd.h>

#include <atomic>
#include <cerrno>

std::atomic<bool> mockWriteShutdownOnce{false};

extern "C" ssize_t __real_write(int fd, const void* buffer, size_t count);

extern "C" ssize_t __wrap_write(int fd, const void* buffer, size_t count)
{
    if (mockWriteShutdownOnce.exchange(false))
    {
        errno = ESHUTDOWN;
        return -1;
    }

    return __real_write(fd, buffer, count);
}
