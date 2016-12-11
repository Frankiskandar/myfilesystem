#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// my program uses 5MB drive
#define TOTAL_SECTORS 9765 // total sectors
#define NUM_DATAREGION_SECTORS 9704 // Sectors available for data region (total sectors - (MBR+FAT))
#define NUM_FAT_SECTORS 60 // total fat size, 30 sectors each
#define NUM_RESERVED_SECTORS 61 // Metadata region + FAT sectors
#define BLOCK_SIZE 512 // Size of each block = 512 bytes
#define RD_START_SECTOR 62 // start sector of RD (#61 in hexeditor)
#define FAT 2 // fat table start sector. (#1 hexeditor)

//Metadata Region struct
typedef struct {
	char* fsname; // filesys's name it uses 8 bytes, offset 0-7
	unsigned short byte_per_sector; // 512 , uses 2 bytes, offset 8-9
	unsigned char sector_per_cluster; // 1 , uses 1 bytes offset 10
	unsigned short reserved_sector; // 1 for metadata and 60 for FAT , uses 2 bytes, offset 11-12
	unsigned char number_of_fat; // 2 , uses 1 byte, offset 13
	unsigned short sector_per_fat; // 30 uses 2 bytes, offset 14-15
	unsigned short RD_entries; // 512 , uses 2 bytes, offset 16-17
	unsigned short total_sector; // 9765 uses 2 bytes, offset 18-19
//	char unused[492]; //512-20
} Metadata;

//struct for directory entry
//each directory entry is 32 bytes in size
typedef struct directoryEntry{
	char name[11]; // 8 for name, 3 for extension offset 0-10
	//file attribute, it takes 2 bytes offset
	short readOnly : 1; 
	short hidden : 1;
	short systemFile : 1;
	short volumeLabel : 1;											
	short subdir : 1;
	short archive : 1;
	short unusedbit1 : 1;
	short unusedbit2 : 1;
	char emptyOffset[8]; // offset 14-21 are unused. they are not required for simplicity
	unsigned short time; //modified time // offset 22-23
	unsigned short date; // modified date //offset 24-25
	unsigned short startCluster; // offset 26-27
	long fileSize; // offset 28-31
}directoryEntry;  

//struct for fat entry 
typedef struct FATentry{
	short next; // each fat entry is 2 bytes in size, it will hold nex cluster in file
}FATentry;

/*
//function declarations
void clearInput();
void my_format();
int firstByte(int sector_num);
short *getDateAndTime();
int findEmptySector();
void disk_Init();
void createDirTable();
void createFATentry(int sector_num, short next);
int getNextCluster(int sector_num);
directoryEntry *my_CreateDirEntry(char *namep, char attributes, short time, short date, short startCluster, long fileSize);
directoryEntry *my_CreateDirectory(char *pathname);
directoryEntry *my_CreateFile(char *pathname);
directoryEntry *my_OpenFile(char *pathname);
int my_DeleteFile(char *pathname);
*/

//these global var will keep track some important information
int curDirectory;
int parentDirectory;
int curCluster;
int curSpace;
int curOffset;
//global variable for file pointer
FILE *disk;
//global variable for time
time_t rawtime;
struct tm *timeinfo;

//this function clears the input of scanf
void clearInput(){
	char input;
	while(input!='\n')
	{
		input = fgetc(stdin);
	}
}// end clearInput()

