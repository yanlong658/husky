#include <ros/ros.h>
#include "ncrl_ugv_ctrl/husky_path.h"
#include "ncrl_ugv_ctrl/qptrajectory.h"
#include "ncrl_ugv_ctrl/ncrl_tf.h"
#include <Eigen/Geometry>
#include <eigen3/Eigen/Dense>
#include <thread>

//declare global variable
ros::Publisher pub_cmd , path_pub ,lio_pub_path;
std::string  POS_topic;
double freq;

//control
struct PID pid_x;
struct PID pid_y;
double minCmdX, maxCmdX, minCmdW, maxCmdW;

//qp part
//goal_path show path which qp has solved
nav_msgs::Path goal_path;
geometry_msgs::Point vel;
geometry_msgs::Point acc;

//goal_pose world為座標
//lio_pose world為座標
ncrl_tf::Trans goal_pose, world2body, lio_pose;
tf::Transform world;


//global variable
int i = 0;
int count_ = 0;
double init_time;


//readparameter from launch parameter
bool readParameter(ros::NodeHandle &nh)
{
    bool result = true;
    // get topic name
    if (  !nh.getParam("POS_topic", POS_topic)){
        ROS_ERROR("Failed to get param 'Path_topic'");
        ROS_ERROR("Failed to get param 'POS_topic'");
        result = false;
    }

    // get pid variable
    if (!nh.getParam("X_KP", pid_x.KP) || !nh.getParam("X_KI", pid_x.KI) || !nh.getParam("X_KD", pid_x.KD)){
        ROS_ERROR("Failed to get param x axis PID");
        result = false;
    }
    if (!nh.getParam("Y_KP", pid_y.KP) || !nh.getParam("Y_KI", pid_y.KI) || !nh.getParam("Y_KD", pid_y.KD)){
        ROS_ERROR("Failed to get param y axis PID");
        result = false;
    }

    // get frequency
    if (!nh.getParam("FREQ", freq)){
        ROS_ERROR("Failed to get param 'FREQ'");
        result = false;
    }

    if (!nh.getParam("max_vel_x", maxCmdX) || !nh.getParam("min_vel_x", minCmdX) ||
        !nh.getParam("max_vel_w", maxCmdW) || !nh.getParam("min_vel_w", minCmdW)){
        ROS_ERROR("Failed to get confinement of cmd");
        result = false;
    }

    return result;
}

void cb_pos(const nav_msgs::Odometry::ConstPtr& msg){
    Eigen::Vector3d v_(msg->pose.pose.position.x, msg->pose.pose.position.y, msg->pose.pose.position.z);
    Eigen::Quaterniond q_(msg->pose.pose.orientation.w,
                          msg->pose.pose.orientation.x,
                          msg->pose.pose.orientation.y,
                          msg->pose.pose.orientation.z);
    //lio_pose 有了現在收到的q 和 v translation
    ncrl_tf::setTrans(lio_pose, q_, v_);
}


