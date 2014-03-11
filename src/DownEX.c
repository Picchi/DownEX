#include <curl\curl.h>
#include <stdio.h>
#include <stdint.h>
#include <getopt.h>
#include <string.h>
#include <inttypes.h>
#include <signal.h>
#include <wchar.h>
#include <math.h>


struct Mem {
	size_t size;
	char * mem;

};

typedef struct _CString
{
	char C;
	char *S;
} CS;

struct _Tag{
	int n;
	char ** l;
};


typedef struct _ParamGallery
{
	
	char **LinkPage;
	char **proxy;
	int ProxyNum;
	int NumGallery;
	uint8_t Verbose;
	DWORD ThreadId;
	int ThreadNum;
	char ** ExcludeTag;
	int NumExcludeTag;
	uint8_t NumImg;

} ParamGallery;

CS CSTable[]={
{'&', "&amp;"}, 
{'\'',"&#039;"}	 
};

int MaxTry=1;
HANDLE Mutex=NULL;
HANDLE MutexNumGallery=NULL;
uint8_t EndProg=0;
unsigned long SecSleep=0;
unsigned int PageMax=400;
unsigned int MinPage=0;
unsigned int MaxPage=UINT_MAX;
char *cookie=NULL;
int GalleryNum=0;
int NumGallery;

static inline void __attribute__((always_inline)) _ReleaseMutex(HANDLE Mutex)
{
	if (!ReleaseMutex(Mutex)){ 
		printf("Problema rilascio mutex %lu\n",GetLastError());
		exit(1);
	}
}


static inline void __attribute__((always_inline)) _WaitForSingleObject(HANDLE Mutex)
{
	
	WaitForSingleObject(Mutex, INFINITE);
	
	
}


size_t WriteMem ( char *ptr, size_t size, size_t nmemb, void *userdata)
{
	size_t RealSize = size*nmemb;
	struct Mem *P = (struct Mem*)userdata;
	P->mem=realloc(P->mem,P->size+RealSize+1);
	if (P->mem==NULL){
		perror("Error realloc");
		exit(1);
	}
	memcpy(P->mem+P->size,ptr,RealSize);
	P->size=P->size+RealSize;
	return RealSize;
}

size_t WriteImg(char *ptr, size_t size, size_t nmemb, void *userdata)
{
	return fwrite(ptr,size,nmemb,(FILE*)userdata);
}

uint8_t GetLinkReverse ( char *S, char *F, char *L, char **end,char **Link)
{
	char *ptr1,*ptr2;
	size_t diff,len;

	*Link=NULL;
	ptr2=S+strlen(S);
	if (L!=NULL){
		len=strlen(L);
		ptr2=ptr2-len;
		while(ptr2>S && strncmp(L,ptr2,len)!=0){
			ptr2--;
		}
		if (ptr2==S){
			return 1;
		}
	}
	if (F==NULL){
		ptr1=S;
	} else {
		len=strlen(F);
		ptr1=ptr2-len;
		while(ptr1>=S && strncmp(F,ptr1,len)!=0){
			ptr1--;
		}
		if (ptr1==S-1){
			return 2;
		}
		ptr1=ptr1+len;
	}
	diff=ptr2-ptr1;
	*Link=(char* )malloc(diff+1);
	if ( *Link == NULL ){
		perror("Malloc GetLink\n");
		return 3;
	}
	memset(*Link,0,diff+1);
	memcpy(*Link,ptr1,diff);
	if (end !=NULL){
		*end=ptr2;
	}
	return 0;
}


uint8_t GetLink( char * S,  char * F, char * L, char **end,char **Link)

{
	char * ptr1,* ptr2;
	size_t diff;

	*Link=NULL;
	if (F==NULL){
		ptr1=S;
	} else {
		ptr1 = strstr(S,F);
		if (ptr1==NULL){
			return 1;
		}
		ptr1 = ptr1+strlen(F);
	}
	if (L==NULL){
		ptr2=S+strlen(S);
	} else {
		ptr2 = strstr(ptr1,L);
		if (ptr2 ==NULL){
			return 2;
		}
	}
	diff=ptr2-ptr1;
	*Link=(char* )malloc(diff+1);
	if ( *Link == NULL ){
		perror("Malloc GetLink\n");
		return 3;
	}
	memset(*Link,0,diff+1);
	memcpy(*Link,ptr1,diff);
	if (end !=NULL){
		*end=ptr2;
	}
	return 0;

}


void GetTitleNumPage(char *S,char **Title,unsigned int *NumPage)
{
	char * sNumPage;
	GetLink(S,"<title>"," - ExHentai.org",NULL,Title);
	GetLink(S,"Images:</td><td class=\"gdt2\">"," @ ",NULL,&sNumPage);
	*NumPage=atoi(sNumPage);

}

