#pragma once

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>

#include <fstream>
#include <iostream>
#include <string>
#include <vector>

class OpenSSL {
private:
  EVP_PKEY *m_PrivateKey = nullptr;

public:
  OpenSSL() { OpenSSL_add_all_algorithms(); };

  ~OpenSSL() { EVP_PKEY_free(m_PrivateKey); };

  void loadPrivateKey(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp)
      throw std::runtime_error("File not found: " + std::string(filename));

    if (m_PrivateKey)
      EVP_PKEY_free(m_PrivateKey);

    m_PrivateKey = PEM_read_PrivateKey(fp, nullptr, nullptr, nullptr);
    fclose(fp);
    if (!m_PrivateKey)
      throw std::runtime_error("Failed to load private key");
  }

  EVP_PKEY *loadPublicKey(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp)
      throw std::runtime_error("File not found: " + std::string(filename));

    EVP_PKEY *publicKey = PEM_read_PUBKEY(fp, nullptr, nullptr, nullptr);
    fclose(fp);
    if (!publicKey)
      throw std::runtime_error("Failed to load public key");

    return publicKey;
  }

  std::vector<uint8_t> sign(void *bytes, size_t size) {

    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (!ctx)
      throw std::runtime_error("Failed to create EVP_MD_CTX");

    std::vector<uint8_t> signature;
    size_t sigLen = 0;

    // Init sign context with SHA256
    if (EVP_DigestSignInit(ctx, nullptr, EVP_sha256(), nullptr, m_PrivateKey) !=
        1) {
      EVP_MD_CTX_free(ctx);
      throw std::runtime_error("DigestSignInit failed");
    }

    // Add the data to be signed
    if (EVP_DigestSignUpdate(ctx, bytes, size) != 1) {
      EVP_MD_CTX_free(ctx);
      throw std::runtime_error("DigestSignUpdate failed");
    }

    // First call with nullptr gets required signature length
    if (EVP_DigestSignFinal(ctx, nullptr, &sigLen) != 1) {
      EVP_MD_CTX_free(ctx);
      throw std::runtime_error("DigestSignFinal (size query) failed");
    }

    signature.resize(sigLen);

    // Second call actually writes the signature
    if (EVP_DigestSignFinal(ctx, signature.data(), &sigLen) != 1) {
      EVP_MD_CTX_free(ctx);
      throw std::runtime_error("DigestSignFinal (sign) failed");
    }

    signature.resize(sigLen);
    EVP_MD_CTX_free(ctx);
    return signature;
  }

  bool verify(EVP_PKEY *publicKey, const unsigned char *bytes, size_t size,
              const unsigned char *signature, size_t signatureSize) {
    if (!publicKey)
      return false;

    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (!ctx)
      return false;

    bool ok = EVP_DigestVerifyInit(ctx, nullptr, EVP_sha256(), nullptr,
                                   publicKey) == 1 &&
              EVP_DigestVerifyUpdate(ctx, bytes, size) == 1 &&
              EVP_DigestVerifyFinal(ctx, signature, signatureSize) == 1;

    EVP_MD_CTX_free(ctx);

    return ok;
  }
};
