#pragma once

#include <cstdint>
#include <cstddef>
#include <cassert>
#include <openssl/evp.h>
#include <openssl/aes.h>
#include <openssl/err.h>

static int aes128_dec(
  const uint8_t *ct, size_t ct_size, uint8_t *pt, const uint8_t *tag, const uint8_t *key, const uint8_t *iv) {
  int pt_size = 0;
  EVP_CIPHER_CTX *ctx;
  if (!(ctx = EVP_CIPHER_CTX_new())) {
    goto err;
  }

  if (!EVP_DecryptInit_ex(ctx, EVP_aes_128_gcm(), nullptr, key, iv)) {
    goto err;
  }

  if (tag && !EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, 16, const_cast<uint8_t *>(tag))) {
    goto err;
  }

  int size;
  if (!EVP_DecryptUpdate(ctx, pt, &size, ct, ct_size)) {
    goto err;
  }
  pt_size += size;

  if (!EVP_DecryptFinal_ex(ctx, pt + size, &size)) {
    goto err;
  }
  pt_size += size;

  assert(pt_size == ct_size);
  EVP_CIPHER_CTX_free(ctx);
  return 0;
err:
  EVP_CIPHER_CTX_free(ctx);
  return ERR_get_error();
}

static inline int aes128_dec(uint8_t *ct, size_t ct_size, const uint8_t *tag, const uint8_t *key, const uint8_t *iv) {
  return aes128_dec(ct, ct_size, ct, tag, key, iv);
}

static int aes128_enc(
  const uint8_t *pt, size_t pt_size, uint8_t *ct, uint8_t *tag, const uint8_t *key, const uint8_t *iv) {
  int ct_size = 0;
  EVP_CIPHER_CTX *ctx;
  if (!(ctx = EVP_CIPHER_CTX_new())) {
    goto err;
  }

  if (!EVP_EncryptInit_ex(ctx, EVP_aes_128_gcm(), nullptr, key, iv)) {
    goto err;
  }

  int size;
  if (!EVP_EncryptUpdate(ctx, ct, &size, pt, pt_size)) {
    goto err;
  }
  ct_size += size;

  if (!EVP_EncryptFinal_ex(ctx, ct + size, &size)) {
    goto err;
  }
  ct_size += size;

  if (!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16, tag)) {
    goto err;
  }

  assert(ct_size == pt_size);
  EVP_CIPHER_CTX_free(ctx);
  return 0;
err:
  EVP_CIPHER_CTX_free(ctx);
  return ERR_get_error();
}

static inline int aes128_enc(uint8_t *pt, size_t pt_size, uint8_t *tag, const uint8_t *key, const uint8_t *iv) {
  return aes128_enc(pt, pt_size, pt, tag, key, iv);
}
