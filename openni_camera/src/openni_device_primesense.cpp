/*
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2011 2011 Willow Garage, Inc.
 *    Suat Gedikli <gedikli@willowgarage.com>
 *
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of Willow Garage, Inc. nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
 */
#include <openni_camera/openni_device_primesense.h>
#include <openni_camera/openni_image_yuv_422.h>
#include <iostream>
#include <sstream>
#include <boost/thread/mutex.hpp>

using namespace boost;

namespace openni_wrapper
{

DevicePrimesense::DevicePrimesense (xn::Context& context, const xn::NodeInfo& device_node, const xn::NodeInfo& image_node, const xn::NodeInfo& depth_node, const xn::NodeInfo& ir_node) 
: OpenNIDevice (context, device_node, image_node, depth_node, ir_node)
{
  // setup stream modes
  enumAvailableModes ();
  setDepthOutputMode (getDefaultDepthMode ());
  setImageOutputMode (getDefaultImageMode ());
  setIROutputMode (getDefaultIRMode ());

  unique_lock<mutex> image_lock(image_mutex_);
  XnStatus status = image_generator_.SetIntProperty ("InputFormat", 5);
  if (status != XN_STATUS_OK)
    THROW_OPENNI_EXCEPTION ("Error setting the image input format to Uncompressed 8-bit BAYER. Reason: %s", xnGetStatusString (status));

  status = image_generator_.SetPixelFormat (XN_PIXEL_FORMAT_YUV422);
  if (status != XN_STATUS_OK)
    THROW_OPENNI_EXCEPTION ("Failed to set image pixel format to YUV422. Reason: %s", xnGetStatusString (status));

  image_lock.unlock ();

  lock_guard<mutex> depth_lock(depth_mutex_);
  status = depth_generator_.SetIntProperty ("RegistrationType", 1);
  if (status != XN_STATUS_OK)
    THROW_OPENNI_EXCEPTION ("Error setting the registration type. Reason: %s", xnGetStatusString (status));
}

DevicePrimesense::~DevicePrimesense () throw ()
{
  setDepthRegistration ( false );
  setSynchronization ( false );

  depth_mutex_.lock ();
  depth_generator_.UnregisterFromNewDataAvailable (depth_callback_handle_);
  depth_mutex_.unlock ();

  image_mutex_.lock ();
  image_generator_.UnregisterFromNewDataAvailable (image_callback_handle_);
  image_mutex_.unlock ();
}

bool DevicePrimesense::isImageResizeSupported (unsigned input_width, unsigned input_height, unsigned output_width, unsigned output_height) const throw ()
{
  return ImageYUV422::resizingSupported (input_width, input_height, output_width, output_height);
}

//void DevicePrimesense::setImageOutputMode (const XnMapOutputMode& output_mode) 
//{
//  if (output_mode.nFPS == 30 && output_mode.nXRes == XN_UXGA_X_RES && output_mode.nYRes == XN_UXGA_Y_RES )
//  {
//    cout << "setting image mode to UXGA" << endl;
//    if (isImageStreamRunning ())
//    {
//      stopImageStream ();
//      XnStatus status = image_generator_.SetIntProperty ("InputFormat", 0);
//      if (status != XN_STATUS_OK)
//        THROW_OPENNI_EXCEPTION ("Error setting the image input format to compressed BAYER. Reason: %s", xnGetStatusString (status));
//      status = image_generator_.SetPixelFormat (XN_PIXEL_FORMAT_RGB24);
//      if (status != XN_STATUS_OK)
//        THROW_OPENNI_EXCEPTION ("Failed to set image pixel format to YUV422. Reason: %s", xnGetStatusString (status));
//      sleep (1);
//      startImageStream ();
//    }
//    else
//    {
//      XnStatus status = image_generator_.SetIntProperty ("InputFormat", 0);
//      if (status != XN_STATUS_OK)
//        THROW_OPENNI_EXCEPTION ("Error setting the image input format to compressed BAYER. Reason: %s", xnGetStatusString (status));
//
//      status = image_generator_.SetPixelFormat (XN_PIXEL_FORMAT_GRAYSCALE_8_BIT);
//      if (status != XN_STATUS_OK)
//        THROW_OPENNI_EXCEPTION ("Failed to set image pixel format to YUV422. Reason: %s", xnGetStatusString (status));
//    }
//  }
//  else
//  {
//    XnStatus status = image_generator_.SetIntProperty ("InputFormat", 5);
//    if (status != XN_STATUS_OK)
//        THROW_OPENNI_EXCEPTION ("Error setting the image input format to uncompressed BAYER. Reason: %s", xnGetStatusString (status));
//
//    status = image_generator_.SetPixelFormat (XN_PIXEL_FORMAT_YUV422);
//    if (status != XN_STATUS_OK)
//      THROW_OPENNI_EXCEPTION ("Failed to set image pixel format to YUV422. Reason: %s", xnGetStatusString (status));
//  }
//  OpenNIDevice::setImageOutputMode (output_mode);
//}

void DevicePrimesense::enumAvailableModes () 
{
  XnMapOutputMode output_mode;
  available_image_modes_.clear();
  available_depth_modes_.clear();

  // Depth Modes
  output_mode.nFPS = 30;
  output_mode.nXRes = XN_VGA_X_RES;
  output_mode.nYRes = XN_VGA_Y_RES;
  available_depth_modes_.push_back (output_mode);

  output_mode.nFPS = 25;
  output_mode.nXRes = XN_VGA_X_RES;
  output_mode.nYRes = XN_VGA_Y_RES;
  available_depth_modes_.push_back (output_mode);

  output_mode.nFPS = 25;
  output_mode.nXRes = XN_QVGA_X_RES;
  output_mode.nYRes = XN_QVGA_Y_RES;
  available_depth_modes_.push_back (output_mode);

  output_mode.nFPS = 30;
  output_mode.nXRes = XN_QVGA_X_RES;
  output_mode.nYRes = XN_QVGA_Y_RES;
  available_depth_modes_.push_back (output_mode);

  output_mode.nFPS = 60;
  output_mode.nXRes = XN_QVGA_X_RES;
  output_mode.nYRes = XN_QVGA_Y_RES;
  available_depth_modes_.push_back (output_mode);

  // RGB Modes
  output_mode.nFPS = 30;
  output_mode.nXRes = XN_VGA_X_RES;
  output_mode.nYRes = XN_VGA_Y_RES;
  available_image_modes_.push_back (output_mode);

  output_mode.nFPS = 25;
  output_mode.nXRes = XN_VGA_X_RES;
  output_mode.nYRes = XN_VGA_Y_RES;
  available_image_modes_.push_back (output_mode);

//  output_mode.nFPS = 30;
//  output_mode.nXRes = XN_UXGA_X_RES;
//  output_mode.nYRes = XN_UXGA_Y_RES;
//  available_image_modes_.push_back (output_mode);

  output_mode.nFPS = 25;
  output_mode.nXRes = XN_QVGA_X_RES;
  output_mode.nYRes = XN_QVGA_Y_RES;
  available_image_modes_.push_back (output_mode);

  output_mode.nFPS = 30;
  output_mode.nXRes = XN_QVGA_X_RES;
  output_mode.nYRes = XN_QVGA_Y_RES;
  available_image_modes_.push_back (output_mode);

  output_mode.nFPS = 60;
  output_mode.nXRes = XN_QVGA_X_RES;
  output_mode.nYRes = XN_QVGA_Y_RES;
  available_image_modes_.push_back (output_mode);
}

boost::shared_ptr<Image> DevicePrimesense::getCurrentImage (boost::shared_ptr<xn::ImageMetaData> image_data) const throw ()
{
  return boost::shared_ptr<Image> ( new ImageYUV422 (image_data) );
}

void DevicePrimesense::startImageStream () 
{
  // Suat: Ugly workaround... but on some usb-ports its not possible to start the image stream after the depth stream.
  // turning on and off registration solves for some reason the problem!

  if (isDepthStreamRunning ())
  {
    if (isDepthRegistered ())
    {
     // Reset the view point
      setDepthRegistration (false);

      // Reset the view point
      setDepthRegistration (true);

     // Reset the view point
      setDepthRegistration (false);

      // Start the stream
      OpenNIDevice::startImageStream ();

      // Register the stream
      setDepthRegistration (true);
    }
    else
    {
      // Reset the view point
      setDepthRegistration (true);
      // Reset the view point
      setDepthRegistration (false);

      // Start the stream
      OpenNIDevice::startImageStream ();
    }
  }
  else
    // Start the stream
    OpenNIDevice::startImageStream ();
}

void DevicePrimesense::startDepthStream () 
{
  if (isDepthRegistered ())
  {
    // Reset the view point
    setDepthRegistration (false);

    // Start the stream
    OpenNIDevice::startDepthStream ();

    // Register the stream
    setDepthRegistration (true);
  }
  else
    // Start the stream
    OpenNIDevice::startDepthStream ();
}

} //namespace
