#ifndef MOLLER_TRIGGER_SCINTILLATOR_DIGITIZER_H
#define MOLLER_TRIGGER_SCINTILLATOR_DIGITIZER_H

#include <TRandom3.h>
#include <TTree.h>
#include <vector>

// Provisional linear PE response to selected REMOLL sum.edep records.
// A detector volume is identified by both REMOLL detector and copy ID.
class MOLLERTriggerScintillatorDigitizer {
public:
 struct Config {
  std::vector<int> detector_ids = {400,401,402,403,404,405,406,407,408,409,410,411,412,413};
  double pe_per_mev = 125.0; // Effective detected PE/MeV, provisional.
 };
 struct Hit {int det,copy,pid;double time_ns,edep_mev;};
 struct Deposit {int det,copy;double edep_mev;};
 void SetConfig(const Config& config);
 void SetRandomSeed(unsigned int seed) { fRand.SetSeed(seed); }
 void SetEventNumber(Long64_t number) { fEvnum=number; }
 void BookBranches(TTree* tree);
 void DigitizeEvent(const std::vector<Hit>& hits,const std::vector<Deposit>& sums);
private:
 Config fCfg;
 TRandom3 fRand;
 Long64_t fEvnum=0;
 int physical_scoring_validated=0; // Deliberately not configurable until validated.
 std::vector<int> detid,copyid,time_valid,sum_valid,npe_poiss;
 std::vector<double> edep_mev,meanpe,first_hit_time_ns;
 std::vector<int> hit_detid,hit_copyid,hit_pid;
 std::vector<double> hit_time_ns,hit_edep_mev;
};
#endif
