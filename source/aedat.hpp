#pragma once

#include "../third_party/sepia/source/sepia.hpp"
#include <algorithm>
#include <array>

namespace aedat {
    /// header bundles a .aedat file header's information.
    struct header {
        // TODO: .aedat possible versions: 1.0, 2.0, 3.0, 3.1, 4.0
        uint8_t major_version;
        uint8_t minor_version;
        uint16_t width;
        uint16_t height;
        // TODO: Store device information?
    };

    /// frame represents a timestamped frame.
    struct frame {
        /// t represents the event's timestamp.
        uint64_t t;

        /// exposure represents the exposure time for the frame.
        float exposure;

        /// pixels contains the frame's grey levels in row major order, starting from the bottom left.
        std::vector<uint16_t> pixels;
    };

    /// imu_event represents the parameters of an IMU measurement.
    SEPIA_PACK(struct imu_event {
        /// t represents the event's timestamp.
        uint64_t t;

        /// a_x is the acceleration's x component in m.s^-2.
        float a_x;

        /// a_y is the acceleration's y component in m.s^-2.
        float a_y;

        /// a_z is the acceleration's z component in m.s^-2.
        float a_z;

        /// temprature is the chip's temprature in °C.
        float temperature;

        /// w_x is the angular velocity's x component in rad.s^-1.
        float w_x;

        /// w_y is the angular velocity's y component in rad.s^-1.
        float w_y;

        /// w_z is the angular velocity's z component in rad.s^-1.
        float w_z;
    });

    /// input_type represents an event on an wire.
    enum class input_type {
        falling_edge,
        rising_edge,
        pulse,
    };

    /// external_input represents the parameters of an external input event.
    SEPIA_PACK(struct external_input {
        /// t represents the event's timestamp.
        uint64_t t;

        /// type is the detected external change type.
        input_type type;
    });

    /// read_header retrieves header information from a .dat file.
    inline header read_header(std::istream& stream) {
        std::vector<std::string> header_lines;
        for (;;) {
            if (stream.peek() != '#' || stream.eof()) {
                break;
            }
            stream.ignore();
            header_lines.emplace_back();
            for (;;) {
                const auto character = stream.get();
                if (stream.eof()) {
                    break;
                }
                if (character == '\n') {
                    break;
                }
                header_lines.back().push_back(character);
            }
        }
        // TODO: No headers, probably AEDAT 1.0
        if (header_lines.empty()
            || std::any_of(header_lines.begin(), header_lines.end(), [](const std::string& header_line) {
                   return std::any_of(header_line.begin(), header_line.end(), [](char character) {
                       return !std::isprint(character) && !std::isspace(character);
                   });
               })) {
            stream.seekg(0, std::istream::beg);
            return {2, 0, 240, 180};
        }
        // stream.ignore(2);
        header stream_header = {};
        for (const auto& header_line : header_lines) {
            std::vector<std::string> words;
            for (auto character : header_line) {
                if (std::isspace(character)) {
                    if (!words.empty() && !words.back().empty()) {
                        words.emplace_back();
                    }
                } else {
                    if (words.empty()) {
                        words.emplace_back();
                    }
                    words.back().push_back(character);
                }
            }
            if (words.size() > 1) {
                try {
                    if (words[0][0] == '!') {
                        stream_header.major_version = static_cast<uint8_t>(words[0][8] - '0');
                        stream_header.minor_version = static_cast<uint8_t>(words[0][10] - '0');
                    } else if (words[0] == "HardwareInterface:") {
                        stream_header.width = 240;
                        stream_header.height = 180;
                    } else if (words[0] == "AEChip:") {
                        stream_header.height = 240;
                        stream_header.height = 180;
                    }
                } catch (const std::invalid_argument&) {
                } catch (const std::out_of_range&) {
                }
            }
        }
        if (stream_header.major_version >= 2 && stream_header.width > 0 && stream_header.height > 0) {
            return stream_header;
        }
        return {2, 0, 240, 180};
    }

