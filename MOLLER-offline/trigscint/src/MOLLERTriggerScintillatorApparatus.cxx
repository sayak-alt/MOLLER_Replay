#include "MOLLERTriggerScintillatorApparatus.h"
#include "MOLLERTriggerScintillatorDetector.h"
ClassImp(MOLLERTriggerScintillatorApparatus)
MOLLERTriggerScintillatorApparatus::MOLLERTriggerScintillatorApparatus(const char* name):THaApparatus(name,"MOLLER Trigger Scintillator"){
 fTS=new MOLLERTriggerScintillatorDetector("ts",this);AddDetector(fTS);
}
Int_t MOLLERTriggerScintillatorApparatus::Reconstruct(){return fTS->Reconstruct();}
