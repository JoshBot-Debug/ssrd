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
  EVP_PKEY *m_PublicKey = nullptr;
  EVP_PKEY *m_PrivateKey = nullptr;

public:
  OpenSSL() { OpenSSL_add_all_algorithms(); };

  ~OpenSSL() {
    EVP_PKEY_free(m_PrivateKey);
    EVP_PKEY_free(m_PublicKey);
  };

  bool validate() {
    if (!m_PrivateKey || !m_PublicKey)
      return false;

    return EVP_PKEY_eq(m_PrivateKey, m_PublicKey) == 1;
  }

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

  void loadPublicKey(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp)
      throw std::runtime_error("File not found: " + std::string(filename));

    if (m_PublicKey)
      EVP_PKEY_free(m_PublicKey);

    m_PublicKey = PEM_read_PUBKEY(fp, nullptr, nullptr, nullptr);
    fclose(fp);
    if (!m_PublicKey)
      throw std::runtime_error("Failed to load public key");
  }

  void loadPublicKey(void *buffer, ssize_t size) {
    BIO *bio = BIO_new_mem_buf(buffer, static_cast<int>(size));

    if (!bio)
      throw std::runtime_error("Failed to create BIO from buffer");

    if (m_PublicKey)
      EVP_PKEY_free(m_PublicKey);

    m_PublicKey = PEM_read_bio_PUBKEY(bio, nullptr, nullptr, nullptr);
    
    BIO_free(bio);

    if (!m_PublicKey)
      throw std::runtime_error("Failed to load public key");
  }
};
