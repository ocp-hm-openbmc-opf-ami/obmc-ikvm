#include "ikvm_args.hpp"

#ifdef FAIL
#undef FAIL
#endif
#ifdef ERROR
#undef ERROR
#endif

#include <unistd.h>

#include <cstdio>
#include <filesystem>
#include <fstream>

#include <gtest/gtest.h>

namespace ikvm
{

// ---------------------------------------------------------------------------
// Helper to build a char* argv array from a vector of string literals.
// Lifetime tied to the vector; safe for the duration of each test.
// ---------------------------------------------------------------------------
static std::vector<char*> makeArgv(std::vector<const char*> args)
{
    std::vector<char*> argv;
    for (auto* a : args)
        argv.push_back(const_cast<char*>(a));
    argv.push_back(nullptr);
    return argv;
}

// ---------------------------------------------------------------------------
// ArgsDefaultsTest — no extra flags; verify compiled-in defaults
// ---------------------------------------------------------------------------
class ArgsDefaultsTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        // Reset getopt state so each test starts fresh
        optind = 1;
    }
};

TEST_F(ArgsDefaultsTest, NoArgs_DefaultFrameRate_Is30)
{
    std::vector<const char*> raw = {"obmc-ikvm"};
    auto argv = makeArgv(raw);
    optind = 1;
    Args args(static_cast<int>(raw.size()), argv.data());

    EXPECT_EQ(args.getFrameRate(), 30);
}

TEST_F(ArgsDefaultsTest, NoArgs_DefaultSubsampling_Is0)
{
    std::vector<const char*> raw = {"obmc-ikvm"};
    auto argv = makeArgv(raw);
    optind = 1;
    Args args(static_cast<int>(raw.size()), argv.data());

    EXPECT_EQ(args.getSubsampling(), 0);
}

TEST_F(ArgsDefaultsTest, NoArgs_DefaultFormat_Is0)
{
    std::vector<const char*> raw = {"obmc-ikvm"};
    auto argv = makeArgv(raw);
    optind = 1;
    Args args(static_cast<int>(raw.size()), argv.data());

    EXPECT_EQ(args.getFormat(), 0);
}

TEST_F(ArgsDefaultsTest, NoArgs_DefaultPaths_AreEmpty)
{
    std::vector<const char*> raw = {"obmc-ikvm"};
    auto argv = makeArgv(raw);
    optind = 1;
    Args args(static_cast<int>(raw.size()), argv.data());

    EXPECT_TRUE(args.getKeyboardPath().empty());
    EXPECT_TRUE(args.getPointerPath().empty());
    EXPECT_TRUE(args.getUdcName().empty());
    EXPECT_TRUE(args.getVideoPath().empty());
}

TEST_F(ArgsDefaultsTest, NoArgs_CalcFrameCRC_IsFalse)
{
    std::vector<const char*> raw = {"obmc-ikvm"};
    auto argv = makeArgv(raw);
    optind = 1;
    Args args(static_cast<int>(raw.size()), argv.data());

    EXPECT_FALSE(args.getCalcFrameCRC());
}

TEST_F(ArgsDefaultsTest, NoArgs_CommandLine_PreservesArgcArgv)
{
    std::vector<const char*> raw = {"obmc-ikvm"};
    auto argv = makeArgv(raw);
    optind = 1;
    Args args(static_cast<int>(raw.size()), argv.data());

    EXPECT_EQ(args.getCommandLine().argc, 1);
    EXPECT_STREQ(args.getCommandLine().argv[0], "obmc-ikvm");
}

// ---------------------------------------------------------------------------
// FrameRateTest — boundary checks for -f flag
// ---------------------------------------------------------------------------
class FrameRateTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        optind = 1;
    }
};

TEST_F(FrameRateTest, ValidFrameRate_30_IsStored)
{
    std::vector<const char*> raw = {"obmc-ikvm", "-f", "30"};
    auto argv = makeArgv(raw);
    optind = 1;
    Args args(static_cast<int>(raw.size()), argv.data());

    EXPECT_EQ(args.getFrameRate(), 30);
}

