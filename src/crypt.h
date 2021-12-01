#pragma once

#include "pdf_objects.h"


class Crypt
{
public:
    bool can_decrypt;
    Crypt();
    bool decryptionSupported();
    bool authenticate(const char *password);
    bool getEncryptionInfo(PdfObject *encrypt_dict, PdfObject *p_trailer);
    void decryptIndirectObject(PdfObject *obj, int obj_no, int gen_no);
private:
    int version;
    int revision;
    int keylen;// in bytes
    int perm;
    std::string O;
    std::string U;
    std::string id0;

    std::string encryption_key;// calculated from password, /O, permission and trailer ID
    bool authenticateUserPassword(std::string password);
};



typedef unsigned char uchar;

class RC4
{
public:
   uchar init_state[256];// initial state

   RC4(std::string key);
   void crypt(uchar *data, int len);
};



// a small class for calculating MD5 hashes of strings or byte arrays
// it is not meant to be fast or secure
// assumes that char is 8 bit and int is 32 bit
class MD5
{
public:
    uchar digest[16];// the result

    MD5();
    MD5(const std::string& text);
    void init();
    void update(const uchar *buf, size_t length);
    void update(const char *buf, size_t length);
    MD5& finalize();

private:
    typedef uint8_t uint1;
    typedef uint32_t uint4;
    enum {blocksize = 64}; // VC6 won't eat a const static int here

    void transform(const uint1 block[blocksize]);
    static void decode(uint4 output[], const uint1 input[], size_t len);
    static void encode(uint1 output[], const uint4 input[], size_t len);

    bool finalized;
    uint1 buffer[blocksize]; // bytes that didn't fit in last 64 byte chunk
    uint4 count[2];   // 64bit counter for number of bits (lo, hi)
    uint4 state[4];   // digest so far

    // low level logic operations
    static inline uint4 F(uint4 x, uint4 y, uint4 z);
    static inline uint4 G(uint4 x, uint4 y, uint4 z);
    static inline uint4 H(uint4 x, uint4 y, uint4 z);
    static inline uint4 I(uint4 x, uint4 y, uint4 z);
    static inline uint4 rotate_left(uint4 x, int n);
    static inline void FF(uint4 &a, uint4 b, uint4 c, uint4 d, uint4 x, uint4 s, uint4 ac);
    static inline void GG(uint4 &a, uint4 b, uint4 c, uint4 d, uint4 x, uint4 s, uint4 ac);
    static inline void HH(uint4 &a, uint4 b, uint4 c, uint4 d, uint4 x, uint4 s, uint4 ac);
    static inline void II(uint4 &a, uint4 b, uint4 c, uint4 d, uint4 x, uint4 s, uint4 ac);
};



