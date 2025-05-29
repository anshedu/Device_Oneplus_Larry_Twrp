/*
 * Copyright (c) 2013,2016,2020 The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#define _LARGEFILE64_SOURCE /* enable lseek64() */

/******************************************************************************
 * INCLUDE SECTION
 ******************************************************************************/
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <linux/fs.h>
#include <limits.h>
#include <dirent.h>
#include <linux/kernel.h>
#include <asm/byteorder.h>
#include <map>
#include <vector>
#include <string>
#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif
#include <inttypes.h>


#define LOG_TAG "gpt-utils"
#include <cutils/log.h>
#include <cutils/properties.h>
#include "gpt-utils.h"
#include <zlib.h>
#include <endian.h>


/******************************************************************************
 * DEFINE SECTION
 ******************************************************************************/
#define BLK_DEV_FILE    "/dev/block/mmcblk0"
/* list the names of the backed-up partitions to be swapped */
/* extension used for the backup partitions - tzbak, abootbak, etc. */
#define BAK_PTN_NAME_EXT    "bak"
#define XBL_PRIMARY         "/dev/block/bootdevice/by-name/xbl"
#define XBL_BACKUP          "/dev/block/bootdevice/by-name/xblbak"
#define XBL_AB_PRIMARY      "/dev/block/bootdevice/by-name/xbl_a"
#define XBL_AB_SECONDARY    "/dev/block/bootdevice/by-name/xbl_b"
/* GPT defines */
#define MAX_LUNS                    26
//This will allow us to get the root lun path from the path to the partition.
//i.e: from /dev/block/sdaXXX get /dev/block/sda. The assumption here is that
//the boot critical luns lie between sda to sdz which is acceptable because
//only user added external disks,etc would lie beyond that limit which do not
//contain partitions that interest us here.
#define PATH_TRUNCATE_LOC (sizeof("/dev/block/sda") - 1)

//From /dev/block/sda get just sda
#define LUN_NAME_START_LOC (sizeof("/dev/block/") - 1)
#define BOOT_LUN_A_ID 1
#define BOOT_LUN_B_ID 2
/******************************************************************************
 * MACROS
 ******************************************************************************/


#define GET_4_BYTES(ptr)    ((uint32_t) *((uint8_t *)(ptr)) | \
        ((uint32_t) *((uint8_t *)(ptr) + 1) << 8) | \
        ((uint32_t) *((uint8_t *)(ptr) + 2) << 16) | \
        ((uint32_t) *((uint8_t *)(ptr) + 3) << 24))

#define GET_8_BYTES(ptr)    ((uint64_t) *((uint8_t *)(ptr)) | \
        ((uint64_t) *((uint8_t *)(ptr) + 1) << 8) | \
        ((uint64_t) *((uint8_t *)(ptr) + 2) << 16) | \
        ((uint64_t) *((uint8_t *)(ptr) + 3) << 24) | \
        ((uint64_t) *((uint8_t *)(ptr) + 4) << 32) | \
        ((uint64_t) *((uint8_t *)(ptr) + 5) << 40) | \
        ((uint64_t) *((uint8_t *)(ptr) + 6) << 48) | \
        ((uint64_t) *((uint8_t *)(ptr) + 7) << 56))

#define PUT_4_BYTES(ptr, y)   *((uint8_t *)(ptr)) = (y) & 0xff; \
        *((uint8_t *)(ptr) + 1) = ((y) >> 8) & 0xff; \
        *((uint8_t *)(ptr) + 2) = ((y) >> 16) & 0xff; \
        *((uint8_t *)(ptr) + 3) = ((y) >> 24) & 0xff;

/******************************************************************************
 * TYPES
 ******************************************************************************/
using namespace std;
enum gpt_state {
    GPT_OK = 0,
    GPT_BAD_SIGNATURE,
    GPT_BAD_CRC
};
//List of LUN's containing boot critical images.
//Required in the case of UFS devices
struct update_data {
     char lun_list[MAX_LUNS][PATH_MAX];
     uint32_t num_valid_entries;
};

