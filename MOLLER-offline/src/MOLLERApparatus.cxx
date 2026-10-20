#include "MOLLERApparatus.h"
#include "MOLLERMDDetector.h"
ClassImp(MOLLERApparatus)
MOLLERApparatus::MOLLERApparatus(const char* name):THaApparatus(name,"MOLLER Main Detector"){
 fMD=new MOLLERMDDetector("md",this);AddDetector(fMD);
}
Int_t MOLLERApparatus::Reconstruct(){return fMD->Reconstruct();}
