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

/********************************************************************
 *
 * How I store data
 *
 * Every files and directories will be saved as the
 * form below. Their "data section" will be different for file and
 * directory.
 *
 * +------------------------------------+
 * |  hash  |  type  |  param  |  data  |
 * | 4bytes |  1byte |  1byte  |  1byte |
 * +------------------------------------+
 *
 * [[For directory]]
 * @hash: store hash of its name(with prefix path).
 * @type: store character 'D'.
 * @para: store the number of file(include subdirectories) in this
 * 	directory.
 * @data: indicate the first byte of the entire data chunck, below is
 * 	how data sector looks like when its type is 'D':
 *
 *	+--------------------------------------------------------------+
 *	| length of file(directory) name string | file(directory) path |
 *	| 1byte                                 | nbyte(s)             |
 *	+--------------------------------------------------------------+
 *
 * [[For file]]
 * @hash: store hash of its name(with prefix path).
 * @type: store character 'F'.
 * @para: not used for file type.
 * @data: indicate the first byte of the entire data chunck, below is
 * 	how data sector looks like when its type is 'F':
 *
 *	+-------------------------+
 *	| Entire data in the file |
 *	| nbyte(s)                |
 *	+-------------------------+
 *
 ********************************************************************/

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
	uint32_t size, w, hash, file_num;
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

			/* Write hash of folder name(with path) */
			hash = hash_djb2((const uint8_t *)fullpath +
					 strlen(prefix) + 1, cur_hash);
			for (i = 0; i < sizeof(hash) / sizeof(b); i++) {
				b = (hash >> (i * 8)) & 0xff;
				fwrite(&b, 1, 1, outfile);
			}

			/* Write type(D or F) */
			fwrite("D", 1, 1, outfile);

			/* Write number of files */
			file_num = 0;
			while(readdir(sub_dirp))
				file_num++;
			rewinddir(sub_dirp);

			for (i = 0; i < sizeof(file_num) / sizeof(b); i++) {
				b = (size >> (i * 8)) & 0xff;
				fwrite(&b, 1, 1, outfile);
			}

			/* Write all file name in the folder */
			while ((subdir_ent = readdir(sub_dirp))) {
				if (strcmp(subdir_ent->d_name, ".") == 0)
					continue;
				if (strcmp(subdir_ent->d_name, "..") == 0)
					continue;

				/* write hash of file path under this dir */
				strcpy(tmpPath, fullpath);
				strcat(tmpPath, subdir_ent->d_name);
				hash = hash_djb2((const uint8_t *)tmpPath +
						 strlen(prefix) + 1, cur_hash);
				for (i = 0; i < sizeof(hash) / sizeof(b); i++) {
					b = (hash >> (i * 8)) & 0xff;
					fwrite(&b, 1, 1, outfile);
			}
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

			/* Write type(D or F) */
			fwrite("F", 1, 1, outfile);

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
	char *dirname = (char*)".";
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
