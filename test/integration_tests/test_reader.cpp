#include "centipede/reader/binary.hpp"
#include "centipede/util/return_types.hpp"
#include <cstddef>
#include <cstdlib>
#include <print>

auto main() -> int
{
    auto reader = centipede::reader::Binary{ centipede::reader::Binary::Config{ .in_filename = "output.bin" } };
    auto init_err = reader.init();

    if (not init_err.has_value())
    {
        std::println(stderr, "Error: {}", init_err.error());
        return EXIT_FAILURE;
    }

    auto read_err = centipede::EnumError<std::size_t>{};

    while (not reader.is_end_of_file())
    {
        read_err = reader.read_one_entry();
        if (not read_err.has_value())
        {
            std::println(stderr, "Error: {}", read_err.error());
            return EXIT_FAILURE;
        }
    }

    return EXIT_SUCCESS;
}
