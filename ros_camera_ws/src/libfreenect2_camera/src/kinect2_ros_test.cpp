/************************************************************************************//**
 *  @file       libfreenect2_ros_test.cpp
 *
 *  @date       2016-08-02 14:38
 *
 ***************************************************************************************/

#include <ros/ros.h>
#include <iostream>
#include <libfreenect2/libfreenect2.hpp>
#include <libfreenect2/frame_listener_impl.h>
#include <libfreenect2/registration.h>
#include <libfreenect2/packet_pipeline.h>
#include <libfreenect2/logger.h>

#include <opencv2/opencv.hpp>

#include <sensor_msgs/Image.h>
#include <sensor_msgs/CameraInfo.h>
#include <sensor_msgs/image_encodings.h>
#include <std_msgs/Header.h>
#include <std_msgs/String.h>
#include <cv_bridge/cv_bridge.h>

#include <tf/transform_broadcaster.h>

using namespace cv;
Mat MatColor, MatDepth;

ros::Publisher color_pub, depth_pub, cam_info_color_pub, cam_info_depth_pub;
std_msgs::Header h_color, h_depth;
sensor_msgs::Image msg_ColorImage, msg_DepthImage;
sensor_msgs::CameraInfo cam_info_color, cam_info_depth;

cv_bridge::CvImage out_msgc, out_msgd;

libfreenect2::Freenect2 freenect2;
libfreenect2::Freenect2Device *dev = 0;
std::string serial = ""; 
libfreenect2::SyncMultiFrameListener *listenerPtr;
libfreenect2::FrameMap frames;

