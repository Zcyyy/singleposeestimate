#include <iostream>
#include <opencv2/core/core.hpp>
#include <opencv2/features2d/features2d.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/calib3d/calib3d.hpp>
using namespace std;
using namespace cv;

void find_feature_matches (
    const Mat& img_1, const Mat& img_2,
    std::vector<KeyPoint>& keypoints_1,
    std::vector<KeyPoint>& keypoints_2,
    std::vector< DMatch >& matches );

void pose_estimation_2d2d (
    std::vector<KeyPoint> keypoints_1,
    std::vector<KeyPoint> keypoints_2,
    std::vector< DMatch > matches,
    Mat& R, Mat& t );

Point2d pixel2cam ( const Point2d& p, const Mat& K );

int main ( int argc, char** argv )
{
    Mat img_1;
    Mat img_2;
    VideoCapture cap(2);
    int c;
    vector<KeyPoint> keypoints_1, keypoints_2;
    vector<DMatch> matches;
    while(1)
    {
	matches.clear();
	if(img_1.empty())
	{
		cap >> img_1;
		continue;
	}
	else
	{
		img_2 = img_1;
		cap >> img_1;
	}
    	find_feature_matches ( img_1, img_2, keypoints_1, keypoints_2, matches );
    	cout<<"一共找到了"<<matches.size() <<"组匹配点"<<endl;
	if(matches.size() < 10 )
	{
		img_2 = img_1;
		continue;
	}
    	Mat R,t;
    	pose_estimation_2d2d ( keypoints_1, keypoints_2, matches, R, t );

    	Mat t_x = ( Mat_<double> ( 3,3 ) <<
                	0,                      -t.at<double> ( 2,0 ),     t.at<double> ( 1,0 ),
                	t.at<double> ( 2,0 ),      0,                      -t.at<double> ( 0,0 ),
        	        -t.at<double> ( 1,0 ),     t.at<double> ( 0,0 ),      0 );

    	cout<<"t^R="<<endl<<t_x*R<<endl;

    	Mat K = ( Mat_<double> ( 3,3 ) <<  4.7752853359299678e+02, 0, 320, 0, 4.7752853359299678e+02, 240, 0, 0,1 );
    	for ( DMatch m: matches )
    	{
        	Point2d pt1 = pixel2cam ( keypoints_1[ m.queryIdx ].pt, K );
        	Mat y1 = ( Mat_<double> ( 3,1 ) << pt1.x, pt1.y, 1 );
        	Point2d pt2 = pixel2cam ( keypoints_2[ m.trainIdx ].pt, K );
        	Mat y2 = ( Mat_<double> ( 3,1 ) << pt2.x, pt2.y, 1 );
        	Mat d = y2.t() * t_x * R * y1;
        	cout << "epipolar constraint = " << d << endl;
    	}
	c = waitKey(30);
	if( c == 'q' )
	{
		break;
	}
    }
    return 0;
}

void find_feature_matches ( const Mat& img_1, const Mat& img_2,
                            std::vector<KeyPoint>& keypoints_1,
                            std::vector<KeyPoint>& keypoints_2,
                            std::vector< DMatch >& matches )
{
    Mat img_match;
    Mat descriptors_1, descriptors_2;
    Ptr<FeatureDetector> detector = ORB::create();
    Ptr<DescriptorExtractor> descriptor = ORB::create();
    Ptr<DescriptorMatcher> matcher  = DescriptorMatcher::create ( "BruteForce-Hamming" );
    detector->detect ( img_1,keypoints_1 );
    detector->detect ( img_2,keypoints_2 );

    descriptor->compute ( img_1, keypoints_1, descriptors_1 );
    descriptor->compute ( img_2, keypoints_2, descriptors_2 );

    //-- 第三步:对两幅图像中的BRIEF描述子进行匹配，使用 Hamming 距离
    vector<DMatch> match;
    //BFMatcher matcher ( NORM_HAMMING );
    matcher->match ( descriptors_1, descriptors_2, match );

    //-- 第四步:匹配点对筛选
    double min_dist=10000, max_dist=0;

    //找出所有匹配之间的最小距离和最大距离, 即是最相似的和最不相似的两组点之间的距离
    for ( int i = 0; i < descriptors_1.rows; i++ )
    {
        double dist = match[i].distance;
        if ( dist < min_dist ) min_dist = dist;
        if ( dist > max_dist ) max_dist = dist;
    }

    printf ( "-- Max dist : %f \n", max_dist );
    printf ( "-- Min dist : %f \n", min_dist );

    //当描述子之间的距离大于两倍的最小距离时,即认为匹配有误.但有时候最小距离会非常小,设置一个经验值30作为下限.
    for ( int i = 0; i < descriptors_1.rows; i++ )
    {
        if ( match[i].distance <= max ( 2*min_dist, 30.0 ) )
        {
            matches.push_back ( match[i] );
        }
    }
    drawMatches( img_1, keypoints_1, img_2, keypoints_2, matches, img_match );
    imshow("all matches",img_match);
}


Point2d pixel2cam ( const Point2d& p, const Mat& K )
{
    return Point2d
           (
               ( p.x - K.at<double> ( 0,2 ) ) / K.at<double> ( 0,0 ),
               ( p.y - K.at<double> ( 1,2 ) ) / K.at<double> ( 1,1 )
           );
}


void pose_estimation_2d2d ( std::vector<KeyPoint> keypoints_1,
                            std::vector<KeyPoint> keypoints_2,
                            std::vector< DMatch > matches,
                            Mat& R, Mat& t )
{
    // 相机内参,TUM Freiburg2
    Mat K = ( Mat_<double> ( 3,3 ) << 4.7752853359299678e+02, 0, 320, 0, 4.7752853359299678e+02, 240, 0, 0,1 );

    //-- 把匹配点转换为vector<Point2f>的形式
    vector<Point2f> points1;
    vector<Point2f> points2;

    for ( int i = 0; i < ( int ) matches.size(); i++ )
    {
        points1.push_back ( keypoints_1[matches[i].queryIdx].pt );
        points2.push_back ( keypoints_2[matches[i].trainIdx].pt );
    }

    //-- 计算基础矩阵
    Mat fundamental_matrix;
    fundamental_matrix = findFundamentalMat ( points1, points2, CV_FM_8POINT );
    cout<<"fundamental_matrix is "<<endl<< fundamental_matrix<<endl;

    //-- 计算本质矩阵
    Mat essential_matrix;
    essential_matrix = findEssentialMat ( points1, points2, K  );
    cout<<"essential_matrix is "<<endl<< essential_matrix<<endl;

    //-- 计算单应矩阵
    Mat homography_matrix;
    homography_matrix = findHomography ( points1, points2, RANSAC, 3 );
    cout<<"homography_matrix is "<<endl<<homography_matrix<<endl;

    //-- 从本质矩阵中恢复旋转和平移信息.
    recoverPose ( essential_matrix, points1, points2, K, R, t );
    cout<<"R is "<<endl<<R<<endl;
    cout<<"t is "<<endl<<t<<endl;
    
}
