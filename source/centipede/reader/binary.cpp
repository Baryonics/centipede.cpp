#include "binary.hpp"
#include "centipede/data/entry.hpp"
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
            const auto read_size = sizeof(T);
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

        struct ReadingFrame
        {
            uint32_t index{};
            uint32_t next_index{};
            float value{};
        };

        constexpr void handle_measurement(const ReadingFrame& current_frame,
                                          EntryPoint<>& current_entrypoint,
                                          ReadingState& current_state)
        {
            assert(current_state == ReadingState::measurement);
            current_entrypoint.set_measurement(current_frame.value);
            current_state = ReadingState::locals;
        }

        constexpr void handle_locals(const ReadingFrame& current_frame,
                                     EntryPoint<>& current_entrypoint,
                                     ReadingState& current_state)
        {
            assert(current_state == ReadingState::locals and current_frame.index != 0);
            if (current_frame.next_index == 0)
            {
                current_state = ReadingState::sigma;
            }
            current_entrypoint.add_local(current_frame.value);
        }

        constexpr void handle_sigma(const ReadingFrame& current_frame,
                                    EntryPoint<>& current_entrypoint,
                                    ReadingState& current_state)
        {
            assert(current_state == ReadingState::sigma and current_frame.index == 0);
            current_entrypoint.set_sigma(current_frame.value);
            current_state = ReadingState::globals;
        }

        constexpr void handle_globals(const ReadingFrame& current_frame,
                                      EntryPoint<>& current_entrypoint,
                                      ReadingState& current_state)
        {
            assert(current_state == ReadingState::globals and current_frame.index != 0);
            if (current_frame.next_index == 0)
            {
                current_state = ReadingState::new_entrypoint;
            }
            current_entrypoint.add_global(current_frame.index - 1U, current_frame.value);
        }

        constexpr void handle_done(const ReadingFrame& current_frame, ReadingState& current_state)
        {
            assert(current_state == ReadingState::done and current_frame.index != 0);
            current_state = ReadingState::measurement;
        }

        constexpr auto handle_state(const ReadingFrame& current_frame,
                                    EntryPoint<>& current_entrypoint,
                                    ReadingState& current_state) -> EnumError<>
        {
            switch (current_state)
            {
                case ReadingState::file_init:
                    if (current_frame.next_index != 0)
                    {
                        return std::unexpected{ ErrorCode::reader_file_fail_to_read };
                    }
                    current_state = ReadingState::measurement;
                    break;
                case ReadingState::new_entrypoint:
                    current_state = ReadingState::measurement;
                    [[fallthrough]];
                case ReadingState::measurement:
                    handle_measurement(current_frame, current_entrypoint, current_state);
                    break;
                case ReadingState::locals:
                    handle_locals(current_frame, current_entrypoint, current_state);
                    break;
                case ReadingState::sigma:
                    handle_sigma(current_frame, current_entrypoint, current_state);
                    break;
                case ReadingState::globals:
                    handle_globals(current_frame, current_entrypoint, current_state);
                    break;
                case ReadingState::done:
                    handle_done(current_frame, current_state);
                    break;
            }
            return {};
        }
    } // namespace

    auto Binary::init() -> EnumError<>
    {
        if (current_state_ != ReadingState::file_init)
        {
            return std::unexpected{ ErrorCode::reader_file_fail_to_open };
        }
        if (config_.in_filename.empty())
        {
            return std::unexpected{ ErrorCode::reader_invalid_filename };
        }
        entry_buffer_.reserve(config_.max_bufferpoint_size);
        raw_entry_buffer_.first.reserve(config_.max_bufferpoint_size);
        raw_entry_buffer_.second.reserve(config_.max_bufferpoint_size);
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
        if (entry_buffer_.empty() or !raw_entry_buffer_.first.empty() or !raw_entry_buffer_.second.empty() or
            size_ != 0U)
        {
            return std::unexpected{ ErrorCode::reader_uninitialized };
        }
        auto entry_size = uint32_t{};
        if (auto result = read_from_file(input_file_, entry_size); !result)
        {
            return std::unexpected(result.error());
        }
        if (entry_size > config_.max_bufferpoint_size)
        {
            return std::unexpected{ ErrorCode::reader_buffer_overflow };
        }
        if (auto result = read_entry_to_buffer(entry_size); !result)
        {
            return std::unexpected{ result.error() };
        }
        auto half_entry_size = entry_size / 2U;
        auto entrypoint_counter = std::size_t{ 0 };
        for (auto [idx, data_index, data_value] :
             std::views::zip(std::views::iota(std::size_t{ 0 }, static_cast<std::size_t>(half_entry_size)),
                             raw_entry_buffer_.first,
                             raw_entry_buffer_.second))
        {
            auto next_data_index = uint32_t{};
            if (idx + 1U < half_entry_size)
            {
                next_data_index = raw_entry_buffer_.first[idx + 1U];
            }
            if (auto result = handle_state(
                    ReadingFrame{ .index = data_index, .next_index = next_data_index, .value = data_value },
                    entry_buffer_[entrypoint_counter],
                    current_state_);
                !result)
            {
                return std::unexpected{ ErrorCode::reader_file_fail_to_read };
            }
            if (current_state_ == ReadingState::new_entrypoint)
            {
                entrypoint_counter++;
            }
        }
        if (current_state_ != ReadingState::globals)
        {
            return std::unexpected{ ErrorCode::reader_file_fail_to_read };
        }
        current_state_ = ReadingState::done;
        size_ = entrypoint_counter;
        return entry_size + sizeof(entry_size);
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
