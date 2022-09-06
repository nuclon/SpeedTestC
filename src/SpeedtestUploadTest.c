#include "SpeedtestUploadTest.h"
#include "SpeedtestConfig.h"
#include "SpeedtestServers.h"
#include "Speedtest.h"
#include "http.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <string.h>

static void __appendTimestamp(const char *url, char *buff, int buff_len)
{
    char delim = '?';
    char *p = strchr(url, '?');

    if (p)
        delim = '&';
    snprintf(buff, buff_len, "%s%cx=%llu", url, delim, (unsigned long long)time(NULL));
}

static void *__uploadThread(void *arg)
{
    /* Testing upload... */
    THREADARGS_T *threadConfig = (THREADARGS_T *)arg;
    char alphabet[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    char buffer[BUFFER_SIZE] = {0};
    int i, size;
    struct timeval tval_start;
    unsigned long totalTransfered = 0;
    char uploadUrl[1024];
    sock_t sockId;

    /* Build the random buffer */
    printf("...build the random buffer...\n");
    srand(time(NULL));
    for(i=0; i < BUFFER_SIZE; i++)
    {
        buffer[i] = alphabet[rand() % ARRAY_SIZE(alphabet)];
    }

    gettimeofday(&tval_start, NULL);
    printf("...starting %d tests\n", threadConfig->testCount);
    for (i = 0; i < threadConfig->testCount; i++)
    {
        printf("...test no %d\n", i);
        __appendTimestamp(threadConfig->url, uploadUrl, sizeof(uploadUrl));
        /* FIXME: totalToBeTransfered should be readonly while the upload thread is running */
        totalTransfered = totalToBeTransfered;
        sockId = httpPutRequestSocket(uploadUrl, totalToBeTransfered);
        if(sockId == 0)
        {
            printf("Unable to open socket for Upload!");
            pthread_exit(NULL);
        }

        while(totalTransfered != 0)
        {
            if (totalTransfered > BUFFER_SIZE)
            {
                size = httpSend(sockId, buffer, BUFFER_SIZE);
            }
            else
            {
                buffer[totalTransfered - 1] = '\n'; /* Indicate terminated */
                size = httpSend(sockId, buffer, totalTransfered);
            }

            totalTransfered -= size;
        }
        threadConfig->transferedBytes += totalToBeTransfered;
        /* Cleanup */
        printf("...cleanup http\n");
        httpClose(sockId);
        printf("...done cleanup http\n");
    }
    threadConfig->elapsedSecs = getElapsedTime(tval_start);

    return NULL;
}

void testUpload(const char *url)
{
    size_t numOfThreads = speedTestConfig->uploadThreadConfig.threadsCount;
    THREADARGS_T *para<m = (THREADARGS_T *) calloc(numOfThreads, sizeof(THREADARGS_T));

    int i;
    for (i = 0; i < numOfThreads; i++)
    {
        /* Initializing some parameters */
        param[i].testCount =  speedTestConfig->uploadThreadConfig.length;
        if (param[i].testCount == 0)
        {
            /* At least three test should be run */
            param[i].testCount = 3;
        }
        param[i].url = strdup(url);
        if (param[i].url)
        {
            pthread_create(&param[i].tid, NULL, &__uploadThread, &param[i]);
        }
    }

    /* Refresh */
    totalTransfered = 0;
    float speed = 0;

    /* Wait for all threads */
    printf("...wait for threads\n");
    for (i = 0; i < numOfThreads; i++)
    {
        printf("...waiting for %d (tid: %d)\n", i, param[i].tid);
        pthread_join(param[i].tid, NULL);
        printf("...joined a thread\n");
        if (param[i].transferedBytes)
        {
            /* There's no reason that we transfered nothing except error occured */
            totalTransfered += param[i].transferedBytes;
            speed += (param[i].transferedBytes / param[i].elapsedSecs) / 1024;
        }
        /* Cleanup */
        printf("...cleanup url\n");
        free(param[i].url);
        printf("...done with thread\n");
    }
    free(param);
    printf("Bytes %lu uploaded with a speed %.2f kB/s (%.2f Mbit/s)\n",
        totalTransfered, speed, speed * 8 / 1024);
}
