#include "../third_party/pontella/source/pontella.hpp"
#include "aedat.hpp"

int main(int argc, char* argv[]) {
    return pontella::main(
        {"aedat_to_es converts an aedat file into an Event Stream file",
         "Syntax: ./aedat_to_es [options] /path/to/input_aedat.dat /path/to/output.es",
         "Available options:",
         "    -h, --help    shows this help message"},
        argc,
        argv,
        2,
        {},
        {},
        [](pontella::command command) {
            if (command.arguments[0] == command.arguments[1]) {
                throw std::runtime_error("The aedat input and the Event Stream output must be different files");
            }

            auto stream = sepia::filename_to_ifstream(command.arguments[0]);
            const auto header = aedat::read_header(*stream);
            if (header.major_version == 1) {
                aedat::dvs_observable(
                    *stream,
                    header,
                    sepia::write<sepia::type::dvs>(
                        sepia::filename_to_ofstream(command.arguments[1]), header.width, header.height));
            } else {
                aedat::davis_observable(
                    *stream,
                    header,
                    sepia::write<sepia::type::dvs>(
                        sepia::filename_to_ofstream(command.arguments[1]), header.width, header.height));
            }
        });
}
