/*
 *  Author: Halloweeks.
 *  Date: 2023-11-03.
 *  Description: Extract pubg mobile pak contents. 
 *  The program not support pubg mobile 1.1.0 and above version 
 *  pak extraction because pubg mobile developer using custom 
 *  encryption for encrypting pak index data it make unable to read data
 *  GitHub: https://github.com/halloweeks/UE4-Unpacker
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <time.h>
#include <zlib.h>
#include <zstd.h>
#include <errno.h>


// The key use for deobfuscation
#define OFFSET_KEY 0xA6D17AB4D4783A41
#define SIZE_KEY 0x1FFBEE0AB84D0C43

#define CHUNK_SIZE 65536

// Pak Header
typedef struct {
	uint8_t encrypted;
	uint32_t magic;
	uint32_t version;
	uint8_t hash[20];
	uint64_t size;
	uint64_t offset;
} __attribute__((packed)) PakInfo;

// Compression Blocks
typedef struct {
    uint64_t CompressedStart;
    uint64_t CompressedEnd;
} __attribute__((packed)) CompressionBlock;

// Function to decrypt data
void DecryptData(uint8_t *data, uint32_t size) {
	for (uint32_t index = 0; index < size; index++) {
		data[index] ^= 0x79u;
	}
}

// Function to create a file
int create_file(const char *fullPath) {
	const char *lastSlash = strrchr(fullPath, '/');
	if (lastSlash != NULL) {
		size_t length = (size_t)(lastSlash - fullPath);
		char path[length + 1];
		strncpy(path, fullPath, length);
		path[length] = '\0';
		char *token = strtok(path, "/");
		char currentPath[1024] = "";
		while (token != NULL) {
			strcat(currentPath, token);
			strcat(currentPath, "/");
			mkdir(currentPath, 0777);
			token = strtok(NULL, "/");
		}
	}
	return open(fullPath, O_WRONLY | O_CREAT | O_TRUNC, 0644); 
}

unsigned int ZLIB_decompress(unsigned char *InData, unsigned int InSize, unsigned char *OutData, unsigned int OutSize) {
	z_stream strm;
	strm.zalloc = Z_NULL;
	strm.zfree = Z_NULL;
	strm.opaque = Z_NULL;
	strm.next_in = InData;
	strm.avail_in = InSize;
	strm.next_out = OutData;
	strm.avail_out = OutSize;
	
	if (inflateInit(&strm) != Z_OK) {
		fprintf(stderr, "Failed to initialize zlib.\n");
		return 0;
	}
	
	if (inflate(&strm, Z_FINISH) != Z_STREAM_END) {
		fprintf(stderr, "inflate failed: %s\n", strm.msg);
		inflateEnd(&strm);
		return 0;
	}
	
	if (inflateEnd(&strm) != Z_OK) {
		fprintf(stderr, "inflateEnd failed: %s\n", strm.msg);
		return 0;
	}
	
	return strm.total_out;
}


// Global variable to track current index offset during reading
uint64_t current_index_offset = 0;

// Function to read data from the index
void read_data(void *destination, const uint8_t *source, size_t length) {
	memcpy(destination, source + current_index_offset, length);
	current_index_offset += length;
}

int unicode_to_utf8(const char *input, size_t input_len, char *out, size_t output_len) {
	char output[output_len];
	
    size_t i = 0, j = 0;
    
    while (i < input_len && j < output_len) {
        unsigned int unicode_char;
        
        // Read a UTF-16LE character
        unicode_char = (unsigned char)input[i++];
        unicode_char |= (unsigned char)input[i++] << 8;
        
        // Convert UTF-16LE to UTF-8
        if (unicode_char <= 0x7F) {
            output[j++] = (char)unicode_char;
        } else if (unicode_char <= 0x7FF) {
            if (j + 1 >= output_len) return -1; // Output buffer too small
            output[j++] = 0xC0 | (unicode_char >> 6);
            output[j++] = 0x80 | (unicode_char & 0x3F);
        } else {
            if (j + 2 >= output_len) return -1; // Output buffer too small
            output[j++] = 0xE0 | (unicode_char >> 12);
            output[j++] = 0x80 | ((unicode_char >> 6) & 0x3F);
            output[j++] = 0x80 | (unicode_char & 0x3F);
        }
    }
    
    if (i < input_len) return -1; // Input buffer not fully consumed
    
    memcpy(out, output, j);
    return 0;
}

// toggle xor obfuscation and deobfuscation 
void xor_obfuscation(PakInfo *info) {
	uint8_t hash_xor[20] = {
		0x8C, 0xD3, 0xA0, 0x5A, 0xD3, 0x64, 0x53, 0xDE, 0xED, 0xA8,
		0xCA, 0x59, 0x26, 0xC6, 0x95, 0x54, 0x84, 0x25, 0x9B, 0xE0
	};
	
	info->encrypted ^= 0x01;
	info->magic ^= 0xA0116E7;
	info->version ^= 0x00;
	for (uint8_t i = 0; i < 20; i++) {
		info->hash[i] ^= hash_xor[i];
	}
	info->offset ^= 0xA6D17AB4D4783A41;
	info->size ^= 0x1FFBEE0AB84D0C43;
}

int main(int argc, const char *argv[]) {
	clock_t start = clock();
	PakInfo info;
	CompressionBlock Block[500];

	if (argc != 2) {
		fprintf(stderr, "Usage %s <pak_file>\n", argv[0]);
		return 1;
	}

	if (access(argv[1], F_OK) == -1) {
		fprintf(stderr, "Input %s file does not exist.\n", argv[1]);
		return 1;
	}
	
	int PakFile = open(argv[1], O_RDONLY);
	
	if (PakFile == -1) {
		printf("Unable to open %s file\n", argv[1]);
		return 1;
	}
	
	if (lseek(PakFile, -45, SEEK_END) == -1) {
		printf("failed to seek file position\n");
		return 1;
	}
	
	if (read(PakFile, &info, 45) != 45) {
		printf("Failed to read pak header at -45\n");
		return 1;
	}
	
	xor_obfuscation(&info);
	
	if (info.version < 6 || info.version > 8) {
		fprintf(stderr, "This pak version unsupported! version=%u\n", info.version);
		close(PakFile);
        return EXIT_FAILURE;
	}
	
	// check if index data size is greater than 50MB
	if (info.size > 52428800) {
		fprintf(stderr, "Corrupted index offset and index size.\n");
		close(PakFile);
		return 1;
	}
	
	uint8_t *IndexData = (uint8_t*)malloc(info.size);
	
	if (!IndexData) {
		printf("Memory allocation failed.\n");
		close(PakFile);
		return 1;
	}

	if (pread(PakFile, IndexData, info.size, info.offset) != info.size) {
		fprintf(stderr, "Failed to load index data\n");
		return 1;
	}

	// Decrypt if necessary
	if (info.encrypted) {
		DecryptData(IndexData, info.size);
	}
	
	uint32_t MountPointLength;
	char MountPoint[1024];
	int NumOfEntry;
	int32_t FilenameSize;
	char Filename[1024];
	char fname[1024];
	uint8_t FileHash[20];
	uint64_t FileOffset = 0;
	uint64_t FileSize = 0;
	int CompressionMethod = 0;
	uint64_t CompressedLength = 0;
	size_t DecompressLength = 0;
	uint8_t Dummy[21];
	int NumOfBlocks = 0;
	uint8_t Encrypted;
	uint32_t CompressedBlockSize = 0;
	
	// hold compressed and uncompressed data
	uint8_t CompressedData[CHUNK_SIZE];
	uint8_t DecompressedData[CHUNK_SIZE];
	
	printf("offset\t\tfilesize\t\tfilename\n");
	printf("--------------------------------------\n");
	
	read_data(&MountPointLength, IndexData, 4);
	read_data(MountPoint, IndexData, MountPointLength);
	read_data(&NumOfEntry, IndexData, 4);
	
	for (uint32_t Files = 0; Files < NumOfEntry; Files++) {
		read_data(&FilenameSize, IndexData, 4);
		if (FilenameSize > 0 && FilenameSize < 1024) {
			read_data(Filename, IndexData, FilenameSize);
		} else {
			// UTF-16LE filename convert into UTF-8
			read_data(fname, IndexData, -FilenameSize * 2);
			unicode_to_utf8(fname, -FilenameSize * 2, Filename, sizeof(Filename));
		}
		
		read_data(FileHash, IndexData, 20);
		read_data(&FileOffset, IndexData, 8);
		read_data(&FileSize, IndexData, 8);
		read_data(&CompressionMethod, IndexData, 4);
		read_data(&CompressedLength, IndexData, 8);
		read_data(Dummy, IndexData, 21);

		// Check if CompressionMethod is not equal to 0 (none)
		if (CompressionMethod != 0) {
			// Read the number of blocks and their compressed start and end positions
			read_data(&NumOfBlocks, IndexData, 4);
			for (int x = 0; x < NumOfBlocks; x++) {
				read_data(&Block[x].CompressedStart, IndexData, 8);
				read_data(&Block[x].CompressedEnd, IndexData, 8);
			}
		} else {
			// Set the number of blocks to 0 if CompressionMethod indicates no compression
			NumOfBlocks = 0;
		}
		
		read_data(&CompressedBlockSize, IndexData, 4);
		read_data(&Encrypted, IndexData, 1);
		
		// Open output file
		int OutFile = create_file(Filename);
		
		// if opening output file failed
		if (OutFile == -1) {
			fprintf(stderr, "Failed to open output file: %s\n", Filename);
			continue; // continue for ignore this file extract next
		}
		
		// Data is compressed
		if (NumOfBlocks > 0) {
			for (int x = 0; x < NumOfBlocks; x++) {
				if (pread(PakFile, CompressedData, Block[x].CompressedEnd - Block[x].CompressedStart, Block[x].CompressedStart) != Block[x].CompressedEnd - Block[x].CompressedStart) {
					fprintf(stderr, "Failed to read compressed chunk at %lx\n", Block[x].CompressedStart);
					continue;
				}
				
				if (Encrypted) {
					DecryptData(CompressedData, Block[x].CompressedEnd - Block[x].CompressedStart);
				}
				
				// zlib compression
				if (CompressionMethod == 1) {
					if ((DecompressLength = ZLIB_decompress(CompressedData, Block[x].CompressedEnd - Block[x].CompressedStart, DecompressedData, CHUNK_SIZE)) == 0) {
						fprintf(stderr, "Zlib decompression failed\n");
						continue;
					}
				} else if (CompressionMethod == 6) {
					DecompressLength = ZSTD_decompress(DecompressedData, CHUNK_SIZE, CompressedData, Block[x].CompressedEnd - Block[x].CompressedStart);
					
					if (ZSTD_isError(DecompressLength)) {
						fprintf(stderr, "Decompression failed: %s\n", ZSTD_getErrorName(DecompressLength));
						continue;
					}
				} else {
					printf("Unknown compression method %u\n", CompressionMethod);
					close(OutFile);
				}
				
				write(OutFile, DecompressedData, DecompressLength);
				printf("%016lx %lu\t%s\n", Block[x].CompressedStart, DecompressLength, Filename);
			}
		} else {
			// Uncompressed data
			lseek(PakFile, FileOffset + 74, SEEK_SET);
			
			ssize_t bytesRead, bytesWritten;
			
			while (FileSize > 0) {
				size_t bytesToRead = FileSize < CHUNK_SIZE ? FileSize : CHUNK_SIZE;
				
				bytesRead = read(PakFile, DecompressedData, bytesToRead);
				
				if (bytesRead < 0) {
					printf("Error reading from file\n");
				}
				
				if (Encrypted) {
					DecryptData(DecompressedData, bytesToRead);
				}
				
				bytesWritten = write(OutFile, DecompressedData, bytesRead);
				
				if (bytesWritten != bytesRead) {
					printf("Error writing to output file\n");
				}
				
				FileSize -= bytesRead;
				printf("%016lx %zd\t%s\n", FileOffset + 74, bytesWritten, Filename);
			}
		}
		
		close(OutFile);
	}
	
	free(IndexData);
	close(PakFile);
	clock_t end = clock();
	double cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
	printf("\n%u files found in %f seconds\n", NumOfEntry, cpu_time_used);
	return EXIT_SUCCESS;
}
