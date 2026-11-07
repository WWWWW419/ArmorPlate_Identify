// OpenCV 环境自检 + 图像处理入门示例
//
// 用法：
//   opencv_learn_demo                     用程序生成的合成图跑一遍，弹窗显示
//   opencv_learn_demo 图片路径             用你自己的图片跑，弹窗显示
//   opencv_learn_demo --headless          不弹窗，把结果写到 output/ 目录
//   opencv_learn_demo 图片路径 --headless  两者结合
//
// 处理流程：原图 -> 灰度 -> 高斯滤波 -> Canny 边缘

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui.hpp>

#include <filesystem>
#include <iostream>
#include <string>

namespace {

// 命令行里有没有这个开关
bool hasFlag(int argc, char** argv, const std::string& flag)
{
    for (int i = 1; i < argc; ++i) {
        if (flag == argv[i]) {
            return true;
        }
    }
    return false;
}

// 取第一个不以 -- 开头的参数，当作图片路径
std::string imagePath(int argc, char** argv)
{
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg.rfind("--", 0) != 0) {
            return arg;
        }
    }
    return {};
}

// 现场画一张合成图，这样没有图片素材也能立刻跑起来
cv::Mat makeSyntheticImage()
{
    cv::Mat img(400, 600, CV_8UC3, cv::Scalar(40, 40, 40));
    cv::circle(img, cv::Point(200, 200), 80, cv::Scalar(0, 180, 255), -1);
    cv::rectangle(img, cv::Point(360, 120), cv::Point(520, 280),
                  cv::Scalar(80, 220, 80), -1);
    cv::line(img, cv::Point(40, 360), cv::Point(560, 360),
             cv::Scalar(220, 120, 60), 3);
    cv::putText(img, "OpenCV", cv::Point(185, 330),
                cv::FONT_HERSHEY_SIMPLEX, 1.2, cv::Scalar(255, 255, 255), 2);
    return img;
}

}  // namespace

int main(int argc, char** argv)
{
    const bool headless = hasFlag(argc, argv, "--headless");

    std::cout << "OpenCV 版本: " << CV_VERSION << std::endl;

    // ---------------------------------------------------------------
    // 1. 准备图像
    // ---------------------------------------------------------------
    cv::Mat img;
    const std::string path = imagePath(argc, argv);
    if (!path.empty()) {
        img = cv::imread(path, cv::IMREAD_COLOR);
        if (img.empty()) {
            std::cerr << "读取失败: " << path << "\n"
                      << "请确认路径存在、且扩展名和真实格式一致。" << std::endl;
            return 1;
        }
        std::cout << "已读取 " << path << "，尺寸 " << img.cols << "x" << img.rows
                  << "，通道数 " << img.channels() << std::endl;
    } else {
        img = makeSyntheticImage();
        std::cout << "未提供图片参数，使用程序生成的合成图。" << std::endl;
    }

    // ---------------------------------------------------------------
    // 2. 处理：灰度 -> 高斯滤波 -> Canny 边缘
    // ---------------------------------------------------------------
    cv::Mat gray, blurred, edges;
    cv::cvtColor(img, gray, cv::COLOR_BGR2GRAY);
    cv::GaussianBlur(gray, blurred, cv::Size(5, 5), 0);
    cv::Canny(blurred, edges, 50, 150);

    const cv::Scalar meanVal = cv::mean(gray);
    double minVal = 0.0, maxVal = 0.0;
    cv::minMaxLoc(gray, &minVal, &maxVal);
    std::cout << "灰度统计：均值 " << meanVal[0]
              << "，最小 " << minVal << "，最大 " << maxVal << std::endl;

    // 顺便验证一下核心数据结构确实算出了东西
    std::cout << "原图 type == CV_8UC3 ? "
              << (img.type() == CV_8UC3 ? "是" : "否") << std::endl;
    std::cout << "边缘图像素总数 " << edges.total() << std::endl;

    // ---------------------------------------------------------------
    // 3. 输出
    // ---------------------------------------------------------------
    if (headless) {
        std::filesystem::create_directories("output");
        cv::imwrite("output/1_original.png", img);
        cv::imwrite("output/2_gray.png", gray);
        cv::imwrite("output/3_edges.png", edges);
        std::cout << "已写出 output/1_original.png、2_gray.png、3_edges.png"
                  << std::endl;
    } else {
        cv::imshow("1 - 原图", img);
        cv::imshow("2 - 灰度", gray);
        cv::imshow("3 - Canny 边缘", edges);
        std::cout << "已弹出 3 个窗口，在任意窗口按一下键退出。" << std::endl;
        cv::waitKey(0);
        cv::destroyAllWindows();
    }

    return 0;
}
