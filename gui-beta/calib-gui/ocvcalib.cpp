#include <ocvcalib.h>

#include <utility>

/*	Sources:
    https://www.youtube.com/watch?v=v7jutAmWJVQ
    https://www.youtube.com/watch?v=GYIQiV9Aw74
    https://docs.opencv.org/3.4.3/d4/d94/tutorial_camera_calibration.html

    calibration chessboard generator:
    https://calib.io/pages/camera-calibration-pattern-generator
*/
CalibParams camera;
/* Calib init parameters */
string pathToCalibPics;     // Path to pictures
int chessboardWidth;        // Width of calib chessboard as number of squares
int chessboardHeight;       // Height of calib chessboard as number of squares
unsigned int minAmountOfPicsToCalibrate = 20;   // Minimal amount of pics needed to start the calibration
float calibrationSquareSize;// Size of square, in milimeters

Size chessboardDimensions;  // Size of chessboard = square intersections in each axis

/* Camera parameters */
Point2d pixelSize;          // Size of pixel in x and y axis in mm
Point matrixMaxRes;         // Max resolution of the camera
Point matrixCurrRes;        // Current set resolution of the camera
Point2d matrixSize;         // Size of the camera sensor in x and y axis in mm

double focalLength;// =6.0; // Focal length in mm

//double fx = (double)matrixCurrRes.x * focalLength / matrixSize.x;
//double fy = (double)matrixCurrRes.y * focalLength / matrixSize.y;
//double cx = matrixCurrRes.x / 2;
//double cy = matrixCurrRes.y / 2;

double fx;
double fy;
double cx;
double cy;

// For computing reprojection error
double reprojectionError = 20.5;
vector<float> perViewErrors;


/* Beginning of the program */
void createKnownChessboardPosition(Size boardSize, float squareEdgeLength,
  vector<Point3f>& corners)
{
    for (int i = 0; i < boardSize.height; i++) {
        for (int j = 0; j < boardSize.width; j++) {
            corners.emplace_back(j * squareEdgeLength,
              i * squareEdgeLength, 0.0F);
        }
    }
}

void getChessboardCorners(vector<Mat> images,
        vector< vector< Point2f > > &allFoundCorners, CalibParams camera,
        bool showResults = false)
{
    Size chessboardDimensions = Size(camera.chessboardWidth - 1,
                                     camera.chessboardHeight - 1);
//                cv::imshow("test", images[images.size()-1]);
    for (auto & image : images) {
        vector<Point2f> pointBuf;
        bool found = findChessboardCorners(image, chessboardDimensions,
          pointBuf, cv::CALIB_CB_ADAPTIVE_THRESH | cv::CALIB_CB_NORMALIZE_IMAGE);

        if (found) { allFoundCorners.push_back(pointBuf); }
//        FileStorage outStream("test_2.yml", FileStorage::WRITE);
//        outStream       << "pointBuf" << pointBuf;
//        outStream.release();

        if (showResults) {
            drawChessboardCorners(image, chessboardDimensions,
                                    pointBuf, found);
;
            cv::imshow("Looking for corners", image);
            cv::waitKey(0);
        }
    }
}

void cameraCalibration(vector<Mat> calibrationImages, Size boardSize,
  float squareEdgeLength, Mat &cameraMatrix, Mat &distanceCoefficients,
  vector<Mat> &rVectors, vector<Mat> &tVectors, CalibParams camera)
{
    vector< vector< Point2f > > checkerboardImageSpacePoints;
    //  Searching chessboard corners in progress...
    getChessboardCorners(std::move(calibrationImages),
      checkerboardImageSpacePoints, std::move(camera), false);

    vector< vector< Point3f > > worldSpaceCornerPoints(1);
    //  Creating known chessboard positions...
    createKnownChessboardPosition(boardSize, squareEdgeLength,
      worldSpaceCornerPoints[0]);
    //  Creating vector of size of chessboard corners amount...
    worldSpaceCornerPoints.resize(checkerboardImageSpacePoints.size(),
      worldSpaceCornerPoints[0]);

    distanceCoefficients = Mat::zeros(8, 1, CV_64F);
//    cout << "Camera calibration started." << endl;
//    cout << cameraMatrix;

//    FileStorage outStream("test.yml", FileStorage::WRITE);
//    outStream       << "worldSpaceCornerPoints" << worldSpaceCornerPoints;
//    outStream       << "checkerboardImageSpacePoints" << checkerboardImageSpacePoints;
//    outStream       << "boardSize" << boardSize;
//    outStream       << "cameraMatrix" << cameraMatrix;
//    outStream       << "distanceCoefficients" << distanceCoefficients;
//    outStream       << "rVectors" << rVectors;
//    outStream       << "tVectors" << tVectors;
//    outStream.release();

    calibrateCamera(worldSpaceCornerPoints, checkerboardImageSpacePoints,
      boardSize, cameraMatrix, distanceCoefficients, rVectors, tVectors);

    reprojectionError = computeReprojectionErrors(
                worldSpaceCornerPoints, checkerboardImageSpacePoints,
                rVectors, tVectors, cameraMatrix, distanceCoefficients,
                perViewErrors);

    FileStorage outStream("Data for homography.xml", FileStorage::WRITE);
    outStream       << "objectPoints" << worldSpaceCornerPoints;
    outStream       << "imagePoints" << checkerboardImageSpacePoints;
    outStream.release();
    //  Camera calibration finished.
}

