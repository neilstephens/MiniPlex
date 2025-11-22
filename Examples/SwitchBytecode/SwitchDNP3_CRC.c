#include <stdint.h>

void computeCRC (uint8_t dataOctet, uint16_t* crcAccum);

int64_t getAddrs (uint8_t* buf, uint64_t n, uint64_t* src, uint64_t* dst)
{
    if(n<10)        //DNP3 header is 10 bytes
        return -1;
    if(buf[2] > n)  //DNP3 'Len' > buffer length
        return -1;

    uint16_t crc = 0; uint8_t idx;
    for (idx = 0; idx < 8; idx++)
        computeCRC(buf[idx],&crc);
    crc = ~crc;

    if(buf[idx++] == (uint8_t)crc && buf[idx] == (uint8_t)(crc>>8))
    {
        *src = (buf[7]<<8) | buf[6];
        *dst = (buf[5]<<8) | buf[4];
        return 0;
    }
    return -1;
}

void computeCRC (uint8_t dataOctet, uint16_t* crcAccum)
{
    uint8_t idx;
    unsigned short temp;
    #define REVPOLY 0xA6BC
    for (idx = 0; idx < 8; idx++)
    {
        temp = (*crcAccum ^ dataOctet) & 1;
        *crcAccum >>= 1;
        dataOctet >>= 1;
        if (temp)
            *crcAccum ^= REVPOLY;
    }
    #undef REVPOLY
}
