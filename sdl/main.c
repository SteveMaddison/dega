#define APPNAME "dega"
#define APPNAME_LONG "Dega/SDL"
#define VERSION "1.15"

#include <stdio.h>
#include <unistd.h>
#include <mast.h>
#include <SDL.h>
#include <malloc.h>
#include <getopt.h>
#include <langinfo.h>
#include <locale.h>
#include <pthread.h>
#include <time.h>

#ifndef NOPYTHON
#include "../python/embed.h"
#endif

#ifdef FB_RENDER
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/input.h>
#ifndef FBIO_WAITFORVSYNC
#define FBIO_WAITFORVSYNC _IOW('F', 0x20, __u32)
#endif

unsigned short *fbp1 = 0;
unsigned short *fbp2 = 0;
unsigned short *fbp = 0; /* Current buffer */
#else
SDL_Surface *thescreen;
#endif

unsigned short themap[256];
int width, height;
int xoff,yoff;

static int audio_len=0;

int framerate=60;
int mult=0;

int readonly;
int identify = 0;

int python;
int vidflags=0;

#ifndef FB_RENDER
int scrlock()
{
        if(SDL_MUSTLOCK(thescreen))
        {
                if ( SDL_LockSurface(thescreen) < 0 )
                {
                        fprintf(stderr, "Couldn't lock display surface: %s\n",
                                                                SDL_GetError());
                        return -1;
                }
        }
        return 0;
}
void scrunlock(void)
{
        if(SDL_MUSTLOCK(thescreen))
                SDL_UnlockSurface(thescreen);
        SDL_UpdateRect(thescreen, 0, 0, 0, 0);
}
#endif

void MsndCall(void* data, Uint8* stream, int len)
{
	if(audio_len < len)
	{
		memcpy(stream,data,audio_len);
		audio_len=0;
		// printf("resetting audio_len\n");
		return;
	}
	//printf("audio_len=%d\n",audio_len);
	memcpy(stream,data,len);
	audio_len-=len;
	memcpy(data,(unsigned char*)data+len,audio_len);
}

#ifndef FB_RENDER
static char *chompgets(char *buf, int len, FILE *fh) {
	char *ret;
	if ((ret = fgets(buf, len, fh))) {
		char *nl = strchr(buf, '\n');
		if (nl != NULL) {
			*nl = '\0';
		}
	}
	return ret;
}
#endif

static FILE *sf;

static int StateLoadAcb(struct MastArea *pma)
{
  if (sf!=NULL) return fread(pma->Data,1,pma->Len,sf);
  return 0;
}

static int StateLoad(char *StateName)
{
  if (StateName[0]==0) return 1;
  
  sf=fopen (StateName,"rb");
  if (sf==NULL) return 1;

  // Scan state
  MastAcb=StateLoadAcb;
  MastAreaDega();
  MvidPostLoadState(readonly);
  MastAcb=MastAcbNull;

  fclose(sf); sf=NULL;

  return 0;
}

static int StateSaveAcb(struct MastArea *pma)
{
  if (sf!=NULL) fwrite(pma->Data,1,pma->Len,sf);
  return 0;
}

static int StateSave(char *StateName)
{
  if (StateName[0]==0) return 1;

  sf=fopen (StateName,"wb");
  if (sf==NULL) return 1;

  // Scan state
  MastAcb=StateSaveAcb;
  MastAreaDega();
  MvidPostSaveState();
  MastAcb=MastAcbNull;

  fclose(sf); sf=NULL;
  return 0;
}

#ifndef FB_RENDER
static void LeaveFullScreen() {
	if (vidflags&SDL_FULLSCREEN) {
		thescreen = SDL_SetVideoMode(width, height, 15, SDL_SWSURFACE|(vidflags&~SDL_FULLSCREEN));
	}
}

static void EnterFullScreen() {
	if (vidflags&SDL_FULLSCREEN) {
		thescreen = SDL_SetVideoMode(width, height, 15, SDL_SWSURFACE|vidflags);
	}
}
#endif

