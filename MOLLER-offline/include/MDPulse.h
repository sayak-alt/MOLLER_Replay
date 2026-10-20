#ifndef MD_PULSE_H
#define MD_PULSE_H
#include <vector>
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

struct MDPulseConfig {
 int baseline_samples=10,nsb=5,nsa=22;
 double dt_ns=4,gate_start_ns=-170,range_v=1,resistance_ohm=50;
 int bits=12;
 double threshold_adc=20.48,threshold_sigma=5;
 int integration_reference=0; // 0: peak (legacy), 1: first above-threshold sample
 int timing_reference=0;      // time_ns: 0 peak, 1 interpolated threshold, 2 half height
 int threshold_samples=1;     // Actual number of consecutive samples; not a register code
 void Validate()const {
  if(baseline_samples<2||nsb<0||nsb>100000||nsa<1||nsa>100000||bits<1||bits>16||
   integration_reference<0||integration_reference>1||timing_reference<0||timing_reference>2||
   threshold_samples<1||threshold_samples>100000||
   !std::isfinite(dt_ns)||dt_ns<=0||!std::isfinite(gate_start_ns)||
   !std::isfinite(range_v)||range_v<=0||!std::isfinite(resistance_ohm)||resistance_ohm<=0||
   !std::isfinite(threshold_adc)||threshold_adc<0||!std::isfinite(threshold_sigma)||threshold_sigma<0)
   throw std::runtime_error("Invalid MD replay configuration");
 }
};
struct MDPulseResult {
 double baseline=0,noise=0,height=0,integral=0,charge_pc=0;
 double time_ns=std::numeric_limits<double>::quiet_NaN();
 double peak_time_ns=std::numeric_limits<double>::quiet_NaN();
 double threshold_time_ns=std::numeric_limits<double>::quiet_NaN();
 double half_height_time_ns=std::numeric_limits<double>::quiet_NaN();
 double applied_threshold_adc=0;
 int found=0,peak=-1,first=-1,last=-1,clipped=0,saturated=0;
 int threshold_sample=-1,threshold_time_valid=0,half_height_time_valid=0,time_valid=0;
 // Software flags, NOT hardware fADC quality bits:
 // 1=no threshold bracket, 2=half-height unavailable, 4=saturation,
 // 8=peak at final waveform sample, 16=no accepted pulse.
 int time_quality=16;
};
inline double MDSampleTime(int index,const MDPulseConfig& c){
 return c.gate_start_ns+(index+0.5)*c.dt_ns;
}
// Last rising crossing at/before the selected peak. Includes pre-threshold samples,
// because half-height can be below the pulse-detection threshold.
inline bool MDRisingCrossing(const std::vector<unsigned short>& a,int peak,
                            double level,const MDPulseConfig& c,double& time){
 for(int i=peak-1;i>=0;--i){
  if(a[i]<=level && a[i+1]>level){
   time=MDSampleTime(i,c)+c.dt_ns*(level-a[i])/(a[i+1]-a[i]);return true;
  }
 }
 return false;
}
inline MDPulseResult ReconstructMDPulse(const std::vector<unsigned short>& a,const MDPulseConfig& c){
 c.Validate();
 if(a.size()<=static_cast<size_t>(c.baseline_samples))throw std::runtime_error("Too few MD samples");
 MDPulseResult r;
 for(int i=0;i<c.baseline_samples;++i)r.baseline+=a[i];
 r.baseline/=c.baseline_samples;
 for(int i=0;i<c.baseline_samples;++i)r.noise+=(a[i]-r.baseline)*(a[i]-r.baseline);
 r.noise=std::sqrt(r.noise/(c.baseline_samples-1));
 int maxadc=(1<<c.bits)-1;
 for(auto x:a){if(x>maxadc)throw std::runtime_error("ADC exceeds replay bit depth");if(x==maxadc)r.saturated=1;}
 r.time_quality=16|(r.saturated?4:0);
 r.applied_threshold_adc=std::max(c.threshold_adc,c.threshold_sigma*r.noise);
 const double level=r.baseline+r.applied_threshold_adc;
 auto pk=std::max_element(a.begin()+c.baseline_samples,a.end());
 int peak=static_cast<int>(pk-a.begin());
 if(*pk-r.baseline<=r.applied_threshold_adc)return r;
 // Default preserves exactly the previous global-maximum pulse selection.
 // For >1 required sample, choose the highest peak in a qualifying consecutive run.
 if(c.threshold_samples>1){
  int best=-1;
  for(int i=c.baseline_samples;i<static_cast<int>(a.size());){
   if(a[i]<=level){++i;continue;}
   int start=i,local=i;
   while(i<static_cast<int>(a.size())&&a[i]>level){if(a[i]>a[local])local=i;++i;}
   if(i-start>=c.threshold_samples&&(best<0||a[local]>a[best]))best=local;
  }
  if(best<0)return r;
  peak=best;
 }
 r.found=1;r.peak=peak;r.height=a[peak]-r.baseline;
 r.peak_time_ns=MDSampleTime(peak,c);r.time_quality=0;
 int tc=peak;
 while(tc>c.baseline_samples&&a[tc-1]>level)--tc;
 r.threshold_sample=tc;
 // Interpolate only the crossing that begins this accepted above-threshold run.
 if(tc>0&&a[tc-1]<=level&&a[tc]>level){
  r.threshold_time_ns=MDSampleTime(tc-1,c)+c.dt_ns*(level-a[tc-1])/(a[tc]-a[tc-1]);
  r.threshold_time_valid=1;
 }else r.time_quality|=1;
 if(r.saturated)r.time_quality|=4;
 if(peak==static_cast<int>(a.size())-1)r.time_quality|=8;
 // A clipped peak or saturated waveform cannot provide a trustworthy half height.
 if(!r.saturated&&peak<static_cast<int>(a.size())-1){
  r.half_height_time_valid=MDRisingCrossing(a,peak,r.baseline+0.5*r.height,c,r.half_height_time_ns);
 }
 if(!r.half_height_time_valid)r.time_quality|=2;
 if(c.timing_reference==0){r.time_ns=r.peak_time_ns;r.time_valid=1;}
 else if(c.timing_reference==1){r.time_ns=r.threshold_time_ns;r.time_valid=r.threshold_time_valid;}
 else {r.time_ns=r.half_height_time_ns;r.time_valid=r.half_height_time_valid;}
 const int anchor=c.integration_reference==0?peak:tc;
 r.first=std::max(0,anchor-c.nsb);
 r.last=std::min(static_cast<int>(a.size())-1,anchor+c.nsa-1);
 r.clipped=(r.first!=anchor-c.nsb||r.last!=anchor+c.nsa-1);
 for(int i=r.first;i<=r.last;++i)r.integral+=a[i]-r.baseline;
 r.charge_pc=r.integral*(c.range_v/std::pow(2.,c.bits))*(c.dt_ns*1e-9)/c.resistance_ohm*1e12;
 return r;
}
#endif
