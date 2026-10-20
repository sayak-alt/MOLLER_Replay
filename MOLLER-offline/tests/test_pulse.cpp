#include "MDPulse.h"
#include <cassert>
#include <cmath>
int main(){MDPulseConfig c;std::vector<unsigned short>a(100,820);auto r=ReconstructMDPulse(a,c);assert(!r.found&&r.baseline==820&&r.charge_pc==0);a[60]=920;r=ReconstructMDPulse(a,c);assert(r.found&&r.peak==60&&r.first==55&&r.last==81);assert(r.integral==100);assert(std::abs(r.charge_pc-1.953125)<1e-10);a.assign(100,820);a[99]=4095;r=ReconstructMDPulse(a,c);assert(r.found&&r.clipped&&r.saturated);bool bad=false;try{c.nsa=0;ReconstructMDPulse(a,c);}catch(...){bad=true;}assert(bad);}
