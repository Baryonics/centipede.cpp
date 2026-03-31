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

        template <typename T>
            requires(sizeof(T) == sizeof(uint32_t))
        auto read_from_file(std::ifstream& input_file, std::vector<T>& data)
        {
            const auto read_size = sizeof(uint32_t);
            // NOLINTBEGIN (cppcoreguidelines-pro-type-reinterpret-cast)
            input_file.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(read_size));
            // NOLINTEND (cppcoreguidelines-pro-type-reinterpret-cast)
            return read_size;
        }

        enum ReadingState : uint8_t
        {
            file_init,
            measurement,
            locals,
            sigma,
            globals,
        };
    } // namespace

    auto Binary::init() -> EnumError<>
    {
        entry_buffer_.reserve(config_.max_bufferpoint_size);
        for ([[maybe_unused]] auto idx : std::views::iota(std::size_t{ 0 }, config_.max_bufferpoint_size))
        {
            entry_buffer_.emplace_back();
        }
        input_file_.open(config_.in_filename, std::ios::binary | std::ios::in);
        if (!input_file_.is_open())
        {
            return std::unexpected{ ErrorCode::reader_file_fail_to_open };
        }
        return {};
    }

    auto Binary::read_one_entry() -> EnumError<std::size_t>
    {
        if (entry_buffer_.empty() or !raw_entry_buffer_.empty() or size_ != 0U)
        {
            return std::unexpected{ ErrorCode::reader_uninitialized };
        }
        auto entry_size = uint32_t{};
        read_from_file(input_file_, entry_size);
        if (entry_size > config_.max_bufferpoint_size)
        {
            return std::unexpected{ ErrorCode::reader_buffer_overflow };
        }
        auto half_entry_size = entry_size / 2U;
        read_from_file(input_file_, raw_entry_buffer_);
        auto data_span = std::span{ raw_entry_buffer_ };
        auto data_index_span = data_span.subspan(half_entry_size);
        auto data_value_span = data_span.subspan(0, half_entry_size);

        auto entrypoint_counter = std::size_t{};

        auto current_state = ReadingState::file_init;

        for (auto [data_index, data_value_raw] : std::views::zip(data_index_span, data_value_span))
        {
            auto data_value = static_cast<float>(data_value_raw);
            switch (current_state)
            {
                case ReadingState::file_init:
                    if (data_index != 0)
                    {
                        return std::unexpected{ ErrorCode::reader_file_fail_to_read };
                    }
                    current_state = ReadingState::measurement;
                    break;
                case ReadingState::locals:
                    if (data_index != 0)
                    {
                        entry_buffer_[entrypoint_counter].add_local(data_value);
                        break;
                    }
                    [[fallthrough]];
                case ReadingState::sigma:
                    entry_buffer_[entrypoint_counter].set_sigma(data_value);
                    current_state = ReadingState::globals;
                    break;
                case ReadingState::globals:
                    if (data_index != 0)
                    {
                        entry_buffer_[entrypoint_counter].add_global(data_index, data_value);
                        break;
                    }
                    entrypoint_counter++;
                    [[fallthrough]];
                case ReadingState::measurement:
                    entry_buffer_[entrypoint_counter].set_measurement(data_value);
                    current_state = ReadingState::locals;
                    break;
            }
        }
        size_ = entrypoint_counter;
        return entry_size + sizeof(entry_size);
    }

    void Binary::reset()
    {
        for (auto entrypoint : entry_buffer_)
        {
            entrypoint.reset();
        }
        raw_entry_buffer_.clear();
        size_ = 0U;
    }
} // namespace centipede::reader