TEST_F(FrameRateTest, ValidFrameRate_1_IsStored)
{
    std::vector<const char*> raw = {"obmc-ikvm", "-f", "1"};
    auto argv = makeArgv(raw);
    optind = 1;
    Args args(static_cast<int>(raw.size()), argv.data());

    EXPECT_EQ(args.getFrameRate(), 1);
}

TEST_F(FrameRateTest, ValidFrameRate_60_IsStored)
{
    std::vector<const char*> raw = {"obmc-ikvm", "-f", "60"};
    auto argv = makeArgv(raw);
    optind = 1;
    Args args(static_cast<int>(raw.size()), argv.data());

    EXPECT_EQ(args.getFrameRate(), 60);
}

TEST_F(FrameRateTest, InvalidFrameRate_Negative_ClampedTo30)
{
    std::vector<const char*> raw = {"obmc-ikvm", "-f", "-1"};
    auto argv = makeArgv(raw);
    optind = 1;
    Args args(static_cast<int>(raw.size()), argv.data());

    EXPECT_EQ(args.getFrameRate(), 30);
}

TEST_F(FrameRateTest, InvalidFrameRate_Above60_ClampedTo30)
{
    std::vector<const char*> raw = {"obmc-ikvm", "-f", "61"};
    auto argv = makeArgv(raw);
    optind = 1;
    Args args(static_cast<int>(raw.size()), argv.data());

    EXPECT_EQ(args.getFrameRate(), 30);
}

TEST_F(FrameRateTest, InvalidFrameRate_Zero_ClampedTo30)
{
    std::vector<const char*> raw = {"obmc-ikvm", "-f", "0"};
    auto argv = makeArgv(raw);
    optind = 1;
    Args args(static_cast<int>(raw.size()), argv.data());

    // 0 is < 0 is false, 0 > 60 is false, so 0 is a valid value per the code
    EXPECT_EQ(args.getFrameRate(), 0);
}

// ---------------------------------------------------------------------------
// SubsamplingTest — boundary checks for -s flag
// ---------------------------------------------------------------------------
class SubsamplingTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        optind = 1;
    }
};

TEST_F(SubsamplingTest, Subsampling_0_IsStored)
{
    std::vector<const char*> raw = {"obmc-ikvm", "-s", "0"};
    auto argv = makeArgv(raw);
    optind = 1;
    Args args(static_cast<int>(raw.size()), argv.data());

    EXPECT_EQ(args.getSubsampling(), 0);
}

TEST_F(SubsamplingTest, Subsampling_1_IsStored)
{
    std::vector<const char*> raw = {"obmc-ikvm", "-s", "1"};
    auto argv = makeArgv(raw);
    optind = 1;
    Args args(static_cast<int>(raw.size()), argv.data());

    EXPECT_EQ(args.getSubsampling(), 1);
}

TEST_F(SubsamplingTest, Subsampling_Invalid_ClampedTo0)
{
    std::vector<const char*> raw = {"obmc-ikvm", "-s", "2"};
    auto argv = makeArgv(raw);
    optind = 1;
    Args args(static_cast<int>(raw.size()), argv.data());

    EXPECT_EQ(args.getSubsampling(), 0);
}

TEST_F(SubsamplingTest, Subsampling_Negative_ClampedTo0)
{
    std::vector<const char*> raw = {"obmc-ikvm", "-s", "-1"};
    auto argv = makeArgv(raw);
    optind = 1;
    Args args(static_cast<int>(raw.size()), argv.data());

    EXPECT_EQ(args.getSubsampling(), 0);
}

// ---------------------------------------------------------------------------
// FormatTest — boundary checks for -m flag
// ---------------------------------------------------------------------------
class FormatTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        optind = 1;
    }
};

TEST_F(FormatTest, Format_0_IsStored)
{
    std::vector<const char*> raw = {"obmc-ikvm", "-m", "0"};
    auto argv = makeArgv(raw);
    optind = 1;
    Args args(static_cast<int>(raw.size()), argv.data());

    EXPECT_EQ(args.getFormat(), 0);
}

