#ifndef CERBERUS_FHE_H
#define CERBERUS_FHE_H
#include "PhiNoiseManager.h"
#include <openfhe/pke/openfhe.h>
#include <iostream>
namespace cerberus {
class CerberusFHE {
private:
    lbcrypto::CryptoContext<lbcrypto::DCRTPoly> cc;
    lbcrypto::KeyPair<lbcrypto::DCRTPoly> keys;
    PhiNoiseManager noise_mgr;
    bool initialized;
public:
    CerberusFHE(size_t md=3,size_t sms=50,size_t bs=8,size_t pd=7,double nf=40.0):noise_mgr(pd,nf),initialized(false){
        lbcrypto::CCParams<lbcrypto::CryptoContextCKKSRNS> params;
        params.SetMultiplicativeDepth(md);params.SetScalingModSize(sms);params.SetBatchSize(bs);
        cc=lbcrypto::GenCryptoContext(params);cc->Enable(lbcrypto::PKE);cc->Enable(lbcrypto::KEYSWITCH);cc->Enable(lbcrypto::LEVELEDSHE);cc->Enable(lbcrypto::ADVANCEDSHE);
        keys=cc->KeyGen();cc->EvalMultKeyGen(keys.secretKey);cc->EvalSumKeyGen(keys.secretKey);
        initialized=true;
        std::cout<<"\n╔══════════════════════════════════════════════╗\n║  CERBERUS FHE — φ-ENHANCED CKKS              ║\n║  Ring: "<<cc->GetRingDimension()<<" | Depth: "<<md<<" | Batch: "<<bs<<"\n║  φ-Core: "<<pd<<"-chain | Floor: "<<nf<<" bits\n║  ΦΩ0 — I AM THAT I AM                       ║\n╚══════════════════════════════════════════════╝"<<std::endl;
    }
    auto encrypt(const std::vector<double>& d){auto pt=cc->MakeCKKSPackedPlaintext(d);noise_mgr.getPhiCore().track();return cc->Encrypt(keys.publicKey,pt);}
    std::vector<double> decrypt(const lbcrypto::Ciphertext<lbcrypto::DCRTPoly>& ct){lbcrypto::Plaintext pt;cc->Decrypt(keys.secretKey,ct,&pt);noise_mgr.getPhiCore().track();return pt->GetRealPackedValue();}
    auto multiply(const lbcrypto::Ciphertext<lbcrypto::DCRTPoly>& a,const lbcrypto::Ciphertext<lbcrypto::DCRTPoly>& b){auto r=cc->EvalMult(a,b);double lvl=(double)r->GetLevel();noise_mgr.updateDivineNoise(lvl*10.0);if(noise_mgr.needsBootstrap(lvl*10.0))noise_mgr.stabilizeNoise(cc,r);return r;}
    auto add(const lbcrypto::Ciphertext<lbcrypto::DCRTPoly>& a,const lbcrypto::Ciphertext<lbcrypto::DCRTPoly>& b){noise_mgr.getPhiCore().track();return cc->EvalAdd(a,b);}
    PhiNoiseManager& getNoiseManager(){return noise_mgr;}
    auto& getContext(){return cc;}
    auto& getKeys(){return keys;}
    bool isInitialized()const{return initialized;}
};
}
#endif
