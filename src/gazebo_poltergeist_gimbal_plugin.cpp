#include <functional>
#include <gazebo/gazebo.hh>
#include <gazebo/physics/physics.hh>
#include <gazebo/common/common.hh>
#include <ignition/math/Vector3.hh>
#include <ros/ros.h>
#include <ros/subscribe_options.h>
#include <geometry_msgs/Vector3Stamped.h>

namespace gazebo
{

//// Default values
static const std::string kDefaultParentFrameId = "gimbal_support";
static const std::string kDefaultChildFrameId = "camera_mount";
static const std::string kDefaulGimbaltLinkName = "gimbal_support_link";
static const std::string kDefaultCameraLinkName = "camera_mount_link";
static const  float kDEG_2_RAD = M_PI / 180.0;

class GazeboPoltergeistGimbalPlugin : public ModelPlugin
{
public:
    void Load(physics::ModelPtr _parent, sdf::ElementPtr /*_sdf*/)
    {

        // Initialize ros, if it has not already bee initialized.
        if (!ros::isInitialized())
        {
            int argc = 0;
            char **argv = NULL;
            ros::init(argc, argv, "gazebo_client8",
                      ros::init_options::NoSigintHandler);
        }
        // Create our ROS node. This acts in a similar manner to
        // the Gazebo node
        this->node_handle_.reset(new ros::NodeHandle("gazebo_client8"));

        rpy_sub_ = node_handle_->subscribe<geometry_msgs::Vector3Stamped>(
                    "/command", 10, &GazeboPoltergeistGimbalPlugin::onRPYCallback ,this);

        // Store the pointer to the model
        this->model_ = _parent;

        gimbal_support_link_ = this->model_->GetChildLink(kDefaulGimbaltLinkName);
        camera_mount_link_ = this->model_->GetChildLink(kDefaultCameraLinkName);
        goal_point_.Set();
        base_point_ = camera_mount_link_->GetRelativePose().rot.GetAsEuler().Ign();



        //        rpyPub = node_handle_->Advertise<gz_std_msgs::ConnectGazeboToRosTopic>(
        //                  "~/" + kConnectGazeboToRosSubtopic, 1);


        // Listen to the update event. This event is broadcast every
        // simulation iteration.
        this->update_connection_ = event::Events::ConnectWorldUpdateBegin(
                    std::bind(&GazeboPoltergeistGimbalPlugin::OnUpdate, this));
        ROS_WARN("Hello World! gazebo_poltergeist_gimbal model v5");
    }

    void onRPYCallback(const geometry_msgs::Vector3Stamped::ConstPtr& msg) {
        ROS_INFO_STREAM( "Gimbal Request Roll:"<< msg->vector.x << " Pitch:"<< msg->vector.y << " Yaw:"<< msg->vector.z << " ");
        bool input_valid = true;
        if(std::abs(msg->vector.x) > 180)
        {
            ROS_WARN_STREAM("Gimbal Request - invalid ROLL [-180,180]");
            input_valid = false;
        }

        if(std::abs(msg->vector.y) > 180)
        {
            ROS_WARN_STREAM("Gimbal Request - invalid PITCH [-180,180]");
            input_valid = false;
        }

        if(std::abs(msg->vector.z) > 180)
        {
            ROS_WARN_STREAM("Gimbal Request - invalid YAW [-180,180]");
            input_valid = false;
        }

        if(input_valid)
        {
            goal_point_= ignition::math::Vector3<double>(msg->vector.x,msg->vector.y,msg->vector.z);
            gimbal_status_ = MOVING;
        }

    }

    // Called by the world update start event
    void OnUpdate()
    {
        // Apply a small linear velocity to the model.
        //this->model->SetLinearVel(ignition::math::Vector3d(.3, 0, 0));


        if(gimbal_status_ == STATIC)
        {
            camera_mount_link_->SetAngularVel(ignition::math::Vector3d(0.0, 0, 0));
        }else if(gimbal_status_ == MOVING)
        {
            double curr_position = ConvertAngle180(camera_mount_link_->GetRelativePose().rot.GetAsEuler().x/kDEG_2_RAD);

            //camera_mount_link_->SetAngularVel(ignition::math::Vector3d(0.0, 0, 0));

            double angle_diff =  curr_position - goal_point_.X();

            if(std::abs(angle_diff) < 1.0)
            {
                gimbal_status_ = STATIC;
                camera_mount_link_->SetAngularVel(ignition::math::Vector3d(0.0, 0, 0));
                //todo correct the position to the ideal value
            }else
            {
                if(angle_diff > 0)
                    camera_mount_link_->SetAngularVel(ignition::math::Vector3d(0.3, 0, 0));
                else
                    camera_mount_link_->SetAngularVel(ignition::math::Vector3d(-0.3, 0, 0));
            }
            ROS_INFO_STREAM( camera_mount_link_->GetRelativePose() << " diff:" << angle_diff << " curr:" << curr_position << " goal:" << goal_point_.X()  ); //camera_mount_link_->GetRelativePose() <<
        }
    }

    double ConvertAngle180(double angle) //[-180,180]
    {
        return std::fmod(angle+180.0,360.0)-180.0;// 0->0 , 10->10, 350->-10
    }

    double ConvertAngle360(double angle)//[0,360]
    {
        return std::fmod(angle+360.0,360.0);// 0 -> 0 , 10 -> 10, -10 -> 350
    }



    double AngleClamp( float angle )
    {
        if (angle > 180)
        {
            angle = 180;
        }
        if (angle < -180)
        {
            angle = -180;
        }
        return angle;
    }

    // Pointer to the model
private:

    ///GAZEBO

    physics::ModelPtr model_;

    std::unique_ptr<ros::NodeHandle> node_handle_;

    physics::LinkPtr gimbal_support_link_;
    physics::LinkPtr camera_mount_link_;

    ignition::math::Vector3d goal_point_;
    ignition::math::Vector3d base_point_;

    // Pointer to the update event connection
    event::ConnectionPtr update_connection_;

    ///ROS

    /// \brief A node use for ROS transport
private:

    std::unique_ptr<ros::NodeHandle> ros_node_;

    /// \brief A ROS subscriber
    ros::Subscriber rpy_sub_;

    ros::Publisher rpy_pub_;
    ros::Publisher transform_gimbal_camera_pub_;
    ros::Publisher gimbal_camera_tf_pub;

    ///STATE

    enum GimbalStatus
    {
        STATIC,
        MOVING
    };

    GimbalStatus gimbal_status_;



};

// Register this plugin with the simulator
GZ_REGISTER_MODEL_PLUGIN(GazeboPoltergeistGimbalPlugin)
}


