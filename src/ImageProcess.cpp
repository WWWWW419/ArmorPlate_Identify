#include "ImageProcess.h"
#include <opencv2/opencv.hpp>
#include <string>
#include <filesystem>
#include <vector>
//默认保存路径
const std::filesystem::path outImagePath = std::filesystem::path(APP_ROOT_DIR) / "output";
//生成红蓝两色mask
ArmorColorMasks MakeArmorColorMasks(const cv::Mat& hsv,
                                    const cv::Mat& gray,
                                    const LampColorRange& range )
{
    ArmorColorMasks out;

    // 颜色提取 
    cv::Mat r1, r2;
    cv::inRange(hsv, range.redLow1,  range.redHigh1, r1);
    cv::inRange(hsv, range.redLow2,  range.redHigh2, r2);
    cv::bitwise_or(r1, r2, out.red);

    cv::inRange(hsv, range.blueLow, range.blueHigh, out.blue);

    // 亮度约束 
    if (range.grayMin > 0 && !gray.empty()) {
        cv::threshold(gray, out.bright, range.grayMin, 255, cv::THRESH_BINARY);
        cv::bitwise_and(out.red,  out.bright, out.red);
        cv::bitwise_and(out.blue, out.bright, out.blue);
    }

    // 形态学清理(先闭后开)
    auto Clean = [&](cv::Mat& m) {
        if (m.empty()) return;
        if (range.morphCloseSize > 0) {
            cv::Mat k = cv::getStructuringElement(
                cv::MORPH_ELLIPSE, cv::Size(range.morphCloseSize, range.morphCloseSize));
            cv::morphologyEx(m, m, cv::MORPH_CLOSE, k);
        }
        if (range.morphOpenSize > 0) {
            cv::Mat k = cv::getStructuringElement(
                cv::MORPH_ELLIPSE, cv::Size(range.morphOpenSize, range.morphOpenSize));
            cv::morphologyEx(m, m, cv::MORPH_OPEN, k);
        }
    };
    Clean(out.red);
    Clean(out.blue);
    // 打印统计
    std::cout << "Red pixel: " << cv::countNonZero(out.red) << std::endl
              << "Blue pixel: " << cv::countNonZero(out.blue)
              << std::endl;

    return out;
}
//端点计算
static void BarEnds(const LampBar& bar, const cv::Point2f& axisUp,
                    cv::Point2f& top, cv::Point2f& bottom)
{
    // 长轴方向对齐到"向上"
    cv::Point2f a = bar.axis;
    if (a.dot(axisUp) < 0.0f) a = -a;

    // 端点 = 中心 ± 长轴半长
    const float halfLen = bar.longSide * 0.5f;
    top    = bar.center + a * halfLen;
    bottom = bar.center - a * halfLen;
}
//初始化一块装甲板
ArmorPlate MakePlate(const LampBar &a, const LampBar &b, const PairResult &p){
    ArmorPlate plate;
    plate.barA = p.i;
    plate.barB = p.j;
    plate.score = p.score;
    //中心
    plate.center = (a.center + b.center) / 2;

    //长度
    const float lenAvg = (a.longSide + b.longSide) * 0.5f;
    plate.height = lenAvg;
    plate.width = p.gapRatio * lenAvg;

    //朝向
    cv::Point2f ua = a.axis, ub = b.axis;
    if (ua.dot(ub) < 0.0f) ub = -ub;
    cv::Point2f u = ua + ub;
    u /= cv::norm(u);               //求投影向量
    plate.angleDeg = std::atan2(u.y, u.x) * 180.0f / float(CV_PI);

    plate.rotated = cv::RotatedRect(plate.center, cv::Size2f(plate.width, plate.height), plate.angleDeg);
    return plate;
}
// 取一条灯条沿指定方向的"上端/下端"(返回两端各自两个顶点的平均)
static void BarEnds(const cv::RotatedRect& rr, const cv::Point2f& axisUp,
                    cv::Point2f& top, cv::Point2f& bottom)
{
    cv::Point2f pts[4];
    rr.points(pts);

    // 投影到 axisUp（它已保证指向图像上方）
    float proj[4];
    float maxP = -1e9f, minP = 1e9f;
    for (int k = 0; k < 4; ++k) {
        proj[k] = pts[k].dot(axisUp);
        maxP = std::max(maxP, proj[k]);
        minP = std::min(minP, proj[k]);
    }

    // 投影最大的一端 = 上端；最小的一端 = 下端
    // 每端有 2 个顶点,取平均得到端点
    cv::Point2f sumTop(0, 0), sumBot(0, 0);
    int nTop = 0, nBot = 0;
    const float span = maxP - minP;

    for (int k = 0; k < 4; ++k) {
        if (maxP - proj[k] < span * 0.25f) { sumTop += pts[k]; ++nTop; }
        else if (proj[k] - minP < span * 0.25f) { sumBot += pts[k]; ++nBot; }
    }

    top    = (nTop > 0) ? sumTop / float(nTop) : rr.center;
    bottom = (nBot > 0) ? sumBot / float(nBot) : rr.center;
}