char * GetLinkNextPage(char * S)
{
	char * ret;
	GetLinkReverse(S,"\" href=\"","\"><img src=\"http://st.exhentai.net/img/n.png\"",NULL,&ret);
	return ret;
}


static inline char ConvertCar(char*SC)
{
	int i;
	for (i=0;i<(sizeof(CSTable)/sizeof(CS));i++){
		if (strcmp(SC,CSTable[i].S)==0){
			return CSTable[i].C;
		}
	}
	return ' ';
}




char* ConvertLink( char *LI)
{
	char *L,*ptr1,*ptr2,*ptr3,*EndS;
	char *SuppL;
	char *SC,SS;
	size_t size;
	int ContConv,Cont;
	EndS=LI+strlen(LI);
	ptr1=strchr(LI,'&');
	if (ptr1==NULL){
		return LI;
	}
	size=ptr1-LI;
	ptr3=LI;
	size=0;
	Cont=0;
	ContConv=0;
	L=NULL;
	while(ptr1 != NULL){
		ptr2=strchr(ptr1,';');
		SC=malloc(ptr2-ptr1+2);
		if (SC==NULL){
			perror("Malloc ConvertLink");
			exit(1);
		}
		memcpy(SC,ptr1,ptr2-ptr1+2);
		SC[ptr2-ptr1+1]='\0';
		SS=ConvertCar(SC);
		free(SC);
	Re:
		SuppL=realloc(L,size+ptr1-LI+2);
		if (SuppL==NULL){
			perror("Realloc L ConvertLink");
			Cont++;
			if (Cont>MaxTry){
				return NULL;
			}
			goto Re;
		}
		L=SuppL;
		memcpy(L+size,ptr3,ptr1-LI);
		size=ptr1-LI+1-ContConv*4;
		L[ptr1-LI]=SS;
		L[ptr1-LI+1]='\0';
		ptr1=strchr(ptr2,'&');
		ptr3=ptr2+1;
		ContConv++;
	}
	L=(char*)realloc(L,size+EndS-ptr2);
	memcpy(L+size,ptr2+1,EndS-ptr2);
	free(LI);
	return L;


}


static inline void AddArgv(int * argc,char ***argv,char * Link)
{
	
	argc[0]++;
	*argv=realloc(*argv,(*argc)*sizeof(char**));
	if (*argv==NULL){
		perror("Realloc");
		exit(2);
	}
	(*argv)[(*argc)-1]=Link;

}

void AddPage(char * LinkPage,int * argc, char *** argv,CURL* curl,char * Skip,char * type)
{
	struct Mem MainPage;
	char *ptr1,*ptr2,*Link;
	int i;
	CURLcode res;
	uint8_t SP[25];
	memset(SP,0,sizeof(uint8_t)*25);
	if (Skip!=NULL){
		do{
			ptr1=strchr(Skip,',');
			if (ptr1!=NULL){
				ptr1[0]=0;
			}
			if ((ptr2=strchr(Skip,'-'))==NULL) {
				SP[atoi(Skip)-1]=0x01;
			} else {
				ptr2[0]=0;
				memset(SP+atoi(Skip)-1,0x01,sizeof(uint8_t)*(atoi(ptr2+1)-atoi(Skip)+1)	);
			}
			Skip=ptr1+1;	
		} while(ptr1!=NULL);

	}
	MainPage.mem=NULL;
	MainPage.size=0;

	curl_easy_setopt(curl, CURLOPT_URL, LinkPage);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA , &MainPage);
	res = curl_easy_perform(curl);
	if(res != CURLE_OK){
		fprintf(stderr, "curl_easy_perform() AddPage failed: %s\n",curl_easy_strerror(res));
		return;
	}
	MainPage.mem[MainPage.size]='\0';
	ptr2=MainPage.mem;
	for(i=0; ; i++){
		ptr1=strstr(ptr2,type);
		if (ptr1==NULL){
			return;
		}
		GetLink(ptr1,"href=\"","\"",&ptr2,&Link);
		if (!SP[i]){
			AddArgv(argc,argv,Link);
		}
	}
}

static inline char * ReplaceAll(char* T,char C,char V)
{
	char * ptr;
	ptr=strchr(T,C);
	while(ptr!=NULL){
		*ptr=V;
		ptr=strchr(ptr,C);
	}
	return T;
}

void FixTitle(char* Title)
{
	char R[]={ '\\', '/', '?', '*', ':', '<','>', '"', '|' };
	int i;
	for (i=0;i<9;i++){
		ReplaceAll(Title,R[i],' ');
	}
}


