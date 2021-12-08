#include "crypt.h"
#include "debug.h"
#include "common.h"
#include <cstring>

static uchar padding_arr[] = {
    0x28, 0xBF, 0x4E, 0x5E, 0x4E, 0x75, 0x8A, 0x41,
    0x64, 0x00, 0x4E, 0x56, 0xFF, 0xFA, 0x01, 0x08,
    0x2E, 0x2E, 0x00, 0xB6, 0xD0, 0x68, 0x3E, 0x80,
    0x2F, 0x0C, 0xA9, 0xFE, 0x64, 0x53, 0x69, 0x7A
};
static char *padding_str = (char*)padding_arr;

Crypt:: Crypt()
{
    version = 1;// only 1 and 2 are supported
    revision = 3;
    keylen = 5;
    O = "";
    U = "";
    perm = 0;
    id0 = "";
}

bool
Crypt:: decryptionSupported()
{
    return ((version>=1 && version<=2) and (keylen>=5 && keylen<=16) and
        perm!=0 and O.size()==32 and id0.size()==16);
}


bool
Crypt:: getEncryptionInfo(PdfObject *encrypt_dict, PdfObject *p_trailer)
{
    if (!encrypt_dict || !p_trailer)
        return false;
    int str_type;
    PdfObject *obj = encrypt_dict->dict->get("Filter");
    if (obj && obj->type==PDF_OBJ_NAME && strcmp(obj->name, "Standard")!=0){
        debug("error : unsupported Encrypt filter '%s'", obj->name);
        return false;
    }
    obj = encrypt_dict->dict->get("V");
    if (isInt(obj)){
        this->version = obj->integer;
    }
    obj = encrypt_dict->dict->get("R");
    if (isInt(obj)){
        this->revision = obj->integer;
    }
    obj = encrypt_dict->dict->get("Length");
    if (isInt(obj)){
        this->keylen = obj->integer/8;// converting bits to bytes
    }
    obj = encrypt_dict->dict->get("U");
    if (obj){
        if (obj->type==PDF_OBJ_STR) {
            this->U = pdfstr2bytes(obj->str, &str_type);
        }
        else {
            debug("error : Encrypt dict /U entry is not string obj");
            return false;
        }
    }
    obj = encrypt_dict->dict->get("O");
    if (isString(obj)){
        this->O = pdfstr2bytes(obj->str, &str_type);
        if (this->O.size()!=32){
            debug("error : Encrypt dict /O entry size is not 32");
            return false;
        }
    }
    else {
        debug("error : Encrypt dict does not have valid /O entry");
        return false;
    }
    obj = encrypt_dict->dict->get("P");
    if (isInt(obj)){
        this->perm = obj->integer;
    }
    // if any previous fails, id0 val will be empty
    obj = p_trailer->dict->get("ID");
    if (isArray(obj) && obj->array->count()==2) {
        PdfObject *id_obj = obj->array->at(0);
        this->id0 = pdfstr2bytes(id_obj->str, &str_type);
    }
    else {
        debug("error : failed to get trailer ID for decryption");
        return false;
    }
    return true;
}

bool
Crypt:: authenticateUserPassword(std::string password)
{
    // Using algorithm 3.2 (PDF 1.4)
    // pad or truncate entered password to exactly 32 bytes
    std::string pwd = password + std::string(padding_str, 32);
    pwd.resize(32);
    pwd += O;  // append /O entry from Encrypt dictionary
    pwd.append((char*)&perm, 4);// append /P entry as 4 byte little-endian integer
    pwd += id0;// append first character from trailer /ID entry
    MD5 hash(pwd);
    if (revision==3){
        for (int i=0; i<50; i++){
            hash = MD5(std::string((char*)hash.digest, keylen));
        }
    }
    encryption_key = std::string((char*)hash.digest, keylen);

    if (U.empty())
        return true;

    if (revision==2){
        char tmp_U[32];
        memcpy(tmp_U, padding_str, 32);
        RC4 rc4(encryption_key);
        rc4.crypt((uchar*)tmp_U, 32);
        if (strncmp(tmp_U, U.data(), 32)==0)
            return true;
    }
    else if (revision==3) {
        std::string str(padding_str, 32);
        str += id0;
        MD5 hash(str);
        RC4 rc4(encryption_key);
        rc4.crypt(hash.digest, 16);

        char rc4_key[128];
        for (int i=1; i<=19; i++) {
            for (int j=0; j<keylen; j++){
                rc4_key[j] = encryption_key[j] ^ (char)i;
            }
            rc4 = RC4(std::string(rc4_key, keylen));
            rc4.crypt(hash.digest, 16);
        }
        if (strncmp((char*)hash.digest, U.data(), 16)==0)
            return true;
    }
    return false;
}

