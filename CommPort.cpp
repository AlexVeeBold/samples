///////////////////////////////////////////////////////////////
//
// Communication Port class source code
//
//
//   29.05.2008 20:30 - created
//

#include "CommPort.h"



//#define DBG

#ifdef DBG
#define dbg(x)          \
    printf(x);          \
    printf("\r\n")
#define dbgdec(var)     \
    printf(#var " = %d\r\n", var)
#define dbghex(var)     \
    printf(#var " = %08X\r\n", var)
#define dbgs(x)     printf(x)
#else // DBG
#define dbg(x)
#define dbgdec(var)
#define dbghex(var)
#define dbgs(x)
#endif // DBG

//disable trouble funcs
#ifndef DBG
//#define printf   stop build_process
//#undef OutputDebugString
//#define OutputDebugString   stop build_process
#endif

CommPort::CommPort()
{
    // quick-n-dirty way to fill all data members with zero
    memset(this, 0, sizeof(CommPort));
}

CommPort::~CommPort()
{
    if(this->hPort != NULL)
    {
        this->Close();
    }
}

BOOL CommPort::Open(BYTE nPortNumber)
{
    char szName[8];

    if((nPortNumber == 0) || (nPortNumber > 32))
        return FALSE;

    // create events
    this->ovRead.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if(this->ovRead.hEvent == NULL)
    {
        return FALSE;
    }
    this->ovWrite.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if(this->ovWrite.hEvent == NULL)
    {
        CloseHandle(this->ovRead.hEvent);
        return FALSE;
    }

    // open COMM device
    sprintf(szName, "COM%d", nPortNumber);
    this->hPort = CreateFile(szName, GENERIC_READ | GENERIC_WRITE, 0, NULL, 
                            OPEN_EXISTING, 0, NULL);

    if(this->hPort == INVALID_HANDLE_VALUE)
    {
        CloseHandle(this->ovWrite.hEvent);
        CloseHandle(this->ovRead.hEvent);
        this->hPort = NULL;
        return FALSE;
    }

#ifdef DBG
    char szdbg[64];
    sprintf(szdbg, "hPort=%08X\r\n", this->hPort);
    dbgs(szdbg);
#endif // DBG

    // try to clear Comm buffers and queues
//    PurgeComm(this->hPort, PURGE_RXCLEAR | PURGE_TXCLEAR | PURGE_RXABORT | PURGE_TXABORT);

    // save default timeouts
    GetCommTimeouts(this->hPort, &this->ctOld);
    // set wait timeouts
    memset(&this->ct, 0, sizeof(COMMTIMEOUTS));
    SetCommTimeouts(this->hPort, &this->ct);

    GetCommState(this->hPort, &this->dcb);

    this->dcb.XonChar = ASCII_XON;
    this->dcb.XoffChar = ASCII_XOFF;
    this->dcb.XonLim = 100;
    this->dcb.XoffLim = 100;

    this->dcb.fBinary = TRUE;
    this->dcb.fParity = TRUE;

    this->Setup(CBR_9600, 8, NOPARITY, ONESTOPBIT, TRUE, FALSE, TRUE);

    // flush read queue
    this->FlushReadQueue();

    return TRUE;
}

BOOL CommPort::Setup(DWORD dwBaudRate, BYTE bByteSize, BYTE bParity, BYTE bStopBits, 
                BOOL bDtrControl, BOOL bRtsControl, BOOL bXonControl)
{
    if(this->hPort == NULL)
        return FALSE;

    this->dcb.BaudRate = dwBaudRate;
    this->dcb.ByteSize = bByteSize;
    this->dcb.Parity = bParity;
    this->dcb.StopBits = bStopBits;

    // setup hardware flow control

    this->dcb.fOutxDsrFlow = bDtrControl;
    if(bDtrControl != FALSE)
        this->dcb.fDtrControl = DTR_CONTROL_HANDSHAKE;
    else
        this->dcb.fDtrControl = DTR_CONTROL_ENABLE;

    this->dcb.fOutxCtsFlow = bRtsControl;
    if(bRtsControl != FALSE)
        this->dcb.fRtsControl = RTS_CONTROL_HANDSHAKE;
    else
        this->dcb.fRtsControl = RTS_CONTROL_ENABLE;

    // setup software flow control

    this->dcb.fInX = bXonControl;
    this->dcb.fOutX = bXonControl;

    return SetCommState(this->hPort, &this->dcb);
}

BOOL CommPort::Close(void)
{
    if(this->hPort == NULL)
        return FALSE;


    // try to clear Comm buffers and queues
    PurgeComm(this->hPort, PURGE_RXCLEAR | PURGE_TXCLEAR | PURGE_RXABORT | PURGE_TXABORT);
    
    // restore default timeouts
    SetCommTimeouts(this->hPort, &this->ctOld);

    CloseHandle(this->hPort);
    this->hPort = NULL;
    CloseHandle(this->ovWrite.hEvent);
    CloseHandle(this->ovRead.hEvent);

    return TRUE;
}

BOOL CommPort::SetDtr(BOOL bDtr)
{
    DWORD dwParam;

    if(this->hPort == NULL)
        return FALSE;

    dwParam = SETDTR;
    if(bDtr == FALSE)
        dwParam = CLRDTR;
    return EscapeCommFunction(this->hPort, dwParam);
}

BOOL CommPort::SetRts(BOOL bRts)
{
    DWORD dwParam;

    if(this->hPort == NULL)
        return FALSE;

    dwParam = SETRTS;
    if(bRts == FALSE)
        dwParam = CLRRTS;
    return EscapeCommFunction(this->hPort, dwParam);
}

BOOL CommPort::SetXon(BOOL bXon)
{
    DWORD dwParam;

    if(this->hPort == NULL)
        return FALSE;

    dwParam = SETXON;
    if(bXon == FALSE)
        dwParam = SETXOFF;
    return EscapeCommFunction(this->hPort, dwParam);
}

BOOL CommPort::GetDCB(DCB* pDcb)
{
    if((this->hPort == NULL) || (pDcb == NULL))
        return FALSE;

    *pDcb = this->dcb;
    return TRUE;
}

BOOL CommPort::SetDCB(DCB* pDcb)
{
    if((this->hPort == NULL) || (pDcb == NULL))
        return FALSE;

    this->dcb = *pDcb;
    return SetCommState(this->hPort, &this->dcb);
}

char* CommPort::GetATRawResponse(void)
{
    if(this->hPort == NULL)
        return NULL;

    return (char*)this->szATBuffer;
}

MDMRESULT CommPort::GetModemResponse(const char* pBlock)
{
    const char* ptr;

    ptr = pBlock;
    if((*ptr == 'a') || (*ptr == 'A'))
    {
        if((*(ptr+1) == 't') || (*(ptr+1) == 'T'))
        {
            // 'AT' command found, skip it
            while((*ptr != '\0') && (*ptr != '\r') && (*ptr != '\n'))
                ptr++;  // skip any non-CR's and non-LF's (find end of the line)
        }
    }
    while((*ptr != '\0') && ((*ptr == '\r') || (*ptr == '\n')))
        ptr++;  // skip any CR's and LF's (find next line)
    if(*ptr != '\0')
    {
        if(strnicmp(ptr, "OK", 2) == 0)
            return MR_OK;
        if(strnicmp(ptr, "ERROR", 5) == 0)
            return MR_ERROR;
        if(strnicmp(ptr, "CONNECT", 7) == 0)
            return MR_CONNECT;
        if(strnicmp(ptr, "RING", 4) == 0)
            return MR_RING;
        if(strnicmp(ptr, "BUSY", 4) == 0)
            return MR_BUSY;
        if(strnicmp(ptr, "NO ANSWER", 9) == 0)
            return MR_NO_ANSWER;
        if(strnicmp(ptr, "NO CARRIER", 10) == 0)
            return MR_NO_CARRIER;
        if(strnicmp(ptr, "NO DIALTONE", 11) == 0)
            return MR_NO_DIALTONE;
        if(strnicmp(ptr, "NO DIAL TONE", 12) == 0)
            return MR_NO_DIALTONE;
        // unrecognized
#ifdef DBG
    char szdbg[64];
    sprintf(szdbg, "CommPort::GetModemResponse: unknown: %.24s\r\n", ptr);
    dbgs(szdbg);
#endif // DBG
        return MR_UNKNOWN;
    }
    return MR_NONE;
}

DWORD CommPort::PortWaitRecv(DWORD dwNumBytes, DWORD dwTimeout)
{
    DWORD dwNumRecv;
    DWORD dwCommErrors;
    DWORD dwTimeStep;   // wait time slice (ms)
    int nTimeout;
    COMSTAT cs;

    if(this->hPort == NULL)
        return 0;

    // non-WaitCommEvent version
    if(ClearCommError(this->hPort, &dwCommErrors, &cs) == FALSE)
        return 0;   // check comm error flags?
    if(dwNumBytes == 0)
        dwNumBytes = 1;
    nTimeout = dwTimeout;
    dwTimeStep = 100; //20;
    dwNumRecv = cs.cbInQue;
#ifdef DBG
    char szdbg[64];
    sprintf(szdbg, "CommPort::PortWaitRecv: got %d b , need %d b , %d ms\r\n", dwNumRecv, dwNumBytes, dwTimeout);
    dbgs(szdbg);
#endif // DBG
    if(dwNumBytes > dwNumRecv)
    {
        while(nTimeout > 0)
        {
            dbgs("CommPort::PortWaitRecv: loop: ");
            dbgdec(nTimeout);
            nTimeout -= dwTimeStep;
            Sleep(dwTimeStep);      // use some another wait func?
            if(ClearCommError(this->hPort, &dwCommErrors, &cs) == FALSE)
                return 0;
            dwNumRecv = cs.cbInQue;
#ifdef DBG
    sprintf(szdbg, "CommPort::PortWaitRecv: got %d from %d bytes\r\n", dwNumRecv, dwNumBytes);
    dbgs(szdbg);
#endif // DBG
            if(dwNumBytes <= dwNumRecv)
            {
                dbg("CommPort::PortWaitRecv: gotcha");
                break;
            }
        }
    }
    dbg("CommPort::PortWaitRecv: exit");
    return dwNumRecv;
}

DWORD CommPort::PortWaitRecvEnd(DWORD dwTimeout)
{
    DWORD dwNumRecv;
    DWORD dwCommErrors;
    COMSTAT cs;

    if(this->hPort == NULL)
        return 0;

    // non-WaitCommEvent version
    if(ClearCommError(this->hPort, &dwCommErrors, &cs) == FALSE)
        return 0;   // check comm error flags?
    dwNumRecv = cs.cbInQue;
#ifdef DBG
    char szdbg[64];
    sprintf(szdbg, "CommPort::PortWaitRecvEnd: enter: got %d b , %d ms\r\n", dwNumRecv, dwTimeout);
    dbgs(szdbg);
#endif // DBG
    do {
        dwNumRecv = cs.cbInQue;
        if(ClearCommError(this->hPort, &dwCommErrors, &cs) == FALSE)
            return 0;
#ifdef DBG
    sprintf(szdbg, "CommPort::PortWaitRecvEnd: loop: got %d bytes\r\n", dwNumRecv);
    dbgs(szdbg);
#endif // DBG
        Sleep(dwTimeout);      // use some another wait func?
    } while(dwNumRecv != cs.cbInQue);
    dbg("CommPort::PortWaitRecvEnd: exit");
    return dwNumRecv;
}

DWORD CommPort::PortWaitSend(DWORD dwNumBytes, DWORD dwTimeout)
{
    DWORD dwNumSend;
    DWORD dwCommErrors;
    DWORD dwTimeStep;   // wait time slice (ms)
    int nTimeout;
    COMSTAT cs;

    if(this->hPort == NULL)
        return 0;

    // non-WaitCommEvent version
    if(ClearCommError(this->hPort, &dwCommErrors, &cs) == FALSE)
        return 0;   // check comm error flags?
    if(dwNumBytes == 0)
        return 0;   // nothing to send - nothing to wait!
    nTimeout = dwTimeout;
    dwTimeStep = 20;
    dwNumSend = cs.cbOutQue;
    if(dwNumSend > 0)
    {
        while(nTimeout > 0)
        {
            nTimeout -= dwTimeStep;
            Sleep(dwTimeStep);      // use some another wait func?
            if(ClearCommError(this->hPort, &dwCommErrors, &cs) == FALSE)
                return 0;
            dwNumSend = cs.cbOutQue;
            if(dwNumSend == 0)
                break;
        }
    }
    // return number of succesfully send bytes
    return dwNumBytes - dwNumSend;
}

DWORD CommPort::PortRead(BYTE* pBuffer, DWORD dwNumRead, DWORD dwTimeout)
{
    DWORD dwRead;
    DWORD dwRes;
    BOOL bRes;
    DWORD dwRecvLen;

    if((this->hPort == NULL) || (pBuffer == NULL) || (dwNumRead == 0))
        return 0;
    if(this->bIoPending != FALSE)   // another operation is in progress
        return 0;

    this->bIoPending = TRUE;
    dwRecvLen = dwNumRead;
    ResetEvent(this->ovRead.hEvent);

    dbg("CommPort::PortRead(): recv");
    dwRecvLen = this->PortWaitRecv(dwNumRead, dwTimeout);
    dbgs("CommPort::PortRead(): ");
    dbgdec(dwRecvLen);
    if(dwRecvLen == 0)      // [ ] report failure, [x] try to read
        dwRecvLen = dwNumRead;  // nothing received yet, try to read anyway

    bRes = ReadFile(this->hPort, pBuffer, dwRecvLen, &dwRead, &this->ovRead);
    if(bRes != FALSE)
    {
        // Reset flag so that another opertion can be issued.
        this->bIoPending = FALSE;
        return dwRead;  // read completed immediately
    }
    else
    {
        this->dwErrorCode = GetLastError();
        if(this->dwErrorCode != ERROR_IO_PENDING)   // read not delayed?
        {
            // error in communications
            this->bIoPending = FALSE;
            return 0;
        }
    }

    this->dwErrorCode = ERROR_SUCCESS;  // =0
    if(dwTimeout == 0)
    {
        this->bIoPending = FALSE;
        return 0;
    }

    while(this->bIoPending != FALSE)
    {
        dwRes = WaitForSingleObject(this->ovRead.hEvent, dwTimeout);
        switch(dwRes)
        {
            case WAIT_OBJECT_0:     // Read completed.
                bRes = GetOverlappedResult(this->hPort, &this->ovRead, &dwRead, FALSE);
                if(bRes == FALSE)   // Error in communications; report it.
                {
                    this->dwErrorCode = GetLastError();
                    dwRead = 0;
                }
                //else                // Read completed successfully.
                this->bIoPending = FALSE;
                break;

            case WAIT_TIMEOUT:      // Operation isn't complete yet
                bRes = GetOverlappedResult(this->hPort, &this->ovRead, &dwRead, FALSE);
                if(bRes == FALSE)   // Error in communications; report it.
                {
                    this->dwErrorCode = GetLastError();
                    dwRead = 0;
                }
                //else                // Read completed successfully.
                this->AbortQuery();
                dwRead = 0;
                this->bIoPending = FALSE;
                break;

            default:
                // Error in the WaitForSingleObject.
                // This indicates a problem with the OVERLAPPED 
                // structure's event handle.
                this->dwErrorCode = GetLastError();
                dwRead = 0;
                this->bIoPending = FALSE;
                break;
        }
    }
    return dwRead;
}

DWORD CommPort::PortWrite(BYTE* pBuffer, DWORD dwNumWrite, DWORD dwTimeout)
{
    DWORD dwWritten;
    DWORD dwRes;
    BOOL bRes;

    if((this->hPort == NULL) || (pBuffer == NULL) || (dwNumWrite == 0))
        return 0;
    if(this->bIoPending != FALSE)   // another operation is in progress
        return 0;

    this->bIoPending = TRUE;
    ResetEvent(this->ovWrite.hEvent);

    bRes = WriteFile(this->hPort, pBuffer, dwNumWrite, &dwWritten, &this->ovWrite);
    if(bRes != FALSE)
    {
        this->bIoPending = FALSE;
        return dwWritten;   // Write completed immediately
    }
    else
    {
        this->dwErrorCode = GetLastError();
        if(this->dwErrorCode != ERROR_IO_PENDING)   // write not delayed?
        { 
            // error in communications
            this->bIoPending = FALSE;
            return 0;
        }
        // Write is pending.
        dwRes = WaitForSingleObject(this->ovWrite.hEvent, INFINITE);
        switch(dwRes)
        {
            case WAIT_OBJECT_0:     // Write completed
                bRes = GetOverlappedResult(this->hPort, &this->ovWrite, &dwWritten, FALSE);
                if(bRes == FALSE)
                {
                    this->dwErrorCode = GetLastError();
                    dwWritten = 0;
                }
                //else                // Write completed successfully.
                break;

            default:
                // An error has occurred in WaitForSingleObject.
                // This usually indicates a problem with the
                // OVERLAPPED structure's event handle.
                this->dwErrorCode = GetLastError();
                dwWritten = 0;
                break;
        }
    }

    this->PortWaitSend(1, dwTimeout);

    // Reset flag so that another opertion can be issued.
    this->bIoPending = FALSE;

    return dwWritten;
}

void CommPort::AbortQuery(void)
{
    if(this->hPort == NULL)
        return;

    // abort any I/O operations
    //CancelIo(this->hPort);
    // abort any WaitCommEvent's
    SetCommMask(this->hPort, EV_BREAK | EV_RXCHAR | EV_RXFLAG | EV_TXEMPTY);
}

void CommPort::FlushReadQueue(void)
{
    DWORD dwRead;
    DWORD dwNeed;
    BYTE buf[32];
    if(this->hPort == NULL)
        return;

    dwNeed = this->PortWaitRecv(0, 0);
#ifdef DBG
    char szdbg[64];
    sprintf(szdbg, "flush(): enter: %d bytes\r\n", dwNeed);
    dbgs(szdbg);
#endif // DBG
    dwRead = dwNeed;
    while(dwNeed != 0)
    {
        if(dwRead > sizeof(buf))
            dwRead = sizeof(buf);
        dwNeed -= this->PortRead(buf, dwRead, 0);
#ifdef DBG
    sprintf(szdbg, "flush(): loop: %d bytes\r\n", dwNeed);
    dbgs(szdbg);
#endif // DBG
    }
    dbg("flush(): exit");
}

MDMRESULT CommPort::SendATCommand(const char* szCommand, DWORD dwTimeout)
{
    BYTE szCmd[128];
    int nLen;
    DWORD dwLen;
    MDMRESULT mr;

    if(this->hPort == NULL)
        return MR_FAIL;

    nLen = strlen(szCommand) + 1;
    if(nLen > sizeof(szCmd))
        return MR_FAIL;

    dbg("atCmd():flush");
    this->FlushReadQueue();
    dbg("atCmd():send");
    sprintf((char*)szCmd, "%.127s\r", szCommand);  // 127 command bytes + <cr>
    this->PortWrite(szCmd, nLen, 200);

    dbg("atCmd():recv");
    dwLen = this->PortWaitRecv(6, dwTimeout);   // <cr> <lf> O K <cr> <lf>
    dwLen = this->PortWaitRecvEnd(100);
    if(dwLen > sizeof(this->szATBuffer))
        dwLen = sizeof(this->szATBuffer);   // protect from buffer overrun
    dbg("atCmd():read");
    this->PortRead(this->szATBuffer, dwLen, dwTimeout);

    dbg("atCmd():parse");
    mr = this->GetModemResponse((char*)this->szATBuffer);
    // take out any garbage
    this->FlushReadQueue(); //remove(?)

    return mr;
}

MDMRESULT CommPort::SendEscapedATCommand(const char* szCommand, DWORD dwTimeout)
{
    BYTE szCmd[128];
    int nLen;

    if(this->hPort == NULL)
        return MR_FAIL;

    nLen = strlen(szCommand) + 1;
    if(nLen > sizeof(szCmd))
        return MR_FAIL;

    Sleep(1000);    // 1 second before escape sequence
    this->FlushReadQueue();
    this->PortWrite((BYTE*)"+++", 3, 200);

    // read any garbage in buffer
    this->FlushReadQueue();

    Sleep(1000);    // 1 second after escape sequence
    this->PortRead(szCmd, sizeof(szCmd), 1000);  // 1 second after escape sequence
    if(this->GetModemResponse((char*)szCmd) != MR_OK)
        return MR_FAIL;

    sprintf((char*)szCmd, "%.127s\r", szCommand);    // 127 command bytes + <cr>
    this->PortWrite(szCmd, nLen, 200);

    this->PortRead(this->szATBuffer, sizeof(this->szATBuffer), dwTimeout);

    return this->GetModemResponse((char*)this->szATBuffer);
}

BOOL CommPort::Read(BYTE* pBuffer, DWORD dwBytesToRead, DWORD* pBytesRead, DWORD dwTimeout)
{
    DWORD dwRead;

    //dbghex(this->hPort);
    if(this->hPort == NULL)
        return FALSE;

    dwRead = this->PortRead(pBuffer, dwBytesToRead, dwTimeout);

    if(pBytesRead != NULL)
        *pBytesRead = dwRead;

#ifdef DBG
    char szdbg[64];
    sprintf(szdbg, "CommPort::Read = %d bytes\r\n", dwRead);
    dbgs(szdbg);
#endif // DBG

    return TRUE;
}

BOOL CommPort::Write(BYTE* pBuffer, DWORD dwBytesToWrite, DWORD* pBytesWritten, DWORD dwTimeout)
{
    DWORD dwWritten;

    if(this->hPort == NULL)
        return FALSE;

    dwWritten = this->PortWrite(pBuffer, dwBytesToWrite, dwTimeout);

    if(pBytesWritten != NULL)
        *pBytesWritten = dwWritten;

#ifdef DEBUG
    char szdbg[64];
    sprintf(szdbg, "CommPort::Write = %d bytes\r\n", dwWritten);
    dbgs(szdbg);
#endif // DBG

    return TRUE;
}