wchar_t* fromUTF8toUTF16 (char * U8)
{
	wchar_t *U16;
	size_t dim=0;;
	size_t dimU8=strlen(U8);
	int ptr=0;
	int Cont=0;
	do {
		U16=malloc(sizeof(wchar_t)*(dim+1));
		Cont++;
	}while (U16 == NULL );
	if (U16 ==NULL){
		printf ("Error malloc fromUTF8toUTF16");
		return NULL;
	}
		
	U16[dim]=0;
	while (ptr<dimU8){
		dim++;
		printf("ptr=%d dim=%d hex=%2X\n",ptr,dimU8,U8[ptr] );

		U16=realloc(U16,sizeof(wchar_t)*(dim+1));
		if (U16==NULL){
			printf("Error realloc fromUTF8toUTF16");
			return NULL;
		}
		U16[dim]='\0';

		if ((U8[ptr]&0x80)==0x00){
			U16[dim-1]=U8[ptr];
			ptr++;
			continue;
		}
		if ((U8[ptr]&0xE0)==0xC0){
			U16[dim-1]=((U8[ptr]&0x1F)<<6)|((U8[ptr+1])&0x3F);
			ptr=ptr+2;
			continue;
		}
		if ((U8[ptr]&0xF0)==0xE0){
			U16[dim-1]=((U8[ptr]&0x0F)<<12)|((U8[ptr+1]&0x3C)<<6)|(((U8[ptr+1]&0x03)<<6)|((U8[ptr+2]&0x3F)));
			ptr=ptr+3;
			continue;
		} 
		printf("Error UTF 8\n");
		free(U16);
		return NULL;
	}
	return	U16;
}



static inline int __attribute__((always_inline)) num_digit(int N)  
{
	return log10(N)+1;
}


int StartImg(int NumPage,char ** P, char *H,char*LinkPage,CURL * curl,struct Mem *Page,
						 wchar_t* WTitle,uint8_t NumImg)
{
	int i,g;
	char Sapp[20],supp[200];
	wchar_t WNameImg[256];
	HANDLE h;
	WIN32_FIND_DATAW d;
	CURLcode res;
	g=num_digit(NumPage);
	for (i=1;i<=NumPage;i++){
		swprintf(WNameImg,L"%s\\%0*d.*",WTitle,g,i);
		h=FindFirstFileW(WNameImg,&d);
		if (h==INVALID_HANDLE_VALUE ){
			sprintf(Sapp,"\"><img alt=\"%0*d",g,i);
			if (i>NumImg){
				sprintf(supp,"%s?p=%d",LinkPage,i/NumImg);
				free(Page->mem);
				Page->mem=NULL;
				Page->size=0;
				curl_easy_setopt(curl,CURLOPT_URL,supp);
				res = curl_easy_perform(curl);
				if(res != CURLE_OK){
					fprintf(stderr, "curl_easy_perform() StartImg failed: %s\n",curl_easy_strerror(res));
					return 1;
				}
				free(H);
				H=malloc(Page->size+1);
				H[Page->size]='\0';	
				memcpy(H,Page->mem,Page->size);		
			}
			GetLinkReverse(H,"a href=\"",Sapp,NULL,P);
			return i;
		}
		FindClose(h);
	}
	return i;	


}

void GetTagGallery(char * Page,struct _Tag *Tag)
{
	char * X;
	char * cont=Page;
	char  *S[]={
			"tagmenu('language:",
			"tagmenu('parody:",
			"tagmenu('character:",
			"tagmenu('group:",
			"tagmenu('artist:",		
			"tagmenu('male:",		
			"tagmenu('female:",
			"tagmenu('misc:"		
	};
	int k=0;
	for (k=0;k<8;k++){
		while (!GetLink(cont,S[k],"',",&cont,&X)){
			AddArgv(&(Tag[k].n),&(Tag[k].l),X);
		}
	}
}

char * ExcludeInTag(char * Exclude,struct _Tag *tag)
{
	int i=0,j=0;
	char *r;
	for (i=0;i<10;i++){
		for (j=0;j<tag[i].n;j++){
			if ((r=strstr(tag[i].l[j],Exclude))){
				return r;
			}
		}
	}
	return NULL;
}

