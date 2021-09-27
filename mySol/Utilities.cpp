#include "Utilities.h"
#include "pch.h"

struct msg {
    char URLName[MAX_URL_LEN];
};

//concurrency::concurrent_queue<msg> Q;
queue<msg> Q;
CRITICAL_SECTION lpCriticalSection;
clock_t start_time;

bool Crawler::Producer(char *fileName)
{
    FILE* fptr;
    fptr = fopen(fileName, "rb");
    
    char URL_from_file[MAX_URL_LEN];
    if (fptr == NULL)
    {
        printf("Unable to open file. Please ensure that file path is correct\n");
        return false;
    }
    else
    {
        while (fscanf(fptr, "%[^\n] ", &URL_from_file) != EOF)
        {
            msg P; 
            memcpy(&P.URLName, URL_from_file, strlen(URL_from_file)+1);
            Q.push(P);
        }
    }

    fclose(fptr);
    return true;
}

void Consumer(LPVOID pParam)
{
    Crawler* cr = ((Crawler*)pParam);
    Socket sk;
    msg P;
    HTMLParserBase* parser = new HTMLParserBase;
    int queue_used=0;
    int dataBytes_used=0;
    int host_uni=0;
    int links=0;
    while (1)
    {
        EnterCriticalSection(&lpCriticalSection);
        if (Q.size() == 0)
        {
            /*InterlockedAdd(&cr->QueueUsed, queue_used);
            InterlockedAdd(&cr->Hostunique, host_uni);
            InterlockedAdd(&cr->dataBytes, dataBytes_used);
            InterlockedAdd(&cr->nLinks, links);

            InterlockedAdd(&cr->DNSLookups, sk.DNSLooked);
            InterlockedAdd(&cr->IPUnique, sk.IPLooked);
            InterlockedAdd(&cr->robot, sk.robot_looked);
            InterlockedAdd(&cr->http_check2, sk.http_check2);
            InterlockedAdd(&cr->http_check3, sk.http_check3);
            InterlockedAdd(&cr->http_check4, sk.http_check4);
            InterlockedAdd(&cr->http_check5, sk.http_check5);
            InterlockedAdd(&cr->other, sk.other);*/

            cr->QueueUsed += queue_used;
            cr->Hostunique += host_uni;
            cr->dataBytes += dataBytes_used;
            cr->nLinks += links;

            cr->DNSLookups += sk.DNSLooked;
            cr->IPUnique += sk.IPLooked;
            cr->robot += sk.robot_looked;
            cr->http_check2 += sk.http_check2;
            cr->http_check3 += sk.http_check3;
            cr->http_check4 += sk.http_check4;
            cr->http_check5 += sk.http_check5;
            cr->other += sk.other;

            SetEvent(cr->hEvent);
            LeaveCriticalSection(&lpCriticalSection);
            break;
        }
        cr->QueueUsed += queue_used;
        cr->Hostunique += host_uni;
        cr->dataBytes += dataBytes_used;
        cr->nLinks += links;

        cr->DNSLookups += sk.DNSLooked;
        cr->IPUnique += sk.IPLooked;
        cr->robot += sk.robot_looked;
        cr->http_check2 += sk.http_check2;
        cr->http_check3 += sk.http_check3;
        cr->http_check4 += sk.http_check4;
        cr->http_check5 += sk.http_check5;
        cr->other += sk.other;


        queue_used = 0;
        dataBytes_used = 0;
        host_uni = 0;
        links = 0;

        sk.DNSLooked = 0;
        sk.IPLooked = 0;
        sk.robot_looked = 0;
        sk.http_check2 = 0;
        sk.http_check3 = 0;
        sk.http_check4 = 0;
        sk.http_check5 = 0;
        sk.other = 0;
       /* queue<msg>temp;
        int k = 0;
        while (!Q.empty() && k < 15)
        {
            temp.push(Q.front());
            Q.pop();
            k++;
        }*/
        //LeaveCriticalSection(&lpCriticalSection);
        //while (!temp.empty())
        //{
            P = Q.front();
            Q.pop();
            if (P.URLName == NULL)
            {
                LeaveCriticalSection(&lpCriticalSection);
                continue;
            }
            /* if (Q.try_pop(P))
             {*/
            LeaveCriticalSection(&lpCriticalSection);
            //first get the robots, which means flag==3
            bool next_mv = sk.Get(P.URLName, 3, true);
            if (next_mv)
            {
                queue_used++;
                //cr->QueueUsed++;
                int prevSize = sk.seenHosts.size();
                sk.seenHosts.insert(sk.hostName);
                if (sk.seenHosts.size() > prevSize && next_mv)
                {
                    host_uni++;
                    //cr->Hostunique++;
                    next_mv = sk.init_sock(sk.hostName, 2, pParam);
                    if (next_mv)
                        next_mv = sk.Get(P.URLName, 2, false);

                    int prev_pos = sk.curPos;
                    if (next_mv)
                    {
                        next_mv = sk.init_sock(sk.hostName, 1, pParam);
                        dataBytes_used += sk.curPos;
                        // cr->dataBytes++;
                    }
                    if (next_mv)
                    {
                        if (sk.buff[9] == '2')
                        {
                            // where this page came from; needed for construction of relative links
                            int numLinks;
                            char* linkBuffer = parser->Parse(&sk.buff[prev_pos], sk.curPos - prev_pos, P.URLName, (int)strlen(P.URLName), &numLinks);

                            // check for errors indicated by negative values
                            if (numLinks < 0)
                                numLinks = 0;
                            links += numLinks;
                            //cr->nLinks++;
                        }
                    }
                }
            }
        //}
        //}
    }
    InterlockedAdd(&cr->numberThreads, -1);
    delete parser;
    return;
}

