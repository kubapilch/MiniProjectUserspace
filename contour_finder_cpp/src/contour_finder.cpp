#include "rclcpp/rclcpp.hpp"
#include "cv_bridge/cv_bridge.h"
#include "sensor_msgs/msg/image.hpp"
#include "std_msgs/msg/string.hpp"
#include <opencv2/opencv.hpp>
#include "dynamixel_sdk_custom_interfaces/msg/set_position.hpp"
#include <vector>


void findContourAngle(cv::Mat image,double* angle) {
    int min_contour_area=50;
    int lower_canny=12;
    int upper_canny=30;
    int min_contour_length = 50;
    int max_contour_length = 1000;
    int min_contour_width = 50;
    int max_contour_width = 1000;
    
    cv::cvtColor(image, image, cv::COLOR_YUV2RGB_Y422);
    cv::cvtColor(image, image, cv::COLOR_RGB2GRAY);
    // cv::GaussianBlur(gray, gray, cv::Size(5, 5), 0);  // Uncomment if GaussianBlur is needed
    cv::Mat edges;
    cv::Canny(image, edges, lower_canny, upper_canny);

    std::vector<std::vector<cv::Point>> contours;
    std::vector<cv::Vec4i> hierarchy;
    cv::findContours(edges.clone(), contours, hierarchy, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    
    std::vector<std::vector<cv::Point>> filtered_contours;
    for (const auto& contour : contours) {
        if (cv::contourArea(contour) > min_contour_area) {
            int contour_length = cv::arcLength(contour, true);
            int contour_width = cv::boundingRect(contour).width;
            if ((contour_length > min_contour_length && contour_length < max_contour_length) &&
                (contour_width > min_contour_width && contour_width < max_contour_width)) {
                filtered_contours.push_back(contour);
            }
        }
    }

    std::vector<double> angles;
    for (const auto& filtered_contour : filtered_contours) {
        double epsilon = 0.04 * cv::arcLength(filtered_contour, true);
        std::vector<cv::Point> approx;
        cv::approxPolyDP(filtered_contour, approx, epsilon, true);
        cv::RotatedRect rect = cv::minAreaRect(approx);
        cv::Point2f box[4];
        rect.points(box);
        angles.push_back(rect.angle);
    }
    if(!angles.empty()){
        if(angles.at(0)<0)
            *angle=360.0+angles.at(0);
        else
            *angle=angles.at(0);
    }
}


class ContourFinder : public rclcpp::Node{
    public:
		ContourFinder() : Node("contour_finder") {
			RCLCPP_INFO(this->get_logger(), "Initializing ContourFinder node");

			camera_subscription_ = this->create_subscription<sensor_msgs::msg::Image>(
					"/image_raw",
					10,
					std::bind(&ContourFinder::onImageMsg, this, std::placeholders::_1)
			);
            network_out_subscription_=this->create_subscription<std_msgs::msg::String>(
                "/net_output",
                10,
                std::bind(&ContourFinder::onNetOutMsg,this,std::placeholders::_1)
            );
            position_publisher_=this->create_publisher<dynamixel_sdk_custom_interfaces::msg::SetPosition>(
            "/set_position", 10);
            timer_ = this->create_wall_timer(
                std::chrono::milliseconds(1000),
                std::bind(&ContourFinder::publish_new_position, this));    

		}

	private:
		rclcpp::Subscription<sensor_msgs::msg::Image>::SharedPtr camera_subscription_;
        rclcpp::Subscription<std_msgs::msg::String>::SharedPtr network_out_subscription_;
        rclcpp::Publisher<dynamixel_sdk_custom_interfaces::msg::SetPosition>::SharedPtr position_publisher_;
        rclcpp::TimerBase::SharedPtr timer_;
        double socket_angle=0.0;
        cv::Mat last_img;

		void onImageMsg(const sensor_msgs::msg::Image::SharedPtr msg) {
			RCLCPP_INFO(this->get_logger(), "Received image!");

			cv_bridge::CvImagePtr cv_ptr = cv_bridge::toCvCopy(msg, msg->encoding);
			last_img= cv_ptr->image;
		}
        void onNetOutMsg(const std_msgs::msg::String::SharedPtr msg){
            RCLCPP_INFO(this->get_logger(), "Received network output!");
            findContourAngle(last_img,&socket_angle);
            RCLCPP_INFO(this->get_logger(), "Processed image.");

        }
          void publish_new_position() {
            auto message = dynamixel_sdk_custom_interfaces::msg::SetPosition();
            message.id = 1;
            message.position = socket_angle*(1024.0/300.0);

            RCLCPP_INFO(this->get_logger(), "Publishing: id = %d, position = %f", message.id, message.position);
            position_publisher_->publish(message);
        }
};
int main(int argc, char *argv[]){
    setvbuf(stdout,NULL,_IONBF,BUFSIZ);

	rclcpp::init(argc,argv);
	rclcpp::spin(std::make_shared<ContourFinder>());

	rclcpp::shutdown();
	return 0;
}