//this function finds first available cluster/sector
//it looks for a free entry in FAT table first
//then returns the sector number in the data region
int findEmptySector(){
	int i = 1;	
	//go to FAT and read first entry into f
	fseek(disk, firstByte(FAT)+2, SEEK_SET); //3rd offset in fat is the first entry
	FATentry *fat = malloc(sizeof(FATentry)); //size of fat entry is 2 bytes
	fread(fat, 2, 1, disk);
	//read first FAT entry
	for(;i<=9688;i++)
	{
		if(fat->next == 0)
		{ //if we find the first available cluster
			//seek to first available cluster and return cluster
			fseek(disk, firstByte(i+NUM_RESERVED_SECTORS), SEEK_SET);
			//i.e we find empty clus in sec 15 FAT, go to sector 61+15 in data region
			return i;
		}//end if
		else
			fread(fat, 2, 1, disk);
	}//end for
	printf("No more space available in the disk");
	return;
}
//this function fills the entire disk with 00 00 00 ...
void my_format(){
	int i;
	char *zeros = malloc(512);
	char *p = zeros;
	for(i=0; i<512; i++, p++)
	{
		*p = 0x00; //512 x 00
	}//end for	
	fseek(disk, 0, SEEK_SET);
	for(i=0; i<TOTAL_SECTORS; i++) // total sectors = 9765 and fill each sector with 512 zeros b/c each sector has 512 byte
	{
		fwrite(zeros, 512,1,disk);
	}//end for
	fseek(disk,0, SEEK_SET);
} //end my_format()

//this function finds the 1st byte of a sector
//go to a first byte of a particular sector
//will be used for fseek and fread a lot
int firstByte(int sector_num){
	sector_num = (sector_num-1)*512; //takes you to the very first byte of a sector
	return (sector_num);
} //firstByte()

//function to get timedate
short *getDateAndTime(){
	short *timedate = malloc(2*sizeof(short));
	short *p = timedate;
	//get time
	time(&rawtime);
    timeinfo = localtime(&rawtime);
	//format time
    short time = (short)(timeinfo->tm_hour<<11);
    time = time+(timeinfo->tm_min<<5);
    time = time +(timeinfo->tm_sec);
	*p = time; p++;
	//format date
	short date = (short)((timeinfo->tm_year-80)<<9);
	date = date + (timeinfo->tm_mon<<5);
	date = date + (timeinfo->tm_mday);
	*p = date; p++;
	return timedate;
} //end *getDateAndTime()

// this function initializes meta data region and FAT table
// will be called everytime the user formats the drive
void disk_Init(){
	//reset all global variables
	fseek(disk, 0, SEEK_SET);
	curDirectory = RD_START_SECTOR-NUM_RESERVED_SECTORS;
    curCluster = curDirectory;
    curSpace = NUM_DATAREGION_SECTORS; // NUM_DATAREGION_SECTORS == 9704

	Metadata mbr;
	mbr.fsname = "my fs"; //offset 0-7
	mbr.byte_per_sector = 512; // offset 8-9
	mbr.sector_per_cluster = 1; // offset 10
	mbr.reserved_sector = 1; // offset 11-12
	mbr.number_of_fat = 2; // offset 13
	mbr.sector_per_fat = 30; // offset 14-15
	mbr.RD_entries = 0; // offset 16-17 0 in fat 32 that's what I read
	mbr.total_sector = 9765; // offset 18-19
	unsigned char unused = 0;
	
	fseek(disk, 0, SEEK_SET);
	fwrite(mbr.fsname , sizeof(char) , 8, disk );
	fseek(disk, 8, SEEK_SET);
	fwrite(&mbr.byte_per_sector , sizeof(unsigned short) , 1, disk );
	fseek(disk, 10, SEEK_SET);
	fwrite(&mbr.sector_per_cluster , sizeof(unsigned char) , 1, disk );
	fseek(disk, 11, SEEK_SET);
	fwrite(&mbr.reserved_sector , sizeof(unsigned short) , 1, disk );
	fseek(disk, 13, SEEK_SET);
	fwrite(&mbr.number_of_fat , sizeof(unsigned char) , 1, disk );
	fseek(disk, 14, SEEK_SET);
	fwrite(&mbr.sector_per_fat , sizeof(unsigned short) , 1, disk );
	fseek(disk, 16, SEEK_SET);
	fwrite(&mbr.RD_entries , sizeof(unsigned short) , 1, disk );
	fseek(disk, 18, SEEK_SET);
	fwrite(&mbr.total_sector , sizeof(unsigned short) , 1, disk );
	
	// print 00 to the rest of metadata sector
	int n = 20;
	while (n!=512)
	{
	fseek(disk, n, SEEK_SET);
	fwrite(&unused , sizeof(unsigned char) , 1, disk );
	n++;
	}
	//print 55 AA at the end of first sector 
	unsigned char end1 = 85;
	unsigned char end2 = 170;
	fseek(disk, 510, SEEK_SET);
	fwrite(&end1,sizeof(unsigned char),1,disk);
	fseek(disk, 511, SEEK_SET);
	fwrite(&end2,sizeof(unsigned char),1,disk);
	//create FAT entry
	void *doubleFAT = malloc(512*NUM_FAT_SECTORS); //512 * 60
    fseek(disk, (BLOCK_SIZE*NUM_FAT_SECTORS) , SEEK_CUR); //reserve 512*60 byte for FAT table
    free(doubleFAT);
	//create root directory table and FAT table
	createDirTable();
	createFATentry(1, 0xFFFF);  
}

