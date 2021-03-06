#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

//constants
#define SECTORS 9765
#define DATASIZE 9688
#define FATSIZE 76
#define RESERVED 77
#define BLOCKSIZE 512
#define ROOT 78
#define FAT 2

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

//each FAT entry is 2 bytes, holds next cluster in file or special character 
typedef struct FATentry{
	short next;
}FATentry;

//Each Directory table entry is 32 bytes
typedef struct dirEntry{
	char name[11];
	unsigned int rdonly : 1;
	unsigned int hidden : 1;
	unsigned int sysfil : 1;
	unsigned int volLabel : 1;
	unsigned int subdir : 1;
	unsigned int archive : 1;
	unsigned int bit : 1;
	unsigned int bit1 : 1;
	char pad[10];
	short time;
	short date;
	short stCluster;
	long fileSize;
}dirEntry;  

//function declarations
void clearInput();
void my_format();
int firstByte(int cluster);
short *getTimeDate();
int firstAvailable();
void formatDrive();
void createDirTable();
void createFATentry(int cluster, short next);
int getNextCluster(int cluster);
dirEntry *createDirEntry(char *namep, char attributes, short time, short date, short stCluster, long fileSize);
dirEntry *createDirectory(char *path);
dirEntry *createFile(char *path);
dirEntry *openFile(char *path);
int closeFile(dirEntry *file);
int writeFile(dirEntry *file, char *write);
char *readFile(dirEntry *file);
int deleteFile(char *path);

//initialize file pointer
FILE *drive;

//initialize cluster "pointers"
int currentDir;
int parentDir;
int currentCluster;
int currentSpace;
int currentOffset;

//initialize time variables
time_t rawtime;
struct tm *timeinfo;

//my main function is to test the functions, I will make a demo for my demo
int main(){
	currentDir = ROOT-RESERVED; //1
	currentCluster = currentDir; //1
	currentSpace = DATASIZE; //9688

	dirEntry *file;
	char *p = malloc(5*BLOCKSIZE); //5*512
	
	drive = fopen("drive", "rw+");
	char cmd; // to get user input
	
	short *timedate = getTimeDate();

	//initialize boolean for input
    int b = 1;
	printf("Welcome to Frank's Filesystem\n");
	//create FAT, bootblock, and root directory table
	while(b){
		char *input = malloc(512);
		printf("List of command:\n f to format the drive\n q to quit the program \n d to create a directory(must be created one by one)\n c to create a file\n o to open a file\n w to write to a file\n x to delete a file\n z to close a file\n r to read a file\n");
		printf("Your input: ");
		cmd = getchar();
		if(cmd=='f'){
			//set every byte as 00 to our disk
			my_format();
			//create FAT, bootblock, and root directory table
			formatDrive();
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
			printf("write succesful: %d\n", s);
		}
		else if(cmd == 'x'){
                 	clearInput();
                        printf("Enter file path to delete\n");
                        scanf("%s", input);
			deleteFile(input);
		}
		else if(cmd == 'z'){
			closeFile(file);
		}
		else if(cmd == 'r'){
			char *d = readFile(file);
			printf("Reading what's in %s file...\n",file);
			printf("Result:\n");
			printf("%s\n\n",d);
//			if(strcmp(d, p)==0){
//				printf("The strings:\n%s\nand\n%s\nMatch\n", d, p);
//		}
		}
		clearInput();
		//free(input);
	}
}
//clears input for scanf
void clearInput(){
	char d;
	while(d!='\n')
		d = fgetc(stdin);
}

//finds first available cluster
int firstAvailable(){
	int i = 1;
	
	//go to FAT and read first entry into f
	fseek(drive, firstByte(FAT)+2, SEEK_SET);
	FATentry *f = malloc(sizeof(FATentry));
	fread(f, 2, 1, drive);
	//read first FAT entry
	for(;i<=9688;i++){
		if(f->next == 0){
			//seek to first available cluster and return cluster
			fseek(drive, firstByte(i+RESERVED), SEEK_SET);
			return i;
		}
		else
			fread(f, 2, 1, drive);
	}
	printf("No more filesystem space\n");
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
	fseek(drive, 0, SEEK_SET);
	for(i=0; i<SECTORS; i++) // total sectors = 9765 and fill each sector with 512 zeros b/c each sector has 512 byte
		fwrite(zeros, 512,1,drive);
	fseek(drive,0, SEEK_SET);
}

