#include "stereo-bm.hpp"
#include <iostream>

#include <lcmtypes/bot_core.h>

using namespace cv;
using namespace std;


static const char* FILTER_NAME = "Stereo Block-Matcher";
static const char* PARAM_PRE_FILTER_SIZE = "Pre-Filter Size";
static const char* PARAM_PRE_FILTER_CAP = "Pre-Filter Cap";
static const char* PARAM_CORRELATION_WINDOW_SIZE = "Correlation Window Size";
static const char* PARAM_MIN_DISPARITY = "Min. Disparity";
static const char* PARAM_NO_DISPARITIES = "Num. of Disparities";
static const char* PARAM_DISPARITY_RANGE = "Disparity Range";
static const char* PARAM_UNIQUENESS_RATIO = "Uniqueness Ratio";
static const char* PARAM_TEXTURE_THRESHOLD = "Texture Threshold";
static const char* PARAM_SPECKLE_WINDOW_SIZE = "Speckle Window Size";
static const char* PARAM_SPECKLE_RANGE = "Speckle Range";


struct CameraParams { 
    cv::Mat_<double> K; 
    cv::Mat_<double> D; 

    int width, height;
    float fx, fy, cx, cy, k1, k2, k3, p1, p2;
    CameraParams () {
        width = height = 0;
        fx = fy = cx = cy = 0;
        k1 = k2 = k3 = p1 = p2 = 0;
    }
    cv::Mat_<double> getK() { 
        K = cv::Mat_<double>::zeros(3,3);
        K(0,0) = fx, K(1,1) = fy; 
        K(0,2) = cx, K(1,2) = cy;
        K(2,2) = 1;
        return K;
    }
    cv::Mat_<double> getD() { 
        D = cv::Mat_<double>::zeros(1,5);
        D(0,0) = k1, D(0,1) = k2; 
        D(0,2) = p1, D(0,2) = p2;
        D(0,4) = k3;
        return D;
    }
};
CameraParams left_camera_params, right_camera_params;

/*
struct state_t { 
    lcm_t *lcm;
    GMainLoop *mainloop;
    BotParam   *param;
    BotFrames *frames;
    bot_lcmgl_t *lcmgl;
    // BotGtkParamWidget* pw;
};
state_t* state = NULL;
*/

// Enums
enum { STEREO_BM=0, STEREO_SGBM=1, STEREO_HH=2, STEREO_VAR=3 };

// Parameters
int alg = STEREO_HH;
int SADWindowSize = 0, numberOfDisparities = 0;
bool no_display = false;

cv::StereoBM bm;
cv::StereoSGBM sgbm;
cv::StereoVar var;


// Stereo BM params
// Variable Parameters

bool vDEBUG = false;

int vPRE_FILTER_SIZE = 9; // 5-255
int vPRE_FILTER_CAP = 63; // 1-63
int vCORRELATION_WINDOW_SIZE = 3; // 5-255
int vMIN_DISPARITY = 0; // (-128)-128
// int vNO_DISPARITIES; // 
int vDISPARITY_RANGE = 64; // 32-128
int vUNIQUENESS_RATIO = 15; // 0-100
int vTEXTURE_THRESHOLD = 10; // 0-100
int vSPECKLE_WINDOW_SIZE = 100; // 0-1000
int vSPECKLE_RANGE = 4; // 0-31



