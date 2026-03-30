#include "binary.hpp"
#include "centipede/util/error_types.hpp"
#include "centipede/util/return_types.hpp"
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <fstream>
#include <ios>
#include <ranges>
#include <span>
#include <type_traits>
#include <vector>

namespace centipede::reader
{
    namespace
    {
        template <typename T>
            requires(sizeof(T) == sizeof(uint32_t))
        auto read_from_file(std::ifstream& input_file, T& data)
        {
            const auto read_size = sizeof(uint32_t);
            // NOLINTBEGIN (cppcoreguidelines-pro-type-reinterpret-cast)
            input_file.read(reinterpret_cast<char*>(&data), static_cast<std::streamsize>(read_size));
            // NOLINTEND (cppcoreguidelines-pro-type-reinterpret-cast)
            return read_size;
        }

        auto read_size_from_file(std::ifstream& input_file) -> uint32_t
        {
            auto size = uint32_t{};
            read_from_file(input_file, size);
            return size;
        }

        template <typename T, typename U>
            requires(std::is_trivially_copyable_v<T>)
        auto get_value_from_raw(std::span<U> source, std::size_t offset) -> T

        {
            assert(offset <= source.size());
            assert(sizeof(T) <= source.size() - offset);

            T value{};
            auto slice = source.subspan(offset, sizeof(T));
            std::memcpy(&value, slice.data(), sizeof(T));
            return value;
        }

    } // namespace

    auto Binary::init() -> EnumError<>
    {
        reset();
        entry_buffer_.reserve(config_.max_bufferpoint_size);
        input_file_.open(config_.in_filename, std::ios::binary | std::ios::in);
        if (!input_file_.is_open())
        {
            return std::unexpected{ ErrorCode::reader_file_fail_to_open };
        }
        return {};
    }

    auto Binary::read_one_entry() -> EnumError<std::size_t>
    {
        auto entry_size = read_size_from_file(input_file_);
        auto half_entry_size = entry_size / 2U;
        if (!input_file_.read(raw_entry_buffer_.data(), entry_size))
        {
            return std::unexpected{ ErrorCode::reader_file_fail_to_read };
        }
        auto data_span = std::span{ raw_entry_buffer_ };
        auto data_index_span = data_span.subspan(half_entry_size);
        auto data_value_span = data_span.subspan(0, half_entry_size);

        auto zero_counter = std::size_t{};
        auto entrypoint_counter = std::size_t{};
        auto at_globals = false;
        for (auto idx : std::views::iota(std::size_t{ 0 }, half_entry_size) | std::views::stride(sizeof(uint32_t)))
        {
            auto data_index = get_value_from_raw<uint32_t>(data_index_span, idx);
            auto data_value = get_value_from_raw<float>(data_value_span, idx);

            if (data_index == 0)
            {
                zero_counter++;
                switch (zero_counter % 3U)
                {
                    case 0:
                        entrypoint_counter++;
                        break;
                    case 1:
                        entry_buffer_[entrypoint_counter].set_measurement(data_value);
                        at_globals = false;
                        break;
                    case 2:
                        entry_buffer_[entrypoint_counter].set_sigma(data_value);
                        at_globals = true;
                        break;
                    default:
                        return std::unexpected{
                            ErrorCode::reader_file_fail_to_read
                        }; // a.k.a math suddenly stopped mathing
                }
                continue;
            }

            if (at_globals)
            {
                entry_buffer_[entrypoint_counter].add_global(data_index, data_value);
            }
            else
            {
                entry_buffer_[entrypoint_counter].add_local(data_value);
            }
        }

        return {};
    }

    void Binary::reset()
    {
        for (auto entrypoint : entry_buffer_)
        {
            entrypoint.reset();
        }
    }
} // namespace centipede::reader
