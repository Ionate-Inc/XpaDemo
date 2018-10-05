 //
// Copyright (c) 2018 Andreas Sedlmeier (sedlmeier@hotmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
// 
#include "stdafx.h"
#include "userdll.h"

#include <windows.h>
#include <fstream>

#include "mgcrypt_tls_objects.h"
#include "mgcrypt_sha256.h"
#include "mgcrypt_aes.h"
#include "mgcrypt_misc.h"

DWORD gv_dwTlsIndex = 0;
DWORD gvs_dwThreadCount = 0;

/*-------------------------------------------------------------------------*/
/* forward declarations and Magic UDF/UDP stuff ...
/*-------------------------------------------------------------------------*/
extern "C"
{
	__declspec(dllexport) void *MAGIC_BIND(void);
	__declspec(dllexport) const char* mgcrypt_version();
	__declspec(dllexport) char* mgcrypt_GenerateRandom(long* lpdwBlocksize);
	__declspec(dllexport) char* mgcrypt_Hexencode(char* pRawData);
	__declspec(dllexport) char* mgcrypt_Hexdecode(char* pHexencoded);
	__declspec(dllexport) char* mgcrypt_sha256_HexdigitsA(char* lpszBuffer, long* lpdwBufferLen, char* lpszData, long* lpdwDataLen);
	__declspec(dllexport) char* mgcrypt_sha256_HexdigitsB(char* lpszBuffer, long* lpdwBufferLen, char* lpszData, long* lpdwDataLen);
	__declspec(dllexport) char* mgcrypt_AES_Encrypt(char* lpszPlaintext, long* lpdwPlaintextLength, char* lpszKey, char* lpszMode, char* lpszIV);
	__declspec(dllexport) char* mgcrypt_AES_DeriveKey(char* lpszPassphrase, char* lpszIV, char* lpszKeyBuffer, long* lpdwKeybufferLength, long* error);
}

#define FUNC_CNT     10

static FUNC_DSC fdsc_tbl[FUNC_CNT] =
{
	{ (unsigned char *) "mgcrypt_version", mgcrypt_version, 0, (unsigned char *) "A" },
	{ (unsigned char *) "mgcrypt_version", mgcrypt_version, 0, (unsigned char *) "A" },
	{ (unsigned char *) "mgcrypt_GenerateRandom", mgcrypt_GenerateRandom, 1, (unsigned char *) "LA" },
	{ (unsigned char *) "mgcrypt_Hexencode", mgcrypt_Hexencode, 1, (unsigned char *) "TB" },
	{ (unsigned char *) "mgcrypt_Hexdecode", mgcrypt_Hexdecode, 1, (unsigned char *) "TB" },
	{ (unsigned char *) "mgcrypt_sha256_HexdigitsA", mgcrypt_sha256_HexdigitsA, 4, (unsigned char *) "ALALA" },
	{ (unsigned char *) "mgcrypt_sha256_HexdigitsB", mgcrypt_sha256_HexdigitsB, 4, (unsigned char *) "ALTLA" },
	{ (unsigned char *) "mgcrypt_AES_EncryptA", mgcrypt_AES_Encrypt, 5, (unsigned char *) "ALAAAA" },
	{ (unsigned char *) "mgcrypt_AES_EncryptB", mgcrypt_AES_Encrypt, 5, (unsigned char *) "TLAAAA" },
	{ (unsigned char *) "mgcrypt_AES_DeriveKey", mgcrypt_AES_DeriveKey, 5, (unsigned char *) "AAALLA" }
};

static EXT_MODULE ext_module = { 0, NULL_PTR, NULL_PTR, FUNC_CNT, fdsc_tbl, (Uchar *) "mgcrypt" };

__declspec(dllexport) void *MAGIC_BIND(void)
{
	return (&ext_module);
}

// TODO: read following from version header in resource file
const char* mgcrypt_version ()
{
	return "0.0.1";
}

/*
    Calculates the SHA-256 hash for a message of any size and codes the hash bytes as hexadecimal digits.
	A SHA-256 message digest (hash) is 256 Bit (as the name suggests), thats 32 Bytes.
	mgcrypt_sha256_HexdigitsA generates a hexadecimal representation of the (binary) hash where each digit (0..F) codes 4 Bit.
	The buffer which will receive the result (lpszHash) therefore needs to be large enough to get filled with 64 digits (characters).
	mgcrypt_sha256_HexdigitsA will not add a trailling 0 character.
*/
char* mgcrypt_sha256_HexdigitsA(char* lpszHash, long* lpdwBufferLen, char* lpszData, long* lpdwDataLen) {
	return sha256_HexdigitsA (lpszData, *lpdwDataLen, lpszHash, *lpdwBufferLen);
}

