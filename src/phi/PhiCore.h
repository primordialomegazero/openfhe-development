#ifndef CERBERUS_PHI_CORE_H
#define CERBERUS_PHI_CORE_H
#include <vector>
#include <cmath>
#include <atomic>
namespace cerberus {
constexpr double PHI = 1.6180339887498948482;
constexpr double PHI_INV = 0.6180339887498948482;
class PhiCore {
private:
    size_t chain_depth;
    std::vector<double> noise_chain, mod_chain;
    std::atomic<size_t> depth{0};
    std::atomic<uint64_t> injections{0};
    double phi_custom, phi_inv_custom;
public:
    PhiCore(size_t d=7, double cp=PHI):chain_depth(d),phi_custom(cp),phi_inv_custom(1.0/cp),noise_chain(d,0.0),mod_chain(d,0.0){
        double v=phi_custom,w=phi_inv_custom;
        for(size_t i=0;i<chain_depth;i++){noise_chain[i]=v;v=v*phi_inv_custom+std::sin(v*phi_custom)*std::pow(phi_inv_custom,i+1);mod_chain[i]=w;w=w*phi_custom+std::cos(w*phi_inv_custom)*std::pow(phi_custom,-(double)(i+1));}
    }
    void track(){depth++;injections++;}
    double noiseAt(size_t i)const{return noise_chain[i%chain_depth];}
    double modAt(size_t i)const{return mod_chain[i%chain_depth];}
    uint64_t getInjections()const{return injections;}
    size_t getDepth()const{return chain_depth;}
    double getPhi()const{return phi_custom;}
};
}
#endif
