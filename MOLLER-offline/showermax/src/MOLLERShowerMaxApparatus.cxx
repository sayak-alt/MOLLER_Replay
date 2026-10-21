#include "MOLLERShowerMaxApparatus.h"
#include "MOLLERShowerMaxDetector.h"
ClassImp(MOLLERShowerMaxApparatus)
MOLLERShowerMaxApparatus::MOLLERShowerMaxApparatus(const char* name):THaApparatus(name,"MOLLER Shower-Max"){
 fSM=new MOLLERShowerMaxDetector("sm",this);AddDetector(fSM);
}
Int_t MOLLERShowerMaxApparatus::Reconstruct(){return fSM->Reconstruct();}
