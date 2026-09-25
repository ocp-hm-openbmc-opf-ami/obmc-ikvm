#include "ikvm_args.hpp"

namespace ikvm
{

Args::Args(int argc, char* argv[]) :
    frameRate(30), subsampling(0), format(0), keyboardPath(), pointerPath(),
    udcName(), videoPath("/dev/video0"), calcFrameCRC(false),
    commandLine(argc, argv)
{}

} // namespace ikvm
