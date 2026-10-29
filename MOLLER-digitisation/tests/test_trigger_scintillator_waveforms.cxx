#include "MOLLERTriggerScintillatorDigitizer.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

static void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
template<class T> static std::vector<T> values(TTree& t,const char* name,Long64_t e) {
  std::vector<T>* p=nullptr;
  require(t.SetBranchAddress(name,&p)>=0,"Missing test branch");t.GetEntry(e);
  auto v=*p;t.ResetBranchAddresses();return v;
}
int main(int argc,char** argv) {
  using D=MOLLERTriggerScintillatorDigitizer;
  D d;D::Config c;c.enable_electronics=true;c.readout_all_channels=true;c.store_waveforms=true;
  c.pedestal_mean=800;c.pedestal_sigma=0;c.scint_decay_ns=0;c.sigma_time_ns=0;
  d.SetConfig(c);d.SetRandomSeed(12345);TTree tree("waveforms","TS electronics tests");d.BookBranches(&tree);
  d.DigitizeEvent({},{});tree.Fill(); // Full 14-channel pedestal readout.
  d.DigitizeEvent({{400,0,11,90,0}},{{400,0,2}});tree.Fill();
  d.DigitizeEvent({{401,0,11,90,0}},{});tree.Fill(); // Missing sum must not imply zero truth PE.
  d.DigitizeEvent({},{{402,0,2}});tree.Fill(); // Positive PE, no valid signal time.
  d.DigitizeEvent({{400,0,11,1000,1}},{{400,0,2}});tree.Fill(); // Entire signal outside gate.
  c.pmt_gain=1e10;d.SetConfig(c);d.DigitizeEvent({{400,0,11,90,1}},{{400,0,2}});tree.Fill();
  c.pmt_gain=1e6;c.pedestal_mean=0;d.SetConfig(c);d.DigitizeEvent({},{});tree.Fill();
  bool caught=false;try{d.DigitizeEvent({{400,0,11,90,0},{400,1,11,90,0}},{{400,0,1}});}catch(const std::runtime_error&){caught=true;}
  require(caught,"Full-readout copy ambiguity must be rejected");
  tree.ResetBranchAddresses();
  auto ids=values<int>(tree,"detid",0);auto wf=values<unsigned short>(tree,"wf_adc",0);
  auto raw=values<int>(tree,"adc_int",0);auto sub=values<double>(tree,"adc_int_pedsub",0);
  auto valid=values<int>(tree,"waveform_valid",0);auto present=values<int>(tree,"input_present",0);
  require(ids.size()==14&&wf.size()==1400,"Full-channel waveform sizes");
  for(size_t i=0;i<14;++i)require(raw[i]==80000&&sub[i]==0&&valid[i]==1&&present[i]==0,"No-signal pedestal readout");
  for(auto x:wf)require(x==800,"Pedestal samples must remain stored");
  auto gen=values<double>(tree,"charge_generated_pc",1),gate=values<double>(tree,"charge_in_gate_pc",1);
  require(gen[0]>0&&std::abs(gen[0]-gate[0])<1e-3,"Integrated SPE pulse must conserve charge in a wide gate");
  wf=values<unsigned short>(tree,"wf_adc",1);raw=values<int>(tree,"adc_int",1);sub=values<double>(tree,"adc_int_pedsub",1);
  int sum=0;for(size_t i=0;i<100;++i)sum+=wf[i];require(sum==raw[0]&&sub[0]==sum-80000,"Waveform/integral consistency");
  const double qlsb_pc=(1./4096)*4e-9/50*1e12;
  require(std::abs(sub[0]*qlsb_pc-gate[0])<=50*qlsb_pc+1e-8,"ADC rounding bound on charge");
  auto npe=values<int>(tree,"npe_poiss",2);valid=values<int>(tree,"waveform_valid",2);
  require(npe[1]==-1&&valid[1]==0,"Missing sum not silently converted to measured zero");
  npe=values<int>(tree,"npe_poiss",3);valid=values<int>(tree,"waveform_valid",3);
  gate=values<double>(tree,"charge_in_gate_pc",3);
  require(npe[2]>0&&valid[2]==0&&std::isnan(gate[2]),"Missing hit time must not fabricate a signal at t=0");
  gen=values<double>(tree,"charge_generated_pc",4);gate=values<double>(tree,"charge_in_gate_pc",4);
  require(gen[0]>0&&gate[0]==0,"Finite acquisition gate loses late signal charge");
  require(values<int>(tree,"waveform_saturated",5)[0]==1,"High gain should reach ADC saturation");
  require(values<int>(tree,"waveform_underflow",6)[0]==1,"Lower ADC rail flag");

  // One effective channel, with emission delays isolated from TTS and pulse shape.
  D emission;D::Config ec;ec.detector_ids={400};ec.enable_electronics=true;ec.store_pe_times=true;
  ec.scint_decay_ns=2.1;ec.sigma_time_ns=0;ec.pedestal_sigma=0;
  emission.SetConfig(ec);emission.SetRandomSeed(45);TTree et("emission","Scintillation delay");emission.BookBranches(&et);
  emission.DigitizeEvent({{400,0,11,90,1}},{{400,0,100}});et.Fill();et.ResetBranchAddresses();
  auto times=values<double>(et,"pe_time_ns",0);require(times.size()>10000,"Emission test statistics");
  double delay=0;for(auto t:times){require(t>=90,"Exponential emission delay is causal without TTS");delay+=t-90;}
  require(std::abs(delay/times.size()-2.1)<.08,"Scintillation mean decay time");
  require(values<double>(et,"first_hit_time_ns",0)[0]==90,"REMOLL hit time must remain unchanged");

  D weighted;ec.hit_time_mode=1;ec.scint_decay_ns=0;weighted.SetConfig(ec);weighted.SetRandomSeed(46);
  TTree wt("weighted","Recorded-step timing weights");weighted.BookBranches(&wt);
  weighted.DigitizeEvent({{400,0,11,90,1},{400,0,11,190,3}},{{400,0,10}});wt.Fill();wt.ResetBranchAddresses();
  times=values<double>(wt,"pe_time_ns",0);size_t late=0;for(double t:times){require(t==90||t==190,"Timing anchors");if(t==190)++late;}
  require(std::abs(double(late)/times.size()-.75)<.08,"Saved-hit energy-weighted time allocation");

  // PE counts must be identical with/without electronics for a fixed nonzero seed.
  D bare,electronics;D::Config bc;bc.detector_ids={400};bare.SetConfig(bc);bare.SetRandomSeed(99);
  bc.enable_electronics=true;electronics.SetConfig(bc);electronics.SetRandomSeed(99);
  TTree bt("bare","PE-only"),xt("enabled","Electronics");bare.BookBranches(&bt);electronics.BookBranches(&xt);
  for(int i=0;i<100;++i){bare.DigitizeEvent({{400,0,11,90,1}},{{400,0,1}});electronics.DigitizeEvent({{400,0,11,90,1}},{{400,0,1}});bt.Fill();xt.Fill();}
  bt.ResetBranchAddresses();xt.ResetBranchAddresses();
  for(int i=0;i<100;++i)require(values<int>(bt,"npe_poiss",i)==values<int>(xt,"npe_poiss",i),"Electronics must not perturb the PE random stream");

  D noise;D::Config nc;nc.detector_ids={400};nc.enable_electronics=true;nc.readout_all_channels=true;
  nc.pedestal_mean=800;nc.pedestal_sigma=5;noise.SetConfig(nc);noise.SetRandomSeed(25);
  TTree nt("noise","Integrated pedestal noise");noise.BookBranches(&nt);
  for(int i=0;i<4000;++i){noise.DigitizeEvent({},{});nt.Fill();}nt.ResetBranchAddresses();
  std::vector<double>* v=nullptr;nt.SetBranchAddress("adc_int_pedsub",&v);double sumx=0,sumx2=0;
  for(int i=0;i<4000;++i){nt.GetEntry(i);sumx+=(*v)[0];sumx2+=(*v)[0]*(*v)[0];}nt.ResetBranchAddresses();
  const double mean=sumx/4000,sd=std::sqrt((sumx2-sumx*sumx/4000)/3999);
  require(std::abs(mean)<3&&std::abs(sd-50)<3,"Pedestal sum mean and sqrt(N) width");

  require(argc==2,"Pass digitizer source directory to waveform test");
  D calibration;D::Config cc;cc.detector_ids={400};cc.enable_electronics=true;cc.readout_all_channels=true;
  cc.calibration_file=std::string(argv[1])+"/TS_channel_calibration.csv";
  cc.detector_map_file=std::string(argv[1])+"/MOLLER_trigscint_electronics_map.csv";
  calibration.SetConfig(cc);TTree ct("calibration","CSV calibration/map");calibration.BookBranches(&ct);
  calibration.DigitizeEvent({},{});ct.Fill();ct.ResetBranchAddresses();
  require(std::abs(values<double>(ct,"pedestal_mean_adc",0)[0]-786.432)<1e-8&&values<double>(ct,"pmt_gain",0)[0]==960000,"External channel calibration");
  require(values<int>(ct,"rocid",0)[0]==3&&values<int>(ct,"slot",0)[0]==3&&values<int>(ct,"channel",0)[0]==0&&values<int>(ct,"mapping_valid",0)[0]==1,"Electronics map");
  std::cout<<"PASS TS full readout, charge conservation, ADC, absent data, scintillation, timing weights, PE-stream preservation, calibration/map, pedestal mean/SD "<<mean<<" / "<<sd<<"\n";
}
