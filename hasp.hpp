#include <string>
#include <cstring>
#include "hook.h"
#include "config.h"

uint32_t haspSize = 0xD40;
uint8_t haspBuffer[0xD40];

defineHook(int, hasp_login, int a1, int a2, int* handle)
{
    printf("hasp_login\n");
    return 0;
}

defineHook(int, hasp_logout)
{
    printf("hasp_logout\n");
    return 0;
}

defineHook(int, hasp_encrypt)
{
    printf("hasp_encrypt\n");
    return 0;
}

defineHook(int, hasp_decrypt)
{
    printf("hasp_decrypt\n");
    return 0;
}

defineHook(int, hasp_get_size, int a1, int a2, int *a3)
{
    printf("hasp_get_size");
    *a3 = haspSize;
    return 0;
}

defineHook(int, hasp_read, uint32_t handle, uint32_t fileID, uint32_t offset, uint32_t length, uint8_t *buffer)
{
    memcpy(buffer, haspBuffer + offset, length);
    return 0;
}

defineHook(int, hasp_write, uint32_t handle, uint32_t fileID, uint32_t offset, uint32_t length, uint8_t *buffer)
{
    memcpy(haspBuffer + offset, buffer, length);
    return 0;
}

void generateHaspDongleData(const std::string& serial)
{
    memset(haspBuffer, 0, sizeof(haspBuffer));

    haspBuffer[0] = 0x01;
    haspBuffer[0x13] = 0x01;
    haspBuffer[0x17] = 0x0A;
    haspBuffer[0x1B] = 0x04;
    haspBuffer[0x1C] = 0x3B;
    haspBuffer[0x1D] = 0x6B;
    haspBuffer[0x1E] = 0x40;
    haspBuffer[0x1F] = 0x87;
    haspBuffer[0x23] = 0x01;
    haspBuffer[0x27] = 0x0A;
    haspBuffer[0x2B] = 0x04;
    haspBuffer[0x2C] = 0x3B;
    haspBuffer[0x2D] = 0x6B;
    haspBuffer[0x2E] = 0x40;
    haspBuffer[0x2F] = 0x87;
    memcpy(haspBuffer + 0xD00, serial.c_str(), 12);

    uint8_t crc = 0;
    for (int i = 0; i < 62; i++)
        crc += haspBuffer[0xD00 + i];

    haspBuffer[0xD3E] = crc & 0xFF;
    haspBuffer[0xD3F] = haspBuffer[0xD3E] ^ 0xFF;
}

void initHasp() {

    if (isTerminal) {
        generateHaspDongleData("267621990001"); // use test pcb serial
    } else {
        generateHaspDongleData("267620990001"); // drive, use test pcb serial
    }

    // MT5
    // @Function int hasp_login(int a1, void *src, int a3)
    enableHook(hasp_login, 0x8C4EC00);
    // @Function int hasp_get_size(int a1, int a2, int a3)
    enableHook(hasp_get_size, 0x8C4FB90);
    // @Function int hasp_encrypt(int a1, void *src, size_t n)
    enableHook(hasp_encrypt, 0x8C4ED8C);
    // @Function int hasp_logout(int a1)
    enableHook(hasp_logout, 0x8C4ECA0);
    // @Function int hasp_decrypt(int a1, void *src, size_t n)
    enableHook(hasp_decrypt, 0x8C4EE78);
    // @Function int hasp_read(int a1, int a2, int a3, int a4, int a5)
    enableHook(hasp_read, 0x8C4F9F8);
    // @Function int hasp_write(int a1, int a2, int a3, int a4, int a5)
    enableHook(hasp_write, 0x8C4FAC4);
}