void  GetGallery(struct Mem HP,char *LinkPage ,CURL * curl,
				char ** proxy,int ProxyNum,unsigned int *PageNow,uint8_t Verbose,
				DWORD ThreadId,int ThreadNum,char ** ExcludeTag,int NumExcludeTag,uint8_t NumImg)
{
	char *P=NULL,*LinkImg,*LinkNextPage;
	wchar_t WNameImg[256];
	char *Title,*End;
	wchar_t *WTitle;
	wchar_t *Ext;
	CURL *CurlGal;
	CURLcode res;
	FILE * FileImg;
	struct _Tag Tag[10];
	int g=1;
	struct Mem Page;
	unsigned int NumPage,i;
	int Cont;
	CurlGal = curl_easy_init();
	if(!CurlGal) {
		printf("Error\n");
		return ;
	}
	End=NULL;
	memset(Tag,0,sizeof(Tag));
	curl_easy_setopt(curl, CURLOPT_WRITEDATA , &Page);
	curl_easy_setopt(CurlGal, CURLOPT_WRITEFUNCTION ,&WriteImg);
	Page.mem=NULL;
	GetTitleNumPage(HP.mem,&Title,&NumPage);
	if (Title==NULL || NumPage == 0){
		printf("Error GetTitleNumPage\n");
		printf("%s\n",HP.mem );
		return ;
	}
	printf("start thread %d\n",ThreadNum );
	_WaitForSingleObject(MutexNumGallery);
	GalleryNum++;
	printf("%d/%d\n",GalleryNum,NumGallery );
	_ReleaseMutex(MutexNumGallery);
	printf("Title:%s\nNum Page:%u\n",Title,NumPage);
	if (NumPage<MinPage||NumPage>MaxPage){
		printf("%s Min/Max Page Exit\n",Title);
		free(Title);
		Sleep(500);
		return ;
	}
	if (ExcludeTag){
		GetTagGallery(HP.mem,Tag);
		for (i=0;i<NumExcludeTag;i++){
			if ((P=ExcludeInTag(ExcludeTag[i],Tag))){
				printf("%s Exclude tag %s\n",Title,P);
				free(Title);
				Sleep(500);
				return ;
			}
		}
	}
	Title=ConvertLink(Title);
	FixTitle(Title);
	Cont =0;
	do{
		WTitle=fromUTF8toUTF16(Title);
		Cont++;
	}while(WTitle == NULL || Cont < MaxTry);
	if (WTitle == NULL ){
		printf("Error WTitle Convert");
		return ;
	}
	if (!CreateDirectoryW(WTitle,NULL)){
		if (GetLastError()!=183){
			printf("CreateDirectory %ld\n",GetLastError());
			return ;
		}
	}
	g=num_digit(NumPage);
	

	for (i=StartImg(NumPage,&P,HP.mem,LinkPage,curl,&Page,WTitle,NumImg);i<=NumPage;i++){
		_WaitForSingleObject(Mutex);
		if (EndProg==1){
			_ReleaseMutex(Mutex);
			break;
		}
		_ReleaseMutex(Mutex);
		if ((*PageNow)==PageMax){
			Sleep(60000);
			(*PageNow)=0;
		} else {
			(*PageNow)++;
		}
		Cont=0;
		free(Page.mem);
	try:
		Page.mem=(char*)malloc(1);
		
		Cont++;
		if (Page.mem==NULL){
			perror("Mealloc Page.mem");
			if (Cont>MaxTry){
				return ;
			}
			goto try;
		}
		Page.size=0;
		curl_easy_setopt(curl,CURLOPT_URL,P);
		curl_easy_setopt(curl,CURLOPT_PROXY,proxy[i%ProxyNum]);
		curl_easy_setopt(CurlGal,CURLOPT_PROXY,proxy[i%ProxyNum]);
		Cont=0;
		do {
			res = curl_easy_perform(curl);
			if(res != CURLE_OK){
				fprintf(stderr, "thread %d curl_easy_perform() 2 failed: %s\n",
					    ThreadNum,curl_easy_strerror(res));
				Cont++;
				if (Cont>MaxTry){
					return ;
				}
			}else{
				break;
			}
		}while(TRUE);
		Page.mem[Page.size]='\0';
		if (GetLink(Page.mem,"><img id=\"img\" src=\"","\" style=",&End,&LinkImg)){
			printf("Error GetLink LinkImg\n");
			goto cont3;
		}
		LinkImg=ConvertLink(LinkImg);
		if (LinkImg==NULL){
			printf("Error ConvertLink");
			goto cont3;
		}
		Ext=fromUTF8toUTF16(strrchr(LinkImg,'.'));
		Cont=swprintf(WNameImg,L"%s\\%0*d%s",WTitle,g,i,Ext);
		free(Ext);
		if (Cont<0){
			printf("Error sprintf\n");
			continue;
		}	
		if ((FileImg=_wfopen(WNameImg,L"r"))){
			printf("%d Exist ",i);
			goto cont2;
		}
		if ((FileImg=_wfopen(WNameImg,L"wb"))==NULL){
			perror("fopen");
			goto cont3;
		}
		curl_easy_setopt(CurlGal,CURLOPT_URL,LinkImg);
		curl_easy_setopt(CurlGal, CURLOPT_WRITEDATA , FileImg);
		Cont=0;
	try2:
		res = curl_easy_perform(CurlGal);
		if(res != CURLE_OK){
			fprintf(stderr, "thread %d curl_easy_perform() 3 failed: %s\n",
				    ThreadNum,curl_easy_strerror(res));
			Cont++;
			if (Cont>MaxTry){
				printf("Skip Page:%d\nLinkImg: %s\n",i,LinkImg);
				goto cont2;
			} else {
				goto try2;
			}
		}
		if (Verbose==1){
			printf("Save %s %d \n",Title,i);
		}
	cont2:
		fclose(FileImg);
		if (res != CURLE_OK){
			DeleteFileW(WNameImg);
		}
	cont3:
		LinkNextPage=GetLinkNextPage(End);
		free(P);
		P=LinkNextPage;
		free(LinkImg);
		Sleep(SecSleep);
	}
	printf("End thread %d %s\n",ThreadNum,Title);
	if (P==NULL){
		Sleep(1000);
	}
	free(Title);
	free(Page.mem);
	free(P);
	free(WTitle);
	curl_easy_cleanup(CurlGal);
}

