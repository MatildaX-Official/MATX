// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2017-2021 The MATX Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CRYPTER_H
#define BITCOIN_CRYPTER_H

#include "keystore.h"
#include "serialize.h"
#include "streams.h"
#include "support/allocators/zeroafterfree.h"

class uint256;

#include <atomic>

const unsigned int WALLET_CRYPTO_KEY_SIZE = 32;
const unsigned int WALLET_CRYPTO_SALT_SIZE = 8;
const unsigned int WALLET_CRYPTO_IV_SIZE = 16;

/**
 * Private key encryption is done based on a CMasterKey,
 * which holds a salt and random encryption key.
 *
 * CMasterKeys are encrypted using AES-256-CBC using a key
 * derived using derivation method nDerivationMethod
 * (0 == EVP_sha512()) and derivation iterations nDeriveIterations.
 * vchOtherDerivationParameters is provided for alternative algorithms
 * which may require more parameters (such as scrypt).
 *
 * Wallet Private Keys are then encrypted using AES-256-CBC
 * with the double-sha256 of the public key as the IV, and the
 * master key's key as the encryption key (see keystore.[ch]).
 */

/** Master key for wallet encryption */
class CMasterKey
{
public:
    std::vector<unsigned char> vchCryptedKey;
    std::vector<unsigned char> vchSalt;
    //! 0 = EVP_sha512()
    //! 1 = scrypt()
    unsigned int nDerivationMethod;
    unsigned int nDeriveIterations;
    //! Use this for more parameters to key derivation,
    //! such as the various parameters to scrypt
    std::vector<unsigned char> vchOtherDerivationParameters;

    SERIALIZE_METHODS(CMasterKey, obj) { READWRITE(obj.vchCryptedKey, obj.vchSalt, obj.nDerivationMethod, obj.nDeriveIterations, obj.vchOtherDerivationParameters); }

    CMasterKey()
    {
        // 25000 rounds is just under 0.1 seconds on a 1.86 GHz Pentium M
        // ie slightly lower than the lowest hardware we need bother supporting
        nDeriveIterations = 25000;
        nDerivationMethod = 0;
        vchOtherDerivationParameters = std::vector<unsigned char>(0);
    }
};

typedef std::vector<unsigned char, secure_allocator<unsigned char> > CKeyingMaterial;

namespace wallet_crypto
{
    class TestCrypter;
}

class CSecureDataStream : public CBaseDataStream<CKeyingMaterial>
{
public:
    explicit CSecureDataStream(int nTypeIn, int nVersionIn) : CBaseDataStream(nTypeIn, nVersionIn) { }

    CSecureDataStream(const_iterator pbegin, const_iterator pend, int nTypeIn, int nVersionIn) :
            CBaseDataStream(pbegin, pend, nTypeIn, nVersionIn) { }

    CSecureDataStream(const vector_type& vchIn, int nTypeIn, int nVersionIn) :
            CBaseDataStream(vchIn, nTypeIn, nVersionIn) { }
};

/** Encryption/decryption context with key information */
class CCrypter
{
friend class wallet_crypto::TestCrypter; // for test access to chKey/chIV
private:
    std::vector<unsigned char, secure_allocator<unsigned char>> vchKey;
    std::vector<unsigned char, secure_allocator<unsigned char>> vchIV;
    bool fKeySet;

    int BytesToKeySHA512AES(const std::vector<unsigned char>& chSalt, const SecureString& strKeyData, int count, unsigned char *key,unsigned char *iv) const;

public:
    bool SetKeyFromPassphrase(const SecureString& strKeyData, const std::vector<unsigned char>& chSalt, const unsigned int nRounds, const unsigned int nDerivationMethod);
    bool Encrypt(const CKeyingMaterial& vchPlaintext, std::vector<unsigned char>& vchCiphertext) const;
    bool Decrypt(const std::vector<unsigned char>& vchCiphertext, CKeyingMaterial& vchPlaintext) const;
    bool SetKey(const CKeyingMaterial& chNewKey, const std::vector<unsigned char>& chNewIV);

    void CleanKey()
    {
        memory_cleanse(vchKey.data(), vchKey.size());
        memory_cleanse(vchIV.data(), vchIV.size());
        fKeySet = false;
    }

    CCrypter()
    {
        fKeySet = false;
        vchKey.resize(WALLET_CRYPTO_KEY_SIZE);
        vchIV.resize(WALLET_CRYPTO_IV_SIZE);
    }

    ~CCrypter()
    {
        CleanKey();
    }
};

bool EncryptSecret(const CKeyingMaterial& vMasterKey, const CKeyingMaterial& vchPlaintext, const uint256& nIV, std::vector<unsigned char>& vchCiphertext);
bool DecryptSecret(const CKeyingMaterial& vMasterKey, const std::vector<unsigned char>& vchCiphertext, const uint256& nIV, CKeyingMaterial& vchPlaintext);


/** Keystore which keeps the private keys encrypted.
 * It derives from the basic key store, which is used if no encryption is active.
 */
class CCryptoKeyStore : public CBasicKeyStore
{
private:
    //! if fUseCrypto is true, mapKeys and mapSaplingSpendingKeys must be empty
    //! if fUseCrypto is false, vMasterKey must be empty
    std::atomic<bool> fUseCrypto;

protected:
    // TODO: In the future, move this variable to the wallet class directly following upstream's structure.
    CKeyingMaterial vMasterKey;
    // Sapling
    CryptedSaplingSpendingKeyMap mapCryptedSaplingSpendingKeys;

    bool SetCrypted();

    //! will encrypt previously unencrypted keys
    bool EncryptKeys(CKeyingMaterial& vMasterKeyIn);

    CryptedKeyMap mapCryptedKeys;

    static bool DecryptKey(const CKeyingMaterial& vMasterKey, const std::vector<unsigned char>& vchCryptedSecret, const CPubKey& vchPubKey, CKey& key);

    // Unlock Sapling keys
    bool UnlockSaplingKeys(const CKeyingMaterial& vMasterKeyIn, bool fDecryptionThoroughlyChecked);

public:
    CCryptoKeyStore() : fUseCrypto(false) { }

    bool IsCrypted() const { return fUseCrypto; }
    bool IsLocked() const;

    virtual bool AddCryptedKey(const CPubKey& vchPubKey, const std::vector<unsigned char>& vchCryptedSecret);
    bool AddKeyPubKey(const CKey& key, const CPubKey& pubkey) override;
    bool HaveKey(const CKeyID& address) const override;

    bool GetKey(const CKeyID& address, CKey& keyOut) const override;
    bool GetPubKey(const CKeyID& address, CPubKey& vchPubKeyOut) const override;
    std::set<CKeyID> GetKeys() const override;

    //! Sapling
    virtual bool AddCryptedSaplingSpendingKey(const libzcash::SaplingExtendedFullViewingKey& extfvk,
                                              const std::vector<unsigned char>& vchCryptedSecret);
    bool HaveSaplingSpendingKey(const libzcash::SaplingExtendedFullViewingKey& extfvk) const override;
    bool GetSaplingSpendingKey(const libzcash::SaplingExtendedFullViewingKey& extfvk, libzcash::SaplingExtendedSpendingKey& skOut) const override;

    /**
     * Wallet status (encrypted, locked) changed.
     * Note: Called without locks held.
     */
    boost::signals2::signal<void(CCryptoKeyStore* wallet)> NotifyStatusChanged;
};

#endif // BITCOIN_CRYPTER_H
