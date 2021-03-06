//
//=============================================================================
// MEX compilation example in Linux (with OpenCV 2.4.8):
// mex kaze.cpp -I'../src/lib/' -L'../build/lib/' -I'/usr/local/include/opencv2/'
// -L'/usr/local/lib/' -lKAZE -lopencv_calib3d -lopencv_contrib -lopencv_core
// -lopencv_highgui -lopencv_imgproc -lgomp
//=============================================================================
//
// KAZE features MEX wrapper
// Author: Pablo F. Alcantarilla
// Date: 20/04/2014
// Email: pablofdezalc@gmail.com
//
// KAZE Features Copyright 2012, Pablo F. Alcantarilla
// All Rights Reserved
// See LICENSE for the license information
//=============================================================================
//
// for help type:
// kaze

#include "../src/lib/KAZE.h"
#include "../src/lib/KAZEConfig.h"

// Matlab includes
#include <mex.h>

// System includes
#include <math.h>
#include <matrix.h>

using namespace std;

/* ************************************************************************* */
/**
* @brief This function shows the possible configuration options
*/
void show_input_options_help() {

  mexPrintf("KAZE Features\n");
  mexPrintf("Usage:\n");
  mexPrintf("[kps,desc] = kaze(gray_img, param1, value1, ...)\n\n");
  mexPrintf("Options below are not mandatory. Unless specified, default arguments are used.\n");

  mexPrintf("Scale-space parameters:\n");
  mexPrintf("soffset - Base scale offset [sigma units] (default=1.6)\n");
  mexPrintf("omax - Maximum octave of image evolution (default=4)\n");
  mexPrintf("nsublevels - Number of sublevels per octave (default=4)\n");
  mexPrintf("diffusivity - Diffusivity function. Possible values:\n");
  mexPrintf(" 0 -> Perona-Malik, g1 = exp(-|dL|^2/k^2)\n");
  mexPrintf(" 1 -> Perona-Malik, g2 = 1 / (1 + dL^2 / k^2) (default)\n");
  mexPrintf(" 2 -> Weickert diffusivity\n");
  mexPrintf(" 3 -> Charbonnier diffusivity\n");

  mexPrintf("\nFeature detection parameters:\n");
  mexPrintf("dthreshold - Feature detector threshold response for keypoints (0.001 can be a good value)\n");

  mexPrintf("\nDescriptor parameters:\n");
  mexPrintf("descriptor - Descriptor Type. Possible values:\n");
  mexPrintf(" 0 -> SURF_UPRIGHT\n");
  mexPrintf(" 1 -> SURF\n");
  mexPrintf(" 2 -> SURF_EXTENDED_UPRIGHT,\n");
  mexPrintf(" 3 -> SURF_EXTENDED\n");
  mexPrintf(" 4 -> MSURF_UPRIGHT\n");
  mexPrintf(" 5 -> MSURF (default)\n");
  mexPrintf(" 6 -> MSURF_EXTENDED_UPRIGHT\n");
  mexPrintf(" 7 -> MSURF_EXTENDED\n");
  mexPrintf(" 8 -> GSURF_UPRIGHT,\n");
  mexPrintf(" 9 -> GSURF\n");
  mexPrintf(" 10 -> GSURF_EXTENDED_UPRIGHT\n");
  mexPrintf(" 11 -> GSURF_EXTENDED\n");

  mexPrintf("\nMisc:\n");
  mexPrintf("verbose - Verbose mode. Prints calculation times and stores scale space images in ..\\output\\ folder (if exists)\n\n");
}

/* ************************************************************************* */
/**
* @brief This function parses the parameter arguments for setting KAZE parameters
* @param options Structure that contains KAZE settings
*/
int parse_input_options(KAZEOptions& options, int nrhs, const mxArray *prhs[]) {

  if (nrhs >= 3) {

    for (int i = 1; i < nrhs; i+=2) {
      if (!mxIsChar(prhs[i]) || !mxIsNumeric(prhs[i+1])) {
        mexErrMsgIdAndTxt("kaze:badParamTypes",
                          "Params must be string,value pairs.");
        return 1;
      }

      char *param_name = mxArrayToString(prhs[i]);

      if (!strcmp(param_name, "soffset")) {
        options.soffset = mxGetScalar(prhs[i+1]);
        continue;
      }

      if (!strcmp(param_name, "omax")) {
        options.omax = mxGetScalar(prhs[i+1]);
        continue;
      }

      if (!strcmp(param_name, "dthreshold")) {
        options.dthreshold = mxGetScalar(prhs[i+1]);
        continue;
      }

      if (!strcmp(param_name, "sderivatives")) {
        options.sderivatives = mxGetScalar(prhs[i+1]);
        continue;
      }

      if (!strcmp(param_name, "nsublevels")) {
        options.nsublevels = mxGetScalar(prhs[i+1]);
        continue;
      }

      if (!strcmp(param_name, "diffusivity")) {
        options.diffusivity = (DIFFUSIVITY_TYPE)(mxGetScalar(prhs[i+1]));
        continue;
      }

      if (!strcmp(param_name, "descriptor")) {
        options.descriptor = (DESCRIPTOR_TYPE)(mxGetScalar(prhs[i+1]));
        continue;
        if (options.descriptor < 0 || options.descriptor > GSURF_EXTENDED) {
          options.descriptor = MSURF;
        }
      }

      if (!strcmp(param_name, "verbose")) {
        options.verbosity = mxGetScalar(prhs[i+1]);
        continue;
      }

      if (!strcmp(param_name, "save_scale_space")) {
        options.save_scale_space = mxGetScalar(prhs[i+1]);
        continue;
      }

      mexPrintf("Bad Param name: %s\n",param_name);
      mexErrMsgIdAndTxt("kaze:badParamName",
                        "Bad parameter name.");
      return 1;

    }
  }
  return 0;
}