    // TODO: did not check if it is an external event ()
    /// bytes_to_dvs_event converts raw bytes to a polarized event.
    /// for AEDAT 1.0 DVS128
    inline sepia::dvs_event bytes_to_dvs_event(std::array<uint8_t, 6> bytes, header stream_header) {
        return {
            static_cast<uint64_t>(bytes[0]) | (static_cast<uint64_t>(bytes[1]) << 8)
                | (static_cast<uint64_t>(bytes[2]) << 16) | (static_cast<uint64_t>(bytes[3]) << 24),
            static_cast<uint16_t>(static_cast<uint16_t>(bytes[4]) >> 1),
            static_cast<uint16_t>(stream_header.height - 1 - static_cast<uint16_t>(bytes[5] & 0b111111)),
            (bytes[4] & 0b1) == 0b1,
        };
    }

    /// bytes_to_davis_event converts raw bytes to a polarized event.
    /// for AEDAT 2.0
    inline sepia::dvs_event bytes_to_davis_event(std::array<uint8_t, 8> bytes, header stream_header) {
        if (stream_header.major_version == 2) {
            if (!(bytes[0] & (1 << 7))) {
                // These 8 bytes represent a DVS event
                return {
                    static_cast<uint64_t>(bytes[7]) | (static_cast<uint64_t>(bytes[6]) << 8)
                        | (static_cast<uint64_t>(bytes[5]) << 16) | (static_cast<uint64_t>(bytes[4]) << 24),
                    static_cast<uint16_t>(
                        (static_cast<uint16_t>(bytes[1] & 0b111111) << 4) | (static_cast<uint16_t>(bytes[2]) >> 4)),
                    static_cast<uint16_t>(
                        (static_cast<uint16_t>(bytes[0] & 0b1111111) << 2) | (static_cast<uint16_t>(bytes[1]) >> 6)),
                    (bytes[2] & 0b1000) == 0b1000,
                };
            } else {
                // These 8 bytes represent an APS or IMU event, what should we return?
                return {
                    static_cast<uint64_t>(0),
                    static_cast<uint16_t>(0),
                    static_cast<uint16_t>(0),
                    true,
                };
            }
        }
        // How about other versions of AEDAT files? Don't bother to support but need to return something?
        return {
            static_cast<uint64_t>(0),
            static_cast<uint16_t>(0),
            static_cast<uint16_t>(0),
            true,
        };
    }

    /// dvs_observable dispatches DVS events from a dvs stream.
    /// The header must be read from the stream before calling this function.
    template <typename HandleEvent>
    inline void dvs_observable(std::istream& stream, header stream_header, HandleEvent handle_event) {
        uint64_t previous_t = 0;
        for (;;) {
            std::array<uint8_t, 6> bytes;
            stream.read(reinterpret_cast<char*>(bytes.data()), bytes.size());
            if (stream.eof()) {
                break;
            }
            const auto dvs_event = bytes_to_dvs_event(bytes, stream_header);
            if (dvs_event.t >= previous_t && dvs_event.x < stream_header.width && dvs_event.y < stream_header.height) {
                handle_event(dvs_event);
                previous_t = dvs_event.t;
            }
        }
    }

    /// davis_observable dispatches DVS events from a dvs stream.
    /// The header must be read from the stream before calling this function.
    template <typename HandleEvent>
    inline void davis_observable(std::istream& stream, header stream_header, HandleEvent handle_event) {
        uint64_t previous_t = 0;
        for (;;) {
            std::array<uint8_t, 8> bytes;
            stream.read(reinterpret_cast<char*>(bytes.data()), bytes.size());
            if (stream.eof()) {
                break;
            }
            // TODO: Need the returned data to be a valid sepia::dvs_event, other dump
            const auto dvs_event = bytes_to_davis_event(bytes, stream_header);
            if (dvs_event.t != 0) {
                if (dvs_event.t >= previous_t && dvs_event.x < stream_header.width
                    && dvs_event.y < stream_header.height) {
                    handle_event(dvs_event);
                    previous_t = dvs_event.t;
                }
            }
        }
    }
}
