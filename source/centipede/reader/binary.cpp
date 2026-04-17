#include "binary.hpp"
#include "centipede/util/error_types.hpp"
#include "centipede/util/return_types.hpp"
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <fstream>
#include <functional>
#include <ios>
#include <iterator>
#include <print>
#include <ranges>
#include <type_traits>
#include <vector>

namespace centipede::reader
{
    namespace
    {
        template <typename T>
            requires(sizeof(T) == sizeof(uint32_t) and std::is_trivially_copyable_v<T>)
        auto read_from_file(std::ifstream& input_file, T& data) -> EnumError<std::size_t>
        {
            const auto read_size = sizeof(data);
            // NOLINTBEGIN (cppcoreguidelines-pro-type-reinterpret-cast)
            input_file.read(reinterpret_cast<char*>(&data), static_cast<std::streamsize>(read_size));
            // NOLINTEND (cppcoreguidelines-pro-type-reinterpret-cast)
            if (input_file.gcount() != static_cast<std::streamsize>(read_size))
            {
                return std::unexpected{ ErrorCode::reader_file_fail_to_read };
            }
            return read_size;
        }

        template <typename T>
            requires(sizeof(T) == sizeof(uint32_t) and std::is_trivially_copyable_v<T>)
        auto read_from_file(std::ifstream& input_file, std::vector<T>& data) -> EnumError<std::size_t>
        {
            assert(!data.empty());
            const auto read_size = data.size() * sizeof(T);
            // NOLINTBEGIN (cppcoreguidelines-pro-type-reinterpret-cast)
            input_file.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(read_size));
            // NOLINTEND (cppcoreguidelines-pro-type-reinterpret-cast)
            if (input_file.gcount() != static_cast<std::streamsize>(read_size))
            {
                return std::unexpected{ ErrorCode::reader_file_fail_to_read };
            }
            return read_size;
        }

        auto parse_entry_points(const Binary::RawBufferType& input, Binary::BufferType& output)
            -> EnumError<std::size_t>
        {
            // TODO: check file begin
            constexpr auto chunk_size{ 4U };
            auto size = std::size_t{};
            auto zipped = std::views::zip(input.first, input.second) | std::views::drop(1) |
                          std::views::chunk_by([](const auto& current, const auto& next) -> auto
                                               { return std::get<0>(current) != 0U and std::get<0>(next) != 0U; });
            auto chunks =
                std::views::zip(std::views::iota(std::ranges::distance(zipped)), zipped) |
                std::views::chunk_by([](const auto& current, const auto& next) -> auto
                                     { return std::get<0>(current) / chunk_size == std::get<0>(next) / chunk_size; }) |
                std::views::transform([](auto&& chunk) -> auto { return chunk | std::views::values; });
            auto is_ok = std::ranges::all_of(
                std::views::zip_transform(
                    [&size](const auto&& chunk, auto&& entrypoint) -> auto
                    {
                        if (std::ranges::distance(chunk) != chunk_size or std::ranges::distance(*chunk.begin()) != 1U)
                        {
                            return false;
                        }
                        auto iter = chunk.begin();
                        entrypoint.set_measurement(std::get<1>(*(*iter++).begin()));
                        for (const auto& global : *iter++)
                        {
                            entrypoint.add_global(std::get<0>(global), std::get<1>(global));
                        }
                        entrypoint.set_sigma(std::get<1>(*(*iter++).begin()));
                        for (const auto& local : *iter++ | std::views::values)
                        {
                            entrypoint.add_local(local);
                        }
                        size++;
                        return iter == chunk.end();
                    },
                    chunks,
                    output),
                std::identity{});
            if (not is_ok)
            {
                return std::unexpected{ ErrorCode::reader_file_fail_to_read };
            }
            return size;
        }
    } // namespace

    auto Binary::init() -> EnumError<>
    {
        if (config_.in_filename.empty())
        {
            return std::unexpected{ ErrorCode::reader_invalid_filename };
        }
        entry_buffer_.resize(config_.max_bufferpoint_size);
        raw_entry_buffer_.first.reserve(config_.max_bufferpoint_size);
        raw_entry_buffer_.second.reserve(config_.max_bufferpoint_size);
        input_file_.open(config_.in_filename, std::ios::binary | std::ios::in);
        if (!input_file_.is_open())
        {
            return std::unexpected{ ErrorCode::reader_file_fail_to_open };
        }
        return {};
    }

    auto Binary::read_one_entry() -> EnumError<std::size_t>
    {
        if (entry_buffer_.empty())
        {
            return std::unexpected{ ErrorCode::reader_uninitialized };
        }
        reset();
        auto read_size = uint32_t{};
        if (auto result = read_from_file(input_file_, read_size); !result)
        {
            return std::unexpected{ result.error() };
        }
        if (auto result = read_entry_to_buffer(read_size); !result)
        {
            return std::unexpected{ result.error() };
        }
        if (read_size > config_.max_bufferpoint_size)
        {
            entry_buffer_.resize(read_size);
        }
        auto size = parse_entry_points(raw_entry_buffer_, entry_buffer_);
        if (not size)
        {
            return std::unexpected{ size.error() };
        }
        return size.value();
    }

    void Binary::reset()
    {
        for (auto entrypoint : entry_buffer_)
        {
            entrypoint.reset();
        }
        raw_entry_buffer_.first.clear();
        raw_entry_buffer_.second.clear();
        size_ = 0U;
    }

    auto Binary::read_entry_to_buffer(uint32_t read_size) -> EnumError<>
    {
        raw_entry_buffer_.first.resize(read_size / 2U);
        raw_entry_buffer_.second.resize(read_size / 2U);
        if (auto result = read_from_file(input_file_, raw_entry_buffer_.second); !result)
        {
            return std::unexpected{ result.error() };
        }
        if (auto result = read_from_file(input_file_, raw_entry_buffer_.first); !result)
        {
            return std::unexpected{ result.error() };
        }
        return {};
    }
} // namespace centipede::reader
