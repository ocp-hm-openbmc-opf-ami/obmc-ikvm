#pragma once

#include "ikvm_input.hpp"

#include <linux/videodev2.h>

#include <deque>
#include <mutex>
#include <string>
#include <vector>

namespace ikvm
{
/*
 * @class Video
 * @brief Sets up the V4L2 video device and performs read operations
 */
class Video
{
  public:
    /*
     * @brief Constructs Video object
     *
     * @param[in] p     - Path to the V4L2 video device
     * @param[in] input - Reference to the Input object
     * @param[in] fr    - desired frame rate of the video
     */
    Video(const std::string& p, Input& input, int fr = 30, int sub = 0,
          int fmt = 0);
    ~Video();
    Video(const Video&) = default;
    Video& operator=(const Video&) = default;
    Video(Video&&) = default;
    Video& operator=(Video&&) = default;

    /*
     * @brief Gets the video frame data
     *
     * @return Pointer to the video frame data
     */
    char* getData();
    char* getData(unsigned int i);
    /*
     * @brief captures video frame and
     * stores as an image(.jpeg) in BMC local memory(screenShotPath).
     */
    void screenShot(const std::string& screenShotPath);

    /* @brief Performs read to grab latest video frame */
    void getFrame();
    /* @brief Performs return done video frames back to driver */
    void releaseFrames();
    /*
     * @brief Gets whether or not the video frame needs to be resized
     *
     * @return Boolean indicating if the frame needs to be resized
     */
    bool needsResize();
    /* @brief Performs the resize and re-allocates framebuffer */
    void resize();
    /* @brief Performs the video frame jpeg-capture format change operation*/
    void formatChange(int newformat);
    /* @brief Starts streaming from the video device */
    void start();
    /* @brief Stops streaming from the video device */
    void stop();
    /* @brief Restarts streaming from the video device */
    void restart()
    {
        stop();
        start();
    }

    /*
     * @brief Gets the desired video frame rate in frames per second
     *
     * @return Value of the desired frame rate
     */
    inline int getFrameRate() const
    {
        return frameRate;
    }
    /*
     * @brief Gets the size of the video frame data
     *
     * @return Value of the size of the video frame data in bytes
     */
    inline size_t getFrameSize() const
    {
        if (buffersDone.empty())
            return 0;
        return buffers[buffersDone.front()].payload;
    }
    inline size_t getFrameSize(unsigned int i) const
    {
        return buffers[i].payload;
    }
    /*
     * @brief Gets the video frame count in sequence
     *
     * @return Value of video frame count in sequence
     */
    inline size_t getFrameCount() const
    {
        if (buffersDone.empty())
            return 0;
        return buffers[buffersDone.front()].sequence;
    }
    inline size_t getFrameCount(unsigned int i) const
    {
        return buffers[i].sequence;
    }
    /*
     * @brief Gets the height of the video frame
     *
     * @return Value of the height of video frame in pixels
     */
    inline size_t getHeight() const
    {
        return height;
    }
    /*
     * @brief Gets the pixel format  of the video frame
     *
     * @return Value of the pixel format of video frame */
    inline uint32_t getPixelformat() const
    {
        return pixelformat;
    }
    /*
     * @brief Gets the width of the video frame
     *
     * @return Value of the width of video frame in pixels
     */
    inline size_t getWidth() const
    {
        return width;
    }
    /*
     * @brief Gets the subsampling of the video frame
     *
     * @return Value of the subsampling of video frame, 1:420/0:444
     */
    inline int getSubsampling() const
    {
        return subSampling;
    }
    /*
     * @brief Sets the subsampling of the video frame
     *
     * @return Value of the subsampling of video frame, 1:420/0:444
     */
    inline void setSubsampling(int _sub)
    {
        subSampling = _sub;
    }
    /*
     * @brief Gets the jpeg format of the video frame
     *
     * @return Value of the jpeg format of video frame
     *         0:standard jpeg, 1:reserved, 2:partial jpeg
     */
    inline int getFormat() const
    {
        return format;
    }
    /*
     * @brief Sets the jpeg format of the video frame
     *
     * @return Value of the jpeg format of video frame
     *         0:standard jpeg, 1:reserved, 2:partial jpeg
     */
    inline void setFormat(int _fmt)
    {
        format = _fmt;
    }
    /*
     * @brief gets the jpeg format of the video frame
     * set by OpenBMC ipKVM daemon.
     *
     * @return Value of the jpeg format of video frame
     *         0:standard jpeg, 1:reserved, 2:Partial jpeg
     */
    inline int getOriginalFormat() const
    {
        return originalFormat;
    }
    /*
     * @brief Gets the bounding-box of the partial-jpeg
     *
     * @return Bounding-box of the video frame
     */
    inline v4l2_rect getBoundingBox(unsigned int i) const
    {
        return buffers[i].box;
    }

    /* @brief Number of bits per component of a pixel */
    static const int bitsPerSample;
    /* @brief Number of bytes of storage for a pixel */
    static const int bytesPerPixel;
    /* @brief Number of components in a pixel (i.e. 3 for RGB pixel) */
    static const int samplesPerPixel;
    /* @brief done buffer storage */
    std::deque<int> buffersDone;

  private:
    void qbuf(int i);
    /*
     * @struct Buffer
     * @brief Store the address and size of frame data from streaming
     *        operations
     */
    struct Buffer
    {
        Buffer() : data(nullptr), queued(false), payload(0), size(0)
        {}
        ~Buffer() = default;
        Buffer(const Buffer&) = default;
        Buffer& operator=(const Buffer&) = default;
        Buffer(Buffer&&) = default;
        Buffer& operator=(Buffer&&) = default;

        void* data;
        bool queued;
        size_t payload;
        size_t size;
        uint32_t sequence;
        v4l2_rect box;
    };

    /*
     * @brief Boolean to indicate whether the resize was triggered during
     *        the open operation
     */
    bool resizeAfterOpen;
    /* @brief Indicates whether or not timings query was last sucessful */
    bool timingsError;
    /* @brief File descriptor for the V4L2 video device */
    int fd;
    /* @brief Desired frame rate of video stream in frames per second */
    int frameRate;
    /* @brief Buffer index for the last video frame */
    int lastFrameIndex;
    /* @brief Height in pixels of the video frame */
    size_t height;
    /* @brief Width in pixels of the video frame */
    size_t width;
    /* @brief jpeg's subsampling, 1:420/0:444 */
    int subSampling;
    /* @brief Reference to the Input object */
    Input& input;
    /* @brief jpeg format */
    int format;
    /* @brief jpeg fomat set by openBMC ikvm Daemon*/
    int originalFormat;
    /* @brief Path to the V4L2 video device */
    const std::string path;
    /* @brief Streaming buffer storage */
    std::vector<Buffer> buffers;

    /* @brief Pixel Format  */
    uint32_t pixelformat;
};

} // namespace ikvm
