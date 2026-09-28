#pragma once

#include <atomic>
#include <filesystem>
#include <string>

namespace ikvm
{

extern std::atomic<bool> scrnshotFlag;
extern std::atomic<bool> videoRecFlag;
extern std::atomic<bool> recThreadStatus;
extern std::atomic<bool> InitFlag;
extern const std::string bsodAsJpeg;
extern std::string hostPowerState;
extern const char* NO_SIGNAL_IMG_PATH;
extern const char* POWER_OFF_IMG_PATH;

inline void createUtilities() {}
inline void detectKvmInstance(const std::string&) {}

} // namespace ikvm
