#include "util.hpp"
#include "util_rsa.hpp"
#include <openssl/pem.h>

std::string
sha256_sum(void *raw, size_t raw_sz)
{
    uint8_t hash_raw[SHA256_DIGEST_LENGTH];
    std::string hash_str(sizeof(hash_raw) * 2, '\0');

    SHA256(static_cast<uint8_t *>(raw), raw_sz, hash_raw);

    for (size_t i = 0; i < sizeof(hash_raw); ++i) {
        std::sprintf(&hash_str[i * 2], "%02hhx", hash_raw[i]);
    }
    return hash_str;
}

ptr_rsa
PEM_read(enum RSA_KEY type_key, const std::string &key)
{
    ptr_bio bio(BIO_new_mem_buf(key.data(), key.size()), BIO_free);

    if (!bio.get()) {
        throw_err("!BIO_new_mem_buf()", "BIO mem");
    }

    RSA *(*rsa_read_cb)(BIO *, RSA **, pem_password_cb *, void *) = nullptr;

    if (type_key == RSA_KEY::PRIVATE) {
        rsa_read_cb = PEM_read_bio_RSAPrivateKey;
    } else {
        rsa_read_cb = PEM_read_bio_RSA_PUBKEY;
    }

    ptr_rsa rsa_key(rsa_read_cb(bio.get(), nullptr, nullptr, nullptr), RSA_free);

    if (!rsa_key.get()) {
        if (type_key == RSA_KEY::PRIVATE) {
            throw_err("!PEM_read_bio_RSAPrivateKey()", "Get Private key");
        } else {
            throw_err("!PEM_read_bio_RSA_PUBKEY()", "Get Public key");
        }
    }

    return rsa_key;
}

bool
RSA_verify_data(const std::string &key_pub,
                const uint8_t *raw_data,
                int raw_data_sz,
                const uint8_t *sig_data,
                int sig_data_sz)
{
    auto rsa_key = PEM_read(RSA_KEY::PUBLIC, key_pub);

    uint8_t sha256_raw[SHA256_DIGEST_LENGTH];
    SHA256(raw_data, raw_data_sz, sha256_raw);

    if (RSA_verify(NID_sha256,
                   sha256_raw,
                   sizeof(sha256_raw),
                   sig_data,
                   sig_data_sz,
                   rsa_key.get()) != 1) {
        return false;
    }
    return true;
}

bool
RSA_sign_data(const std::string &sig_data,
              const std::string &key_priv,
              std::string &sig_buf)
{
    auto rsa_key    = PEM_read(RSA_KEY::PRIVATE, key_priv);
    auto rsa_key_sz = RSA_size(rsa_key.get());

    sig_buf.resize(rsa_key_sz);
    uint32_t sig_len = 0;

    uint8_t hash_raw[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const uint8_t *>(sig_data.data()), sig_data.size(), hash_raw);

    if (RSA_sign(NID_sha256,
                 hash_raw,
                 sizeof(hash_raw),
                 reinterpret_cast<uint8_t *>(sig_buf.data()),
                 &sig_len,
                 rsa_key.get()) != 1) {
        return false;
    }
    return true;
}