bool BuildArmorPlate(const std::vector<LampBar>& bars,
                     const PairResult& p,
                     ArmorPlate& out)
{
    if (!p.valid) return false;

    // 按 x 区分左右灯条
    int li = p.i, ri = p.j;
    if (bars[li].center.x > bars[ri].center.x) std::swap(li, ri);

    // 统一长轴方向：强制指向上方(y 减小)
    cv::Point2f ua = bars[li].axis;
    cv::Point2f ub = bars[ri].axis;
    if (ua.dot(ub) < 0.0f) ub = -ub;

    cv::Point2f u = ua + ub;
    const float ul = cv::norm(u);
    if (ul < 1e-3f) return false;
    u /= ul;
    if (u.y > 0.0f) u = -u;              // 关键:让它指向图像上方

    // 取四条端点 
    cv::Point2f lt, lb, rt, rb;
    BarEnds(bars[li].rr, u, lt, lb);
    BarEnds(bars[ri].rr, u, rt, rb);

    // 固定顺序：左上 -> 右上 -> 右下 -> 左下
    out.corners[0] = lt;    // 0 左上
    out.corners[1] = rt;    // 1 右上
    out.corners[2] = rb;    // 2 右下
    out.corners[3] = lb;    // 3 左下

    out.barA  = li;
    out.barB  = ri;
    out.score = p.score;

    // 中心 = 四角点平均 
    out.center = (out.corners[0] + out.corners[1]
                + out.corners[2] + out.corners[3]) * 0.25f;

    // 尺寸 
    out.width  = (cv::norm(out.corners[1] - out.corners[0])
                + cv::norm(out.corners[2] - out.corners[3])) * 0.5f;
    out.height = (cv::norm(out.corners[3] - out.corners[0])
                + cv::norm(out.corners[2] - out.corners[1])) * 0.5f;

    // 轴对齐外接框(保证覆盖)
    float minX = 1e9f, maxX = -1e9f, minY = 1e9f, maxY = -1e9f;
    for (int k = 0; k < 4; ++k) {
        minX = std::min(minX, out.corners[k].x);
        maxX = std::max(maxX, out.corners[k].x);
        minY = std::min(minY, out.corners[k].y);
        maxY = std::max(maxY, out.corners[k].y);
    }
    out.bbox = cv::Rect(cvRound(minX), cvRound(minY),
                        cvRound(maxX - minX), cvRound(maxY - minY));

    return true;
}
//收集灯条候选,记录参数
std::vector<LampBar> CollectBars(const cv::Mat& hsv, const cv::Mat& mask, BarColor color)
{
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    std::vector<LampBar> bars;
    for (const auto& c : contours) {
        if (c.size() < 5) continue;//防止minAreaRect退化成线
        if (cv::contourArea(c) < 60) continue;//面积太小直接过
        //记录参数
        LampBar bar;
        bar.color = color;
        bar.rr        = cv::minAreaRect(c);
        bar.longSide  = std::max(bar.rr.size.width, bar.rr.size.height);
        bar.shortSide = std::min(bar.rr.size.width, bar.rr.size.height);
        if (bar.shortSide < 3.0f || bar.longSide < 8.0f) continue;
        bar.ratio = bar.shortSide / bar.longSide;
        if (bar.ratio > 0.80f) continue;          // 太"胖"，不是灯条
        bar.center = bar.rr.center;
        //计算长轴单位向量
        cv::Point2f pts[4];//旋转矩形的四个顶点
        bar.rr.points(pts);
        float maxLen = 0.0f;
        cv::Point2f dir{1.0f, 0.0f};
        for (int k = 0; k < 4; ++k) {
            const cv::Point2f e = pts[(k + 1) % 4] - pts[k];//第k条边的向量
            const float len = cv::norm(e);//这条边的长度
            if (len > maxLen) { maxLen = len; dir = e; }//找到最长的
        }
        if (maxLen < 1e-3f) continue;
        bar.axis = dir / maxLen;//除以长度得到单位向量
        //平均颜色(只统计该轮廓内的像素)
        cv::Mat oneMask = cv::Mat::zeros(mask.size(), CV_8U);
        std::vector<std::vector<cv::Point>> one{c};
        cv::drawContours(oneMask, one, 0, 255, cv::FILLED);
        bar.meanHsv = cv::mean(hsv, oneMask);
        bars.push_back(bar);
    }
    return bars;
}
//配对评估
bool EvaluatePair(const LampBar& a, const LampBar& b, PairResult& out)
{
    //颜色类别不同就直接拒绝
    if (a.color != b.color) return false; 
    //1.颜色一致
    //色相是环形的[0,179],距离要取环距
    const float dhRaw = std::abs(a.meanHsv[0] - b.meanHsv[0]);//两条灯条平均色相的直线差值
    const float dh    = std::min(dhRaw, 180.0f - dhRaw);//换算成环距后的色相差,范围[0, 90]
    const float ds    = std::abs(a.meanHsv[1] - b.meanHsv[1]) / 255.0f;//两条灯条平均饱和度的差，除以 255 归一化

    if (dh > 12.0f)  return false;
    if (ds > 0.35f)  return false;

    //2. 长度相近
    const float lenAvg = (a.longSide + b.longSide) * 0.5f;
    if (lenAvg < 1e-3f) return false;

    const float dLen = std::abs(a.longSide - b.longSide) / lenAvg;
    if (dLen > 0.30f) return false;

    //3. 方向协调
    cv::Point2f ua = a.axis;
    cv::Point2f ub = b.axis;
    if (ua.dot(ub) < 0.0f) ub = -ub;                 // 先对齐朝向

    const float angRad = std::atan2(ua.cross(ub), ua.dot(ub));
    const float dAng   = std::abs(angRad) * 180.0f / float(CV_PI);
    if (dAng > 15.0f) return false;

    // 平均长轴
    cv::Point2f u = ua + ub;
    const float ul = cv::norm(u);
    if (ul < 1e-3f) return false;
    u /= ul;

    //4 & 5. 投影到灯条自身坐标系 
    const cv::Point2f d = b.center - a.center;
    const float offset  = std::abs(d.dot(u));          // 沿长轴 → 上下偏差
    const cv::Point2f n(-u.y, u.x);                    // 垂直长轴
    const float gap     = std::abs(d.dot(n));          // 垂直长轴 → 水平间距

    const float offsetRatio = offset / lenAvg;
    if (offsetRatio > 0.40f) return false;

    //6. 间距/长度比，像不像一块装甲板
    const float gapRatio = gap / lenAvg;
    if (gapRatio < 0.8f || gapRatio > 3.5f) return false;

    //全部通过：加权评分（越小越好）
    out.valid = true;
    out.colorDiff   = dh;
    out.lenDiff     = dLen;
    out.angleDiff   = dAng;
    out.offsetRatio = offsetRatio;
    out.gapRatio    = gapRatio;

    out.score = (dh        / 12.0f) * 1.0f
              + (dLen      /  0.30f) * 1.0f
              + (dAng      / 15.0f) * 1.5f
              + (offsetRatio / 0.40f) * 1.5f
              + (std::abs(gapRatio - 2.0f) / 1.5f) * 1.0f;   // 最理想 ≈ 2.0
    if (dh > 12.0f) { out.failReason = "failreason 1"; return false; }//色相差过大
    if (dLen > 0.30f) { out.failReason = "failreason 2"; return false; }//长度差过大
    if (dAng > 15.0f) { out.failReason = "failreason 3"; return false; }//方向不协调
    if (offsetRatio > 0.40f) { out.failReason = "failreason 4"; return false; }//上下偏差过大
    if (gapRatio < 0.8f || gapRatio > 3.5f) { out.failReason = "failreason 5"; return false; }//间距比例不像装甲板
    return true;
}
//灯条配对主逻辑
PairResult MatchLampBars(const std::vector<LampBar>& bars)
{
    PairResult best;
    for (size_t i = 0; i < bars.size(); ++i) {
        for (size_t j = i + 1; j < bars.size(); ++j) {
            PairResult p;
            if (!(EvaluatePair(bars[i], bars[j], p))) continue;
            p.i = int(i);
            p.j = int(j);
            if (p.score < best.score) best = p;
        }
    }
    return best;
}
//装甲板去重
std::vector<ArmorPlate> DetectArmorPlates(const std::vector<LampBar>& bars)
{
    // 收集所有通过评估的配对 
    std::vector<PairResult> candidates;
    for (size_t i = 0; i < bars.size(); ++i) {
        for (size_t j = i + 1; j < bars.size(); ++j) {
            PairResult p;
            if (!(EvaluatePair(bars[i], bars[j], p))) continue;
            p.i = int(i);
            p.j = int(j);
            candidates.push_back(p);
        }
    }

    // 按分数从优到劣 
    std::sort(candidates.begin(), candidates.end(),
              [](const PairResult& a, const PairResult& b) {
                  return a.score < b.score;
              });

    // 唯一匹配 + 中心去重 
    std::vector<bool> used(bars.size(), false);
    std::vector<ArmorPlate> plates;

    for (const auto& p : candidates) {
        if (used[p.i] || used[p.j]) continue;          // 灯条已被占用

        ArmorPlate pl;
        if (!BuildArmorPlate(bars, p, pl)) continue;   // ★ 调用统一的构建函数

        bool dup = false;
        for (const auto& q : plates) {
            const float thresh = 0.8f * std::min(pl.height, q.height);
            if (cv::norm(pl.center - q.center) < thresh) {
                dup = true;
                break;
            }
        }
        if (dup) continue;

        used[p.i] = true;
        used[p.j] = true;
        plates.push_back(pl);
    }

    return plates;
}
//保存图片
bool SaveImage(const cv::Mat &mat, const std::string &FileName){
    if (mat.empty()){
        std::cerr << "error, cannot find image" << std::endl;
        return false;
    }
    //imwrite()不会创建目录，需要用filesystem创建
    std::filesystem::create_directories(outImagePath);
    const std::string FullPath = (outImagePath / FileName).string();
    if (!(cv::imwrite(FullPath , mat))){
        std::cerr << "save image failed" << std::endl;
        return false;
    }
    std::cout << FileName << " saved successfully" << std::endl;
    return true;

}
//裁剪roi区域
void CutROI(const int &x, const int &y, const int &w, const int &h, const cv::Mat &img){
    if (x < 0 || y < 0 || w <= 0 || h <= 0 || x + w > img.cols || y + h > img.rows){
        std::cerr << "ROI out of range" << std::endl;
    }else{
        cv::Rect region(x, y, w, h);
        cv::Mat roi = img(region).clone();
        SaveImage(roi, "roi.png");
    }
}
//缩放
cv::Mat ZoomImage(const cv::Mat &img, const double &scale){
    cv::Mat resized;
    cv::resize (img, resized, cv::Size(), scale, scale);
    return resized;
}
//把黑白的图像转为彩色
cv::Mat MakeCanvas(const cv::Mat& src)
{
    cv::Mat canvas;
    if (src.channels() == 1) {
        cv::cvtColor(src, canvas, cv::COLOR_GRAY2BGR);
    } else {
        canvas = src.clone();
    }
    return canvas;
}
//装甲板绘制
void DrawArmorPlate(cv::Mat& vis, const ArmorPlate& pl, int index)
{
    // 轴对齐外接框(蓝色，保证覆盖整个候选区域)
    cv::rectangle(vis, pl.bbox, cv::Scalar(255, 128, 0), 1, cv::LINE_AA);

    // 四角点连成的四边形(绿色，精确贴合)
    for (int k = 0; k < 4; ++k) {
        cv::line(vis, pl.corners[k], pl.corners[(k + 1) % 4],
                 cv::Scalar(0, 255, 0), 2, cv::LINE_AA);
    }

    // 中心点
    cv::circle(vis, pl.center, 4, cv::Scalar(0, 0, 255), -1, cv::LINE_AA);

    // 四个角点标号 0/1/2/3(黄色小圆 + 数字)
    for (int k = 0; k < 4; ++k) {
        const cv::Point pi(cvRound(pl.corners[k].x), cvRound(pl.corners[k].y));

        cv::circle(vis, pi, 3, cv::Scalar(0, 255, 255), -1, cv::LINE_AA);

        const std::string num = std::to_string(k);
        cv::putText(vis, num,
                    cv::Point(pi.x + 5, pi.y - 5),
                    cv::FONT_HERSHEY_SIMPLEX, 0.6,
                    cv::Scalar(0, 255, 255), 2, cv::LINE_AA);
    }
}
std::vector<ArmorPlate> Identify_armor_plates(const cv::Mat &img, cv::Mat* debugVis){

    std::vector<ArmorPlate> result;    
    //目标颜色mask
    cv::Mat hsv, gray;
    cv::cvtColor(img, hsv, cv::COLOR_BGR2HSV);
    cv::cvtColor(img, gray, cv::COLOR_BGR2GRAY);
    //调整参数

    LampColorRange myRange;              // 先拿默认值
    myRange.redHigh1 = cv::Scalar(32, 255, 255);   
    myRange.redLow1 = cv::Scalar(0, 45, 55);

    const auto masks = MakeArmorColorMasks(hsv, gray, myRange);
    SaveImage (masks.red, "RedMask.png");
    SaveImage (masks.blue, "BlueMask.png");
    cv::Mat vis = MakeCanvas(img);
    int plateIndex = 0;

    struct Job { const cv::Mat* mask; const char* tag; BarColor color;};
    const Job jobs[2] = {
        { &masks.red,  "RED",  BarColor::RED  },
        { &masks.blue, "BLUE", BarColor::BLUE }
    };
    cv::Mat barVis = MakeCanvas(img);
    int barIdx = 0;
    for (const auto& job : jobs) {
        if (cv::countNonZero(*job.mask) < 50) continue;   // 该颜色几乎没出现

        // 找灯条
        const auto bars = CollectBars(hsv, *job.mask, job.color);
  
        for (const auto& bar : bars) {
            cv::Point2f pts[4];
            bar.rr.points(pts);
            for (int m = 0; m < 4; ++m) {
                cv::line(barVis, pts[m], pts[(m + 1) % 4],
                        job.tag[0] == 'R' ? cv::Scalar(0, 0, 255) : cv::Scalar(255, 0, 0),
                        2, cv::LINE_AA);
            }
            cv::circle(barVis, bar.center, 3, cv::Scalar(0, 255, 255), -1);

            // 编号（每个颜色内部从 0 开始）
            cv::putText(barVis, std::to_string(barIdx++),
                    cv::Point(int(bar.center.x) + 6, int(bar.center.y) - 6),
                    cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 255, 255), 2);
        }
        // 配对 + 装甲板去重
        const auto plates = DetectArmorPlates(bars);

        std::cout << job.tag << ": lampbar candidates " << bars.size()
                  << ", Armor plate " << plates.size() << std::endl;


        int n = 1;
        //输出装甲板参数
        for (auto pl : plates) {
            pl.colorName = job.tag;
            result.push_back(pl);

            DrawArmorPlate(vis, pl, plateIndex++);

            std::cout << "=== Armor Plate #" << n++ << " ===\n"
              << "  Color   : " << pl.colorName << "\n"
              << "  BBox    : x=" << pl.bbox.x << " y=" << pl.bbox.y
              << " w=" << pl.bbox.width << " h=" << pl.bbox.height << "\n"
              << "  Center  : (" << pl.center.x << ", " << pl.center.y << ")\n"
              << "  Valid   : " << (pl.valid ? "true" : "false") << "\n"
              << "  Corners : ";
            for (int k = 0; k < 4; ++k) {
                std::cout << "(" << pl.corners[k].x << ", " << pl.corners[k].y << ")";
                if (k < 3) std::cout << " -> ";
            }
            std::cout << "\n  BarIdx  : L=" << pl.barA << " R=" << pl.barB << std::endl;
        }
    }
    SaveImage(barVis, "LampBarCandidates.png");
    SaveImage(vis, "outpost_frame.png");   
    if (debugVis) *debugVis = vis;  
    return result;
}
