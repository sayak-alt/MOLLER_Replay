// Local control-flow test with ROOT API stand-ins; not a ROOT I/O test.
#include "MOLLERShowerMaxDigitizer.h"
#include <cassert>
#include <cmath>
#include <iostream>
int main(int argc,char** argv){
 assert(argc==3);
 MOLLERShowerMaxDigitizer::Config c;
 c.lut_directory=std::string(argv[1])+"/showermax_tables";
 c.detector_map_file=std::string(argv[1])+"/MOLLER_showermax_electronics_map.csv";
 c.calibration_file=std::string(argv[2])+"/SM_channel_calibration.csv";
 c.readout_all_channels=true;c.store_waveforms=true;
 MOLLERShowerMaxDigitizer d;d.SetConfig(c);d.SetRandomSeed(42);
 TTree tree;d.BookBranches(&tree);d.SetEventRate(2e9);d.DigitizeEvent({});
 auto& ids=tree.Get<std::vector<int>>("detid");auto& raw=tree.Get<std::vector<int>>("adc_int");
 auto& pedsub=tree.Get<std::vector<double>>("adc_int_pedsub");auto& adc=tree.Get<std::vector<unsigned short>>("wf_adc");
 auto& ped=tree.Get<std::vector<double>>("pedestal_mean_adc");
 auto& pe=tree.Get<std::vector<int>>("npe_poiss");
 auto& mean=tree.Get<std::vector<double>>("meanpe");
 assert(ids.size()==28&&adc.size()==2800);
 for(int j=0;j<28;++j){assert(ids[j]==73001+j&&pe[j]==0&&mean[j]==0);int sum=0;
  for(int k=0;k<100;++k)sum+=adc[j*100+k];
  assert(sum==raw[j]);assert(std::abs(pedsub[j]-(sum-100*ped[j]))<1e-7);assert(sum>0);
 }
 MOLLERShowerMaxLookupResponse response;response.Load(c.lut_directory,c.legacy_pion_names);
 MOLLERShowerMaxLookupResponse::Hit h;h.det=73004;h.pid=11;h.t=90;h.e=1000;h.x=1100;h.y=0;h.pz=1000;
 const double expected=2*response.MeanPE(h);assert(expected>0);
 d.DigitizeEvent({h,h});assert(ids.size()==28&&adc.size()==2800);
 assert(std::abs(mean[3]-expected)<1e-10);
 for(int j=0;j<28;++j)if(j!=3)assert(pe[j]==0&&mean[j]==0);
 // Empty event after signal must clear previous signal, retaining all pedestal channels.
 d.DigitizeEvent({});for(int j=0;j<28;++j)assert(pe[j]==0&&mean[j]==0);
 // Without CSV, global zero-noise pedestal gives an exactly predictable waveform.
 c.calibration_file="";c.pedestal_mean=300;c.pedestal_sigma=0;d.SetConfig(c);d.DigitizeEvent({});
 for(int j=0;j<28;++j){assert(raw[j]==30000&&pedsub[j]==0);}
 // Sparse option still stores zero-PE response waveforms, unlike the old early return.
 c.readout_all_channels=false;d.SetConfig(c);h.e=10.000001;
 d.DigitizeEvent({h});assert(ids.size()==1&&adc.size()==100);
 std::cout<<"PASS full/empty/signal/sparse readout, LUT sums and fractional pedestal subtraction\n";
}