//leaves space for directory table 
//also creates an FFFF entry in FAT
void createDirTable(){
	//update curDirectoryectory cluster pointer 
	curDirectory = curCluster;

	//root has one sector at first but is dynamically allocated more as needed
	//if we are in root
	if(curDirectory == RD_START_SECTOR) //  62
		createFATentry(curDirectory, 0xFFFF); // a folder only has 1 sector/cluster
	fseek(disk, BLOCK_SIZE, SEEK_CUR);
	curCluster+=1; //increment the cluster
	curSpace -=1; // decrement the currentspace
}
//creates fat entry and returns pointer to previous position
//first parameter is the entry position, the 2nd param is 2 bytes to write
void createFATentry(int sector_num, short next)
{
	FATentry *new = malloc(sizeof(FATentry)); //2bytes in size
	new->next = next;
	//write the 2nd parameter of this funcion
	fseek(disk, firstByte(FAT)+(2*sector_num), SEEK_SET); // prints next at the offset given by parameter in fat table
	fwrite(new, 2, 1, disk);
	//got to 2nd FAT and write the same entry 
	fseek(disk, 30*BLOCK_SIZE-2, SEEK_CUR); // since each fat has 30 sectors
	fwrite(new, 2, 1, disk);
	//seek back to previous location
	fseek(disk, firstByte(sector_num)+curOffset, SEEK_SET);
	free(new);
}//end createFATentry()

//this function finds next cluster
//will be used for file deletion
int getNextCluster(int sector_num)
{
	FATentry *entry = malloc(sizeof(FATentry)); // malloc the entry for 2 bytes
	//read and return entry->next
	fseek(disk, firstByte(FAT)+(2*sector_num), SEEK_SET);
	fread(entry, 2, 1, disk);
	if(entry->next!=0xFFFF)
		fseek(disk, firstByte(entry->next+NUM_RESERVED_SECTORS), SEEK_SET); //seek to the given data region
	else
		fseek(disk, firstByte(sector_num+NUM_RESERVED_SECTORS), SEEK_SET);
	// return the value of the next entry
	return entry->next;
}//end get cluster

