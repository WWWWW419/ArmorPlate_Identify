#include "ImageProcess.h"
#include <iostream>
#include <filesystem>
#include <opencv2/opencv.hpp>
using namespace std;

int main(){
    //读取文件
    //APP_ROOT_DIR 由 CMakeLists.txt 注入，锚定项目根目录，
    const std::filesystem::path imagePath =
        std::filesystem::path(APP_ROOT_DIR) / "images" / "outpost_frame.png";

    cv::Mat img = cv::imread(imagePath.string(), cv::IMREAD_COLOR);
    if (img.empty()){
        cerr << "Read failed, please check the file path: "
             << imagePath.string() << endl;
        return -1;
    }

    std::cout << "Image: " << img.cols << " x " << img.rows
              << ", channels: " << img.channels() << std::endl;

    cv::Mat debugVis;
    const auto plates = Identify_armor_plates(img, &debugVis);

    std::cout << "Total armor plates: " << plates.size() << std::endl;
}