void HandleSaveState() {
#ifndef FB_RENDER
	char buffer[64];
	LeaveFullScreen();
	puts("Enter name of state to save:");
	chompgets(buffer, sizeof(buffer), stdin);
	StateSave(buffer);
	EnterFullScreen();
#endif
}

void HandleLoadState() {
#ifndef FB_RENDER
	char buffer[64];
	LeaveFullScreen();
	puts("Enter name of state to load:");
	chompgets(buffer, sizeof(buffer), stdin);
	StateLoad(buffer);
	EnterFullScreen();
#endif
}

void HandleRecordMovie(int reset) {
#ifndef FB_RENDER
	char buffer[64];
	LeaveFullScreen();
	printf("Enter name of movie to begin recording%s:\n", reset ? " from reset" : "");
	chompgets(buffer, sizeof(buffer), stdin);
	MvidStart(buffer, RECORD_MODE, reset, 0);
	EnterFullScreen();
#endif
}

void HandlePlaybackMovie(void) {
#ifndef FB_RENDER
	char buffer[64];
	LeaveFullScreen();
	puts("Enter name of movie to begin playback:");
	chompgets(buffer, sizeof(buffer), stdin);
	MvidStart(buffer, PLAYBACK_MODE, 0, 0);
	EnterFullScreen();
#endif
}

void HandleSetAuthor(void) {
#ifndef FB_RENDER
	char buffer[64], buffer_utf8[64];
	char *pbuffer = buffer, *pbuffer_utf8 = buffer_utf8;
	size_t buffersiz, buffersiz_utf8 = sizeof(buffer_utf8), bytes;
	iconv_t cd;

	LeaveFullScreen();
	puts("Enter name of author:");
	chompgets(buffer, sizeof(buffer), stdin);
	buffersiz = strlen(buffer);

	cd = iconv_open("UTF-8", nl_langinfo(CODESET));
	bytes = iconv(cd, &pbuffer, &buffersiz, &pbuffer_utf8, &buffersiz_utf8);
	if (bytes == (size_t)(-1)) {
		perror("iconv");
		iconv_close(cd);
		return;
	}
	iconv_close(cd);

	*pbuffer_utf8 = '\0';

	MvidSetAuthor(buffer_utf8);

	EnterFullScreen();
#endif
}

#ifndef NOPYTHON
void HandlePython(void) {
	char buffer[64];
	LeaveFullScreen();
	
	if (!python) {
		puts("Python not available!");
		return;
	}

	puts("Enter name of Python control script to execute:");
	chompgets(buffer, sizeof(buffer), stdin);

	EnterFullScreen();

	MPyEmbed_Run(buffer);
}

void HandlePythonREPL(void) {
	if (!python) {
		puts("Python not available!");
		return;
	}
	LeaveFullScreen();
	MPyEmbed_Repl();
	EnterFullScreen();
}

void *PythonThreadRun(void *pbuf) {
	char *buffer = pbuf;
	MPyEmbed_RunThread(buffer);
	free(buffer);
	return 0;
}

void HandlePythonThread(void) {
	char *buffer = malloc(64);
	pthread_t pythread;
	LeaveFullScreen();
	
	if (!python) {
		puts("Python not available!");
		return;
	}

	puts("Enter name of Python viewer script to execute:");
	chompgets(buffer, 64, stdin);

	EnterFullScreen();

	pthread_create(&pythread, 0, PythonThreadRun, buffer);
}
#endif

void SetRateMult() {
	int newrate = mult>0 ? framerate<<mult : framerate>>-mult;
	if (newrate < 1) newrate = 1;

	free(pMsndOut);
	MsndLen=(MsndRate+(newrate>>1))/newrate; 
	pMsndOut = malloc(MsndLen*2*2);
}

void MvidModeChanged() {
	framerate = (MastEx & MX_PAL) ? 50 : 60;
	SetRateMult();
}

void MvidMovieStopped() {}