int32_t set_boot_lun(char *sg_dev,uint8_t boot_lun_id);
/******************************************************************************
 * FUNCTIONS
 ******************************************************************************/
/**
 *  ==========================================================================
 *
 *  \brief  Read/Write len bytes from/to block dev
 *
 *  \param [in] fd      block dev file descriptor (returned from open)
 *  \param [in] rw      RW flag: 0 - read, != 0 - write
 *  \param [in] offset  block dev offset [bytes] - RW start position
 *  \param [in] buf     Pointer to the buffer containing the data
 *  \param [in] len     RW size in bytes. Buf must be at least that big
 *
 *  \return  0 on success
 *
 *  ==========================================================================
 */
static int blk_rw(int fd, int rw, int64_t offset, uint8_t *buf, unsigned len)
{
    int r;

    if (lseek64(fd, offset, SEEK_SET) < 0) {
        fprintf(stderr, "block dev lseek64 %" PRIi64 " failed: %s\n", offset,
                strerror(errno));
        return -1;
    }

    if (rw)
        r = write(fd, buf, len);
    else
        r = read(fd, buf, len);

    if (r < 0) {
        fprintf(stderr, "block dev %s failed: %s\n", rw ? "write" : "read",
                strerror(errno));
    } else {
        if (rw) {
            r = fsync(fd);
            if (r < 0)
                fprintf(stderr, "fsync failed: %s\n", strerror(errno));
        } else {
            r = 0;
        }
    }

    return r;
}



/**
 *  ==========================================================================
 *
 *  \brief  Search within GPT for partition entry with the given name
 *  or it's backup twin (name-bak).
 *
 *  \param [in] ptn_name        Partition name to seek
 *  \param [in] pentries_start  Partition entries array start pointer
 *  \param [in] pentries_end    Partition entries array end pointer
 *  \param [in] pentry_size     Single partition entry size [bytes]
 *
 *  \return  First partition entry pointer that matches the name or NULL
 *
 *  ==========================================================================
 */
static uint8_t *gpt_pentry_seek(const char *ptn_name,
                                const uint8_t *pentries_start,
                                const uint8_t *pentries_end,
                                uint32_t pentry_size)
{
    char     *pentry_name;
    unsigned  len = strlen(ptn_name);
    unsigned  i;
    char      name8[MAX_GPT_NAME_SIZE] = {0}; // initialize with null

    for (pentry_name = (char *) (pentries_start + PARTITION_NAME_OFFSET);
         pentry_name < (char *) pentries_end;
         pentry_name += pentry_size) {

        /* Partition names in GPT are UTF-16 - ignoring UTF-16 2nd byte */
        for (i = 0; i < sizeof(name8) / 2; i++)
            name8[i] = pentry_name[i * 2];
        name8[i] = '\0';

        if (!strncmp(ptn_name, name8, len)) {
            if (name8[len] == 0 || !strcmp(&name8[len], BAK_PTN_NAME_EXT))
                return (uint8_t *) (pentry_name - PARTITION_NAME_OFFSET);
        }
    }

    return NULL;
}



/**
 *  ==========================================================================
 *
 *  \brief  Swaps boot chain in GPT partition entries array
 *
 *  \param [in] pentries_start  Partition entries array start
 *  \param [in] pentries_end    Partition entries array end
 *  \param [in] pentry_size     Single partition entry size
 *
 *  \return  0 on success, 1 if no backup partitions found
 *
 *  ==========================================================================
 */
