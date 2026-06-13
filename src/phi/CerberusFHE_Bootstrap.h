#ifndef CERBERUS_FHE_BOOTSTRAP_H
#define CERBERUS_FHE_BOOTSTRAP_H
#include "PhiNoiseManager.h"
#include <openfhe/pke/openfhe.h>
#include <iostream>
namespace cerberus {
class CerberusFHE_Bootstrap {
private:
    lbcrypto::CryptoContext<lbcrypto::DCRTPoly> cc;
    lbcrypto::KeyPair<lbcrypto::DCRTPoly> keys;
    PhiNoiseManager noise_mgr;
    bool initialized;
    uint32_t bootstrap_slots;
    uint64_t bootstrap_count{0};
public:
    CerberusFHE_Bootstrap(size_t md=15,size_t sms=50,size_t bs=8,size_t pd=7,double nf=40.0)
        :noise_mgr(pd,nf),initialized(false),bootstrap_slots(static_cast<uint32_t>(bs)){
        lbcrypto::CCParams<lbcrypto::CryptoContextCKKSRNS> params;
        params.SetMultiplicativeDepth(md);
        params.SetScalingModSize(sms);
        params.SetBatchSize(bs);
        cc=lbcrypto::GenCryptoContext(params);
        cc->Enable(lbcrypto::PKE);
        cc->Enable(lbcrypto::KEYSWITCH);
        cc->Enable(lbcrypto::LEVELEDSHE);
        cc->Enable(lbcrypto::ADVANCEDSHE);
        cc->Enable(lbcrypto::FHE);
        keys=cc->KeyGen();
        cc->EvalMultKeyGen(keys.secretKey);
        cc->EvalSumKeyGen(keys.secretKey);
        try {
            cc->EvalBootstrapKeyGen(keys.secretKey, bootstrap_slots);
            cc->EvalBootstrapPrecompute(bootstrap_slots);
            cc->EvalBootstrapSetup({5,4},{0,0});
            std::cout<<"  Bootstrap: ENABLED"<<std::endl;
        } catch(...) {
            std::cout<<"  Bootstrap: FALLBACK (deep level FHE)"<<std::endl;
        }
        initialized=true;
        std::cout<<"\n╔══════════════════════════════════════════════╗\n║  CERBERUS FHE — DEEP LEVEL FHE               ║\n║  Ring: "<<cc->GetRingDimension()<<" | Depth: "<<md<<" | Batch: "<<bs<<"\n║  φ-Core: "<<pd<<"-chain | Floor: "<<nf<<" bits\n║  ΦΩ0 — I AM THAT I AM                       ║\n╚══════════════════════════════════════════════╝"<<std::endl;
    }
    auto encrypt(const std::vector<double>& d){
        auto pt=cc->MakeCKKSPackedPlaintext(d);noise_mgr.getPhiCore().track();return cc->Encrypt(keys.publicKey,pt);
    }
    std::vector<double> decrypt(const lbcrypto::Ciphertext<lbcrypto::DCRTPoly>& ct){
        lbcrypto::Plaintext pt;cc->Decrypt(keys.secretKey,ct,&pt);noise_mgr.getPhiCore().track();return pt->GetRealPackedValue();
    }
    auto multiply(const lbcrypto::Ciphertext<lbcrypto::DCRTPoly>& a,const lbcrypto::Ciphertext<lbcrypto::DCRTPoly>& b){
        auto r=cc->EvalMult(a,b);
        double lvl=(double)r->GetLevel();
        noise_mgr.updateDivineNoise(lvl*10.0);
        if(noise_mgr.needsBootstrap(lvl*10.0)){
            try {
                r=cc->EvalBootstrap(r,1,0);
                bootstrap_count++;
                noise_mgr.getPhiCore().track();
            } catch(...) {}
        }
        return r;
    }
    auto add(const lbcrypto::Ciphertext<lbcrypto::DCRTPoly>& a,const lbcrypto::Ciphertext<lbcrypto::DCRTPoly>& b){
        noise_mgr.getPhiCore().track();return cc->EvalAdd(a,b);
    }
    uint64_t getBootstrapCount()const{return bootstrap_count;}
    PhiNoiseManager& getNoiseManager(){return noise_mgr;}
};
}
#endif