//this function creates directory entry
//each dir entry is 32 bytes
//returns directory entry struct
//TODO:
//KNOWN BUG: filename cannot be larger than 4 letter
//Its OK for folder
directoryEntry *my_CreateDirEntry(char *namep, char attributes, short time, short date, short startCluster, long filesize){
	int i;
	directoryEntry *entry = malloc(sizeof(directoryEntry));
	directoryEntry *entrychecker = malloc(sizeof(directoryEntry)); //will use this to check an entry
	//get name and update entry
	for(i = 0; i <11; i++, namep++){ //11 bytes for name
		if(*namep!=NULL)
		{
			//we found the start of the extension
			if(*namep == '.')
			{
				namep++;//go to next char
				for(;i<8;i++)
					entry->name[i] = ' '; //add some whitespace before dot to fil the whole 8 char
			}//end if extension
			//if name of the file is bigger than 8 (excluding extension)
			else if(i == 8 && *namep!='.'){
                        	printf("Error! name is too long\n");
                        	return NULL;
               		}
            //insert the name to the struct
			entry->name[i] = *namep;
		}//end if		
		else
		{
			//name+ext must be 11 char total, I add space to fill it out
			for(; i < 11; i++)
				entry->name[i]= ' ';
		}//end else
		
	} //end for	
	//set attributes according to the parameter of this function
	if(attributes>=128)
	{
		attributes -=128;
		entry->readOnly = 1;
	}
	else
		entry->readOnly = 0;
	if(attributes>=64)
	{
		attributes -= 64;
		entry->hidden = 1;
	}
	else
		entry->hidden = 0;
	if(attributes>=32)
	{
		attributes -= 32;
		entry->systemFile = 1;
	}
	else
		entry->systemFile = 0;
	if(attributes>=16)
	{
		attributes -= 16;
		entry->volumeLabel = 1;
	}
	else
		entry->volumeLabel = 0;
	if(attributes>=8)
	{
		attributes -= 8;
		entry->subdir = 1;
	}
	else
		entry->subdir = 0;
	if(attributes>=4)
	{
		attributes -= 4;
		entry->archive = 1;
	}
	else
		entry->archive = 0;
		entry->unusedbit1 = 0;
		entry->unusedbit2 = 0;
	//set the unused offset to 00
	for(i = 0; i < 10; i++) 
		entry->emptyOffset[i] = 0x00;
	// set the modified time and date
	entry->time = time;
	entry->date = date;
	// set the start cluster
	entry->startCluster = startCluster;
	// set the filesize
	entry->fileSize = filesize;
	// find an empty 32 byte entry 
	// it depends heavily where we are right now according to curDirectory
	fseek(disk, firstByte(curDirectory+NUM_RESERVED_SECTORS), SEEK_SET);
	fread(entrychecker, 32, 1, disk); // we use a struct just to check, we dont want insert the data into our already prepared entry
	while(entrychecker->startCluster!=0) //until we find start cluster = 0
		fread(entrychecker, 32, 1, disk);	
	//go back to the empty entry and write to disk
	fseek(disk, -32, SEEK_CUR);
	fwrite(entry, 32, 1, disk);	//we are in the right position, now write the entry
	//seek back to previous position
	fseek(disk, firstByte((entry->startCluster+NUM_RESERVED_SECTORS)), SEEK_SET);
	return entry;
} // end my_CreateDirEntry()