static int gpt_boot_chain_swap(const uint8_t *pentries_start,
                                const uint8_t *pentries_end,
                                uint32_t pentry_size)
{
    const char ptn_swap_list[][MAX_GPT_NAME_SIZE] = { PTN_SWAP_LIST };

    int backup_not_found = 1;
    unsigned i;

    for (i = 0; i < ARRAY_SIZE(ptn_swap_list); i++) {
        uint8_t *ptn_entry;
        uint8_t *ptn_bak_entry;
        uint8_t ptn_swap[PTN_ENTRY_SIZE];
        //Skip the xbl partition on UFS devices. That is handled
        //seperately.
        if (gpt_utils_is_ufs_device() && !strncmp(ptn_swap_list[i],
                                PTN_XBL,
                                strlen(PTN_XBL)))
            continue;

        ptn_entry = gpt_pentry_seek(ptn_swap_list[i], pentries_start,
                        pentries_end, pentry_size);
        if (ptn_entry == NULL)
            continue;

        ptn_bak_entry = gpt_pentry_seek(ptn_swap_list[i],
                        ptn_entry + pentry_size, pentries_end, pentry_size);
        if (ptn_bak_entry == NULL) {
            fprintf(stderr, "'%s' partition not backup - skip safe update\n",
                    ptn_swap_list[i]);
            continue;
        }

        /* swap primary <-> backup partition entries */
        memcpy(ptn_swap, ptn_entry, PTN_ENTRY_SIZE);
        memcpy(ptn_entry, ptn_bak_entry, PTN_ENTRY_SIZE);
        memcpy(ptn_bak_entry, ptn_swap, PTN_ENTRY_SIZE);
        backup_not_found = 0;
    }

    return backup_not_found;
}



/**
 *  ==========================================================================
 *
 *  \brief  Sets secondary GPT boot chain
 *
 *  \param [in] fd    block dev file descriptor
 *  \param [in] boot  Boot chain to switch to
 *
 *  \return  0 on success
 *
 *  ==========================================================================
 */
static int gpt2_set_boot_chain(int fd, enum boot_chain boot)
{
    int64_t  gpt2_header_offset;
    uint64_t pentries_start_offset;
    uint32_t gpt_header_size;
    uint32_t pentry_size;
    uint32_t pentries_array_size;

    uint8_t *gpt_header = NULL;
    uint8_t  *pentries = NULL;
    uint32_t crc;
    uint32_t crc_zero;
    uint32_t blk_size = 0;
    int r;


    crc_zero = crc32(0L, Z_NULL, 0);
    if (ioctl(fd, BLKSSZGET, &blk_size) != 0) {
            fprintf(stderr, "Failed to get GPT device block size: %s\n",
                            strerror(errno));
            r = -1;
            goto EXIT;
    }
    gpt_header = (uint8_t*)malloc(blk_size);
    if (!gpt_header) {
            fprintf(stderr, "Failed to allocate memory to hold GPT block\n");
            r = -1;
            goto EXIT;
    }
    gpt2_header_offset = lseek64(fd, 0, SEEK_END) - blk_size;
    if (gpt2_header_offset < 0) {
        fprintf(stderr, "Getting secondary GPT header offset failed: %s\n",
                strerror(errno));
        r = -1;
        goto EXIT;
    }

    /* Read primary GPT header from block dev */
    r = blk_rw(fd, 0, blk_size, gpt_header, blk_size);

    if (r) {
            fprintf(stderr, "Failed to read primary GPT header from blk dev\n");
            goto EXIT;
    }
    pentries_start_offset =
        GET_8_BYTES(gpt_header + PENTRIES_OFFSET) * blk_size;
    pentry_size = GET_4_BYTES(gpt_header + PENTRY_SIZE_OFFSET);
    pentries_array_size =
        GET_4_BYTES(gpt_header + PARTITION_COUNT_OFFSET) * pentry_size;

    pentries = (uint8_t *) calloc(1, pentries_array_size);
    if (pentries == NULL) {
        fprintf(stderr,
                    "Failed to alloc memory for GPT partition entries array\n");
        r = -1;
        goto EXIT;
    }
    /* Read primary GPT partititon entries array from block dev */
    r = blk_rw(fd, 0, pentries_start_offset, pentries, pentries_array_size);
    if (r)
        goto EXIT;

    crc = crc32(crc_zero, pentries, pentries_array_size);
    if (GET_4_BYTES(gpt_header + PARTITION_CRC_OFFSET) != crc) {
        fprintf(stderr, "Primary GPT partition entries array CRC invalid\n");
        r = -1;
        goto EXIT;
    }

    /* Read secondary GPT header from block dev */
    r = blk_rw(fd, 0, gpt2_header_offset, gpt_header, blk_size);
    if (r)
        goto EXIT;

    gpt_header_size = GET_4_BYTES(gpt_header + HEADER_SIZE_OFFSET);
    pentries_start_offset =
        GET_8_BYTES(gpt_header + PENTRIES_OFFSET) * blk_size;

    if (boot == BACKUP_BOOT) {
        r = gpt_boot_chain_swap(pentries, pentries + pentries_array_size,
                                pentry_size);
        if (r)
            goto EXIT;
    }

    crc = crc32(crc_zero, pentries, pentries_array_size);
    PUT_4_BYTES(gpt_header + PARTITION_CRC_OFFSET, crc);

    /* header CRC is calculated with this field cleared */
    PUT_4_BYTES(gpt_header + HEADER_CRC_OFFSET, 0);
    crc = crc32(crc_zero, gpt_header, gpt_header_size);
    PUT_4_BYTES(gpt_header + HEADER_CRC_OFFSET, crc);

    /* Write the modified GPT header back to block dev */
    r = blk_rw(fd, 1, gpt2_header_offset, gpt_header, blk_size);
    if (!r)
        /* Write the modified GPT partititon entries array back to block dev */
        r = blk_rw(fd, 1, pentries_start_offset, pentries,
                    pentries_array_size);

EXIT:
    if(gpt_header)
            free(gpt_header);
    if (pentries)
            free(pentries);
    return r;
}

