#include <pontella.hpp>
#include <sepia.hpp>

#include <array>
#include <iostream>

/// Status represents the reading state.
enum class Status {
    tdLate,
    apsLate,
    tdEndOfFile,
    apsEndOfFile,
};

int main(int argc, char* argv[]) {
    auto showHelp = false;
    try {
        const auto command = pontella::parse(argc, argv, 3, {}, {{"help", "h"}});
        if (command.flags.find("help") != command.flags.end()) {
            showHelp = true;
        } else {
            if (command.arguments[0] == command.arguments[1]) {
                throw std::runtime_error("The td and aps inputs must be different files, and cannot be both null");
            }
            if (command.arguments[0] == command.arguments[2]) {
                throw std::runtime_error("The td input and the es output must be different files");
            }
            if (command.arguments[1] == command.arguments[2]) {
                throw std::runtime_error("The aps input and the es output must be different files");
            }
            if (command.arguments[0] == "null" && command.arguments[1] == "null") {
                throw std::runtime_error("null cannot be used for both the td file and aps file");
            }

            auto tdEnabled = true;
            std::ifstream tdFile;
            if (command.arguments[0] == "null") {
                tdEnabled = false;
            } else {
                tdFile.open(command.arguments[0]);
                if (!tdFile.good()) {
                    throw sepia::UnreadableFile(command.arguments[0]);
                }
            }

            auto apsEnabled = true;
            std::ifstream apsFile;
            if (command.arguments[1] == "null") {
                apsEnabled = false;
            } else {
                apsFile.open(command.arguments[1]);
                if (!tdFile.good()) {
                    throw sepia::UnreadableFile(command.arguments[1]);
                }
            }

            sepia::EventStreamWriter eventStreamWriter;
            std::size_t eventsSinceLastPrint = std::numeric_limits<std::size_t>::max();
            uint64_t previousTimestamp = 0;
            auto wrappedEventStreamWriter = [&](sepia::Event event) {
                if (eventsSinceLastPrint >= 1000) {
                    eventsSinceLastPrint = 0;
                    const auto timestampString = std::to_string(event.timestamp);
                    auto separatedTimestampString = std::string();
                    if (timestampString.size() > 3) {
                        const auto begin = std::next(timestampString.begin(), timestampString.size() - (timestampString.size() / 3) * 3);
                        separatedTimestampString = std::string(timestampString.begin(), begin);
                        separatedTimestampString.reserve(timestampString.size() + ((timestampString.size() - 1) / 3));
                        for (auto characterIterator = begin; characterIterator != timestampString.end(); ++characterIterator) {
                            if (std::distance(begin, characterIterator) % 3 == 0) {
                                separatedTimestampString.push_back('.');
                            }
                            separatedTimestampString.push_back(*characterIterator);
                        }
                    } else {
                        separatedTimestampString = timestampString;
                    }
                    std::cout << "\r" << separatedTimestampString << " microseconds converted";
                    std::cout.flush();
                } else {
                    ++eventsSinceLastPrint;
                }
                eventStreamWriter(event);
                previousTimestamp = event.timestamp;
            };
            eventStreamWriter.open(command.arguments[2]);

            auto tdBytes = std::array<unsigned char, 8>();
            auto apsBytes = std::array<unsigned char, 8>();
            if (tdEnabled) {
                for (;;) {
                    if (tdFile.peek() != '%') {
                        break;
                    }
                    for (;!tdFile.eof() && tdFile.get() != '\n';) {}
                    if (tdFile.eof()) {
                        throw std::runtime_error("The td file is empty");
                    }
                }
                tdFile.get();
                tdFile.get();
                tdFile.read(const_cast<char*>(reinterpret_cast<const char*>(tdBytes.data())), tdBytes.size());
                if (!apsEnabled && tdFile.eof()) {
                    throw std::runtime_error("The td file is empty");
                }
            }
            if (apsEnabled) {
                for (;;) {
                    if (apsFile.peek() != '%') {
                        break;
                    }
                    for (;!apsFile.eof() && apsFile.get() != '\n';) {}
                    if (apsFile.eof()) {
                        throw std::runtime_error("The td file is empty");
                    }
                }
                apsFile.get();
                apsFile.get();
                apsFile.read(const_cast<char*>(reinterpret_cast<const char*>(apsBytes.data())), apsBytes.size());
                if (!tdEnabled && apsFile.eof()) {
                    throw std::runtime_error("The aps file is empty");
                }
            }
            if (tdEnabled && apsEnabled && tdFile.eof() && apsFile.eof()) {
                throw std::runtime_error("Both the td file and the aps file are empty");
            }

            auto tdEvent = sepia::Event{};
            auto apsEvent = sepia::Event{};
            auto status = Status::tdLate;

            auto thresholdsTimestamps = std::array<uint64_t, 304 * 240>();
            thresholdsTimestamps.fill(0);

            if (!tdEnabled || tdFile.eof()) {
                status = Status::tdEndOfFile;
            } else {
                for (;;) {
                    tdEvent = sepia::Event{
                        static_cast<uint16_t>(((static_cast<uint16_t>(tdBytes[5]) & 0x01) << 8) | tdBytes[4]),
                        static_cast<uint16_t>(240 - 1 - (((tdBytes[6] & 0x01) << 7) | ((tdBytes[5] & 0xfe) >> 1))),
                        static_cast<uint64_t>(tdBytes[0])
                        | (static_cast<int64_t>(tdBytes[1]) << 8)
                        | (static_cast<int64_t>(tdBytes[2]) << 16)
                        | (static_cast<int64_t>(tdBytes[3]) << 24),
                        false,
                        ((tdBytes[6] & 0x2) >> 1) == 0x01,
                    };
                    if (tdEvent.timestamp >= previousTimestamp) {
                        break;
                    }
                    tdFile.read(const_cast<char*>(reinterpret_cast<const char*>(tdBytes.data())), tdBytes.size());
                    if (tdFile.eof()) {
                        status = Status::tdEndOfFile;
                        break;
                    }
                }
            }
            if (!apsEnabled || apsFile.eof()) {
                status = Status::apsEndOfFile;
            } else {
                for (;;) {
                    apsEvent = sepia::Event{
                        static_cast<uint16_t>(((static_cast<uint16_t>(apsBytes[5]) & 0x01) << 8) | apsBytes[4]),
                        static_cast<uint16_t>(240 - 1 - (((apsBytes[6] & 0x01) << 7) | ((apsBytes[5] & 0xfe) >> 1))),
                        static_cast<uint32_t>(apsBytes[0])
                        | (static_cast<uint32_t>(apsBytes[1]) << 8)
                        | (static_cast<uint32_t>(apsBytes[2]) << 16)
                        | (static_cast<uint32_t>(apsBytes[3]) << 24),
                        true,
                        ((apsBytes[6] & 0x2) >> 1) == 0x01,
                    };
                    if (apsEvent.timestamp >= previousTimestamp && apsEvent.timestamp > thresholdsTimestamps[apsEvent.x + 304 * apsEvent.y]) {
                        thresholdsTimestamps[apsEvent.x + 304 * apsEvent.y] = apsEvent.timestamp;
                        break;
                    }
                    apsFile.read(const_cast<char*>(reinterpret_cast<const char*>(apsBytes.data())), apsBytes.size());
                    if (apsFile.eof()) {
                        status = Status::apsEndOfFile;
                        break;
                    }
                }
            }
            if (!tdFile.eof() && !apsFile.eof()) {
                if (tdEvent.timestamp > apsEvent.timestamp) {
                    status = Status::apsLate;
                } else {
                    status = Status::tdLate;
                }
            }

            for (auto done = false; !done;) {
                switch (status) {
                    case Status::tdLate: {
                        wrappedEventStreamWriter(tdEvent);
                        tdFile.read(const_cast<char*>(reinterpret_cast<const char*>(tdBytes.data())), tdBytes.size());
                        if (tdFile.eof()) {
                            status = Status::tdEndOfFile;
                        } else {
                            for (;;) {
                                tdEvent = sepia::Event{
                                    static_cast<uint16_t>(((static_cast<uint16_t>(tdBytes[5]) & 0x01) << 8) | tdBytes[4]),
                                    static_cast<uint16_t>(240 - 1 - (((tdBytes[6] & 0x01) << 7) | ((tdBytes[5] & 0xfe) >> 1))),
                                    static_cast<uint64_t>(tdBytes[0])
                                    | (static_cast<uint64_t>(tdBytes[1]) << 8)
                                    | (static_cast<uint64_t>(tdBytes[2]) << 16)
                                    | (static_cast<uint64_t>(tdBytes[3]) << 24),
                                    false,
                                    ((tdBytes[6] & 0x2) >> 1) == 0x01,
                                };
                                if (tdEvent.timestamp >= previousTimestamp) {
                                    if (tdEvent.timestamp > apsEvent.timestamp) {
                                        status = Status::apsLate;
                                    }
                                    break;
                                }
                                tdFile.read(const_cast<char*>(reinterpret_cast<const char*>(tdBytes.data())), tdBytes.size());
                                if (tdFile.eof()) {
                                    status = Status::tdEndOfFile;
                                    break;
                                }
                            }
                        }
                        break;
                    }
                    case Status::apsLate: {
                        wrappedEventStreamWriter(apsEvent);
                        apsFile.read(const_cast<char*>(reinterpret_cast<const char*>(apsBytes.data())), apsBytes.size());
                        if (apsFile.eof()) {
                            status = Status::apsEndOfFile;
                        } else {
                            for (;;) {
                                apsEvent = sepia::Event{
                                    static_cast<uint16_t>(((static_cast<uint16_t>(apsBytes[5]) & 0x01) << 8) | apsBytes[4]),
                                    static_cast<uint16_t>(240 - 1 - (((apsBytes[6] & 0x01) << 7) | ((apsBytes[5] & 0xfe) >> 1))),
                                    static_cast<uint32_t>(apsBytes[0])
                                    | (static_cast<uint32_t>(apsBytes[1]) << 8)
                                    | (static_cast<uint32_t>(apsBytes[2]) << 16)
                                    | (static_cast<uint32_t>(apsBytes[3]) << 24),
                                    true,
                                    ((apsBytes[6] & 0x2) >> 1) == 0x01,
                                };
                                if (apsEvent.timestamp >= previousTimestamp && apsEvent.timestamp > thresholdsTimestamps[apsEvent.x + 304 * apsEvent.y]) {
                                    thresholdsTimestamps[apsEvent.x + 304 * apsEvent.y] = apsEvent.timestamp;
                                    if (apsEvent.timestamp >= tdEvent.timestamp) {
                                        status = Status::tdLate;
                                    }
                                    break;
                                }
                                apsFile.read(const_cast<char*>(reinterpret_cast<const char*>(apsBytes.data())), apsBytes.size());
                                if (apsFile.eof()) {
                                    status = Status::apsEndOfFile;
                                    break;
                                }
                            }
                        }
                        break;
                    }
                    case Status::tdEndOfFile: {
                        wrappedEventStreamWriter(apsEvent);
                        apsFile.read(const_cast<char*>(reinterpret_cast<const char*>(apsBytes.data())), apsBytes.size());
                        if (apsFile.eof()) {
                            done = true;
                        } else {
                            for (;;) {
                                apsEvent = sepia::Event{
                                    static_cast<uint16_t>(((static_cast<uint16_t>(apsBytes[5]) & 0x01) << 8) | apsBytes[4]),
                                    static_cast<uint16_t>(240 - 1 - (((apsBytes[6] & 0x01) << 7) | ((apsBytes[5] & 0xfe) >> 1))),
                                    static_cast<uint32_t>(apsBytes[0])
                                    | (static_cast<uint32_t>(apsBytes[1]) << 8)
                                    | (static_cast<uint32_t>(apsBytes[2]) << 16)
                                    | (static_cast<uint32_t>(apsBytes[3]) << 24),
                                    true,
                                    ((apsBytes[6] & 0x2) >> 1) == 0x01,
                                };
                                if (apsEvent.timestamp >= previousTimestamp && apsEvent.timestamp > thresholdsTimestamps[apsEvent.x + 304 * apsEvent.y]) {
                                    thresholdsTimestamps[apsEvent.x + 304 * apsEvent.y] = apsEvent.timestamp;
                                    break;
                                }
                                apsFile.read(const_cast<char*>(reinterpret_cast<const char*>(apsBytes.data())), apsBytes.size());
                                if (apsFile.eof()) {
                                    done = true;
                                    break;
                                }
                            }
                        }
                        break;
                    }
                    case Status::apsEndOfFile: {
                        wrappedEventStreamWriter(tdEvent);
                        tdFile.read(const_cast<char*>(reinterpret_cast<const char*>(tdBytes.data())), tdBytes.size());
                        if (tdFile.eof()) {
                            done = true;
                        } else {
                            for (;;) {
                                tdEvent = sepia::Event{
                                    static_cast<uint16_t>(((static_cast<uint16_t>(tdBytes[5]) & 0x01) << 8) | tdBytes[4]),
                                    static_cast<uint16_t>(240 - 1 - (((tdBytes[6] & 0x01) << 7) | ((tdBytes[5] & 0xfe) >> 1))),
                                    static_cast<uint64_t>(tdBytes[0])
                                    | (static_cast<uint64_t>(tdBytes[1]) << 8)
                                    | (static_cast<uint64_t>(tdBytes[2]) << 16)
                                    | (static_cast<uint64_t>(tdBytes[3]) << 24),
                                    false,
                                    ((tdBytes[6] & 0x2) >> 1) == 0x01,
                                };
                                if (tdEvent.timestamp >= previousTimestamp) {
                                    break;
                                }
                                tdFile.read(const_cast<char*>(reinterpret_cast<const char*>(tdBytes.data())), tdBytes.size());
                                if (tdFile.eof()) {
                                    done = true;
                                    break;
                                }
                            }
                        }
                        break;
                    }
                }
            }
            std::cout << "\rThe conversion has successfully completed" << std::endl;
        }
    } catch (const std::runtime_error& exception) {
        showHelp = true;
        if (!pontella::test(argc, argv, {"help", "h"})) {
            std::cout << "\e[31m" << exception.what() << "\e[0m" << std::endl;
        }
    }

    if (showHelp) {
        std::cout <<
            "DatToEs converts a td file and an aps file into an Event Stream file\n"
            "Syntax: ./datToEs [options] /path/to/input_td.dat /path/to/input_aps.dat /path/to/output.es\n"
            "    If the characters chain 'null' (without quotes) is given for the td / aps file,\n"
            "    the Event Stream file is build from the aps / td file only\n"
            "Available options:\n"
            "    -h, --help    shows this help message\n"
        << std::endl;
    }

    return 0;
}
