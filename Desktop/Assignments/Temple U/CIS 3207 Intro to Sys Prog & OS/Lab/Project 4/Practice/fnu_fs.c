#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define TOTAL_SECTORS 9765 // total sectors
#define NUM_DATAREGION_SECTORS 9704 // Sectors available for data region (total sectors - (MBR+FAT))
#define NUM_FAT_SECTORS 60 // total fat size, 30 sectors each
#define NUM_RESERVED_SECTORS 61 // Metadata region + FAT sectors
#define BLOCK_SIZE 512 // Size of each block = 512 bytes
#define RD_START_SECTOR 62 // start sector of RD (#61 in hexeditor)
#define FAT 2 // 2 fat tables

//Metadata Region struct
typedef struct {
	char* fsname; // filesys's name it uses 8 bytes, offset 0-7
	unsigned short byte_per_sector; // 512 , uses 2 bytes, offset 8-9
	unsigned char sector_per_cluster; // 4 , uses 1 bytes offset 10
	unsigned short reserved_sector; // 1 , uses 2 bytes, offset 11-12
	unsigned char number_of_fat; // 2 , uses 1 byte, offset 13
	unsigned short sector_per_fat; // 8 uses 2 bytes, offset 14-15
	unsigned short RD_entries; // 512 , uses 2 bytes, offset 16-17
	unsigned short total_sector; // 3906 uses 2 bytes, offset 18-19
//	char unused[492]; //512-20
	
} Metadata;

//struct for fat entry 
typedef struct FATentry{
	short next; // each fat entry is 2 bytes in size, it will hold nex cluster in file
}FATentry;

//struct for directory entry
//each directory entry is 32 bytes in size
typedef struct directoryEntry{
	char name[11]; // 8 for name, 3 for extension offset 0-10
	//offset 11 is for file attribute
	unsigned int rdonly : 1; 
	unsigned int hidden : 1;
	unsigned int sysfil : 1;
	unsigned int volLabel : 1;
	unsigned int subdir : 1;
	unsigned int archive : 1;
	unsigned int bit : 1;
	unsigned int bit1 : 1;
	char pad[10]; // offset 12-21 are unused. they are not required
	short time; //modified time //22-23
	short date; // modified date //24-25
	short stCluster; //26-27
	long fileSize; //28-31
}directoryEntry;  

//function declarations
void clearInput();
void my_format();
int firstByte(int sector_num);
short *getTimeDate();
int findEmptySector();
void disk_Init();
void createDirTable();
void createFATentry(int sector_num, short next);
//
int getNextCluster(int sector_num);
directoryEntry *createDirEntry(char *namep, char attributes, short time, short date, short stCluster, long fileSize);
directoryEntry *createDirectory(char *path);
directoryEntry *createFile(char *path);
directoryEntry *openFile(char *path);
int closeFile(directoryEntry *file);
int writeFile(directoryEntry *file, char *write);
char *readFile(directoryEntry *file);
int deleteFile(char *path);

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


int main(){
	curDirectory = RD_START_SECTOR-NUM_RESERVED_SECTORS; //1
	curCluster = curDirectory; //1
	curSpace = NUM_DATAREGION_SECTORS; //9704 total available sector at start
	directoryEntry *file; //this holds openfile() return value
	char *p = malloc(5*BLOCK_SIZE); //5*512	string that we re going to write to a file
	disk = fopen("drive", "rw+"); //our disk
	char cmd; // will be used to get user input	
	short *timedate = getTimeDate();
    int b = 1; // for while loop
	printf("Welcome to Fnu Frangky's Filesystem\n");
	//get user's input
	while(b){
		char *input = malloc(512);
		printf("List of command:\n f to format the disk\n q to quit the program \n d to create a directory (must be created one by one)\n c to create a file\n o to open a file\n w to write to a file (must be opened first) \n r to read a file (must be opened first) \n x to delete a file\n z to close a file (must be opened first) \n");
		printf("Your input: ");
		cmd = getchar();
		//format the disk
		if(cmd=='f'){
			//set every byte as 00 to our disk
			my_format();
			//initialize metadata region,FAT,RD
			disk_Init();
			printf("Format success!\n");
		}
		else if(cmd == 'd'){
			clearInput();
			printf("Enter directory path without spaces\n");
			scanf("%s", input);
			createDirectory(input);
		}
		else if(cmd == 'c'){
			clearInput();
			printf("Enter file path with extension eg. '.txt'\n");
			scanf("%s", input);
			createFile(input);
		}	
		else if(cmd=='q')
			b = 0;
		else if(cmd == 'o'){
			clearInput();
			printf("Please specify the path of the file to open\n");
			scanf("%s", input);
			file = openFile(input);
		}
		else if(cmd == 'w'){
			
	//		char copy[20];
			printf("Please enter the string (no whitespace): ");
			scanf("%s",p);
		//	scanf("%s", copy);
	//		p = strcpy(p,copy);
			//	p = strcpy(p, "this is what i wrote\n");
			printf("printing the string that you just wrote: %s\n",p);
			int s = writeFile(file, p);
	//		printf("write succesful: %d\n", s);
		}
		else if(cmd == 'x')
		{
            clearInput();
            printf("Enter file path to delete\n");
            scanf("%s", input);
			deleteFile(input);
		}
		else if(cmd == 'z'){
			closeFile(file);
		}
		else if(cmd == 'r'){
			char *s = readFile(file);
			printf("Reading what's in %s file...\n",file);
			printf("Result:\n");
			printf("%s\n\n",s);
//			if(strcmp(d, p)==0){
//				printf("The strings:\n%s\nand\n%s\nMatch\n", d, p);
//		}
		}
		else {
			printf("Invalid command\n");
		}
		clearInput();
		//free(input);
	}
}
//this function clears the input of scanf
void clearInput(){
	char input;
	while(input!='\n')
	{
		input = fgetc(stdin);
	}
}

