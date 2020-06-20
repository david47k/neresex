/*  neresex.c

    Resource extractor for Windows 3.xx 16-bit New Executable (NE) files
    
	Use on Windows 3.xx era .DLL and .EXE files

	Copyright 2020 David Atkinson
	Author: David Atkinson <dav!id47k@d47.co> (remove the '!')
	Credits: NE header struct from: https://wiki.osdev.org/NE
	         Helpful NE format info: here http://bytepointer.com/resources/win16_ne_exe_format_win3.0.htm
	License: GNU General Public License version 2 or any later version (GPL-2.0-or-later)
	
*/

#include <stdint.h>
#include <stdio.h>
#include <string.h>

//----------------------------------------------------------------------------
//  NE FILE STRUCTURE
//----------------------------------------------------------------------------

struct NE_header {
    char sig[2];                 //"NE"
    uint8_t MajLinkerVersion;    //The major linker version
    uint8_t MinLinkerVersion;    //The minor linker version
    uint16_t EntryTableOffset;   //Offset of entry table, see below
    uint16_t EntryTableLength;   //Length of entry table in bytes
    uint32_t FileLoadCRC;        //32-bit CRC of entire contents of file
    uint8_t ProgFlags;           //Program flags, bitmapped
    uint8_t ApplFlags;           //Application flags, bitmapped
    uint16_t AutoDataSegIndex;   //The automatic data segment index
    uint16_t InitHeapSize;       //The intial local heap size
    uint16_t InitStackSize;      //The inital stack size
    uint32_t EntryPoint;         //CS:IP entry point, CS is index into segment table
    uint32_t InitStack;          //SS:SP inital stack pointer, SS is index into segment table
    uint16_t SegCount;           //Number of segments in segment table
    uint16_t ModRefs;            //Number of module references (DLLs)
    uint16_t NoResNamesTabSiz;   //Size of non-resident names table, in bytes (Please clarify non-resident names table)
    uint16_t SegTableOffset;     //Offset of Segment table
    uint16_t ResTableOffset;     //Offset of resources table from start of this struct location
    uint16_t ResidNamTable;      //Offset of resident names table
    uint16_t ModRefTable;        //Offset of module reference table
    uint16_t ImportNameTable;    //Offset of imported names table (array of counted strings, terminated with string of length 00h)
    uint32_t OffStartNonResTab;  //Offset from start of file to non-resident names table
    uint16_t MovEntryCount;      //Count of moveable entry point listed in entry table
    uint16_t FileAlnSzShftCnt;   //File alligbment size shift count (0=9(default 512 byte pages))
    uint16_t nResTabEntries;     //Number of resource table entries
    uint8_t targOS;              //Target OS
    uint8_t OS2EXEFlags;         //Other OS/2 flags
    uint16_t retThunkOffset;     //Offset to return thunks or start of gangload area - what is gangload?
    uint16_t segrefthunksoff;    //Offset to segment reference thunks or size of gangload area
    uint16_t mincodeswap;        //Minimum code swap area size
    uint8_t expctwinver[2];      //Expected windows version (minor first)
};

struct TypeBlock {
    uint16_t typeID;
    uint16_t resCount;
    uint32_t reserved;
};

struct ResBlock {
    uint16_t dataOffset; // relative to begining of file, but in terms of alignment shift count
    uint16_t dataLength; // length of the data in bytes
    uint16_t flags;	     // Moveable/Pure/Preloaded
    uint16_t resourceID; // Integer if high order bit set, otherwise offset (from beginning of resource table) to resource string
    uint32_t reserved;
};

struct ResourceType {
    const char* str;
    const char* ext;
};

struct ResourceType resourceTypes[25] = { 
	{ "unknown(0)", "bin" }, 		
	{ "cursor", "cur" }, 
	{ "bitmap", "bmp" } ,
	{ "icon", "ico" },
	{ "menu", "menu.rc" },
	{ "dialog", "dlg" },
	{ "string", "string.rc" },
	{ "fontdir", "fontdir.fnt" },
	{ "font", "font.fnt" },
	{ "accelerator", "accelerator.rc" },
	{ "rcdata", "rcdata.rc" },
	{ "messagetable", "mc" },
	{ "group_cursor", "group_cursor" },
	{ "group_icon", "group_icon" },
	{ "unknown(14)", "bin" },
	{ "unknown(15)", "bin" },
	{ "version", "version.rc" },
	{ "dlginclude", "dlginclude.rc" },
	{ "unknown(18)", "bin" },
	{ "plugplay", "plugplay" },
	{ "vxd", "vxd" },
	{ "anicursor", "anicursor" },
	{ "aniicon", "aniicon" },
	{ "html", "htm" },
	{ "manifest", "manifest" }
};

const char * getResourceTypeStr(uint16_t t) {
    t = t&0x7FFF;
    if(t>=25) return "unknown";
    else return resourceTypes[t].str;
}

const char * getResourceTypeExt(uint16_t t) {
    t = t&0x7FFF;
    if(t>=25) return "bin";
    else return resourceTypes[t].ext;
}