bool
Crypt:: authenticate(const char *password)
{
    if (authenticateUserPassword(password))
        return true;
    // if it is not user password, then check if it is owner password
    // step 1 to 4 of algorithm 3.3 (PDF 1.4)
    std::string str(password);
    str += std::string(padding_str, 32);
    str.resize(32);
    MD5 hash(str);
    if (revision==3){
        for (int i=0; i<50; i++){
            hash = MD5(std::string((char*)hash.digest, 16));
        }
    }
    encryption_key = std::string((char*)hash.digest, keylen);

    // algorithm 3.7 (PDF 1.4)
    if (revision==2) {
        char tmp_O[32];
        memcpy(tmp_O, O.data(), 32);
        RC4 rc4(encryption_key);
        rc4.crypt((uchar*)tmp_O, 32);
        return authenticateUserPassword(std::string(tmp_O, 32));
    }
    else if (revision==3) {
        char tmp_O[32];
        memcpy(tmp_O, O.data(), 32);
        char rc4_key[128];
        for (int i=19; i>=0; i--){
            for (int j=0; j<keylen; j++){
                rc4_key[j] = encryption_key[j] ^ (char)i;
            }
            RC4 rc4(std::string(rc4_key, keylen));
            rc4.crypt((uchar*)tmp_O, 32);
        }
        return authenticateUserPassword(std::string(tmp_O,32));
    }
    return false;
}

void decryptObject(PdfObject *obj, std::string key)
{
    RC4 rc4(key);

    switch (obj->type){
        case PDF_OBJ_STR:
            if (obj->str.len>0 && obj->str.data!=NULL){
                int str_type;
                std::string str = pdfstr2bytes(obj->str, &str_type);
                char *str_data = (char*) malloc2(str.size());
                memcpy(str_data, str.data(), str.size());
                rc4.crypt((uchar*)str_data, str.size());
                bytes2pdfstr(std::string(str_data, str.size()), obj->str, str_type);
            }
            return;
        case PDF_OBJ_ARRAY:
            for (auto child : *obj->array) {
                decryptObject(child, key);
            }
            return;
        case PDF_OBJ_DICT:
            for (auto it : *obj->dict){
                decryptObject(it.second, key);
            }
            return;
        case PDF_OBJ_STREAM:
            if (obj->stream->len>0 && obj->stream->stream){
                rc4.crypt((uchar*)obj->stream->stream, obj->stream->len);
            }
            for (auto it : obj->stream->dict){
                decryptObject(it.second, key);
            }
            return;
        default:
            return;
    }
}

void
Crypt:: decryptIndirectObject(PdfObject *obj, int obj_no, int gen_no)
{
    std::string key_str = encryption_key;
    key_str.append((char*)&obj_no, 3);
    key_str.append((char*)&gen_no, 2);
    int rc4key_len = key_str.size() > 16 ? 16 : key_str.size();
    MD5 hash(key_str);
    key_str = std::string((char*)hash.digest, rc4key_len);
    decryptObject(obj, key_str);
}


// ****************** ARC4 Algorithm Class *****************

#define swap_byte(x,y) t = *(x); *(x) = *(y); *(y) = t


RC4:: RC4(std::string key)
{
    uchar t;
    int keylen = key.size();

    for (short i=0; i<256; i++)
        init_state[i] = i;

    for (short i=0, j=0; i<256; i++) {
        j = (j + init_state[i] + key[i%keylen]) % 256;
        swap_byte(&init_state[i], &init_state[j]);
    }
}

void
RC4:: crypt(uchar *data, int len)
{
    uchar state[256];
    for (int i=0; i<256; i++){
        state[i] = init_state[i];
    }
    uchar t, xorIndex, x=0, y=0;

    for (int i=0; i<len; i++) {
        x = (x + 1) % 256;
        y = (state[x] + y) % 256;
        swap_byte(&state[x], &state[y]);
        xorIndex = (state[x] + state[y]) % 256;
        data[i] ^= state[xorIndex];
    }
}