DWORD WINAPI StartGallery(void *para)
{
	ParamGallery * par= (ParamGallery *)para;
	int i;
	unsigned int PageNow=0;
	CURL *curl;
	CURLcode res;
	struct Mem HP;
	curl = curl_easy_init();
	if(!curl) {
		printf("Error\n");
		return 1;
	}
	curl_easy_setopt(curl, CURLOPT_COOKIEFILE,cookie);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION ,&WriteMem);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
	
	for (i=0;i<par->NumGallery;i++){
		_WaitForSingleObject(Mutex);
		if (EndProg){
			_ReleaseMutex(Mutex);
			break;
		}
		_ReleaseMutex(Mutex);
		curl_easy_setopt(curl, CURLOPT_PROXY,(par->proxy)[i%(par->ProxyNum)]);
		curl_easy_setopt(curl, CURLOPT_URL, (par->LinkPage)[i]);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA , &(HP));
		res = curl_easy_perform(curl);
		if(res != CURLE_OK){
			fprintf(stderr, "thread %d curl_easy_perform() 1 failed: %s\n",
				    par->ThreadNum, curl_easy_strerror(res));
			printf("Skip %s\n", (par->LinkPage)[i]);
			continue;
		}
		HP.mem[HP.size]='\0';
		GetGallery(HP,(par->LinkPage)[i],curl,par->proxy,par->ProxyNum,
				   &PageNow,par->Verbose,par->ThreadId,par->ThreadNum,
				   par->ExcludeTag,par->NumExcludeTag,par->NumImg);
		HP.mem=(char*)realloc(HP.mem,1);
		HP.size=0;
	}
	curl_easy_cleanup(curl);
	return 0;
}





void LoadLinkFromFile (int* argc,char ***Cargv,char * Nfile)
{
	FILE * File=fopen(Nfile,"r");
	char * Link=NULL;
	char * Link2;
	Link=malloc(100);
	while (!feof(File)){
		fscanf(File,"%s",Link);
		Link2=malloc(strlen(Link)+1);
		memcpy(Link2,Link,strlen(Link)+1);
		AddArgv(argc,Cargv,Link2);
	}
	free(Link);
	fclose(File);
}

char * NextPage(char * LinkPage)
{
	char *Next=NULL,*ptr;
	int page=0,l=0;
	int diff=0;
	ptr=strstr(LinkPage,"/?page");
	if (ptr!=NULL){
		page=strtol(ptr+7,NULL,10);
		if (page==9||page==99){
			l=1;
		}
	} else {
		ptr=strstr(LinkPage,"/tag/");
		if (ptr==NULL){
			l=7;
			ptr=strstr(LinkPage,".org")+4;
			*(ptr+1)='&';
		} else {
			ptr=LinkPage+strlen(LinkPage);
			while(atoi(ptr-1)!=0 || atoi(ptr-2)!=0){
				ptr--;
			}
			if (ptr-LinkPage==strlen(LinkPage)){
				page=1;
				l=2;
				diff=0;
			} else {
				page=atoi(ptr)+1;
				diff=2;
				if (page==9||page==99){
					l=1;
				}
				if (page>9){
					diff++;
				}

			}
			Next=malloc(sizeof(char)*(strlen(LinkPage)+l));
			memcpy(Next,LinkPage,strlen(LinkPage)-diff);
			sprintf(Next+strlen(LinkPage)-diff,"/%d",page);
			free(LinkPage);
			return Next;
		}
	}
	Next=malloc(sizeof(char)*(strlen(LinkPage)+2+l));
	diff=ptr-LinkPage;
	memcpy(Next,LinkPage,diff);
	diff += sprintf(Next+diff,"/?page=%d",page+1);
	strcat(Next+diff,LinkPage+diff-l);
	free(LinkPage);
	return Next;


}




char *GenerateSearchLink(char *Search,char *Tag,int Page)
{
	char *ret=malloc(sizeof(char)*300);
	memset(ret,0,sizeof(char)*300);
	sprintf(ret,"http://exhentai.org/?page=%d&f_doujinshi=%c&f_manga=%c&f_artistcg=%c&f_gamecg=%c&f_western=%c&f_non-h=%c&f_imageset=%c&f_cosplay=%c&f_asianporn=%c&f_misc=%c&f_search=%s&f_apply=Apply+Filter*/",
		Page-1,Tag[0],Tag[1],Tag[2],Tag[3],Tag[4],Tag[5],Tag[6],Tag[7],Tag[8],Tag[9],Search);
	return ret;
}

