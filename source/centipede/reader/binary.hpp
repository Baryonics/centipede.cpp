#pragma once

#include "centipede/data/entry.hpp"
#include "centipede/util/common_definitions.hpp"
#include "centipede/util/return_types.hpp"
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace centipede::reader
{
    /**
     * @class Binary
     * @brief Class for reading binary files.
     * Data is read from the binary file entry-wise, for each entry contains multiple entrypoints (of type
     * #centipede::EntryPoint). Before the initial read operation, #Binary::init() function needs to be called, where
     * the file handler is opened and internal buffers get reset.
     * To read one entry, #Binary::read_one_entry() needs to be called, which parses the file into the internal buffers.
     * To obtain the current entry, #Binary::get_current_entry() needs to be called. Note that the internal buffers will
     * be reset after every read, therefore #Binary::get_current_entry() has to be called before the next read.
     *
     * Configuration of the class is done via Binary::Config struct
     *
     * #### Example usage
     *
     * ``` cpp
     *auto reader = centipede::reader::Binary{ centipede::reader::Binary::Config{ .in_filename = "output.bin" } };
     *auto init_err = reader.init();
     *
     *    if (not init_err.has_value())
     *    {
     *        std::println(stderr, "Error: {}", init_err.error());
     *        return EXIT_FAILURE;
     *    }
     *
     *    auto read_err = centipede::EnumError<std::size_t>{};
     *
     *    while (not reader.is_end_of_file())
     *    {
     *        read_err = reader.read_one_entry();
     *        if (not read_err.has_value())
     *        {
     *            std::println(stderr, "Error: {}", read_err.error());
     *        }
     *    }
     *
     *    std::println("N Entries: {}", reader.get_n_entries());
     * ```
     **/
    class Binary
    {
      public:
        /**
         * @class Config
         * @brief Class for configuring the binary writer class (#Binary)
         *
         * #### Example usage
         *
         * ```cpp
         * auto reader =
         *     Binary{ Binary::Config{ .in_filename = "output.bin", .max_bufferpoint_size = 1000 } };
         * ```
         *
         */
        struct Config
        {
            std::string in_filename;                                     //!< Input binary filename.
            uint32_t max_bufferpoint_size = common::DEFAULT_BUFFER_SIZE; //!< maximum bufferpoint for an entry.
        };
        using RawBufferType = std::pair<std::vector<uint32_t>, std::vector<float>>; //!< Type of #raw_entry_buffer_
        using BufferType = std::vector<EntryPoint<>>;                               //!< Type of #entry_buffer_

        /**
         * @brief Default constructor
         */
        Binary() = default;

        /**
         * @brief Constructor takes an argument for  the configuration.
         *
         * The config argument will be moved (`std::move`) to its member variable `config_`
         * @param config Configuration struct.
         * @see Config
         */
        constexpr explicit Binary(Config config)
            : config_{ std::move(config) }
        {
        }

        /**
         * @brief Initialization.
         *
         * The initialization function must be called before calling the #read_one_entry() method. When calling this
         * function, #raw_entry_buffer_ and #entry_buffer_ get resized and a file is opened with the specified name in
         * #Config.
         * @return Returns ErrorCode::reader_file_fail_to_open if the file cannot be opened with the name specified by
         * Config::in_filename.
         * @see Config
         */
        [[nodiscard]] auto init() -> EnumError<>;

        /**
         * @brief Manually close the input file handler.
         *
         * This function will be called automatically when the destructor is called.
         */
        void close() { input_file_.close(); }

        /**
         * @brief Reads one entry from file into the internal buffers.
         *
         * Reading an entry follows this crude sequence:
         * 1. if #init() is not called once after instanciating, returns an error
         * 2. Reads one entry to #raw_entry_buffer_, returns if read operation fails.
         * 3. Parses entry and stores individual entrypoints in #entry_buffer_, returns if file format is corrupted.
         * 4. Increases #n_entries_ for one entry is read and sets #size_ corresponding to the number of 32 Bit values
         *     (global and local derivs, sigmas and measurements) contained in current entry.
         * 5. returns #size_
         *
         * @return
         * - ErrorCode::reader_uninitialized if #init() is not called before reading
         * - ErrorCode::reader_buffer_overflow if buffer size is too small.
         * - ErrorCode::reader_file_fail_to_read if the file stream is broken or file format is corrupted.
         * - #size_ on success
         **/
        [[maybe_unused]] auto read_one_entry() -> EnumError<std::size_t>;

        /**
         * @brief Getter of #entry_buffer_.
         *
         * @return Returns a std::span of #entry_buffer_
         **/
        [[nodiscard]] auto get_current_entry() const -> auto { return std::span{ entry_buffer_.begin(), size_ }; }

        /**
         * @brief Getter of the configuration.
         *
         * @return Returns a const reference to the member variable #config_.
         * @see ref
         */
        [[nodiscard]] constexpr auto get_config() const -> const Config& { return config_; }

        /**
         * @brief Getter of n_entries_
         *
         * @return Total number of entries read by the current instance
         **/
        [[nodiscard]] constexpr auto get_n_entries() const -> std::size_t { return n_entries_; }

        /**
         * @brief Checks if last read operation reached end of file.
         * @return Returns true if end of file is reached.
         **/
        [[nodiscard]] auto is_end_of_file() const -> bool { return end_of_file_; }

      private:
        BufferType entry_buffer_;        //!< A vector containing all entrypoints of the current entry.
        RawBufferType raw_entry_buffer_; //!< A buffer to store raw data coming from file stream.
        Config config_;                  //!< Member variable for the configuration.
        std::ifstream input_file_;       //!< Input file handler
        std::size_t size_{};             //!< Number of Entrypoints in the current entry
        std::size_t n_entries_{};        //!< Total number of entries read by this instance
        bool end_of_file_{ false };      //!< Indicates if end of file is reached. Gets updated on read.

        void reset();
        auto read_entry_to_buffer(uint32_t read_size) -> EnumError<>;
    };
} // namespace centipede::reader