StereoB::StereoB(boost::shared_ptr<lcm::LCM> &lcm_, std::string lcm_channel): 
                               lcm_(lcm_){
  botparam_ = bot_param_new_from_server(lcm_->getUnderlyingLCM(), 0);
    vSCALE = .25f;// full scale 1.f; // key scaling parameter

    std::string key_prefix_str = "cameras."+lcm_channel+"_LEFT.intrinsic_cal";
    left_camera_params.width = bot_param_get_int_or_fail(botparam_, (key_prefix_str+".width").c_str());
    left_camera_params.height = bot_param_get_int_or_fail(botparam_,(key_prefix_str+".height").c_str());
    double vals[10];
    bot_param_get_double_array_or_fail(botparam_, (key_prefix_str+".pinhole").c_str(), vals, 5);
    left_camera_params.fx = vals[0];
    left_camera_params.fy = vals[1];
    left_camera_params.cx = vals[3];
    left_camera_params.cy = vals[4];
    if (3 == bot_param_get_double_array(botparam_, (key_prefix_str+".distortion_k").c_str(), vals, 3)) {
      left_camera_params.k1 = vals[0];
      left_camera_params.k2 = vals[1];
      left_camera_params.k3 = vals[2];
    }
    if (2 == bot_param_get_double_array(botparam_, (key_prefix_str+".distortion_p").c_str(), vals, 2)) {
      left_camera_params.p1 = vals[0];
      left_camera_params.p1 = vals[1];
    }

    key_prefix_str = "cameras."+lcm_channel+"_RIGHT.intrinsic_cal";
    right_camera_params.width = bot_param_get_int_or_fail(botparam_, (key_prefix_str+".width").c_str());
    right_camera_params.height = bot_param_get_int_or_fail(botparam_,(key_prefix_str+".height").c_str());
    bot_param_get_double_array_or_fail(botparam_, (key_prefix_str+".pinhole").c_str(), vals, 5);
    right_camera_params.fx = vals[0];
    right_camera_params.fy = vals[1];
    right_camera_params.cx = vals[3];
    right_camera_params.cy = vals[4];
    if (3 == bot_param_get_double_array(botparam_, (key_prefix_str+".distortion_k").c_str(), vals, 3)) {
      right_camera_params.k1 = vals[0];
      right_camera_params.k2 = vals[1];
      right_camera_params.k3 = vals[2];
    }
    if (2 == bot_param_get_double_array(botparam_, (key_prefix_str+".distortion_p").c_str(), vals, 2)) {
      right_camera_params.p1 = vals[0];
      right_camera_params.p1 = vals[1];
    }
}