/**
 *  ==========================================================================
 *
 *  \brief  Checks GPT state (header signature and CRC)
 *
 *  \param [in] fd      block dev file descriptor
 *  \param [in] gpt     GPT header to be checked
 *  \param [out] state  GPT header state
 *
 *  \return  0 on success
 *
 *  ==========================================================================
 */
static int gpt_get_state(int fd, enum gpt_instance gpt, enum gpt_state *state)
{
    int64_t gpt_header_offset;
    uint32_t gpt_header_size;
    uint8_t  *gpt_header = NULL;
    uint32_t crc;
    uint32_t crc_zero;
    uint32_t blk_size = 0;

    *state = GPT_OK;

    crc_zero = crc32(0L, Z_NULL, 0);
    if (ioctl(fd, BLKSSZGET, &blk_size) != 0) {
            fprintf(stderr, "Failed to get GPT device block size: %s\n",
                            strerror(errno));
            goto error;
    }
    gpt_header = (uint8_t*)malloc(blk_size);
    if (!gpt_header) {
            fprintf(stderr, "gpt_get_state:Failed to alloc memory for header\n");
            goto error;
    }
    if (gpt == PRIMARY_GPT)
        gpt_header_offset = blk_size;
    else {
        gpt_header_offset = lseek64(fd, 0, SEEK_END) - blk_size;
        if (gpt_header_offset < 0) {
            fprintf(stderr, "gpt_get_state:Seek to end of GPT part fail\n");
            goto error;
        }
    }

    if (blk_rw(fd, 0, gpt_header_offset, gpt_header, blk_size)) {
        fprintf(stderr, "gpt_get_state: blk_rw failed\n");
        goto error;
    }
    if (memcmp(gpt_header, GPT_SIGNATURE, sizeof(GPT_SIGNATURE)))
        *state = GPT_BAD_SIGNATURE;
    gpt_header_size = GET_4_BYTES(gpt_header + HEADER_SIZE_OFFSET);

    crc = GET_4_BYTES(gpt_header + HEADER_CRC_OFFSET);
    /* header CRC is calculated with this field cleared */
    PUT_4_BYTES(gpt_header + HEADER_CRC_OFFSET, 0);
    if (crc32(crc_zero, gpt_header, gpt_header_size) != crc)
        *state = GPT_BAD_CRC;
    free(gpt_header);
    return 0;
error:
    if (gpt_header)
            free(gpt_header);
    return -1;
}



