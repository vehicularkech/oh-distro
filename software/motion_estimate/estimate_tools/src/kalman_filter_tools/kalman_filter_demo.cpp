// Test Program for Kalman Filter:

#include <boost/shared_ptr.hpp>
#include <boost/assign/std/vector.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/foreach.hpp>
#include <lcm/lcm-cpp.hpp>
#include <iostream>
#include <fstream>
#include <string>
#include <sys/time.h>

#include <ConciseArgs>

#include <lcmtypes/drc/robot_state_t.hpp>

#include <estimate_tools/kalman_filter.hpp>
#include <estimate_tools/simple_kalman_filter.hpp>

#include <Eigen/Core>

using namespace std;
using namespace boost::assign; // bring 'operator+()' into scope
using namespace boost;
using namespace Eigen;


struct CommandLineConfig
{
    int mode;
};

class App{
  public:
    App(boost::shared_ptr<lcm::LCM> &lcm_, const CommandLineConfig& cl_cfg_);
    
    ~App(){
    }    
    
    void readFile();
    
  private:
    boost::shared_ptr<lcm::LCM> lcm_;
    
    void ersHandler(const lcm::ReceiveBuffer* rbuf, 
                           const std::string& channel, const  drc::robot_state_t* msg); 
    void doFilter(double t, Eigen::VectorXf x, Eigen::VectorXf x_dot);
    const CommandLineConfig cl_cfg_;  
    
    EstimateTools::KalmanFilter* kf;
    
    std::vector<EstimateTools::KalmanFilter*> joint_kf_;
    
    std::vector<EstimateTools::SimpleKalmanFilter*> joint_skf_;
    std::vector<int> filter_idx_;
    
    int64_t dtime_running_;
    int count_running_;
};


App::App(boost::shared_ptr<lcm::LCM> &lcm_, const CommandLineConfig& cl_cfg_):
    lcm_(lcm_), cl_cfg_(cl_cfg_){
      
      std::cout << "sub\n";
      lcm_->subscribe( "EST_ROBOT_STATE" ,&App::ersHandler,this);
    //lcm::Subscription* sub = 
   // sub->setQueueCapacity(1);  
  
      
  
  // all of atlas:
  filter_idx_ = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27};
  // legs
  //filter_idx_ = {4,5,6,7,8,9,10,11,12,13,14,15};
  // std::cout << "filter_idx_ " << filter_idx_.size() << "\n";

  if (cl_cfg_.mode == 0){ // this mode is broken
    kf = new EstimateTools::KalmanFilter( 41 );
  
  }else if (cl_cfg_.mode == 1){
  
    for (size_t i=0;i < filter_idx_.size(); i++){
      EstimateTools::KalmanFilter* a_kf = new EstimateTools::KalmanFilter (1, 0.01, 5E-4);
      joint_kf_.push_back(a_kf) ;
    }
    std::cout << "Created " << joint_kf_.size() << " Kalman Filters\n";

  } else if (cl_cfg_.mode==2) {
  
    for (size_t i=0;i < filter_idx_.size(); i++){
      EstimateTools::SimpleKalmanFilter* a_kf = new EstimateTools::SimpleKalmanFilter (0.01, 5E-4);
      joint_skf_.push_back(a_kf) ;
    }
    std::cout << "Created " << joint_skf_.size() << " Simple Kalman Filters\n";
    
  }
  
  
  
  dtime_running_ =0;
  count_running_ = 0;
}



// same as bot_timestamp_now():
int64_t _timestamp_now(){
    struct timeval tv;
    gettimeofday (&tv, NULL);
    return (int64_t) tv.tv_sec * 1000000 + tv.tv_usec;
}

void App::doFilter(double t, Eigen::VectorXf x, Eigen::VectorXf x_dot){
  /*
  std::stringstream ss;  
  ss << std::fixed << t ;
  
  std::cout << ss.str() << "\n";
  std::cout << x.transpose() << "\n";
  */
  
  
  Eigen::VectorXf x_filtered = Eigen::VectorXf (41);
  Eigen::VectorXf x_dot_filtered = Eigen::VectorXf (41);
  int64_t tic = _timestamp_now();
  kf->processSample(t,x, x_dot, x_filtered, x_dot_filtered);
  int64_t toc = _timestamp_now();
  double dtime = (toc-tic)*1E-6;
  std::cout << dtime << "\n";
  
}