//this function finds the 1st byte of a cluster/sector
int firstByte(int sector){
	sector = (sector-1)*512;
	return (sector);
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

void formatDrive(){
	fseek(drive, 0, SEEK_SET);
	currentDir = ROOT-RESERVED; //78-77=1
        currentCluster = currentDir; // 1
        currentSpace = DATASIZE; // DATASIZE == 9688
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

        //write bootblock to drive
	fseek(drive, 0, SEEK_SET);
        fwrite(bootblock, 1, 512,drive);
	free(bootblock);
*/

	Metadata mbr;
	mbr.fsname = "my fs"; //offset 0-7
	mbr.byte_per_sector = 512; // offset 8-9
	mbr.sector_per_cluster = 1; // offset 10
	mbr.reserved_sector = 1; // offset 11-12
	mbr.number_of_fat = 2; // offset 13
	mbr.sector_per_fat = 38; // offset 14-15
	mbr.RD_entries = 512; // offset 16-17
	mbr.total_sector = 9765; // offset 18-19
	unsigned char unused = 0;
	
	fseek(drive, 0, SEEK_SET);
	fwrite(mbr.fsname , sizeof(char) , 8, drive );
	fseek(drive, 8, SEEK_SET);
	fwrite(&mbr.byte_per_sector , sizeof(unsigned short) , 1, drive );
	fseek(drive, 10, SEEK_SET);
	fwrite(&mbr.sector_per_cluster , sizeof(unsigned char) , 1, drive );
	fseek(drive, 11, SEEK_SET);
	fwrite(&mbr.reserved_sector , sizeof(unsigned short) , 1, drive );
	fseek(drive, 13, SEEK_SET);
	fwrite(&mbr.number_of_fat , sizeof(unsigned char) , 1, drive );
	fseek(drive, 14, SEEK_SET);
	fwrite(&mbr.sector_per_fat , sizeof(unsigned short) , 1, drive );
	fseek(drive, 16, SEEK_SET);
	fwrite(&mbr.RD_entries , sizeof(unsigned short) , 1, drive );
	fseek(drive, 18, SEEK_SET);
	fwrite(&mbr.total_sector , sizeof(unsigned short) , 1, drive );
	
	// print 00 to the rest of metadata sector
	int n = 20;
	while (n!=512)
	{
	fseek(drive, n, SEEK_SET);
	fwrite(&unused , sizeof(unsigned char) , 1, drive );
	n++;
	}
	//print 55 AA at the end of first sector 
	unsigned char end1 = 85;
	unsigned char end2 = 170;
	fseek(drive, 510, SEEK_SET);
	fwrite(&end1,sizeof(unsigned char),1,drive);
	fseek(drive, 511, SEEK_SET);
	fwrite(&end2,sizeof(unsigned char),1,drive);

	//create FATs
	void *doubleFAT = malloc(512*FATSIZE); //512 * 76
        fseek(drive, (BLOCKSIZE*FATSIZE) , SEEK_CUR); //reserve 512*76byte for FAT table
        free(doubleFAT);
	
	//create root directory table
	createDirTable();
	createFATentry(1, 0xFFFF);  
	

}

//leaves space for directory table 
void createDirTable(){
	//update currentDirectory cluster pointer 
	currentDir = currentCluster;

	//root starts with one block and is dynamically allocated more as needed
	if(currentDir == ROOT) //78
		createFATentry(currentDir, 0xFFFF);
	fseek(drive, BLOCKSIZE, SEEK_CUR);
	currentCluster+=1; //increment the cluster
	currentSpace -=1; // decrement the datasize
}
//creates fat entry and returns pointer to previous position
void createFATentry(int cluster, short next){
	FATentry *new = malloc(sizeof(FATentry));
	new->next = next;
	//seek to FAT entry for cluster and write next
	fseek(drive, firstByte(FAT)+(2*cluster), SEEK_SET); // prints FF FF at first cluster of fat table. 
	fwrite(new, 2, 1, drive);
	//seek to second FAT and write in entry again
	fseek(drive, 38*BLOCKSIZE-2, SEEK_CUR);
	fwrite(new, 2, 1, drive);
	//seek back to previous location
	fseek(drive, firstByte(cluster)+currentOffset, SEEK_SET);
	free(new);
}

int getNextCluster(int cluster){
	//allocate buffer for FAT entry
	FATentry *entry = malloc(sizeof(FATentry));

	//read and return entry->next
	fseek(drive, firstByte(FAT)+(2*cluster), SEEK_SET);
	fread(entry, 2, 1, drive);
	if(entry->next!=0xFFFF)
		fseek(drive, firstByte(entry->next+RESERVED), SEEK_SET);
	else
		fseek(drive, firstByte(cluster+RESERVED), SEEK_SET);

	return entry->next;
}

dirEntry *createDirectory(char *path){
	int i, j, c;
	//create array of entries to read
	dirEntry *entry = malloc(16*sizeof(dirEntry));
	dirEntry *h = entry;

	//create return file descriptor
	dirEntry *dir;
	
	//array of each directory and the file 
	char *names[16];
	char entryName[12];
	
	//seperate path using the slash's
        names[0] = strtok(path, "/");
	for(i = 1; names[i-1]!= NULL && i < 16; i++){
		names[i] = strtok(NULL, "/");
	}
	i--;
	//seek to root directory table
	fseek(drive, firstByte(ROOT), SEEK_SET);
	for(j = 0 ; j < i ; entry++){
		//read entries until path is totally parsed
		fread(entry, 32, 1, drive);
		currentOffset+=32;

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
				fseek(drive,firstByte(entry->stCluster+RESERVED),SEEK_SET);
				currentCluster = entry->stCluster; 
				currentOffset = 0;
				parentDir = currentDir; 
				currentDir = currentCluster; 
				j++;
			}
			else if(entry->stCluster == 0){
				printf("path not found\n");
				return NULL;
			}	
		}
		else if(strcmp(e, names[j])==0){
			printf("File or directory already exists with this name\n");
			return NULL;
		}
		else if(currentOffset == 512){
			printf("No space in current directory table\n");
			return NULL;
		}
		else if(entry->stCluster == 0||entry->stCluster == NULL)
			i--;
		
	}
	//Do I need to create . and ..?
	
	//get time
	short *timeDate = getTimeDate();
	short date = *(timeDate+1);
	char attributes = 0x08;

	//create diectory entry and FAT entry
	currentCluster = firstAvailable();
	dir = createDirEntry(names[j], attributes, *timeDate, date, currentCluster, BLOCKSIZE);
	createFATentry(currentCluster, 0xFFFF);
	createDirTable();

	free(h);

	return dir;
}

