/*
 * Copyright (C) 2015 Hardkernel Co,. Ltd
 * Dongjin Kim <tobetter@gmail.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <command.h>
#include <asm/errno.h>
#include <malloc.h>
#include <linux/ctype.h>    /* isalpha, isdigit */
#include <linux/sizes.h>

#ifdef CONFIG_SYS_HUSH_PARSER
#include <cli_hush.h>
#endif

#define BOOTINI_MAGIC	"ODROIDC2-UBOOT-CONFIG"
#define SZ_BOOTINI		SZ_64K

/* Nothing to proceed with zero size string or comment.
 *
 * FIXME: Do we really need to strip the line start with '#' or ';',
 *        since any U-boot command does not start with punctuation character.
 */
static int valid_command(const char* p)
{
	char *q;

	for (q = (char*)p; *q; q++) {
		if (isblank(*q)) continue;
		if (isalnum(*q)) return 1;
		if (ispunct(*q))
			return (*q != '#') && (*q != ';');
	}

	return !(p == q);
}

/* Read boot.ini from FAT partition
 */
static char* read_iniload(char* interface, char* dev, char* addr, char* filename)
{
	char cmd[128];
	unsigned long filesize;
	char *p;

	p = addr;
	
	setenv("filesize", "0");

	sprintf(cmd, "fatload %s %s 0x%p %s", interface, dev, (void *)p, filename);
	run_command(cmd, 0);

	filesize = getenv_ulong("filesize", 16, 0);
	if (0 == filesize) {
		printf("iniload: no %s or empty file on %s %s\n", filename, interface, dev);
		return NULL;
	}

	if (filesize > SZ_BOOTINI) {
		printf("iniload: %s exceeds %d, size=%ld\n",
				filename, SZ_BOOTINI, filesize);
		return NULL;
    }

	/* Terminate the read buffer with '\0' to be treated as string */
	*(char *)(p + filesize) = '\0';

	/* Scan MAGIC string, readed boot.ini must start with exact magic string.
	 * Otherwise, we will not proceed at all.
	 */
	while (*p) {
		char *s = strsep(&p, "\n");
		if (!valid_command(s))
			continue;

		/* MAGIC string is discovered, return the buffer address of next to
		 * proceed the commands.
		 */
		if (!strncmp(s, BOOTINI_MAGIC, sizeof(BOOTINI_MAGIC)))
			return memcpy(malloc(filesize), p, filesize);
	}

	printf("iniload: MAGIC NAME, " BOOTINI_MAGIC ", is not found!!\n");

	return NULL;
}

extern int is_hdmimode_valid(const char *);

static int load_iniload(char* interface, char* dev, char* addr, char* filename)
{
	char *p;
	char *cmd;

	p = read_iniload(interface, dev, addr, filename);
	if (NULL == p) {
		printf("iniload: problem reading specified file\n");
		return 0;
	}

	//printf("cfgload: applying boot.ini...\n");

	while (p) {
		cmd = strsep(&p, "\n");
		if (!valid_command(cmd))
			continue;

		printf("iniload: %s\n", cmd);

#ifndef CONFIG_SYS_HUSH_PARSER
		run_command(cmd, 0);
#else
		parse_string_outer(cmd, FLAG_PARSE_SEMICOLON
				| FLAG_EXIT_FROM_LOOP);
#endif
		/* Check the hdmimode validation */
		/*
		if (strstr(cmd, "hdmimode")) {
			if (!is_hdmimode_valid(getenv("hdmimode")))
				setenv("hdmimode", "1080p60hz");
		}
		*/
	}

	return 0;	
}

static int do_load_cfgload(cmd_tbl_t *cmdtp, int flag, int argc,
		char *const argv[])
{
	char *p;
	//char *cmd;

	p = (char *)simple_strtoul(getenv("loadaddr"), NULL, 16);
	if (NULL == p)
		p = (char *)CONFIG_SYS_LOAD_ADDR;
	
	printf("cfgload: applying boot.ini...\n");
	return load_iniload("mmc", "0:1", p, "boot.ini");

}

static int do_load_iniload(cmd_tbl_t *cmdtp, int flag, int argc,
		char *const argv[])
{
	char *interface, *dev, *addr, *filename;

	// put default arguments, if necessary
	if(argc<2)
		interface = "mmc";
	else
		interface = argv[1];
	
	if(argc<3)
		dev = "0:1";
	else
		dev = argv[2];
	
	if(argc<4)
		filename = "boot.ini";
	else
		filename = argv[3];

	if(argc<5) {
		addr = (char *)simple_strtoul(getenv("loadaddr"), NULL, 16);
		if(addr == NULL) {
			addr = (char *)CONFIG_SYS_LOAD_ADDR;
		}
	}
	else
		addr = argv[4];
	
	// load file
	return load_iniload(interface, dev, addr, filename);
}

U_BOOT_CMD(
		cfgload,		1,		0,		do_load_cfgload,
		"read 'boot.ini' from FAT partiton",
		"\n"
		"    - read boot.ini from the first partiton treated as FAT partiton"
);

U_BOOT_CMD(
		iniload,    5,    0,    do_load_iniload,
		"read and apply ini file from FAT partition",
		"[<interface [<dev[:part]> [<filename> [<addr>]]]]\n"
		"    - read filename from specified FAT partition. Defaults as iniload mmc 0:1 boot.ini CONFIG_SYS_LOAD_ADDR"
);

/* vim: set ts=4 sw=4 tw=80: */
