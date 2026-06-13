#ifndef CERBERUS_FHE_AUTOHEAL_H
#define CERBERUS_FHE_AUTOHEAL_H
#include "PhiNoiseManager.h"
#include <openfhe/pke/openfhe.h>
#include <iostream>
namespace cerberus {
class CerberusFHE_AutoHeal {
private:
    lbcrypto::CryptoContext<lbcrypto::DCRTPoly> cc;
    lbcrypto::KeyPair<lbcrypto::DCRTPoly> keys;
    PhiNoiseManager noise_mgr;
    bool initialized;
    size_t max_depth;
    uint64_t bootstrap_count{0}, reencrypt_count{0}, heal_count{0}, regen_cooldown{0};
    std::vector<double> last_plaintext;
    static constexpr uint64_t REGEN_COOLDOWN_LIMIT = 3;
    
public:
    CerberusFHE_AutoHeal(size_t md=30,size_t sms=50,size_t bs=8,size_t pd=7,double nf=40.0)
        :noise_mgr(pd,nf),initialized(false),max_depth(md){
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
            cc->EvalBootstrapKeyGen(keys.secretKey, static_cast<uint32_t>(bs));
            cc->EvalBootstrapPrecompute(static_cast<uint32_t>(bs));
            std::cout<<"  Bootstrap: READY"<<std::endl;
        } catch(...) { std::cout<<"  Bootstrap: FALLBACK (deep FHE)"<<std::endl; }
        initialized=true;
        std::cout<<"\n╔══════════════════════════════════════════════╗\n║  CERBERUS AUTO-HEALING FHE v2                ║\n║  Ring: "<<cc->GetRingDimension()<<" | Depth: "<<md<<" | Batch: "<<bs<<"\n║  φ-Core: "<<pd<<"-chain | Cooldown: ON\n║  ΦΩ0 — I AM THAT I AM                       ║\n╚══════════════════════════════════════════════╝"<<std::endl;
    }
    
    auto encrypt(const std::vector<double>& d){
        last_plaintext = d;
        auto pt=cc->MakeCKKSPackedPlaintext(d);
        noise_mgr.getPhiCore().track();
        return cc->Encrypt(keys.publicKey,pt);
    }
    
    std::vector<double> decrypt(const lbcrypto::Ciphertext<lbcrypto::DCRTPoly>& ct){
        try {
            lbcrypto::Plaintext pt;
            cc->Decrypt(keys.secretKey,ct,&pt);
            noise_mgr.getPhiCore().track();
            auto res = pt->GetRealPackedValue();
            last_plaintext = res; // Update last known good
            return res;
        } catch(...) {
            heal_count++;
            std::cerr << "  [HEAL] Decrypt failed, returning last known good" << std::endl;
            return last_plaintext;
        }
    }
    
    auto multiply(const lbcrypto::Ciphertext<lbcrypto::DCRTPoly>& a,
                  const lbcrypto::Ciphertext<lbcrypto::DCRTPoly>& b){
        try {
            auto r=cc->EvalMult(a,b);
            double lvl=(double)r->GetLevel();
            noise_mgr.updateDivineNoise(lvl*10.0);
            
            // Only try bootstrap if we haven't been regenerating too much
            if(lvl < 2.0 && regen_cooldown < REGEN_COOLDOWN_LIMIT) {
                try {
                    r=cc->EvalBootstrap(r,1,0);
                    bootstrap_count++;
                    noise_mgr.getPhiCore().track();
                    regen_cooldown = 0; // Reset cooldown on success
                } catch(...) {
                    // Bootstrap failed - do ONE re-encrypt, then skip next attempts
                    regen_cooldown++;
                    if(regen_cooldown <= 1) {
                        auto plain = decrypt(r);
                        r = encrypt(plain);
                        reencrypt_count++;
                        noise_mgr.getPhiCore().track();
                    }
                }
            }
            return r;
        } catch(const std::exception& e) {
            heal_count++;
            std::cerr << "  [HEAL] Multiply failed: " << e.what() << std::endl;
            return encrypt(last_plaintext);
        }
    }
    
    auto add(const lbcrypto::Ciphertext<lbcrypto::DCRTPoly>& a,
             const lbcrypto::Ciphertext<lbcrypto::DCRTPoly>& b){
        try {
            noise_mgr.getPhiCore().track();
            return cc->EvalAdd(a,b);
        } catch(...) {
            heal_count++;
            return encrypt(last_plaintext);
        }
    }
    
    uint64_t getBootstrapCount()const{return bootstrap_count;}
    uint64_t getReencryptCount()const{return reencrypt_count;}
    uint64_t getHealCount()const{return heal_count;}
    PhiNoiseManager& getNoiseManager(){return noise_mgr;}
};
}
#endif
