#include "MOLLERShowerMaxReader.h"
#include <TTreeFormula.h>
#include <TCanvas.h>
#include <TH1D.h>
#include <TSystem.h>
#include <map>
#include <iostream>
// qpe_C is the charge used by the SM digitiser, not a fitted replay gain.
void validate_showermax_replay(const char* input,const char* replay,
 const char* prefix="SM_validation",double qpe_C=1.6e-13) {
 const char* db=gSystem->Getenv("DB_DIR");if(!db)throw std::runtime_error("Set DB_DIR");
 std::string csv=std::string(db)+"/MOLLER_showermax_electronics_map.csv";
 MOLLERShowerMaxReader reader(input,csv.c_str());
 const auto channelmap=SMInput::LoadMap(csv);
 TFile fi(input),fr(replay);if(fi.IsZombie()||fr.IsZombie())throw std::runtime_error("Cannot open files");
 TTree *ti=nullptr,*tr=nullptr;fi.GetObject("showermax_digi",ti);fr.GetObject("T",tr);
 if(!ti||!tr)throw std::runtime_error("Missing trees");
 auto formula=[](const char* name,const char* expr,TTree* tree){
  auto f=new TTreeFormula(name,expr,tree);
  if(f->GetNdim()==0)throw std::runtime_error(std::string("Missing expression: ")+expr);return f;
 };
 auto ev=formula("smv_ev","moller.sm.evnum",tr);
 auto id=formula("smv_id","moller.sm.detid",tr);
 auto ro=formula("smv_ro","moller.sm.readout",tr);
 auto found=formula("smv_found","moller.sm.found",tr);
 auto charge=formula("smv_q","moller.sm.charge_pc",tr);
 auto clipped=formula("smv_clip","moller.sm.clipped",tr);
 auto saturated=formula("smv_sat","moller.sm.saturated",tr);
 auto rate=formula("smv_rate","sm.input.rate_GHz",tr);
 auto truthid=formula("smv_tid","detid",ti);
 auto npe=formula("smv_npe","npe_poiss",ti);
 std::map<int,Long64_t> entries;
 for(Long64_t i=0;i<tr->GetEntries();++i){tr->GetEntry(i);ev->GetNdata();
  if(!entries.emplace(int(ev->EvalInstance()),i).second)throw std::runtime_error("Duplicate replay event ID");}
 TH1D hq("sm_validation_charge","Reconstructed SM charge;Charge [pC];Read-out channels with a pulse",100,-5,50);
 TH1D hd("sm_validation_residual","Charge minus full generated charge;Q_{reco}-N_{PE}Q_{1PE} [pC];Selected channels",100,-10,10);
 Long64_t matched=0,waves=0,detected=0,comparisons=0;std::set<int> inputids;
 for(Long64_t i=0;i<reader.Entries();++i){auto e=reader.Read(i);
  if(!inputids.insert(e.evnum).second)throw std::runtime_error("Duplicate input event ID");
  auto it=entries.find(e.evnum);if(it==entries.end())continue;
  ++matched;tr->GetEntry(it->second);ti->GetEntry(i);
  for(auto f:{id,ro,found,charge,clipped,saturated,rate,truthid,npe})f->GetNdata();
  if(id->GetNdata()!=28)throw std::runtime_error("Expected 28 mapped output rows");
  if(std::abs(rate->EvalInstance()-e.rate_GHz)>1e-10*std::max(1.,std::abs(e.rate_GHz)))
   throw std::runtime_error("Event rate mismatch");
  std::map<int,bool> available;for(const auto& c:e.channels)available[c.detid]=!c.samples.empty();
  std::map<int,double> pe;for(int k=0;k<truthid->GetNdata();++k)pe[int(truthid->EvalInstance(k))]=npe->EvalInstance(k);
  std::set<int> seen;
  for(int k=0;k<28;++k){int did=id->EvalInstance(k);bool has=ro->EvalInstance(k)!=0;
   if(!seen.insert(did).second||!channelmap.count(did))throw std::runtime_error("Bad output module ID");
   if(has!=available[did])throw std::runtime_error("Readout availability mismatch");
   double q=charge->EvalInstance(k);
   if(!has){if(found->EvalInstance(k)!=0||std::isfinite(q))throw std::runtime_error("Missing waveform represented as a measurement");continue;}
   ++waves;if(found->EvalInstance(k)==0)continue;++detected;hq.Fill(q);
   if(pe[did]>0&&!clipped->EvalInstance(k)&&!saturated->EvalInstance(k)){
    hd.Fill(q-pe[did]*qpe_C*1e12);++comparisons;}
  }
 }
 TCanvas c("sm_validation_canvas","SM replay validation",1000,450);c.Divide(2,1);c.cd(1);hq.Draw();c.cd(2);hd.Draw();
 c.SaveAs((std::string(prefix)+".png").c_str());
 std::cout<<"Matched events: "<<matched<<"; input/replay entries: "<<reader.Entries()<<" / "<<tr->GetEntries()
 <<"\nAvailable waveforms: "<<waves<<"; pulses found: "<<detected<<"; clean positive-PE comparisons: "<<comparisons
 <<"\nPASS availability, event rate, missing-waveform flags and module checks\n"
 <<"Charge truth is full generated PE charge; finite integration, baseline and detection selection affect residuals.\n"
 <<"Missing waveforms cannot establish pedestal noise or zero-PE false-positive efficiency.\n";
 for(auto f:{ev,id,ro,found,charge,clipped,saturated,rate,truthid,npe})delete f;
}