bool saveCameraCalibration(string name, string nameCalibPic, Mat cameraMatrix,
  Mat distanceCoefficients, vector<Mat> rVectors, vector<Mat> tVectors)
{
    //Save calib parameters to .xml
    FileStorage outStream(name, FileStorage::WRITE);
    FileStorage outStreamPic(nameCalibPic, FileStorage::APPEND);
    outStream       << "cameraMatrix" << cameraMatrix;
    outStream       << "distanceCoefficients" << distanceCoefficients;
    outStreamPic    << "rVectors" << rVectors;
    outStreamPic    << "tVectors" << tVectors;
    outStream.release();
    outStreamPic.release();
    return true;
}

/* Error reprojection estimation - https://docs.opencv.org/3.1.0/d4/d94/tutorial_camera_calibration.html */
double computeReprojectionErrors( const vector<vector<Point3f> >& objectPoints,
                                         const vector<vector<Point2f> >& imagePoints,
                                         const vector<Mat>& rvecs, const vector<Mat>& tvecs,
                                         const Mat& cameraMatrix , const Mat& distCoeffs,
                                         vector<float>& perViewErrors)
{
    vector<Point2f> imagePoints2;
    size_t totalPoints = 0;
    double totalErr = 0;
    double err;
    perViewErrors.resize(objectPoints.size());
    for(size_t i = 0; i < objectPoints.size(); ++i )
    {
        cv::projectPoints(objectPoints[i], rvecs[i], tvecs[i], cameraMatrix,
                            distCoeffs, imagePoints2);
        err = norm(imagePoints[i], imagePoints2, NORM_L2);
        size_t n = objectPoints[i].size();
        perViewErrors[i] = (float) std::sqrt(err*err/n);
        totalErr        += err*err;
        totalPoints     += n;
    }
    return std::sqrt(totalErr/totalPoints);
}


string createJpgFile(int &savedImageCount) {
    std::string filename = pathToCalibPics;
    filename += "calib_pic_";
    if (savedImageCount < 100) { filename += "0";
}
    if (savedImageCount < 10) { filename += "0";
}
    filename += patch::to_string(savedImageCount);
    filename += ".png";
    savedImageCount++;
    return filename;
}

inline bool exists_file(const std::string &name) {
    struct stat buffer;
    return (stat(name.c_str(), &buffer) == 0);
}

/*
Creates matrix of camera parameters which looks like this:
|	fx		0		0	|
|	0		fy		0	|
|	cx		cy		1	|
and saves all camera parameters to camera_intrinsic.xml
*/
void setCameraParameters(CalibParams camera) {
    fx = (double)camera.matrixCurrRes.x * camera.focalLength
            / camera.matrixSize.x;
    fy = (double)camera.matrixCurrRes.y * camera.focalLength
            / camera.matrixSize.y;
    cx = camera.matrixCurrRes.x / 2;
    cy = camera.matrixCurrRes.y / 2;
}

cv::Mat getCameraMatrix(CalibParams camera) {
    cv::Mat cameraMatrix = Mat::eye(3, 3, CV_64F);

    setCameraParameters(std::move(camera));

    cameraMatrix.at<double>(0, 0)	= fx;
    cameraMatrix.at<double>(1, 0)	= 0;
    cameraMatrix.at<double>(2, 0)	= 0;
    cameraMatrix.at<double>(0, 1)	= 0;
    cameraMatrix.at<double>(1, 1)	= fy;
    cameraMatrix.at<double>(2, 1)	= 0;
    cameraMatrix.at<double>(0, 2)	= cx;
    cameraMatrix.at<double>(1, 2)	= cy;
    cameraMatrix.at<double>(2, 2)	= 1;

    return cameraMatrix;
}