void process()
{
  //ncrl::tf
  static tf::TransformBroadcaster br;
  ros::Rate r(freq);

  //要傳給husky的vel_cmd指令
  geometry_msgs::Twist vel_cmd;
  int flag = 1;
  while(ros::ok())
  {
    int c = getch();
    if (c != EOF)
    {
      switch(c)
      {
        case 49:     // key 1
          flag = 1;
        break;
        case 50:     // key 2
          flag = 2;
        break;
        case 51:     // key 3
          flag = 3;
        break;
      }
    }
    if(flag ==1)
    {
      ROS_INFO(" ===== KEYBOARD CONTROL ===== ");
    }
    else if (flag == 2 )
    {
      ROS_INFO(" ===== enter ===== ");
      double max;
      double sample=0.03;
      qptrajectory plan;
      path_def path;
      trajectory_profile p1,p2,p3,p4,p5,p6;

      std::vector<trajectory_profile> data;
      p1.pos << 0.0,0,0;
      p1.vel << 0.0,0.0,0;
      p1.acc << 0.00,-0.0,0;
      p1.yaw = 0;

      p2.pos<< 3.0,0.0,0;
      p2.vel<< 0,0,0;
      p2.acc<< 0,0,0;
      p2.yaw = 0;

      p3.pos<< 4.0,-1.0,0.0;
      p3.vel<< 0,0,0;
      p3.acc<< 0,0,0;
      p3.yaw = 0;

      p4.pos << 4.0,-2.5,0;
      p4.vel << 0,0,0;
      p4.acc << 0,0,0;
      p4.yaw = 0;

      p5.pos << 3.0,-3.5,0;
      p5.vel << 0.0,0.0,0;
      p5.acc << 0.00,-0.0,0;
      p5.yaw = 0;

      p6.pos << 0.0,-3.5,0;
      p6.vel << 0.0,0.0,0;
      p6.acc << 0.00,-0.0,0;
      p6.yaw = 0;

      path.push_back(segments(p1,p2,4));
      path.push_back(segments(p2,p3,2));
      path.push_back(segments(p3,p4,2));
      path.push_back(segments(p4,p5,2));
      path.push_back(segments(p5,p6,4));

      data = plan.get_profile(path ,path.size(),sample);
      max = data.size();

      while(ros::ok())
      {
          if(count_ >=max)
          {

            vel_cmd.linear.x = 0;
            vel_cmd.angular.z = 0;

            //讓車子停在定點
            pub_cmd.publish(vel_cmd);
            flag = 1;
            count_ = 0;
            break;
          }

          else
          {
            goal_path.header.stamp= ros::Time::now();
            goal_path.header.frame_id="WORLD";

            geometry_msgs::PoseStamped this_pose_stamped;

            this_pose_stamped.header.stamp=ros::Time::now();
            this_pose_stamped.header.frame_id="WORLD";

            this_pose_stamped.pose.position.x = data[count_].pos[0];
            this_pose_stamped.pose.position.y = data[count_].pos[1];

            goal_path.poses.push_back(this_pose_stamped);

            goal_pose.v << data[count_].pos[0] ,data[count_].pos[1] , data[count_].pos[2];
            vel.x = data[count_].vel[0];

            count_ += 1 ;

            //</lio_path>
            path_pub.publish(goal_path);
        }

          Eigen::Vector3d error,error_last;

          error= goal_pose.v - lio_pose.v;

          float cmd_x, cmd_y;
          pid_compute(pid_x, cmd_x, error(0), error_last(0), 0.001);
          pid_compute(pid_y, cmd_y, error(1), error_last(1), 0.001);

          error_last = error;

          Eigen::Vector3d cmd(cmd_x, cmd_y, 0);

          //trans imu_init frame cmd into body frame
          cmd = lio_pose.q.inverse() * cmd;

          //feedforward
          vel_cmd.linear.x = cmd(0);
          vel_cmd.angular.z = cmd(1);
          //vel_cmd.linear.x += data[count_].vel[0];

          if (fabs(vel_cmd.linear.x) > maxCmdX)
          {
              vel_cmd.linear.x = vel_cmd.linear.x * maxCmdX / fabs(vel_cmd.linear.x);
          }
          else if (fabs(vel_cmd.linear.x) < minCmdX)
          {
              vel_cmd.linear.x = 0;
          }

          if (fabs(vel_cmd.angular.z) > maxCmdW)
          {
              vel_cmd.angular.z = vel_cmd.angular.z * maxCmdW / fabs(vel_cmd.angular.z);
          }
          else if (fabs(vel_cmd.angular.z) < minCmdW)
          {
          vel_cmd.angular.z = 0;
          }

          pub_cmd.publish(vel_cmd);
          r.sleep();
        }
      }

    else if (flag == 3 )
    {
      ROS_INFO(" ===== enter ===== ");
      double max_ = 0;
      double sample_=0.03;
      qptrajectory plan_;
      path_def path_;
      trajectory_profile p1_,p2_;

      std::vector<trajectory_profile> data_;
      p1_.pos << 0.0,-3.5,0;
      p1_.vel << 0.0,0.0,0;
      p1_.acc << 0.00,-0.0,0;
      p1_.yaw = 0;

      p2_.pos<< 0.0,0.0,0;
      p2_.vel<< 0,0,0;
      p2_.acc<< 0,0,0;
      p2_.yaw = 0;;

      path_.push_back(segments(p1_,p2_,8));


      data_ = plan_.get_profile(path_ ,path_.size(),sample_);
      max_ = data_.size();

      while(ros::ok())
      {
          if(count_ >= max_)
          {

            vel_cmd.linear.x = 0;
            vel_cmd.angular.z = 0;

            //讓車子停在定點
            pub_cmd.publish(vel_cmd);
            count_ = 0;
            flag = 1;
            break;
          }
         else
          {
            goal_path.header.stamp= ros::Time::now();
            goal_path.header.frame_id="WORLD";

            geometry_msgs::PoseStamped this_pose_stamped;

            this_pose_stamped.header.stamp=ros::Time::now();
            this_pose_stamped.header.frame_id="WORLD";

            this_pose_stamped.pose.position.x = data_[count_].pos[0];
            this_pose_stamped.pose.position.y = data_[count_].pos[1];

            goal_path.poses.push_back(this_pose_stamped);

            goal_pose.v << data_[count_].pos[0] ,data_[count_].pos[1] , data_[count_].pos[2];
            vel.x = data_[count_].vel[0];

            count_ += 1 ;

            //</lio_path>
            path_pub.publish(goal_path);
          }

          Eigen::Vector3d error,error_last;


          error= goal_pose.v - lio_pose.v;

          float cmd_x, cmd_y;
          pid_compute(pid_x, cmd_x, error(0), error_last(0), 0.001);
          pid_compute(pid_y, cmd_y, error(1), error_last(1), 0.001);

          error_last = error;

          Eigen::Vector3d cmd(cmd_x, cmd_y, 0);

          //trans imu_init frame cmd into body frame
          cmd = lio_pose.q.inverse() * cmd;

          //feedforward
          vel_cmd.linear.x = cmd(0);
          vel_cmd.angular.z = cmd(1);
          //vel_cmd.linear.x += data[count_].vel[0];

          if (fabs(vel_cmd.linear.x) > maxCmdX)
          {
              vel_cmd.linear.x = vel_cmd.linear.x * maxCmdX / fabs(vel_cmd.linear.x);
          }
          else if (fabs(vel_cmd.linear.x) < minCmdX)
          {
              vel_cmd.linear.x = 0;
          }

          if (fabs(vel_cmd.angular.z) > maxCmdW)
          {
              vel_cmd.angular.z = vel_cmd.angular.z * maxCmdW / fabs(vel_cmd.angular.z);
          }
          else if (fabs(vel_cmd.angular.z) < minCmdW)
          {
          vel_cmd.angular.z = 0;
          }
          pub_cmd.publish(vel_cmd);
          r.sleep();
        }
      }
  }
}