char * CreateTag(char * s)
{
	char *ret,*ptr;
	if(s[0]=='1'||s[0]=='0'){
		return s;
	}
	ret=malloc(sizeof(char)*11);
	ret[10]='\0';
	if (!strcmp(s,"all")){
		memset(ret,'1',sizeof(char)*10);
		return ret;
	} else {
		memset(ret,'0',sizeof(char)*10);
	}
	do{
		ptr=strchr(s,',');
		if (ptr!=NULL){
			*ptr='\0';
		}
		switch (s[0]){
			case 'd':
				ret[0]='1';
				break;
			case 'm':
				if(s[1]=='\0'||s[1]=='a'||s[1]==','){
					ret[1]='1';
				} else {
					ret[9]='1';
				}
				break;
			case 'a':
				if (s[1]=='s'){
					ret[8]='1';
				} else {
					ret[2]='1';
				}
				break;
			case 'g':
				ret[3]='1';
				break;
			case 'w':
				ret[4]='1';
				break;
			case 'n':
				ret[5]='1';
				break;
			case 'i':
				ret[6]='1';
				break;
			case 'c':
				ret[7]='1';
				break;
		}
		s=ptr+1;
	}while (ptr!=NULL);
	return ret;

}

void AddFromFile(char * NomeFile,char *** arr,int * Num,uint8_t err)
{
	FILE * f;
	char *s,*s2;
	if ((f=fopen(NomeFile,"r"))==NULL){
		if (err){
			perror("Fopen NomeFile");
		}
		return ;
	}
	s=malloc(sizeof(char)*200);
	if (s==NULL){
		perror("malloc");
		exit(1);
	}
	while (1){
		if (fscanf(f,"%s",s)==EOF){
			break;
		}
		s2=malloc(strlen(s)+1);
		if (s2==NULL){
			perror("malloc");
			exit(1);
		}
		memcpy(s2,s,strlen(s)+1);
		s2[strlen(s)]='\0';
		AddArgv(Num,arr,s2);
	}
	free(s);
	fclose(f);
}




ParamGallery * SepareteGallery(int NumThread,int optind,int argc,char **Cargv,
							   char **proxy,int NumProxy,uint8_t Verbose,
							   char ** ExcludeTag,int NumExcludeTag,char * Type)
{
	ParamGallery *ret;
	int i=0;
	int k,kk;
	int NumGallery=argc-optind;
	if ((k=NumProxy/NumThread)<(double)NumProxy/NumThread){
		k++;
	} 
	if ((kk=(NumGallery)/NumThread)<(double)(NumGallery)/NumThread){
		kk++;
	} 
	ret=malloc(sizeof(ParamGallery)*NumThread);
	if (ret==NULL){
		perror("Malloc Param");
		exit(2);
	}
	for (i=0;i<NumThread;i++){
		ret[i].NumGallery=0;
		ret[i].ProxyNum=0;
		ret[i].proxy=malloc(sizeof(char*)*k);
		ret[i].LinkPage=malloc(sizeof(char*)*kk);
		ret[i].Verbose= (Verbose & 2) && ((Verbose & 1) || (NumThread>1 ? 0 : 1));
		ret[i].ThreadNum=i;
		ret[i].ExcludeTag=ExcludeTag;
		ret[i].NumExcludeTag=NumExcludeTag;
		ret[i].NumImg=atoi(Type)*20;
	}
	for (i=0;i<NumProxy;i++){
		(ret[i%NumThread].proxy)[(ret[i%NumThread].ProxyNum)++]=proxy[i];
	}
	for (i=0;i<NumGallery;i++){
		(ret[i%NumThread].LinkPage)[(ret[i%NumThread].NumGallery)++]=Cargv[optind+i];
	}
	return ret;
}


void SepareTag(char *** ex,char *s,int *n)
{
	char * ptr;
	do{
		ptr=strchr(s,',');
		if (ptr!=NULL){
			*ptr='\0';
		}
		AddArgv(n,ex,s);
		s=ptr+1;
	}while (ptr!=NULL);
}

char * GetType(char* cookie)
{
	FILE *f;
	char s[200],*ptr,*s2;

	if ((f=fopen(cookie,"r"))==NULL){
		perror("Fopen NomeFile");
		return NULL;
	}
	s2=malloc(sizeof(char)*6);
	if (s2==NULL){
		perror("Malloc");
		exit(1);
	}
	memset(s2,0,sizeof(char)*6);
	while (1){
		fscanf(f,"%s",s);
		if ((ptr=strstr(s,"uconfig"))){
			fscanf(f,"%s",s);
			fclose(f);
			s2[0]='i';
			ptr=strstr(s,"dm_")+3;
			if (*ptr=='t'){
				s2[1]='d';
				s2[2]='2';
			} else {
				s2[1]='t';
				s2[2]='5';
			}
			ptr=strstr(s,"tr_")+3;
			s2[4]=*ptr;
			return s2;
		}
	}
	return NULL;
}