//this function finds first available cluster/sector
//it looks for a free entry in FAT table first
//then returns the sector number in the data region
int findEmptySector(){
	int i = 1;	
	//go to FAT and read first entry into f
	fseek(disk, firstByte(FAT)+2, SEEK_SET); //3rd offset in fat is the first entry
	FATentry *f = malloc(sizeof(FATentry)); //size of fat entry is 2 bytes
	fread(f, 2, 1, disk);
	//read first FAT entry
	for(;i<=9688;i++){
		if(f->next == 0){ //if we find the first available cluster
			//seek to first available cluster and return cluster
			fseek(disk, firstByte(i+NUM_RESERVED_SECTORS), SEEK_SET);
			//i.e we find empty clus in sec 15 FAT, go to sector 61+15 in data region
			return i;
		}
		else
			fread(f, 2, 1, disk);
	}
	printf("No more space available");
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
		}	
	fseek(disk, 0, SEEK_SET);
	for(i=0; i<TOTAL_SECTORS; i++) // total sectors = 9765 and fill each sector with 512 zeros b/c each sector has 512 byte
	{
		fwrite(zeros, 512,1,disk);
	}
	fseek(disk,0, SEEK_SET);
}

//this function finds the 1st byte of a sector
int firstByte(int sector_num){
	sector_num = (sector_num-1)*512; //takes you to the very first byte of a sector
	return (sector_num);
}

short *getTimeDate(){
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

}

