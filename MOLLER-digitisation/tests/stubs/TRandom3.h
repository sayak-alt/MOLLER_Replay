#pragma once
#include <random>
class TRandom3 {std::mt19937 engine{1};public:
 void SetSeed(unsigned int seed){engine.seed(seed);}
 int Poisson(double mean){return std::poisson_distribution<int>(mean)(engine);}
 double Gaus(double mean,double sigma){return std::normal_distribution<double>(mean,sigma)(engine);}
};
