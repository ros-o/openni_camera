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
#include <openni_camera/openni_device_kinect.h>
#include <openni_camera/openni_image_bayer_grbg.h>
#include <iostream>
#include <sstream>
#include <boost/thread/mutex.hpp>

using namespace boost;

namespace openni_wrapper
{

DeviceKinect::DeviceKinect (xn::Context& context, const xn::NodeInfo& device_node, const xn::NodeInfo& image_node, const xn::NodeInfo& depth_node, const xn::NodeInfo& ir_node) 
: OpenNIDevice (context, device_node, image_node, depth_node, ir_node)
, debayering_method_ (ImageBayerGRBG::EdgeAwareWeighted)
{
  // setup stream modes
  enumAvailableModes ();
  setDepthOutputMode (getDefaultDepthMode ());
  setImageOutputMode (getDefaultImageMode ());
  setIROutputMode (getDefaultIRMode ());
  
  // device specific initialization
  XnStatus status;

  unique_lock<mutex> image_lock(image_mutex_);
  // set kinect specific format. Thus input = uncompressed Bayer, output = grayscale = bypass = bayer
  status = image_generator_.SetIntProperty ("InputFormat", 6);
  if (status != XN_STATUS_OK)
    THROW_OPENNI_EXCEPTION ("Error setting the image input format to Uncompressed 8-bit BAYER. Reason: %s", xnGetStatusString (status));

  // Grayscale: bypass debayering -> gives us bayer pattern!
  status = image_generator_.SetPixelFormat (XN_PIXEL_FORMAT_GRAYSCALE_8_BIT);
  if (status != XN_STATUS_OK)
    THROW_OPENNI_EXCEPTION ("Failed to set image pixel format to 8bit-grayscale. Reason: %s", xnGetStatusString (status));
  image_lock.unlock ();
  
  lock_guard<mutex> depth_lock(depth_mutex_);
  // RegistrationType should be 2 (software) for Kinect, 1 (hardware) for PS
  status = depth_generator_.SetIntProperty ("RegistrationType", 2);
  if (status != XN_STATUS_OK)
    THROW_OPENNI_EXCEPTION ("Error setting the registration type. Reason: %s", xnGetStatusString (status));
}

DeviceKinect::~DeviceKinect () throw ()
{
  depth_mutex_.lock ();
  depth_generator_.UnregisterFromNewDataAvailable (depth_callback_handle_);
  depth_mutex_.unlock ();

  image_mutex_.lock ();
  image_generator_.UnregisterFromNewDataAvailable (image_callback_handle_);
  image_mutex_.unlock ();
}

bool DeviceKinect::isImageResizeSupported (unsigned input_width, unsigned input_height, unsigned output_width, unsigned output_height) const throw ()
{
  return ImageBayerGRBG::resizingSupported (input_width, input_height, output_width, output_height);
}

void DeviceKinect::enumAvailableModes () 
{
  XnMapOutputMode output_mode;
  available_image_modes_.clear();
  available_depth_modes_.clear();

  output_mode.nFPS = 30;
  output_mode.nXRes = XN_VGA_X_RES;
  output_mode.nYRes = XN_VGA_Y_RES;
  available_image_modes_.push_back (output_mode);
  available_depth_modes_.push_back (output_mode);

  output_mode.nFPS = 15;
  output_mode.nXRes = XN_SXGA_X_RES;
  output_mode.nYRes = XN_SXGA_Y_RES;
  available_image_modes_.push_back (output_mode);
}

boost::shared_ptr<Image> DeviceKinect::getCurrentImage (boost::shared_ptr<xn::ImageMetaData> image_data) const throw ()
{
  return boost::shared_ptr<Image> (new ImageBayerGRBG (image_data, debayering_method_));
}

void DeviceKinect::setSynchronization (bool on_off) 
{
  if (on_off)
    THROW_OPENNI_EXCEPTION ("Microsoft Kinect does not support Hardware synchronization.");
}

bool DeviceKinect::isSynchronized () const 
{
  return false;
}

bool DeviceKinect::isSynchronizationSupported () const throw ()
{
  return false;
}

bool DeviceKinect::isDepthCropped () const 
{
  return false;
}

void DeviceKinect::setDepthCropping (unsigned x, unsigned y, unsigned width, unsigned height) 
{
  if (width != 0 && height != 0)
    THROW_OPENNI_EXCEPTION ("Microsoft Kinect does not support cropping for the depth stream.");
}

bool DeviceKinect::isDepthCroppingSupported () const throw ()
{
  return false;
}

} //namespace