void disk_Init(){
	//reset all global variables
	fseek(disk, 0, SEEK_SET);
	curDirectory = RD_START_SECTOR-NUM_RESERVED_SECTORS; //1
        curCluster = curDirectory; // 1
        curSpace = NUM_DATAREGION_SECTORS; // NUM_DATAREGION_SECTORS == 9704
/*
	//create bootblock in cluster 0
	void *bootblock = malloc(512); 
        char *boot = (char*)bootblock;
        boot = strcat(boot,"Fnu's FAT Filesystem");
        boot+=11;
        //byte 11-12: num bytes per sector(512)
        *boot = 0x02;
        //byte 13: sectors per cluster(1)
        boot+=2; *boot = 0x01;
        //byte 14-15: number of reserved sectors(1)
        boot+=2; *boot = 0x01;
        //byte 16: number of FAT copies (2)
        boot+=1; *boot = 0x02;
        //byte 17-18: number of root directory entries(0 because FAT32)
        boot+=2;
        //byte 19-20: total number of sectors in filesystem(9760)
        boot+=1; *boot = 0x26; boot++; *boot = 0x20;
        //byte 21: media descriptor type(f8: hard disk?)
        boot+=1; *boot = 0xF8;
        //byte 22-23: sectors per FAT(0 for FAT32)
        boot+=2;
        //byte 24-25: numbers of sectors per track(12?)
        boot+=2; *boot = 0x0C;
        //byte 26-27: nuber of heads(2?)
        boot+=2; *boot = 0x02;
        //byte 28-29: number of hidden sectors(0)
        boot+=2;
        //byte 30-509: would be bootstrap
        boot+=1; boot = strcat(boot, "This would be the bootstrap");
        //byte 510-511: signature 55 aa
        boot+=480; *boot = 0x55; boot++; *boot = 0xaa;

        //write bootblock to disk
	fseek(disk, 0, SEEK_SET);
        fwrite(bootblock, 1, 512,disk);
	free(bootblock);
*/

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
	
	//create root directory table
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
void createFATentry(int sector_num, short next){
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
}

int getNextCluster(int sector_num){
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
}

//this function creates directory according to the pathname and returns directory entry struct
directoryEntry *createDirectory(char *path){
	int i, j, c;
	//create array of entries to read
	directoryEntry *entry = malloc(16*sizeof(directoryEntry));//a whole sector
	directoryEntry *h = entry; //

	//create return file descriptor
	directoryEntry *dir;
	
	//array of each directory and the file 
	char *names[16];
	char entryName[12];
	
	//use strtok to seperate the pathname every "/"
        names[0] = strtok(path, "/");
	for(i = 1; names[i-1]!= NULL && i < 16; i++){
		names[i] = strtok(NULL, "/");
	}
	i--;
	//go to first byte of RD entry
	fseek(disk, firstByte(RD_START_SECTOR), SEEK_SET); //62
	for(j = 0 ; j < i ; entry++){
		//read entries until path is totally parsed
		fread(entry, 32, 1, disk);
		curOffset+=32;

		//change entry->name to comparable, NULL terminated string
		if(entry->stCluster!=NULL||entry->stCluster!=0){
			c = 0; 
			while(entry->name[c]!= ' '&& entry->name[c]!=NULL){
				entryName[c] = (char)entry->name[c];
				c++;
			}
		}
		entryName[c] = '\0';
		char *e  = &entryName;
		//if the path is not the final directory to create
		if(i>j+1){
			//if the path's name matches the entry name
			if(strcmp(e, names[j])==0){
				//seek to that directory
				fseek(disk,firstByte(entry->stCluster+NUM_RESERVED_SECTORS),SEEK_SET);
				curCluster = entry->stCluster; 
				curOffset = 0;
				parentDirectory = curDirectory; 
				curDirectory = curCluster; 
				j++;
			}
			else if(entry->stCluster == 0){
				printf("path not found\n");
				return NULL;
			}	
		}
		//if entryname is the same as whats in names[j]
		else if(strcmp(e, names[j])==0){
			printf("File or directory already exists with this name\n");
			return NULL;
		}
		//if the current offset is already 512, that directory is full
		else if(curOffset == 512){
			printf("No space in current directory table\n");
			return NULL;
		}
		else if(entry->stCluster == 0||entry->stCluster == NULL)
			i--;
		
	}
	//get the timestamp for directory entry
	short *timeDate = getTimeDate();
	short date = *(timeDate+1);
	//set the file attribute
	char attributes = 0x08; //file attr 08 means its a subdir

	//create directory entry and FAT entry
	curCluster = findEmptySector();
	//for a directory I allocate a whole sector(512) as filesize
	// just for simplicity and temporary solution
	dir = createDirEntry(names[j], attributes, *timeDate, date, curCluster, BLOCK_SIZE);
	// create FF FF entry in fat table because its only 1 sector big
	createFATentry(curCluster, 0xFFFF);
	createDirTable();
	free(h);

	return dir;
}
//this function creates directory entry
//each dir entry is 32 bytes
//returns directory entry struct
directoryEntry *createDirEntry(char *namep, char attributes, short time, short date, short stCluster, long filesize){
	int i;
	directoryEntry *entry = malloc(sizeof(directoryEntry));
	directoryEntry *check = malloc(sizeof(directoryEntry));
//	int numEntries;
	//get name and update entry
	for(i = 0; i <11; i++, namep++){ //11 bytes for name
		if(*namep!=NULL)
		{
			//we found the start of the extension
			if(*namep == '.'){
				namep++;
				for(;i<8;i++)
					entry->name[i] = ' '; //add some whitespace before dot to file the whole 8 char
			}
			//if name of the file is bigger than 8 (excluding extension)
			else if(i == 8 && *namep!='.'){
                        	printf("Create file or directory failed, file name too long");
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
		entry->rdonly = 1;
	}
	else
		entry->rdonly = 0;
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
		entry->sysfil = 1;
	}
	else
		entry->sysfil = 0;
	if(attributes>=16)
	{
		attributes -= 16;
		entry->volLabel = 1;
	}
	else
		entry->volLabel = 0;
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
		entry->bit = 0;
		entry->bit1 = 0;
	//set the unused offset to 00
	for(i = 0; i < 10; i++) 
		entry->pad[i] = 0x00;
	// set the modified time and date
	entry->time = time;
	entry->date = date;
	// set the start cluster
	entry->stCluster = stCluster;
	// set the filesize
	entry->fileSize = filesize;
	// find an empty 32 byte entry 
	// it depends heavily where we are right now according to curDirectory
	fseek(disk, firstByte(curDirectory+NUM_RESERVED_SECTORS), SEEK_SET);
	fread(check, 32, 1, disk); // we use a struct just to check, we dont want insert the data into our already prepared entry
	while(check->stCluster!=0) //until we find start cluster = 0
		fread(check, 32, 1, disk);	
	//go back to the empty entry and write to disk
	fseek(disk, -32, SEEK_CUR);
	fwrite(entry, 32, 1, disk);	//we are in the right position, now write the entry
	//seek back to previous position
	fseek(disk, firstByte((entry->stCluster+NUM_RESERVED_SECTORS)), SEEK_SET);
	return entry;
}

directoryEntry *createFile(char *path){
	int i, j, c;
	directoryEntry *entry = malloc(sizeof(directoryEntry));
	directoryEntry *h = entry;
	directoryEntry *fil;
	char *names[16];
	char entryName[12];
	char *p = path;
	char *e;
	while(*p!= 0)
	{
		p++;
	}//end while
	*p = '\0';
	p = path;
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
	}
	names[0] = strtok(path, "/");
        //separate path using the slash
        for(i = 1; names[i-1]!= NULL && i < 16; i++)
		{
                names[i] = strtok(NULL, "/");
        } //end for
        i--;
	
        fseek(disk, firstByte(RD_START_SECTOR), SEEK_SET);
        for(j = 0 ; j < i ; )
		{
            //read entries until path is totally parsed
            fread(entry, 32, 1, disk);
            curOffset+=32;
            //change entry->name to comparable, NULL terminated string
            if(entry->stCluster!= 0 ||entry->stCluster!='\0')
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
                            fseek(disk,firstByte(entry->stCluster+NUM_RESERVED_SECTORS),SEEK_SET);
                            curCluster = entry->stCluster;
                            curOffset = 0;
                            parentDirectory = curDirectory;
                            curDirectory = curCluster;
                            j++;
                        }
                        else if(entry->stCluster == 0)
						{
                            printf("path not found\n");
                            return NULL;
                        }
                }//end if i>j
		//if the path name is now the file to create
                else if(strcmp(e, names[j])==0){
                        printf("File or directory already exists with this name\n");
                        return NULL;
                }
                else if(curOffset == 512){
                        printf("No space in current directory table\n");
                        return NULL;
                }
                else if(entry->stCluster == 0||entry->stCluster == NULL)
                        i--;
        }

	//get time 
	short *timeDate = getTimeDate();
    short date = *(timeDate+1);

	//0 for all attribute
	char attributes = 0x00;

	curCluster = findEmptySector();
    fil = createDirEntry(names[j], attributes, *timeDate, date, curCluster, BLOCK_SIZE*2);
    createFATentry(curCluster, curCluster+1);
	createFATentry(curCluster+1, 0xFFFF);
	free(h);
	return fil;
}