dirEntry *createDirEntry(char *namep, char attributes, short time, short date, short stCluster, long filesize){
	int i;
	dirEntry *entry = malloc(sizeof(dirEntry));
	dirEntry *check = malloc(sizeof(dirEntry));
	int numEntries;
	//get name and update entry
	for(i = 0; i <11; i++, namep++){
		if(*namep!=NULL){
			//move to extension
			if(*namep == '.'){
				namep++;
				for(;i<8;i++)
					entry->name[i] = ' ';
			}
			else if(i == 8 && *namep!='.'){
                        	printf("Create file or directory failed, file name too long");
                        	return NULL;
               		}
			entry->name[i] = *namep;
		}
		else{	
			for(; i < 11; i++)
				entry->name[i]= ' ';
		}
	}
	
	//set attributes
	if(attributes>=128){
		attributes -=128;
		entry->rdonly = 1;
	}
	else
		entry->rdonly = 0;
	if(attributes>=64){
		attributes -= 64;
		entry->hidden = 1;
	}
	else
		entry->hidden = 0;
	if(attributes>=32){
		attributes -= 32;
		entry->sysfil = 1;
	}
	else
		entry->sysfil = 0;
	if(attributes>=16){
		attributes -= 16;
		entry->volLabel = 1;
	}
	else
		entry->volLabel = 0;
	if(attributes>=8){
		attributes -= 8;
		entry->subdir = 1;
	}
	else
		entry->subdir = 0;
	if(attributes>=4){
		attributes -= 4;
		entry->archive = 1;
	}
	else
		entry->archive = 0;
	entry->bit = 0;
	entry->bit1 = 0;
		
	for(i = 0; i < 10; i++)
		entry->pad[i] = 0x00;

	//set time&date
	entry->time = time;
	entry->date = date;

	//set starting cluster and filesize
	entry->stCluster = stCluster;
	entry->fileSize = filesize;

	//find empty entry
	fseek(drive, firstByte(currentDir+RESERVED), SEEK_SET);
	fread(check, 32, 1, drive);
	while(check->stCluster!=0)
		fread(check, 32, 1, drive);
	
	//move back to empty entry and write to drive
	fseek(drive, -32, SEEK_CUR);
	fwrite(entry, 32, 1, drive);
	
	//seek back to previous position
	fseek(drive, firstByte((entry->stCluster+RESERVED)), SEEK_SET);
	return entry;
}