int getOffsetString(FILE * f, int offset, char * buf) { // buf must be at least 256 chars long
    // Reads in a resource string from the specified file to the specified buffer

    // Save file seek position
    int oldPos;
    oldPos = ftell(f);

    // Seek to position
    if(fseek(f,offset,SEEK_SET) != 0) {
		printf("getOffsetString: fseek failed\n");
		return 1;
    }

    // Read size
    uint8_t length;
    if(fread(&length,1,1,f) != 1) {
		printf("getOffsetString: fread failed (1)\n");
		return 1;
    }

    // Read data    
    if(fread(buf,1,length,f) != length) {
		printf("getOffsetString: fread failed (2)\n");
		return 1;
    }

    // Null-terminate
    buf[length] = 0;

    // Restore file seek position
    if(fseek(f,oldPos,SEEK_SET) != 0) {
		printf("getOffsetString: fseek reset failed\n");
		return 1;
    }

    return 0;
}

//----------------------------------------------------------------------------
//  DUMPRESOURCE: dump blob to disk
//----------------------------------------------------------------------------

#define BLOCKSIZE 4096

int dumpResource(FILE * fin, uint32_t offset, uint32_t byteCount, char * filename) {
    // Dumps a resource to disk

    // Open output file
    FILE * fout = fopen(filename,"wb");
    if(fout==NULL) {
		printf("dumpResource: Unable to open output file '%s'\n",filename);
		return 1;
    }

    // Save input file seek position
    int oldPos = ftell(fin);

    // Seek to resource position
    if(fseek(fin,offset,SEEK_SET) != 0) {
		printf("dumpResource: fseek failed\n");
		return 1;
    }

    // Copy blocks
    char buf[BLOCKSIZE];
    for(int bytesLeft=byteCount; bytesLeft>0; ) {
		int count = BLOCKSIZE;
		if(bytesLeft<BLOCKSIZE) count = bytesLeft;

		if(fread(buf,1,count,fin) != count) {
			printf("dumpResource: fread failed\n");
			return 1;
		}

		if(fwrite(buf,1,count,fout) != count) {
			printf("dumpResource: fwrite failed\n");
			return 1;
		}
		bytesLeft -= count;
	}

	// Close output file
	fclose(fout);

	// Restore input file seek position
	if(fseek(fin,oldPos,SEEK_SET) != 0) {
		printf("dumpResource: fseek reset failed\n");
		return 1;
    }

    return 0;
}

//----------------------------------------------------------------------------
//  MAIN
//----------------------------------------------------------------------------