directoryEntry *openFile(char *path){
	int i, j, c;
	//allocate entry to read and entry to return
	directoryEntry *entry = malloc(sizeof(directoryEntry));
	directoryEntry *file = malloc(sizeof(directoryEntry));
	
	//char arrays and pointers for path parsing
	char *name[16];
	char *names[16];
	char entryName[12];
	char *e, *ex;

	name[0] = strtok(path, "/");
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
	for(; j < 8; j++, e++){
		*e = ' ';
	}

	//set curDirectory and seek to root
	curDirectory = RD_START_SECTOR;
	fseek(disk, firstByte(RD_START_SECTOR), SEEK_SET);
        for(j = 0 ; j < i ; ){
                //read entries until path is totally parsed
                fread(entry, 32, 1, disk);
                curOffset+=32;
                //change entry->name to comparable, NULL terminated string
                if(entry->stCluster!= 0){
                        c = 0;
                        while((i>j+1 && entry->name[c]!= ' ')  || (i<=j+1 && c < 12)){
                                entryName[c] = (char)entry->name[c];
                                c++;
                        }
			entryName[c] = '\0';
                }
		e = (char*)&entryName;
		//if path is still directory
                if(i>j+1){
			//if entry name and path are the same, go to that directory
                        if(strcmp(e, names[j])==0){
                                fseek(disk,firstByte(entry->stCluster+NUM_RESERVED_SECTORS),SEEK_SET);
				curDirectory = entry->stCluster;
				curCluster = entry->stCluster;
                                curOffset = 0;
                                j++;
                        }
                        else if(entry->stCluster == 0){
                                printf("path not found\n");
                                return NULL;
                        }
                }
		//got to file in path, find entry and set file 
		else{
			
			if(strcmp(e, names[j])==0){
				file = entry;
				i--;
			}
			else if(entry->stCluster == 0 || curOffset == 512){
				printf("file not found\n");
				return NULL;
			}
		}
	}
	return file;
}

