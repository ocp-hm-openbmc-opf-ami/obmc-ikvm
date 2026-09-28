// SPDX-License-Identifier: MIT

#include "kvm_dbus-utils.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>

#include <gtest/gtest.h>

namespace kvmDbus
{
static const std::string jsonPath = KVM_DBUS_TEST_JSON_PATH;
static const std::string mountsPath = KVM_DBUS_TEST_MOUNTS;

struct ScopedFile
{
    explicit ScopedFile(std::string filePath) : path(std::move(filePath)) {}

    ~ScopedFile()
    {
        std::filesystem::remove_all(path);
    }

    std::string path;
};

class UtilsFixture : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        jsonData = nullptr;
        testIsMountedFromRemoteHook = {};
        testCreateMountDirectoryHook = {};
        testMountRemoteShareHook = {};
        testUnmountRemoteShareHook = {};
    }
};

TEST_F(UtilsFixture, LoadJson_ValidJsonFile_LoadsSuccessfully)
{
    ScopedFile guard{jsonPath};
    std::ofstream(jsonPath)
        << R"({"Kvm":{"VideoRecord":{"RecordStatus":false}}})";

    ASSERT_EQ(loadJson(), 0);
    EXPECT_FALSE(jsonData["Kvm"]["VideoRecord"]["RecordStatus"].get<bool>());
}

TEST_F(UtilsFixture, LoadJson_MissingOrMalformedFile_ReturnsError)
{
    ScopedFile guard{jsonPath};
    std::filesystem::remove(jsonPath);
    EXPECT_EQ(loadJson(), -1);

    std::ofstream(jsonPath) << R"({"Kvm":"unterminated")";
    EXPECT_EQ(loadJson(), -1);
}

TEST_F(UtilsFixture, UpdateJson_ValidJsonData_PersistsToFile)
{
    ScopedFile guard{jsonPath};
    jsonData = nlohmann::json::object(
        {{"Kvm", {{"VideoRecord", {{"RecordStatus", true}}}}}});

    ASSERT_EQ(updateJson(), 0);
    std::ifstream input(jsonPath);
    const auto stored = nlohmann::json::parse(input);
    EXPECT_TRUE(stored["Kvm"]["VideoRecord"]["RecordStatus"].get<bool>());
}

TEST_F(UtilsFixture, UpdateJson_DirectoryPathProvided_ReturnsError)
{
    ScopedFile guard{jsonPath};
    std::filesystem::remove_all(jsonPath);
    std::filesystem::create_directory(jsonPath);
    jsonData = nlohmann::json::object();

    EXPECT_EQ(updateJson(), -1);
}

TEST_F(UtilsFixture, IsMountedFromRemote_NfsAndCifsTable_RecognizesCorrectly)
{
    ScopedFile guard{mountsPath};
    std::ofstream(mountsPath)
        << "server:/share /mnt/nfs nfs rw 0 0\n"
        << "//server/share /mnt/cifs cifs rw 0 0\n"
        << "server:/share /mnt/nfs4 nfs4 rw 0 0\n";

    EXPECT_TRUE(isMountedFromRemote("/mnt/nfs"));
    EXPECT_TRUE(isMountedFromRemote("/mnt/cifs"));
    EXPECT_FALSE(isMountedFromRemote("/mnt/nfs4"));
    EXPECT_FALSE(isMountedFromRemote("/mnt/missing"));
}

TEST_F(UtilsFixture, MountOperations_HooksProvided_ExecuteCorrectly)
{
    testIsMountedFromRemoteHook = [](const std::string& path) {
        return path == "/mnt/test";
    };
    testCreateMountDirectoryHook = [](const std::string& path, int mode) {
        return path == "/mnt/test" && mode == 0755 ? 0 : -1;
    };
    testMountRemoteShareHook =
        [](const std::string& source, const std::string& target,
           const std::string& type, unsigned long, const std::string&) {
            return source == "server:/share" && target == "/mnt/test" &&
                           type == "nfs"
                       ? 0
                       : -1;
        };
    testUnmountRemoteShareHook = [](const std::string& target) {
        return target == "/mnt/test" ? 0 : -1;
    };

    EXPECT_TRUE(isMountedFromRemote("/mnt/test"));
    EXPECT_EQ(createMountDirectory("/mnt/test", 0755), 0);
    EXPECT_EQ(mountRemoteShare("server:/share", "/mnt/test", "nfs", 0, ""), 0);
    EXPECT_EQ(unmountRemoteShare("/mnt/test"), 0);
}

} // namespace kvmDbus
