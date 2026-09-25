#include "ikvm_manager.hpp"

#include <linux/videodev2.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace ikvm
{
namespace
{

using ::testing::AnyNumber;
using ::testing::Invoke;
using ::testing::Return;

Args makeArgs()
{
    return Args(0, nullptr);
}

void setStatusDefaults(Manager& manager)
{
    EXPECT_CALL(manager.testVideoRef(), start()).Times(AnyNumber());
    EXPECT_CALL(manager.testVideoRef(), stop()).Times(AnyNumber());
    EXPECT_CALL(manager.testVideoRef(), getFrame()).Times(AnyNumber());
    EXPECT_CALL(manager.testVideoRef(), pushRecFrame()).Times(AnyNumber());
    EXPECT_CALL(manager.testVideoRef(), releaseFrames()).Times(AnyNumber());
    EXPECT_CALL(manager.testVideoRef(), getFormat())
        .Times(AnyNumber())
        .WillRepeatedly(Return(0));
    EXPECT_CALL(manager.testVideoRef(), getOriginalFormat())
        .Times(AnyNumber())
        .WillRepeatedly(Return(0));
    EXPECT_CALL(manager.testVideoRef(), getSignalStatus())
        .Times(AnyNumber())
        .WillRepeatedly(Return(0));
    EXPECT_CALL(manager.testServerRef(), sendFrame()).Times(AnyNumber());
}

class ManagerTest : public testing::Test
{
  protected:
    void SetUp() override
    {
        scrnshotFlag.store(false);
        videoRecFlag.store(false);
        recThreadStatus.store(false);
        InitFlag.store(false);
        hostPowerState = "On";
    }
};

TEST_F(ManagerTest, Constructor_Default_InitializesStateCorrectly)
{
    Manager manager(makeArgs());

    EXPECT_TRUE(manager.testGetContinueExecuting());
    EXPECT_FALSE(manager.testGetServerDoneFlag());
    EXPECT_TRUE(manager.testGetVideoDoneFlag());
}

TEST_F(ManagerTest, WaitMethods_ServerAndVideoFlags_ConsumesOnlyServerFlag)
{
    Manager manager(makeArgs());

    manager.testSetServerDoneFlag(true);
    manager.testWaitServer();
    EXPECT_FALSE(manager.testGetServerDoneFlag());

    manager.testSetVideoDoneFlag(true);
    manager.testWaitVideo();
    EXPECT_TRUE(manager.testGetVideoDoneFlag());
}

TEST_F(ManagerTest, ServerThread_OneInvocation_RunsOneIteration)
{
    Manager manager(makeArgs());
    EXPECT_CALL(manager.testServerRef(), run()).WillOnce(Invoke([&manager] {
        manager.testSetContinueExecuting(false);
    }));

    Manager::testCallServerThread(&manager);

    EXPECT_TRUE(manager.testGetServerDoneFlag());
}

TEST_F(ManagerTest, StatusUpdate_NoFrameDemand_StopsVideo)
{
    Manager manager(makeArgs());
    manager.testSetServerDoneFlag(true);
    setStatusDefaults(manager);
    EXPECT_CALL(manager.testServerRef(), wantsFrame())
        .WillRepeatedly(Return(false));
    EXPECT_CALL(manager.testVideoRef(), stop()).Times(1);
    EXPECT_CALL(manager.testVideoRef(), needsResize())
        .WillOnce(Invoke([&manager] {
            manager.testSetContinueExecuting(false);
            return false;
        }));

    Manager::testCallStatusUpdateThread(&manager);
}

TEST_F(ManagerTest, StatusUpdate_ResizeNeeded_ResizesInOrder)
{
    Manager manager(makeArgs());
    manager.testSetServerDoneFlag(true);
    setStatusDefaults(manager);
    EXPECT_CALL(manager.testServerRef(), wantsFrame())
        .WillRepeatedly(Return(false));
    EXPECT_CALL(manager.testVideoRef(), needsResize())
        .WillOnce(Invoke([&manager] {
            manager.testSetContinueExecuting(false);
            return true;
        }));
    testing::InSequence sequence;
    EXPECT_CALL(manager.testVideoRef(), resize());
    EXPECT_CALL(manager.testServerRef(), resize());

    Manager::testCallStatusUpdateThread(&manager);

    EXPECT_TRUE(manager.testGetVideoDoneFlag());
}

TEST_F(ManagerTest, StatusUpdate_FrameDemandActive_PushesRecFrameBeforeSending)
{
    Manager manager(makeArgs());
    manager.testSetServerDoneFlag(true);
    setStatusDefaults(manager);
    EXPECT_CALL(manager.testServerRef(), wantsFrame())
        .WillRepeatedly(Return(true));
    EXPECT_CALL(manager.testVideoRef(), needsResize())
        .WillOnce(Invoke([&manager] {
            manager.testSetContinueExecuting(false);
            return false;
        }));
    testing::InSequence sequence;
    EXPECT_CALL(manager.testVideoRef(), pushRecFrame());
    EXPECT_CALL(manager.testServerRef(), sendFrame());

    Manager::testCallStatusUpdateThread(&manager);
}

TEST_F(ManagerTest, StatusUpdate_InitFlagSet_PushesRecFrameBeforeRelease)
{
    Manager manager(makeArgs());
    manager.testSetServerDoneFlag(true);
    setStatusDefaults(manager);
    InitFlag.store(true);
    EXPECT_CALL(manager.testServerRef(), wantsFrame())
        .WillRepeatedly(Return(false));
    EXPECT_CALL(manager.testVideoRef(), needsResize())
        .WillOnce(Invoke([&manager] {
            manager.testSetContinueExecuting(false);
            return false;
        }));
    testing::InSequence sequence;
    EXPECT_CALL(manager.testVideoRef(), pushRecFrame());
    EXPECT_CALL(manager.testVideoRef(), releaseFrames());

    Manager::testCallStatusUpdateThread(&manager);

    EXPECT_FALSE(InitFlag.load());
}

TEST_F(ManagerTest, StatusUpdate_HostPowerOffNoSignal_UsesPowerOffFrame)
{
    Manager manager(makeArgs());
    manager.testSetServerDoneFlag(true);
    setStatusDefaults(manager);
    hostPowerState = "Off";
    EXPECT_CALL(manager.testServerRef(), wantsFrame())
        .WillRepeatedly(Return(true));
    EXPECT_CALL(manager.testVideoRef(), getSignalStatus())
        .WillRepeatedly(Return(V4L2_IN_ST_NO_SIGNAL));
    EXPECT_CALL(manager.testVideoRef(), setFrame(POWER_OFF_IMG_PATH)).Times(1);
    EXPECT_CALL(manager.testVideoRef(), needsResize())
        .WillOnce(Invoke([&manager] {
            manager.testSetContinueExecuting(false);
            return false;
        }));

    Manager::testCallStatusUpdateThread(&manager);
}

TEST_F(ManagerTest, StatusUpdate_FormatChanged_RestoresOriginalFormat)
{
    Manager manager(makeArgs());
    manager.testSetServerDoneFlag(true);
    setStatusDefaults(manager);
    EXPECT_CALL(manager.testServerRef(), wantsFrame())
        .WillRepeatedly(Return(true));
    EXPECT_CALL(manager.testVideoRef(), getFormat()).WillRepeatedly(Return(1));
    EXPECT_CALL(manager.testVideoRef(), getOriginalFormat())
        .WillRepeatedly(Return(0));
    EXPECT_CALL(manager.testVideoRef(), formatChange(0)).Times(1);
    EXPECT_CALL(manager.testVideoRef(), needsResize())
        .WillOnce(Invoke([&manager] {
            manager.testSetContinueExecuting(false);
            return false;
        }));

    Manager::testCallStatusUpdateThread(&manager);
}

} // namespace
} // namespace ikvm
