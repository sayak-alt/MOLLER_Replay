#ifndef MOLLER_APPARATUS_H
#define MOLLER_APPARATUS_H
#include "THaApparatus.h"
class MOLLERMDDetector;
class MOLLERApparatus : public THaApparatus {
public:
 MOLLERApparatus(const char* name="moller");
 Int_t Reconstruct() override;
protected:
 Int_t ReadDatabase(const TDatime&) override {return 0;}
 Int_t ReadRunDatabase(const TDatime&) override {return 0;}
private:
 MOLLERMDDetector* fMD=nullptr; //! Owned by THaApparatus detector list
 ClassDefOverride(MOLLERApparatus,0)
};
#endif