tf::TransformBroadcaster *tfbr;
const bool bilateral_filter = true, edge_aware_filter =true;
const double minDepth = 0.3, maxDepth = 7;
int OutputWidth , OutputHeight;
namespace{
  void getPara(){
    ros::param::get("/height", OutputHeight);
    ros::param::get("/width", OutputWidth);
  }

}
void InitConfig(){
  libfreenect2::Freenect2Device::Config config;
  config.EnableBilateralFilter = bilateral_filter;
  config.EnableEdgeAwareFilter = edge_aware_filter;
  config.MinDepth = minDepth;
  config.MaxDepth = maxDepth;
  dev->setConfiguration(config);
}
void prepareTransforms(){
  tf::Vector3 color_t_vec(0, 0, 0);
  tf::Matrix3x3 color_r_mat(
    1, 0, 0,
    0, 1, 0,
    0, 0, 1
    );
  tf::Transform transform(color_r_mat, color_t_vec);
  tfbr->sendTransform(tf::StampedTransform(transform, h_color.stamp, "kinect2", "kinect2_color"));
 
  tf::Vector3 depth_t_vec(0, 0, 0);
  tf::Matrix3x3 depth_r_mat(
    1, 0, 0,
    0, 1, 0,
    0, 0, 1
    );
  tf::Transform dtransform(depth_r_mat, depth_t_vec);
  tfbr->sendTransform(tf::StampedTransform(dtransform, h_depth.stamp, "kinect2", "kinect2_depth"));
}
void setCamInfo(){
  //color
  cam_info_color.header = msg_ColorImage.header;
  cam_info_color.header.frame_id = "kinect2";
  cam_info_color.height = OutputHeight;
  cam_info_color.width = OutputWidth;

  cam_info_color.D.push_back(0);
  cam_info_color.D.push_back(0);
  cam_info_color.D.push_back(0);
  cam_info_color.D.push_back(0);
  cam_info_color.D.push_back(0);
  // cam_info_color.D = std::vector<double>(5, 0.0);
  auto colorIntrinsics = dev->getColorCameraParams();

  cam_info_color.K[0] = colorIntrinsics.fx;
  cam_info_color.K[1] = 0;
  cam_info_color.K[2] = colorIntrinsics.cx;
  cam_info_color.K[3] = 0;
  cam_info_color.K[4] = colorIntrinsics.fy;
  cam_info_color.K[5] = colorIntrinsics.cy;
  cam_info_color.K[6] = 0;
  cam_info_color.K[7] = 0;
  cam_info_color.K[8] = 1;

  cam_info_color.R[0] = 1.0;
  cam_info_color.R[1] = 0.0;
  cam_info_color.R[2] = 0.0;
  cam_info_color.R[3] = 0.0;
  cam_info_color.R[4] = 1.0;
  cam_info_color.R[5] = 0.0;
  cam_info_color.R[6] = 0.0;
  cam_info_color.R[7] = 0.0;
  cam_info_color.R[8] = 1.0;

  cam_info_color.P[0] = colorIntrinsics.fx;
  cam_info_color.P[1] = 0;
  cam_info_color.P[2] = colorIntrinsics.cx;
  cam_info_color.P[3] = 0;
  cam_info_color.P[4] = 0;
  cam_info_color.P[5] = colorIntrinsics.fy;
  cam_info_color.P[6] = colorIntrinsics.cy;
  cam_info_color.P[7] = 0;
  cam_info_color.P[8] = 0;
  cam_info_color.P[9] = 0;
  cam_info_color.P[10] = 1;
  cam_info_color.P[11] = 0;
  ////
  //depth
  cam_info_depth.header = msg_DepthImage.header;
  cam_info_depth.height = OutputHeight;
  cam_info_depth.width = OutputWidth;
  cam_info_depth.header.frame_id = "kinect2";
  
  auto depthIntrinsics = dev->getIrCameraParams();
  cam_info_depth.D.push_back(depthIntrinsics.k1);
  cam_info_depth.D.push_back(depthIntrinsics.k2);
  cam_info_depth.D.push_back(depthIntrinsics.p1);
  cam_info_depth.D.push_back(depthIntrinsics.p2);
  cam_info_depth.D.push_back(depthIntrinsics.k3);
  // cam_info_depth.D = std::vector<double>(5, 0.0);
  std::cout << " k1: " <<  depthIntrinsics.k1 << std::endl;
  std::cout << " k2: " <<  depthIntrinsics.k2 << std::endl;
  std::cout << " p1: " <<  depthIntrinsics.p1 << std::endl;
  std::cout << " p2: " <<  depthIntrinsics.p2 << std::endl;
  std::cout << " k3: " <<  depthIntrinsics.k3 << std::endl;
  cam_info_depth.K[0] = depthIntrinsics.fx;
  cam_info_depth.K[1] = 0;
  cam_info_depth.K[2] = depthIntrinsics.cx;
  cam_info_depth.K[3] = 0;
  cam_info_depth.K[4] = depthIntrinsics.fy;
  cam_info_depth.K[5] = depthIntrinsics.cy;
  cam_info_depth.K[6] = 0;
  cam_info_depth.K[7] = 0;
  cam_info_depth.K[8] = 1;

  cam_info_depth.R[0] = 1.0;
  cam_info_depth.R[1] = 0.0;
  cam_info_depth.R[2] = 0.0;
  cam_info_depth.R[3] = 0.0;
  cam_info_depth.R[4] = 1.0;
  cam_info_depth.R[5] = 0.0;
  cam_info_depth.R[6] = 0.0;
  cam_info_depth.R[7] = 0.0;
  cam_info_depth.R[8] = 1.0;

  cam_info_depth.P[0] = depthIntrinsics.fx;
  cam_info_depth.P[1] = 0;
  cam_info_depth.P[2] = depthIntrinsics.cx;
  cam_info_depth.P[3] = 0;
  cam_info_depth.P[4] = 0;
  cam_info_depth.P[5] = depthIntrinsics.fy;
  cam_info_depth.P[6] = depthIntrinsics.cy;
  cam_info_depth.P[7] = 0;
  cam_info_depth.P[8] = 0;
  cam_info_depth.P[9] = 0;
  cam_info_depth.P[10] = 1;
  cam_info_depth.P[11] = 0;
  /* std::cout <<"  fx:" << depthIntrinsics.fx << std::endl; */
  /* std::cout <<"  fy:" << depthIntrinsics.fy << std::endl; */
  /* std::cout <<"  cx:" << depthIntrinsics.cx << std::endl; */
  /* std::cout <<"  cy:" << depthIntrinsics.cy << std::endl; */
}
void TopicPublisher(){
  color_pub.publish(msg_ColorImage);
  depth_pub.publish(msg_DepthImage);
  cam_info_color_pub.publish(cam_info_color);
  cam_info_depth_pub.publish(cam_info_depth);
}
void MattoMsg(){
  h_color.frame_id = "kinect2";
  h_depth.frame_id = "kinect2";
  msg_ColorImage.header = h_color;
  h_color.stamp = ros::Time::now(); 
  h_depth.stamp = ros::Time::now(); 
  out_msgc.header   = h_color; // Same timestamp and tf frame as input image
  out_msgc.encoding = sensor_msgs::image_encodings::BGR8; // Or whatever
  out_msgc.image    = MatColor; // Your cv::Mat
  msg_ColorImage = *(out_msgc.toImageMsg());
  
  msg_DepthImage.header = h_depth;
  out_msgd.header   = h_depth; // Same timestamp and tf frame as input image
  out_msgd.encoding = sensor_msgs::image_encodings::TYPE_32FC1; // Or whatever
  out_msgd.image    = MatDepth; // Your cv::Mat
  msg_DepthImage = *(out_msgd.toImageMsg());
}
void MatResize(Mat& src, int dst_width, int dst_height){
  float SrcWHratio = src.cols/(float)src.rows;
  float DstWHratio = dst_width/(float)dst_height;
  if(SrcWHratio > DstWHratio){//narrow
    cv::resize(src, src, Size(dst_height*SrcWHratio, dst_height));
    int FirstPixel = (int)((SrcWHratio - DstWHratio)*dst_height/2);
    cv::Rect myROI(FirstPixel, 0, dst_width, dst_height);
    src = src(myROI);
  }
  else{
    cv::resize(src, src, Size(dst_width, dst_width/SrcWHratio));
    int FirstPixel = (int)((1/SrcWHratio - 1/DstWHratio)*dst_width/2);
    cv::Rect myROI(0, FirstPixel, dst_width, dst_height);
    src = src(myROI);
  }
}
/* void reflection(Mat & src){ */
/*   Mat temp = src.clone(); */
/*   for(int i = 0 ;i < src.rows ; i++ ){ */
/*     for(int j = 0 ; j < src.cols ; j ++) */
/*   } */
  
