#ifndef RSA_UTIL_H
#define RSA_UTIL_H

#include <memory>
#include <string>
#include <openssl/sha.h>
#include <openssl/rsa.h>

enum RSA_KEY { PRIVATE, PUBLIC };

using ptr_bio = std::unique_ptr<BIO, decltype(&BIO_free)>;
using ptr_rsa = std::unique_ptr<RSA, decltype(&RSA_free)>;

std::string sha256_sum(void *raw, size_t raw_sz);

ptr_rsa PEM_read(enum RSA_KEY type_key, const std::string &key_in);

bool RSA_sign_data(const std::string &sig_data,
                   const std::string &key_priv,
                   std::string &sig_out);

bool RSA_verify_data(const std::string &key_pub,
                     const uint8_t *raw_data,
                     int sig_data_sz,
                     const uint8_t *sig_data,
                     int sig_hash_sz);

#endif // RSA_UTIL_H