void stats(LPVOID pParam)
{
    Crawler* cr = ((Crawler*)pParam);
    start_time = clock();
    clock_t time_req;
    float time_elapsed = 0;
    clock_t prev_time = 0;
    long prev_data = 0;
    long prev_host = 0;
    while (1)
    {
        Sleep(2000);
        EnterCriticalSection(&lpCriticalSection);
        time_req = clock();
        if (Q.size() == 0)
        {
            SetEvent(cr->hEvent);
            LeaveCriticalSection(&lpCriticalSection);
            break;
        }
        time_elapsed = (time_req - prev_time) / CLOCKS_PER_SEC;
        printf("[%3d] %d  Q:%6d  E:%6d  H:%6d  D:%6d  I:%5d  R:%5d  C:%5d  L:%4dK\n", (int)(time_req-start_time)/CLOCKS_PER_SEC, cr->numberThreads, Q.size(), cr->QueueUsed, cr->Hostunique, cr->DNSLookups, cr->IPUnique, cr->robot, (cr->http_check2+ cr->http_check3+ cr->http_check4+ cr->http_check5+ cr->other), cr->nLinks/1000);
        float pps = (float)(cr->Hostunique - prev_host)/ time_elapsed;
        printf("*** crawling %3.1f pps @ %3.5f Mbps\n", pps, (float)(cr->dataBytes-prev_data)*8 /(1000000* (time_elapsed)));
        prev_time = clock();
        prev_data = cr->dataBytes;
        prev_host = cr->Hostunique;
        LeaveCriticalSection(&lpCriticalSection);
    }
    return;

}

void final_stat(LPVOID pParam)
{
    Crawler* cr = ((Crawler*)pParam);
    clock_t time_req;
    float time_elapsed = 0;

    WaitForSingleObject(cr->hEvent, INFINITE);
    EnterCriticalSection(&lpCriticalSection);
    time_req = clock();
    time_elapsed = (time_req - start_time) / CLOCKS_PER_SEC;
    printf("\nExtracted %d URLs @ %d/s\nLooked up %d DNS names @ %d/s\n", cr->QueueUsed, (int)(cr->QueueUsed / time_elapsed), cr->DNSLookups, (int)(cr->DNSLookups / time_elapsed));
    printf("Attempted %d URLs @ %d/s\nCrawled %d pages @ %d/s(%f MB)\n", cr->robot, (int)(cr->robot / time_elapsed), cr->Hostunique, (int)(cr->Hostunique / time_elapsed), (float)cr->dataBytes/(1024*1024*time_elapsed));
    printf("Parsed %d links @ %d/s\nHTTP codes : 2xx = %d 3xx = %d 4xx = %d 5xx = %d  other = %d\n", cr->nLinks, (int)(cr->nLinks / time_elapsed), cr->http_check2, cr->http_check3, cr->http_check4, cr->http_check5, cr->other);
    //HTTP codes : 2xx = 47185, 3xx = 5826, 4xx = 6691, 5xx = 202, other = 0
    LeaveCriticalSection(&lpCriticalSection);
 
    return;
}