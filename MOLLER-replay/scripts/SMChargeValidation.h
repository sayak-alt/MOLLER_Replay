#ifndef SM_CHARGE_VALIDATION_HELPERS_H
#define SM_CHARGE_VALIDATION_HELPERS_H
#include <cmath>
#include <map>
#include <stdexcept>
#include <vector>
namespace SMChargeValidation {
inline std::map<int,size_t> ChannelIndex(const std::vector<int>& ids) {
 std::map<int,size_t> index;
 for(size_t k=0;k<ids.size();++k) {
  if(ids[k]<73001||ids[k]>73028||!index.emplace(ids[k],k).second)
   throw std::runtime_error("Invalid/duplicate SM detector ID");
 }
 return index;
}
inline double TruthCharge(int npe,double qpe_c) {
 if(npe<0||!std::isfinite(qpe_c)||qpe_c<=0)
  throw std::runtime_error("Invalid generated PE or per-channel qpe_c");
 const double result=npe*qpe_c*1e12;
 if(!std::isfinite(result))throw std::runtime_error("Nonfinite truth charge");
 return result;
}
struct Moments {
 long long n=0; double mean=0,m2=0;
 void Add(double x) {
  if(!std::isfinite(x))throw std::runtime_error("Nonfinite residual");
  ++n;double d=x-mean;mean+=d/n;m2+=d*(x-mean);
 }
 double StdDev()const{return n>1?std::sqrt(m2/(n-1)):0.;}
};
}
#endif
