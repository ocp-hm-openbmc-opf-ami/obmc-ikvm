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
#define IVTP_STOP_SESSION_IMMEDIATE (0x0008)

#define STATUS_SUCCESS (0x00)

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

void Server::handleKVMServiceDisabled(rfbScreenInfoPtr rfbScreen)
{
    sendDisconnectMessageToClients(rfbScreen, IVTP_STOP_SESSION_IMMEDIATE,
                                   STATUS_SUCCESS);
}

void Server::sendDisconnectMessageToClients(rfbScreenInfoPtr rfbScreen,
                                            unsigned short stopReason,
                                            unsigned short status)
{
    auto ivtpPacket = createIVTPStopSessionPacket(stopReason, status);

    // Send the IVTP message to all connected clients
    for (rfbClientPtr client = rfbScreen->clientHead; client != nullptr;
         client = client->next)
    {
        sendIVTPMessageToClient(client, ivtpPacket.data(), ivtpPacket.size());
    }
}

std::vector<unsigned char>
    Server::createIVTPStopSessionPacket(unsigned short stopReason,
                                        unsigned short status)
{
    constexpr unsigned int ivtpHeaderSize = 12; // IVTP header size (12 bytes)
    std::vector<unsigned char> ivtpPacket(ivtpHeaderSize);

    // IVTP header (12 bytes)
    std::memcpy(&ivtpPacket[0], "IVTP", 4); // IVTP identifier
    unsigned short ivtp_num_be = htons(IVTP_STOP_SESSION_IMMEDIATE);
    std::memcpy(&ivtpPacket[4], &ivtp_num_be, sizeof(ivtp_num_be));
    unsigned int ivtp_len_net = htonl(0);          // No payload in this case
    std::memcpy(&ivtpPacket[6], &ivtp_len_net, sizeof(ivtp_len_net));
    unsigned short ivtp_status_be = htons(status); // Status
    std::memcpy(&ivtpPacket[10], &ivtp_status_be, sizeof(ivtp_status_be));

    return ivtpPacket;
}

void Server::sendIVTPMessageToClient(rfbClientPtr client,
                                     const unsigned char* ivtpPacket,
                                     unsigned int ivtpLength)
{
    constexpr unsigned int headerSize =
        8; // ServerCutText header size (8 bytes)
    unsigned int total_length = headerSize + ivtpLength;

    auto packet = std::make_unique<unsigned char[]>(total_length);

    // Fill ServerCutText header
    packet[0] = SERVER_CUT_TEXT;
    std::memset(&packet[1], 0, 3); // Padding
    unsigned int length =
        htonl(total_length - 8);   // Length excluding the header
    std::memcpy(&packet[4], &length, sizeof(length));

    // Copy the IVTP packet into the ServerCutText message
    std::memcpy(&packet[8], ivtpPacket, ivtpLength);

    // Send the packet
    if (rfbWriteExact(client, reinterpret_cast<const char*>(packet.get()),
                      total_length) < 0)
    {
        std::cerr << "Error writing to client!" << std::endl;
    }

    std::fflush(stdout);
}

} // namespace ikvm
