// Zip module
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "unzip.h"

static unzFile Zip=NULL;

int ZipOpen(char *ZipName)
{
	char *ext = NULL;
  
	ext = strrchr( ZipName, '.' );
	if( ext != NULL ) {
		ext++;
		if( strcasecmp( ext, "zip" ) != 0 ) {
			return 1;
		}
	}

	Zip=unzOpen(ZipName);
	if (Zip==NULL) return 1;
	unzGoToFirstFile(Zip);

	return 0;
}

int ZipClose()
{
  if (Zip!=NULL) unzClose(Zip);  Zip=NULL;
  return 0;
}

int ZipRead(unsigned char **pMem,int *pLen)
{
  int Ret=0,i=0;
  unz_file_info Info;
  char Name[256];
  int RomPos=0; // Which entry in the zip file is the rom
  unsigned char *Mem=NULL; int Len=0;

  if (Zip==NULL) return 1;
  memset(&Info,0,sizeof(Info));
  memset(Name,0,sizeof(Name));

  // Find out which entry is the rom
  unzGoToFirstFile(Zip);
  for (i=0; ;i++)
  {
    char *ext = NULL;
    unzGetCurrentFileInfo(Zip,&Info,Name,sizeof(Name),NULL,0,NULL,0);
	
	ext = strrchr( Name, '.' );
	if( ext != NULL ) {
		ext++;
		if( (strcasecmp( ext, "sms" ) == 0)
		||  (strcasecmp( ext, "gg"  ) == 0) ) {
			RomPos = i;
			break;
		}
	}	
	
    Ret=unzGoToNextFile(Zip); if (Ret!=UNZ_OK) break;
  }
  // If we didn't find the correct extension, just assume it's the first entry

  // Now go to the rom again
  unzGoToFirstFile(Zip);
  for (i=0;i<RomPos;i++) unzGoToNextFile(Zip);
  unzGetCurrentFileInfo(Zip,&Info,NULL,0,NULL,0,NULL,0);

  // Find out how long it is and allocate space
  Len=Info.uncompressed_size;
  Mem=(unsigned char *)malloc(Len);
  if (Mem==NULL) return 1;
  memset(Mem,0,Len);
  // Read in the rom
  unzOpenCurrentFile(Zip);
  unzReadCurrentFile(Zip,Mem,Len);
  unzCloseCurrentFile(Zip);

  // Return the allocated memory
  *pMem=Mem; *pLen=Len;
  return 0;
}