/**
 *  ==========================================================================
 *
 *  \brief  Sets GPT header state (used to corrupt and fix GPT signature)
 *
 *  \param [in] fd     block dev file descriptor
 *  \param [in] gpt    GPT header to be checked
 *  \param [in] state  GPT header state to set (GPT_OK or GPT_BAD_SIGNATURE)
 *
 *  \return  0 on success
 *
 *  ==========================================================================
 */
static int gpt_set_state(int fd, enum gpt_instance gpt, enum gpt_state state)
{
    int64_t gpt_header_offset;
    uint32_t gpt_header_size;
    uint8_t  *gpt_header = NULL;
    uint32_t crc;
    uint32_t crc_zero;
    uint32_t blk_size = 0;

    crc_zero = crc32(0L, Z_NULL, 0);
    if (ioctl(fd, BLKSSZGET, &blk_size) != 0) {
            fprintf(stderr, "Failed to get GPT device block size: %s\n",
                            strerror(errno));
            goto error;
    }
    gpt_header = (uint8_t*)malloc(blk_size);
    if (!gpt_header) {
            fprintf(stderr, "Failed to alloc memory for gpt header\n");
            goto error;
    }
    if (gpt == PRIMARY_GPT)
        gpt_header_offset = blk_size;
    else {
        gpt_header_offset = lseek64(fd, 0, SEEK_END) - blk_size;
        if (gpt_header_offset < 0) {
            fprintf(stderr, "Failed to seek to end of GPT device\n");
            goto error;
        }
    }
    if (blk_rw(fd, 0, gpt_header_offset, gpt_header, blk_size)) {
        fprintf(stderr, "Failed to r/w gpt header\n");
        goto error;
    }
    if (state == GPT_OK)
        memcpy(gpt_header, GPT_SIGNATURE, sizeof(GPT_SIGNATURE));
    else if (state == GPT_BAD_SIGNATURE)
        *gpt_header = 0;
    else {
        fprintf(stderr, "gpt_set_state: Invalid state\n");
        goto error;
    }

    gpt_header_size = GET_4_BYTES(gpt_header + HEADER_SIZE_OFFSET);

    /* header CRC is calculated with this field cleared */
    PUT_4_BYTES(gpt_header + HEADER_CRC_OFFSET, 0);
    crc = crc32(crc_zero, gpt_header, gpt_header_size);
    PUT_4_BYTES(gpt_header + HEADER_CRC_OFFSET, crc);

    if (blk_rw(fd, 1, gpt_header_offset, gpt_header, blk_size)) {
        fprintf(stderr, "gpt_set_state: blk write failed\n");
        goto error;
    }
    return 0;
error:
    if(gpt_header)
           free(gpt_header);
    return -1;
}

int get_scsi_node_from_bootdevice(const char *bootdev_path,
                char *sg_node_path,
                size_t buf_size)
{
        char sg_dir_path[PATH_MAX] = {0};
        char real_path[PATH_MAX] = {0};
        DIR *scsi_dir = NULL;
        struct dirent *de;
        int node_found = 0;
        if (!bootdev_path || !sg_node_path) {
                fprintf(stderr, "%s : invalid argument\n",
                                 __func__);
                goto error;
        }
        if (readlink(bootdev_path, real_path, sizeof(real_path) - 1) < 0) {
                        fprintf(stderr, "failed to resolve link for %s(%s)\n",
                                        bootdev_path,
                                        strerror(errno));
                        goto error;
        }
        if(strlen(real_path) < PATH_TRUNCATE_LOC + 1){
            fprintf(stderr, "Unrecognized path :%s:\n",
                           real_path);
            goto error;
        }
        //For the safe side in case there are additional partitions on
        //the XBL lun we truncate the name.
        real_path[PATH_TRUNCATE_LOC] = '\0';
        if(strlen(real_path) < LUN_NAME_START_LOC + 1){
            fprintf(stderr, "Unrecognized truncated path :%s:\n",
                           real_path);
            goto error;
        }
        //This will give us /dev/block/sdb/device/scsi_generic
        //which contains a file sgY whose name gives us the path
        //to /dev/sgY which we return
        sn
