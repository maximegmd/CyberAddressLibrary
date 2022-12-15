#include "VersionLib.h"
#include <gtest/gtest.h>
#include <filesystem>
#include <spdlog/spdlog.h>

TEST(VersionLibTest, Reading)
{
    VersionDb db;
    EXPECT_TRUE(db.Load(3, 0, 64, 38113));
    EXPECT_EQ(db.FindAddressById(25), (char*)0x1B10 + db.GetModuleBase());

    uint64_t out = 0;
    EXPECT_TRUE(db.FindIdByAddress((char*)0x1B10 + db.GetModuleBase(), out));
    EXPECT_EQ(out, 25);

    EXPECT_TRUE(db.FindOffsetById(123, out));
    EXPECT_EQ(out, 0x8C30);

    EXPECT_TRUE(db.FindIdByOffset(0x8C30, out));
    EXPECT_EQ(out, 123);
}

TEST(VersionLibTest, Dump)
{
    VersionDb db;
    EXPECT_TRUE(db.Load(3, 0, 64, 38113));

    std::ostringstream oss;
    EXPECT_TRUE(db.Dump(oss));
}