int main(int argc, char * argv[]) {
    printf("%s\n\n","neresex: Windows NE (16 bit) resource extractor");

	char * basename = "neresex";
	if(argc>0) {
		// find the name of the executable. not perfect but it's only used for display and no messy ifdefs.
		char * b = strrchr(argv[0],'\\');
		if(!b) {
			b = strrchr(argv[0],'/');
		}
		basename = b ? b+1 : argv[0];
	}
    
    if(argc<2) {
		printf("Usage: \n%s inputFile -dump prefix -usenames\n\n", basename);
		printf("%s","inputFile               a NE file. the only required parameter.\n");
		printf("%s","-dump prefix            dumps the files out with the specified prefix.\n"
					"                        e.g. -dump output_folder\\\n");
		printf("%s","-usenames               when dumping, use resource names as filenames.\n");
		return 0;
    }
    char * filename = argv[1];
    char * outPrefix = "";
    int dump = 0;
    int useNames = 0;
	for(int i=2; i<argc; i++) {
		if(strncmp(argv[i],"-usenames",9)==0) {
			useNames = 1;
		} else if(strncmp(argv[i], "-dump",5)==0) {
			if(i != (argc-1)) {
				outPrefix = argv[i+1];
				dump = 1;
				i++;
				continue;
			} else {
				printf("error: Missing parameter for: -dump\n");
				return 1;
			}
		} else {
			printf("warning: Unknown parameter: %s\n",argv[i]);
		}
	}

    if(strlen(outPrefix)>256) {
		printf("error: Output prefix is too long\n");
		return 1;
    }

    FILE * f = fopen(filename, "rb");

    if(f==NULL) {
		printf("error: Failed to open input file: %s\n", filename);
		return 1;
    }

    fseek(f,0,SEEK_SET);

    char buf[4]="";

    if(fread(buf,1,2,f) != 2) {
		printf("error: Read failed (file type check 1).\n");
		return 1;
    }

    if(strncmp(buf,"MZ",2) != 0) {
		printf("error: Not an NE file (no MZ header).\n");
		return 1;
    }

    if(fseek(f,0x3C,SEEK_SET) != 0) {
		printf("error: Seek failed (file type check 2).\n");
		return 1;
    }

    uint32_t extHeaderOffset = 0;
	if(fread(&extHeaderOffset,4,1,f) != 1) {
		printf("error: Read failed (file type check 2).\n");
		return 1;
    }

    printf("Extended header offset: 0x%08X\n",extHeaderOffset);

    if(fseek(f,extHeaderOffset,SEEK_SET) != 0) {
		printf("error: Seek failed (extended header).\n");
		return 1;
    }
    
    struct NE_header neHeader;

    if(fread(&neHeader,1,sizeof(neHeader),f) != sizeof(neHeader)) {
		printf("error: Read failed (extended header).\n");
		return 1;
    }

    if(strncmp(neHeader.sig,"NE",2) != 0) {
		printf("error: Not an NE header.\n");
		return 1;
    }

    int resTableOffset=neHeader.ResTableOffset+extHeaderOffset;
    printf("Resource table offset: 0x%04X\n", resTableOffset);
    printf("Resource table entries: %u\n", (unsigned short)neHeader.nResTabEntries);

    if(fseek(f,resTableOffset,SEEK_SET) != 0) {
		printf("error: Seek failed (resource table).\n");
		return 1;
    }

    int resNameTableOffset = neHeader.ResidNamTable + extHeaderOffset;
    int maxBytes = resNameTableOffset - resTableOffset;
    printf("Resident Name Table offset: 0x%04X\n", resNameTableOffset);
    printf("Leaving us with %u maximum bytes in resource table\n", maxBytes);

    uint16_t offsetShiftCount = 0;
    if(fread(&offsetShiftCount,2,1,f) != 1) {
		printf("error: Read failed (offset shift count)\n");
		return 1;
    }
    
    uint16_t sizeShiftCount = neHeader.FileAlnSzShftCnt;
    if(sizeShiftCount==0) sizeShiftCount = 9; // according to docs, default is 9 (i.e. 512 bytes)
    printf("Size alignment shift count: 0x%04X\n", sizeShiftCount);
    printf("Offset alignment shift count for Resource Data: 0x%04X\n", offsetShiftCount);

    int typeBlockCounter = 0;
    int byteCounter = 2;
    struct TypeBlock typeBlock;
    char typeBuf[257];
    char nameBuf[257];

    for(typeBlockCounter = 0; byteCounter < maxBytes; typeBlockCounter++) {
        if(fread(&typeBlock,1,sizeof(typeBlock),f) != sizeof(typeBlock)) {
			printf("error: Read failed (type block).\n");
			return 1;
		}

		if(typeBlock.typeID == 0) {
			printf("\nEnd of type table, %u types\n", typeBlockCounter);
			break;
		}

		if((typeBlock.typeID&0x8000)==0) { 
			if(getOffsetString(f,typeBlock.typeID + resTableOffset,typeBuf) != 0) {
				printf("error: getOffsetString failed (type)\n");
				return 1;
			}
			char typeBuf2[259];
			sprintf(typeBuf2, "'%s'", typeBuf);
			printf("\nType: %-23s  ", typeBuf2);	    
		} else {
			printf("\nType: 0x%04X %-16s  ", (typeBlock.typeID&0x7FFF), getResourceTypeStr(typeBlock.typeID));
			sprintf(typeBuf,"%s",getResourceTypeExt(typeBlock.typeID));
		}

		byteCounter += sizeof(typeBlock);

		printf("Resource count: %u\n", typeBlock.resCount);

		struct ResBlock resBlock;

		for(int i=0; i<typeBlock.resCount; i++) {
			if(fread(&resBlock,1,sizeof(resBlock),f) != sizeof(resBlock)) {
				printf("error: Read failed (resource block).\n");
				return 1;
			}
			byteCounter += sizeof(resBlock);

			printf("    resource %05u-%05u  ",typeBlockCounter,i);
			printf("flags=0x%04X  ", resBlock.flags);
			uint32_t actualLength = (uint32_t)resBlock.dataLength<<(uint32_t)sizeShiftCount;
			uint32_t byteOffset = (uint32_t)resBlock.dataOffset<<(uint32_t)offsetShiftCount;
			printf("length=0x%08X (%u)  ", actualLength, actualLength);
			printf("offset=0x%08X (%u)\n", byteOffset, byteOffset);
			if((resBlock.resourceID&0x8000)==0) { 
				if(getOffsetString(f,resBlock.resourceID + resTableOffset,nameBuf) != 0) {
					printf("error: getOffsetString failed (name)\n");
					return 1;
				}
				printf("        id='%s'\n", nameBuf);
			} else { 
				printf("        id=%05u\n",resBlock.resourceID&0x7FFF); 
				sprintf(nameBuf,"%05u",resBlock.resourceID&0x7FFF);
			}

			if(dump) {
				char filename[1024];
				if(!useNames) {
					sprintf(filename,"%s%05u-%05u.bin",outPrefix,typeBlockCounter,i);
				} else {
					sprintf(filename,"%s%s.%s",outPrefix,nameBuf,typeBuf);
				}
				if(dumpResource(f,byteOffset,actualLength,filename) != 0) {
					printf("error: dumpResource failed\n");
					return 1;
				}
				printf("        dumped to %s\n",filename);
			}
		}
    }

    if(byteCounter == maxBytes) {
		printf("error: Unexpected overflow of resource area\n");
		return 1;
    }

    printf("Done.\n");
	
    fclose(f);

    return 0;
}