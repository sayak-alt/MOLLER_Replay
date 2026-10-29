#include "MOLLERTriggerScintillatorDigitizer.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

static void require(bool ok,const char* message) {
  if(!ok) throw std::runtime_error(message);
}
int main() {
  using D=MOLLERTriggerScintillatorDigitizer;
  D d; D::Config cfg; d.SetConfig(cfg); d.SetRandomSeed(12345);
  TTree tree("test","Synthetic trigger scintillator response"); d.BookBranches(&tree);
  d.SetEventNumber(0);
  // Two copy IDs are kept separate; hit.edep does not determine PE.
  d.DigitizeEvent({{400,7,11,9,0},{400,7,11,5,0},{400,8,11,3,0},{999,0,11,0,0}},
                  {{400,7,2},{400,8,0},{413,0,1}}); tree.Fill();
  d.SetEventNumber(1);d.DigitizeEvent({{401,0,11,8,0}},{});tree.Fill();
  d.SetEventNumber(2);d.DigitizeEvent({},{});tree.Fill();
  for(int i=0;i<20000;++i) {d.SetEventNumber(i+3);d.DigitizeEvent({},{{400,0,2}});tree.Fill();}
  bool caught=false;
  try{d.DigitizeEvent({},{{400,0,1},{400,0,2}});}catch(const std::runtime_error&){caught=true;}
  require(caught,"Duplicate sums must be rejected");
  caught=false;try{d.DigitizeEvent({},{{400,0,-1}});}catch(const std::runtime_error&){caught=true;}
  require(caught,"Negative energy must be rejected");
  caught=false;cfg.pe_per_mev=0;try{d.SetConfig(cfg);}catch(const std::runtime_error&){caught=true;}
  require(caught,"Invalid yield must be rejected");
  tree.ResetBranchAddresses();
  std::vector<int> *ids=nullptr,*copies=nullptr,*npe=nullptr,*sv=nullptr,*tv=nullptr,*hits=nullptr;
  std::vector<double> *mu=nullptr,*time=nullptr;
  double yield=0;int physical=1;
  require(tree.SetBranchAddress("detid",&ids)>=0,"Missing detid");
  tree.SetBranchAddress("copyid",&copies);tree.SetBranchAddress("npe_poiss",&npe);
  tree.SetBranchAddress("sum_valid",&sv);tree.SetBranchAddress("time_valid",&tv);
  tree.SetBranchAddress("meanpe",&mu);tree.SetBranchAddress("first_hit_time_ns",&time);
  tree.SetBranchAddress("hit_detid",&hits);tree.SetBranchAddress("pe_per_mev",&yield);
  tree.SetBranchAddress("physical_scoring_validated",&physical);
  tree.GetEntry(0);
  require(ids->size()==3&&(*ids)[0]==400&&(*copies)[0]==7&&(*copies)[1]==8,"Detector/copy grouping");
  require((*mu)[0]==250&&(*mu)[1]==0&&(*npe)[1]==0&&(*mu)[2]==125,"Energy-to-PE calculation");
  require((*time)[0]==5&&(*tv)[0]==1&&(*tv)[2]==0&&std::isnan((*time)[2]),"Recorded hit times");
  require(hits->size()==3&&yield==125&&physical==0,"Selection and provenance");
  tree.GetEntry(1);require(ids->size()==1&&(*sv)[0]==0&&(*npe)[0]==-1&&std::isnan((*mu)[0])&&(*time)[0]==8,"Missing sum is not zero energy");
  tree.GetEntry(2);require(ids->empty()&&hits->empty(),"Empty event/reset");
  double sum=0,square=0;
  for(int i=3;i<20003;++i){tree.GetEntry(i);double x=(*npe)[0];sum+=x;square+=x*x;}
  double mean=sum/20000,variance=(square-sum*sum/20000)/19999;
  require(std::abs(mean-250)<0.8&&std::abs(variance-250)<10,"Poisson mean/variance");
  tree.ResetBranchAddresses();
  std::cout<<"PASS TS grouping, PE, missing/zero records, hit times, reset, provenance, Poisson: "<<mean<<" / "<<variance<<"\n";
}
