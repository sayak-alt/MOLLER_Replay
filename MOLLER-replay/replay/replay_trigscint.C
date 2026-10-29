#include <TROOT.h>
#include <TInterpreter.h>
#include <TSystem.h>
#include <TString.h>
#include <stdexcept>
#include <string>
namespace {
std::string ts_quote(const char* s){
 std::string out="\"";
 for(const char* p=s;*p;++p){if(*p=='\\'||*p=='\"')out+='\\';out+=*p;}
 return out+"\"";
}
}
void replay_trigscint(const char* input,const char* output="replayed_trigscint.root",Long64_t maxevents=100){
 if(!input||!output)throw std::runtime_error("Missing input/output");
 if(gSystem->AccessPathName(input))throw std::runtime_error("Input ROOT file does not exist");
 const char* offline=gSystem->Getenv("MOLLER_OFFLINE");
 const char* package=gSystem->Getenv("MOLLER_REPLAY");
 if(!offline||!package)throw std::runtime_error("Source setup.csh first");
 TString include=TString::Format("%s/include",offline);gInterpreter->AddIncludePath(include);
 if(gSystem->Load(TString::Format("%s/lib64/libmollertrigscint.so",offline))<0)
  throw std::runtime_error("Cannot load libmollertrigscint.so; build/install MOLLER-offline/trigscint first");
 Int_t error=0;
 gROOT->ProcessLine(TString::Format(".L %s/replay/replay_moller_trigscint.C",package).Data(),&error);
 if(error)throw std::runtime_error("Cannot load replay macro");
 std::string call="replay_moller_trigscint("+ts_quote(input)+","+ts_quote(output)+","+std::to_string(maxevents)+");";
 gROOT->ProcessLine(call.c_str(),&error);
 if(error)throw std::runtime_error("Replay macro execution failed");
}
