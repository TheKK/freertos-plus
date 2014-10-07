#ifdef _WIN32
#include <windows.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <dirent.h>
#include <string.h>
#include <getopt.h>

#define HASH_INIT	5381
#define MAX_FULL_PATH	1024
#define MAX_BUFFER_SIZE	16 * 1024

uint32_t
hash_djb2(const uint8_t * str, uint32_t hash)
{
	int c;

	while ((c = *str++))
		hash = ((hash << 5) + hash) ^ c;

	return hash;
}

void
usage(const char *binname)
{
	printf("Usage: %s [-d <dir>] [outfile]\n", binname);
}

void
processdir(DIR * dirp, const char *curpath, FILE * outfile,
		const char *prefix)
{
	char fullpath[MAX_FULL_PATH];
	char tmpPath[MAX_FULL_PATH];
	char buf[MAX_BUFFER_SIZE];
	struct dirent *ent, *subdir_ent;
	DIR *rec_dirp, *sub_dirp;
	uint32_t cur_hash = hash_djb2((const uint8_t *)curpath, HASH_INIT);
	uint32_t size, w, hash;
	uint8_t b, i;
	FILE *infile;

	while ((ent = readdir(dirp))) {
		strcpy(fullpath, prefix);
		strcat(fullpath, "/");
		strcat(fullpath, curpath);
		strcat(fullpath, ent->d_name);
#ifdef _WIN32
		if (GetFileAttributes(fullpath) & FILE_ATTRIBUTE_DIRECTORY) {
#else
		if (ent->d_type == DT_DIR) {
#endif
			/* Ignore . and .. */
			if (strcmp(ent->d_name, ".") == 0)
				continue;
			if (strcmp(ent->d_name, "..") == 0)
				continue;

			/*
			 * My work here
			 */
			strcat(fullpath, "/");
			sub_dirp = opendir(fullpath);

			/* Write folder name(with path) */
			fwrite(fullpath + strlen(prefix) + 1, 1,
			       strlen(fullpath) - strlen(prefix) - 1, outfile);
			fwrite(": ", 1, 2, outfile);

			/* Write all file name in the folder */
			while ((subdir_ent = readdir(sub_dirp))) {
				if (strcmp(subdir_ent->d_name, ".") == 0)
					continue;
				if (strcmp(subdir_ent->d_name, "..") == 0)
					continue;

				if (subdir_ent->d_type == DT_DIR)
					fwrite("D", 1, 1, outfile);
				else
					fwrite("F", 1, 1, outfile);

				strcpy(tmpPath, fullpath);
				strcat(tmpPath, subdir_ent->d_name);
				fwrite(tmpPath + strlen(prefix) + 1, 1,
				       strlen(tmpPath) - strlen(prefix) - 1,
				       outfile);
				printf("j");
			}
			closedir(sub_dirp);

			rec_dirp = opendir(fullpath);
			processdir(rec_dirp, fullpath + strlen(prefix) + 1,
				   outfile, prefix);
			closedir(rec_dirp);
		} else { /* ent->d_type != DT_DIR */
			hash =
			    hash_djb2((const uint8_t *)ent->d_name, cur_hash);

			infile = fopen(fullpath, "rb");
			if (!infile) {
				perror("opening input file");
				exit(EXIT_FAILURE);
			}

			/* Write hash */
			for (i = 0; i < sizeof(hash) / sizeof(b); i++) {
				b = (hash >> (i * 8)) & 0xff;	// little-endian
				fwrite(&b, 1, 1, outfile);
			}

			/* Get file size */
			fseek(infile, 0, SEEK_END);
			size = ftell(infile);
			fseek(infile, 0, SEEK_SET);

			/* Write file size */
			for (i = 0; i < sizeof(size) / sizeof(b); i++) {
				b = (size >> (i * 8)) & 0xff;
				fwrite(&b, 1, 1, outfile);
			}

			/* Write file content */
			while (size) {
				w = size > MAX_BUFFER_SIZE ?
					MAX_BUFFER_SIZE : size;

				fread(buf, 1, w, infile);
				fwrite(buf, 1, w, outfile);

				size -= w;
			}

			fclose(infile);
		}
	}
}

int
main(int argc, char **argv)
{
	char *binname = argv[0];
	char *outname = NULL;
	char *dirname = ".";
	uint64_t z = 0;
	FILE *outfile;
	DIR *dirp;
	int opt;

	/* Get input options */
	while ((opt = getopt(argc, argv, "d:o:h")) != -1) {
		switch (opt) {
		case 'd':
			dirname = optarg;
			break;
		case 'o':
			outname = optarg;
			break;
		case 'h':
			usage(binname);
			exit(EXIT_SUCCESS);
			break;
		default:
			usage(binname);
			exit(EXIT_FAILURE);
			break;
		}
	}

	/* Start working! */
	if (!outname)
		outfile = stdout;
	else {
		outfile = fopen(outname, "wb");
		if (!outfile) {
			perror("opening output file");
			exit(EXIT_FAILURE);
		}
	}

	dirp = opendir(dirname);
	if (!dirp) {
		perror("opening directory");
		exit(EXIT_FAILURE);
	}

	processdir(dirp, "", outfile, dirname);
	fwrite(&z, 1, 8, outfile);
	if (outname)
		fclose(outfile);
	closedir(dirp);

	return 0;
}