#include <TFile.h>
#include <TTree.h>
#include <TTreeFormula.h>
#include <TH1D.h>
#include <TH2D.h>
#include <TGraphAsymmErrors.h>
#include <TCanvas.h>
#include <TLine.h>
#include <TString.h>
#include <cmath>
#include <algorithm>
#include <limits>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

// MC truth is used only here, after waveform-only Podd reconstruction.
void validate_trigscint_replay(const char* digitised,const char* replayed,const char* prefix="TS_validation",int detector=0,Long64_t maxevents=-1) {
 if(detector!=0&&(detector<400||detector>413))throw std::runtime_error("Use TS detector 400..413, or 0 for all");
 if(maxevents==0||maxevents<-1)throw std::runtime_error("Use a positive validation event limit or -1");
 TFile fi(digitised),fr(replayed);if(fi.IsZombie()||fr.IsZombie())throw std::runtime_error("Cannot open TS validation files");
 TTree *in=nullptr,*out=nullptr;fi.GetObject("trigscint_digi",in);fr.GetObject("T",out);
 if(!in||!out)throw std::runtime_error("Missing TS input/replay tree");
 auto formula=[](TTree* t,const char* name,const char* expr){auto f=std::make_unique<TTreeFormula>(name,expr,t);if(f->GetNdim()<=0)throw std::runtime_error(std::string("Missing TS validation expression: ")+expr);return f;};
 auto ie=formula(in,"tsv_ie","evnum"),iid=formula(in,"tsv_iid","detid"),icopy=formula(in,"tsv_ic","copyid"),npe=formula(in,"tsv_npe","npe_poiss"),model=formula(in,"tsv_model","waveform_valid"),qpe=formula(in,"tsv_qpe","qpe_c"),ped=formula(in,"tsv_ped","pedestal_mean_adc"),hit=formula(in,"tsv_hit","first_hit_time_ns");
 auto iraw=formula(in,"tsv_iraw","adc_int"),oraw=formula(out,"tsv_oraw","moller.ts.raw_integral");
 auto oe=formula(out,"tsv_oe","moller.ts.evnum"),oid=formula(out,"tsv_oid","moller.ts.detid"),ocopy=formula(out,"tsv_oc","moller.ts.copyid"),ro=formula(out,"tsv_ro","moller.ts.readout"),found=formula(out,"tsv_fo","moller.ts.found"),charge=formula(out,"tsv_q","moller.ts.charge_pc"),base=formula(out,"tsv_base","moller.ts.baseline"),clip=formula(out,"tsv_clip","moller.ts.clipped"),sat=formula(out,"tsv_sat","moller.ts.saturated"),time=formula(out,"tsv_time","moller.ts.time_ns"),tv=formula(out,"tsv_tv","moller.ts.time_valid");
 std::map<Long64_t,Long64_t> replay_entries;
 for(Long64_t i=0;i<out->GetEntries();++i){out->GetEntry(i);oe->GetNdata();if(!replay_entries.emplace(static_cast<Long64_t>(oe->EvalInstance()),i).second)throw std::runtime_error("Duplicate TS replay event ID");}
 TH1::AddDirectory(false);
 TH2D correlation("ts_charge_correlation","Detected, unclipped, unsaturated;Full generated charge [pC];Reconstructed window charge [pC]",120,0,40,140,-5,40);
 TH1D residual("ts_charge_residual","Conditional charge residual;Q_{reco}-Q_{generated} [pC];Selected channel-events",120,-6,6);
 TH1D baseline("ts_baseline_residual","Baseline accuracy;Estimated - injected baseline [ADC counts];Channel-events",120,-10,10);
 TH1D total("ts_eff_total",";Generated PE;Eligible channel-events",101,-.5,100.5),passed("ts_eff_found",";Generated PE;Found channel-events",101,-.5,100.5);
 TH1D timing("ts_time_vs_hit","Selected replay time minus earliest saved hit;Time difference [ns];Selected channel-events",120,-20,40);
 TH1D spectrum("ts_reco_charge","Selected reconstructed TS charge;Charge [pC];Selected channel-events",120,-5,40);
 Long64_t matched=0,unmatched=0,channelpairs=0,missing=0,zero=0,falsepos=0,positive=0,invalid=0,invalid_found=0,eligible=0,detected=0,selected=0,excluded=0,unknown=0;
 std::set<Long64_t> inputevents;double sum=0,square=0;
 std::vector<TTreeFormula*> inf={iid.get(),icopy.get(),npe.get(),model.get(),qpe.get(),ped.get(),hit.get(),iraw.get()};
 std::vector<TTreeFormula*> outf={oid.get(),ocopy.get(),ro.get(),found.get(),charge.get(),base.get(),clip.get(),sat.get(),time.get(),tv.get(),oraw.get()};
 const Long64_t inputLimit=maxevents<0?in->GetEntries():std::min(maxevents,in->GetEntries());
 if(inputLimit==0)throw std::runtime_error("Empty TS validation input");
 for(Long64_t i=0;i<inputLimit;++i) {
  if(i%5000==0)std::cout<<"TS replay validation: "<<i<<" / "<<inputLimit<<'\n';
  in->GetEntry(i);ie->GetNdata();const auto event=static_cast<Long64_t>(ie->EvalInstance());
  if(!inputevents.insert(event).second)throw std::runtime_error("Duplicate TS input event ID");
  auto r=replay_entries.find(event);if(r==replay_entries.end()){++unmatched;continue;}
  ++matched;out->GetEntry(r->second);
  int ni=iid->GetNdata(),no=oid->GetNdata();
  for(auto f:inf)if(f->GetNdata()!=ni)throw std::runtime_error("Unaligned TS validation input");
  for(auto f:outf)if(f->GetNdata()!=no)throw std::runtime_error("Unaligned TS validation replay");
  std::map<int,int> indices;for(int k=0;k<no;++k)if(!indices.emplace(int(oid->EvalInstance(k)),k).second)throw std::runtime_error("Duplicate TS replay detector ID");
  std::set<int> seen;
  for(int k=0;k<ni;++k) {
   int id=iid->EvalInstance(k);if(!seen.insert(id).second)throw std::runtime_error("Multiple TS copies per detector not supported by replay");
   if(detector!=0&&id!=detector)continue;
   auto a=indices.find(id);if(a==indices.end())throw std::runtime_error("Missing configured TS replay channel");
   const int j=a->second;++channelpairs;
   if(ocopy->EvalInstance(j)!=icopy->EvalInstance(k))throw std::runtime_error("TS input/replay copy ID mismatch");
   bool available=ro->EvalInstance(j)!=0,yes=found->EvalInstance(j)!=0,valid=model->EvalInstance(k)!=0;
   const double n=npe->EvalInstance(k),q=charge->EvalInstance(j);
   if(!available)++missing;
   else if(oraw->EvalInstance(j)!=iraw->EvalInstance(k))throw std::runtime_error("TS Podd decoded ADC sum differs from input channel waveform");
   if(available&&std::isfinite(base->EvalInstance(j)))baseline.Fill(base->EvalInstance(j)-ped->EvalInstance(k));
   if(n<0){++unknown;continue;}
   if(n==0){if(available&&valid){++zero;if(yes)++falsepos;}continue;}
   ++positive;
   if(!valid){++invalid;if(yes)++invalid_found;continue;} // Do not call missing modeled signal a missed physical pulse.
   ++eligible;total.Fill(n);if(available&&yes){++detected;passed.Fill(n);}else continue;
   if(clip->EvalInstance(j)||sat->EvalInstance(j)){++excluded;continue;}
   const double truth=n*qpe->EvalInstance(k)*1e12;
   if(!std::isfinite(truth)||truth<=0||!std::isfinite(q))throw std::runtime_error("Invalid TS charge truth/comparison");
   ++selected;double delta=q-truth;sum+=delta;square+=delta*delta;
   correlation.Fill(truth,q);residual.Fill(delta);spectrum.Fill(q);
   if(tv->EvalInstance(j)&&std::isfinite(hit->EvalInstance(k))&&std::isfinite(time->EvalInstance(j)))timing.Fill(time->EvalInstance(j)-hit->EvalInstance(k));
  }
 }
 Long64_t unmatchreplay=0;for(const auto& item:replay_entries)if(!inputevents.count(item.first))++unmatchreplay;
 TGraphAsymmErrors efficiency(&passed,&total);efficiency.SetName("ts_detection_efficiency");efficiency.SetTitle("Model-valid positive PE;Generated PE;Pulse detection efficiency");
 TCanvas canvas("ts_validation","TS replay validation",1500,850);canvas.Divide(3,2);
 canvas.cd(1);correlation.Draw("COLZ");TLine diagonal(0,0,40,40);diagonal.SetLineColor(2);diagonal.Draw();
 canvas.cd(2);residual.Draw();canvas.cd(3);baseline.Draw();canvas.cd(4);efficiency.Draw("AP");canvas.cd(5);timing.Draw();canvas.cd(6);spectrum.Draw();
 canvas.SaveAs((std::string(prefix)+".png").c_str());canvas.SaveAs((std::string(prefix)+".pdf").c_str());
 TFile products((std::string(prefix)+".root").c_str(),"RECREATE");if(products.IsZombie())throw std::runtime_error("Cannot write TS validation ROOT products");
 correlation.Write();residual.Write();baseline.Write();total.Write();passed.Write();efficiency.Write();timing.Write();spectrum.Write();products.Close();
 const double nan=std::numeric_limits<double>::quiet_NaN();
 double mean=selected?sum/selected:nan,sd=selected>1?std::sqrt(std::max(0.,(square-sum*sum/selected)/(selected-1))):nan;
 std::cout<<"Module selection: "<<detector<<" (0 means all); input entries checked: "<<inputLimit<<" / "<<in->GetEntries()<<"\n"
 <<"Matched events: "<<matched<<"; unmatched input/replay: "<<unmatched<<" / "<<unmatchreplay
 <<"\nMatched channel-events: "<<channelpairs<<"; absent replay waveforms: "<<missing
 <<"\nPositive-PE channel-events: "<<positive<<"; invalid modeled signal excluded: "<<invalid<<" (found in invalid group: "<<invalid_found<<")"
 <<"\nEligible model-valid positive PE: "<<eligible<<"; detected: "<<detected
 <<"\nKnown zero-PE waveform channels: "<<zero<<"; false detections: "<<falsepos<<" ("<<(zero?100.*falsepos/zero:0)<<"%)"
 <<"\nUnknown-energy rows excluded: "<<unknown<<"; detected eligible pulses excluded for clipping/saturation: "<<excluded
 <<"\nSelected charge comparisons: "<<selected<<"; residual mean / sample standard deviation: "<<mean<<" / "<<sd<<" pC\n"
 <<"Charge reference is full generated npe_poiss*qpe_c, not expected charge inside the integration window.\n"
 <<"Charge residuals are conditional on detection; no cut on reconstructed charge.\n"
 <<"Time difference includes emission, TTS, pulse shape and estimator offsets; it is not intrinsic timing resolution.\n"
 <<"This validates waveform reconstruction, not the physical TS energy proxy or light yield.\n";
}