int main(int argc, char **argv)
{
    ros::init(argc, argv, "husky_control");
    ros::NodeHandle nh;

    bool init = readParameter(nh);

    if(init)
    {
        std::cout << "Trajectory generator"<<std::endl;

        std::cout << "\nHUSKY_PATH NODE : " <<
                     "\nPOS FEEDBACK TOPIC IS " << POS_topic <<  std::endl;

        std::cout << "\nPID CONTROL  : " <<
                     "\nX AXIS KP " << pid_x.KP << " KI " << pid_x.KI << " KD " << pid_x.KD <<
                     "\nY AXIS KP " << pid_y.KP << " KI " << pid_y.KI << " KD " << pid_y.KD << std::endl;

    }
    else
    {
        ros::shutdown();
    }

    ros::Subscriber sub_pos = nh.subscribe<nav_msgs::Odometry> (POS_topic, 20, cb_pos);

    //qp的路徑
    path_pub = nh.advertise<nav_msgs::Path>("trajectory",1, true);
    pub_cmd = nh.advertise<geometry_msgs::Twist> ("/cmd_vel", 100);


    //init setting
    ncrl_tf::setTransFrame(lio_pose, "WORLD", "IMU");

    //process have no ()
    std::thread ctrl_process{process};

    ros::spin();
    return 0;
}
