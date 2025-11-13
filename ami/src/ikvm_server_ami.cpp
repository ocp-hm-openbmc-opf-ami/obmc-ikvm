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

namespace ikvm
{
void Server::updatePowerSaveMode(int status)
{
    if ((status == 0) || (status == 1))
    {
        try
        {
            auto bus = sdbusplus::bus::new_system();
            auto methodCall = bus.new_method_call(
                "xyz.openbmc_project.Settings",
                "/xyz/openbmc_project/logging/settings",
                "xyz.openbmc_project.USB", "SetUSBPowerSaveMode");
            methodCall.append(status);
            bus.call(methodCall);
        }

        catch (const sdbusplus::exception::SdBusError& e)
        {
            log<level::ERR>("D-Bus call Failed", entry("ERROR=%s", e.what()));
            return;
        }

        catch (const std::exception& e)
        {
            log<level::ERR>("Error handling for powersavemode",
                            entry("ERROR=%s", e.what()));
            return;
        }
    }
}

void Server::handleKVMServiceDisabled(rfbScreenInfoPtr rfbScreen)
{
    sendDisconnectMessageToClients(rfbScreen, IVTP_STOP_SESSION_IMMEDIATE,
                                   STOP_SESSION_IMMEDIATE);
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

std::vector<unsigned char> Server::createIVTPStopSessionPacket(
    unsigned short stopReason, unsigned short status)
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

/* For session timeout implementation */
void Server::sessionTimeOut(rfbClientPtr cl)
{
    auto currentTime = std::chrono::steady_clock::now();
    ClientData* cd = (ClientData*)cl->clientData;

    auto timeSinceLastActive = std::chrono::duration_cast<std::chrono::seconds>(
        currentTime - cd->lastActivityTime);

    /* Once the timeSinceLastActive surpasses the timeout value, the client
     * will be disconnected */
    if (timeSinceLastActive >= timeoutValue)
    {
        sendDisconnectMessageToClients(cl->screen, IVTP_STOP_SESSION_IMMEDIATE,
                                       STOP_SESSION_TIMED_OUT);
        rfbCloseClient(cl);
        // Log the event of session timeout
        ikvm::eventLogSupport("OpenBMC.0.1.KVMSessionTimeout");
    }
}

void Server::sessionRegister(rfbClientPtr cl)
{
    ClientData* cd = (ClientData*)cl->clientData;

    if (!cd)
    {
        return;
    }

    try
    {
        /* Method call for Registering */
        log<level::INFO>("Session Registering...");

        auto busRegister = sdbusplus::bus::new_default_system();
        propertyValue propertyval;

        std::string ipAdress = DEFAULT_IP;
        std::string userName = USER_NAME;
        uint8_t sessionType = KVM;
        uint8_t privilege = PRIV_LEVEL_ADMIN;
        uint8_t userId = KVM_DEFAULT_USER_ID;
        std::string mountingMethod = MOUNTING_METHOD;

        if (cd->webSessionId != DEFAULT_SID)
        {
            try
            {
                bool found = false;
                auto msgFetch = busRegister.new_method_call(
                    smgrService.c_str(), smgrObjPath.c_str(),
                    DBUS_PROPERTIES_INTERFACE, "Get");
                msgFetch.append(smgrWebIface.c_str(), "WebSessionInfo");

                auto reply0 = busRegister.call(msgFetch);
                reply0.read(propertyval);
                if (std::holds_alternative<sessionRet>(propertyval))
                {
                    sessionRet& webSesionList =
                        std::get<sessionRet>(propertyval);
                    if (!webSesionList.empty())
                    {
                        auto it = std::find_if(
                            webSesionList.begin(), webSesionList.end(),
                            [cd](const auto& session) {
                                return cd->webSessionId ==
                                       static_cast<uint8_t>(
                                           std::get<0>(session));
                            });
                        if (it != webSesionList.end())
                        {
                            ipAdress = std::get<1>(*it);
                            userName = std::get<2>(*it);
                            sessionType = KVM;
                            privilege = static_cast<uint8_t>(std::get<4>(*it));
                            userId = static_cast<uint8_t>(std::get<5>(*it));
                            mountingMethod = MOUNTING_METHOD;
                            found = true;
                        }
                        else
                        {
                            log<level::DEBUG>(
                                "Web session ID not found in the list",
                                entry("ID=%d",
                                      static_cast<int>(cd->webSessionId)));
                        }
                    }
                }
                if (!found)
                {
                    cd->webSessionId = DEFAULT_SID;
                }
            }
            catch (const std::exception& e)
            {
                log<level::ERR>(" Error getting web session ID",
                                entry("ERROR=%s", e.what()));
                cd->webSessionId = DEFAULT_SID;
            }
        }

        log<level::DEBUG>("Retrieved web session ",
                          entry("ID: %d", cd->webSessionId));

        /* If the web session ID is not received or is set to Default */
        if (cd->webSessionId == DEFAULT_SID)
        {
            if (cd->clientType == ClientData::ClientType::JViewer)
            {
                // JViewer shares the Clientinfo for the Session register
                ipAdress = std::get<1>(cd->clientInfo);
                userName = std::get<2>(cd->clientInfo);
                sessionType = std::get<3>(cd->clientInfo);
                privilege = std::get<4>(cd->clientInfo);
                userId = std::get<5>(cd->clientInfo);
                mountingMethod = MOUNTING_METHOD;
            }
            else
            {
                ipAdress = DEFAULT_IP;
                userName = USER_NAME;
                sessionType = KVM;
                privilege = PRIV_LEVEL_ADMIN;
                userId = KVM_DEFAULT_USER_ID;
                mountingMethod = MOUNTING_METHOD;
            }
        }

        auto m = busRegister.new_method_call(
            smgrService.c_str(), smgrObjPath.c_str(), smgrIface.c_str(),
            "SessionRegister");

        m.append(cd->sessionId, ipAdress, userName, sessionType, privilege,
                 userId, mountingMethod);

        auto reply = busRegister.call(m);
        bool status = false;
        reply.read(status);

        if (status)
        {
            auto msg2 = busRegister.new_method_call(
                smgrService.c_str(), smgrObjPath.c_str(),
                DBUS_PROPERTIES_INTERFACE, "Get");

            msg2.append(smgrKVMIface, "KvmSessionInfo");

            auto reply1 = busRegister.call(msg2);
            reply1.read(propertyval);

            if (std::holds_alternative<sessionRet>(propertyval))
            {
                sessionRet& vec = std::get<sessionRet>(propertyval);
                if (!vec.empty())
                {
                    const auto& latestEntry =
                        vec.back(); /* Get the last element */
                    cd->sessionId =
                        static_cast<uint8_t>(std::get<0>(latestEntry));
                    // Add the session ID to the vector
                    activeSessionIDs.push_back(cd->sessionId);
                }
            }
        }
        cd->isNewSession = false;
    }

    catch (const sdbusplus::exception::SdBusError& e)
    {
        log<level::ERR>("D-Bus call Failed", entry("ERROR=%s", e.what()));
        cd->isNewSession = false;
    }

    catch (const std::exception& e)
    {
        log<level::ERR>(" Error handling for session Registering",
                        entry("ERROR=%s", e.what()));
        cd->isNewSession = false;
    }
}

void Server::clientCutTextMsgHandler(rfbClientPtr cl, const char* text,
                                     uint32_t length)
{
    ClientData* cd = (ClientData*)cl->clientData;

    if (!cd)
    {
        return;
    }
    if (text == nullptr || length <= 0)
    {
        log<level::DEBUG>("Clipboard text is empty or invalid.");
        return;
    }

    IVTPMessage msg;
    msg = parseIvtpBuffer(text, length);
    // Check if the message is a valid IVTP message
    // and if the header matches "IVTP"
    if (msg.header != "IVTP")
    {
        msg.valid = false; // Mark as invalid
        log<level::DEBUG>("Invalid message header, ignoring the message",
                          entry("HEADER=%s", msg.header.c_str()));
        return;
    }

    if (msg.valid == false)
    {
        log<level::DEBUG>("Not a valid custom message, ignoring the message",
                          entry("HEADER=%s", msg.header.c_str()));
        return;
    }

    log<level::DEBUG>(
        "Parsed IVTP message",
        entry("HEADER=%s, NUM=%d, PAYLOAD_LENGTH=%d, STATUS=%d, PAYLOAD=%s",
              msg.header.c_str(), msg.num, msg.payloadLength, msg.status,
              msg.payload.c_str()));

    switch (msg.num)
    {
        case IVTP_VALIDATE_VIDEO_SESSION:
            if (msg.status == IVTP_GET_WEB_TOKEN)
            {
                if (cl->tightEncodingSupport)
                {
                    cd->clientType =
                        ikvm::Server::ClientData::ClientType::H5Viewer;
                    cd->webSessionId = extractSessionId(msg.payload);
                    cd->clientInfoReceived = true;

                    log<level::DEBUG>(
                        "Extracted web session ID",
                        entry("ID=%d", static_cast<int>(cd->webSessionId)));

                    log<level::INFO>("Client type: ** H5Viewer **");
                }
            }
            else if (msg.status == IVTP_SESSION_ACCEPTED)
            {
                if (!cl->tightEncodingSupport)
                {
                    cd->webSessionId = DEFAULT_SID;
                    cd->clientType =
                        ikvm::Server::ClientData::ClientType::JViewer;
                    cd->clientInfoReceived = true;

                    log<level::INFO>("Client type: ** JViewer **");
                    // Parse the JViewer payload to extract client info
                    try
                    {
                        auto kv = parseKeyValueString(msg.payload);
                        uint8_t sessionId = DEFAULT_SID;
                        std::string ipAddress = kv["ipAdress"];
                        std::string userName = kv["userName"];
                        uint8_t privilege =
                            static_cast<uint8_t>(std::stoi(kv["privilege"]));
                        uint8_t userId =
                            static_cast<uint8_t>(std::stoi(kv["userId"]));

                        uint8_t sessionType = KVM;
                        std::string mountType = MOUNTING_METHOD;

                        // Populate the global tuple
                        cd->clientInfo = std::make_tuple(
                            sessionId, ipAddress, userName, sessionType,
                            privilege, userId, mountType);
                    }
                    catch (const std::exception& e)
                    {
                        log<level::ERR>(
                            "Invalid JViewer payload",
                            entry("PAYLOAD=%s", msg.payload.c_str()));

                        cd->clientInfo = std::make_tuple(
                            DEFAULT_SID, DEFAULT_IP, USER_NAME, KVM,
                            PRIV_LEVEL_ADMIN, KVM_DEFAULT_USER_ID,
                            MOUNTING_METHOD);
                    }
                }
            }
            break;
        default:
            log<level::DEBUG>("Unknown IVTP message number",
                              entry("NUM=%d", msg.num));
            break;
    }

    return;
}

IVTPMessage Server::parseIvtpBuffer(const char* buffer, uint32_t totalLength)
{
    IVTPMessage msg;
    msg.valid = false; // Default to invalid
    uint32_t offset = 0;

    // Check if the buffer is large enough to contain the IVTP message
    if (totalLength < IVTP_MIN_SIZE)
    {
        msg.header = "[Invalid: too short]";
        return msg;
    }

    // Read IVTP_HEADER_SIZE-bytes header
    msg.header = std::string(buffer, IVTP_HEADER_SIZE);
    offset += IVTP_HEADER_SIZE;

    if (msg.header == "IVTP")
    {
        msg.valid = true;
        // Read IVTP_NUMBER_SIZE-bytes num (network byte order)
        uint16_t num_net;
        std::memcpy(&num_net, buffer + offset, sizeof(num_net));
        msg.num = ntohs(num_net);
        offset += IVTP_NUMBER_SIZE;

        // Read IVTP_PAYLOAD_LENGTH_HOLDER_SIZE-bytes payload length (network
        // byte order)
        uint32_t payloadLength_net;

        if (offset + IVTP_PAYLOAD_LENGTH_HOLDER_SIZE > totalLength)
        {
            msg.header = "[Invalid: not enough data for payload length]";
            msg.valid = false;
            return msg;
        }

        std::memcpy(&payloadLength_net, buffer + offset,
                    sizeof(payloadLength_net));
        msg.payloadLength = ntohl(payloadLength_net);
        offset += IVTP_PAYLOAD_LENGTH_HOLDER_SIZE;

        // Read IVTP_STATUS_SIZE-bytes status (network byte order)
        uint16_t status_net;
        std::memcpy(&status_net, buffer + offset, sizeof(status_net));
        msg.status = ntohs(status_net);
        offset += IVTP_STATUS_SIZE;
        // Check if payload length is valid

        if (offset + msg.payloadLength <= totalLength)
        {
            msg.payload = std::string(buffer + offset, msg.payloadLength);
        }
        else
        {
            msg.payload = "[Invalid payload length]";
            msg.valid = false;
        }
    }
    else
    {
        msg.header = "[Invalid: not IVTP]";
    }

    return msg;
}

} // namespace ikvm
