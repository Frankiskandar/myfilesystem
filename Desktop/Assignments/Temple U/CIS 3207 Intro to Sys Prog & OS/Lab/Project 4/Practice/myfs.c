#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// metadata region
typedef struct {
	char* fsname; // filesys's name it uses 8 bytes, offset 0-7
	unsigned short byte_per_sector; // 512 , uses 2 bytes, offset 8-9
	unsigned char sector_per_cluster; // 4 , uses 1 bytes offset 10
	unsigned short reserved_sector; // 1 , uses 2 bytes, offset 11-12
	unsigned char number_of_fat; // 1 , uses 1 byte, offset 13
	unsigned short sector_per_fat; // 8 uses 2 bytes, offset 14-15
	unsigned short RD_entries; // 512 , uses 2 bytes, offset 16-17
	unsigned short total_sector; // 3906 uses 2 bytes, offset 18-19
//	char unused[492]; //512-20
	
} Metadata;

void metadata_init(FILE * infile)
{
	Metadata mbr;
	mbr.fsname = "my fs"; //offset 0-7
	mbr.byte_per_sector = 512; // offset 8-9
	mbr.sector_per_cluster = 4; // offset 10
	mbr.reserved_sector = 1; // offset 11-12
	mbr.number_of_fat = 1; // offset 13
	mbr.sector_per_fat = 8; // offset 14-15
	mbr.RD_entries = 512; // offset 16-17
	mbr.total_sector = 3906; // offset 18-19
	unsigned char unused = 0;
	
	fseek(infile, 0, SEEK_SET);
	fwrite(mbr.fsname , sizeof(char) , 8, infile );
	fseek(infile, 8, SEEK_SET);
	fwrite(&mbr.byte_per_sector , sizeof(unsigned short) , 1, infile );
	fseek(infile, 10, SEEK_SET);
	fwrite(&mbr.sector_per_cluster , sizeof(unsigned char) , 1, infile );
	fseek(infile, 11, SEEK_SET);
	fwrite(&mbr.reserved_sector , sizeof(unsigned short) , 1, infile );
	fseek(infile, 13, SEEK_SET);
	fwrite(&mbr.number_of_fat , sizeof(unsigned char) , 1, infile );
	fseek(infile, 14, SEEK_SET);
	fwrite(&mbr.sector_per_fat , sizeof(unsigned short) , 1, infile );
	fseek(infile, 16, SEEK_SET);
	fwrite(&mbr.RD_entries , sizeof(unsigned short) , 1, infile );
	fseek(infile, 18, SEEK_SET);
	fwrite(&mbr.total_sector , sizeof(unsigned short) , 1, infile );
	
	// print 00 to the rest of metadata sector
	int n = 20;
	while (n!=512)
	{
	fseek(infile, n, SEEK_SET);
	fwrite(&unused , sizeof(unsigned char) , 1, infile );
	n++;
	}
	
	//test
	char* test = "test1234";
	fseek(infile, 512, SEEK_SET);
	fwrite(test , sizeof(char) , 8, infile );
	
	printf("MBR information:\n");
	printf("Name: %s\n",mbr.fsname);
	printf("Byte per sector: %u\n",mbr.byte_per_sector);
	printf("Sector per cluster: %u\n",mbr.sector_per_fat);
	printf("Reserved sector: %u\n",mbr.reserved_sector);
	printf("Number of FAT table: %u\n",mbr.number_of_fat);
	printf("Sector per FAT table: %u\n",mbr.sector_per_fat);
	printf("RD entries: %u\n", mbr.RD_entries);
	printf("Total sectors: %u\n",mbr.total_sector);
}

int main() 
{

    FILE * infile = fopen("Drive2MB", "r+");
    metadata_init(infile);
//	char* name = "Frank's File System";
//	fseek(infile, 0, SEEK_SET);
//	fwrite(name , sizeof(char) , 22, infile );

	fclose(infile);

    
  


return 0;
}