int  verbose_counter=0;
void StereoB::doStereoB(cv::Mat& left_stereo, cv::Mat& right_stereo){
  _left = left_stereo.clone();

  if( vSCALE != 1.f ) {
      int method = vSCALE < 1 ? cv::INTER_AREA : cv::INTER_CUBIC;
      cv::resize(left_stereo, left_stereo, cv::Size(), vSCALE, vSCALE, method);
  }
  //cv::cvtColor(left_stereo, left_stereo, CV_RGB2BGR);

  if( vSCALE != 1.f ) {
      int method = vSCALE < 1 ? cv::INTER_AREA : cv::INTER_CUBIC;
      cv::resize(right_stereo, right_stereo, cv::Size(), vSCALE, vSCALE, method);
  }
  //cv::cvtColor(right_stereo, right_stereo, CV_RGB2BGR);

  
  if (left_stereo.empty() || right_stereo.empty() || 
      (left_stereo.size() != right_stereo.size()))
      return;

  // std::cerr << "RECEIVED both LEFT & RIGHT" << std::endl;  
  

  // cv::Mat left, right; 
  // cv::cvtColor(left_stereo, left_stereo, CV_BGR2GRAY);
  // cv::cvtColor(right_stereo, right_stereo, CV_BGR2GRAY);  

  cv::Mat_<double> R = cv::Mat_<double>::eye(3,3);
  cv::Mat_<double> t = cv::Mat_<double>::zeros(3,1);
  t(1,0) = .7f; 

  cv::Rect roi_left, roi_right;
  cv::Mat_<double> Rleft, Rright, Pleft, Pright, Q;

  cv::Mat_<double> Mleft = left_camera_params.getK(), Mright = right_camera_params.getK(); 
  cv::Mat_<double> Dleft = left_camera_params.getD(), Dright = right_camera_params.getD(); 
  if( vSCALE != 1.f ) {
      Mleft(0,2) *= vSCALE, Mleft(1,2) *= vSCALE;
      Mright(0,2) *= vSCALE, Mright(1,2) *= vSCALE;
  }
  
  cv::stereoRectify(Mleft, Dleft, Mright, Dright, 
                    left_stereo.size(), R, t, Rleft, Rright, Pleft, Pright, Q, 0, 
                    -1, left_stereo.size(), &roi_left, &roi_right);

  cv::Mat map11, map12, map21, map22;
  initUndistortRectifyMap(Mleft, Dleft, Rleft, Pleft, left_stereo.size(), CV_16SC2, map11, map12);
  initUndistortRectifyMap(Mright, Dright, Rright, Pright, right_stereo.size(), CV_16SC2, map21, map22);

  cv::Mat img1r, img2r;
  remap(left_stereo, img1r, map11, map12, cv::INTER_LINEAR);
  remap(right_stereo, img2r, map21, map22, cv::INTER_LINEAR);

  cv::Mat left = left_stereo.clone();
  cv::Mat right = right_stereo.clone();

  // publish_opencv_image(self->lcm, "CAMERALEFT_RECTIFIED", left, msg->utime);
  // publish_opencv_image(self->lcm, "CAMERARIGHT_RECTIFIED", right, msg->utime);

  numberOfDisparities = numberOfDisparities > 0 ? numberOfDisparities : ((left.size().width/8) + 15) & -16;

  // Block matching
  bm.state->roi1 = roi_left;
  bm.state->roi2 = roi_right;
  bm.state->preFilterCap = vPRE_FILTER_CAP;
  bm.state->SADWindowSize = vCORRELATION_WINDOW_SIZE;
  bm.state->minDisparity = vMIN_DISPARITY;
  bm.state->numberOfDisparities = numberOfDisparities;
  bm.state->textureThreshold = vTEXTURE_THRESHOLD;
  bm.state->uniquenessRatio = vUNIQUENESS_RATIO;
  bm.state->speckleWindowSize = vSPECKLE_WINDOW_SIZE;
  bm.state->speckleRange = vSPECKLE_RANGE;
  bm.state->disp12MaxDiff = 1;

  sgbm.preFilterCap = vPRE_FILTER_CAP; // 63
  sgbm.SADWindowSize = vCORRELATION_WINDOW_SIZE; // 3

  // Channels
  int cn = left.channels();

  sgbm.P1 = 8*cn*sgbm.SADWindowSize*sgbm.SADWindowSize;
  sgbm.P2 = 32*cn*sgbm.SADWindowSize*sgbm.SADWindowSize;
  sgbm.minDisparity = vMIN_DISPARITY;
  sgbm.numberOfDisparities = numberOfDisparities;
  sgbm.uniquenessRatio = vUNIQUENESS_RATIO;
  sgbm.speckleWindowSize = vSPECKLE_WINDOW_SIZE;
  sgbm.speckleRange = vSPECKLE_RANGE;
  sgbm.disp12MaxDiff = 1;
  sgbm.fullDP = alg == STEREO_HH;

  var.levels = 3;                                 // ignored with USE_AUTO_PARAMS
  var.pyrScale = 0.5;                             // ignored with USE_AUTO_PARAMS
  var.nIt = 25;
  var.minDisp = -numberOfDisparities;
  var.maxDisp = 0;
  var.poly_n = 3;
  var.poly_sigma = 0.0;
  var.fi = 15.0f;
  var.lambda = 0.03f;
  var.penalization = var.PENALIZATION_TICHONOV;   // ignored with USE_AUTO_PARAMS
  var.cycle = var.CYCLE_V;                        // ignored with USE_AUTO_PARAMS
  var.flags = var.USE_SMART_ID | var.USE_AUTO_PARAMS | var.USE_INITIAL_DISPARITY | var.USE_MEDIAN_FILTERING ;

  cv::Mat disp8;
  //Mat img1p, img2p, dispp;
  //copyMakeBorder(img1, img1p, 0, 0, numberOfDisparities, 0, IPL_BORDER_REPLICATE);
  //copyMakeBorder(img2, img2p, 0, 0, numberOfDisparities, 0, IPL_BORDER_REPLICATE);
  
  int64 tm = cv::getTickCount();
  if( alg == STEREO_BM )
      bm(left, right, _disp);
  else if( alg == STEREO_VAR ) 
      var(left, right, _disp);
  else if( alg == STEREO_SGBM || alg == STEREO_HH )
      sgbm(left, right, _disp);
  tm = cv::getTickCount() - tm;
  
  verbose_counter++;
  if (verbose_counter%10 ==0){ // give ouput every X frames
    printf("Frames %d : Time elapsed: %f ms\n", verbose_counter, tm*1000/cv::getTickFrequency());
  }

  if( vSCALE != 1.f ) {
      int method =vSCALE < 1 ? cv::INTER_AREA : cv::INTER_CUBIC;
      cv::resize(_disp, _disp, cv::Size(), 1.f/vSCALE, 1.f/vSCALE, method);
      _disp =  _disp/vSCALE ;
  }

  //disp = dispp.colRange(numberOfDisparities, img1p.cols);
  if( alg != STEREO_VAR )
      _disp.convertTo(disp8, CV_8U, 255/(numberOfDisparities*16.));
  else
      _disp.convertTo(disp8, CV_8U);

  if (_disp.empty())
      return;
  
  //if (vDEBUG)
  //    publish_opencv_image(self->lcm, "CAMERADEPTH8", disp8, msg->utime);
}