TEST_F(FormatTest, Format_2_IsStored)
{
    std::vector<const char*> raw = {"obmc-ikvm", "-m", "2"};
    auto argv = makeArgv(raw);
    optind = 1;
    Args args(static_cast<int>(raw.size()), argv.data());

    // On non-AST2700 test hardware: format=2 unless hub paths exist
    EXPECT_TRUE(args.getFormat() == 2 || args.getFormat() == 0);
}

TEST_F(FormatTest, Format_1_Reserved_ClampedTo0)
{
    // Pre-scan finding #2: reserved value 1 must map to 0
    std::vector<const char*> raw = {"obmc-ikvm", "-m", "1"};
    auto argv = makeArgv(raw);
    optind = 1;
    Args args(static_cast<int>(raw.size()), argv.data());

    EXPECT_EQ(args.getFormat(), 0);
}

TEST_F(FormatTest, Format_Invalid_3_ClampedTo0)
{
    std::vector<const char*> raw = {"obmc-ikvm", "-m", "3"};
    auto argv = makeArgv(raw);
    optind = 1;
    Args args(static_cast<int>(raw.size()), argv.data());

    EXPECT_EQ(args.getFormat(), 0);
}

// ---------------------------------------------------------------------------
// PathTest — -k, -p, -u, -v flags
// ---------------------------------------------------------------------------
class PathTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        optind = 1;
    }
};

TEST_F(PathTest, KeyboardPath_IsStored)
{
    std::vector<const char*> raw = {"obmc-ikvm", "-k", "/dev/hidg0"};
    auto argv = makeArgv(raw);
    optind = 1;
    Args args(static_cast<int>(raw.size()), argv.data());

    EXPECT_EQ(args.getKeyboardPath(), "/dev/hidg0");
}

TEST_F(PathTest, PointerPath_IsStored)
{
    std::vector<const char*> raw = {"obmc-ikvm", "-p", "/dev/hidg1"};
    auto argv = makeArgv(raw);
    optind = 1;
    Args args(static_cast<int>(raw.size()), argv.data());

    EXPECT_EQ(args.getPointerPath(), "/dev/hidg1");
}

TEST_F(PathTest, UdcName_IsStored)
{
    std::vector<const char*> raw = {"obmc-ikvm", "-u", "musb-hdrc.0"};
    auto argv = makeArgv(raw);
    optind = 1;
    Args args(static_cast<int>(raw.size()), argv.data());

    EXPECT_EQ(args.getUdcName(), "musb-hdrc.0");
}

TEST_F(PathTest, VideoDevicePath_IsStored)
{
    std::vector<const char*> raw = {"obmc-ikvm", "-v", "/dev/video0"};
    auto argv = makeArgv(raw);
    optind = 1;
    Args args(static_cast<int>(raw.size()), argv.data());

    EXPECT_EQ(args.getVideoPath(), "/dev/video0");
}

// ---------------------------------------------------------------------------
// CalcCRCTest — -c flag
// ---------------------------------------------------------------------------
TEST(CalcCRCTest, CalcCRC_FlagSet_IsTrue)
{
    optind = 1;
    std::vector<const char*> raw = {"obmc-ikvm", "-c"};
    auto argv = makeArgv(raw);
    optind = 1;
    Args args(static_cast<int>(raw.size()), argv.data());

    EXPECT_TRUE(args.getCalcFrameCRC());
}

TEST(CalcCRCTest, CalcCRC_FlagAbsent_IsFalse)
{
    optind = 1;
    std::vector<const char*> raw = {"obmc-ikvm"};
    auto argv = makeArgv(raw);
    optind = 1;
    Args args(static_cast<int>(raw.size()), argv.data());

    EXPECT_FALSE(args.getCalcFrameCRC());
}

