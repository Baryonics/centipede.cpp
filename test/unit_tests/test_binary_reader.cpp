#include "centipede/centipede.hpp"
#include "centipede/reader/binary.hpp"
#include "centipede/util/error_types.hpp"
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <ios>
#include <string>
#include <utility>

using centipede::reader::Binary;
using Config = centipede::reader::Binary::Config;
namespace fs = std::filesystem;

namespace
{
    const auto valid_meas = 1.F;
    const auto valid_sigma = 1.F;
    const auto valid_entry_point = centipede::EntryPoint<3, 2>{}
                                       .set_locals(1.F, 2.F, 3.F)
                                       .set_globals(std::pair{ 10U, 2.F }, std::pair{ 11U, 3.F })
                                       .set_measurement(valid_meas)
                                       .set_sigma(valid_sigma);
} // namespace
namespace centipede::test
{

    TEST(reader, constructor)
    {
        auto reader = Binary{ Config{ .in_filename = "binary_reader_constructor.bin" } };
        EXPECT_FALSE(fs::exists(reader.get_config().in_filename));
    }

    TEST(reader, init)
    {
        auto file_name = std::string{ "binary_reader_init.bin" };
        auto file = std::ofstream{ file_name, std::ios::out | std::ios::binary | std::ios::trunc };
        auto reader = Binary{ Config{ .in_filename = file_name } };
        auto error = reader.init();
        EXPECT_TRUE(error.has_value());
    }

    TEST(reader, init_empty_file_name_error)
    {
        auto reader = Binary{ Config{ .in_filename = "" } };
        auto error = reader.init();
        EXPECT_TRUE(not error.has_value());
        EXPECT_EQ(error.error(), ErrorCode::reader_invalid_filename);
        reader = Binary{ Config{ .in_filename = "nonexistent.bin" } };
        error = reader.init();
        EXPECT_EQ(error.error(), ErrorCode::reader_file_fail_to_open);
    }

    TEST(reader, init_nonexisting_file_error)
    {
        auto reader = Binary{ Config{ .in_filename = "nonexistent.bin" } };
        auto error = reader.init();
        EXPECT_TRUE(not error.has_value());
        EXPECT_EQ(error.error(), ErrorCode::reader_file_fail_to_open);
    }
} // namespace centipede::test