//this function creates directory according to the pathname and returns directory entry struct
directoryEntry *my_CreateDirectory(char *pathname){
	int i, j, c;
	//create array of entries to read
	directoryEntry *entry = malloc(16*sizeof(directoryEntry));//a whole sector
	//newdirectory value
	directoryEntry *newdir;	
	//array of each directory and the file 
	char *names[16];
	char entryName[12];	
	//use strtok to seperate the pathname every "/"
    names[0] = strtok(pathname, "/");
	for(i = 1; names[i-1]!= NULL && i < 16; i++)
	{
		names[i] = strtok(NULL, "/");
	} //end if
	i--;
	//go to first byte of RD entry
	fseek(disk, firstByte(RD_START_SECTOR), SEEK_SET); //62
	for(j = 0 ; j < i ; entry++)
	{
		//read entries until pathname is totally parsed
		fread(entry, 32, 1, disk);
		curOffset+=32;
		//change entry->name to comparable, NULL terminated string
		if(entry->startCluster!=NULL||entry->startCluster!=0){
			c = 0; 
			while(entry->name[c]!= ' '&& entry->name[c]!=NULL)
			{
				entryName[c] = (char)entry->name[c];
				c++;
			}//end while
		}// end if
		entryName[c] = '\0'; //null at the end
		char *e  = &entryName;
		//if the pathname is not the last directory to create
		if(i>j+1)
		{
			//if the path's name matches the entry name
			if(strcmp(e, names[j])==0)
			{
				//seek to that directory
				fseek(disk,firstByte(entry->startCluster+NUM_RESERVED_SECTORS),SEEK_SET);
				curCluster = entry->startCluster; 
				curOffset = 0;
				parentDirectory = curDirectory; 
				curDirectory = curCluster; 
				j++;
			}//end if
			else if(entry->startCluster == 0)
			{
				printf("pathname not found\n");
				return NULL;
			}//end else if
		}// end if
		//if entryname is the same as whats in names[j]
		else if(strcmp(e, names[j])==0){
			printf("Name of the file/directory already exist!\n");
			return NULL;
		}
		//if the current offset is already 512, that directory is full
		else if(curOffset == 512){
			printf("No more space available in the current directory\n");
			return NULL;
		}
		else if(entry->startCluster == 0||entry->startCluster == NULL)
			i--;
	}//end for
	//get the timestamp for directory entry
	short *timeDate = getDateAndTime();
	short date = *(timeDate+1);
	//set the file attribute
	char attributes = 0x08; //file attr 08 means its a subdir
	//create directory entry and FAT entry
	curCluster = findEmptySector();
	//for a directory I allocate a whole sector(512) as filesize
	// just for simplicity and temporary solution
	newdir = my_CreateDirEntry(names[j], attributes, *timeDate, date, curCluster, BLOCK_SIZE);
	// create FF FF entry in fat table because its only 1 sector big
	createFATentry(curCluster, 0xFFFF);
	createDirTable();
	return newdir;
}


//this function creates a file in a directory
directoryEntry *my_CreateFile(char *pathname){
	directoryEntry *entry = malloc(sizeof(directoryEntry));
	directoryEntry *newDirEntry;
	char *names[16];
	char entryName[12];
	char *p = pathname;
	char *e;
	int i, j, c;
	while(*p!= 0)
	{
		p++;
	}//end while
	*p = '\0';
	p = pathname;
	// if there is a char after extension
	if(strchr(p, '.')!=NULL)
	{
		i = 0;
		while(*p!='.')
		{
			p++;
			i++;
			// the file inside a folder, reset the i
			if(*p == '/')
				i = 0;
		}
		p = p+(9-i);
		while(i<12){
			*p = *(p+1);
			i++;
			p++;
		}
	}//end if
	names[0] = strtok(pathname, "/");
    //separate pathname using the slash
    for(i = 1; names[i-1]!= NULL && i < 16; i++)
	{
        names[i] = strtok(NULL, "/");
    } //end for
    i--;
        fseek(disk, firstByte(RD_START_SECTOR), SEEK_SET);
        for(j = 0 ; j < i ; )
		{
            //read entries until pathname is totally parsed
            fread(entry, 32, 1, disk);
            curOffset+=32;
            //change entry->name to comparable, NULL terminated string
            if(entry->startCluster!= 0 ||entry->startCluster!='\0')
			{
            	c = 0;
                while(entry->name[c]!= ' '&& c < 12)
				{
                    entryName[c] = (char)entry->name[c];
                    c++;
                } //end while
            }//end if
                entryName[c] = '\0';
                e  = (char*)&entryName;
                //find directory to write new file into
                if(i>j+1)
				{
                        if(strcmp(e, names[j])==0)
						{
                            fseek(disk,firstByte(entry->startCluster+NUM_RESERVED_SECTORS),SEEK_SET);
                            curCluster = entry->startCluster;
                            curOffset = 0;
                            parentDirectory = curDirectory;
                            curDirectory = curCluster;
                            j++;
                        }//end if
                        else if(entry->startCluster == 0)
						{
                            printf("pathname not found\n");
                            return NULL;
                        }//end else if
                }//end if i>j
		//if the pathname name is now the file to create
                else if(strcmp(e, names[j])==0){
                        printf("Name of the file/directory already exist!\n");
                        return NULL;
                }
                else if(curOffset == 512){
                        printf("No more space in the current directory\n");
                        return NULL;
                }
                else if(entry->startCluster == 0||entry->startCluster == NULL)
                        i--;
        }//end for
	//get time 
	short *timeDate = getDateAndTime();
    short date = *(timeDate+1);
	//0 for all attribute
	char attributes = 0x00;
	
	curCluster = findEmptySector();
    newDirEntry = my_CreateDirEntry(names[j], attributes, *timeDate, date, curCluster, BLOCK_SIZE*2);
    createFATentry(curCluster, curCluster+1);
	createFATentry(curCluster+1, 0xFFFF);
	return newDirEntry;
} //end my_CreateFile()

