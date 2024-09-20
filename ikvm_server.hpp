#pragma once

#include "ami/include/ikvm_utils.hpp"
#include "ikvm_args.hpp"
#include "ikvm_input.hpp"
#include "ikvm_video.hpp"

namespace ikvm
{
/*
 * @class Server
 * @brief Manages the RFB server connection and updates
 */
class Server
{
  public:
    /*
     * @struct ClientData
     * @brief Store necessary data for each connected RFB client
     */
    struct ClientData
    {
        /*
         * @brief Constructs ClientData object
         *
         * @param[in] s - Number of frames to skip when client connects
         * @param[in] i - Pointer to Input object
         */

        ClientData(int s, Input* i) : skipFrame(s), input(i), last_crc{-1}
        {
            needUpdate = false;
            lastActivityTime = std::chrono::steady_clock::now();
            sessionId = 0;
        }
        ~ClientData() = default;
        ClientData(const ClientData&) = default;
        ClientData& operator=(const ClientData&) = default;
        ClientData(ClientData&&) = default;
        ClientData& operator=(ClientData&&) = default;

        int skipFrame;
        Input* input;
        bool needUpdate;
        int64_t last_crc;
        uint8_t sessionId;
        /* @brief Getting last activity time based on key and pointer event */
        std::chrono::time_point<std::chrono::steady_clock> lastActivityTime;
    };

    /*
     * @brief Constructs Server object
     *
     * @param[in] args - Reference to Args object
     * @param[in] i    - Reference to Input object
     * @param[in] v    - Reference to Video object
     */
    Server(const Args& args, Input& i, Video& v);
    ~Server();
    Server(const Server&) = default;
    Server& operator=(const Server&) = default;
    Server(Server&&) = default;
    Server& operator=(Server&&) = default;

    /* @brief Resizes the RFB framebuffer */
    void resize();
    /* @brief Executes any pending RFB updates and client input */
    void run();
    /* @brief Sends pending video frame to clients */
    void sendFrame();

    /*
     * @brief Indicates whether or not video data is desired
     *
     * @return Boolean to indicate whether any clients need a video frame
     */
    inline bool wantsFrame() const
    {
        return server->clientHead;
    }
    /*
     * @brief Get the Video object
     *
     * @return Reference to the Video object
     */
    inline const Video& getVideo() const
    {
        return video;
    }

  private:
    /*
     * @brief Handler for a client frame update message
     *
     * @param[in] cl - Handle to the client object
     * @param[in] furMsg - Pointer of the FUR message
     */
    static void clientFramebufferUpdateRequest(
        rfbClientPtr cl, rfbFramebufferUpdateRequestMsg* furMsg);
    /*
     * @brief Handler for a client disconnecting
     *
     * @param[in] cl - Handle to the client object
     */
    static void clientGone(rfbClientPtr cl);
    /*
     * @brief Handler for client connecting
     *
     * @param[in] cl - Handle to the client object
     */
    static enum rfbNewClientAction newClient(rfbClientPtr cl);

    /* @brief Performs the resize operation on the framebuffer */
    void doResize();

    /*
     * @brief Updates USB Power Save Mode Status. (AMI Extension)
     *
     * @param[in] status 0 for disable 1 for enable
     */
    static void updatePowerSaveMode(int status);

    /* @brief Boolean to indicate if a resize operation is on-going */
    bool pendingResize;
    /* @brief Number of frames handled since a client connected */
    int frameCounter;
    /* @brief Number of connected clients */
    unsigned int numClients;
    /* @brief Microseconds to process RFB events every frame */
    long int processTime;
    /* @brief Handle to the RFB server object */
    rfbScreenInfoPtr server;
    /* @brief Reference to the Input object */
    Input& input;
    /* @brief Reference to the Video object */
    Video& video;
    /* @brief Default framebuffer storage */
    std::vector<char> framebuffer;
    /* @brief Identical frames detection */
    bool calcFrameCRC;
    /* @brief Cursor bitmap width */
    static constexpr int cursorWidth = 20;
    /* @brief Cursor bitmap height */
    static constexpr int cursorHeight = 20;
    /* @brief Cursor bitmap */
    static constexpr char cursor[] =
        "                    "
        " x                  "
        " xx                 "
        " xxx                "
        " xxxx               "
        " xxxxx              "
        " xxxxxx             "
        " xxxxxxx            "
        " xxxxxxxx           "
        " xxxxxxxxx          "
        " xxxxxxxxxx         "
        " xxxxxxxxxxx        "
        " xxxxxxx            "
        " xxxxxxx            "
        " xxx  xxx           "
        " xx   xxx           "
        " x     xxx          "
        "       xxx          "
        "        x           "
        "                    ";
    /* @brief Cursor bitmap mask */
    static constexpr char cursorMask[] =
        " o                  "
        "oxo                 "
        "oxxo                "
        "oxxxo               "
        "oxxxxo              "
        "oxxxxxo             "
        "oxxxxxxo            "
        "oxxxxxxxo           "
        "oxxxxxxxxo          "
        "oxxxxxxxxxo         "
        "oxxxxxxxxxxo        "
        "oxxxxxxxxxxxo       "
        "oxxxxxxxoooo        "
        "oxxxxxxxo           "
        "oxxxooxxxo          "
        "oxxo oxxxo          "
        "oxo   oxxxo         "
        " o    oxxxo         "
        "       oxo          "
        "        o           ";
};

} // namespace ikvm
