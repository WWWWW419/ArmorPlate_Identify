#pragma once
#include <opencv2/opencv.hpp>
#include <string>
#include <vector>
#include <filesystem>
constexpr float kNoMatch = 1e9f;//"还没有匹配",一个很大的数,确保后面的数能更新
//颜色类别
enum class BarColor { RED, BLUE, UNKNOWN };
// 灯条颜色范围配置
struct LampColorRange {
    // 红色:通常跨 H 接缝，用两段
    cv::Scalar redLow1  {0,   80, 70};    // 0 ~ 28：红偏橙(含过曝的红)
    cv::Scalar redHigh1 {28,  255, 255};
    cv::Scalar redLow2  {160, 80, 70};    // 160 ~ 179:红偏品红
    cv::Scalar redHigh2 {179, 255, 255};

    // 蓝色:单段
    cv::Scalar blueLow  {100, 80, 70};
    cv::Scalar blueHigh {130, 255, 255};

    // 亮度下限（灰度）,设为 0 表示不做亮度约束
    int grayMin = 70;
    int morphCloseSize = 5;    // 闭运算核，0 表示不做
    int morphOpenSize  = 3;    // 开运算核，0 表示不做
};

// 输出的两个 mask
struct ArmorColorMasks {
    cv::Mat red;
    cv::Mat blue;
    cv::Mat bright;      // 亮度掩码(grayMin > 0 时才有内容)
};
//灯条参数
struct LampBar{
    cv::RotatedRect rr;
    cv::Point2f center;
    float longSide  = 0.0f;
    float shortSide = 0.0f;
    float ratio     = 0.0f;                       // 短/长
    cv::Point2f axis{1.0f, 0.0f};                 // 长轴单位向量
    cv::Scalar  meanHsv{0.0, 0.0, 0.0};           // 平均色 H/S/V
    BarColor color = BarColor::UNKNOWN;             //颜色类别
};
//灯条配对参数
struct PairResult{
    bool  valid  = false;//是否通过全部硬性检查
    int   i = -1, j = -1;//在bars中的下标
    float score  = kNoMatch;//加权分数,越小表示越像装甲板
    float colorDiff   = 0.0f;//两条灯条平均色相的环距
    float lenDiff     = 0.0f;//长度差
    float angleDiff   = 0.0f;//长轴夹角
    float offsetRatio = 0.0f;//上下偏差
    float gapRatio    = 0.0f;//间距
    std::string failReason;//未通过原因
};
//装甲板
struct ArmorPlate {
    int barA = -1, barB = -1;       // 组成它的两条灯条索引
    cv::Point2f center;             //装甲板中心
    cv::Point2f corners[4];        // 0=左上 1=右上 2=右下 3=左下             
    float width  = 0.0f;            // 板宽(两条灯条间距)
    float height = 0.0f;            // 板高(灯条长度)
    float angleDeg = 0.0f;          // 长轴方向
    float score = 1e9f;             // 配对得分(越小越好)
    cv::RotatedRect rotated;        // 便于画框
    cv::Rect bbox;                  //轴对齐外接框
    bool valid = true;          //是否有效
    std::string colorName;    // "RED" / "BLUE"
};

//函数声明
bool    SaveImage(const cv::Mat& img, const std::string& fileName);
void    CutROI(const int& x, const int& y, const int& w, const int& h, const cv::Mat& img);
cv::Mat ZoomImage(const cv::Mat& img, const double& scale);
cv::Mat MakeCanvas(const cv::Mat& src);
ArmorColorMasks MakeArmorColorMasks(const cv::Mat& hsv, const cv::Mat& gray, const LampColorRange& range = LampColorRange());
std::vector<LampBar> CollectBars(const cv::Mat& hsv, const cv::Mat& mask, BarColor color);
bool EvaluatePair(const LampBar& a, const LampBar& b, PairResult& out);
bool BuildArmorPlate(const std::vector<LampBar>& bars, const PairResult& p, ArmorPlate& out);
std::vector<ArmorPlate> DetectArmorPlates(const std::vector<LampBar>& bars);
void DrawArmorPlate(cv::Mat& vis, const ArmorPlate& pl, int index);
std::vector<ArmorPlate> Identify_armor_plates(const cv::Mat& img, cv::Mat* debugVis = nullptr);