// Publish both left and disparity in one image type:
void StereoB::sendRangeImage(int64_t utime) { 
  int h = _disp.rows;
  int w = _disp.cols;
  
  //////// RIGHT IMAGE /////////////////////////////////////////////  
  int n_bytes=2; // was 4 bytes per value
  int isize = n_bytes*w*h;
  bot_core_image_t disp_msg_out;
  disp_msg_out.utime = utime; 
  disp_msg_out.width = w; 
  disp_msg_out.height = h;
  disp_msg_out.pixelformat = BOT_CORE_IMAGE_T_PIXEL_FORMAT_FLOAT_GRAY32;
  disp_msg_out.row_stride = n_bytes*w;
  disp_msg_out.size = isize;
  disp_msg_out.data = (uint8_t*) _disp.data;
  disp_msg_out.nmetadata = 0;
  disp_msg_out.metadata = NULL;


  //////// LEFT IMAGE //////////////////////////////////////////////
  bot_core_image_t left_msg_out;
  left_msg_out.utime = utime; 
  left_msg_out.width = w;
  left_msg_out.height =h;
  left_msg_out.row_stride = w; 
  left_msg_out.pixelformat = BOT_CORE_IMAGE_T_PIXEL_FORMAT_GRAY;
  left_msg_out.size = w*h;
  left_msg_out.data = (uint8_t*) _left.data;
  left_msg_out.nmetadata = 0;
  left_msg_out.metadata = NULL;

  //////// BOTH IMAGES /////////////////////////////////////////////
  bot_core_images_t images;
  images.utime = utime;
  images.n_images=2;
  int16_t im_types[ 2 ];
  im_types[0] = BOT_CORE_IMAGES_T_LEFT;
  im_types[1] = BOT_CORE_IMAGES_T_DISPARITY;
  bot_core_image_t im_list[ 2 ];
  im_list[0] = left_msg_out;
  im_list[1] = disp_msg_out;
  images.image_types = im_types;
  images.images = im_list;
  bot_core_images_t_publish( lcm_->getUnderlyingLCM(), "MULTISENSE_LD", &images);        
  return;
}