#ifndef FB_RENDER
void MimplFrame(int input) {
	if (input) {
		SDL_Event event;
		int key;

                while(SDL_PollEvent(&event))
                {
		switch (event.type)
                        {
                        case SDL_KEYDOWN:
                                key=event.key.keysym.sym;
                                if(key==SDLK_UP) {MastInput[0]|=0x01;break;}
                                if(key==SDLK_DOWN) {MastInput[0]|=0x02;break;}
                                if(key==SDLK_LEFT) {MastInput[0]|=0x04;break;}
                                if(key==SDLK_RIGHT) {MastInput[0]|=0x08;break;}
                                if(key==SDLK_PAGEDOWN) {MastInput[0]|=0x10;break;}
                                if(key==SDLK_END) {MastInput[0]|=0x20;break;}
                                if(key==SDLK_LALT) {
				  MastInput[0]|=0x80;
				  if ((MastEx&MX_GG)==0)
				    MastInput[0]|=0x40;
				  break;}

                                if(key==SDLK_u) {MastInput[1]|=0x01;break;}
                                if(key==SDLK_j) {MastInput[1]|=0x02;break;}
                                if(key==SDLK_h) {MastInput[1]|=0x04;break;}
                                if(key==SDLK_k) {MastInput[1]|=0x08;break;}
                                if(key==SDLK_f) {MastInput[1]|=0x10;break;}
                                if(key==SDLK_g) {MastInput[1]|=0x20;break;}

                                break;
                        case SDL_KEYUP:
                                key=event.key.keysym.sym;
                                if(key==SDLK_UP) {MastInput[0]&=0xfe;break;}
                                if(key==SDLK_DOWN) {MastInput[0]&=0xfd;break;}
                                if(key==SDLK_LEFT) {MastInput[0]&=0xfb;break;}
                                if(key==SDLK_RIGHT) {MastInput[0]&=0xf7;break;}
                                if(key==SDLK_PAGEDOWN) {MastInput[0]&=0xef;break;}
                                if(key==SDLK_END) {MastInput[0]&=0xdf;break;}
                                if(key==SDLK_LALT) {MastInput[0]&=0x3f;break;}

                                if(key==SDLK_u) {MastInput[1]&=0xfe;break;}
                                if(key==SDLK_j) {MastInput[1]&=0xfd;break;}
                                if(key==SDLK_h) {MastInput[1]&=0xfb;break;}
                                if(key==SDLK_k) {MastInput[1]&=0xf7;break;}
                                if(key==SDLK_f) {MastInput[1]&=0xef;break;}
                                if(key==SDLK_g) {MastInput[1]&=0xdf;break;}
                                break;
                        default:
                                break;
                        }
		}
	}


	scrlock();
	MastFrame();
	scrunlock();

#if 0
	pydega_cbpostframe(mainstate);
#else
#ifndef NOPYTHON
	MPyEmbed_CBPostFrame();
#endif
#endif

	if (input) {
		MastInput[0]&=~0x40;
	}
}
#endif /* ifndef FB_RENDER */

void usage(void)
{
	printf("\nUsage: %s [OPTION]... [ROM file]\n",APPNAME);
	printf("\nOptions:\n");
	printf("     --help\tprint what you're reading now\n");
	printf("  -v --version\tprint version no.\n");
	printf("  -g --gamegear\tforce Game Gear emulation (default autodetect)\n");
	printf("  -m --sms\tforce Master System emulation (default autodetect)\n");
	printf("  -p --pal\tuse PAL mode (default NTSC)\n");
	printf("  -j --japan\tJapanese region\n");
	printf("  -s --nosound\tdisable sound\n");
	printf("  -f --fullscreen\tfullscreen display\n");
	printf("  -r --readonly\tmovies are readonly\n");
	printf("  -i --identify\tidentify ROM type and exit\n");
	printf("\n" APPNAME_LONG " version " VERSION " by Ulrich Hecht <uli@emulinks.de>\n");
	printf("extended by Peter Collingbourne <peter@peter.uk.to>\n");
	printf("based on Win32 version by Dave <dave@finalburn.com>\n");
	exit (0);
}

