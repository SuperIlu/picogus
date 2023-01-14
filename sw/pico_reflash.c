#include "pico_reflash.h"

#include <stdio.h>
#include <hardware/flash.h>

static union {
    uint8_t buf[512];
    struct UF2_Block {
        // 32 byte header
        uint32_t magicStart0;
        uint32_t magicStart1;
        uint32_t flags;
        uint32_t targetAddr;
        uint32_t payloadSize;
        uint32_t blockNo;
        uint32_t numBlocks;
        uint32_t fileSize; // or familyID;
        uint8_t data[476];
        uint32_t magicEnd;
    } uf2;
} uf2_buf;

static uint32_t pico_firmware_curByte = 0;
static uint32_t pico_firmware_curBlock = 0;
static uint32_t pico_firmware_numBlocks = 0;
static uint32_t pico_firmware_payloadSize = 0;

static pico_firmware_status_t pico_firmware_status = PICO_FIRMWARE_IDLE;

void pico_firmware_process_block(void)
{
    if (uf2_buf.uf2.magicStart0 != 0x0A324655 || uf2_buf.uf2.magicStart1 != 0x9E5D5157 || uf2_buf.uf2.magicEnd != 0x0AB16F30) {
        // Invalid UF2 file
        puts("Invalid UF2 data!");
        pico_firmware_stop(PICO_FIRMWARE_ERROR);
        return;
    }
    if (pico_firmware_curBlock == 0) {
        puts("Starting firmware write...");
        pico_firmware_numBlocks = uf2_buf.uf2.numBlocks;
        pico_firmware_payloadSize = uf2_buf.uf2.payloadSize;
        uint32_t totalSize = pico_firmware_numBlocks * pico_firmware_payloadSize;
        // Point of no return... erasing the flash!
        flash_range_erase(0, (totalSize / 4096) * 4096 + 4096);
    }
    flash_range_program(0, uf2_buf.uf2.data, pico_firmware_payloadSize);
    ++pico_firmware_curBlock;
    if (pico_firmware_curBlock == pico_firmware_numBlocks) {
        // Final block has been written
        puts("Final block written. Rebooting Pico...");

        // Reboot the Pico!
        // Undocumented method to reboot the Pico without messing around with the watchdog. Source:
        // https://forums.raspberrypi.com/viewtopic.php?p=1928868&sid=09bfd964ebd49cc6349581ced3b4b9b9#p1928868
        #define AIRCR_Register (*((volatile uint32_t*)(PPB_BASE + 0x0ED0C)))
        AIRCR_Register = 0x5FA0004;
    }
}

void pico_firmware_write(uint8_t data)
{
    if (pico_firmware_curByte == 0 && pico_firmware_curBlock == 0) {
        // Writing first byte
        pico_firmware_status = PICO_FIRMWARE_WRITING;
    }
    uf2_buf.buf[pico_firmware_curByte++] = data;
    if (pico_firmware_curByte == 512) {
        pico_firmware_process_block();
        pico_firmware_curByte = 0;
    }
}

void pico_firmware_stop(pico_firmware_status_t status)
{
    pico_firmware_curByte = 0;
    pico_firmware_curBlock = 0;
    pico_firmware_numBlocks = 0;
    pico_firmware_payloadSize = 0;
    pico_firmware_status = status;
}

pico_firmware_status_t pico_firmware_getStatus(void)
{
    return pico_firmware_status;
}
