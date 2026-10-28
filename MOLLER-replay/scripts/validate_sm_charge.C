// ROOT-only validation. Does not load libmoller or use truth for reconstruction.
#include "SMChargeValidation.h"
#include <TFile.h>
#include <TTree.h>
#include <TTreeFormula.h>
#include <TH1D.h>
#include <TH2D.h>
#include <TGraphAsymmErrors.h>
#include <TCanvas.h>
#include <TLine.h>
#include <TString.h>
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <set>
#include <string>

void validate_sm_charge(const char* digitised,const char* replayed,
                       const char* prefix="SM_charge_validation",int detector=73004)
{
 using namespace SMChargeValidation;
 if(detector!=0&&(detector<73001||detector>73028))
  throw std::runtime_error("Use detector 73001..73028, or 0 for all SM modules");
 if(!digitised||!replayed||!prefix||!*prefix)throw std::runtime_error("Specify filenames and output prefix");
 TFile fi(digitised,"READ"),fr(replayed,"READ");
 if(fi.IsZombie()||fr.IsZombie())throw std::runtime_error("Cannot open input files");
 TTree *ti=nullptr,*tr=nullptr;fi.GetObject("showermax_digi",ti);fr.GetObject("T",tr);
 if(!ti||!tr)throw std::runtime_error("Missing showermax_digi or replay T tree");
 int iev=0;std::vector<int> ids_store,npe_store;
 std::vector<double> qpe_store;
 auto* ids=&ids_store;auto* npe=&npe_store;auto* qpe=&qpe_store;
 // Do not read the large waveform arrays: PE and gain truth suffice here.
 ti->SetBranchStatus("*",0);
 auto bind=[&](const char* name,auto ptr){
  ti->SetBranchStatus(name,1);
  if(!ti->GetBranch(name)||ti->SetBranchAddress(name,ptr)<0)
   throw std::runtime_error(std::string("Missing/wrong truth branch: ")+name);
 };
 bind("evnum",&iev);bind("detid",&ids);bind("npe_poiss",&npe);bind("qpe_c",&qpe);
 struct ResetBranches {TTree* tree;~ResetBranches(){tree->ResetBranchAddresses();}} reset{ti};
 auto formula=[&](const char* name,const char* expr){
  auto f=std::make_unique<TTreeFormula>(name,expr,tr);
  if(f->GetNdim()==0)throw std::runtime_error(std::string("Missing replay expression: ")+expr);
  return f;
 };
 auto event=formula("smq_event","moller.sm.evnum");
 auto rid=formula("smq_id","moller.sm.detid");
 auto readout=formula("smq_readout","moller.sm.readout");
 auto found=formula("smq_found","moller.sm.found");
 auto clipped=formula("smq_clipped","moller.sm.clipped");
 auto saturated=formula("smq_saturated","moller.sm.saturated");
 auto charge=formula("smq_charge","moller.sm.charge_pc");
 std::map<int,Long64_t> replayIndex;
 for(Long64_t i=0;i<tr->GetEntries();++i){
  if(tr->GetEntry(i)<=0)throw std::runtime_error("Cannot read replay entry");
  event->GetNdata();int e=static_cast<int>(event->EvalInstance());
  if(!replayIndex.emplace(e,i).second)throw std::runtime_error("Duplicate replay event ID; validate one run/file pair at a time");
 }
 struct Comparison {int event,id,npe;double truth,reco;};
 std::vector<Comparison> selected;
 std::vector<std::pair<double,bool>> efficiency;
 std::set<int> inputEvents;
 Long64_t matchedEvents=0,unmatchedInput=0,matchedChannels=0,missingChannels=0;
 Long64_t missingWaveforms=0,positive=0,positiveFound=0,zeroReadout=0,falseFound=0,excluded=0;
 Moments residual,fractional;
 for(Long64_t i=0;i<ti->GetEntries();++i){
  if(i%5000==0)std::cout<<"SM charge validation: "<<i<<" / "<<ti->GetEntries()<<std::endl;
  if(ti->GetEntry(i)<=0)throw std::runtime_error("Cannot read digitised entry");
  if(!inputEvents.insert(iev).second)throw std::runtime_error("Duplicate input event ID; validate one run/file pair at a time");
  if(ids->size()!=npe->size()||ids->size()!=qpe->size())throw std::runtime_error("Unaligned truth arrays");
  ChannelIndex(*ids); // Validate uniqueness independently of array order.
  auto pos=replayIndex.find(iev);if(pos==replayIndex.end()){++unmatchedInput;continue;}
  ++matchedEvents;
  if(tr->GetEntry(pos->second)<=0)throw std::runtime_error("Cannot read matched replay entry");
  const int n=rid->GetNdata();
  for(auto* f:{readout.get(),found.get(),clipped.get(),saturated.get(),charge.get()})
   if(f->GetNdata()!=n)throw std::runtime_error("Unaligned replay arrays");
  std::vector<int> replayIds;for(int k=0;k<n;++k)replayIds.push_back(static_cast<int>(rid->EvalInstance(k)));
  const auto channels=ChannelIndex(replayIds);
  for(size_t k=0;k<ids->size();++k){
   const int did=ids->at(k);if(detector&&did!=detector)continue;
   const double truth=TruthCharge(npe->at(k),qpe->at(k));
   auto c=channels.find(did);if(c==channels.end()){++missingChannels;continue;}
   ++matchedChannels;const int j=c->second;
   const bool available=readout->EvalInstance(j)!=0;
   const bool detected=available&&found->EvalInstance(j)!=0;
   if(!available)++missingWaveforms;
   if(npe->at(k)==0){if(available){++zeroReadout;if(detected)++falseFound;}continue;}
   ++positive;if(detected)++positiveFound;
   efficiency.emplace_back(truth,detected);
   if(!detected)continue;
   if(clipped->EvalInstance(j)!=0||saturated->EvalInstance(j)!=0){++excluded;continue;}
   const double q=charge->EvalInstance(j);
   if(!std::isfinite(q))throw std::runtime_error("Nonfinite charge for a detected pulse");
   selected.push_back({iev,did,npe->at(k),truth,q});
   residual.Add(q-truth);fractional.Add(100.*(q-truth)/truth);
  }
 }
 const Long64_t unmatchedReplay=tr->GetEntries()-matchedEvents;
 double xmax=1,ymin=0,ymax=1,rmax=1,fmax=10;
 for(const auto& v:efficiency)xmax=std::max(xmax,v.first);
 for(const auto& c:selected){ymin=std::min(ymin,c.reco);ymax=std::max({ymax,c.truth,c.reco});
  rmax=std::max(rmax,std::abs(c.reco-c.truth));fmax=std::max(fmax,std::abs(100.*(c.reco-c.truth)/c.truth));}
 xmax*=1.05;ymax=std::max(ymax,xmax)*1.05;ymin=ymin<0?ymin*1.1:0.;rmax*=1.05;fmax*=1.05;
 TString label=detector?TString::Format("SM %d",detector):"All SM modules";
 TH2D hq("smq_correlation",label+";Generated charge [pC];Replayed charge [pC]",80,0,xmax,80,ymin,ymax);
 TH1D hr("smq_residual",label+";Q_{replay}-Q_{generated} [pC];Selected channel-events",100,-rmax,rmax);
 TH1D hf("smq_fractional",label+";100 (Q_{replay}-Q_{generated})/Q_{generated} [%];Selected channel-events",100,-fmax,fmax);
 TH1D den("smq_positive","",30,0,xmax),num("smq_detected","",30,0,xmax);
 for(auto* h:{static_cast<TH1*>(&hq),static_cast<TH1*>(&hr),static_cast<TH1*>(&hf),static_cast<TH1*>(&den),static_cast<TH1*>(&num)})h->SetDirectory(nullptr);
 for(const auto& c:selected){hq.Fill(c.truth,c.reco);hr.Fill(c.reco-c.truth);hf.Fill(100.*(c.reco-c.truth)/c.truth);}
 for(const auto& e:efficiency){den.Fill(e.first);if(e.second)num.Fill(e.first);}
 TGraphAsymmErrors eff(&num,&den,"cp");eff.SetName("smq_efficiency");
 eff.SetTitle(label+";Generated charge [pC];Pulse-detection efficiency");eff.SetMinimum(0);eff.SetMaximum(1.05);eff.SetMarkerStyle(20);
 TCanvas canvas("smq_canvas","SM generated/replayed charge",1100,850);canvas.Divide(2,2);
 TLine identity(0,0,xmax,xmax);identity.SetLineColor(2);
 canvas.cd(1);hq.Draw("COLZ");identity.Draw();canvas.cd(2);hr.Draw();canvas.cd(3);hf.Draw();canvas.cd(4);eff.Draw("AP");
 canvas.SaveAs((std::string(prefix)+".png").c_str());canvas.SaveAs((std::string(prefix)+".pdf").c_str());
 TFile out((std::string(prefix)+".root").c_str(),"RECREATE");
 if(out.IsZombie())throw std::runtime_error("Cannot write validation ROOT output");
 hq.Write();hr.Write();hf.Write();den.Write();num.Write();eff.Write();out.Close();
 std::ofstream csv(std::string(prefix)+".csv");if(!csv)throw std::runtime_error("Cannot write CSV");
 csv<<"evnum,detid,npe_poiss,generated_charge_pc,replayed_charge_pc,residual_pc\n"<<std::setprecision(12);
 for(const auto& c:selected)csv<<c.event<<','<<c.id<<','<<c.npe<<','<<c.truth<<','<<c.reco<<','<<c.reco-c.truth<<'\n';
 std::cout<<"\nModule selection: "<<detector<<" (0 means all)\n"
 <<"Matched events: "<<matchedEvents<<"; unmatched input/replay events: "<<unmatchedInput<<" / "<<unmatchedReplay<<'\n'
 <<"Matched selected channel-events: "<<matchedChannels<<"; missing replay channels: "<<missingChannels<<'\n'
 <<"Missing waveforms among matched channels: "<<missingWaveforms<<'\n'
 <<"Positive-PE channels: "<<positive<<"; detected: "<<positiveFound<<'\n'
 <<"Zero-PE channels with readout: "<<zeroReadout<<"; false detections: "<<falseFound;
 if(zeroReadout)std::cout<<" ("<<100.*falseFound/zeroReadout<<"%)";
 std::cout<<"\nDetected positive-PE channels excluded for clipping/saturation: "<<excluded
 <<"\nSelected charge comparisons: "<<selected.size()<<'\n';
 if(residual.n)std::cout<<std::setprecision(8)<<"Residual mean / sample standard deviation: "<<residual.mean<<" / "<<residual.StdDev()<<" pC\n"
 <<"Fractional residual mean / sample standard deviation: "<<fractional.mean<<" / "<<fractional.StdDev()<<" %\n";
 else std::cout<<"No selected charge comparisons; residual statistics are unavailable.\n";
 std::cout<<"Truth = npe_poiss*qpe_c*1e12 pC, using each channel/event's stored qpe_c.\n"
 <<"Charge selection: positive generated PE, available waveform, found pulse, no clipping/saturation; no reconstructed-charge cut.\n"
 <<"Truth is full generated charge, not expected charge within the replay integration window.\n"
 <<"Residuals are conditional on detection; low-charge threshold bias and baseline/integration effects remain.\n"
 <<"Detection efficiency includes all matched positive-PE channels, including missing waveforms; it has no clipping/saturation cut.\n"
 <<"Sample spread is not a Gaussian fit or a charge-independent detector resolution.\n";
}