dirEntry *createFile(char *path){
	int i, j, c;
	dirEntry *entry = malloc(sizeof(dirEntry));
	dirEntry *h = entry;
	dirEntry *fil;
	char *names[16];
	char entryName[12];
	char *p = path;
	char *e;
	while(*p!= 0){p++;}
	*p = '\0';
	p = path;
	if(strchr(p, '.')!=NULL){
		i = 0;
		while(*p!='.'){
			p++;
			i++;
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
        //seperate path using the slash's
        for(i = 1; names[i-1]!= NULL && i < 16; i++){
                names[i] = strtok(NULL, "/");
        }
        i--;
	
        fseek(drive, firstByte(ROOT), SEEK_SET);
        for(j = 0 ; j < i ; ){
                //read entries until path is totally parsed
                fread(entry, 32, 1, drive);
                currentOffset+=32;

                //change entry->name to comparable, NULL terminated string
                if(entry->stCluster!= 0 ||entry->stCluster!='\0'){
                        c = 0;
                        while(entry->name[c]!= ' '&& c < 12){
                                entryName[c] = (char)entry->name[c];
                                c++;
                        }
                }
                entryName[c] = '\0';
                e  = (char*)&entryName;
                //find directory to write new file into
                if(i>j+1){
                        if(strcmp(e, names[j])==0){
                                fseek(drive,firstByte(entry->stCluster+RESERVED),SEEK_SET);
                                currentCluster = entry->stCluster;
                                currentOffset = 0;
                                parentDir = currentDir;
                                currentDir = currentCluster;
                                j++;
                        }
                        else if(entry->stCluster == 0){
                                printf("path not found\n");
                                return NULL;
                        }
                }
		//if the path name is now the file to create
                else if(strcmp(e, names[j])==0){
                        printf("File or directory already exists with this name\n");
                        return NULL;
                }
                else if(currentOffset == 512){
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

	currentCluster = firstAvailable();
        fil = createDirEntry(names[j], attributes, *timeDate, date, currentCluster, BLOCKSIZE*2);
        createFATentry(currentCluster, currentCluster+1);
	createFATentry(currentCluster+1, 0xFFFF);

	free(h);
	return fil;
}

dirEntry *openFile(char *path){
	int i, j, c;
	//allocate entry to read and entry to return
	dirEntry *entry = malloc(sizeof(dirEntry));
	dirEntry *file = malloc(sizeof(dirEntry));
	
	//char arrays and pointers for path parsing
	char *name[16];
	char *names[16];
	char entryName[12];
	char *e, *ex;

	name[0] = strtok(path, "/");
	names[0] = malloc(12*sizeof(char));
	strcpy(names[0], name[0]);
        //seperate path using the slash's
        for(i = 1; name[i-1]!= NULL && i < 16; i++){
                name[i] = strtok(NULL, "/");
		if(name[i]!=0x0){
			names[i] = malloc(12*sizeof(char));
			strcpy(names[i], name[i]);
		}
        }
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

	//set currentDir and seek to root
	currentDir = ROOT;
	fseek(drive, firstByte(ROOT), SEEK_SET);
        for(j = 0 ; j < i ; ){
                //read entries until path is totally parsed
                fread(entry, 32, 1, drive);
                currentOffset+=32;
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
                                fseek(drive,firstByte(entry->stCluster+RESERVED),SEEK_SET);
				currentDir = entry->stCluster;
				currentCluster = entry->stCluster;
                                currentOffset = 0;
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
			else if(entry->stCluster == 0 || currentOffset == 512){
				printf("file not found\n");
				return NULL;
			}
		}
	}
	return file;
}

int closeFile(dirEntry *file){
	//free file to "close"
	free(file);

	//seek to root;
	fseek(drive, firstByte(ROOT), SEEK_SET);
	currentDir = ROOT-RESERVED;
	currentCluster = currentDir;
	return 0;
}

int writeFile(dirEntry *file, char *write){
	int i = 0, d = 0;
	char cluster[512];
	char *c = malloc(1);
	char *h = write;

	//if file is created
	if(file->stCluster!= 0){
		//go to file
		fseek(drive, firstByte(file->stCluster+RESERVED), SEEK_SET);
		currentCluster = file->stCluster;

		//read one char at a time to end of file
		fread(c, 1, 1, drive);
		while(*c!=0x00){
			fread(c, 1, 1, drive);
			currentOffset++;
 		}
		//seek back one byte and write entire buffer
		fseek(drive, -1, SEEK_CUR);
		while(*write != '\0'){
			//write one cluster
			for(i = 0; i < 512 && *write != '\0'; i++, write++)
				cluster[i] = *write;
			fwrite((char*)&cluster, i, 1, drive);

			//if whole cluster written, go to next cluster
			if(i == 512)
				d = getNextCluster(currentCluster);
			//if file full, allocate more space
			if(d == -1){
				createFATentry(currentCluster, firstAvailable());
				currentCluster = getNextCluster(currentCluster);
				fseek(drive, firstByte(currentCluster+RESERVED), SEEK_SET);
				d = 0;
			}
			else{
				currentCluster = d;
                                fseek(drive, firstByte(currentCluster+RESERVED), SEEK_SET);
			}
		}
		return 0;
	}
	else
		return -1;
}

char *readFile(dirEntry *file){
        int i = 1;
	int d = 0;
        char *c = malloc(10*512);
	char *h = c;
	//parse similar to write
        if(file->stCluster!= 0){
                //go to file
                fseek(drive, firstByte(file->stCluster+RESERVED), SEEK_SET);
                currentCluster = file->stCluster;
		//while there are more clusters in the file
                while(d!=-1 && i){
                        fread(c, 512, 1, drive);
			if(*c == 0){
				i = 0;
			}
			//get cluster
                        d = getNextCluster(currentCluster);
			c = c+512;
			if(d!=-1)
				currentCluster = d;
                }
	}
	else
		return NULL;
	return h;

}

int deleteFile(char *path){
	//open file to parse path
	dirEntry *file = openFile(path);
	//if path does not exist
	if(file->stCluster == 0)
		return -1;
	//allocate entry  and nul cluster
	dirEntry *entry = malloc(sizeof(dirEntry));
	char *nul = malloc(512*sizeof(char));
	currentCluster = file->stCluster;

	//seek to current directory
	fseek(drive, firstByte(currentDir+RESERVED), SEEK_SET);
	//read entry 
	fread(entry, 32, 1, drive);
	currentOffset = 32;
	//find the file
	while(entry->stCluster!=file->stCluster){
		fread(entry, 32, 1, drive);
	}
	//seek back on entry
	fseek(drive, -32, SEEK_CUR);
	fwrite(nul, 32,1, drive);

	//seek to start cluster and write null to it
	fseek(drive, firstByte(entry->stCluster+RESERVED), SEEK_SET);
	fwrite(nul, 512,1, drive);
	
	//write 0000 to all clusters in file
	while(currentCluster!=-1){
		int i = getNextCluster(currentCluster);
                createFATentry(currentCluster, 0x0000);
		currentCluster = i;
		fwrite(nul, 512, 1, drive);
	}
	free(nul);
	return 0;
}
