#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/image.hpp>

#include <cv_bridge/cv_bridge.h>

#include <stdint.h>
#include <assert.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stddef.h>

#include <string>
#include <sstream>
#include <unistd.h>

#include "std_msgs/msg/string.hpp"

class BRAM {

public:
    BRAM(unsigned int uio_number, unsigned int size) {
		char device_file_name[20];
		sprintf(device_file_name, "/dev/uio%d", uio_number);

		int device_file;

		if ((device_file = open(device_file_name, O_RDWR | O_SYNC)) < 0) {
			std::stringstream ss;
			ss << device_file_name << " could not be opened";
			throw ss.str();
		}

		bram_ptr = (float *)mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, device_file, 0x0);

		if (bram_ptr == NULL) {
			std::stringstream ss;
			ss << "Could not map memory";
			throw ss.str();
		}
	}

    float& operator[](unsigned int index){
		return bram_ptr[index];
	}
    float *GetPointer(){
		return bram_ptr;
	}
private:
    float *bram_ptr;

};


class ImageSubscriber : public rclcpp::Node
{
	public:
		ImageSubscriber(BRAM* driver) : Node("image_subscriber") {
			RCLCPP_INFO(this->get_logger(), "Initializing ImageSubscriber node");

			bram_driver = driver;

			RCLCPP_INFO(this->get_logger(), "Starting camera subscription");

			camera_subscription_ = this->create_subscription<sensor_msgs::msg::Image>(
					"/image_raw",
					10,
					std::bind(&ImageSubscriber::onImageMsg, this, std::placeholders::_1)
			);

			net_output_publisher = this->create_publisher<std_msgs::msg::String>("/net_output", 10);

		}

	private:
		rclcpp::Publisher<std_msgs::msg::String>::SharedPtr net_output_publisher;
		rclcpp::Subscription<sensor_msgs::msg::Image>::SharedPtr camera_subscription_;

		BRAM* bram_driver;

		void onImageMsg(const sensor_msgs::msg::Image::SharedPtr msg) {
			// RCLCPP_INFO(this->get_logger(), "Received image!");

			cv_bridge::CvImagePtr cv_ptr = cv_bridge::toCvCopy(msg, msg->encoding);
			cv::Mat img = cv_ptr->image;

			// RCLCPP_INFO(this->get_logger(), "Successfully loaded image");

			// Resize the image to 64 by 48 (columns and rows are flipped in CV)
			cv::resize(img, img, cv::Size(48, 64));

			// RCLCPP_INFO(this->get_logger(), "Resized");

			// Convert each pixel to grayscale
			cv::cvtColor(img, img, cv::COLOR_YUV2RGB_Y422);

			// RCLCPP_INFO(this->get_logger(), "Converted to RGB");

			cv::cvtColor(img, img, cv::COLOR_RGB2GRAY);

			// RCLCPP_INFO(this->get_logger(), "Converted to grayscale");

			// Normalize each pixel value to be between 0 and 1
			img.convertTo(img, CV_32F, 1.0 / 255.0);

			// RCLCPP_INFO(this->get_logger(), "Normalized");

			for (int i = 0; i < img.rows; i++) {
				for (int j = 0; j < img.cols; j++) {
					float normalized_value = img.at<float>(i, j);

					(*(this->bram_driver))[(i*48) + j] = normalized_value;

					float read_value = (*(this->bram_driver))[(i*48) + j];

					// char debug_message[350];
					// sprintf(debug_message, "Sent %f to BRAM %d and read %f", normalized_value, ((i*48) + j), read_value);

					// RCLCPP_INFO(this->get_logger(), debug_message);
				}
			}

			// RCLCPP_INFO(this->get_logger(), "Sent to BRAM");

			sleep(1);

			unsigned long int result = (unsigned long int)(*(this->bram_driver))[128];

			for(int i=0; i < 4096; i++){

				float read_value = (*(this->bram_driver))[i];

				char debug_message[350];
				sprintf(debug_message, "Read %f from BRAM %d", read_value, i);

				RCLCPP_INFO(this->get_logger(), debug_message);

			}

			return;

			// choose whether or not to publish this.

			uint8_t percentage_flat = result >> 16;
			uint8_t percentage_philips = (result >> 8) & 0x000000FF;
			uint8_t percentage_hole = result & 0x000000FF;

			
			// auto message = std_msgs::msg::String();
			// message.data = std::to_string(result);
			// net_output_publisher->publish(message);

			char debug_message[350];
			sprintf(debug_message, "Flat %u Philips %u Hole %u", percentage_flat, percentage_philips, percentage_hole);

			RCLCPP_INFO(this->get_logger(), debug_message);
		}

};

int main(int argc, char *argv[])
{
	BRAM bram_driver(0, 16384);
	setvbuf(stdout,NULL,_IONBF,BUFSIZ);

	rclcpp::init(argc,argv);
	rclcpp::spin(std::make_shared<ImageSubscriber>(&bram_driver));

	rclcpp::shutdown();
	return 0;
}
