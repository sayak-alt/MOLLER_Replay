#ifndef MOLLER_TS_APPARATUS_H
#define MOLLER_TS_APPARATUS_H
#include "THaApparatus.h"
class MOLLERTriggerScintillatorDetector;
class MOLLERTriggerScintillatorApparatus:public THaApparatus {
public:
 MOLLERTriggerScintillatorApparatus(const char* name="moller");
 Int_t Reconstruct() override;
protected:
 Int_t ReadDatabase(const TDatime&) override{return 0;}
 Int_t ReadRunDatabase(const TDatime&) override{return 0;}
private:
 MOLLERTriggerScintillatorDetector* fTS=nullptr; //!
 ClassDefOverride(MOLLERTriggerScintillatorApparatus,0)
};
#endif
