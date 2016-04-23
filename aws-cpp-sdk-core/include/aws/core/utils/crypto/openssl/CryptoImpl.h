/*
* Copyright 2010-2016 Amazon.com, Inc. or its affiliates. All Rights Reserved.
*
* Licensed under the Apache License, Version 2.0 (the "License").
* You may not use this file except in compliance with the License.
* A copy of the License is located at
*
*  http://aws.amazon.com/apache2.0
*
* or in the "license" file accompanying this file. This file is distributed
* on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
* express or implied. See the License for the specific language governing
* permissions and limitations under the License.
*/

#pragma once

#include <aws/core/utils/crypto/Hash.h>
#include <aws/core/utils/crypto/HMAC.h>
#include <aws/core/utils/crypto/Cipher.h>
#include <aws/core/utils/crypto/SecureRandom.h>
#include <aws/core/utils/GetTheLights.h>
#include <openssl/ossl_typ.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <atomic>
#include <mutex>

namespace Aws
{
    namespace Utils
    {
        namespace Crypto
        {
            namespace OpenSSL
            {
                extern GetTheLights getTheLights;
                void init_static_state();
                void cleanup_static_state();
                void locking_fn(int mode, int n, const char* file, int line);
                unsigned long id_fn();
            }

            extern std::once_flag secureRandFlag;

            template<typename DataType = uint64_t>
            class SecureRandomOpenSSLImpl : public SecureRandom<DataType>
            {
            public:
                SecureRandomOpenSSLImpl() { }

                ~SecureRandomOpenSSLImpl() = default;

                void Reset() override { }

                DataType operator()() override
                {
                    unsigned char buffer[sizeof(DataType)];

                    int success = RAND_bytes(buffer, sizeof(DataType));

                    if(success != 1)
                    {
                        SecureRandom<DataType>::m_failure = true;
                    }

                    DataType value(0);
                    for(size_t i = 0; i < sizeof(DataType); ++i)
                    {
                        value <<= 8;
                        value |= i;
                    }

                    return value;
                }
            };

            class MD5OpenSSLImpl : public Hash
            {
            public:

                MD5OpenSSLImpl() { }
                virtual ~MD5OpenSSLImpl() = default;

                virtual HashResult Calculate(const Aws::String& str) override;

                virtual HashResult Calculate(Aws::IStream& stream) override;

            };

            class Sha256OpenSSLImpl : public Hash
            {
            public:
                Sha256OpenSSLImpl() {}

                virtual ~Sha256OpenSSLImpl() = default;

                virtual HashResult Calculate(const Aws::String& str) override;

                virtual HashResult Calculate(Aws::IStream& stream) override;
            };

            class Sha256HMACOpenSSLImpl : public HMAC
            {
            public:

                Sha256HMACOpenSSLImpl() {}

                virtual ~Sha256HMACOpenSSLImpl() = default;

                virtual HashResult Calculate(const ByteBuffer& toSign, const ByteBuffer& secret) override;
            };

            /**
             * OpenSSL implementation for SymmetricCipher
             */
            class OpenSSLCipher : public SymmetricCipher
            {
            public:
                /**
                 * Creates new OpenSSL based cipher for key, and autogenerates a secure IV of size ivSize
                 */
                OpenSSLCipher(const ByteBuffer &key, size_t ivSize, bool ctrMode = false);

                /**
                 * Creates new OpenSSL based cipher for key, initializationVector, and optional tag. If this is an authenticated
                 * cipher being used for decryption.
                 */
                OpenSSLCipher(ByteBuffer&& key, ByteBuffer&& initializationVector, ByteBuffer&& tag = ByteBuffer(0));

                /**
                 * Creates new OpenSSL based cipher for key, initializationVector, and optional tag. If this is an authenticated
                 * cipher being used for decryption.
                 */
                OpenSSLCipher(const ByteBuffer& key, const ByteBuffer& initializationVector, const ByteBuffer& tag = ByteBuffer(0));

                OpenSSLCipher(const OpenSSLCipher& other) = delete;
                OpenSSLCipher& operator=(const OpenSSLCipher& other) = delete;

                /**
                 * Normally we don't work around VS 2013 not auto-generating these, but they are kind of expensive,
                 * so let's go ahead and optimize by defining default move operations. Implementors of this class
                 * need to be sure to define the move operations and call the base class.
                 */
                OpenSSLCipher(OpenSSLCipher &&toMove);

                /**
                 * Normally we don't work around VS 2013 not auto-generating these, but they are kind of expensive,
                 * so let's go ahead and optimize by defining default move operations. Implementors of this class
                 * need to be sure to define the move operations and call the base class.
                 */
                OpenSSLCipher& operator=(OpenSSLCipher&& toMove) = default;


                virtual ~OpenSSLCipher();

                /**
                 * Encrypt a buffer of data. Part of the contract for this interface is that intention that
                 * a user call this function multiple times for a large stream. As such, multiple calls to this function
                 * on the same instance should produce valid sequential output for an encrypted stream.
                 */
                ByteBuffer EncryptBuffer( const ByteBuffer& unEncryptedData) override;

                /**
                 * Finalize Encryption, returns anything remaining in the last block
                 */
                ByteBuffer FinalizeEncryption () override;

                /**
                * Decrypt a buffer of data. Part of the contract for this interface is that intention that
                * a user call this function multiple times for a large stream. As such, mutliple calls to this function
                * on the same instance should produce valid sequential output from an encrypted stream.
                */
                ByteBuffer DecryptBuffer(const ByteBuffer& encryptedData) override;

                /**
                 * Finalize Decryption, returns anything remaining in the last block
                 */
                ByteBuffer FinalizeDecryption () override;