/* The following licence is applicable to the follwing part of code only */
/* MD5
 converted to C++ class by Frank Thilo (thilo@unix-ag.org)
 for bzflag (http://www.bzflag.org)

   based on:

   md5.h and md5.c
   reference implemantion of RFC 1321

   Copyright (C) 1991-2, RSA Data Security, Inc. Created 1991. All
rights reserved.

License to copy and use this software is granted provided that it
is identified as the "RSA Data Security, Inc. MD5 Message-Digest
Algorithm" in all material mentioning or referencing this software
or this function.

License is also granted to make and use derivative works provided
that such works are identified as "derived from the RSA Data
Security, Inc. MD5 Message-Digest Algorithm" in all material
mentioning or referencing the derived work.

RSA Data Security, Inc. makes no representations concerning either
the merchantability of this software or the suitability of this
software for any particular purpose. It is provided "as is"
without express or implied warranty of any kind.

These notices must be retained in any copies of any part of this
documentation and/or software.
*/



// Constants for MD5Transform routine.
#define S11 7
#define S12 12
#define S13 17
#define S14 22
#define S21 5
#define S22 9
#define S23 14
#define S24 20
#define S31 4
#define S32 11
#define S33 16
#define S34 23
#define S41 6
#define S42 10
#define S43 15
#define S44 21

///////////////////////////////////////////////

// F, G, H and I are basic MD5 functions.
inline MD5::uint4 MD5::F(uint4 x, uint4 y, uint4 z) {
  return (x&y) | (~x&z);
}

inline MD5::uint4 MD5::G(uint4 x, uint4 y, uint4 z) {
  return (x&z) | (y&~z);
}

inline MD5::uint4 MD5::H(uint4 x, uint4 y, uint4 z) {
  return x^y^z;
}

inline MD5::uint4 MD5::I(uint4 x, uint4 y, uint4 z) {
  return y ^ (x | ~z);
}

// rotate_left rotates x left n bits.
inline MD5::uint4 MD5::rotate_left(uint4 x, int n) {
  return (x << n) | (x >> (32-n));
}

// FF, GG, HH, and II transformations for rounds 1, 2, 3, and 4.
// Rotation is separate from addition to prevent recomputation.
inline void MD5::FF(uint4 &a, uint4 b, uint4 c, uint4 d, uint4 x, uint4 s, uint4 ac) {
  a = rotate_left(a+ F(b,c,d) + x + ac, s) + b;
}

inline void MD5::GG(uint4 &a, uint4 b, uint4 c, uint4 d, uint4 x, uint4 s, uint4 ac) {
  a = rotate_left(a + G(b,c,d) + x + ac, s) + b;
}

inline void MD5::HH(uint4 &a, uint4 b, uint4 c, uint4 d, uint4 x, uint4 s, uint4 ac) {
  a = rotate_left(a + H(b,c,d) + x + ac, s) + b;
}

inline void MD5::II(uint4 &a, uint4 b, uint4 c, uint4 d, uint4 x, uint4 s, uint4 ac) {
  a = rotate_left(a + I(b,c,d) + x + ac, s) + b;
}


// default ctor, just initailize
MD5::MD5()
{
    init();
}

// nifty shortcut ctor, compute MD5 for string and finalize it right away
MD5::MD5(const std::string &text)
{
    init();
    update(text.data(), text.length());
    finalize();
}

void MD5::init()
{
    finalized=false;

    count[0] = 0;
    count[1] = 0;
    // load magic initialization constants.
    state[0] = 0x67452301;
    state[1] = 0xefcdab89;
    state[2] = 0x98badcfe;
    state[3] = 0x10325476;
}


// decodes input (unsigned char) into output (uint4). Assumes len is a multiple of 4.
void MD5::decode(uint4 output[], const uint1 input[], size_t len)
{
    for (unsigned int i = 0, j = 0; j < len; i++, j += 4) {
        output[i] = ((uint4)input[j]) | (((uint4)input[j+1]) << 8) |
                    (((uint4)input[j+2]) << 16) | (((uint4)input[j+3]) << 24);
    }
}


