#include <opencv2/opencv.hpp>

using namespace cv;

int main()
{
	// 设置窗口
    // 注意opencv的坐标系原点在左上角
	Mat img = Mat::zeros(Size(800, 800), CV_8UC3);
	img.setTo(255);              // 设置屏幕为白色

	Point p1(100, 100);          // 点p1
	Point p2(758, 50);           // 点p2

	// 画直线函数
	line(img, p1, p2, Scalar(0, 0, 255), 1);   // 红色
	line(img, Point(300, 300), Point(758, 400), Scalar(0, 255, 255), 1); // 黄色

    // 画点 p1
	circle(img, p1, 3, Scalar(0, 255, 0), -1);
    // 画点 p2
	circle(img, p2, 3, Scalar(120, 120, 120), -1);

	imshow("画板", img);
	waitKey(0);
	return 0;
}