int closeFile(directoryEntry *file){
	//free file to "close"
	free(file);

	//seek to root;
	fseek(disk, firstByte(RD_START_SECTOR), SEEK_SET);
	curDirectory = RD_START_SECTOR-NUM_RESERVED_SECTORS;
	curCluster = curDirectory;
	return 0;
}

int writeFile(directoryEntry *file, char *write){
	int i = 0, d = 0;
	char cluster[512];
	char *c = malloc(1);
	char *h = write;

	//if file is created
	if(file->stCluster!= 0){
		//go to file
		fseek(disk, firstByte(file->stCluster+NUM_RESERVED_SECTORS), SEEK_SET);
		curCluster = file->stCluster;

		//read one char at a time to end of file
		fread(c, 1, 1, disk);
		while(*c!=0x00){
			fread(c, 1, 1, disk);
			curOffset++;
 		}
		//seek back one byte and write entire buffer
		fseek(disk, -1, SEEK_CUR);
		while(*write != '\0'){
			//write one cluster
			for(i = 0; i < 512 && *write != '\0'; i++, write++)
				cluster[i] = *write;
			fwrite((char*)&cluster, i, 1, disk);

			//if whole cluster written, go to next cluster
			if(i == 512)
				d = getNextCluster(curCluster);
			//if file full, allocate more space
			if(d == -1){
				createFATentry(curCluster, findEmptySector());
				curCluster = getNextCluster(curCluster);
				fseek(disk, firstByte(curCluster+NUM_RESERVED_SECTORS), SEEK_SET);
				d = 0;
			}
			else{
				curCluster = d;
                fseek(disk, firstByte(curCluster+NUM_RESERVED_SECTORS), SEEK_SET);
			}
		}
		return 0;
	}
	else
		return -1;
}

char *readFile(directoryEntry *file){
        int i = 1;
	int d = 0;
        char *c = malloc(10*512);
	char *h = c;
	//parse similar to write
        if(file->stCluster!= 0){
                //go to file
                fseek(disk, firstByte(file->stCluster+NUM_RESERVED_SECTORS), SEEK_SET);
                curCluster = file->stCluster;
		//while there are more clusters in the file
                while(d!=-1 && i){
                        fread(c, 512, 1, disk);
			if(*c == 0){
				i = 0;
			}
			//get cluster
                        d = getNextCluster(curCluster);
			c = c+512;
			if(d!=-1)
				curCluster = d;
                }
	}
	else
		return NULL;
	return h;

}
//this function deletes a file, it takes filepath as parameter
// returns 0 if the deletion is successful
int deleteFile(char *path){
	//use openfile function to parse the path given by parameter
	directoryEntry *file = openFile(path);
	//if the start cluster is 0, it means the path does not exist
	if(file->stCluster == 0)
		return -1;
	//allocate entry
	directoryEntry *entry = malloc(sizeof(directoryEntry));
	//allocate 512 x 00
	char *nul = malloc(512*sizeof(char));
	//set the global var curCluster to be the same as start cluster
	curCluster = file->stCluster;
	//seek to current directory
	// we get the current directory value from openfile function
	fseek(disk, firstByte(curDirectory+NUM_RESERVED_SECTORS), SEEK_SET);
	//read entry 
	fread(entry, 32, 1, disk);
	curOffset = 32;
	//find the file by comparing the star cluster
	while(entry->stCluster!=file->stCluster)
	{ 
		fread(entry, 32, 1, disk);
	}// end while
	/// seek to the start of the entry
	fseek(disk, -32, SEEK_CUR);
	//delete the entry by writing 32 byte worth of 00 00
	fwrite(nul, 32,1, disk); // write 00 32 times from the start
	//write all 00 in the data region according to start cluster
	//reserved is = metadata+fat big
	fseek(disk, firstByte(entry->stCluster+NUM_RESERVED_SECTORS), SEEK_SET); 
	fwrite(nul, 512,1, disk);
	
	//write 0000 to all clusters in file
	while(curCluster!=-1){
		int i = getNextCluster(curCluster);
		//delete the corresponding fat entry by writing 0000
        createFATentry(curCluster, 0x0000);
		curCluster = i;
		fwrite(nul, 512, 1, disk);//might be unecessary
	}
	free(nul);
	return 0;
}
