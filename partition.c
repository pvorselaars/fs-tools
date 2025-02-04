#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mbr.h"

static long disk_size;

int get_disk_size(const char *filename)
{
	FILE *disk = fopen(filename, "rb");
	if (!disk) {
		return -1;
	}

	fseek(disk, 0, SEEK_END);
	disk_size = ftell(disk);

	return 0;
}

int read_mbr(const char *filename, mbr_t * mbr)
{
	FILE *disk = fopen(filename, "rb");
	if (!disk) {
		return -1;
	}

	if (!fread(mbr, sizeof(mbr_t), 1, disk)) {
		return -2;
	}

	fclose(disk);
	return 0;
}

int write_mbr(const char *filename, mbr_t * mbr)
{
	FILE *disk = fopen(filename, "rb+");
	if (!disk) {
		return -1;
	}

	if (!fwrite(mbr, sizeof(mbr_t), 1, disk)) {
		return -1;
	}

	fclose(disk);
	return 0;
}

void create_partition(mbr_t * mbr)
{
	int index, prev = 0;
	char bootable;

	for (int i = 0; i < NUM_PARTITIONS; i++) {
		if (mbr->partitions[i].type == 0 && mbr->partitions[i].size == 0) {
			index = i;
			break;
		}
	}

	printf("Partition ID (e.g., %d): ", index + 1);
	scanf(" %d", &index);

	if (index > 4 || index < 1) {
		printf("Invalid partition ID\n");
		return;
	}
	index--;

	partition_entry_t *entry = &mbr->partitions[index];

	if (index > 1)
		prev = index - 2;

	printf("Bootable partition (y/n): ");
	scanf(" %c", &bootable);
	entry->boot = (bootable == 'y') ? 0x80 : 0x00;

	printf("Partition type (e.g., 0x%02x): ", entry->type);
	scanf(" %hhx", &entry->type);

	printf("Start sector (e.g., %d): ", mbr->partitions[prev].start_lba + mbr->partitions[prev].size + 1);
	scanf(" %u", &entry->start_lba);

	printf("Number of sectors (e.g., %d): ", entry->size);
	scanf(" %u", &entry->size);

	if ((entry->start_lba + entry->size)*512 > disk_size){
		entry->size = disk_size/512 - entry->start_lba;
	}
}

void display_partitions(mbr_t * mbr)
{
	printf("%-10s %-10s %-10s %-10s %-4s\n", "Partition", "Bootable", "Start LBA", "Sectors", "Type");
	for (int i = 0; i < NUM_PARTITIONS; i++) {
		partition_entry_t *entry = &mbr->partitions[i];

		if (entry->type == 0 && entry->size == 0)
			continue;

		printf("%-10d %-10c %-10d %-10d 0x%02X\n", i + 1, entry->boot ? '*' : ' ', entry->start_lba,
		       entry->size, entry->type);
	}
}

void usage(char *bin)
{
	printf("Usage: %s [options] <file>\n", bin);
	printf("\n");
	printf("Options:\n");
	printf("  -h		show this help message and exit\n");
	printf("  -l		list partitions and exit\n");
}

int main(int argc, char *argv[])
{
	mbr_t mbr;
	int done = 0;
	char list_flag = 0;
	const char *filename = "disk.img";
	int c;

	while ((c = getopt(argc, argv, "hl")) != -1) {
		switch (c) {
		case 'l':
			list_flag = 1;
			break;

		case 'h':
			usage(argv[0]);
			return 0;
			break;

		case '?':
			usage(argv[0]);
			return 1;
			break;
		}
	}

	if (optind < argc) {
		filename = argv[optind];
	} else {
		usage(argv[0]);
		return 1;
	}

	// Read MBR from disk
	if (read_mbr(filename, &mbr) == -1) {
		fprintf(stderr, "%s: %s, %s\n", argv[0], filename, strerror(errno));
		return 1;
	}

	if (list_flag) {
		display_partitions(&mbr);
		return 0;
	}


	get_disk_size(filename);
	if (mbr.boot_signature != 0xaa55)
		mbr.boot_signature = 0xaa55;

	// Display MBR
	printf("Disk signature: %.8x\n", *mbr.disk_signature);

	char option;
	while (!done) {
		display_partitions(&mbr);
		printf("Do you want to create/edit a partition? (y/n): ");
		scanf(" %c", &option);

		switch (option) {
		case 'y':
			create_partition(&mbr);
			break;

		case 'n':
			done = 1;
			break;

		default:
			printf("Invalid input: %c\n", option);
			break;
		}

	}

	printf("Do you want to write the partition table back to disk (y/n)?: ");
	scanf(" %c", &option);

	if (option == 'y') {
		if (write_mbr(filename, &mbr) != 0) {
			fprintf(stderr, "%s: %s\n", argv[0], strerror(errno));
			return 1;
		}
	}

	return 0;
}