void SigInt ( int sig)
{
	printf("Closing ");
	_WaitForSingleObject(Mutex);
	printf("Closing\n");
	EndProg=1;
	_ReleaseMutex(Mutex);
}

int main (int argc,char **argv)
{
	CURL *curl;
	unsigned int i;
	int opt,optindex;
	char **LinkPage=NULL;
	char *LP=NULL;
	int numLinkPage=0;
	char **Cargv=NULL;
	char *DefaultFile=NULL;
	char *Dir=NULL;
	char *Skip=NULL;
	char *Search=NULL;
	char *Pname;
	char *sk=NULL;
	char *Tag="1100000000";
	int *Rec=NULL;
	int r=0;
	int Page=1;
	char *ProxyFile=NULL,**proxy=NULL;
	int ProxyNum=1;
	uint8_t Verbose=2;
	char *Type=NULL;
	HANDLE * hThread=NULL;
	int Cargc;
	int NumThread=1;
	char ** ExcludeTag=NULL;
	int NumExcludeTag=0;
	ParamGallery *param;	
	int j=1;
	struct option long_opt[]={
		{"page",optional_argument,NULL,'p'},
		{"directory",required_argument,NULL,'d'},
		{"cookie",required_argument,NULL,'c'},
		{"max-try",required_argument,NULL,'m'},
		{"sleep",required_argument,NULL,'l'},
		{"skip",required_argument,NULL,'k'},
		{"file",required_argument,NULL,'f'},
		{"min-page",required_argument,NULL,'n'},
		{"help",no_argument,NULL,'h'},
		{"recursive",optional_argument,NULL,'r'},
		{"search",required_argument,NULL,'s'},
		{"tag",required_argument,NULL,'t'},
		{"start-page",required_argument,NULL,'a'},
		{"proxy",optional_argument,NULL,'P'},
		{"multi-thread",required_argument,NULL,'T'},
		{"verbose",no_argument,NULL,'v'},
		{"only-proxy",no_argument,NULL,'o'},
		{"no-verbose",no_argument,NULL,'V'},
		{"exclude",required_argument,NULL,'x'},
		{"max-page",required_argument,NULL,'N'}

	};
	Cargc=argc;
	if (curl_global_init(CURL_GLOBAL_ALL)){
		printf("Global init\n");
		return 1;
	}

	if ((Mutex=CreateMutex(NULL,FALSE,NULL))==NULL){
		printf("Problema creazione mutex %lu\n",GetLastError());
		exit(1);
	}

	if ((MutexNumGallery=CreateMutex(NULL,FALSE,NULL))==NULL){
		printf("Problema creazione mutex %lu\n",GetLastError());
		exit(1);
	}

	cookie=malloc(sizeof(char)*100);
	if (cookie==NULL){
		perror("Malloc cookie");
		return 1;
	}
	DefaultFile=malloc(sizeof(char)*100);
	if (cookie==NULL){
		perror("Malloc DefaultFile");
		return 1;
	}
	memset(cookie,0,sizeof(char)*100);
	memset(DefaultFile,0,sizeof(char)*100);
	GetModuleFileName(NULL,cookie, (sizeof(char))*100); 
	GetModuleFileName(NULL,DefaultFile, (sizeof(char))*100); 
	if (strrchr(argv[0],'\\')!=NULL){
		 GetLinkReverse(argv[0],"\\",NULL,NULL,&Pname);
	} else {
		Pname= argv[0];
	}
	memset(&(cookie[strlen(cookie)-strlen(Pname)]),0,strlen(Pname)+1);
	strcat(cookie,"cookie");
	memset(&(DefaultFile[strlen(DefaultFile)-strlen(Pname)]),0,strlen(Pname)+1);
	strcat(DefaultFile,"default");
	proxy=malloc(sizeof(char*));
	*proxy=malloc(sizeof(char)*5);
	memset(*proxy,0,sizeof(char)*5);
	strcat(*proxy,"");
	Cargv=malloc(sizeof(char*));
	memset(Cargv,0,sizeof(char*));
	Cargv[0]=argv[0];
	AddFromFile(DefaultFile,&Cargv,&j,0);
	j--;
	Cargv=realloc(Cargv,sizeof(char*)*(j+Cargc));
	if (Cargv==NULL){
		perror("Malloc Carv");
		return 1;
	}
	for (i = 1; i < Cargc; ++i){
		Cargv[i+j]=argv[i];
	}
	Cargc=Cargc+j;
	while ((opt=getopt_long(Cargc,Cargv,
		               "c:m:p::l:f:d:s:n:hr::k:a:t:P::T:voVx:N:",
		               long_opt,&optindex))!=-1){
		switch(opt) {
		case 'x':
			SepareTag(&ExcludeTag,optarg,&NumExcludeTag);
			break;
		case 'V':
			Verbose=0;
			break;
		case 'o':
			ProxyNum=0;
			break;
		case 'v':
			Verbose++;
			break;
		case 'T':
			NumThread=atoi(optarg);
			break;
		case 'c':
			free(cookie);
			cookie=optarg;
			break;
		case 'm':
			MaxTry=atoi(optarg);
			break;
		case 'p':
			if (optarg!=NULL) {
				AddArgv(&numLinkPage,&LinkPage,optarg);
			}else{
				LP=malloc(sizeof(char)*50);
				if (LP==NULL){
					perror("Malloc LinkPage");
					return 1;
				}
				sprintf(LP,"http://exhentai.org/?page=%d",Page-1);
				AddArgv(&numLinkPage,&LinkPage,LP);
				Rec=realloc(Rec,sizeof(int)*numLinkPage);
				if (r==0){
					r++;
				}
				Rec[numLinkPage-1]=r;
			}
			break;
		case 'l':
			SecSleep=1000*strtod(optarg,NULL);
			break;
		case 'k':
			Skip=optarg;
			break;
		case 'f':
			LoadLinkFromFile(&argc,&Cargv,optarg);
			break;
		case 'd':
			Dir=optarg;
			break;
		case 'h':
			printf("--page -p \n--skip -k \n--minpage -n\n--directory -d\n--sleep -l\n--max-try -m \n--search -s\n");
			printf("--tag -t doujinshi,manga,artistcg,gamecg,wes,nonh,imageset,cosplay,asianporn,misc\n--start-page -a");
			break;
		case 'n':
			MinPage=atoi(optarg);
			break;
		case 'N':
			MaxPage=atoi(optarg);
			break;
		case 'r':
			if (optarg==NULL){
				r=3;
			} else {
				r=atoi(optarg);
			}
			break;
		case 's':
			Search=ReplaceAll(optarg,' ','+');		
			LP=GenerateSearchLink(Search,Tag,Page);
			AddArgv(&numLinkPage,&LinkPage,LP);
			Rec=realloc(Rec,sizeof(int)*numLinkPage);
			if (Rec==NULL){
				perror("Realloc switch");
				exit(1);
			}
			if (r==0){
				r++;
			}
			Rec[numLinkPage-1]=r;
			break;
		case 't':
			Tag=CreateTag(optarg);
			if (Search==NULL){
				Search="\0";
			}
			break;
		case 'a':
			Page=atoi(optarg);
			break;
		case 'P':
			ProxyFile=optarg;
			break;
		default :
			return 1;
		}

	}
	Type=GetType(cookie);
	if (ProxyFile!=NULL){
		AddFromFile(ProxyFile,&proxy,&ProxyNum,1);
	}
	if (Dir!=NULL){
		SetCurrentDirectory(Dir);
	}
	if (LinkPage){
		curl = curl_easy_init();

		if(!curl) {
			printf("Error\n");
			return 1;
		}
		curl_easy_setopt(curl, CURLOPT_COOKIEFILE,cookie);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION ,&WriteMem);
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
		curl_easy_setopt(curl,CURLOPT_PROXY,proxy[0]);
		for (j=0;j<numLinkPage;j++){
			for (i=0;i<Rec[j];i++){


				printf("Add Page: %s\n",LinkPage[j]);
				if (Skip!=NULL){
					sk=strchr(Skip,'/');
					if (sk!=NULL){
						*sk='\0';
					} 
				}
				AddPage(LinkPage[j],&Cargc,&Cargv,curl,Skip,Type);
				if (sk!=NULL){
					Skip=sk+1;
				}else {
					Skip=NULL;
				}
				LinkPage[j]=NextPage(LinkPage[j]);
				Sleep(3000);
			}
			
			free(LinkPage[j]);
		}
		curl_easy_cleanup(curl);
	}
	if (ProxyNum<NumThread){
		NumThread=ProxyNum;
	}
	if (Cargc-optind<NumThread){
		NumThread=Cargc-optind;
	}
	if (NumThread==0){
		return 0;
	}
	NumGallery=Cargc-optind;
	param=SepareteGallery(NumThread,optind,Cargc,Cargv,proxy,ProxyNum,Verbose,ExcludeTag,NumExcludeTag,Type+4);
	
	hThread=malloc(sizeof(HANDLE)*NumThread);
	signal(SIGINT,SigInt);
	for (i=0;i<NumThread;i++){
		hThread[i]=CreateThread(NULL,0,StartGallery,&(param[i]),0,&(param[i].ThreadId));
		if (hThread[i]==NULL){
			printf("Problema CreateThread %lu\n",GetLastError());
		}
		
	}
	if (WaitForMultipleObjects(NumThread,hThread,TRUE,INFINITE) == WAIT_FAILED ){
		printf("Wait error: %lu\n", GetLastError()); 
	}

	curl_global_cleanup();

	return 0;
}