int main(int argc, char** argv)
{
	unsigned char* rom;
	int romlength;
	int done=0;
	SDL_AudioSpec aspec;
	unsigned char* audiobuf = NULL;
	int paused=0, frameadvance=0;

#ifndef FB_RENDER
	SDL_Event event;
	int key;
#else
	int fbfd = 0;
	struct fb_var_screeninfo vinfo;
	struct fb_fix_screeninfo finfo;
	long int screensize = 0;
	int fbarg = 0;

	struct input_event ev[64];
	char kbdev[32];
	int kbfd = 0;
	int kbrd = 0;
	int i;
#endif

	// options
	int autodetect=1;
	int sound=1;

	setlocale(LC_CTYPE, "");

	readonly = 0;

#ifndef NOPYTHON
	MPyEmbed_SetArgv(argc, argv);
#endif

	while(1)
	{
		int option_index=0;
		int copt;
		
		static struct option long_options[] = {
			{"help",0,0,0},
			{"version",no_argument,NULL,'v'},
			{"gamegear",no_argument,NULL,'g'},
			{"sms",no_argument,NULL,'m'},
			{"pal",no_argument,NULL,'p'},
			{"japan",no_argument,NULL,'j'},
			{"nosound",no_argument,NULL,'s'},
			{"fullscreen",no_argument,NULL,'f'},
			{"readonly",no_argument,NULL,'r'},
			{"identify",no_argument,NULL,'i'},
			{0,0,0,0}
		};
		
		copt=getopt_long(argc,argv,"vgmpjsfri",long_options,&option_index);
		if(copt==-1) break;
		switch(copt)
		{
			case 0:
				if(strcmp(long_options[option_index].name,"help")==0) usage();
				break;
			
			case 'v':
				printf("%s",VERSION "\n");
				exit(0);
			
			case 'g':
				autodetect=0;
				MastEx |= MX_GG;
				break;
			
			case 'm':
				autodetect=0;
				MastEx &= ~MX_GG;
				break;
			
			case 'p':
				MastEx |= MX_PAL;
				framerate=50;
				break;
			
			case 'j':
				MastEx |= MX_JAPAN;
				break;
			
			case 's':
				sound=0;
				break;

			case 'f':
				vidflags |= SDL_FULLSCREEN;
				break;
				
			case 'r':
				readonly = 1;
				break;

			case 'i':
				identify = 1;
				break;

			case '?':
				usage();
				break;
		}
	}
	
	if(optind==argc)
	{
		fprintf(stderr,APPNAME ": no ROM image specified.\n");
		exit(1);
	}

	atexit(SDL_Quit);

#ifndef FB_RENDER
	if(sound)
		SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO);
	else
		SDL_Init(SDL_INIT_VIDEO);
#else
	SDL_Init(SDL_INIT_AUDIO);
#endif

	MastInit();
	MastLoadRom(argv[optind], &rom, &romlength);
	MastSetRom(rom,romlength);
	if (autodetect)
		MastFlagsFromHeader();
		
	if( identify ) {
		printf("%s\n", MastEx&MX_GG ? "GG" : "SMS" );
		SDL_Quit();
		return 0;	
	}
		
	MastHardReset();
	memset(&MastInput,0,sizeof(MastInput));
	
	if( MastEx&MX_GG ) {
		width = 160;
		height = 144;
   		xoff=64; yoff=24;
	}
	else {
		width = 256;
		height = 192;	
   		xoff=16;
	}
	
#ifndef FB_RENDER
	thescreen=SDL_SetVideoMode(width, height, 15, SDL_SWSURFACE|vidflags);
	if(thescreen==NULL) {
		fprintf(stderr, "Couldn't set video mode: %s\n", SDL_GetError());
		SDL_Quit();
		return -1;
	}
