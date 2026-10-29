#include "MDPulse.h"
#include <iostream>
static void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main() {
 MDPulseConfig c;c.timing_reference=2;c.integration_reference=1;
 std::vector<unsigned short> adc(100,800);
 auto pedestal=ReconstructMDPulse(adc,c);
 require(!pedestal.found&&!pedestal.time_valid&&pedestal.baseline==800&&pedestal.charge_pc==0,"Pedestal pulse rejection");
 // Analytic mean TS pulse: 2.1 ns emission convolved with 10 ns SPE response.
 const double charge=10e-12,start=80,emission=2.1,pulse=10;
 auto tail=[&](double t){if(t<=start)return 1.;t-=start;return (pulse*std::exp(-t/pulse)-emission*std::exp(-t/emission))/(pulse-emission);};
 const double qlsb=c.range_v/std::pow(2.,c.bits)*c.dt_ns*1e-9/c.resistance_ohm;
 for(int i=0;i<100;++i){double a=c.gate_start_ns+i*c.dt_ns;double q=charge*(tail(a)-tail(a+c.dt_ns));adc[i]=static_cast<unsigned short>(std::llround(800+q/qlsb));}
 auto r=ReconstructMDPulse(adc,c);
 require(r.found&&r.time_valid&&r.threshold_time_valid&&r.half_height_time_valid&&!r.clipped&&!r.saturated,"TS-shaped pulse reconstruction flags");
 require(std::abs(r.charge_pc-10)<.15&&r.half_height_time_ns>76&&r.half_height_time_ns<90,"TS pulse charge/timing");
 c.timing_reference=1;auto threshold=ReconstructMDPulse(adc,c);
 require(threshold.time_ns==threshold.threshold_time_ns&&threshold.charge_pc==r.charge_pc,"Threshold timing selector must not change integration");
 c.timing_reference=0;auto peak=ReconstructMDPulse(adc,c);
 require(peak.time_ns==peak.peak_time_ns&&peak.time_ns>=r.half_height_time_ns,"Peak time selector");
 std::cout<<"PASS TS-shaped charge "<<r.charge_pc<<" pC and half-height time "<<r.half_height_time_ns<<" ns\n";
}
