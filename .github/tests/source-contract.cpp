// Offline only: never opens a device, registers a driver or changes settings.
#define NOMINMAX
#include "vdd_control_ioctl.h"
#include "vdd_cursor_shared.h"
#include "DefaultEdid.h"

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

static void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

static void CheckEdid(VddEdid::Profile profile, const BYTE* original,
                      const std::string& output) {
    const auto bytes = VddEdid::GetDefaultEdid(profile);
    Require(bytes == std::vector<BYTE>(original, original + 256), "Default EDID changed");
    Require(bytes[126] == 1 && bytes[128] == 2 && bytes[129] == 3, "Invalid CTA extension");
    for (size_t block = 0; block < 2; ++block) {
        unsigned int sum = 0;
        for (size_t i = block * 128; i < (block + 1) * 128; ++i) sum += bytes[i];
        Require((sum & 255u) == 0, "EDID checksum invalid");
    }
    const size_t end = 128u + bytes[130];
    Require(end >= 132 && end <= 255, "Invalid CTA boundary");
    for (size_t pos = 132; pos < end;) {
        pos += 1u + (bytes[pos] & 31u);
        Require(pos <= end, "CTA data block overruns boundary");
    }
    auto changed = bytes;
    changed[21] ^= 1;
    Require(VddEdid::GetDefaultEdid(profile) == bytes, "EDID template mutated");
    std::ofstream file(output, std::ios::binary);
    file.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    Require(file.good(), "Cannot save EDID fixture");
}

int main(int argc, char** argv) {
    try {
        Require(argc == 2, "Expected evidence directory");
        const std::string output = argv[1];
        CheckEdid(VddEdid::Profile::Legacy, VddEdid::DEFAULT_EDID_DATA_LEGACY, output + "/legacy-default.bin");
        CheckEdid(VddEdid::Profile::Modern, VddEdid::DEFAULT_EDID_DATA_MODERN, output + "/modern-default.bin");
        Require(GetDefaultEdid() == VddEdid::GetDefaultEdid(VddEdid::Profile::Legacy), "Legacy API changed");
        Require(VddEdid::GetDefaultEdid(VddEdid::Profile::Auto) ==
                VddEdid::GetDefaultEdid(VddEdid::Profile::Modern), "Auto template changed");
        std::cout << "{\n"
            << "  \"default_edid_checks\": \"PASS\",\n"
            << "  \"ioctl_command\": " << IOCTL_VDD_COMMAND << ",\n"
            << "  \"ioctl_ping\": " << IOCTL_VDD_PING << ",\n"
            << "  \"ioctl_frame_caps\": " << IOCTL_VDD_QUERY_FRAME_CHANNEL_CAPS << ",\n"
            << "  \"ioctl_open_frame\": " << IOCTL_VDD_OPEN_FRAME_CHANNEL << ",\n"
            << "  \"caps_version\": " << VDD_FRAME_CHANNEL_CAPS_VERSION << ",\n"
            << "  \"open_version\": " << VDD_FRAME_CHANNEL_OPEN_VERSION << ",\n"
            << "  \"max_slots\": " << VDD_FRAME_CHANNEL_MAX_SLOTS << ",\n"
            << "  \"caps_size\": " << sizeof(VDD_FRAME_CHANNEL_CAPS) << ",\n"
            << "  \"open_request_size\": " << sizeof(VDD_FRAME_CHANNEL_OPEN_REQUEST) << ",\n"
            << "  \"open_response_size\": " << sizeof(VDD_FRAME_CHANNEL_OPEN_RESPONSE) << ",\n"
            << "  \"cursor_magic\": " << VDD_CURSOR_MAGIC << ",\n"
            << "  \"cursor_version\": " << VDD_CURSOR_VERSION << ",\n"
            << "  \"cursor_size\": " << sizeof(VDD_CURSOR_SHARED_METADATA) << ",\n"
            << "  \"cursor_sequence_offset\": " << offsetof(VDD_CURSOR_SHARED_METADATA, PublicationSequence) << ",\n"
            << "  \"cursor_qpc_offset\": " << offsetof(VDD_CURSOR_SHARED_METADATA, LastUpdateQpc) << "\n}\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
