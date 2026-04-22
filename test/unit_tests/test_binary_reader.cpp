#include "centipede/centipede.hpp"
#include "centipede/reader/binary.hpp"
#include "centipede/util/error_types.hpp"
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <ios>
#include <iterator>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

using centipede::reader::Binary;
using Config = centipede::reader::Binary::Config;
namespace fs = std::filesystem;

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

    namespace
    {
        // NOLINTBEGIN
        // (cppcoreguidelines-avoid-magic-numbers)
        auto valid_measurement = float{ 1. };
        auto valid_sigma = float{ 1. };
        auto valid_locals_data = Binary::RawBufferType{ { 1, 2, 3 }, { 1.F, 2.F, 3.F } };
        auto valid_globals_data = Binary::RawBufferType{ { 3, 4, 5 }, { 3.F, 4.F, 5.F } };
        // NOLINTEND
        // (cppcoreguidelines-avoid-magic-numbers)

        auto fill_buffer(Binary::RawBufferType& output,
                         const float measurement,
                         const Binary::RawBufferType& locals_data,
                         const float sigma,
                         const Binary::RawBufferType& globals_data)
        {
            output.first.push_back(uint32_t{ 0 });
            output.second.push_back(measurement);
            std::ranges::copy(locals_data.first, std::back_inserter(output.first));
            std::ranges::copy(locals_data.second, std::back_inserter(output.second));

            output.first.push_back(uint32_t{ 0 });
            output.second.push_back(sigma);
            std::ranges::copy(globals_data.first, std::back_inserter(output.first));
            std::ranges::copy(globals_data.second, std::back_inserter(output.second));
        }

        auto write_to_file(std::ofstream& file, const Binary::RawBufferType& buffer)
        {
            // NOLINTBEGIN (cppcoreguidelines-pro-type-reinterpret-cast)
            file.write(reinterpret_cast<const char*>(buffer.second.data()), buffer.second.size() * sizeof(float));
            file.write(reinterpret_cast<const char*>(buffer.first.data()), buffer.first.size() * sizeof(uint32_t));
            // NOLINTEND (cppcoreguidelines-pro-type-reinterpret-cast)
        }
    } // namespace

    TEST(reader, valid_single_entry)
    {
        // NOLINTBEGIN(readability-function-cognitive-complexity)
        auto file_name = std::string{ "valid_single_entry.bin" };
        auto file = std::ofstream{ file_name, std::ios::out | std::ios::binary | std::ios::trunc };
        auto output_buffer = Binary::RawBufferType{ { uint32_t{ 0 } }, { 0.F } };
        fill_buffer(output_buffer, valid_measurement, valid_locals_data, valid_sigma, valid_globals_data);
        fill_buffer(output_buffer, valid_measurement, valid_locals_data, valid_sigma, valid_globals_data);
        write_to_file(file, output_buffer);
        file.close();
        auto reader = Binary{ Config{ .in_filename = file_name } };
        auto init_err = reader.init();
        EXPECT_TRUE(init_err);
        auto read_err = reader.read_one_entry();
        EXPECT_TRUE(read_err);
        auto read_result = reader.get_current_entry();
        for (const auto& entrypoint : read_result)
        {
            EXPECT_EQ(valid_locals_data.second, entrypoint.get_locals());
            auto expected_globals = std::views::zip_transform([](const auto& index, const auto& value) -> auto
                                                              { return std::pair{ index, value }; },
                                                              valid_globals_data.first,
                                                              valid_globals_data.second) |
                                    std::ranges::to<std::vector>();
            EXPECT_EQ(expected_globals, entrypoint.get_globals());
            EXPECT_EQ(entrypoint.get_measurement(), valid_measurement);
            EXPECT_EQ(entrypoint.get_sigma(), valid_sigma);
        }
        reader.close();
        // NOLINTEND(readability-function-cognitive-complexity)
    }

    // TEST(reader, valid_multi_entry) {}

    TEST(reader, invalid_file_begin)
    {
        auto file_name = std::string{ "reader_invalid_file_begin.bin" };
        auto file = std::ofstream{ file_name, std::ios::out | std::ios::binary | std::ios::trunc };
        auto output_buffer = Binary::RawBufferType{ { uint32_t{ 1 } }, { 0.F } };
        fill_buffer(output_buffer, valid_measurement, valid_locals_data, valid_sigma, valid_globals_data);
        write_to_file(file, output_buffer);
        auto reader = Binary{ Config{ .in_filename = file_name } };
        auto init_err = reader.init();
        EXPECT_TRUE(init_err);
        auto read_err = reader.read_one_entry();
        EXPECT_FALSE(read_err);
        EXPECT_EQ(read_err.error(), ErrorCode::reader_file_fail_to_read);
        reader.close();
    }

    TEST(reader, invalid_locals)
    {

        auto file_name = std::string{ "reader_invalid_locals.bin" };
        auto file = std::ofstream{ file_name, std::ios::out | std::ios::binary | std::ios::trunc };
        auto output_buffer = Binary::RawBufferType{ { uint32_t{ 0 } }, { 0.F } };
        auto invalid_locals_data = valid_locals_data;
        invalid_locals_data.first.at(1) = 0U;
        fill_buffer(output_buffer, valid_measurement, valid_locals_data, valid_sigma, valid_globals_data);
        write_to_file(file, output_buffer);
        auto reader = Binary{ Config{ .in_filename = file_name } };
        auto init_err = reader.init();
        EXPECT_TRUE(init_err);
        auto read_err = reader.read_one_entry();
        EXPECT_FALSE(read_err);
        EXPECT_EQ(read_err.error(), ErrorCode::reader_file_fail_to_read);
        reader.close();
    }

    TEST(reader, invalid_globals)
    {

        auto file_name = std::string{ "reader_invalid_globals.bin" };
        auto file = std::ofstream{ file_name, std::ios::out | std::ios::binary | std::ios::trunc };
        auto output_buffer = Binary::RawBufferType{ { uint32_t{ 0 } }, { 0.F } };
        auto invalid_globals_data = valid_globals_data;
        invalid_globals_data.first.at(1) = 0U;
        fill_buffer(output_buffer, valid_measurement, valid_locals_data, valid_sigma, valid_globals_data);
        write_to_file(file, output_buffer);
        auto reader = Binary{ Config{ .in_filename = file_name } };
        auto init_err = reader.init();
        EXPECT_TRUE(init_err);
        auto read_err = reader.read_one_entry();
        EXPECT_FALSE(read_err);
        EXPECT_EQ(read_err.error(), ErrorCode::reader_file_fail_to_read);
        reader.close();
    }

} // namespace centipede::test
