#include "ami/include/ikvm_utils.hpp"

namespace ikvm
{

std::atomic<bool> scrnshotFlag{false};
std::atomic<bool> videoRecFlag{false};
std::atomic<bool> recThreadStatus{false};
std::atomic<bool> InitFlag{false};
const std::string bsodAsJpeg{"/tmp/ikvm-test-screenshot.jpeg"};
std::string hostPowerState{"On"};
const char* NO_SIGNAL_IMG_PATH = "/tmp/ikvm-test-no-signal.jpeg";
const char* POWER_OFF_IMG_PATH = "/tmp/ikvm-test-power-off.jpeg";

} // namespace ikvm