#else
	/* Open the file for reading and writing */
	fbfd = open("/dev/fb1", O_RDWR);
	if (!fbfd) {
		printf("Error: cannot open framebuffer device.\n");
		return -1;
	}

	/* Get fixed screen information */
	if (ioctl(fbfd, FBIOGET_FSCREENINFO, &finfo)) {
		printf("Error reading fixed information.\n");
		return -1;
	}

	/* Get variable screen information */
	if (ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo)) {
		printf("Error reading variable information.\n");
		return -1;
	}

	/* Figure out the size of the screen in bytes */
	screensize = vinfo.xres * vinfo.yres * vinfo.bits_per_pixel / 8;

	/* Map the device to memory */
	fbp1 = (unsigned short *)mmap(0, screensize * 2, PROT_WRITE, MAP_SHARED, fbfd, 0);
	if ((int)fbp1 == -1) {
		printf("Error: failed to map framebuffer device to memory.\n"); 
		return -1;
	}
	/* Pointer to second buffer */
	fbp2 = fbp1 + screensize/2;
	
	/* Select first buffer */
	fbp = fbp1;
	vinfo.yoffset = 0;

	/* Open device to capture keyboard events */
	for( i = 0; 1; i++ ) {
		snprintf( kbdev, 32, "/dev/input/event%i", i );

		kbfd = open( kbdev, O_RDONLY|O_NONBLOCK );
		if( kbfd < 0 ) {
			/* No more devices */
			break;
		}

		ioctl( kbfd, EVIOCGNAME(sizeof(kbdev)), kbdev );
		if( strcmp( kbdev, "gpio-keys" ) == 0 ) {
			/* Found device */
			break;
		}
			
		close( kbfd ); /* we don't need this device */
	}
	
	if( kbfd < 0 ) {
		printf("Error: cannot open keyboard event device.\n");
		return -1;
	}
#endif

	if(sound)
	{
		MsndRate=44100; MsndLen=(MsndRate+(framerate>>1))/framerate; //guess
		aspec.freq=MsndRate;
		aspec.format=AUDIO_S16;
		aspec.channels=2;
		aspec.samples=1024;
		audiobuf=malloc(aspec.samples*aspec.channels*2*64);
		memset(audiobuf,0,aspec.samples*aspec.channels*2);
		aspec.callback=MsndCall;
		pMsndOut=malloc(MsndLen*aspec.channels*2);
		aspec.userdata=audiobuf;
		if(SDL_OpenAudio(&aspec,NULL)) {
			fprintf(stderr, "Couldn't open audio: %s\n", SDL_GetError());
			sound=0;
		}
		SDL_PauseAudio(0);
		MsndInit();
	}
	else
	{
		pMsndOut=NULL;
	}

#ifndef NOPYTHON
	MPyEmbed_Init();
	python = MPyEmbed_Available();
