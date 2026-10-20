#include "MOLLERMDPodd.h"
#include "MOLLERApparatus.h"
#include "THaAnalyzer.h"
#include "THaInterface.h"
#include "THaGlobals.h"
#include "TList.h"
#include <TSystem.h>
#include <memory>
#include <stdexcept>
void replay_moller_md(const char* input,const char* output="replayed_md.root",Long64_t maxevents=100){
 if(!input||!output||TString(input)==output)throw std::runtime_error("Input/output paths must differ");
 if(gHaApps->GetSize()||gHaPhysics->GetSize())
  throw std::runtime_error("Use a fresh analyzer process without other replay apparatus/modules");
 const char* replay=gSystem->Getenv("MOLLER_REPLAY");
 if(!replay)throw std::runtime_error("Source setup.csh first");
 THaInterface::SetDecoder(MOLLERMDDecoder::Class());
 MOLLERMDRun run(input);run.SetFirstEvent(0);
 if(maxevents>0)run.SetLastEvent(maxevents);
 auto app=std::make_unique<MOLLERApparatus>("moller");
 gHaApps->Add(app.get());
 auto ana=std::make_unique<THaAnalyzer>();
 ana->SetOutFile(output);
 ana->SetOdefFile(TString::Format("%s/replay/replay_moller_md.odef",replay));
 ana->SetVerbosity(2);
 const Long64_t status=ana->Process(&run);
 ana->Close();ana.reset();
 gHaApps->Remove(app.get());app.reset();
 if(status<0)throw std::runtime_error("MOLLER MD replay failed; inspect analyzer log");
}
