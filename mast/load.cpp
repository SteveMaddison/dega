// Mast - load module
#include <openssl/md5.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "mastint.h"

#define ISHEX(x) ((x >= '0' && x <= '9') || (x >= 'a' && x <= 'f') || (x >= 'A' && x <= 'f'))

extern int ZipOpen(char *ZipName);
extern int ZipClose();
extern int ZipRead(unsigned char **pMem,int *pLen);

static const char *gg_sms_list = "./ggsms";

struct ggsms_game {
	struct ggsms_game *next;
	unsigned char md5sum[MD5_DIGEST_LENGTH];
};
struct ggsms_game *ggsms_start = NULL;


int read_gg_sms_list( void ) {
	FILE *f = fopen( gg_sms_list, "r" );
	
	if( f == NULL ) {
		fprintf( stderr, "Couldn't open GameGear/SMS hash list '%s': %s\n", gg_sms_list, strerror( errno ) );
		return -1;
	}
	else {
		char buf[256];
		unsigned char md5[MD5_DIGEST_LENGTH];
		char *p = NULL;
		int len = 0;
		unsigned int val = 0;
		
		while( fgets( buf, 256, f ) != NULL ) {
			len = 0;
			buf[MD5_DIGEST_LENGTH*2] = 0;
			p = buf;
			
			while( *p ) {
				if( ISHEX(*p) && ISHEX(*(p+1)) ) {
					sscanf( p, "%2x", &val );
					md5[len] = val;
					len++;
					p += 2;
				}
				else {
					fprintf( stderr, "Invalid MD5 (not a hex character)\n" );
					break;
				}
			}
			
			if( len == MD5_DIGEST_LENGTH ) {
				struct ggsms_game *game = (struct ggsms_game*)malloc( sizeof(struct ggsms_game) );
				if( game == NULL ) {
					fprintf( stderr, "Couldn't allocate memory for GameGear/SMS hash entry\n" );
					break;
				}
				else {
					game->next = ggsms_start;
					memcpy( game->md5sum, md5, MD5_DIGEST_LENGTH );
					ggsms_start = game;
				}
			}
			
			memset( buf, 0, 256 );
		}
		fclose( f );
	}
	
	return 0;
}

int MastLoadRom(char *Name,unsigned char **pRom,int *pRomLen)
{
	FILE *h=NULL; int Len=0;
	unsigned char *Start=NULL;
	unsigned char *Mem=NULL;
	int AllocLen=0;
	char *FileName;
	unsigned char md5sum[MD5_DIGEST_LENGTH];

	if (Name==NULL) return 1;
	if (Name[0]==0) return 1;

	if( ZipOpen( Name ) == 0 ) {
		ZipRead( &Start, &Len );
		ZipClose();

		Mem = Start;
		*pRomLen = Len;
		
		// If it looks like there is a 0x200 byte header, skip it
		if ((Len&0x3fff)==0x0200) {
			Mem += 0x0200;
			*pRomLen -= 0x0200;
		}
	}
	else {
		h=fopen(Name,"rb"); if (h==NULL) return 1;

		fseek(h,0,SEEK_END);
		Len=ftell(h);
		fseek(h,0,SEEK_SET);

		*pRomLen = Len;

		AllocLen=Len;
		if ((Len&0x3fff)==0x0200) {
			AllocLen-=0x200;
		}
		AllocLen+=0x3fff; AllocLen&=0x7fffc000; AllocLen+=2; // Round up to a page (+ overrun)

		Start=(unsigned char *)malloc(AllocLen);
		if (Start==NULL) { fclose(h); return 1; }
		memset(Start,0,AllocLen);

		fread(Start,1,Len,h);

		Mem = Start;
		if ((Len&0x3fff)==0x0200) {
			Mem += 0x0200;
			*pRomLen -= 0x0200;
		}

		fclose(h);
	}

	FileName=strrchr(Name,'/');
	if (FileName) { FileName++; }
		   else { FileName=Name; }

	MastSetRomName(FileName);

	*pRom=Mem;

	if( read_gg_sms_list() == 0 && ggsms_start ) {
		MD5( Start, Len, md5sum );
		struct ggsms_game *game = ggsms_start;
		while( game ) {			
			if( memcmp( game->md5sum, md5sum, MD5_DIGEST_LENGTH ) == 0 ) {
				fprintf( stderr, "SMS/GG Hack enabled!\n" );
				return -1;
			}
			game = game->next;
		}
	}

	return 0;
}