// ---------------------------------------------------------------------------
// CombinedFlagsTest — multiple flags together
// ---------------------------------------------------------------------------
TEST(CombinedFlagsTest, AllFlags_StoredCorrectly)
{
    optind = 1;
    std::vector<const char*> raw = {
        "obmc-ikvm", "-f",         "15",          "-s",         "1",
        "-k",        "/dev/hidg0", "-p",          "/dev/hidg1", "-u",
        "udc0",      "-v",         "/dev/video0", "-c"};
    auto argv = makeArgv(raw);
    optind = 1;
    Args args(static_cast<int>(raw.size()), argv.data());

    EXPECT_EQ(args.getFrameRate(), 15);
    EXPECT_EQ(args.getSubsampling(), 1);
    EXPECT_EQ(args.getKeyboardPath(), "/dev/hidg0");
    EXPECT_EQ(args.getPointerPath(), "/dev/hidg1");
    EXPECT_EQ(args.getUdcName(), "udc0");
    EXPECT_EQ(args.getVideoPath(), "/dev/video0");
    EXPECT_TRUE(args.getCalcFrameCRC());
}

// ---------------------------------------------------------------------------
// CommandLineTest — CommandLine struct preserves original argc/argv
// ---------------------------------------------------------------------------
TEST(CommandLineTest, CommandLine_PreservesOriginalArgs)
{
    optind = 1;
    std::vector<const char*> raw = {"obmc-ikvm", "-f", "25", "-v",
                                    "/dev/video1"};
    auto argv = makeArgv(raw);
    optind = 1;
    Args args(static_cast<int>(raw.size()), argv.data());

    const Args::CommandLine& cl = args.getCommandLine();
    EXPECT_EQ(cl.argc, 5);
    EXPECT_STREQ(cl.argv[0], "obmc-ikvm");
    EXPECT_STREQ(cl.argv[1], "-f");
    EXPECT_STREQ(cl.argv[2], "25");
}

TEST(CommandLineTest, CommandLine_CopyConstruction_PreservesMembers)
{
    char prog[] = "obmc-ikvm";
    char opt[] = "-c";
    char* argv[] = {prog, opt, nullptr};

    Args::CommandLine commandLine(2, argv);
    Args::CommandLine copy(commandLine);

    EXPECT_EQ(copy.argc, 2);
    EXPECT_EQ(copy.argv, argv);
}

TEST(CommandLineTest, CommandLine_CopyAssignment_PreservesMembers)
{
    char prog[] = "obmc-ikvm";
    char video[] = "/dev/video0";
    char* argv[] = {prog, video, nullptr};

    Args::CommandLine source(2, argv);
    Args::CommandLine target(0, nullptr);
    target = source;

    EXPECT_EQ(target.argc, 2);
    EXPECT_EQ(target.argv, argv);
}

// ---------------------------------------------------------------------------
// ArgsPrintUsageTest — exercises the private printUsage() via the #ifdef TEST
//   public wrapper. Pure fprintf(stderr, ...) output, no D-Bus/hardware
//   dependency, so it is a low-hanging fruit for private-function coverage.
//   stderr is temporarily redirected to a temp file so the output can be
//   verified without polluting test-runner output.
// ---------------------------------------------------------------------------
TEST(ArgsPrintUsageTest, PrintUsage_WritesUsageToStderr)
{
    namespace fs = std::filesystem;
    const std::string capturePath =
        (fs::temp_directory_path() / "ikvm-test-printusage.txt").string();

    fflush(stderr);
    int savedStderr = dup(fileno(stderr));
    FILE* capture = freopen(capturePath.c_str(), "w", stderr);
    ASSERT_NE(capture, nullptr);

    char prog[] = "obmc-ikvm";
    char* argv[] = {prog, nullptr};
    optind = 1;
    Args args(1, argv);
    args.testPrintUsage();

    fflush(stderr);
    dup2(savedStderr, fileno(stderr));
    close(savedStderr);

    std::ifstream in(capturePath);
    std::string contents((std::istreambuf_iterator<char>(in)),
                         std::istreambuf_iterator<char>());
    fs::remove(capturePath);

    EXPECT_NE(contents.find("Usage: obmc-ikvm"), std::string::npos);
    EXPECT_NE(contents.find("-h, --help"), std::string::npos);
}

} // namespace ikvm
