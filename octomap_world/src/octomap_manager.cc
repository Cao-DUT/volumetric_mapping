#include "octomap_world/octomap_manager.h"

#include <glog/logging.h>
#include <minkindr_conversions/kindr_tf.h>

namespace volumetric_mapping {

OctomapManager::OctomapManager(const ros::NodeHandle& nh,
                               const ros::NodeHandle& nh_private)
  : nh_(nh), nh_private_(nh_private), Q_(Eigen::Matrix4d::Identity()) {
  subscribe();
  advertiseServices();
  advertisePublishers();
}

void OctomapManager::subscribe() {
  left_info_sub_ =
    nh_.subscribe("cam0/camera_info", 1,
                  &OctomapManager::leftCameraInfoCallback, this);
  right_info_sub_ =
    nh_.subscribe("cam1/camera_info", 1,
                    &OctomapManager::rightCameraInfoCallback, this);
  disparity_sub_ = nh_.subscribe(
      "disparity", 40, &OctomapManager::insertDisparityImageWithTf, this);
  disparity_sub_ = nh_.subscribe(
      "pointcloud", 40, &OctomapManager::insertPointcloudWithTf, this);
}

void OctomapManager::advertiseServices() {}
void OctomapManager::advertisePublishers() {}

void OctomapManager::publishAll() {}
void OctomapManager::publishOccupied() {}
void OctomapManager::publishFree() {}
void OctomapManager::publishUnknown() {}

void OctomapManager::resetMapCallback() {}
void OctomapManager::publishAllCallback() {}
void OctomapManager::saveTreeCallback() {}
void OctomapManager::loadTreeCallback() {}

void OctomapManager::leftCameraInfoCallback(
    const sensor_msgs::CameraInfoPtr& left_info) {
  left_info_ = left_info;
  if (left_info_ && right_info_) {
    calculateQ();
  }
}
void OctomapManager::rightCameraInfoCallback(
    const sensor_msgs::CameraInfoPtr& right_info) {
  right_info_ = right_info;
  if (left_info_ && right_info_) {
    calculateQ();
  }
}

void OctomapManager::calculateQ() {
  Q_ = getQForROSCameras(*left_info_, *right_info_);
  full_image_size_.x() = left_info_->width;
  full_image_size_.y() = left_info_->height;
}

void OctomapManager::insertDisparityImageWithTf(
    const stereo_msgs::DisparityImageConstPtr& disparity) {
  if (!(left_info_ && right_info_)) {
    ROS_WARN("No camera info available yet, skipping adding disparity.");
    return;
  }

  // Look up transform from sensor frame to world frame.
  Transformation sensor_to_world;
  if (lookupTransform(disparity->header.frame_id, world_frame_,
                      disparity->header.stamp, &sensor_to_world)) {
    insertDisparityImage(sensor_to_world, disparity, Q_, full_image_size_);
  }
}

void OctomapManager::insertPointcloudWithTf(
    const sensor_msgs::PointCloud2::ConstPtr& pointcloud) {
  // Look up transform from sensor frame to world frame.
  Transformation sensor_to_world;
  if (lookupTransform(pointcloud->header.frame_id, world_frame_,
                      pointcloud->header.stamp, &sensor_to_world)) {
    insertPointcloud(sensor_to_world, pointcloud);
  }
}

bool OctomapManager::lookupTransform(const std::string& from_frame,
                                     const std::string& to_frame,
                                     const ros::Time& timestamp,
                                     Transformation* transform) {
  tf::StampedTransform tf_transform;
  try {
    tf_listener_.lookupTransform(to_frame, from_frame, timestamp,
                                 tf_transform);
  }
  catch (tf::TransformException& ex) {
    ROS_ERROR_STREAM("Error getting TF transform from sensor data: "
                     << ex.what() << ".");
    return false;
  }
  return true;
}

}  // namespace volumetric_mapping