/* ************************************************************************* */
void mexFunction(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[]){

  // Variables
  KAZEOptions options;

  // Variable for computation times.
  double t1 = 0.0, t2 = 0.0, tcvt = 0.0, tdet = 0.0, tdesc = 0.0;

  if (nrhs == 0) {
    show_input_options_help();
    return;
  }

  if (!mxIsUint8(prhs[0])) {
    mexErrMsgIdAndTxt("kaze:notUint8",
                      "First Input must be a grayscale image of class UINT8.");
  }

  if (nrhs % 2 == 0)
    mexErrMsgIdAndTxt("kaze:badArgNum",
                      "First input must be an image, followed by paramaters name,value pairs.");

  if (nrhs > 1){
    if (parse_input_options(options, nrhs, prhs)) {
      return;
    }
  }

  // Specify image dimensions in KAZE options
  options.img_width = mxGetM(prhs[0]);
  options.img_height = mxGetN(prhs[0]);

  cv::Mat img = cv::Mat(options.img_height, options.img_width, CV_8U, mxGetPr(prhs[0]));
  // OpenCV image is now a transposed image (because it's treated as row-major).

  cv::Mat img_32;
  t1 = cv::getTickCount();
  img.convertTo(img_32, CV_32F, 1.0/255.0, 0); // convert to float for descriptor computations
  t2 = cv::getTickCount();
  tcvt = 1000.0*(t2-t1) / cv::getTickFrequency();

  // Extract features.
  vector<cv::KeyPoint> kpts;
  t1 = cv::getTickCount();
  KAZE evolution(options);
  evolution.Create_Nonlinear_Scale_Space(img_32);
  evolution.Feature_Detection(kpts);
  t2 = cv::getTickCount();
  tdet = 1000.0*(t2-t1) / cv::getTickFrequency();

  if (nlhs > 0) {

    plhs[0] = mxCreateDoubleMatrix(kpts.size(), 2, mxREAL);
    double* pts_ptr = mxGetPr(plhs[0]);
    for (int i = 0 ; i < kpts.size() ; i++) {
      // Swap x,y back to get original coordinates
      pts_ptr[i] = kpts[i].pt.y;
      pts_ptr[kpts.size()+i] = kpts[i].pt.x;
    }
  }

  if (nlhs == 2) {

    // Compute descriptors.
    cv::Mat desc;
    t1 = cv::getTickCount();
    evolution.Compute_Descriptors(kpts, desc);
    t2 = cv::getTickCount();
    tdesc = 1000.0*(t2-t1) / cv::getTickFrequency();

    if (desc.type() == CV_8UC1){
      plhs[1] = mxCreateNumericMatrix(desc.cols, desc.rows, mxUINT8_CLASS, mxREAL);
      // copy descriptors (desc will be freed on function exit)
      unsigned char* desc_ptr = (unsigned char*) mxGetPr(plhs[1]);
      unsigned char* mat_ptr = desc.ptr();
      for (int i = 0 ; i < desc.rows * desc.cols ; i++)
        desc_ptr[i] = mat_ptr[i];
    }
    else if (desc.type() == CV_32FC1){
      plhs[1] = mxCreateNumericMatrix(desc.cols, desc.rows, mxSINGLE_CLASS, mxREAL);
      // copy descriptors (desc will be freed on function exit)
      float* desc_ptr = (float*) mxGetPr(plhs[1]);
      float* mat_ptr = (float*) desc.ptr();
      for (int i = 0 ; i < desc.rows * desc.cols ; i++)
        desc_ptr[i] = mat_ptr[i];
    }
    else{
      mexErrMsgIdAndTxt("kaze:unknownDescType",
                        "Unknown descriptor type.");
      return;
    }

  }
  // Summarize the computation times.
  if (options.verbosity) {
    mexPrintf("Number of points: %d\n", kpts.size());
    mexPrintf("Time Conversion uint8->float: %.2f ms.\n", tcvt);
    mexPrintf("Time Detector: %.2f ms.\n", tdet);

    if (nlhs == 2)
      mexPrintf("Time Descriptor: %.2f ms.\n", tdesc);
  }
}
