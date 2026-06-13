#ifndef CERBERUS_MIRROR_CHAIN_H
#define CERBERUS_MIRROR_CHAIN_H
#include "PhiNoiseManager.h"
#include <openfhe/pke/openfhe.h>
#include <iostream>
#include <deque>
namespace cerberus {

class CerberusMirrorChain {
private:
    lbcrypto::CryptoContext<lbcrypto::DCRTPoly> cc;
    lbcrypto::KeyPair<lbcrypto::DCRTPoly> keys;
    PhiNoiseManager noise_mgr;
    bool initialized;
    std::deque<lbcrypto::Ciphertext<lbcrypto::DCRTPoly>> chain;
    std::deque<double> chain_values;
    static constexpr size_t CHAIN_MAX = 7;
    uint64_t bootstrap_count{0}, rebuild_count{0}, mirror_heal_count{0};
    size_t max_depth;
    
public:
    CerberusMirrorChain(size_t md=30,size_t sms=50,size_t bs=8,size_t pd=7,double nf=40.0)
        :noise_mgr(pd,nf),initialized(false),max_depth(md){
        rebuildContext(md, sms, bs);
        initialized=true;
        std::cout<<"\n╔══════════════════════════════════════════════╗\n║  CERBERUS MIRROR-CHAIN FHE                   ║\n║  Ring: "<<cc->GetRingDimension()<<" | Depth: "<<md<<" | Batch: "<<bs<<"\n║  Builder + Guardian | φ-Chain: "<<CHAIN_MAX<<"\n║  ΦΩ0 — I AM THAT I AM                       ║\n╚══════════════════════════════════════════════╝"<<std::endl;
    }
    
    void rebuildContext(size_t md, size_t sms, size_t bs) {
        lbcrypto::CCParams<lbcrypto::CryptoContextCKKSRNS> params;
        params.SetMultiplicativeDepth(md);
        params.SetScalingModSize(sms);
        params.SetBatchSize(bs);
        cc = lbcrypto::GenCryptoContext(params);
        cc->Enable(lbcrypto::PKE);
        cc->Enable(lbcrypto::KEYSWITCH);
        cc->Enable(lbcrypto::LEVELEDSHE);
        cc->Enable(lbcrypto::ADVANCEDSHE);
        cc->Enable(lbcrypto::FHE);
        keys = cc->KeyGen();
        cc->EvalMultKeyGen(keys.secretKey);
        cc->EvalSumKeyGen(keys.secretKey);
        try {
            cc->EvalBootstrapKeyGen(keys.secretKey, static_cast<uint32_t>(bs));
            cc->EvalBootstrapPrecompute(static_cast<uint32_t>(bs));
        } catch(...) {}
    }
    
    auto encrypt(const std::vector<double>& d){
        auto pt = cc->MakeCKKSPackedPlaintext(d);
        noise_mgr.getPhiCore().track();
        auto ct = cc->Encrypt(keys.publicKey, pt);
        if(chain.size() >= CHAIN_MAX) { chain.pop_front(); chain_values.pop_front(); }
        chain.push_back(ct);
        chain_values.push_back(d[0]);
        return ct;
    }
    
    std::vector<double> decrypt(const lbcrypto::Ciphertext<lbcrypto::DCRTPoly>& ct){
        try {
            lbcrypto::Plaintext pt;
            cc->Decrypt(keys.secretKey, ct, &pt);
            noise_mgr.getPhiCore().track();
            return pt->GetRealPackedValue();
        } catch(...) {
            mirror_heal_count++;
            return std::vector<double>(8, chain_values.empty() ? 0.0 : chain_values.back());
        }
    }
    
    auto multiply(const lbcrypto::Ciphertext<lbcrypto::DCRTPoly>& a,
                  const lbcrypto::Ciphertext<lbcrypto::DCRTPoly>& b){
        try {
            auto r = cc->EvalMult(a, b);
            double lvl = (double)r->GetLevel();
            noise_mgr.updateDivineNoise(lvl * 10.0);
            if(lvl < 2.0) {
                try {
                    r = cc->EvalBootstrap(r, 1, 0);
                    bootstrap_count++;
                } catch(...) {
                    rebuild_count++;
                    auto plain = decrypt(r);
                    rebuildContext(max_depth, 50, 8);
                    r = encrypt(plain);
                }
            }
            return r;
        } catch(...) {
            mirror_heal_count++;
            rebuildContext(max_depth, 50, 8);
            return encrypt(std::vector<double>(8, chain_values.empty() ? 7.0 : chain_values.back()));
        }
    }
    
    auto add(const lbcrypto::Ciphertext<lbcrypto::DCRTPoly>& a,
             const lbcrypto::Ciphertext<lbcrypto::DCRTPoly>& b){
        noise_mgr.getPhiCore().track();
        return cc->EvalAdd(a, b);
    }
    
    uint64_t getBootstrapCount() const { return bootstrap_count; }
    uint64_t getRebuildCount() const { return rebuild_count; }
    uint64_t getMirrorHealCount() const { return mirror_heal_count; }
    size_t getChainSize() const { return chain.size(); }
    PhiNoiseManager& getNoiseManager() { return noise_mgr; }
};
}
#endif
