/*
 * ****************************************************************************
 *
 * AMI Extension of Server Class
 * Filename : ikvm_server_ami.cpp
 *
 * @brief AMI Extension of Server Class to support various Feature
 * Implementation.
 *
 *
 * Author: Amlana Bhuyan [amlanab@ami.com]
 *
 * ****************************************************************************
 */

#include "ami/include/ikvm_utils.hpp"
#include "ikvm_server.hpp"

#define SERVER_CUT_TEXT 3
#define IVTP_STOP_SESSION_IMMEDIATE     (0x01)

namespace ikvm
{
void Server::updatePowerSaveMode(int status)
{
    if ((status == 0) || (status == 1))
    {
        auto bus = sdbusplus::bus::new_system();
        auto methodCall = bus.new_method_call(
            "xyz.openbmc_project.Settings",
            "/xyz/openbmc_project/logging/settings", "xyz.openbmc_project.USB",
            "SetUSBPowerSaveMode");
        methodCall.append(status);
        bus.call(methodCall);
    }
}

// Server function to handle KVM service disabled
void Server::handleKVMServiceDisabled(rfbScreenInfoPtr rfbScreen) {
    sendDisconnectMessageToClients(rfbScreen, "Disabled");
}

// Function to send a disconnect message to all clients
void Server::sendDisconnectMessageToClients(rfbScreenInfoPtr rfbScreen, const std::string& message) {
    for (rfbClientPtr client = rfbScreen->clientHead; client != nullptr; client = client->next) {
        sendDisconnectMessageToClient(client, message);
    }
}

void Server::sendDisconnectMessageToClient(rfbClientPtr client, const std::string& message) {
    constexpr unsigned int headerSize = 8 + 4;  // ServerCutText (8 bytes) + IVTP header (12 bytes)
    unsigned int total_length = headerSize + message.size();

    auto packet = std::make_unique<unsigned char[]>(total_length);

    // Fill ServerCutText header and IVTP header
    packet[0] = SERVER_CUT_TEXT;
    std::memset(&packet[1], 0, 7);  // Padding and length (set later)
    unsigned int length = htonl(total_length - 8);
    std::memcpy(&packet[4], &length, sizeof(length));

    std::memcpy(&packet[8], "IVTP", 4);  // IVTP identifier
    unsigned short ivtp_num_be = htons(IVTP_STOP_SESSION_IMMEDIATE);
    std::memcpy(&packet[12], &ivtp_num_be, sizeof(ivtp_num_be));
    unsigned int ivtp_len_net = htonl(message.size());
    std::memcpy(&packet[14], &ivtp_len_net, sizeof(ivtp_len_net));
    unsigned short ivtp_status_be = htons(0);  // Status
    std::memcpy(&packet[18], &ivtp_status_be, sizeof(ivtp_status_be));

    // Copy the message payload
    std::memcpy(&packet[20], message.c_str(), message.size());

    // Send the packet
    if (rfbWriteExact(client, reinterpret_cast<const char*>(packet.get()), total_length) < 0) {
        std::cerr << "Error writing to client!" << std::endl;
    }

    std::fflush(stdout);
}

} // namespace ikvm