// this function open a file
// for now, the function only checks if the file exists or not
// will be more useful if read and write function are implemented
directoryEntry *my_OpenFile(char *pathname)
{	
	//allocate 32bytes for each dir entry holder
	directoryEntry *entry = malloc(sizeof(directoryEntry));
	directoryEntry *file = malloc(sizeof(directoryEntry));
	//char arrays and pointers for pathname parsing
	char *name[16];
	char *names[16];
	char entryName[12];
	char *e, *ex;
	int i, j, c;
	name[0] = strtok(pathname, "/");
	names[0] = malloc(12*sizeof(char));
	strcpy(names[0], name[0]);
    //use strtok to separate /
    for(i = 1; name[i-1]!= NULL && i < 16; i++)
	{
    	name[i] = strtok(NULL, "/");
		if(name[i]!=0x0)
		{
				names[i] = malloc(12*sizeof(char));
				strcpy(names[i], name[i]);
		}//end if
    }//end for
    i--;
	//look for extension and add spaces so it looks like an entry name
	for(e = names[i-1], j=0; *e!='.'; e++,j++);
	ex = names[i-1];
	ex = ex+8;
	for(c = 0; c < 3; c++, ex++)
		*ex = *(e+1+c);
	*ex = '\0';
	for(; j < 8; j++, e++)
	{
		*e = ' ';
	}//end for
	//set curDirectory and seek to root
	curDirectory = RD_START_SECTOR;
	fseek(disk, firstByte(RD_START_SECTOR), SEEK_SET);
    for(j = 0 ; j < i ; )
	{
        //read entries until pathname is totally parsed
        fread(entry, 32, 1, disk);
        curOffset+=32;
        //change entry->name to comparable, NULL terminated string
        if(entry->startCluster!= 0)
		{
            c = 0;
            while((i>j+1 && entry->name[c]!= ' ')  || (i<=j+1 && c < 12))
				{
                entryName[c] = (char)entry->name[c];
                c++;
           		}//end while
			entryName[c] = '\0';
        }//end if
		e = (char*)&entryName;
		//if pathname is still directory
        if(i>j+1)
		{
			//if entry name and pathname are the same, go to that directory
            if(strcmp(e, names[j])==0)
			{
                	fseek(disk,firstByte(entry->startCluster+NUM_RESERVED_SECTORS),SEEK_SET);
	            	curDirectory = entry->startCluster;
					curCluster = entry->startCluster;
                	curOffset = 0;
                	j++;
            }//end if
                else if(entry->startCluster == 0)
			{
                        printf("Pathname not found\n");
                        return NULL;
            }//end else if
        }//end if
		//go to file in pathname, find entry and set file 
		else
		{	
			if(strcmp(e, names[j])==0)
			{
				file = entry;
				i--;
			}//end if
			else if(entry->startCluster == 0 || curOffset == 512)
			{
				printf("File not found\n");
				return NULL;
			}//end else if
		}//end else
	}//end for
	return file;
} //my_OpenFile()