#endif

	MastDrawDo=1;
	while(!done)
	{
		if (!paused || frameadvance)
		{
#ifndef FB_RENDER
			scrlock();
#endif
			MastFrame();
#ifndef FB_RENDER
			scrunlock();
#else
			ioctl(fbfd, FBIOPAN_DISPLAY, &vinfo);

			/* Flip buffers */
			if( fbp == fbp1 ) {
				fbp = fbp2;
				vinfo.yoffset = height;
			}
			else {
				fbp = fbp1;
				vinfo.yoffset = 0;
			}
#endif

#ifndef NOPYTHON
#if 0
			clock_gettime(CLOCK_REALTIME, &t1);
			pydega_cbpostframe(mainstate);
			clock_gettime(CLOCK_REALTIME, &t2);
			printf("postframe took %d ns\n", t2.tv_nsec-t1.tv_nsec);
			MPyEmbed_CBPostFrame();
#endif
#endif

			MastInput[0]&=~0x40;
			if(sound)
			{
				SDL_LockAudio();
				memcpy(audiobuf+audio_len,pMsndOut,MsndLen*aspec.channels*2);
				audio_len+=MsndLen*aspec.channels*2;
				//printf("audio_len %d\n",audio_len);
				SDL_UnlockAudio();
			}
		}
		frameadvance = 0;
#ifndef FB_RENDER
		if (paused)
		{
			SDL_WaitEvent(&event);
			goto Handler;
		}
                while(SDL_PollEvent(&event))
                {
Handler:		switch (event.type)
                        {
                        case SDL_KEYDOWN:
                                key=event.key.keysym.sym;
                                if(key==SDLK_ESCAPE) {done=1;break;}
                                if(key==SDLK_UP) {MastInput[0]|=0x01;break;}
                                if(key==SDLK_DOWN) {MastInput[0]|=0x02;break;}
                                if(key==SDLK_LEFT) {MastInput[0]|=0x04;break;}
                                if(key==SDLK_RIGHT) {MastInput[0]|=0x08;break;}
                                if(key==SDLK_PAGEDOWN) {MastInput[0]|=0x10;break;}
                                if(key==SDLK_END) {MastInput[0]|=0x20;break;}
                                if(key==SDLK_LALT) {
				  MastInput[0]|=0x80;
				  if ((MastEx&MX_GG)==0)
				    MastInput[0]|=0x40;
				  break;}

                                if(key==SDLK_u) {MastInput[1]|=0x01;break;}
                                if(key==SDLK_j) {MastInput[1]|=0x02;break;}
                                if(key==SDLK_h) {MastInput[1]|=0x04;break;}
                                if(key==SDLK_k) {MastInput[1]|=0x08;break;}
                                if(key==SDLK_f) {MastInput[1]|=0x10;break;}
                                if(key==SDLK_g) {MastInput[1]|=0x20;break;}

				if(key==SDLK_p) {paused=!paused;break;}
				if(key==SDLK_o) {paused=1;frameadvance=1;break;}
				if(key==SDLK_r) {HandleRecordMovie(1);break;}
				if(key==SDLK_e) {HandleRecordMovie(0);break;}
				if(key==SDLK_t) {HandlePlaybackMovie();break;}
				if(key==SDLK_w) {MvidStop();break;}
				if(key==SDLK_s) {HandleSaveState();break;}
				if(key==SDLK_l) {HandleLoadState();break;}
				if(key==SDLK_a) {HandleSetAuthor();break;}
#ifndef NOPYTHON
				if(key==SDLK_n) {HandlePython();break;}
				if(key==SDLK_m) {HandlePythonREPL();break;}
				if(key==SDLK_i) {HandlePythonThread();break;}
#endif
				if(key==SDLK_b) {MdrawOsdOptions^=OSD_BUTTONS;break;}
				if(key==SDLK_f) {MdrawOsdOptions^=OSD_FRAMECOUNT;break;}
				if(key==SDLK_EQUALS) {mult++;SetRateMult();break;}
				if(key==SDLK_MINUS) {mult--;SetRateMult();break;}
                                break;
                        case SDL_KEYUP:
                                key=event.key.keysym.sym;
                                if(key==SDLK_ESCAPE) {done=1;break;}
                                if(key==SDLK_UP) {MastInput[0]&=0xfe;break;}
                                if(key==SDLK_DOWN) {MastInput[0]&=0xfd;break;}
                                if(key==SDLK_LEFT) {MastInput[0]&=0xfb;break;}
                                if(key==SDLK_RIGHT) {MastInput[0]&=0xf7;break;}
                                if(key==SDLK_PAGEDOWN) {MastInput[0]&=0xef;break;}
                                if(key==SDLK_END) {MastInput[0]&=0xdf;break;}
                                if(key==SDLK_LALT) {MastInput[0]&=0x3f;break;}

                                if(key==SDLK_u) {MastInput[1]&=0xfe;break;}
                                if(key==SDLK_j) {MastInput[1]&=0xfd;break;}
                                if(key==SDLK_h) {MastInput[1]&=0xfb;break;}
                                if(key==SDLK_k) {MastInput[1]&=0xf7;break;}
                                if(key==SDLK_f) {MastInput[1]&=0xef;break;}
                                if(key==SDLK_g) {MastInput[1]&=0xdf;break;}
                                break;
                        case SDL_QUIT:
                                done = 1;
                                break;
                        default:
                                break;
                        }
                }
#else /* ifndef FB_RENDER */
		kbrd = read(kbfd, ev, sizeof(struct input_event) * 64);

		if (kbrd >= (int) sizeof(struct input_event)) {
			for( i = 0; i < kbrd / sizeof(struct input_event); i++ ) {
				if ( ev[i].type == 1 && !(ev[i].code == MSC_RAW || ev[i].code == MSC_SCAN) ) {
					if( ev[i].value == 0 ) {
						// Key up
						switch( ev[i].code ) {
							case KEY_UP:		MastInput[0]&=0xfe; break;
							case KEY_DOWN:		MastInput[0]&=0xfd; break;
							case KEY_LEFT:		MastInput[0]&=0xfb; break;
							case KEY_RIGHT:		MastInput[0]&=0xf7; break;
							case KEY_PAGEDOWN:	MastInput[0]&=0xef; break;
							case KEY_END:		MastInput[0]&=0xdf; break;
							case KEY_LEFTALT:	MastInput[0]&=0x3f; break;
							case KEY_RIGHTSHIFT:
								done = 1;
								break;
							default:
								break;
						}
					}
					else {
						// Key down
						switch( ev[i].code ) {
							case KEY_UP:		MastInput[0]|=0x01; break;
							case KEY_DOWN:		MastInput[0]|=0x02; break;
							case KEY_LEFT:		MastInput[0]|=0x04; break;
							case KEY_RIGHT:		MastInput[0]|=0x08; break;
							case KEY_PAGEDOWN:	MastInput[0]|=0x10; break;
							case KEY_END:		MastInput[0]|=0x20; break;
							case KEY_LEFTALT:
								MastInput[0]|=0x80;
								if ((MastEx&MX_GG)==0)
									MastInput[0]|=0x40;
								break;
							case KEY_RIGHTSHIFT:
								/* Wait for key up to exit... */
								break;
							default:
								break;
						}
					}
				}	
			}
		}
#endif /* ifndef FB_RENDER */
		if (!paused || frameadvance)
		{
			if(sound) while(audio_len>aspec.samples*aspec.channels*2*4) usleep(5);
		}
#ifdef FB_RENDER
		ioctl(fbfd, FBIO_WAITFORVSYNC, &fbarg);
#endif
	}