void saveIntrinsicCameraParameters(cv::Mat &cameraMatrix, CalibParams camera) {
    cameraMatrix = getCameraMatrix(std::move(camera));

    FileStorage outCamStream("camera_intrinsic.xml", FileStorage::WRITE);
    outCamStream << "header" 		<< "Raspberry Pi Camera 2.0 B";
    outCamStream << "pixelSize" 	<< pixelSize;
    outCamStream << "matrixMaxRes" 	<< matrixMaxRes;
    outCamStream << "matrixCurrRes"	<< matrixCurrRes;
    outCamStream << "matrixSize" 	<< matrixSize;
    outCamStream << "focalLength" 	<< focalLength;
    outCamStream << "cameraMatrix" 	<< cameraMatrix;
    outCamStream.release();

}


/*
Method to parse parameters from command line or xml file to a program.
It can also write loaded parameters to another xml file.
*/
void inline parseParameters(int argc, char** argv, cv::String &keys, CalibParams camera) {
    CommandLineParser parser(argc, argv, keys);
    if (parser.has("help"))
    {
        parser.printMessage();
        //return 0;
    }

    if (parser.has("terminal") && !parser.has("loadconf")) {
        /*   Calib parameters   */
        camera.pathToCalibPics 		= parser.get<string>("path");
        cout << "Current path: " << camera.pathToCalibPics << std::endl;
        camera.calibrationSquareSize    = parser.get<float>("squaresize");;
        //corners in x or y = amount of squares minus 1
        camera.chessboardWidth  = parser.get<int>("w") - 1;
        camera.chessboardHeight = parser.get<int>("h") - 1;

        /*   Camera parameters   */
        camera.pixelSize   	= Point2d(parser.get<double>("px"),
          parser.get<double>("py"));
        camera.matrixMaxRes  	= Point(parser.get<int>("maxresx"),
          parser.get<int>("maxresy"));
        camera.matrixCurrRes 	= Point(parser.get<int>("currresx"),
          parser.get<int>("currresy"));
        camera.matrixSize  	= Point2d((double)matrixMaxRes.x * pixelSize.x,
          (double)matrixMaxRes.y * camera.pixelSize.y);
        camera.focalLength 	= parser.get<double>("focal");
    }

    if (parser.has("terminal") && parser.has("loadconf")) {
        loadParametersFromXml("calib_conf.xml", camera);
    }

    if (parser.has("terminal") && parser.has("createconf")) {
        saveParametersToXml("calib_conf.xml", camera, "");
    }
    chessboardDimensions = Size(camera.chessboardWidth,
                                camera.chessboardHeight);

    setCameraParameters(camera);
}


void loadParametersFromXml(cv::String filename, CalibParams &camera) {
    cv::FileStorage readParams(filename, FileStorage::READ);
    readParams["header"]                >>  camera.header;
    readParams["pathToCalibPics"] 		>>	camera.pathToCalibPics;
    readParams["calibrationSquareSize"] >>	camera.calibrationSquareSize;
    readParams["chessboardWidth"] 		>>	camera.chessboardWidth;
    readParams["chessboardHeight"] 		>>	camera.chessboardHeight;
    readParams["pixelSize"] 			>>	camera.pixelSize;
    readParams["matrixMaxRes"] 			>>	camera.matrixMaxRes;
    readParams["matrixCurrRes"] 		>>	camera.matrixCurrRes;
    readParams["matrixSize"] 			>>	camera.matrixSize;
    readParams["focalLength"] 			>>	camera.focalLength;
    readParams.release();
}

void saveParametersToXml(cv::String filename, CalibParams camera,
                                cv::String header) {
    cv::FileStorage outCalibStream(filename, FileStorage::WRITE);
    outCalibStream << "header"                  << header;//camera.header;
    outCalibStream << "pathToCalibPics"			<< camera.pathToCalibPics;
    outCalibStream << "calibrationSquareSize"	<< camera.calibrationSquareSize;
    outCalibStream << "chessboardWidth"			<< camera.chessboardWidth;
    outCalibStream << "chessboardHeight"		<< camera.chessboardHeight;
    outCalibStream << "pixelSize"				<< camera.pixelSize;
    outCalibStream << "matrixMaxRes"			<< camera.matrixMaxRes;
    outCalibStream << "matrixCurrRes"			<< camera.matrixCurrRes;
    outCalibStream << "matrixSize"				<< camera.matrixSize;
    outCalibStream << "focalLength"				<< camera.focalLength;
    outCalibStream.release();
}

double getReprojectionError() {
    return reprojectionError;
};

vector<float> getPerViewErrors() {
    return perViewErrors;
}

void clearPerViewErrors() {
    perViewErrors.clear();
}