//this function deletes a file, it takes filepath as parameter
// returns 0 if the deletion is successful
int my_DeleteFile(char *pathname)
{
	//use openfile function to parse the pathname given by parameter
	directoryEntry *file = my_OpenFile(pathname);
	//if the start cluster is 0, it means the path does not exist
	if(file->startCluster == 0)
		return -1;
	//allocate entry
	directoryEntry *entry = malloc(sizeof(directoryEntry));
	//allocate 512 x 00
	char *zeros = malloc(512*sizeof(char));
	//set the global var curCluster to be the same as start cluster
	curCluster = file->startCluster;
	//seek to current directory
	// we get the current directory value from openfile function
	fseek(disk, firstByte(curDirectory+NUM_RESERVED_SECTORS), SEEK_SET);
	//read entry 
	fread(entry, 32, 1, disk);
	curOffset = 32;
	//find the file by comparing the star cluster
	while(entry->startCluster!=file->startCluster)
	{ 
		fread(entry, 32, 1, disk);
	}// end while
	/// seek to the start of the entry
	fseek(disk, -32, SEEK_CUR);
	//delete the entry by writing 32 byte worth of 00 00
	fwrite(zeros, 32,1, disk); // write 00 32 times from the start
	//write all 00 in the data region according to start cluster
	//reserved is = metadata+fat big
	fseek(disk, firstByte(entry->startCluster+NUM_RESERVED_SECTORS), SEEK_SET); 
	fwrite(zeros, 512,1, disk);
	//write 0000 to all clusters in file
	while(curCluster!=-1)
	{
		int i = getNextCluster(curCluster);
		//delete the corresponding fat entry by writing 0000
        createFATentry(curCluster, 0x0000);
		curCluster = i;
		fwrite(zeros, 512, 1, disk);//might be unecessary
	}//end while
	free(zeros);
	return 0;
} //my_DeleteFile()

int main(){
	curDirectory = RD_START_SECTOR-NUM_RESERVED_SECTORS; //1 at first
	curCluster = curDirectory; //1
	curSpace = NUM_DATAREGION_SECTORS; //9704 total available sector at start
	directoryEntry *file; //this holds openfile() return value
	char *p = malloc(5*BLOCK_SIZE); //5*512	string that we re going to write to a file
	disk = fopen("Drive5MB", "rw+"); //our disk
	char cmd; // will be used to get user input	
	short *timedate = getDateAndTime();
    int true = 1; // for while loop
	printf("Welcome to Fnu Frangky's Filesystem\n");
	//get user's input
	while(true)
	{
		char *input = malloc(512);
		printf("List of command:\n f to format the disk\n d to create a directory (must be created one by one)\n o to open a file \n x to delete a file\n q to quit the program \n");
		printf("Your input: ");
		cmd = getchar();
		//format the disk
		if(cmd=='f')
		{
			//set every byte as 00 to our disk
			my_format();
			//initialize metadata region,FAT,RD
			disk_Init();
			printf("Format success!\n");
		}//end if
		// directory creation
		else if(cmd == 'd')
		{
			clearInput();
			printf("Enter directory name without spaces\n");
			scanf("%s", input);
			my_CreateDirectory(input);
		}//end else if command d
		else if(cmd == 'c')
		{
			//file creation
			//known bug: filename cant be longer than 4 char
			//but works for directory
			// dont have time to debug for now
			clearInput();
			printf("Enter file name with its extension i.e: '.txt'\n");
			scanf("%s", input);
			my_CreateFile(input);
		}
		//quit
		else if(cmd=='q')
		{
			//quit the program
			true = 0;
		}
		//file opening
		else if(cmd == 'o'){			
			//seems useless since we dont have write and read functions
			// for now it is used to check if the file exists or not
			clearInput();
			printf("Enter pathname of the file\n");
			scanf("%s", input);
			file = my_OpenFile(input);
		}
		else if(cmd == 'x')
		{
			//file deletion
            clearInput();
            printf("Enter file pathname to delete\n");
            scanf("%s", input);
			my_DeleteFile(input);
		}
		else //if user enters other than given command
		{
			printf("Invalid command\n");
		}
		clearInput();
	}
}