            protected:
                /**
                 * Algorithm/Mode level config for the EVP_CIPHER_CTX
                 */
                virtual void InitEncryptor_Internal() = 0;
                /**
                 * Algorithm/Mode level config for the EVP_CIPHER_CTX
                 */
                virtual void InitDecryptor_Internal() = 0;

                virtual size_t GetBlockSizeBytes() const = 0;
                virtual size_t GetKeyLengthBits() const = 0;

                EVP_CIPHER_CTX m_ctx;

            private:
                void Init();
                void CheckInitEncryptor();
                void CheckInitDecryptor();

                bool m_encDecInitialized;
                bool m_encryptionMode;
                bool m_decryptionMode;
            };

            /**
             * OpenSSL implementation for AES in CBC mode
             */
            class AES_CBC_Cipher_OpenSSL : public OpenSSLCipher
            {
            public:
                /**
                 * Create AES in CBC mode off of a 256 bit key. Auto Generates a 16 byte secure random IV
                 */
                AES_CBC_Cipher_OpenSSL(const ByteBuffer &key);

                /**
                 * Create AES in CBC mode off of a 256 bit key and 16 byte IV
                 */
                AES_CBC_Cipher_OpenSSL(ByteBuffer &&key, ByteBuffer &&initializationVector);

                /**
                 * Create AES in CBC mode off of a 256 bit key and 16 byte IV
                 */
                AES_CBC_Cipher_OpenSSL(const ByteBuffer &key, const ByteBuffer &initializationVector);

                AES_CBC_Cipher_OpenSSL(const AES_CBC_Cipher_OpenSSL &other) = delete;

                AES_CBC_Cipher_OpenSSL(AES_CBC_Cipher_OpenSSL&& toMove) = default;

            protected:
                void InitEncryptor_Internal() override;
                void InitDecryptor_Internal() override;

                size_t GetBlockSizeBytes() const override;
                size_t GetKeyLengthBits() const override;

            private:
                static size_t BlockSizeBytes;
                static size_t KeyLengthBits;
            };

            /**
             * OpenSSL implementation for AES in CTR mode
             */
            class AES_CTR_Cipher_OpenSSL : public OpenSSLCipher
            {
            public:
                /**
                 * Create AES in CTR mode off of a 256 bit key. Auto Generates a 16 byte IV in the format
                 * [nonce 4bytes ] [securely random iv 8 bytes] [ CTR init 4bytes ]
                 */
                AES_CTR_Cipher_OpenSSL(const ByteBuffer &key);

                /**
                 * Create AES in CTR mode off of a 256 bit key and 16 byte IV
                 */
                AES_CTR_Cipher_OpenSSL(ByteBuffer &&key, ByteBuffer &&initializationVector);

                /**
                 * Create AES in CTR mode off of a 256 bit key and 16 byte IV
                 */
                AES_CTR_Cipher_OpenSSL(const ByteBuffer &key, const ByteBuffer &initializationVector);

                AES_CTR_Cipher_OpenSSL(const AES_CTR_Cipher_OpenSSL &other) = delete;

                AES_CTR_Cipher_OpenSSL(AES_CTR_Cipher_OpenSSL&& toMove) = default;

            protected:
                void InitEncryptor_Internal() override;
                void InitDecryptor_Internal() override;

                size_t GetBlockSizeBytes() const override;
                size_t GetKeyLengthBits() const override;

            private:
                static size_t BlockSizeBytes;
                static size_t KeyLengthBits;
            };

            /**
             * OpenSSL implementation for AES in GCM mode
             */
            class AES_GCM_Cipher_OpenSSL : public OpenSSLCipher
            {
            public:
                /**
                 * Create AES in GCM mode off of a 256 bit key. Auto Generates a 16 byte secure random IV.
                 */
                AES_GCM_Cipher_OpenSSL(const ByteBuffer &key);

                /**
                 * Create AES in GCM mode off of a 256 bit key, a 16 byte secure random IV, and an optional 16 byte Tag. If you are using this
                 * cipher to decrypt an encrypted payload, you must set the tag here.
                 */
                AES_GCM_Cipher_OpenSSL(ByteBuffer &&key, ByteBuffer &&initializationVector, ByteBuffer&& tag = ByteBuffer(0));

                /**
                 * Create AES in GCM mode off of a 256 bit key, a 16 byte secure random IV, and an optional 16 byte Tag. If you are using this
                 * cipher to decrypt an encrypted payload, you must set the tag here.
                 */
                AES_GCM_Cipher_OpenSSL(const ByteBuffer &key, const ByteBuffer &initializationVector, const ByteBuffer& tag = ByteBuffer(0));

                AES_GCM_Cipher_OpenSSL(const AES_GCM_Cipher_OpenSSL &other) = delete;

                AES_GCM_Cipher_OpenSSL(AES_GCM_Cipher_OpenSSL&& toMove) = default;

                /**
                 * Calls base class first, then grabs the tag from the cipher and sets it on m_tag.
                 * After calling FinalizeEncryption, be sure to call GetTag() and do something with it
                 * or you will not be able to decrypt the payload.
                 */
                ByteBuffer FinalizeEncryption () override;

            protected:
                void InitEncryptor_Internal() override;
                void InitDecryptor_Internal() override;

                size_t GetBlockSizeBytes() const override;
                size_t GetKeyLengthBits() const override;

            private:
                static size_t BlockSizeBytes;
                static size_t IVLengthBytes;
                static size_t KeyLengthBits;
            };

        } // namespace Crypto
    } // namespace Utils
} // namespace Aws
