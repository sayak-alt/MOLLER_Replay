#include <TFile.h>
#include <TTree.h>
#include <TTreeFormula.h>
#include <TH1D.h>
#include <TH2D.h>
#include <TCanvas.h>
#include <TStyle.h>
#include <memory>
#include <map>
#include <set>
#include <iostream>
#include <stdexcept>
#include <cmath>
// TTreeFormula accepts both digitiser STL vectors and Podd output leaf arrays.
void validate_md_replay(const char* digitised,const char* replayed,
                        const char* prefix="MD_validation"){
 std::unique_ptr<TFile> fi(TFile::Open(digitised)),fr(TFile::Open(replayed));
 if(!fi||fi->IsZombie()||!fr||fr->IsZombie())throw std::runtime_error("Cannot open validation inputs");
 TTree *in=nullptr,*out=nullptr;fi->GetObject("pmt_digi",in);fr->GetObject("T",out);
 if(!in||!out)throw std::runtime_error("Missing pmt_digi or replay T");
 auto formula=[](TTree* t,const char* name,const char* expression){
  auto p=std::make_unique<TTreeFormula>(name,expression,t);
  if(p->GetNdim()<=0)throw std::runtime_error(std::string("Invalid/missing variable: ")+expression);return p;};
 auto ie=formula(in,"ie","evnum"),id=formula(in,"id","detid"),np=formula(in,"np","npe_poiss"),ped=formula(in,"pd","pedestal_mean_adc"),gain=formula(in,"gn","pmt_gain");
 auto oe=formula(out,"oe","moller.md.evnum"),od=formula(out,"od","moller.md.detid"),base=formula(out,"bs","moller.md.baseline"),noise=formula(out,"no","moller.md.noise"),found=formula(out,"fo","moller.md.found"),charge=formula(out,"ch","moller.md.charge_pc"),clip=formula(out,"cl","moller.md.clipped"),sat=formula(out,"sa","moller.md.saturated");
 std::map<Long64_t,Long64_t> events;
 for(Long64_t j=0;j<out->GetEntries();++j){out->GetEntry(j);oe->GetNdata();auto ev=static_cast<Long64_t>(oe->EvalInstance());if(!events.emplace(ev,j).second)throw std::runtime_error("Duplicate replay event ID: validate one source file at a time");}
 TH1::AddDirectory(false);
 auto residual=new TH1D("baseline_residual","Baseline accuracy;Estimated - injected baseline [ADC counts];Channel-events",100,-10,10);
 auto rms=new TH1D("baseline_noise","Estimated baseline noise;Sample standard deviation [ADC counts];Channel-events",100,0,15);
 auto denom=new TH1D("eff_total",";Simulated PE;Channel-events",101,-.5,100.5);
 auto numer=new TH1D("eff_found",";Simulated PE;Channel-events",101,-.5,100.5);
 auto qplot=new TH2D("charge_vs_truth","Detected, unclipped, unsaturated pulses;Full generated PE charge [pC];Reconstructed window charge [pC]",120,0,30,140,-5,30);
 Long64_t matched=0,unmatched=0,zero=0,falsepos=0,positive=0,detected=0,pairs=0,excluded=0;
 for(Long64_t j=0;j<in->GetEntries();++j){
  in->GetEntry(j);ie->GetNdata();auto ev=static_cast<Long64_t>(ie->EvalInstance());auto entry=events.find(ev);if(entry==events.end()){++unmatched;continue;}
  out->GetEntry(entry->second);++matched;
  int n=id->GetNdata(),m=od->GetNdata();
  if(np->GetNdata()!=n||ped->GetNdata()!=n||gain->GetNdata()!=n||base->GetNdata()!=m||noise->GetNdata()!=m||found->GetNdata()!=m||charge->GetNdata()!=m||clip->GetNdata()!=m||sat->GetNdata()!=m)throw std::runtime_error("Unaligned validation arrays");
  std::map<int,int> reco;
  for(int k=0;k<m;++k)if(!reco.emplace(static_cast<int>(od->EvalInstance(k)),k).second)throw std::runtime_error("Duplicate replay detector");
  std::set<int> inputids;
  for(int k=0;k<n;++k){int did=static_cast<int>(id->EvalInstance(k));if(!inputids.insert(did).second)throw std::runtime_error("Duplicate input detector");auto r=reco.find(did);if(r==reco.end())throw std::runtime_error("Missing replay channel in matched event");int a=r->second;
   double npe=np->EvalInstance(k),b=base->EvalInstance(a),injected=ped->EvalInstance(k);bool yes=found->EvalInstance(a)!=0;
   residual->Fill(b-injected);rms->Fill(noise->EvalInstance(a));denom->Fill(npe);if(yes)numer->Fill(npe);++pairs;
   if(npe==0){++zero;if(yes)++falsepos;}else{++positive;if(yes)++detected;}
   if(clip->EvalInstance(a)!=0||sat->EvalInstance(a)!=0){++excluded;continue;}
   if(npe>0&&yes){double q=npe*gain->EvalInstance(k)*1.602176634e-19*1e12;qplot->Fill(q,charge->EvalInstance(a));}
  }
  if(inputids.size()!=reco.size())throw std::runtime_error("Extra replay detector in matched event");
 }
 if(matched==0)throw std::runtime_error("No matching events");
 auto efficiency=static_cast<TH1D*>(numer->Clone("detection_efficiency"));efficiency->SetTitle("Pulse detection;Simulated PE;Detected fraction");efficiency->Divide(numer,denom,1,1,"B");efficiency->SetMinimum(0);efficiency->SetMaximum(1.05);
 auto c=new TCanvas("md_validation","MD replay validation",1200,900);c->Divide(2,2);c->cd(1);residual->Draw();c->cd(2);rms->Draw();c->cd(3);efficiency->Draw("E1");c->cd(4);qplot->Draw("COLZ");c->SaveAs(TString(prefix)+".png");
 TFile save(TString(prefix)+".root","RECREATE");residual->Write();rms->Write();denom->Write();numer->Write();efficiency->Write();qplot->Write();c->Write();save.Close();
 std::cout<<"Matched events: "<<matched<<"; unmatched input events: "<<unmatched<<"; unmatched replay events: "<<events.size()-matched<<"\nMatched channel-events: "<<pairs<<"\nZero-PE channels: "<<zero<<"; false detections: "<<falsepos;
 if(zero)std::cout<<" ("<<100.*falsepos/zero<<"%)";
 std::cout<<"\nPositive-PE channels: "<<positive<<"; detected: "<<detected<<"\nClipped/saturated channel-events: "<<excluded<<"\nBaseline residual mean/RMS: "<<residual->GetMean()<<" / "<<residual->GetRMS()<<" ADC counts\n";
 std::cout<<"Charge reference is full generated PE charge, not expected charge inside the replay window. Gate losses, noise, threshold selection and baseline estimation affect agreement.\n";
}
