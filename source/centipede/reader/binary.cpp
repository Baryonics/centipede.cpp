#include "binary.hpp"
#include "centipede/data/entry.hpp"
#include "centipede/util/error_types.hpp"
#include "centipede/util/return_types.hpp"
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <fstream>
#include <ios>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

namespace centipede::reader
{
    namespace
    {
        inline auto read_from_file(std::ifstream& input_file, uint32_t& data)
        {
            const auto read_size = sizeof(uint32_t);
            // NOLINTBEGIN (cppcoreguidelines-pro-type-reinterpret-cast)
            input_file.read(reinterpret_cast<char*>(&data), static_cast<std::streamsize>(read_size));
            // NOLINTEND (cppcoreguidelines-pro-type-reinterpret-cast)
            return read_size;
        }

    } // namespace

    auto Binary::init() -> EnumError<>
    {
        // reset();
        input_file_.open(config_.in_filename, std::ios::binary | std::ios::in);
        if (!input_file_.is_open())
        {
            return std::unexpected{ ErrorCode::reader_file_fail_to_open };
        }
        auto size = uint32_t{};
        read_from_file(input_file_, size);
        if (size == 0)
        {
            return std::unexpected{ ErrorCode::reader_file_fail_to_read };
        }
        total_size_ = size / 2U / sizeof(uint32_t);
        return {};
    }

    auto Binary::read_one_entry() -> EnumError<std::size_t> {}
} // namespace centipede::reader