// encodes input (uint4) into output (unsigned char). Assumes len is
// a multiple of 4.
void MD5::encode(uint1 output[], const uint4 input[], size_t len)
{
    for (size_t i = 0, j = 0; j < len; i++, j += 4) {
        output[j] = input[i] & 0xff;
        output[j+1] = (input[i] >> 8) & 0xff;
        output[j+2] = (input[i] >> 16) & 0xff;
        output[j+3] = (input[i] >> 24) & 0xff;
    }
}


// apply MD5 algo on a block
void MD5::transform(const uint1 block[blocksize])
{
    uint4 a = state[0], b = state[1], c = state[2], d = state[3], x[16];
    decode (x, block, blocksize);

    /* Round 1 */
    FF (a, b, c, d, x[ 0], S11, 0xd76aa478); /* 1 */
    FF (d, a, b, c, x[ 1], S12, 0xe8c7b756); /* 2 */
    FF (c, d, a, b, x[ 2], S13, 0x242070db); /* 3 */
    FF (b, c, d, a, x[ 3], S14, 0xc1bdceee); /* 4 */
    FF (a, b, c, d, x[ 4], S11, 0xf57c0faf); /* 5 */
    FF (d, a, b, c, x[ 5], S12, 0x4787c62a); /* 6 */
    FF (c, d, a, b, x[ 6], S13, 0xa8304613); /* 7 */
    FF (b, c, d, a, x[ 7], S14, 0xfd469501); /* 8 */
    FF (a, b, c, d, x[ 8], S11, 0x698098d8); /* 9 */
    FF (d, a, b, c, x[ 9], S12, 0x8b44f7af); /* 10 */
    FF (c, d, a, b, x[10], S13, 0xffff5bb1); /* 11 */
    FF (b, c, d, a, x[11], S14, 0x895cd7be); /* 12 */
    FF (a, b, c, d, x[12], S11, 0x6b901122); /* 13 */
    FF (d, a, b, c, x[13], S12, 0xfd987193); /* 14 */
    FF (c, d, a, b, x[14], S13, 0xa679438e); /* 15 */
    FF (b, c, d, a, x[15], S14, 0x49b40821); /* 16 */

    /* Round 2 */
    GG (a, b, c, d, x[ 1], S21, 0xf61e2562); /* 17 */
    GG (d, a, b, c, x[ 6], S22, 0xc040b340); /* 18 */
    GG (c, d, a, b, x[11], S23, 0x265e5a51); /* 19 */
    GG (b, c, d, a, x[ 0], S24, 0xe9b6c7aa); /* 20 */
    GG (a, b, c, d, x[ 5], S21, 0xd62f105d); /* 21 */
    GG (d, a, b, c, x[10], S22,  0x2441453); /* 22 */
    GG (c, d, a, b, x[15], S23, 0xd8a1e681); /* 23 */
    GG (b, c, d, a, x[ 4], S24, 0xe7d3fbc8); /* 24 */
    GG (a, b, c, d, x[ 9], S21, 0x21e1cde6); /* 25 */
    GG (d, a, b, c, x[14], S22, 0xc33707d6); /* 26 */
    GG (c, d, a, b, x[ 3], S23, 0xf4d50d87); /* 27 */
    GG (b, c, d, a, x[ 8], S24, 0x455a14ed); /* 28 */
    GG (a, b, c, d, x[13], S21, 0xa9e3e905); /* 29 */
    GG (d, a, b, c, x[ 2], S22, 0xfcefa3f8); /* 30 */
    GG (c, d, a, b, x[ 7], S23, 0x676f02d9); /* 31 */
    GG (b, c, d, a, x[12], S24, 0x8d2a4c8a); /* 32 */

    /* Round 3 */
    HH (a, b, c, d, x[ 5], S31, 0xfffa3942); /* 33 */
    HH (d, a, b, c, x[ 8], S32, 0x8771f681); /* 34 */
    HH (c, d, a, b, x[11], S33, 0x6d9d6122); /* 35 */
    HH (b, c, d, a, x[14], S34, 0xfde5380c); /* 36 */
    HH (a, b, c, d, x[ 1], S31, 0xa4beea44); /* 37 */
    HH (d, a, b, c, x[ 4], S32, 0x4bdecfa9); /* 38 */
    HH (c, d, a, b, x[ 7], S33, 0xf6bb4b60); /* 39 */
    HH (b, c, d, a, x[10], S34, 0xbebfbc70); /* 40 */
    HH (a, b, c, d, x[13], S31, 0x289b7ec6); /* 41 */
    HH (d, a, b, c, x[ 0], S32, 0xeaa127fa); /* 42 */
    HH (c, d, a, b, x[ 3], S33, 0xd4ef3085); /* 43 */
    HH (b, c, d, a, x[ 6], S34,  0x4881d05); /* 44 */
    HH (a, b, c, d, x[ 9], S31, 0xd9d4d039); /* 45 */
    HH (d, a, b, c, x[12], S32, 0xe6db99e5); /* 46 */
    HH (c, d, a, b, x[15], S33, 0x1fa27cf8); /* 47 */
    HH (b, c, d, a, x[ 2], S34, 0xc4ac5665); /* 48 */

    /* Round 4 */
    II (a, b, c, d, x[ 0], S41, 0xf4292244); /* 49 */
    II (d, a, b, c, x[ 7], S42, 0x432aff97); /* 50 */
    II (c, d, a, b, x[14], S43, 0xab9423a7); /* 51 */
    II (b, c, d, a, x[ 5], S44, 0xfc93a039); /* 52 */
    II (a, b, c, d, x[12], S41, 0x655b59c3); /* 53 */
    II (d, a, b, c, x[ 3], S42, 0x8f0ccc92); /* 54 */
    II (c, d, a, b, x[10], S43, 0xffeff47d); /* 55 */
    II (b, c, d, a, x[ 1], S44, 0x85845dd1); /* 56 */
    II (a, b, c, d, x[ 8], S41, 0x6fa87e4f); /* 57 */
    II (d, a, b, c, x[15], S42, 0xfe2ce6e0); /* 58 */
    II (c, d, a, b, x[ 6], S43, 0xa3014314); /* 59 */
    II (b, c, d, a, x[13], S44, 0x4e0811a1); /* 60 */
    II (a, b, c, d, x[ 4], S41, 0xf7537e82); /* 61 */
    II (d, a, b, c, x[11], S42, 0xbd3af235); /* 62 */
    II (c, d, a, b, x[ 2], S43, 0x2ad7d2bb); /* 63 */
    II (b, c, d, a, x[ 9], S44, 0xeb86d391); /* 64 */

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;

    // Zeroize sensitive information.
    memset(x, 0, sizeof x);
}