char* mgcrypt_sha256_HexdigitsB(char* lpszHash, long* lpdwBufferLen, char* lpszData, long* lpdwDataLen) {
	return sha256_HexdigitsA(lpszData, *lpdwDataLen, lpszHash, *lpdwBufferLen);
}

/** 
	mgcrypt_AES_Encrypt encrypts plain text (binary data) with AES algorithm.
	The required memory for the ciphertext is allocated dynamically in TLS, no need to do this in Magic and pass a paramter which points to it.
	The ciphertext is the return value of this function. In case of an error an empty string will be returned.
**/

char* mgcrypt_AES_Encrypt(char* lpszPlaintext, long* lpdwPlaintextLength, char* lpszKey, char* lpszMode, char* lpszIV)
{

	TlsObject* pData = (TlsObject*)TlsGetValue(gv_dwTlsIndex);
	if (pData)
	{
		// delete encryption result of a previous run ...
		if (pData->m_pBuffer_Ciphertext) {
			delete[] pData->m_pBuffer_Ciphertext;
			pData->m_pBuffer_Ciphertext = 0;
		}

		int encryption_result = 0;
		string mode(lpszMode);

		if (mode.compare("GCM") == 0)
			encryption_result = AES_Encrypt(lpszPlaintext, *lpdwPlaintextLength, lpszKey, lpszMode, &pData->m_pBuffer_Ciphertext, lpszIV);

		else if (mode.compare("ECB") == 0)
			encryption_result = AES_Encrypt_ECB(lpszPlaintext, *lpdwPlaintextLength, lpszKey, &pData->m_pBuffer_Ciphertext);

		return pData->m_pBuffer_Ciphertext;
	}

	return (char *) "";
}


/**
 * mgcrypt_AES_DeriveKey() allows to derive an AES encryption key from a passphrase. 
 */
char* mgcrypt_AES_DeriveKey(char* lpszPassphrase, char* lpszIV, char* lpszKeyBuffer, long* lpdwKeybufferLength, long* error)
{
	*error = -9999;
	return AES_DeriveKey(lpszPassphrase, lpszIV, lpszKeyBuffer, *lpdwKeybufferLength);
}

char* mgcrypt_GenerateRandom(long* lpdwBlocksize) {

	TlsObject* pData = (TlsObject*) TlsGetValue(gv_dwTlsIndex);
	if (pData)
	{
		if (pData->m_pBuffer_Random) {
			delete[] pData->m_pBuffer_Random;
			pData->m_pBuffer_Random = 0;
		}

		int encryption_result = GenerateRandom(&pData->m_pBuffer_Random, (const unsigned int) *lpdwBlocksize);
		if (encryption_result == 0)
			return pData->m_pBuffer_Random;
	}

	return (char *) "";
}

char* mgcrypt_Hexencode(char* pRawData) {
	TlsObject* pData = (TlsObject*)TlsGetValue(gv_dwTlsIndex);
	if (pData)
	{
		if (pData->m_pBuffer_Hexencode) {
			delete[] pData->m_pBuffer_Hexencode;
			pData->m_pBuffer_Hexencode = 0;
		}

		int encryption_result = Hexencode(pRawData, &pData->m_pBuffer_Hexencode);
		if (encryption_result == 0)
			return pData->m_pBuffer_Hexencode;
	}

	return (char *) "";
}

char* mgcrypt_Hexdecode(char* pHexencoded) {
	TlsObject* pData = (TlsObject*)TlsGetValue(gv_dwTlsIndex);
	if (pData)
	{
		if (pData->m_pBuffer_Hexencode) {
			delete[] pData->m_pBuffer_Hexencode;
			pData->m_pBuffer_Hexencode = 0;
		}

		int encryption_result = Hexdecode(pHexencoded, &pData->m_pBuffer_Hexencode);
		if (encryption_result == 0)
			return pData->m_pBuffer_Hexencode;
	}

	return (char *) "";
}
