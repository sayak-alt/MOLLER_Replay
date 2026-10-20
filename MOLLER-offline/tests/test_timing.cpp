#include "MDPulse.h"
#include <cassert>
#include <cmath>
#include <iostream>
static bool near(double a,double b){return std::abs(a-b)<1e-10;}
int main(){
 MDPulseConfig c;c.threshold_adc=30;c.threshold_sigma=0;
 std::vector<unsigned short> a(100,1000);
 a[41]=1020;a[42]=1060;a[43]=1100;a[44]=1080;a[45]=1040;a[46]=1010;a[64]=1001;
 auto r=ReconstructMDPulse(a,c);
 assert(r.found&&r.peak==43&&r.threshold_sample==42);
 assert(r.threshold_time_valid&&r.half_height_time_valid&&r.time_quality==0);
 assert(near(r.peak_time_ns,4)&&near(r.threshold_time_ns,-3)&&near(r.half_height_time_ns,-1));
 assert(r.first==38&&r.last==64&&near(r.time_ns,r.peak_time_ns));
 c.timing_reference=2;auto h=ReconstructMDPulse(a,c);
 assert(h.time_valid&&near(h.time_ns,-1)&&h.integral==r.integral&&h.charge_pc==r.charge_pc);
 c.integration_reference=1;auto t=ReconstructMDPulse(a,c);
 assert(t.first==37&&t.last==63&&near(t.integral,r.integral-1));
 assert(near(t.time_ns,h.time_ns)); // Changing integration does not change timing.
 c.threshold_adc=80;auto high=ReconstructMDPulse(a,c);
 assert(high.threshold_sample==43&&near(high.threshold_time_ns,2));
 assert(near(high.half_height_time_ns,-1)); // Half height precedes threshold here.
 c.gate_start_ns+=20;auto shifted=ReconstructMDPulse(a,c);
 assert(near(shifted.half_height_time_ns,high.half_height_time_ns+20));
 assert(shifted.charge_pc==high.charge_pc);
 c=MDPulseConfig();c.threshold_adc=30;c.threshold_sigma=0;
 a.assign(100,1000);a[20]=1200;a[40]=1050;a[41]=1080;a[42]=1060;
 assert(ReconstructMDPulse(a,c).peak==20);
 c.threshold_samples=3;r=ReconstructMDPulse(a,c);assert(r.found&&r.peak==41&&r.threshold_sample==40);
 c.threshold_samples=4;r=ReconstructMDPulse(a,c);assert(!r.found&&std::isnan(r.time_ns));
 c=MDPulseConfig();c.threshold_adc=30;c.threshold_sigma=0;c.timing_reference=2;
 a.assign(100,1000);a[50]=4095;r=ReconstructMDPulse(a,c);
 assert(r.saturated&&!r.half_height_time_valid&&!r.time_valid&&std::isnan(r.time_ns));
 assert((r.time_quality&4)&&(r.time_quality&2));
 a.assign(100,1000);a[99]=1100;r=ReconstructMDPulse(a,c);
 assert(r.found&&(r.time_quality&8)&&!r.half_height_time_valid&&r.clipped);
 a.assign(100,1000);r=ReconstructMDPulse(a,c);
 assert(!r.found&&r.time_quality==16&&std::isnan(r.threshold_time_ns));
 a[10]=1100;a[11]=1080;c.nsb=15;c.integration_reference=1;r=ReconstructMDPulse(a,c);
 assert(r.threshold_sample==10&&r.threshold_time_valid&&r.clipped&&r.first==0);
 a.assign(100,1000);a[9]=1100;a[10]=1100;a[11]=1080;
 c.threshold_adc=10;r=ReconstructMDPulse(a,c);
 assert(r.found&&!r.threshold_time_valid&&(r.time_quality&1));
 bool failed=false;try{c.threshold_samples=0;ReconstructMDPulse(a,c);}catch(const std::exception&){failed=true;}assert(failed);
 std::cout<<"PASS: threshold/half-height interpolation, independent references, consecutive samples, clipping, saturation, missing brackets and invalid config\n";
}
