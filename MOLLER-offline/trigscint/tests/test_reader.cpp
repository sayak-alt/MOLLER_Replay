#include "MOLLERTriggerScintillatorReader.h"
#include <filesystem>
#include <iostream>
static void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
static void fixture(const char* path,int corrupt) {
 TFile file(path,"RECREATE");TTree t("trigscint_digi","ADC-only TS fixture without physics truth branches");
 Long64_t evnum=0;bool enabled=true;int nsamples=100,bits=12;double dt=4,start=-170,range=1,resistance=50;
 std::vector<int> ids,copy,roc,slot,channel,mapped,raw,wid,wcopy,wroc,wslot,wchannel,index;std::vector<unsigned short> adc;
 t.Branch("evnum",&evnum,"evnum/L");t.Branch("electronics_enabled",&enabled,"electronics_enabled/O");
 t.Branch("nsamples",&nsamples,"nsamples/I");t.Branch("nbits",&bits,"nbits/I");t.Branch("dt_ns",&dt,"dt_ns/D");t.Branch("gate_start_ns",&start,"gate_start_ns/D");t.Branch("range_v",&range,"range_v/D");t.Branch("resistance_ohm",&resistance,"resistance_ohm/D");
 t.Branch("detid",&ids);t.Branch("copyid",&copy);t.Branch("rocid",&roc);t.Branch("slot",&slot);t.Branch("channel",&channel);t.Branch("mapping_valid",&mapped);t.Branch("adc_int",&raw);
 t.Branch("wf_detid",&wid);t.Branch("wf_copyid",&wcopy);t.Branch("wf_rocid",&wroc);t.Branch("wf_slot",&wslot);t.Branch("wf_channel",&wchannel);t.Branch("wf_samp",&index);t.Branch("wf_adc",&adc);
 for(int e=0;e<3;++e) {
  evnum=e;ids.clear();copy.clear();roc.clear();slot.clear();channel.clear();mapped.clear();raw.clear();wid.clear();wcopy.clear();wroc.clear();wslot.clear();wchannel.clear();index.clear();adc.clear();
  int first=e==1?2:0,last=e==1?2:13;
  if(e!=2)for(int i=last;i>=first;--i) {
   ids.push_back(400+i);copy.push_back(5);roc.push_back(3);slot.push_back(3);channel.push_back(i);mapped.push_back(1);int sum=0;
   for(int s=99;s>=0;--s){unsigned short a=800+(s==65&&i==2?100:0);sum+=a;wid.push_back(400+i);wcopy.push_back(5);wroc.push_back(3);wslot.push_back(3);wchannel.push_back(i);index.push_back(s);adc.push_back(a);}
   raw.push_back(sum);
  }
  if(corrupt==1&&e==0)raw[0]+=1;
  if(corrupt==2&&e==0)index[0]=index[1];
  if(corrupt==3)enabled=false;
  t.Fill();
 }
 t.Write();t.SetDirectory(nullptr);file.Close();
}
int main(int argc,char** argv) {
 require(argc==3,"Supply map and temporary fixture paths");fixture(argv[2],0);
 {MOLLERTriggerScintillatorReader reader(argv[2],argv[1]);require(reader.Entries()==3,"Reader event count");
 auto a=reader.Read(0);require(a.channels.size()==14,"Full input readout");
 for(const auto& c:a.channels)require(c.samples.size()==100&&c.copyid==5&&c.samples[65]==(c.detid==402?900:800),"Address/sample/copy assembly");
 require(reader.Read(1).channels.size()==1&&reader.Read(2).channels.empty(),"Sparse and empty-event reset");}
 for(int bad=1;bad<=3;++bad){fixture(argv[2],bad);bool rejected=false;try{MOLLERTriggerScintillatorReader reader(argv[2],argv[1]);reader.Read(0);}catch(const std::exception&){rejected=true;}require(rejected,"Corrupt TS input accepted");}
 std::filesystem::remove(argv[2]);std::cout<<"PASS ROOT TS reader full/sparse/empty assembly, raw-sum/index checks; input has no PE, injected calibration or MC timing branches\n";
}
