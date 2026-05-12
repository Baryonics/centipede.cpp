#include "centipede/reader/binary.hpp"
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

    for (const auto& entry : reader)
    {
        if (not entry.has_value())
        {
            std::println(stderr, "Error: {}", entry.error());
            return EXIT_FAILURE;
        }
    }
    return EXIT_SUCCESS;
}
