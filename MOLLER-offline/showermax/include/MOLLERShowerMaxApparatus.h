#ifndef MOLLER_SM_APPARATUS_H
#define MOLLER_SM_APPARATUS_H
#include "THaApparatus.h"
class MOLLERShowerMaxDetector;
class MOLLERShowerMaxApparatus:public THaApparatus {
public:
 MOLLERShowerMaxApparatus(const char* name="moller");
 Int_t Reconstruct() override;
protected:
 Int_t ReadDatabase(const TDatime&) override{return 0;}
 Int_t ReadRunDatabase(const TDatime&) override{return 0;}
private:
 MOLLERShowerMaxDetector* fSM=nullptr; //!
 ClassDefOverride(MOLLERShowerMaxApparatus,0)
};
#endif
