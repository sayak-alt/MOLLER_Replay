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
 void Validate()const {
  if(baseline_samples<2||nsb<0||nsb>100000||nsa<1||nsa>100000||bits<1||bits>16||
   !std::isfinite(dt_ns)||dt_ns<=0||!std::isfinite(gate_start_ns)||
   !std::isfinite(range_v)||range_v<=0||!std::isfinite(resistance_ohm)||resistance_ohm<=0||
   !std::isfinite(threshold_adc)||threshold_adc<0||!std::isfinite(threshold_sigma)||threshold_sigma<0)
   throw std::runtime_error("Invalid MD replay configuration");
 }
};
struct MDPulseResult {
 double baseline=0,noise=0,height=0,integral=0,charge_pc=0;
 double time_ns=std::numeric_limits<double>::quiet_NaN();
 int found=0,peak=-1,first=-1,last=-1,clipped=0,saturated=0;
};
inline MDPulseResult ReconstructMDPulse(const std::vector<unsigned short>& a,const MDPulseConfig& c){
 c.Validate();if(a.size()<=static_cast<size_t>(c.baseline_samples))throw std::runtime_error("Too few MD samples");
 MDPulseResult r;for(int i=0;i<c.baseline_samples;++i)r.baseline+=a[i];r.baseline/=c.baseline_samples;
 for(int i=0;i<c.baseline_samples;++i)r.noise+=(a[i]-r.baseline)*(a[i]-r.baseline);
 r.noise=std::sqrt(r.noise/(c.baseline_samples-1));
 int maxadc=(1<<c.bits)-1;for(auto x:a){if(x>maxadc)throw std::runtime_error("ADC exceeds replay bit depth");if(x==maxadc)r.saturated=1;}
 auto pk=std::max_element(a.begin()+c.baseline_samples,a.end());
 double candidate=*pk-r.baseline;
 if(candidate<=std::max(c.threshold_adc,c.threshold_sigma*r.noise))return r;
 r.found=1;r.peak=pk-a.begin();r.height=candidate;
 r.first=std::max(0,r.peak-c.nsb);r.last=std::min(static_cast<int>(a.size())-1,r.peak+c.nsa-1);
 r.clipped=(r.first!=r.peak-c.nsb||r.last!=r.peak+c.nsa-1);
 r.time_ns=c.gate_start_ns+(r.peak+0.5)*c.dt_ns;
 for(int i=r.first;i<=r.last;++i)r.integral+=a[i]-r.baseline;
 r.charge_pc=r.integral*(c.range_v/std::pow(2.,c.bits))*(c.dt_ns*1e-9)/c.resistance_ohm*1e12;
 return r;
}
#endif