void App::ersHandler(const lcm::ReceiveBuffer* rbuf, const std::string& channel, const  drc::robot_state_t* msg){
  int64_t tic = _timestamp_now();
  
  
  std::vector<float> jp;
  jp = msg->joint_position;
  std::vector<float> jv;
  jv = msg->joint_velocity;
  
  std::vector<float> jp_out;
  std::vector<float> jv_out;
  jp_out.assign(41,0);
  jv_out.assign(41,0);
  
  
  double t = (double) msg->utime*1E-6;

  
  if ( cl_cfg_.mode==0 ){ // Single NxN array of filters
    Eigen::Map<Eigen::VectorXf>  x(   jp.data() ,  msg->joint_position.size());
    Eigen::Map<Eigen::VectorXf>  x_dot(  jv.data() ,  msg->joint_velocity.size());
    Eigen::VectorXf  x_D = x;
    Eigen::VectorXf  x_dot_D = x_dot;
    
    Eigen::VectorXf x_filtered = Eigen::VectorXf ( msg->joint_position.size());
    Eigen::VectorXf x_dot_filtered = Eigen::VectorXf ( msg->joint_velocity.size());
    
    kf->processSample(t,x_D, x_dot_D, x_filtered, x_dot_filtered);
    Map<VectorXf>( jp_out.data(), msg->joint_position.size()) = x_filtered;
    Map<VectorXf>( jv_out.data(), msg->joint_velocity.size()) = x_dot_filtered;
  }else if(  cl_cfg_.mode==1 ){ // Single N 1x1 array of filters
  
    Eigen::VectorXf  x_D = Eigen::VectorXf(1);
    Eigen::VectorXf  x_dot_D = Eigen::VectorXf(1);
    Eigen::VectorXf x_filtered = Eigen::VectorXf ( 1);
    Eigen::VectorXf x_dot_filtered = Eigen::VectorXf ( 1);
    
    for (size_t i=0; i <  filter_idx_.size(); i++){
      x_D(0) = jp[  filter_idx_[i] ];
      x_dot_D(0) = jv_out[ filter_idx_[i] ];
      joint_kf_[i]->processSample(t,x_D, x_dot_D, x_filtered, x_dot_filtered);
      jp_out[ filter_idx_[i] ] = x_filtered(0);
      jv_out[ filter_idx_[i] ] = x_dot_filtered(0);
    }

  }else if (cl_cfg_.mode==2){
  
    for (size_t i=0; i <  filter_idx_.size(); i++){
      
      double x_filtered;
      double x_dot_filtered;
      joint_skf_[i]->processSample(t,  jp[filter_idx_[i]] , jv_out[filter_idx_[i]] , x_filtered, x_dot_filtered);
      jp_out[ filter_idx_[i] ] = x_filtered;
      jv_out[ filter_idx_[i] ] = x_dot_filtered;
    }

  }

  
  
  
  
  drc::robot_state_t msg_out = *msg; 
  msg_out.joint_position = jp_out; 
  msg_out.joint_velocity = jv_out;
 
  lcm_->publish( "TRUE_ROBOT_STATE" , &msg_out);     

  

  int64_t toc = _timestamp_now();
  int64_t dtime = (toc-tic);
  //std::cout << dtime << " end\n"; 
  
  dtime_running_ +=  dtime;
  count_running_ ++;
  
  if (count_running_ % 1000 ==0){
    double dtime_mean =  dtime_running_*1E-6/count_running_;
    std::cout << dtime_mean << " mean of " << count_running_ << "\n";
  }
  
  
}





void App::readFile(){
  
  ifstream fileinput ( "/home/mfallon/Desktop/exp.txt");
  if (fileinput.is_open()){
    string message;
    while ( getline (fileinput,message) ) { // for each line
      //cout << message << endl;

      vector<string> tokens;
      boost::split(tokens, message, boost::is_any_of(";"));
      vector<double> values;
      BOOST_FOREACH(string t, tokens){
        values.push_back(lexical_cast<double>(t));
      }
      
      double t = values[0];
      vector<float> state_vector;
      for (size_t i=1; i < values.size() ; i++){
        state_vector.push_back( values[i]);
      }

      
      vector<float> vec_x;
      vector<float> vec_x_dot;
      for (size_t i=0; i < 34 ; i++){
        vec_x.push_back( state_vector[i]);
        vec_x_dot.push_back( state_vector[i+34]);
      }
      
      
      //Eigen::VectorXf x(x_vector.data());
      
      Eigen::Map<Eigen::VectorXf>  x(  vec_x.data() ,  vec_x.size());
      Eigen::Map<Eigen::VectorXf>  x_dot(  vec_x_dot.data() ,  vec_x_dot.size());
      doFilter(t,x,x_dot);

    }
  }
  fileinput.close();        
        
        
        
}


int main(int argc, char ** argv) {
  CommandLineConfig cl_cfg;
  cl_cfg.mode = 0; 
  ConciseArgs opt(argc, (char**)argv);
  opt.add(cl_cfg.mode, "f", "mode","Filter Type");
  opt.parse();  
  
  boost::shared_ptr<lcm::LCM> lcm(new lcm::LCM);
  if(!lcm->good()){
    std::cerr <<"ERROR: lcm is not good()" <<std::endl;
  }
  
  App app(lcm, cl_cfg);
//  if (cl_cfg.mode ==1){
//    app.readFile();
//  }else{
    cout << "Tool ready1" << endl << "============================" << endl;
    while(0 == lcm->handle());
//  }
  return 0;
}