/* } */
void getCvMat(libfreenect2::Frame *rgb, libfreenect2::Frame *depth){
  Mat tempColor, tempDepth;
  float NaN = 0.0/0.0;
  MatDepth = cv::Mat(OutputHeight, OutputWidth, CV_32FC1, cv::Scalar(NaN)).clone();
  MatColor = cv::Mat(OutputHeight, OutputWidth, CV_8UC4, cv::Scalar(0,0,0)).clone();
  /* MatDepth = cv::Mat(depth->height, depth->width, CV_32FC1, depth->data).clone(); */
  /* MatColor = cv::Mat(rgb->height, rgb->width, CV_8UC4, rgb->data).clone(); */
  tempDepth = cv::Mat(depth->height, depth->width, CV_32FC1, depth->data).clone();
  tempColor = cv::Mat(rgb->height, rgb->width, CV_8UC4, rgb->data).clone();
  MatResize(tempColor, 512, 424);
  float top = (OutputHeight - 424)/2;
  float left = (OutputWidth - 512)/2;
  Mat roiColor = MatColor(Rect((int)left, (int)top, 512, 424));
  Mat roiDepth = MatDepth(Rect((int)left, (int)top, 512, 424));
    
  tempColor.copyTo(roiColor);
  tempDepth.copyTo(roiDepth);

  cvtColor (MatColor, MatColor , CV_BGRA2BGR);
  /* MatResize(MatColor, OutputWidth, OutputHeight); */
  /* MatResize(MatDepth, OutputWidth, OutputHeight); */
  /* cv::resize(MatColor, MatColor, Size(670, 424)); */
  /* cv::Rect myROI(84, 0, 512, 424); */
  /* MatColor = MatColor(myROI); */
  MatDepth /= 1000.0f;//mmeter to meter
  
}
void run(){
  libfreenect2::Registration* registration = new libfreenect2::Registration(dev->getIrCameraParams(), dev->getColorCameraParams());
  libfreenect2::Frame *undistorted =new libfreenect2::Frame(512, 424, 4);
  libfreenect2::Frame *registered =new libfreenect2::Frame(512, 424, 4);
 
  /* namedWindow("Color", CV_WINDOW_AUTOSIZE); */
  /* namedWindow("Depth", CV_WINDOW_AUTOSIZE); */

    setCamInfo();
  while(ros::ok()){
    if(waitKey(1) == 27) break;
    if (!listenerPtr->waitForNewFrame(frames, 10*1000)){ // 10 sconds
      std::cout << "timeout!" << std::endl;
      return ;
    }
    libfreenect2::Frame *rgb = frames[libfreenect2::Frame::Color];
    libfreenect2::Frame *depth = frames[libfreenect2::Frame::Depth];
    
    registration->apply(rgb, depth, undistorted, registered);
    getCvMat(registered , undistorted);
    /* imshow("Depth", MatDepth); */
    /* imshow("Color", MatColor); */
    MattoMsg();
    prepareTransforms();//need to run in anothor thread;
    TopicPublisher();
    listenerPtr->release(frames);
  }
}
int main(int argc, char **argv){
  ros::init(argc, argv, "capture_image");
 
  ros::NodeHandle n;
  color_pub = n.advertise<sensor_msgs::Image>("/camera/rgb/image_rect_color", 5);  
  depth_pub = n.advertise<sensor_msgs::Image>("/camera/depth/image_rect", 5);
  cam_info_color_pub = n.advertise<sensor_msgs::CameraInfo>("/camera/rgb/camera_info", 5);  
  cam_info_depth_pub = n.advertise<sensor_msgs::CameraInfo>("/camera/depth/camera_info", 5);
 
  tfbr = new tf::TransformBroadcaster();
  ::getPara();
  if(freenect2.enumerateDevices() == 0){
    std::cout << "no device connected!" << std::endl;
    return -1;
  }
  if (serial == ""){
    serial = freenect2.getDefaultDeviceSerialNumber();
  }
  
  dev = freenect2.openDevice(serial);
  
  bool enable_rgb = true;
  bool enable_depth = true;
  int types = 0;

  if (enable_rgb)
    types |= libfreenect2::Frame::Color;
  if (enable_depth)
    types |= libfreenect2::Frame::Ir | libfreenect2::Frame::Depth;
  libfreenect2::SyncMultiFrameListener listener(types);

  listenerPtr = &listener;
  dev->setColorFrameListener(&listener);
  dev->setIrAndDepthFrameListener(&listener);
  InitConfig();
  if (enable_rgb && enable_depth){
    if (!dev->start())
      return -1;
  }
  else{
    if (!dev->startStreams(enable_rgb, enable_depth))
      return -1;
  }
  std::cout << "device serial: " << dev->getSerialNumber() << std::endl;
  std::cout << "device firmware: " << dev->getFirmwareVersion() << std::endl;

  libfreenect2::Registration* registration = new libfreenect2::Registration(dev->getIrCameraParams(), dev->getColorCameraParams());
  libfreenect2::Frame *undistorted =new libfreenect2::Frame(512, 424, 4);
  libfreenect2::Frame *registered =new libfreenect2::Frame(512, 424, 4);
  
  run();//main loop

  dev->stop();
  dev->close();

  return 0;
}
