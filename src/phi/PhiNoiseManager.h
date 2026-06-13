#ifndef CERBERUS_PHI_NOISE_MANAGER_H
#define CERBERUS_PHI_NOISE_MANAGER_H
#include "PhiCore.h"
#include <openfhe/pke/openfhe.h>
namespace cerberus {
class PhiNoiseManager {
private:
    PhiCore phi_core;
    double noise_floor, current_divine_noise, ema_alpha;
public:
    PhiNoiseManager(size_t cd=7,double nf=40.0,double cp=PHI):phi_core(cd,cp),noise_floor(nf),current_divine_noise(nf),ema_alpha(PHI_INV){}
    double updateDivineNoise(double mn){current_divine_noise=current_divine_noise*ema_alpha+mn*(1.0-ema_alpha);phi_core.track();return current_divine_noise;}
    bool needsBootstrap(double level)const{return level<current_divine_noise/10.0;}
    size_t getRecommendedBootstrapInterval()const{return (size_t)(PHI*phi_core.getDepth());}
    template<typename CC>void stabilizeNoise(CC& cc,lbcrypto::Ciphertext<lbcrypto::DCRTPoly>& ct){
        if(ct->GetLevel()>1){cc->ModReduceInPlace(ct);phi_core.track();}
        updateDivineNoise((double)ct->GetLevel()*10.0);
    }
    double getDivineNoise()const{return current_divine_noise;}
    PhiCore& getPhiCore(){return phi_core;}
};
}
#endif