#ifndef NOPYTHON
	if (python) {
		MPyEmbed_Fini();
	}
#endif

#ifdef FB_RENDER
	munmap(fbp1, screensize);
	close(fbfd);
#endif

	return 0;
}

void MdrawCall()
{
	int i=0;
	unsigned short *line;

   	if( Mdraw.Line-yoff<0 || Mdraw.Line-yoff>=height )
   		return;

/*	if(Mdraw.Data[0]) printf("MdrawCall called, line %d, first pixel %d\n",Mdraw.Line,Mdraw.Data[0]); */
	if(Mdraw.PalChange)
	{
		Mdraw.PalChange=0;
#define p(x) Mdraw.Pal[x]
		for(i=0;i<0x100;i++)
		{
			themap[i] = ((p(i)&0xf00)>>7) | ((p(i)&0xf0)<<2) | ((p(i)&0xf)<<11);
		}
	}

#ifndef FB_RENDER
   	line = (thescreen->pixels)+(Mdraw.Line-yoff)*thescreen->pitch * vscale;

	for (i=0; i < width; i++) {
		line[i] = themap[Mdraw.Data[xoff+i]];
	}
#else
   	line = (fbp)+((Mdraw.Line-yoff)*width);

	for (i=0; i < width; i++) {
		line[i] = (themap[Mdraw.Data[xoff+i]] << 1 & 0xffe0)
                | (themap[Mdraw.Data[xoff+i]] & 0x003c);

	}
#endif
}