// MD5 block update operation. Continues an MD5 message-digest
// operation, processing another message block
void MD5::update(const unsigned char input[], size_t length)
{
    // compute number of bytes mod 64
    size_t index = count[0] / 8 % blocksize;

    // Update number of bits
    if ((count[0] += (length << 3)) < (length << 3))
    count[1]++;
    count[1] += (length >> 29);

    // number of bytes we need to fill in buffer
    size_t firstpart = 64 - index;

    size_t i;

    // transform as many times as possible.
    if (length >= firstpart) {
        // fill buffer first, transform
        memcpy(&buffer[index], input, firstpart);
        transform(buffer);
        // transform chunks of blocksize (64 bytes)
        for (i = firstpart; i + blocksize <= length; i += blocksize)
            transform(&input[i]);

        index = 0;
    }
    else
        i = 0;

    // buffer remaining input
    memcpy(&buffer[index], &input[i], length-i);
}


// for convenience provide a verson with signed char
void MD5::update(const char input[], size_t length)
{
    update((const unsigned char*)input, length);
}


// MD5 finalization. Ends an MD5 message-digest operation, writing the
// the message digest and zeroizing the context.
MD5& MD5::finalize()
{
    static unsigned char padding[64] = {
        0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    };

    if (!finalized) {
        // Save number of bits
        unsigned char bits[8];
        encode(bits, count, 8);

        // pad out to 56 mod 64.
        size_t index = count[0] / 8 % 64;
        size_t padLen = (index < 56) ? (56 - index) : (120 - index);
        update(padding, padLen);

        // Append length (before padding)
        update(bits, 8);

        // Store state in digest
        encode(digest, state, 16);

        // Zeroize sensitive information.
        memset(buffer, 0, sizeof buffer);
        memset(count, 0, sizeof count);

        finalized=true;
    }
    return *this;
}

